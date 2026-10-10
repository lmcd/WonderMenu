/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "M64File.h"
#include "fatfs/ff.h"

#include <algorithm>
#include <filesystem>

static M64File::Header headerBuffer;

static uint32_t crc32Table[256];
static bool crc32TableInitialised = false;

// Standard CRC-32 (reflected, polynomial 0xEDB88320), which is what mkdatabase
// computes, so a value from here can be compared against one in the database.
static uint32_t calculateCRC32(const unsigned char* data, size_t length) {
    if (!crc32TableInitialised) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t crc = i;

            for (int j = 0; j < 8; j++) {
                crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320) : (crc >> 1);
            }

            crc32Table[i] = crc;
        }

        crc32TableInitialised = true;
    }

    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32Table[(crc ^ data[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFF;
}

bool M64File::hasM64Extension(const char* filename) {
    size_t length = strlen(filename);

    if (length < 4) {
        return false;
    }

    if (filename[0] == '.') {
        return false;
    }

    const char* ext = filename + length - 4;

    return ext[0] == '.'
        && (ext[1] | 0x20) == 'm'
        && ext[2] == '6'
        && ext[3] == '4';
}

bool M64File::loadHeader() {
    FIL file;

    FRESULT result = f_open_obj(&file, &fileObject, FA_READ);

    if (result != FR_OK) {
        debugf("[M64File] Failed to open file from locator (%i)\n", (int)result);
        return false;
    }

    UINT bytesRead = 0;
    result = f_read(&file, &headerBuffer, sizeof(headerBuffer), &bytesRead);

    payloadClusterOffset = file.clust;

    f_close(&file);

    if (bytesRead != sizeof(headerBuffer)) {
        debugf("[M64File] File too small (%i of %zu bytes)\n", bytesRead, sizeof(headerBuffer));
        return false;
    }

    if (headerBuffer.signature[0] != 'M' ||
        headerBuffer.signature[1] != '6' ||
        headerBuffer.signature[2] != '4'
    ) {
        debugf("[M64File] Incorrect magic bytes\n");
        return false;
    }

    romCRC = headerBuffer.romCRC32;
    countryCode = headerBuffer.countryCode;
    authoringProgram = headerBuffer.extAuthorship;
    memcpy(author, &headerBuffer.author, sizeof(headerBuffer.author));
    author[sizeof(author) - 1] = '\0';

    if (strcmp(this->author, "(too lazy to type name)") == 0) {
        strcpy(this->author, "Unknown");
    }

    // Normalize author comma spacing to exactly one space after each comma:
    // first strip ", " down to ",", then expand every "," back to ", ".
    std::string normalized = this->author;

    for (std::string::size_type pos = 0; (pos = normalized.find(", ", pos)) != std::string::npos; ) {
        normalized.erase(pos + 1, 1);
    }

    for (std::string::size_type pos = 0; (pos = normalized.find(',', pos)) != std::string::npos; pos += 2) {
        normalized.insert(pos + 1, " ");
    }

    // Copy back into the fixed buffer, truncating if expansion overflowed it.
    strncpy(this->author, normalized.c_str(), sizeof(this->author) - 1);
    this->author[sizeof(this->author) - 1] = '\0';

    // Display name: base name (stem, extension dropped) with commas -> colons.
    displayName = std::filesystem::path(filename).stem().string();
    std::replace(displayName.begin(), displayName.end(), ',', ':');

    crc = calculateCRC32((const unsigned char*)&headerBuffer, sizeof(headerBuffer));

    return true;
}

bool M64File::stageInputsTable(uint32_t stagingAddress) {
    FIL file;

    FRESULT result = f_open_obj(&file, &fileObject, FA_READ);

    if (result != FR_OK) {
        debugf("[M64File] Failed to open file from locator (%i)\n", (int)result);
        return false;
    }

    int dataSize = size - sizeof(headerBuffer);

    if (dataSize <= 0) {
        debugf("[M64File] File too small (%ld bytes)\n", size);

        f_close(&file);
        return false;
    }

    if (payloadClusterOffset != 0) {
        file.clust = payloadClusterOffset;
        file.fptr = sizeof(headerBuffer);
    }
    else {
        f_lseek(&file, sizeof(headerBuffer));
    }

    UINT bytesRead = 0;
    f_read(&file, (void *)stagingAddress, dataSize, &bytesRead);
    data_cache_hit_writeback_invalidate((void *)stagingAddress, dataSize);

    return true;
}
