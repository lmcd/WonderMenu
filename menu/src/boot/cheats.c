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
#define PATCHER_ADDRESS (0x80700000)
#define ENGINE_TEMPORARY_ADDRESS (PATCHER_ADDRESS + 0x10000)
#define DEFAULT_ENGINE_ADDRESS (0x807C5C00)

// The payload reserves the first 64 bytes at 0x80400000 (payload_settings /
// PayloadData) for its own settings. The resume trampoline's game_ctx snapshot
// buffer -- and every O_* offset / flag the engine writes into it -- therefore
// sits this many bytes higher. MUST match the payload_settings size in resume.S.
#define GAME_CTX_OFFSET (64)

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
    SPECIAL_GUARDED_JUMP = 0xBF, /**< Edge-detected jump to the resident payload (once per press) */
    SPECIAL_SET_BITS = 0x40, /**< [addr] |= value mask   (0x40 byte, 0x41 halfword) */
    SPECIAL_CLEAR_BITS = 0x42, /**< [addr] &= ~value mask (0x42 byte, 0x43 halfword) */
    SPECIAL_BIT_TEST_CLEAR = 0x60, /**< run sub if (addr & mask)==0 (0x60 byte, 0x61 halfword) */
    SPECIAL_BIT_TEST_SET = 0x62, /**< run sub if (addr & mask)!=0 (0x62 byte, 0x63 halfword) */
} cheat_type_special_t;

#define IS_WIDTH_16(t) ((t) & (1 << 0))
#define IS_CONDITION_NOT_EQUAL(t) ((t) & (1 << 1))
#define IS_CONDITION_GS_BUTTON(t) ((t) & (1 << 3))

#define IS_TYPE_REPEATER(t) ((t) == 0x50)
#define IS_TYPE_WRITE(t) ((((t)&0xF0) == 0x80) || (((t)&0xF0) == 0xA0))
#define IS_TYPE_CONDITIONAL(t) (((t)&0xF0) == 0xD0)
// Read-modify-write bitmask ops. Low bit selects width (IS_WIDTH_16), so each is
// a pair: 0x40/0x41 set (|= mask), 0x42/0x43 clear (&= ~mask).
#define IS_TYPE_SET_BITS(t) (((t)&0xFE) == 0x40)
#define IS_TYPE_CLEAR_BITS(t) (((t)&0xFE) == 0x42)
// Bit-test conditional: gate the sub-cheat on (mem & mask) vs 0. bit0 = width,
// bit1 = IS_CONDITION_NOT_EQUAL, so 0x62/0x63 run the sub when the masked bits
// are SET and 0x60/0x61 when they are CLEAR.
#define IS_TYPE_BIT_COND(t) (((t)&0xF0) == 0x60)

