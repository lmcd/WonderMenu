/**
 * @copyright 2026 - Lee McDermott
 * @license AGPLv3
 */

#include "SpeedrunDatabase.h"

#ifndef N64
static void writeU32BE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

std::vector<uint8_t> SpeedrunDatabase::serialize() const {
    std::vector<uint8_t> out;

    writeU32BE(out, gameCRC1);
    writeU32BE(out, currentFrameAddress);
    writeU32BE(out, controllerWriteAddress);
    writeU32BE(out, controllerReadAddress);

    out.push_back((uint8_t)frameDelayOffset);
    out.push_back((uint8_t)groups.size());
    out.push_back((uint8_t)entries.size());

    // reserved
    out.push_back(0);

    for (const SpeedrunGroup& group : groups) {
        out.insert(out.end(), group.title, group.title + sizeof(group.title));
    }

    for (const SpeedrunEntry& entry : entries) {
        writeU32BE(out, entry.m64CRC);

        out.push_back(entry.groupIndex);

        writeU32BE(out, entry.timeInMilliseconds);

        out.push_back(entry.isWorldRecord ? 1 : 0);

        out.insert(out.end(), entry.authors, entry.authors + sizeof(entry.authors));
    }

    return out;
}
#endif
