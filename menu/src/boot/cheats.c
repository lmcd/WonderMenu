/**
 * @file cheats.c
 * @brief Cheat Engine Implementation
 * @ingroup boot
 */

#include <libdragon.h>
#include "boot_io.h"
#include "cheats.h"
#include "vr4300_asm.h"

#define HIT_INVALIDATE_I ((4 << 2) | 0)
#define HIT_WRITE_BACK_D ((6 << 2) | 1)

#define D_CACHE_LINE_SIZE (16)

#define CAUSE_IRQ_PRE_NMI (1 << 12)
#define CAUSE_EXC_CODE_MASK (0x7C)
#define CAUSE_EXC_CODE_WATCH (0x5C)

#define WATCHLO_W (1 << 0)

#define RELOCATED_EXCEPTION_HANDLER_ADDRESS (0x80000120)
#define EXCEPTION_HANDLER_ADDRESS (0x80000180)
// The patcher gets 32 KiB, after the payload's 32 KiB at 0x80700000-0x80707FFF
// (PAYLOAD_ADDRESS)
#define PATCHER_ADDRESS (0x80708000)
#define ENGINE_TEMPORARY_ADDRESS (0x80710000)
#define DEFAULT_ENGINE_ADDRESS (0x807C5C00)

// The payload reserves the first 64 bytes at 0x80700000 (payload_settings /
// PayloadData) for its own settings. The resume trampoline's game_ctx snapshot
// buffer -- and every O_* offset / flag the engine writes into it -- therefore
// sits this many bytes higher. MUST match the payload_settings size in resume.S.
#define GAME_CTX_OFFSET (64)

// game_ctx offset of the pre-NMI jump's "already fired" flag. Cause.IP4 stays
// set until the NMI itself, so without this every later exception would jump
// into the payload again. MUST match the reservation in resume.S.
#define PRE_NMI_FLAG_OFFSET (352)

// TEST: 0 jumps on every exception that shows Cause.IP4, ignoring the flag above
#define PRE_NMI_FIRE_ONCE (0)

// Most SPECIAL_JAL_HOOK cheats one engine can carry
#define MAX_JAL_HOOKS (16)

/** @brief A SPECIAL_JAL_HOOK cheat whose stub is emitted after the engine body */
typedef struct {
    uint16_t value; /**< Cheat value, handed to jal_hook_c() */
    io32_t *compare_hi; /**< `lui k0` of our `jal` word, for the already-hooked test */
    io32_t *compare_lo; /**< `ori k0` of our `jal` word */
    io32_t *slot_hi; /**< `lui k0` of the slot address */
    io32_t *slot_lo; /**< `sw k1` into the slot */
    io32_t *write_hi; /**< `lui k1` of our `jal` word, for the patch write */
    io32_t *write_lo; /**< `ori k1` of our `jal` word */
} jal_hook_t;

/** @brief Cheat structure */
typedef struct {
    uint8_t type; /**< Cheat type */
    uint32_t address; /**< Cheat address */
    uint16_t value; /**< Cheat value */
} cheat_t;

/** @brief Cheat entry structure */
typedef struct {
    cheat_t main; /**< Main cheat */
    cheat_t sub; /**< Sub cheat */
} cheat_entry_t;

/** @brief Special cheat types enumeration */
typedef enum {
    SPECIAL_CLEAR_MEMORY = 0x20, /**< Clear memory between 0x80000200-0x80000300 on boot */
    SPECIAL_SECONDARY_EXCEPTION_HANDLER = 0xCC, /**< Use alternate exception handler */
    SPECIAL_SET_ENTRYPOINT_ADDR = 0xDE, /**< Set boot entrypoint address */
    SPECIAL_DISABLE_EXPANSION_PAK = 0xEE, /**< Disable Expansion Pak */
    SPECIAL_WRITE_BYTE_ON_BOOT = 0xF0, /**< Write byte on boot */
    SPECIAL_WRITE_SHORT_ON_BOOT = 0xF1, /**< Write short on boot */
    SPECIAL_SET_STORE_LOCATION = 0xFF, /**< Set store location */
    SPECIAL_JUMP = 0xBD, /**< Unconditionally jump to the resident payload (every frame) */
    SPECIAL_JUMP_ON_PRE_NMI = 0xBE, /**< Jump to the resident payload once when the reset button is pressed */
    SPECIAL_JAL_HOOK = 0xBC, /**< Redirect the game's `jal` at the address through the payload's jal_hook_c(value) */
    SPECIAL_SET_JAL_HOOK_ENTRY = 0xBB, /**< Set where SPECIAL_JAL_HOOK stubs jump to (the payload's jal_hook_entry) */
} cheat_type_special_t;