#define IS_DOUBLE_ENTRY(t) (IS_TYPE_CONDITIONAL(t) || IS_TYPE_REPEATER(t) || IS_TYPE_BIT_COND(t))

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
bool cheats_install (cic_type_t cic_type, uint32_t *cheat_list) {
    if (!cheat_list) {
        return false;
    }

    io32_t *engine_start = (io32_t *)(ENGINE_TEMPORARY_ADDRESS);
    io32_t *engine_p = engine_start;

    io32_t *patcher_start = (io32_t *)(PATCHER_ADDRESS);
    io32_t *patcher_p = patcher_start;

    if (cheats_patch_ipl3(cic_type, patcher_start)) {
        return false;
    }

    io32_t *final_engine_address = cheats_get_engine_address(cheat_list);

    // Original watch exception handler code written by Jay Oster 'Parasyte'
    // https://github.com/parasyte/alt64/blob/master/utils.c#L1024-L1054

    uint32_t ori_placeholder_instruction = I_ORI(REG_ZERO, REG_K0, A_OFFSET(RELOCATED_EXCEPTION_HANDLER_ADDRESS));
    uint32_t ori_placeholder_address = (uint32_t)(final_engine_address + 20);

    // Load cause register
    *engine_p++ = I_MFC0(REG_K0, C0_REG_CAUSE);

    // Disable watch exception when reset button is pressed
    *engine_p++ = I_ANDI(REG_K1, REG_K0, CAUSE_IRQ_PRE_NMI);
    *engine_p++ = I_BNEL(REG_K1, REG_ZERO, 1);
    *engine_p++ = I_MTC0(REG_ZERO, C0_REG_WATCH_LO);

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
            case SPECIAL_SET_STORE_LOCATION: {
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
                    // if ((!IS_TYPE_WRITE(cheat.sub.type)) || IS_CONDITION_GS_BUTTON(cheat.sub.type)) {
                    //     continue;
                    // }
                    
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

                if (IS_TYPE_BIT_COND(c->type)) {
                    // Bit-test conditional: gate the sub-cheat on whether the bits
                    // in `value` (the mask) are set at [address], ignoring all other
                    // bits. e.g. type 0x62 (byte, set) value 0x20 -> sub runs while
                    // the N64 L button is held, regardless of other buttons.
                    // Compute k0 = mem & mask, k1 = 0, then let the shared sub-cheat
                    // back-patch emit the skip branch: for 0x62/0x63 (NOT_EQUAL) that
                    // is BEQ k0,k1 -> skip when mask clear; for 0x60/0x61 it is BNE
                    // -> skip when mask set. Force cached KSEG0 like the other 0x4x/
                    // 0x6x types (their type byte doesn't carry the 0x80 base).
                    uint32_t target = (c->address & 0x007FFFFF) | 0x80000000;
                    uint16_t width_mask = IS_WIDTH_16(c->type) ? 0xFFFF : 0xFF;
                    uint16_t mask = c->value & width_mask;

                    *engine_p++ = I_LUI(REG_K0, A_BASE(target));
                    *engine_p++ = IS_WIDTH_16(c->type) ? I_LHU(REG_K0, A_OFFSET(target), REG_K0)
                                                       : I_LBU(REG_K0, A_OFFSET(target), REG_K0);
                    *engine_p++ = I_ANDI(REG_K0, REG_K0, mask);   // k0 = mem & mask
                    *engine_p++ = I_ORI(REG_K1, REG_ZERO, 0);     // k1 = 0 for the skip compare

                    cond_branch = engine_p;
                    *engine_p++ = I_NOP();

                    c = &cheat.sub;
                }

                if (c->type == SPECIAL_JUMP || c->type == SPECIAL_GUARDED_JUMP) {
                    bool guarded = (c->type == SPECIAL_GUARDED_JUMP);

                    // Jump target comes from the cheat address (e.g. 0x80400240).
                    // Force cached KSEG0: cheats_get_next masks raw[0] with
                    // 0xA07FFFFF, so the 0xBD/0xBF type byte's bits 31/29 leak in
                    // and would otherwise yield an UNCACHED 0xA04xxxxx target.
                    uint32_t jump_target = (c->address & 0x007FFFFF) | 0x80000000;

                    // Suspend-point gate: only fire the jump at a clean frame
                    // boundary -- when a VI (vblank) interrupt is pending AND no
                    // device completion (SP/PI/DP) is co-pending. Suspending across
                    // an unserviced RSP/RDP/PI completion (very common at vblank, as
                    // the frame's gfx task finishes just before VI) is the residual
                    // freeze window. Both checks branch past the whole block (back-
                    // patched below) so control chains on to the game handler
                    // unchanged. Clobbering k0/k1 is safe: any outer conditional
                    // already compared them, and the guarded path reloads k0.
                    // MI_INTR bits: SP=0x01 SI=0x02 AI=0x04 VI=0x08 PI=0x10 DP=0x20.
                    //
                    // GUARDED JUMPS ONLY. This gate exists for the suspend
                    // (0x80400240 -> trampoline_enter), which must land at a clean
                    // boundary. A plain SPECIAL_JUMP is the per-frame m64 input
                    // injection (0x80400248 -> m64_inject), which has to run on
                    // EVERY frame and doesn't suspend anything -- gating it means
                    // the VI check drops it to at most once per frame, and the
                    // SP/PI/DP check then drops most of those too (the comment
                    // above: completions are common right at vblank), so inputs
                    // stop reaching the game and playback does nothing.
                    io32_t *busy_branch = NULL;
                    io32_t *vi_branch = NULL;

                    if (guarded) {
                        *engine_p++ = I_LUI(REG_K0, 0xA430);           // k0 = MI base (0xA4300000)
                        *engine_p++ = I_LW(REG_K1, 0x0008, REG_K0);    // k1 = MI_INTR

                        *engine_p++ = I_ANDI(REG_K0, REG_K1, 0x0031);  // k0 = MI_INTR & (SP|PI|DP)
                        busy_branch = engine_p;
                        *engine_p++ = I_NOP();                          // patched: BNE k0,0,end (completion co-pending)
                        *engine_p++ = I_NOP();                          // branch delay slot

                        *engine_p++ = I_ANDI(REG_K1, REG_K1, 0x0008);  // k1 = MI_INTR & VI
                        vi_branch = engine_p;
                        *engine_p++ = I_NOP();                          // patched: BEQ k1,0,end (VI not pending)
                        *engine_p++ = I_NOP();                          // branch delay slot
                    }

                    // NOTE: the RDP-idle requirement is NOT enforced here (it would
                    // drop invocations, since the RDP is busy for much of gameplay).
                    // Instead trampoline_enter spins until the RDP drains before it
                    // suspends -- so the jump always fires and the wait is bounded.

                    io32_t *guard_branch = NULL;
                    if (guarded) {
                        // Edge-detect guard, independent of game memory. A flag
                        // word in game_ctx (offset 284, i.e. 0x80400040 + 284,
                        // reached below as 0xA0400000 + GAME_CTX_OFFSET + 284 via
                        // the UNCACHED mirror so it's coherent a frame later) records
                        // whether we've already fired for the CURRENT press. It's
                        // 0 after staging.
                        //  - condition true, flag clear -> set flag, fire (below)
                        //  - condition true, flag set   -> already fired this
                        //    press; the guard branch skips the block, leaving the
                        //    flag set
                        //  - condition false            -> the outer conditional
                        //    branch lands on the clear-flag stub and re-arms it
                        // Both branches are back-patched, so no hand-counted
                        // offsets. k0 here is also the outer BNE's delay slot:
                        // harmless if that branch was taken.
                        *engine_p++ = I_LUI(REG_K0, 0xA040);       // k0 = 0xA0400000 (uncached)
                        *engine_p++ = I_LW(REG_K1, GAME_CTX_OFFSET + 284, REG_K0);   // k1 = "already fired" flag
                        guard_branch = engine_p;                   // patched after the block
                        *engine_p++ = I_NOP();                     // placeholder: BNE k1,0,end
                        *engine_p++ = I_NOP();                     // guard BNE delay slot
                        *engine_p++ = I_ORI(REG_K1, REG_ZERO, 1);  // flag = 1
                        *engine_p++ = I_SW(REG_K1, GAME_CTX_OFFSET + 284, REG_K0);
                    }

                    // Jump to the resident payload. For SPECIAL_JUMP with no outer
                    // conditional this fires every frame; the payload takes over so
                    // control never returns. k0 is also the outer BNE's delay slot
                    // when a conditional gates this: harmless if that branch taken.
                    //
                    // The cheat's 16-bit value rides into the payload in k1, set
                    // from the jump's delay slot so it only runs on the fire path.
                    // k1 is dead here (the gates and the guard flag store are all
                    // done with it), and k0/k1 are the kernel-reserved pair, so no
                    // game state is at risk. The payload MUST consume k1 before
                    // anything re-enables interrupts or takes an exception --
                    // either would clobber it with no visible failure.
                    *engine_p++ = I_LUI(REG_K0, jump_target >> 16);        // k0 = target hi
                    *engine_p++ = I_ORI(REG_K0, REG_K0, jump_target & 0xFFFF); // k0 = target
                    *engine_p++ = I_JR(REG_K0);
                    *engine_p++ = I_ORI(REG_K1, REG_ZERO, c->value);       // k1 = cheat value

                    if (guarded) {
                        // Clear-flag stub: the outer conditional branches here when
                        // its condition is FALSE, re-arming the cheat for the next
                        // press. (The fire path above never reaches here -- it
                        // jumps into the payload.) k0 is already 0xA0400000 from
                        // the outer BNE's delay slot, but we reload it so this
                        // doesn't depend on that.
                        io32_t *clear_flag = engine_p;
                        *engine_p++ = I_LUI(REG_K0, 0xA040);
                        *engine_p++ = I_SW(REG_ZERO, GAME_CTX_OFFSET + 284, REG_K0);  // flag = 0

                        // Guard branch (flag already set, condition still true):
                        // skip to the END, past the clear-flag stub, so the flag
                        // stays set until the condition actually goes false.
                        *guard_branch = I_BNE(REG_K1, REG_ZERO,
                                              (engine_p - guard_branch) - 1);

                        // Outer conditional (condition false): branch to the stub.
                        if (cond_branch) {
                            int off = (clear_flag - cond_branch) - 1;
                            *cond_branch = IS_CONDITION_NOT_EQUAL(cheat.main.type)
                                ? I_BEQ(REG_K0, REG_K1, off)
                                : I_BNE(REG_K0, REG_K1, off);
                        }
                    } else if (cond_branch) {
                        // Ungated: an outer conditional simply skips past the jump
                        // block when its condition is false.
                        int off = (engine_p - cond_branch) - 1;
                        *cond_branch = IS_CONDITION_NOT_EQUAL(cheat.main.type)
                            ? I_BEQ(REG_K0, REG_K1, off)
                            : I_BNE(REG_K0, REG_K1, off);
                    }

                    // Gate back-patches: skip the whole block (chain to the game
                    // handler untouched) if a completion was co-pending, or if VI
                    // wasn't pending, at the top of the block. Only emitted for
                    // guarded jumps, so only patched for those.
                    if (busy_branch) {
                        *busy_branch = I_BNE(REG_K0, REG_ZERO, (engine_p - busy_branch) - 1);
                    }
                    if (vi_branch) {
                        *vi_branch = I_BEQ(REG_K1, REG_ZERO, (engine_p - vi_branch) - 1);
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

                if (IS_TYPE_SET_BITS(c->type) || IS_TYPE_CLEAR_BITS(c->type)) {
                    // Bitmask read-modify-write: change only the bits in `value`,
                    // leaving the rest of the target untouched. SET does |= mask,
                    // CLEAR does &= ~mask. The value is the mask -- e.g. type 0x43
                    // (clear, halfword) with value 0x0020 forces the N64 L button
                    // off without disturbing the other button bits.
                    // Force cached KSEG0: unlike 0x8x writes, these type bytes don't
                    // set bit 31, so the 0xA07FFFFF parse leaves the target outside
                    // KSEG0 (same normalization the jump/indexed handlers do).
                    bool set = IS_TYPE_SET_BITS(c->type);
                    uint32_t target = (c->address & 0x007FFFFF) | 0x80000000;
                    uint16_t width_mask = IS_WIDTH_16(c->type) ? 0xFFFF : 0xFF;
                    uint16_t mask = c->value & width_mask;

                    *engine_p++ = I_LUI(REG_K0, A_BASE(target));
                    *engine_p++ = IS_WIDTH_16(c->type) ? I_LHU(REG_K1, A_OFFSET(target), REG_K0)
                                                       : I_LBU(REG_K1, A_OFFSET(target), REG_K0);
                    *engine_p++ = set ? I_ORI(REG_K1, REG_K1, mask)
                                      : I_ANDI(REG_K1, REG_K1, (~mask) & width_mask);
                    *engine_p++ = IS_WIDTH_16(c->type) ? I_SH(REG_K1, A_OFFSET(target), REG_K0)
                                                       : I_SB(REG_K1, A_OFFSET(target), REG_K0);

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
