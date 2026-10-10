#include "reboot.h"


#define IPL3_ENTRY          0xA4000040
#define REBOOT_ADDRESS      0xA4001000
#define STACK_ADDRESS       0xA4001FF0

#define RI_ADDRESS          0xA4700000

#define RI_SELECT           0x0C
#define RI_REFRESH          0x10

#define PI_ADDRESS          0xA4600000

#define PI_MADDR            0x00
#define PI_PADDR            0x04
#define PI_WDMA             0x0C
#define PI_SR               0x10

#define PI_SR_DMA_BUSY      0x01
#define PI_SR_IO_BUSY       0x02

# How much of the game ROM IPL3 copies into RDRAM before jumping to it.
#define GAME_LOAD_SIZE      0x100000

# VR4300 cache geometry. The `cache` opcode field is (op << 2) | cache, so
# 0x8 is Index_Store_Tag_I and 0x9 is Index_Store_Tag_D. Storing a zeroed
# TagLo marks each line invalid, which is exactly what IPL3 does.
#define D_CACHE_SIZE        0x2000
#define D_CACHE_LINE        16
#define I_CACHE_SIZE        0x4000
#define I_CACHE_LINE        32
#define INDEX_STORE_TAG_I   0x8
#define INDEX_STORE_TAG_D   0x9

#define SP_ADDRESS          0xA4040000

#define SP_MEM_ADDR         0x00
#define SP_DRAM_ADDR        0x04
#define SP_RD_LEN           0x08
#define SP_WR_LEN           0x0C
#define SP_STATUS           0x10

#define SP_STATUS_DMA_BUSY  0x04
#define SP_STATUS_DMA_FULL  0x08

# A single SP DMA tops out at 4 KiB, and DMEM is split in half so that a queued
# read never lands in the half a pending write is still draining.
#define DMEM_CHUNK_SIZE     2048
#define DMEM_HALF_A         0x0000
#define DMEM_HALF_B         0x0800

# The DMA engine addresses RDRAM with the low 24 bits.
#define RDRAM_ADDRESS_MASK  0x00FFFFFF


.set noat
.section .text.reboot, "ax", %progbits

reboot_start:
    .global reboot_start

# NOTE: CIC x105 requirement
ipl2:
.set noreorder
    lui $t5, 0xBFC0
1:
    lw $t0, 0x7FC($t5)
    addiu $t5, $t5, 0x7C0
    andi $t0, $t0, 0x80
    bnel $t0, $zero, 1b
    lui $t5, 0xBFC0
    lw $t0, 0x24($t5)
    lui $t3, 0xB000
.set reorder

reboot_entry:
    .set reboot, REBOOT_ADDRESS + (. - reboot_start)
    .global reboot

    li $sp, STACK_ADDRESS

    li $t0, BOOT_PATH_HLE_CART
    beq $a1, $t0, hle_boot          # Load the game ourselves, bypassing IPL3

    li $t0, BOOT_PATH_HLE_STAGED
    beq $a1, $t0, hle_boot_staged   # As above, from the copy already in RDRAM

    bnez $a0, reset_rdram_skip  # Skip when cheats are enabled
    bnez $s5, reset_rdram_skip  # Skip when reset type is set to NMI

reset_rdram:
    li $t0, RI_ADDRESS

    sw $zero, RI_REFRESH($t0)
    sw $zero, RI_SELECT($t0)
reset_rdram_skip:

detect_console_region:
    li $t0, 1
    beq $s4, $zero, pal_console
    beq $s4, $t0, ntsc_console
    b mpal_console

pal_console:
    li $ra, 0xA4001554
    b prepare_registers

ntsc_console:
    li $ra, 0xA4001550
    b prepare_registers

mpal_console:
    li $ra, 0xA4001554

prepare_registers:
    move $at, $zero
    move $v0, $zero
    move $v1, $zero
    move $a0, $zero
    move $a1, $zero
    move $a2, $zero
    move $a3, $zero
    move $t0, $zero
    move $t1, $zero
    li $t2, 0x40
    move $t4, $zero
    move $t5, $zero
    move $t6, $zero
    move $t7, $zero
    move $s0, $zero
    move $s1, $zero
    move $s2, $zero
    move $t8, $zero
    move $t9, $zero
    move $k0, $zero
    move $k1, $zero
    move $gp, $zero
    move $fp, $zero

run_ipl3:
    li $t3, IPL3_ENTRY
    jr $t3

# HLE boot: do IPL3's job ourselves rather than running the cartridge's copy.
# This has to execute from IMEM, because the DMA below can land on top of
# whatever menu code was resident in RDRAM.
#
#   a1 = non-zero (selects this path)
#   a2 = game load/entry address (KSEG0, from ROM header offset 0x08)
#   a3 = cartridge source address of the game code (ROM offset 0x1000)
#   t8 = where to jump once loaded: the cheat patcher, or a2 when no cheats
#   s3-s7 = boot flags, already published to the OS info block by boot()
hle_boot:
    li $t0, PI_ADDRESS

    # Let any DMA the menu left in flight retire before reprogramming PI.
