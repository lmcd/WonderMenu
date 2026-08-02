/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <fatfs/ff.h>
#include <string>

#ifndef KiB
#define KiB(x) ((x) * 1024)
#endif

/**
 * Streams a pre-built .sprite image out to a file a chunk at a time, so a large
 * write can be spread across frames instead of stalling one.
 *
 * The source is copied verbatim: it must already start with the sprite_t header
 * that sprite_load() expects.
 */
class ScreenshotWriter {
private:
    bool hasOpened = false;
    int writeOffset = 0;

    DIR* dir = nullptr;
    FIL fil;
    bool isOpen = false;
    std::string filename;
    const uint8_t* pixels = nullptr;
    int pixelBytes = 0;
    bool didFail = false;
    int chunkSize = KiB(30);

public:
    // `pixels` is the whole file image -- sprite_t header included -- and
    // `byteCount` its full padded length. The writer copies it verbatim.
    void begin(DIR* dir, const std::string& filename, const void* pixels, int byteCount, int chunkSize) {
        close();

        this->dir = dir;
        this->filename = filename;
        this->pixels = (const uint8_t*)pixels;
        this->pixelBytes = byteCount;
        this->chunkSize = chunkSize;

        hasOpened = false;
        writeOffset = 0;
        didFail = false;
    }

    bool hasSession() const {
        return dir != nullptr && !filename.empty();
    }

    bool isFinished() const {
        return didFail || (hasOpened && writeOffset >= pixelBytes);
    }

    bool hasFailed() const {
        return didFail;
    }

    void close() {
        if (isOpen) {
            f_close(&fil);
            isOpen = false;
        }
    }

    bool advance() {
        if (isFinished()) {
            close();
            return false;
        }

        if (!hasOpened) {
            // Opening inside an already-open directory skips re-resolving every
            // path component. FF_OPEN_NO_LOOKUP skips the existence check too,
            // which is only safe because the caller numbers screenshots from one
            // past the highest already on the card -- see nextScreenshotIndex().
            FRESULT result = f_open_in_dir(
                dir,
                &fil,
                filename.c_str(),
                FA_WRITE | FA_CREATE_ALWAYS,
                FF_OPEN_NO_LOOKUP
            );

            if (result != FR_OK) {
                debugf("[ScreenshotWriter] Failed to open %s (%i)\n", filename.c_str(), (int)result);
                didFail = true;
                return false;
            }

            isOpen = true;

            // No header is synthesised here any more: the source already begins
            // with the sprite_t that the payload laid down, so the file image is
            // copied through as-is.
            hasOpened = true;

            return true;
        }

        int remaining = pixelBytes - writeOffset;
        size_t bytesToWrite = (size_t)((remaining < chunkSize) ? remaining : chunkSize);

        const uint8_t* source = pixels + writeOffset;

        UINT bytesWritten = 0;

        if (f_write(&fil, source, bytesToWrite, &bytesWritten) != FR_OK || bytesWritten != bytesToWrite) {
            debugf("[ScreenshotWriter] Failed to write %s at offset %i\n", filename.c_str(), writeOffset);
            didFail = true;
            close();
            return false;
        }

        writeOffset += (int)bytesToWrite;

        return true;
    }

    bool write() {
        while (advance()) {}

        return !didFail;
    }

    ~ScreenshotWriter() {
        close();
    }
};
