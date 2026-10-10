/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <fatfs/ff.h>

#include <dir.h>
#include <malloc.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

#include "utils/fs.h"

/**
 * `DirectoryScanner` with the FatFs path rebuilt on f_readdir_bulk().
 *
 * f_readdir_obj() reads the directory table a sector at a time, which costs one
 * read command per sector; f_readdir_bulk() fills a multi-sector buffer per
 * command instead, and reports every entry through a callback. That inverts the
 * iteration, so the whole directory is collected up front in open() and next()
 * then walks what was collected -- the API stays identical from the outside.
 *
 * The emulator serves the library out of DragonFS instead ("rom:/sd/"), which
 * has neither bulk reads nor locators, so that falls back to dir_findfirst()/
 * dir_findnext() exactly as `DirectoryScanner` does.
 */
class DirectoryScanner {
private:
    /**
     * One collected entry. Names are packed end to end in `nameArena` rather
     * than padded out to the longest a name could be, which would be 256 bytes
     * apiece over a directory of hundreds.
     */
    struct Record {
        uint32_t nameOffset;
        uint32_t nameLength;
        uint32_t size;
        FFOBJID fileObject;
        bool isDirectory;
    };

    // Read-ahead handed to f_readdir_bulk(). Sectors per read command is the
    // entire point of the call, so this sits far above FF_RDB_MIN_SECT.
    static constexpr UINT BULK_SECTORS = 64;

    std::vector<Record> records;
    std::vector<char> nameArena;
    size_t recordIndex = 0;
    
    dir_t entry = {};
    const char* directoryPath = nullptr;

public:
    // The entry the scanner is currently sitting on
    const char* name = nullptr;
    // Length of `name`, which the scanner has to walk anyway
    uint32_t nameLength = 0;
    uint32_t size = 0;
    bool isDirectory = false;

    FFOBJID fileObject = {};

    bool hasFileObject() const { return fileObject.fs != nullptr; }

    ~DirectoryScanner() { close(); }

    void close() {
        releaseEntries();
    }

    /**
     * Opens `path` for reading. Returns false when it's missing or empty.
     */
    bool open(const char* path) {
        close();

        directoryPath = path;

        return openBulk(path);
    }

    /**
     * Advances to the next entry. Returns false once the directory is spent.
     */
    bool next() {
        if (recordIndex >= records.size()) {
            releaseEntries();

            return false;
        }

        const Record& record = records[recordIndex++];

        name = nameArena.data() + record.nameOffset;
        nameLength = record.nameLength;
        size = record.size;
        isDirectory = record.isDirectory;
        fileObject = record.fileObject;

        return true;
    }

private:
    void readCurrentEntry() {
        name = entry.d_name;
        nameLength = (uint32_t)strlen(entry.d_name);
        size = (uint32_t)entry.d_size;
        isDirectory = (entry.d_type == DT_DIR);
    }

    /**
     * Reads the whole directory in one f_readdir_bulk() pass, then hands the
     * first entry back through `next()`.
     */
    bool openBulk(const char* path) {
        // Only needed for the duration of the scan: every entry is copied out
        // before f_readdir_bulk() returns, and the locators it captures are
        // independent of the directory handle (which is what f_open_obj() is
        // for), so neither outlives this function.
        DIR directory;

        FRESULT result = f_opendir(&directory, strip_fs_prefix((char*)path));

        if (result != FR_OK) {
            debugf("[DirectoryScanner] Failed to open %s (%i)\n", path, (int)result);

            return false;
        }

        // 8-byte aligned, as the sector reads filling it go over the PI bus
        void* buffer = memalign(8, BULK_SECTORS * FF_MAX_SS);

        if (buffer == nullptr) {
            debugf("[DirectoryScanner] Failed to allocate the read-ahead buffer\n");

            f_closedir(&directory);

            return false;
        }

        records.reserve(64);
        nameArena.reserve(64 * 32);

        uint32_t startTicks = get_ticks();

        result = f_readdir_bulk(&directory, buffer, BULK_SECTORS, 0, collectEntry, this);

        uint32_t elapsedTicks = get_ticks() - startTicks;

        debugf("[DirectoryScanner] Read %i entries from %s in %.3fms\n",
            (int)records.size(), path, TICKS_TO_US(elapsedTicks) / 1000.0f);

        free(buffer);
        f_closedir(&directory);

        if (result != FR_OK) {
            debugf("[DirectoryScanner] Failed to read %s (%i)\n", path, (int)result);

            releaseEntries();

            return false;
        }

        return next();
    }

    // `fno` and `obj` are valid only for the duration of the call, so both are
    // copied out here.
    int collect(const FILINFO* fno, const FFOBJID* obj) {
        size_t length = strlen(fno->fname);

        Record record;

        record.nameOffset = (uint32_t)nameArena.size();
        record.nameLength = (uint32_t)length;
        record.size = (uint32_t)fno->fsize;
        record.fileObject = *obj;
        record.isDirectory = (fno->fattrib & AM_DIR) != 0;

        nameArena.insert(nameArena.end(), fno->fname, fno->fname + length + 1);
        records.push_back(record);

        // Carry on scanning
        return 0;
    }

    static int collectEntry(const FILINFO* fno, const FFOBJID* obj, void* arg) {
        return ((DirectoryScanner*)arg)->collect(fno, obj);
    }

    void releaseEntries() {
        // `name` points into the arena, so it cannot outlive it
        name = nullptr;
        nameLength = 0;

        records.clear();
        records.shrink_to_fit();

        nameArena.clear();
        nameArena.shrink_to_fit();

        recordIndex = 0;
    }
};