#define IS_WIDTH_16(t) ((t) & (1 << 0))
#define IS_CONDITION_NOT_EQUAL(t) ((t) & (1 << 1))
#define IS_CONDITION_GS_BUTTON(t) ((t) & (1 << 3))

#define IS_TYPE_REPEATER(t) ((t) == 0x50)
#define IS_TYPE_WRITE(t) ((((t)&0xF0) == 0x80) || (((t)&0xF0) == 0xA0))
#define IS_TYPE_CONDITIONAL(t) (((t)&0xF0) == 0xD0)

#define IS_DOUBLE_ENTRY(t) (IS_TYPE_CONDITIONAL(t) || IS_TYPE_REPEATER(t))

#define X106_XOR_CONSTANT (0x0260BCD5)
#define X106_ENC_START (0x13C)

/**
 * @brief Get the XOR value for a given offset in the CIC x106 encrypted area.
 *
 * Calls to this function ought to always be reduced to constants.
 *
 * @param seed The IPL3 checksum seed (should always be 0x85 for x106; see cic_get_seed()).
 * @param offset The offset in the encrypted area to calculate for.
 * @return the calculated XOR value.
 */
__attribute__((always_inline))
static inline uint32_t cheats_calc_x106_xor(uint8_t seed, uint8_t offset) {
    uint32_t val = X106_XOR_CONSTANT * seed + 1;
    #pragma GCC unroll 256
    for (uint8_t i = 0; i < offset; i++) {
        val *= X106_XOR_CONSTANT;
    }
    return val;
}

/**
 * @brief Patch the IPL3 with the cheat engine.
 * 
 * @param cic_type The CIC type.
 * @param target The target address.
 * @return true if successful, false otherwise.
 */
static bool cheats_patch_ipl3 (cic_type_t cic_type, io32_t *target) {
    uint32_t patch_offset = 0;
    uint32_t j_instruction = I_J((uint32_t)(target));

    io32_t *ipl3 = SP_MEM->DMEM;

    switch (cic_type) {
    case CIC_5101: patch_offset = 476; break;
    case CIC_6101:
    case CIC_7102: patch_offset = 466; break;
    case CIC_x102: patch_offset = 475; break;
    case CIC_x103: patch_offset = 472; break;
    case CIC_x105: patch_offset = 499; break;
    case CIC_x106: patch_offset = 488; break;
    default: return true;
    }

    // NOTE: Check for "jr $t1" instruction
    //       Libdragon IPL3 could be brute-force signed with any retail
    //       CIC seed and checksum, and we support only retail libultra IPL3
    uint32_t test_instruction = cpu_io_read(&ipl3[patch_offset]);
    if (cic_type == CIC_x106) {
        // NOTE: CIC x106 IPL3 is partially scrambled
        test_instruction ^= cheats_calc_x106_xor(cic_get_seed(cic_type), patch_offset - X106_ENC_START);
    }

    if (test_instruction != I_JR(REG_T1)) {
        return false;
    }

    switch (cic_type) {
    case CIC_x105:
        // NOTE: This disables game code checksum verification
        cpu_io_write(&ipl3[486], I_NOP());
        break;

    case CIC_x106:
        // NOTE: CIC x106 IPL3 is partially scrambled
        j_instruction ^= cheats_calc_x106_xor(cic_get_seed(cic_type), patch_offset - X106_ENC_START);
        break;

    default: break;
    }

    cpu_io_write(&ipl3[patch_offset], j_instruction);

    return false;
}

/**
 * @brief Get the next cheat entry from the cheat list.
 * 
 * @param cheat_list Pointer to the cheat list.
 * @param cheat Pointer to the cheat entry structure.
 * @return true if successful, false otherwise.
 */
static bool cheats_get_next (uint32_t **cheat_list, cheat_entry_t *cheat) {
    cheat_t *c = &cheat->main;
    cheat->sub.type = 0;

    for (int i = 0; i < 2; i++) {
        uint32_t raw[2] = {(*cheat_list)[0], (*cheat_list)[1]};

        (*cheat_list) += 2;

        if ((raw[0] == 0) && (raw[1] == 0)) {
            return false;
        }

        c->type = ((raw[0] >> 24) & 0xFF);
        c->address = (raw[0] & 0xA07FFFFF);
        c->value = (raw[1] & 0xFFFF);

        if (!IS_DOUBLE_ENTRY(c->type)) {
            break;
        }

        c = &cheat->sub;
    }

    return true;
}

