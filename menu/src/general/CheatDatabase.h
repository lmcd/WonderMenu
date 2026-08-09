/**
 * @copyright 2026 - Lee McDermott
 * @license AGPLv3
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <istream>

struct __attribute__((packed)) CheatGroup {
    char title[50];
    uint16_t cheatStartIndex;   // span of ALL cheats in sub-tree
    uint16_t cheatCount;
    uint16_t groupStartIndex;   // first direct child group in groups[]
    uint16_t groupCount;        // number of direct child groups
};

struct __attribute__((packed)) CheatWildcardOption {
    char title[50];
    uint16_t value;
};

struct __attribute__((packed)) Cheat {
    char title[50];
    uint16_t codesStartIndex;
    uint16_t codesCount;
    uint16_t codeWithWildcardIndex;
    uint16_t wildcardStartIndex;
    uint16_t wildcardCount;
};

struct __attribute__((packed)) CheatCode {
    uint32_t address;
    uint16_t value;
};

struct CheatDatabase {
    std::vector<CheatGroup> groups;
    std::vector<Cheat>      cheats;
    std::vector<CheatCode>  codes;
    std::vector<CheatWildcardOption> wildcardOptions;

    void print() const;

    #ifndef N64
    // Serializes to a flat byte array in big-endian.
    // Layout:
    //   uint16_t groupCount
    //   uint16_t cheatCount
    //   uint16_t codeCount
    //.  uint16_t wildcardOptionsCount
    //   CheatGroup[groupCount]  { char title[50], u16 cheatStartIndex, u16 cheatCount, u16 groupStartIndex, u16 groupCount }
    //   Cheat[cheatCount]       { char title[50], u16 codesStartIndex, u16 codesCount }
    //   CheatCode[codeCount]    { u32 address, u16 value }
    //   CheatWildcardOption[wildcardOptionsCount] { char title[50], u16 value }
    std::vector<uint8_t> serialize() const;

    size_t byteOffset = 0;

    size_t sizeInBytes() const {
        // Matches the exact byte layout written by serialize() — no struct padding
        return sizeof(uint16_t) * 4
             + (groups.size() * sizeof(CheatGroup))
             + (cheats.size() * sizeof(Cheat))
             + (codes.size()  * sizeof(CheatCode))
             + (wildcardOptions.size() * sizeof(CheatWildcardOption))
        ;
    }

    bool loadChtFile(const char* path, std::string& outKey);
    #endif

private:
    void printGroupContents(size_t groupIdx, const std::string& prefix) const;
    
    #ifndef N64
    bool parseStream(std::istream& stream, std::string& outKey);
    #endif
};
