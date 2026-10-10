#include <libdragon.h>

#include "boot_io.h"
#include "boot.h"
#include "cheats.h"
#include "cic.h"
#include "reboot.h"

// The single definition of the global declared `extern` in boot.h.
boot_params_t boot_params;

/* GAME_LOAD_ROM_OFFSET/GAME_LOAD_SIZE come from boot.h, shared with whoever
   stages the ROM. The destination is not fixed: it comes from the ROM header,
   and varies per game (0x80000400 is common, SM64 uses 0x80246000). */

/* libultra's boot info block, which IPL2/IPL3 populate on a real console.
   Written through the uncached mirror so the values are in RDRAM before
   reboot.S invalidates the caches without writing them back. */
#define OS_INFO_ADDRESS       (0xA0000300)
#define OS_INFO_NMI_BUFFER_WORDS (16)

/**
 * Selects the base IO address for the configured boot device.
 *
 * @param params Boot parameters whose `device_type` determines the base.
 * @returns Pointer to the boot device base IO registers corresponding to `params->device_type` (e.g., `ROM_CART` or `ROM_DDIPL`).
 */
static io32_t *boot_get_device_base (boot_params_t *params) {
    io32_t *device_base_address = ROM_CART;
    if (params->device_type == BOOT_DEVICE_TYPE_64DD) {
        device_base_address = ROM_DDIPL;
    }
    return device_base_address;
}

/**
 * Can we boot this cartridge ourselves instead of running its IPL3?
 *
 * Only the plain retail CIC is claimed for now -- 6102/7101, which covers most
 * NTSC carts including Super Mario 64. Everything else (64DD variants, the
 * x105/x106 oddities, unrecognised or homebrew IPL3s) keeps running the real
 * thing, which is the safer default and a useful bisect when a game misbehaves.
 *
 * @param cic_type The type detected from the cartridge's IPL3.
 * @returns true to take the HLE path in reboot.S.
 */
static bool boot_can_hle (cic_type_t cic_type) {
    return cic_type == CIC_x102;
}

/**
 * Publish the boot info block that IPL2/IPL3 would have written.
 *
 * libultra reads these from 0x80000300 onwards; games that care about TV type,
 * memory size or reset type get them from here rather than from registers.
 *
 * @param params Boot parameters supplying the device and TV type.
 * @param version The libultra `osVersion` value for the chosen TV type.
 */
static void boot_write_os_info (boot_params_t *params, uint32_t version) {
    volatile uint32_t *os_info = (volatile uint32_t *) OS_INFO_ADDRESS;

    os_info[0] = (params->tv_type & 0x03);                  // osTvType
    os_info[1] = (params->device_type & 0x01);              // osRomType
    os_info[2] = (params->device_type == BOOT_DEVICE_TYPE_64DD)
               ? (0xA0000000 | ROM_DDIPL_BASE)
               : (0xA0000000 | ROM_CART_BASE);              // osRomBase
    os_info[3] = BOOT_RESET_TYPE_COLD;                      // osResetType
    os_info[4] = (params->cic_seed & 0xFF);                 // osCicId
    os_info[5] = version;                                   // osVersion
    os_info[6] = __boot_memsize;                            // osMemSize

    /* osAppNMIBuffer: 64 bytes a game may read after a warm boot. We always
       report a cold reset, so hand it over zeroed. */
    for (int i = 0; i < OS_INFO_NMI_BUFFER_WORDS; i++) {
        os_info[7 + i] = 0;
    }
}

static cic_type_t boot_detect_cic (boot_params_t *params) {
    io32_t *base = boot_get_device_base(params);

    uint8_t ipl3[IPL3_LENGTH] __attribute__((aligned(8)));

    data_cache_hit_writeback_invalidate(ipl3, sizeof(ipl3));
    dma_read_raw_async(ipl3, (uint32_t) (&base[16]), sizeof(ipl3));
    dma_wait();

    return cic_detect(ipl3);
}

/**
 * Prepare system hardware, load reboot code and IPL3, install cheats, and transfer control to the reboot routine using the provided boot parameters.
 *
 * This performs CIC detection (and optionally populates the CIC seed), normalizes passthrough TV type, configures and resets CPU/SP/PI/VI/AI state, programs PI DOM timing from the boot device, copies the reboot routine into SP IMEM and IPL3 into SP DMEM, installs cheats, arranges boot-time registers, and jumps to the in-memory reboot entry. If control returns, the function loops indefinitely.
 *
 * @param params Boot configuration and state. Fields read include device_type, tv_type, detect_cic_seed, and cheat_list. On return this function may modify params->tv_type (when passthrough normalization is applied) and params->cic_seed (when detect_cic_seed is true).
 */
