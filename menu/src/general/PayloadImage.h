/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

/**
 * The payload's image in the DFS filesystem of the WonderMenu ROM: its
 * segments flattened at build time into one file (payload.bin), with its load
 * address and size compiled in (payload_layout.h). Only usable while the
 * menu's ROM is in cart SDRAM: loading a game overwrites it.
 */
struct PayloadImage {
    /**
     * The ROM address of the file in DFS. 0 until opened.
     */
    uint32_t romAddress = 0;

    /**
     * Finds the image at `filename` (e.g. payload.bin) in DFS, so arbitrary
     * paths don't work. False if it's missing.
     */
    bool open(const char* filename);

    bool isOpen() const {
        return romAddress != 0;
    }

    /**
     * Read the opened image out of ROM straight to its load address, ready to
     * run, failing an assert if it would be more than `capacity` bytes. Places
     * nothing if it isn't open.
     */
    void place(uint32_t capacity);

    // The payload's load address, and the size of its image, BSS included. 0
    // until placed.
    uint32_t imageBase = 0;
    uint32_t imageSize = 0;

    /**
     * The address just past the payload's image, BSS included. 0 until placed.
     */
    uint32_t imageEnd() const {
        return (imageSize != 0) ? (imageBase + imageSize) : 0;
    }
};
