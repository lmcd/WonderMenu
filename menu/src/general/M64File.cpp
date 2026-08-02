/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "M64File.h"

#include <algorithm>
#include <filesystem>

static M64File::Header headerBuffer;

bool M64File::hasM64Extension(const char* filename) {
    size_t length = strlen(filename);

    if (length < 4) {
        return false;
    }

    if (filename[0] == '.') {
        return false;
    }

    const char* ext = filename + length - 4;
    
    return strcasecmp(ext, ".m64") == 0;
}

bool M64File::loadHeader() {
    FILE* file = fopen(path.c_str(), "rb");

    if (!file) {
        debugf("[M64File] Failed to open .m64 file\n");
        return false;
    }

    size_t bytesRead = fread(&headerBuffer, 1, sizeof(headerBuffer), file);
    
    fclose(file);

    if (bytesRead != sizeof(headerBuffer)) {
        debugf("[M64File] File too small (%zu of %zu bytes)\n", bytesRead, sizeof(headerBuffer));
        return false;
    }

    if (headerBuffer.signature[0] != 'M' ||
        headerBuffer.signature[1] != '6' ||
        headerBuffer.signature[2] != '4'
    ) {
        debugf("[M64File] Incorrect magic bytes\n");
        return false;
    }

    romCRC32 = headerBuffer.romCRC32;
    countryCode = headerBuffer.countryCode;
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
    displayName = std::filesystem::path(path).stem().string();
    std::replace(displayName.begin(), displayName.end(), ',', ':');

    memcpy(this->description, &headerBuffer.description, sizeof(headerBuffer.description));

    return true;
}

bool M64File::stageInputsTable(uint32_t stagingAddress) {
    FILE* file = fopen(path.c_str(), "rb");

    if (!file) {
        debugf("[M64File] Failed to open .m64 file\n");

        return false;
    }

    struct stat st;
    
    if (stat(path.c_str(), &st) != 0) {
        debugf("[M64File] Failed to stat .m64 file\n");

        fclose(file);
        return false;
    }

    long fileSize = st.st_size;
    long dataSize = fileSize - sizeof(headerBuffer);

    if (dataSize <= 0) {
        debugf("[M64File] File too small (%ld bytes)\n", fileSize);

        fclose(file);
        return false;
    }

    fseek(file, sizeof(headerBuffer), SEEK_SET);
    fread((void *)stagingAddress, 1, dataSize, file);
    fclose(file);

    data_cache_hit_writeback_invalidate((void *)stagingAddress, dataSize);

    return true;
}
