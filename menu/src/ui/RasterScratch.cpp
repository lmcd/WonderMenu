/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <scratch.h>

#include "ui/RasterScratch.h"
#include "utils/utils.h"

void RasterScratch::allocate() {
    if (isAllocated() || !is_memory_expanded()) {
        return;
    }

    size_t bufferSize = display_get_width() * display_get_height() * display_get_bitdepth();

    uint8_t* scratchBuffer = (uint8_t*)scratch_memalign(64, bufferSize);

    if (scratchBuffer == nullptr) {
        return;
    }

    // Only the RDP uses it, so drop any dirty lines left by the memory's
    // last owner before they can be written back over what the RDP draws
    data_cache_hit_writeback_invalidate(scratchBuffer, bufferSize);

    buffer = scratchBuffer;
    size = bufferSize;
    used = 0;
    format = (display_get_bitdepth() == 4) ? FMT_RGBA32 : FMT_RGBA16;
    version++;
}

void RasterScratch::free() {
    if (!isAllocated()) {
        return;
    }

    // The RDP may still be copying from it
    rspq_wait();

    scratch_free(buffer);

    buffer = nullptr;
    size = 0;
    used = 0;
}

surface_t RasterScratch::take(Size takeSize) {
    if (!isAllocated()) {
        return {};
    }

    int stride = ALIGN(TEX_FORMAT_PIX2BYTES(format, takeSize.width), 8);
    size_t takeBytes = stride * takeSize.height;

    // The RDP can only draw into a buffer aligned to 64 bytes
    size_t offset = ALIGN(used, 64);

    if (offset + takeBytes > size) {
        return {};
    }

    used = offset + takeBytes;

    return surface_make(buffer + offset, format, takeSize.width, takeSize.height, stride);
}
