/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

/**
 * A dictionary/map for unique, 2-character game IDs
 * Accessed uaing `gameIDMap["GE"]` (GoldenEye, for example)
 * Size is 36 (A-Z, 0-9) * 36 * sizeof(T)
 * The database format encodes a full GameIDMap so games can be referenced at
 * speed without traversing the entire database.
 */
template <typename T, T Invalid = T{}>
struct GameIDMap {
    static constexpr int SIZE = 36;

    T entries[SIZE][SIZE];

    // Reset every cell to the Invalid sentinel. entries has static/zero storage
    // by default, so callers that use Invalid (e.g. -1) to mean "unset" MUST
    // reset() before use -- a zeroed cell would otherwise read as a real index.
    void reset() {
        for (auto& row : entries) {
            for (auto& cell : row) {
                cell = Invalid;
            }
        }
    }

    static int charToIndex(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= '0' && c <= '9') return c - '0' + 26;
        return -1;
    }

    // Look up by a 2-char game ID (e.g. gameIDMap["GE"]). Out-of-range/invalid
    // characters return a shared zeroed fallback entry rather than reading OOB.
    T& operator[](const char id[2]) {
        int a = charToIndex(id[0]);
        int b = charToIndex(id[1]);

        if (a < 0 || b < 0) {
            // Mutable scratch: a bad lookup must not hand back a ref that,
            // when written through, corrupts the shared const fallback.
            static T invalid = Invalid;
            invalid = Invalid;
            return invalid;
        }

        return entries[a][b];
    }

    const T& operator[](const char id[2]) const {
        int a = charToIndex(id[0]);
        int b = charToIndex(id[1]);

        if (a < 0 || b < 0) {
            static const T invalid = Invalid;
            return invalid;
        }

        return entries[a][b];
    }
};