wait_pi_idle:
    lw $t1, PI_SR($t0)
    andi $t1, $t1, (PI_SR_DMA_BUSY | PI_SR_IO_BUSY)
    bnez $t1, wait_pi_idle

    # PI takes physical addresses, so mask off the segment bits.
    li $t2, 0x1FFFFFFF

    and $t3, $a2, $t2
    sw $t3, PI_MADDR($t0)

    and $t3, $a3, $t2
    sw $t3, PI_PADDR($t0)

    # Writing WDMA starts the transfer (cartridge -> RDRAM).
    li $t3, (GAME_LOAD_SIZE - 1)
    sw $t3, PI_WDMA($t0)

wait_pi_dma:
    lw $t1, PI_SR($t0)
    andi $t1, $t1, PI_SR_DMA_BUSY
    bnez $t1, wait_pi_dma

    b hle_enter_game

# HLE boot from the copy already staged in the Expansion Pak, which is a great
# deal quicker than going back to the cart: the slow PI transfer happened frames
# ago, behind the launch animation, leaving only an RDRAM-to-RDRAM copy here.
#
# The RSP's DMA engine does that copy via DMEM, alternating between DMEM's two
# halves and gating on DMA_FULL rather than DMA_BUSY. The engine holds one
# transfer running plus one queued and runs them strictly in order, so each
# chunk has fully landed in DMEM before the write reading it back out begins,
# and the engine is never left idle between transfers.
#
#   a1 = BOOT_PATH_HLE_STAGED
#   a2 = game load/entry address (KSEG0, from ROM header offset 0x08)
#   a3 = staged copy of the game's first MiB (GAME_STAGING_ADDRESS)
#   t8 = where to jump once copied: the cheat patcher, or a2 when no cheats
#   s3-s7 = boot flags, already published to the OS info block by boot()
#
# DMEM is free scratch here: the HLE paths never load IPL3 into it, and the RSP
# was halted before we were entered.
hle_boot_staged:
    li $t0, SP_ADDRESS

    li $t9, RDRAM_ADDRESS_MASK
    and $t4, $a2, $t9           # destination
    and $t5, $a3, $t9           # source

    li $t6, GAME_LOAD_SIZE      # bytes still to copy
    li $t7, DMEM_HALF_A         # half of DMEM this chunk passes through

staged_copy_chunk:
    # RDRAM -> DMEM
wait_sp_read_slot:
    lw $t1, SP_STATUS($t0)
    andi $t1, $t1, SP_STATUS_DMA_FULL
    bnez $t1, wait_sp_read_slot

    sw $t7, SP_MEM_ADDR($t0)
    sw $t5, SP_DRAM_ADDR($t0)
    li $t1, (DMEM_CHUNK_SIZE - 1)
    sw $t1, SP_RD_LEN($t0)

    # DMEM -> RDRAM, queued behind the read above
wait_sp_write_slot:
    lw $t1, SP_STATUS($t0)
    andi $t1, $t1, SP_STATUS_DMA_FULL
    bnez $t1, wait_sp_write_slot

    sw $t7, SP_MEM_ADDR($t0)
    sw $t4, SP_DRAM_ADDR($t0)
    li $t1, (DMEM_CHUNK_SIZE - 1)
    sw $t1, SP_WR_LEN($t0)

    addiu $t4, $t4, DMEM_CHUNK_SIZE
    addiu $t5, $t5, DMEM_CHUNK_SIZE
    addiu $t6, $t6, -DMEM_CHUNK_SIZE

    xori $t7, $t7, DMEM_HALF_B  # flip to the other half

    bgtz $t6, staged_copy_chunk

wait_sp_dma:
    lw $t1, SP_STATUS($t0)
    andi $t1, $t1, SP_STATUS_DMA_BUSY
    bnez $t1, wait_sp_dma

# Shared tail for both HLE paths: the game is in RDRAM, hand control to it.
hle_enter_game:
    # The game arrived by DMA, behind both caches. Invalidate them so the
    # freshly loaded code and data are what actually gets executed and read.
    # Dirty lines are discarded rather than written back: everything the boot
    # path still needs was either written uncached or already flushed.
    mtc0 $zero, $28             # TagLo
    mtc0 $zero, $29             # TagHi

    li $t0, 0x80000000
    li $t1, 0x80000000 + D_CACHE_SIZE
invalidate_dcache:
    cache INDEX_STORE_TAG_D, 0($t0)
    addiu $t0, $t0, D_CACHE_LINE
    bne $t0, $t1, invalidate_dcache

    li $t0, 0x80000000
    li $t1, 0x80000000 + I_CACHE_SIZE
invalidate_icache:
    cache INDEX_STORE_TAG_I, 0($t0)
    addiu $t0, $t0, I_CACHE_LINE
    bne $t0, $t1, invalidate_icache

    # The cheat patcher ends in `jr $t1`, so the entry point goes there whether
    # or not we are routing through it.
    move $t3, $t8
    move $t1, $a2

    move $at, $zero
    move $v0, $zero
    move $v1, $zero
    move $a0, $zero
    move $a1, $zero
    move $a2, $zero
    move $a3, $zero
    move $t0, $zero
    li $t2, 0x40
    move $t4, $zero
    move $t5, $zero
    move $t6, $zero
    move $t7, $zero
    move $s0, $zero
    move $s1, $zero
    move $s2, $zero
    move $t8, $zero
    move $t9, $zero
    move $k0, $zero
    move $k1, $zero
    move $gp, $zero
    move $fp, $zero
    move $ra, $zero

    jr $t3

    .set reboot_size, (. - reboot_start)
    .global reboot_size