/**
 * @brief Get the engine address from the cheat list.
 * 
 * @param cheat_list Pointer to the cheat list.
 * @return io32_t* The engine address.
 */
static io32_t *cheats_get_engine_address (uint32_t *cheat_list) {
    cheat_entry_t cheat;
    while (cheats_get_next(&cheat_list, &cheat)) {
        if (cheat.main.type == SPECIAL_SET_STORE_LOCATION) {
            return (io32_t *)(cheat.main.address & 0x807FFFFF);
        }
    }
    return (io32_t *)(DEFAULT_ENGINE_ADDRESS);
}

/**
 * @brief Find the pre-NMI jump in the cheat list.
 *
 * @param cheat_list Pointer to the cheat list.
 * @param target Receives the jump target (cached KSEG0) when one is found.
 * @return true if the list contains a SPECIAL_JUMP_ON_PRE_NMI cheat.
 */
static bool cheats_get_pre_nmi_jump (uint32_t *cheat_list, uint32_t *target) {
    cheat_entry_t cheat;
    while (cheats_get_next(&cheat_list, &cheat)) {
        if (cheat.main.type == SPECIAL_JUMP_ON_PRE_NMI) {
            // Same KSEG0 forcing as SPECIAL_JUMP: the 0xBE type byte leaks
            // bits 31/29 into the masked address.
            *target = (cheat.main.address & 0x007FFFFF) | 0x80000000;
            return true;
        }
    }
    return false;
}

/**
 * @brief Find the JAL hook entry address in the cheat list.
 *
 * @param cheat_list Pointer to the cheat list.
 * @param address Receives the entry address (cached KSEG0) when one is found.
 * @return true if the list contains a SPECIAL_SET_JAL_HOOK_ENTRY cheat.
 */
static bool cheats_get_jal_hook_entry (uint32_t *cheat_list, uint32_t *address) {
    cheat_entry_t cheat;
    while (cheats_get_next(&cheat_list, &cheat)) {
        if (cheat.main.type == SPECIAL_SET_JAL_HOOK_ENTRY) {
            // Same KSEG0 forcing as SPECIAL_JUMP: the 0xBB type byte leaks
            // bits 31/29 into the masked address.
            *address = (cheat.main.address & 0x007FFFFF) | 0x80000000;
            return true;
        }
    }
    return false;
}

/**
 * @brief Update the cache for the specified memory range.
 * 
 * @param start The start address.
 * @param end The end address.
 */
static void cheats_update_cache (volatile void *start, volatile void *end) {
    data_cache_hit_writeback(start, (end - start));
    inst_cache_hit_invalidate(start, (end - start));
}

/**
 * @brief Install the cheat engine.
 * 
 * @param cic_type The CIC type.
 * @param cheat_list Pointer to the cheat list.
 * @return true if successful, false otherwise.
 */
