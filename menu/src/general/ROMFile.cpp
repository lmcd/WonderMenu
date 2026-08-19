/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ROMFile.h"

static unsigned char headerBuffer[0x40];

bool ROMFile::hasHomebrewGameCode() {
    // Homebrew developed with libdragon has a 'ED' unique ID
    if (uniqueID[0] == 'E' && uniqueID[1] == 'D') {
        return true;
    }

    // A valid retail unique ID is two uppercase-alphanumeric characters (A-Z,
    // 0-9). Anything else is homebrew.
    auto isUpperAlphaNum = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    };

    if (!isUpperAlphaNum(uniqueID[0]) || !isUpperAlphaNum(uniqueID[1])) {
        return true;
    }

    return false;
}

bool ROMFile::hasROMExtension(const char* filename) {
    size_t length = strlen(filename);

    if (length < 4) {
        return false;
    }

    if (filename[0] == '.') {
        return false;
    }

    const char* ext = filename + length - 4;
    
    return strcasecmp(ext, ".z64") == 0 ||
           strcasecmp(ext, ".n64") == 0 ||
           strcasecmp(ext, ".v64") == 0;
}

bool ROMFile::readROMBytes(const char* path, size_t numBytes, unsigned char* buffer) {
    #ifdef N64
    // Opening from the locator captured while the directory was enumerated
    // costs no disk access, where fopen() has to resolve the path and scan the
    // directory for the name -- once per ROM, over a library of hundreds.
    if (hasFileObject()) {
        FIL file;

        FRESULT result = f_open_obj(&file, &fileObject, FA_READ);

        if (result != FR_OK) {
            debugf("[ROMFile] Failed to open %s from locator (%i)\n", path, (int)result);
            return false;
        }

        this->size = (uint32_t)f_size(&file);

        UINT bytesRead = 0;
        result = f_read(&file, buffer, (UINT)numBytes, &bytesRead);

        f_close(&file);

        return (result == FR_OK) && (bytesRead == numBytes);
    }
    #endif

    // Open the ROM file
    FILE* file = fopen(path, "rb");

    if (!file) {
        debugf("[ROMFile] Failed to open file %s\n", path);
        return false;
    }
    
    struct stat statbuf;
    if (fstat(fileno(file), &statbuf) == 0) {
        this->size = statbuf.st_size;
    }

    // TODO: this is slow. Use DMA?
    // Read the specified number of bytes
    size_t bytesRead = fread(buffer, 1, numBytes, file);
    fclose(file);

    return bytesRead == numBytes;
}

bool ROMFile::readAndValidateROMData(const char* path, size_t numBytes, unsigned char* buffer) {
    if (!readROMBytes(path, numBytes, buffer)) {
        return false;
    }

    // N64 expects big-endian byte order
    // .v64 files are typically little-endian
    // .z64 files are typically big-endian
    // Check first 4 bytes to determine if byte order is correct
    // Correct format should be: 0x80 0x37 0x12 0x40
    bool isValid = buffer[0] == 0x80 &&
                   buffer[1] == 0x37 &&
                   buffer[2] == 0x12 &&
                   buffer[3] == 0x40;

    if (!isValid) {
        // Try byte swapping every two bytes
        for (size_t i = 0; i < numBytes; i += 2) {
            unsigned char temp = buffer[i];
            buffer[i] = buffer[i + 1];
            buffer[i + 1] = temp;
        }

        // Check header again after swap
        isValid = buffer[0] == 0x80 &&
                  buffer[1] == 0x37 &&
                  buffer[2] == 0x12 &&
                  buffer[3] == 0x40;

        if (!isValid) {
            return false;
        }
    }

    return true;
}

void ROMFile::setMetadataFromBuffer(const unsigned char* buffer) {
    memcpy(&this->crc1, &buffer[0x10], 4);
    memcpy(&this->crc2, &buffer[0x14], 4);
    
    // Extract title from offset 0x20 (20 bytes, null-terminated)
    memcpy(this->title, &buffer[0x20], 20);
    
    this->categoryCode = static_cast<CategoryCode>(buffer[0x3B]);
    
    // Extract game ID from offset 0x3B-0x3E (4 bytes)
    memcpy(this->uniqueID, &buffer[0x3C], 2);

    this->regionCode = static_cast<RegionCode>(buffer[0x3E]);

    memcpy(&this->version, &buffer[0x3F], 1);
}

#ifndef N64
std::string ROMFile::project64Key() const {
    char key[23];
    snprintf(key, sizeof(key), "%08X-%08X-C:%02X", ntohl(crc1), ntohl(crc2), (unsigned char)regionCode);
    return std::string(key);
}
#endif

std::string ROMFile::gameCode() const {
    return {
        (char)categoryCode,
        uniqueID[0],
        uniqueID[1],
        (char)regionCode
    };
}

bool ROMFile::loadHeader() {
    // Read and validate 64-byte ROM header
    if (!readAndValidateROMData(path.c_str(), 0x40, headerBuffer)) {
        return false;
    }

    // Parse metadata from header buffer
    setMetadataFromBuffer(headerBuffer);

    return true;
}

bool ROMFile::loadIPL3(unsigned char* buffer) {
    // IPL3 is at offset 0x40, size 0xFC0 (4032 bytes)
    // Read the full 0x1000 bytes so readAndValidateROMData can check the header
    static unsigned char ipl3ReadBuffer[0x1000];

    if (!readAndValidateROMData(path.c_str(), 0x1000, ipl3ReadBuffer)) {
        return false;
    }

    memcpy(buffer, &ipl3ReadBuffer[0x40], 0xFC0);
    return true;
}
