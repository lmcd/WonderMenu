/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ELFFile.h"

// From loader.c in libdragon
#define PF_N64_COMPRESSED 0x1000

uint32_t ELFFile::romAddress() {
    return dfs_rom_addr(filename.c_str());
}

bool ELFFile::exists() {
    return (romAddress() != 0);
}

void ELFFile::stage() {
    if (!is_memory_expanded()) {
        debugf("[ELFFile] Expansion Pak required to load ELF payloads\n");
        return;
    }

    if (!exists()) {
        debugf("[ELFFile] Payload doesn't exist! %s\n", filename.c_str());
        return;
    }

    Elf32_Ehdr header;

    uint32_t elfBase = romAddress();

    data_cache_hit_writeback_invalidate(&header, sizeof(header));
    dma_read_raw_async(&header, elfBase, sizeof(header));
    dma_wait();

    bool hasELFMagic =
        header.e_ident[EI_MAG0] == ELFMAG0 &&
        header.e_ident[EI_MAG1] == ELFMAG1 &&
        header.e_ident[EI_MAG2] == ELFMAG2 &&
        header.e_ident[EI_MAG3] == ELFMAG3;

    assertf(
        hasELFMagic,
        "No ELF at 0x%08lx (read %02X %02X %02X %02X). Point rom_base at the raw "
        "payload.elf.stripped.",
        elfBase,
        header.e_ident[EI_MAG0], header.e_ident[EI_MAG1],
        header.e_ident[EI_MAG2], header.e_ident[EI_MAG3]
    );

    uint32_t entry = header.e_entry;
    uint32_t phoff = header.e_phoff;
    int phnum = header.e_phnum;

    assertf(
        phnum > 0 && phnum < 16,
        "Program header table entry count bad: %d", phnum
    );

    assertf(
        entry == 0x80401000,
        "Payload entry = %08lx. Expected 0x80401000", entry
    );

    Elf32_Phdr segment;

    int loadable = 0;
    for (int i = 0; i < phnum; i++) {
        uint32_t ph = elfBase + phoff + i * sizeof(Elf32_Phdr);

        data_cache_hit_writeback_invalidate(&segment, sizeof(segment));
        dma_read_raw_async(&segment, ph, sizeof(segment));
        dma_wait();

        if (segment.p_type != PT_LOAD) {
            continue;
        }

        loadable++;
    }

    assertf(
        loadable == 3,
        "Should be 3 PT_LOAD segments. %d found.", loadable
    );

    for (int i = 0; i < phnum; i++) {
        uint32_t ph = elfBase + phoff + i * sizeof(Elf32_Phdr);

        data_cache_hit_writeback_invalidate(&segment, sizeof(segment));
        dma_read_raw_async(&segment, ph, sizeof(segment));
        dma_wait();

        if (segment.p_type != PT_LOAD) {
            continue;
        }
        
        uint32_t offset = segment.p_offset;
        uint32_t vaddr  = segment.p_vaddr;
        uint32_t filesz = segment.p_filesz;
        uint32_t memsz  = segment.p_memsz;
        uint32_t flags  = segment.p_flags;

        if (memsz == 0) {
            continue;
        }

        // Skip the exception-vectors segment at 0x80000000
        if (vaddr == 0x80000000) {
            continue;
        }

        // Make sure this isn't a libdragon-compressed ELF
        assertf(
            !(flags & PF_N64_COMPRESSED),
            "Segment %d is compressed (PF_N64_COMPRESSED). This loader needs "
            "an UNCOMPRESSED elf.", i
        );

        /* Sanity: destination must be inside RDRAM and reasonably sized.
           Note '>=' so the vectors segment at exactly 0x80000000 is accepted. */
        assertf(
            vaddr >= 0x80000000 && vaddr < 0x80800000 &&
            memsz < 0x800000 && filesz <= memsz,
            "Bad segment %d: vaddr=0x%08lx filesz=0x%lx memsz=0x%lx",
            i, vaddr, filesz, memsz
        );

        void* dst = (void *)vaddr;
        data_cache_hit_writeback_invalidate(dst, memsz);   /* before DMA */
        uint32_t length = (filesz + 1) & ~1u;
        
        if (length) {
            dma_read(dst, elfBase + offset, length);
        }

        if (memsz > filesz) {
            memset(UncachedAddr((uint8_t *)dst + filesz), 0, memsz - filesz);
        }
        
        inst_cache_hit_invalidate(dst, memsz);
    }

    debugf("[ELFFile] Loaded Payload at 0x%08lx\n", entry);
}