void boot (boot_params_t *params) {
    if (params->detect_cic_type) {
        params->cic_type = boot_detect_cic(params);
    }

    if (params->detect_cic_seed) {
        params->cic_seed = cic_get_seed(params->cic_type);
    }

    if (params->tv_type == BOOT_TV_TYPE_PASSTHROUGH) {
        switch (get_tv_type()) {
            case TV_PAL:
                params->tv_type = BOOT_TV_TYPE_PAL;
                break;
            case TV_NTSC:
                params->tv_type = BOOT_TV_TYPE_NTSC;
                break;
            case TV_MPAL:
                params->tv_type = BOOT_TV_TYPE_MPAL;
                break;
            default:
                params->tv_type = BOOT_TV_TYPE_NTSC;
                break;
        }
    }

    while (!(cpu_io_read(&SP->SR) & SP_SR_HALT));

    cpu_io_write(&SP->SR,
        SP_SR_CLR_SIG7 |
        SP_SR_CLR_SIG6 |
        SP_SR_CLR_SIG5 |
        SP_SR_CLR_SIG4 |
        SP_SR_CLR_SIG3 |
        SP_SR_CLR_SIG2 |
        SP_SR_CLR_SIG1 |
        SP_SR_CLR_SIG0 |
        SP_SR_CLR_INTR_BREAK |
        SP_SR_CLR_SSTEP |
        SP_SR_CLR_INTR |
        SP_SR_CLR_BROKE |
        SP_SR_SET_HALT
    );
    cpu_io_write(&SP->SEMAPHORE, 0);
    cpu_io_write(&SP->PC, 0);

    while (cpu_io_read(&SP->DMA_BUSY));

    cpu_io_write(&PI->SR, PI_SR_CLR_INTR | PI_SR_RESET);
    while ((cpu_io_read(&VI->CURR_LINE) & ~(VI_CURR_LINE_FIELD)) != 0);

    /* Blank the DAC before disturbing anything else about the video state. We
       are at the top of a field here, so the field that starts now is entirely
       black, rather than showing the menu's framebuffer scanned out through the
       cleared H_LIMITS below -- which is one visibly corrupt frame. The game
       programs VI from scratch, so there is nothing here worth preserving. */
    cpu_io_write(&VI->CR, 0);

    cpu_io_write(&VI->V_INTR, 0x3FF);
    cpu_io_write(&VI->H_LIMITS, 0);
    cpu_io_write(&VI->CURR_LINE, 0);
    cpu_io_write(&AI->MADDR, 0);
    cpu_io_write(&AI->LEN, 0);

    while (cpu_io_read(&SP->SR) & SP_SR_DMA_BUSY);

    uint32_t *reboot_src = &reboot_start;
    io32_t *reboot_dst = SP_MEM->IMEM;
    size_t reboot_instructions = (size_t) (&reboot_size) / sizeof(uint32_t);

    for (unsigned int i = 0; i < reboot_instructions; i++) {
        cpu_io_write(&reboot_dst[i], reboot_src[i]);
    }

    cpu_io_write(&PI->DOM[0].LAT, 0xFF);
    cpu_io_write(&PI->DOM[0].PWD, 0xFF);
    cpu_io_write(&PI->DOM[0].PGS, 0x0F);
    cpu_io_write(&PI->DOM[0].RLS, 0x03);

    io32_t *base = boot_get_device_base(params);
    uint32_t pi_config = io_read((uint32_t) (base));

    cpu_io_write(&PI->DOM[0].LAT, pi_config & 0xFF);
    cpu_io_write(&PI->DOM[0].PWD, pi_config >> 8);
    cpu_io_write(&PI->DOM[0].PGS, pi_config >> 16);
    cpu_io_write(&PI->DOM[0].RLS, pi_config >> 20);

    if (cpu_io_read(&DPC->SR) & DPC_SR_XBUS_DMEM_DMA) {
        while (cpu_io_read(&DPC->SR) & DPC_SR_PIPE_BUSY);
    }

    /* Prefer the staged copy when the launch path managed to warm it: the slow
       cart transfer already happened behind the launch animation, so all that is
       left at boot time is a quick RDRAM-to-RDRAM copy. */
    bool hle_boot = boot_can_hle(params->cic_type);
    bool hle_staged = hle_boot && params->rom_staged_in_memory && is_memory_expanded();

    uint32_t boot_path = !hle_boot   ? BOOT_PATH_IPL3
                       : hle_staged  ? BOOT_PATH_HLE_STAGED
                                     : BOOT_PATH_HLE_CART;

    /* The HLE path never runs it, so only stage IPL3 into DMEM when we are
       actually going to execute it (and when the cheat hook needs to patch it). */
    if (!hle_boot) {
        io32_t *ipl3_src = base;
        io32_t *ipl3_dst = SP_MEM->DMEM;

        for (int i = 16; i < 1024; i++) {
            cpu_io_write(&ipl3_dst[i], io_read((uint32_t) (&ipl3_src[i])));
        }
    }

    /* Where IPL3 would have put the game, and where it would have jumped:
       ROM header offset 0x08 is the load address and the initial PC. */
    uint32_t game_entry_address = io_read((uint32_t) (&base[2]));
    uint32_t game_source_address = hle_staged
        ? GAME_STAGING_ADDRESS
        : (uint32_t) (&base[GAME_LOAD_ROM_OFFSET / sizeof(io32_t)]);

    uint32_t version_value = (params->tv_type == BOOT_TV_TYPE_PAL) ? 6
                           : (params->tv_type == BOOT_TV_TYPE_NTSC) ? 1
                           : (params->tv_type == BOOT_TV_TYPE_MPAL) ? 4
                           : 0;

    bool cheats_installed = hle_boot
        ? cheats_install_hle(params->cheat_list)
        : cheats_install(params->cic_type, params->cheat_list);

    if (hle_boot) {
        // IPL3 would have written these; nobody else will now.
        boot_write_os_info(params, version_value);

        debugf(
            "[boot] HLE path, %s (CIC type %i): loading 0x%X bytes from 0x%08lX "
            "to 0x%08lX, cheats: %i\n",
            hle_staged ? "staged copy via RSP DMA" : "cart via PI DMA",
            (int) params->cic_type, GAME_LOAD_SIZE, game_source_address,
            game_entry_address, cheats_installed
        );
    }
    else {
        debugf(
            "[boot] IPL3 path (CIC type %i): running the cartridge's own IPL3, "
            "cheats: %i\n",
            (int) params->cic_type, cheats_installed
        );
    }

    register uint32_t skip_rdram_reset asm ("a0");
    register uint32_t hle_flag asm ("a1");
    register uint32_t game_entry asm ("a2");
    register uint32_t game_source asm ("a3");
    register uint32_t jump_target asm ("t8");
    register uint32_t boot_device asm ("s3");
    register uint32_t tv_type asm ("s4");
    register uint32_t reset_type asm ("s5");
    register uint32_t cic_seed asm ("s6");
    register uint32_t version asm ("s7");

    skip_rdram_reset = cheats_installed;
    hle_flag = boot_path;
    game_entry = game_entry_address;
    game_source = game_source_address;
    // With cheats the patcher runs first and jumps onwards via t1; without it
    // there is nothing to do but enter the game.
    jump_target = cheats_installed ? cheats_patcher_address() : game_entry_address;
    boot_device = (params->device_type & 0x01);
    tv_type = (params->tv_type & 0x03);
    reset_type = BOOT_RESET_TYPE_COLD;
    cic_seed = (params->cic_seed & 0xFF);
    version = version_value;

    asm volatile (
        "li $t3, %[c0_status] \n"
        "mtc0 $t3, $12 \n"
        "ctc1 $zero, $f31 \n"
        "la $t3, reboot \n"
        "jr $t3 \n" ::
        [c0_status] "i" (C0_STATUS_CU1 | C0_STATUS_CU0 | C0_STATUS_FR),
        [skip_rdram_reset] "r" (skip_rdram_reset),
        [hle_flag] "r" (hle_flag),
        [game_entry] "r" (game_entry),
        [game_source] "r" (game_source),
        [jump_target] "r" (jump_target),
        [boot_device] "r" (boot_device),
        [tv_type] "r" (tv_type),
        [reset_type] "r" (reset_type),
        [cic_seed] "r" (cic_seed),
        [version] "r" (version) :
        "t3"
    );

    while (1);
}
