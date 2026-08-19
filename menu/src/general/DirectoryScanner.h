/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <dir.h>
#include <cstring>
#include <cstdint>

#ifdef N64
#include <libdragon.h>
#include <fatfs/ff.h>
#endif

#include "utils/fs.h"

/**
 * Walks a directory one entry at a time, capturing a locator for each file
 * where the filesystem can provide one so it can later be opened without a
 * directory lookup.
 *
 * FatFs is used directly when the path lives on the SD card. The emulator
 * serves the library out of DragonFS instead ("rom:/sd/"), which has no
 * locators, so that falls back to dir_findfirst()/dir_findnext().
 * TODO: when ares supports SD card emulation, we can simplify this.
 */
class DirectoryScanner {
private:
    #ifdef N64
    DIR directory = {};
    bool isFatDirectoryOpen = false;
    #endif

    dir_t entry = {};
    const char* directoryPath = nullptr;
    bool hasMoreEntries = false;

public:
    // The entry the scanner is currently sitting on
    const char* name = nullptr;
    uint32_t size = 0;
    bool isDirectory = false;

    #ifdef N64
    FFOBJID fileObject = {};

    bool hasFileObject() const { return fileObject.fs != nullptr; }
    #endif

    ~DirectoryScanner() { close(); }

    void close() {
        #ifdef N64
        if (isFatDirectoryOpen) {
            f_closedir(&directory);
            isFatDirectoryOpen = false;
        }
        #endif
    }

    /**
     * Opens `path` for reading. Returns false when it's missing or empty.
     */
    bool open(const char* path) {
        close();

        directoryPath = path;

        #ifdef N64
        // strip_fs_prefix() leaves the path relative to the default volume,
        // which is where the SD card is mounted
        if (strncmp(path, "sd:", 3) == 0) {
            FRESULT result = f_opendir(&directory, strip_fs_prefix((char*)path));

            if (result == FR_OK) {
                isFatDirectoryOpen = true;

                return next();
            }

            debugf("[DirectoryScanner] Failed to open %s (%i)\n", path, (int)result);

            return false;
        }
        #endif

        hasMoreEntries = (dir_findfirst(path, &entry) == 0);

        if (hasMoreEntries) {
            readCurrentEntry();
        }

        return hasMoreEntries;
    }

    /**
     * Advances to the next entry. Returns false once the directory is spent.
     */
    bool next() {
        #ifdef N64
        if (isFatDirectoryOpen) {
            FILINFO info;

            fileObject.fs = nullptr;

            if (f_readdir_obj(&directory, &info, &fileObject) != FR_OK || info.fname[0] == 0) {
                close();

                return false;
            }

            // `info` dies with this call, so keep our own copy of the name
            strlcpy(entry.d_name, info.fname, sizeof(entry.d_name));

            name = entry.d_name;
            size = (uint32_t)info.fsize;
            isDirectory = (info.fattrib & AM_DIR) != 0;

            return true;
        }
        #endif

        hasMoreEntries = (dir_findnext(directoryPath, &entry) == 0);

        if (hasMoreEntries) {
            readCurrentEntry();
        }

        return hasMoreEntries;
    }

private:
    void readCurrentEntry() {
        name = entry.d_name;
        size = (uint32_t)entry.d_size;
        isDirectory = (entry.d_type == DT_DIR);
    }
};