static bool cheats_install_impl (cic_type_t cic_type, uint32_t *cheat_list, bool patch_ipl3) {
    if (!cheat_list) {
        return false;
    }

    io32_t *engine_start = (io32_t *)(ENGINE_TEMPORARY_ADDRESS);
    io32_t *engine_p = engine_start;

    io32_t *patcher_start = (io32_t *)(PATCHER_ADDRESS);
    io32_t *patcher_p = patcher_start;

    // On the HLE path there is no IPL3 to hook: reboot.S jumps straight to the
    // patcher once it has loaded the game, with the entry point already in t1.
    if (patch_ipl3 && cheats_patch_ipl3(cic_type, patcher_start)) {
        return false;
    }

    io32_t *final_engine_address = cheats_get_engine_address(cheat_list);

    uint32_t pre_nmi_target = 0;
    bool has_pre_nmi_jump = cheats_get_pre_nmi_jump(cheat_list, &pre_nmi_target);

    // Without it SPECIAL_JAL_HOOK cheats have nowhere to go, so they're skipped
    uint32_t jal_hook_entry = 0;
    bool has_jal_hook_entry = cheats_get_jal_hook_entry(cheat_list, &jal_hook_entry);

    // Original watch exception handler code written by Jay Oster 'Parasyte'
    // https://github.com/parasyte/alt64/blob/master/utils.c#L1024-L1054

    uint32_t ori_placeholder_instruction = I_ORI(REG_ZERO, REG_K0, A_OFFSET(RELOCATED_EXCEPTION_HANDLER_ADDRESS));

    // Load cause register
    *engine_p++ = I_MFC0(REG_K0, C0_REG_CAUSE);

    // Disable watch exception when reset button is pressed
    *engine_p++ = I_ANDI(REG_K1, REG_K0, CAUSE_IRQ_PRE_NMI);
    *engine_p++ = I_BNEL(REG_K1, REG_ZERO, 1);
    *engine_p++ = I_MTC0(REG_ZERO, C0_REG_WATCH_LO);

    if (has_pre_nmi_jump) {
        debugf("Had pre-NMI cheat\n");
        // Reset button pressed: jump to the resident payload once. k1 still
        // holds Cause & IP4 from above. trampoline_enter waits for the RDP to
        // drain before it suspends.
        io32_t *not_nmi_branch = engine_p;
        *engine_p++ = I_NOP();                                              // patched: BEQ k1,0,not_nmi
        *engine_p++ = I_NOP();

        // IP4 stays set until the NMI, so every later exception lands here
        // too. The flag lives in game_ctx (zero after staging) and is reached
        // uncached so it's coherent on the next exception.
        *engine_p++ = I_LUI(REG_K0, 0xA070);                                // PAYLOAD_ADDRESS
        *engine_p++ = I_LW(REG_K1, GAME_CTX_OFFSET + PRE_NMI_FLAG_OFFSET, REG_K0);
        io32_t *fired_branch = engine_p;
        *engine_p++ = I_NOP();                                              // patched: BNE k1,0,not_nmi
        *engine_p++ = I_NOP();
        *engine_p++ = I_ORI(REG_K1, REG_ZERO, 1);
        *engine_p++ = I_SW(REG_K1, GAME_CTX_OFFSET + PRE_NMI_FLAG_OFFSET, REG_K0);

        *engine_p++ = I_LUI(REG_K0, pre_nmi_target >> 16);
        *engine_p++ = I_ORI(REG_K0, REG_K0, pre_nmi_target & 0xFFFF);
        *engine_p++ = I_JR(REG_K0);
        *engine_p++ = I_NOP();

        *not_nmi_branch = I_BEQ(REG_K1, REG_ZERO, (engine_p - not_nmi_branch) - 1);
        *fired_branch = PRE_NMI_FIRE_ONCE
            ? I_BNE(REG_K1, REG_ZERO, (engine_p - fired_branch) - 1)
            : I_NOP();

        // The already-fired path clobbered k0, so reload Cause for the watch check
        *engine_p++ = I_MFC0(REG_K0, C0_REG_CAUSE);
    }

    // The placeholder is the NOP 16 instructions after the ANDI below, so its
    // position moves with the prologue above.
    uint32_t ori_placeholder_address = (uint32_t)(final_engine_address + (engine_p - engine_start) + 16);

    // Check if watch exception occurred, if yes then proceed to relocate the game exception handler
    *engine_p++ = I_ANDI(REG_K0, REG_K0, CAUSE_EXC_CODE_MASK);
    *engine_p++ = I_ORI(REG_K1, REG_ZERO, CAUSE_EXC_CODE_WATCH);
    *engine_p++ = I_BNE(REG_K0, REG_K1, 15); // Skips to after the 'eret' instruction

    // Extract base register number from the store instruction
    *engine_p++ = I_MFC0(REG_K1, C0_REG_EPC);
    *engine_p++ = I_LW(REG_K1, 0, REG_K1);
    *engine_p++ = I_LUI(REG_K0, 0x03E0);
    *engine_p++ = I_AND(REG_K1, REG_K0, REG_K1);
    *engine_p++ = I_SRL(REG_K1, REG_K1, 5);

    // Update create final instruction and update its target register number
    *engine_p++ = I_LUI(REG_K0, ori_placeholder_instruction >> 16);
    *engine_p++ = I_ORI(REG_K0, REG_K0, ori_placeholder_instruction);
    *engine_p++ = I_OR(REG_K0, REG_K0, REG_K1);

    // Write created instruction into placeholder
    *engine_p++ = I_LUI(REG_K1, A_BASE(ori_placeholder_address));
    *engine_p++ = I_SW(REG_K0, A_OFFSET(ori_placeholder_address), REG_K1);

    // Force write and instruction cache invalidation
    *engine_p++ = I_CACHE(HIT_WRITE_BACK_D, A_OFFSET(ori_placeholder_address), REG_K1);
    *engine_p++ = I_CACHE(HIT_INVALIDATE_I, A_OFFSET(ori_placeholder_address), REG_K1);

    // Load address base and execute created instruction
    *engine_p++ = I_LUI(REG_K0, A_BASE(RELOCATED_EXCEPTION_HANDLER_ADDRESS));
    *engine_p++ = I_NOP();

    // Return from the exception
    *engine_p++ = I_ERET();

    cheat_entry_t cheat;

    jal_hook_t jal_hooks[MAX_JAL_HOOKS];
    int jal_hook_count = 0;

    while (cheats_get_next(&cheat_list, &cheat)) {
        cheat_t *c = &cheat.main;

        // If this is a conditional cheat, this points at its skip branch so the
        // sub-cheat handlers can back-patch the real offset once their block is
        // emitted -- no fragile hand-counted instruction totals.
        io32_t *cond_branch = NULL;

        switch (c->type) {
            case SPECIAL_WRITE_BYTE_ON_BOOT:
            case SPECIAL_WRITE_SHORT_ON_BOOT: {
                *patcher_p++ = I_LUI(REG_K0, A_BASE(c->address));
                *patcher_p++ = I_ORI(REG_K1, REG_ZERO, c->value);
                *patcher_p++ = IS_WIDTH_16(c->type) ? I_SH(REG_K1, A_OFFSET(c->address), REG_K0)
                                                    : I_SB(REG_K1, A_OFFSET(c->address), REG_K0);
                break;
            }
            case SPECIAL_CLEAR_MEMORY: {
                *patcher_p++ = I_LUI(REG_K0, 0xA000);
                *patcher_p++ = I_ORI(REG_K1, REG_K0, (0x300 - 0x200) - 4);
                *patcher_p++ = I_SW(REG_ZERO, 0x0200, REG_K0);
                *patcher_p++ = I_BNE(REG_K0, REG_K1, -2); // could be BNEL
                *patcher_p++ = I_ADDIU(REG_K0, REG_K0, 4);
                break;
            }
            // N/A
            case SPECIAL_SECONDARY_EXCEPTION_HANDLER:
            // not needed with N64FlashcartMenu's boot method
            case SPECIAL_SET_ENTRYPOINT_ADDR:
            // already handled
            case SPECIAL_SET_STORE_LOCATION:
            case SPECIAL_JUMP_ON_PRE_NMI:
            case SPECIAL_SET_JAL_HOOK_ENTRY: {
                // do nothing
                break;
            }
            case SPECIAL_DISABLE_EXPANSION_PAK: {
                *patcher_p++ = I_LUI(REG_K0, 0xA000);
                *patcher_p++ = I_LUI(REG_K1, 0x0040);
                *patcher_p++ = I_SW(REG_K1, 0x318, REG_K0);
                *patcher_p++ = I_SW(REG_K1, 0x3F0, REG_K0);
                break;
            }
            default: {
                if (IS_TYPE_REPEATER(c->type)) {
                    if ((!IS_TYPE_WRITE(cheat.sub.type)) || IS_CONDITION_GS_BUTTON(cheat.sub.type)) {
                        continue;
                    }

                    int count = ((c->address >> 8) & 0xFF);
                    int step = (c->address & 0xFF);
                    int16_t increment = (int16_t)(c->value);

                    c = &cheat.sub;

                    for (int i = 0; i < count; i++) {
                        *engine_p++ = I_LUI(REG_K0, A_BASE(c->address));
                        *engine_p++ = I_ORI(REG_K1, REG_ZERO, c->value);
                        *engine_p++ = IS_WIDTH_16(c->type) ? I_SH(REG_K1, A_OFFSET(c->address), REG_K0)
                                                        : I_SB(REG_K1, A_OFFSET(c->address), REG_K0);

                        c->address += step;
                        c->value += increment;
                    }

                    continue;
                }

                if (IS_TYPE_CONDITIONAL(c->type)) {
                    if ((!IS_TYPE_WRITE(cheat.sub.type)) || IS_CONDITION_GS_BUTTON(cheat.sub.type)) {
                        continue;
                    }
                    
                    *engine_p++ = I_LUI(REG_K0, A_BASE(c->address));
                    *engine_p++ = IS_WIDTH_16(c->type) ? I_LHU(REG_K0, A_OFFSET(c->address), REG_K0)
                                                    : I_LBU(REG_K0, A_OFFSET(c->address), REG_K0);
                    *engine_p++ = I_ORI(REG_K1, REG_ZERO, c->value & (IS_WIDTH_16(c->type) ? 0xFFFF : 0xFF));

                    // Emit a placeholder for the skip branch; the sub-cheat handler
                    // back-patches the real offset once its block length is known.
                    cond_branch = engine_p;
                    *engine_p++ = I_NOP();

                    c = &cheat.sub;
                }

                if (c->type == SPECIAL_JAL_HOOK) {
                    if (!has_jal_hook_entry || jal_hook_count == MAX_JAL_HOOKS) {
                        continue;
                    }

                    // Rewrites the game's `jal original` at the cheat address to
                    // `jal stub`, where stub is a few instructions emitted after
                    // the engine body (below the loop) that pass this hook's slot
                    // and value on to the payload's jal_hook_entry. That calls
                    // jal_hook_c(value), then tail-jumps to the original target,
                    // which returns straight to the game.
                    //
                    // Done every exception rather than once at boot, as the call
                    // site may not be loaded yet (or may be reloaded later):
                    //  - already our `jal`: nothing to do
                    //  - some other `jal`: save it in the slot (the stub reads
                    //    the original target from it), then write ours
                    //  - anything else: not code we can hook (yet), leave it
                    // Every `jal` word and slot address is a placeholder here,
                    // back-patched once the stub's final address is known.
                    //
                    // KSEG0 call sites only: a `jal` can't leave its 256 MB
                    // region, so one in TLB-mapped code can't reach the stub.
                    jal_hook_t *hook = &jal_hooks[jal_hook_count++];
                    hook->value = c->value;

                    uint32_t site = (c->address & 0x007FFFFF) | 0x80000000;

                    *engine_p++ = I_LUI(REG_K0, A_BASE(site));
                    *engine_p++ = I_LW(REG_K1, A_OFFSET(site), REG_K0);  // k1 = instruction at the call site

                    hook->compare_hi = engine_p;
                    *engine_p++ = I_NOP();                              // patched: LUI k0, our jal hi
                    hook->compare_lo = engine_p;
                    *engine_p++ = I_NOP();                              // patched: ORI k0, our jal lo

                    io32_t *hooked_branch = engine_p;
                    *engine_p++ = I_NOP();                              // patched: BEQ k1,k0,end (already hooked)
                    *engine_p++ = I_SRL(REG_K0, REG_K1, 26);            // k0 = opcode (harmless if taken)
                    *engine_p++ = I_ADDIU(REG_K0, REG_K0, -OP_JAL);

                    io32_t *not_jal_branch = engine_p;
                    *engine_p++ = I_NOP();                              // patched: BNE k0,0,end (not a jal)
                    *engine_p++ = I_NOP();

                    hook->slot_hi = engine_p;
                    *engine_p++ = I_NOP();                              // patched: LUI k0, slot base
                    hook->slot_lo = engine_p;
                    *engine_p++ = I_NOP();                              // patched: SW k1, slot offset(k0)

                    hook->write_hi = engine_p;
                    *engine_p++ = I_NOP();                              // patched: LUI k1, our jal hi
                    hook->write_lo = engine_p;
                    *engine_p++ = I_NOP();                              // patched: ORI k1, our jal lo

                    // Write our jal over the original, and push it through to
                    // the instruction stream
                    *engine_p++ = I_LUI(REG_K0, A_BASE(site));
                    *engine_p++ = I_SW(REG_K1, A_OFFSET(site), REG_K0);
                    *engine_p++ = I_CACHE(HIT_WRITE_BACK_D, A_OFFSET(site), REG_K0);
                    *engine_p++ = I_CACHE(HIT_INVALIDATE_I, A_OFFSET(site), REG_K0);

                    *hooked_branch = I_BEQ(REG_K1, REG_K0, (engine_p - hooked_branch) - 1);
                    *not_jal_branch = I_BNE(REG_K0, REG_ZERO, (engine_p - not_jal_branch) - 1);

                    // Back-patch the outer conditional's skip branch (if any).
                    if (cond_branch) {
                        int off = (engine_p - cond_branch) - 1;
                        *cond_branch = IS_CONDITION_NOT_EQUAL(cheat.main.type)
                            ? I_BEQ(REG_K0, REG_K1, off)
                            : I_BNE(REG_K0, REG_K1, off);
                    }

                    continue;
                }

                if (c->type == SPECIAL_JUMP) {
                    // Jump target comes from the cheat address.
                    // Force cached KSEG0: cheats_get_next masks raw[0] with
                    // 0xA07FFFFF, so the 0xBD type byte's bits 31/29 leak in and
                    // would otherwise yield an UNCACHED 0xA04xxxxx target.
                    uint32_t jump_target = (c->address & 0x007FFFFF) | 0x80000000;

                    // Jump to the resident payload. With no outer conditional this
                    // fires every frame; the payload takes over so control never
                    // returns. k0 is also the outer BNE's delay slot when a
                    // conditional gates this: harmless if that branch taken.
                    //
                    // The cheat's 16-bit value rides into the payload in k1, set
                    // from the jump's delay slot so it only runs on the fire path.
                    // k1 is dead here, and k0/k1 are the kernel-reserved pair, so
                    // no game state is at risk. The payload MUST consume k1 before
                    // anything re-enables interrupts or takes an exception --
                    // either would clobber it with no visible failure.
                    *engine_p++ = I_LUI(REG_K0, jump_target >> 16);        // k0 = target hi
                    *engine_p++ = I_ORI(REG_K0, REG_K0, jump_target & 0xFFFF); // k0 = target
                    *engine_p++ = I_JR(REG_K0);
                    *engine_p++ = I_ORI(REG_K1, REG_ZERO, c->value);       // k1 = cheat value

                    // An outer conditional skips past the jump block when its
                    // condition is false.
                    if (cond_branch) {
                        int off = (engine_p - cond_branch) - 1;
                        *cond_branch = IS_CONDITION_NOT_EQUAL(cheat.main.type)
                            ? I_BEQ(REG_K0, REG_K1, off)
                            : I_BNE(REG_K0, REG_K1, off);
                    }

                    continue;
                }

                if (IS_TYPE_WRITE(c->type)) {
                    if (IS_CONDITION_GS_BUTTON(c->type)) {
                        continue;
                    }

                    *engine_p++ = I_LUI(REG_K0, A_BASE(c->address));
                    *engine_p++ = I_ORI(REG_K1, REG_ZERO, c->value);
                    *engine_p++ = IS_WIDTH_16(c->type) ? I_SH(REG_K1, A_OFFSET(c->address), REG_K0)
                                                    : I_SB(REG_K1, A_OFFSET(c->address), REG_K0);

                    // Back-patch the outer conditional's skip branch (if any).
                    if (cond_branch) {
                        int off = (engine_p - cond_branch) - 1;
                        *cond_branch = IS_CONDITION_NOT_EQUAL(cheat.main.type)
                            ? I_BEQ(REG_K0, REG_K1, off)
                            : I_BNE(REG_K0, REG_K1, off);
                    }

                    continue;
                }
            }
        }
    }

    *engine_p++ = I_J(RELOCATED_EXCEPTION_HANDLER_ADDRESS);
    *engine_p++ = I_NOP();

    // SPECIAL_JAL_HOOK stubs. Past the jump above, so the engine never runs into
    // them; they're reached only from the game's rewritten `jal`, and are copied
    // to the final engine address along with everything else. Each hands its
    // slot (the game's original `jal`, saved by the engine) and its cheat value
    // to the payload's jal_hook_entry in t9/t8 -- caller-saved registers, dead
    // at any call site.
    for (int i = 0; i < jal_hook_count; i++) {
        jal_hook_t *hook = &jal_hooks[i];

        uint32_t stub_address = (uint32_t)(final_engine_address + (engine_p - engine_start));
        uint32_t slot_address = stub_address + (4 * sizeof(uint32_t));

        *engine_p++ = I_LUI(REG_T9, A_BASE(slot_address));
        *engine_p++ = I_ADDIU(REG_T9, REG_T9, A_OFFSET(slot_address));   // t9 = slot
        *engine_p++ = I_J(jal_hook_entry);
        *engine_p++ = I_ORI(REG_T8, REG_ZERO, hook->value);              // t8 = cheat value
        *engine_p++ = 0;                                                 // slot: the original jal

        uint32_t jal_stub = I_JAL(stub_address);

        *hook->compare_hi = I_LUI(REG_K0, jal_stub >> 16);
        *hook->compare_lo = I_ORI(REG_K0, REG_K0, jal_stub & 0xFFFF);
        *hook->slot_hi = I_LUI(REG_K0, A_BASE(slot_address));
        *hook->slot_lo = I_SW(REG_K1, A_OFFSET(slot_address), REG_K0);
        *hook->write_hi = I_LUI(REG_K1, jal_stub >> 16);
        *hook->write_lo = I_ORI(REG_K1, REG_K1, jal_stub & 0xFFFF);
    }

    uint32_t j_engine_from_handler = I_J((uint32_t)(final_engine_address));

    // Copy engine to the final location
    *patcher_p++ = I_LUI(REG_T3, A_BASE((uint32_t)(engine_start)));
    *patcher_p++ = I_ADDIU(REG_T3, REG_T3, A_OFFSET((uint32_t)(engine_start)));

    *patcher_p++ = I_LUI(REG_T4, A_BASE((uint32_t)(engine_p)));
    *patcher_p++ = I_ADDIU(REG_T4, REG_T4, A_OFFSET((uint32_t)(engine_p)));

    *patcher_p++ = I_LUI(REG_T5, A_BASE((uint32_t)(final_engine_address)));
    *patcher_p++ = I_ADDIU(REG_T5, REG_T5, A_OFFSET((uint32_t)(final_engine_address)));

    *patcher_p++ = I_ORI(REG_T6, REG_ZERO, 0);

    *patcher_p++ = I_LW(REG_K1, 0, REG_T3);
    *patcher_p++ = I_SW(REG_K1, 0, REG_T5);
    *patcher_p++ = I_ADDIU(REG_T3, REG_T3, 4);
    *patcher_p++ = I_ADDIU(REG_T5, REG_T5, 4);
    *patcher_p++ = I_BNE(REG_T3, REG_T4, -5);
    *patcher_p++ = I_ADDIU(REG_T6, REG_T6, 4);

    // Force write and invalidate instruction cache
    *patcher_p++ = I_LUI(REG_T5, A_BASE((uint32_t)(final_engine_address)));
    *patcher_p++ = I_ADDIU(REG_T5, REG_T5, A_OFFSET((uint32_t)(final_engine_address)));

    *patcher_p++ = I_CACHE(HIT_WRITE_BACK_D, 0, REG_T5);
    *patcher_p++ = I_CACHE(HIT_INVALIDATE_I, 0, REG_T5);
    *patcher_p++ = I_ADDIU(REG_T6, REG_T6, -D_CACHE_LINE_SIZE);
    *patcher_p++ = I_BGTZ(REG_T6, -4);
    *patcher_p++ = I_ADDIU(REG_T5, REG_T5, D_CACHE_LINE_SIZE);

    // Write jump instruction to the exception handler
    *patcher_p++ = I_LUI(REG_K0, A_BASE(EXCEPTION_HANDLER_ADDRESS));
    *patcher_p++ = I_ADDIU(REG_K0, REG_K0, A_OFFSET(EXCEPTION_HANDLER_ADDRESS));

    *patcher_p++ = I_LUI(REG_K1, j_engine_from_handler >> 16);
    *patcher_p++ = I_ORI(REG_K1, REG_K1, j_engine_from_handler);
    *patcher_p++ = I_SW(REG_K1, 0, REG_K0);
    *patcher_p++ = I_SW(REG_ZERO, 4, REG_K0);

    *patcher_p++ = I_CACHE(HIT_WRITE_BACK_D, 0, REG_K0);
    *patcher_p++ = I_CACHE(HIT_INVALIDATE_I, 0, REG_K0);

    // Set watch exception on address 0x80000180
    *patcher_p++ = I_ORI(REG_K1, REG_ZERO, EXCEPTION_HANDLER_ADDRESS | WATCHLO_W);
    *patcher_p++ = I_MTC0(REG_K1, C0_REG_WATCH_LO);
    *patcher_p++ = I_MTC0(REG_ZERO, C0_REG_WATCH_HI);

    // Jump back to the game code
    *patcher_p++ = I_JR(REG_T1);
    *patcher_p++ = I_NOP();

    cheats_update_cache(engine_start, engine_p);
    cheats_update_cache(patcher_start, patcher_p);

    return true;
}

bool cheats_install (cic_type_t cic_type, uint32_t *cheat_list) {
    return cheats_install_impl(cic_type, cheat_list, true);
}

bool cheats_install_hle (uint32_t *cheat_list) {
    return cheats_install_impl(CIC_UNKNOWN, cheat_list, false);
}

uint32_t cheats_patcher_address (void) {
    return PATCHER_ADDRESS;
}
