/**
 * @copyright 2026 - Lee McDermott
 * @license AGPLv3
 */
#include "CheatDatabase.h"
#include <cinttypes>
#include <cstdlib>
#include <cstring>

#ifdef N64
#include <libdragon.h>
#define PRINTF debugf
#else
#include <fstream>
#include <sstream>
#include <queue>
#include <functional>
#include <algorithm>
#define PRINTF printf
#endif

#ifndef N64
static void setTitle(char* dest, size_t destSize, const std::string& src) {
    strncpy(dest, src.c_str(), destSize - 1);
    dest[destSize - 1] = '\0';
}


static void writeU16BE(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back((v >> 8) & 0xFF);
    out.push_back(v & 0xFF);
}

static void writeU32BE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((v >> 24) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >>  8) & 0xFF);
    out.push_back(v & 0xFF);
}

// outKey is set to the bracketed header key (e.g. "1FBAF161-2C1C54F1-C:41").
bool CheatDatabase::loadChtFile(const char* path, std::string& outKey)
{
    std::ifstream file(path);
    if (!file) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return false;
    }
    return parseStream(file, outKey);
}

bool CheatDatabase::parseStream(std::istream& stream, std::string& outKey)
{
    // ------------------------------------------------------------------
    // Pass 1: parse into a temporary tree.
    //
    // The flat group array requires each group's direct children to occupy
    // a contiguous index range, but children are discovered depth-first as
    // the file streams in. Building a tree first lets us lay the arrays out
    // correctly in pass 2 regardless of nesting depth.
    // ------------------------------------------------------------------
    struct TmpCheat {
        std::string title;
        uint16_t codesStartIndex;
        uint16_t codesCount;
        uint16_t wildcardStartIndex;
        uint16_t wildcardCount;
    };
    struct TmpItem {
        bool isGroup;
        size_t ref; // node index when isGroup, else cheat-pool index
    };
    struct TmpNode {
        std::string name;
        std::vector<TmpItem> items; // direct cheats and child groups, in file order
    };

    std::vector<TmpNode>  nodes;
    std::vector<TmpCheat> cheatPool;
    std::string           rootName;

    nodes.push_back(TmpNode{}); // root is node 0
    const size_t rootNode = 0;

    struct GroupFrame { size_t nodeIdx; std::string name; };
    std::vector<GroupFrame> stack;

    auto currentParent = [&]() -> size_t {
        return stack.empty() ? rootNode : stack.back().nodeIdx;
    };

    // Push a new child group under the current innermost group
    auto pushGroup = [&](const std::string& name) {
        size_t newNode = nodes.size();
        nodes.push_back(TmpNode{});
        nodes[newNode].name = name;
        nodes[currentParent()].items.push_back({true, newNode});
        stack.push_back({newNode, name});
    };

    std::string line;
    size_t currentCheatIndex = SIZE_MAX;
    size_t currentCodeIndex = 0;

    // Options are collected as they're read and committed when the cheat ends,
    // because a file is free to list them either side of the codes they belong
    // to and their value depends on the wildcard code's own value.
    struct PendingOption {
        uint16_t value;
        std::string title;
    };

    std::vector<PendingOption> pendingOptions;

    bool hasWildcardCode = false;
    // True for a "????" (2 byte) wildcard, false for a "??" (1 byte) one.
    bool wildcardIsTwoBytes = false;
    // The wildcard code's value with the question marks zeroed, e.g. 0xAB00 for
    // "AB??". Option values are substituted into this.
    uint16_t wildcardBaseValue = 0;

    // Commits the options gathered for the cheat being parsed. Options without
    // a wildcard code to fill have nothing to apply to, so they're discarded.
    auto commitWildcardOptions = [&]() {
        if (currentCheatIndex != SIZE_MAX && hasWildcardCode) {
            TmpCheat& cheat = cheatPool[currentCheatIndex];

            cheat.wildcardStartIndex = (uint16_t)wildcardOptions.size();

            for (const PendingOption& pendingOption : pendingOptions) {
                CheatWildcardOption option;
                setTitle(option.title, sizeof(option.title), pendingOption.title);

                // Substitute the option into the code's value: a 2 byte
                // wildcard replaces it outright, a 1 byte one only its low byte.
                option.value = wildcardIsTwoBytes
                    ? pendingOption.value
                    : (uint16_t)((wildcardBaseValue & 0xFF00) | (pendingOption.value & 0x00FF));

                wildcardOptions.push_back(option);
                cheat.wildcardCount++;
            }
        }

        pendingOptions.clear();
        hasWildcardCode = false;
        wildcardIsTwoBytes = false;
        wildcardBaseValue = 0;
    };

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.rfind("Note=", 0) == 0) {
            continue;
        }

        if (currentCheatIndex != SIZE_MAX) {
            // Wildcard option line, e.g. "04 Big Boo's Haunt" (1 byte) or
            // "0104 Big Boo's Haunt" (2 bytes): a hex value, a space, then the
            // name. Code lines always lead with an 8 digit address, so a token
            // of 4 hex digits or fewer is what tells the two apart.
            size_t valueEnd = line.find(' ');

            bool isOptionLine =
                valueEnd != std::string::npos
             && valueEnd >= 1
             && valueEnd <= 4
             && line.find_first_not_of("0123456789abcdefABCDEF") == valueEnd;

            if (isOptionLine) {
                uint16_t optionValue = (uint16_t)strtoul(line.substr(0, valueEnd).c_str(), nullptr, 16);

                pendingOptions.push_back({optionValue, line.substr(valueEnd + 1)});

                continue;
            }
        }

        // Square bracket at beginning of game ID.
        // E.g. [033F4C13-319EE7A7-C:45]
        if (line[0] == '[') {
            size_t close = line.find(']');

            if (close != std::string::npos) {
                outKey = line.substr(1, close - 1);
            }
                
            continue;
        }

        // Beginning of game name.
        // E.g. Name=007 - The World Is Not Enough (U)
        if (line.rfind("Name=", 0) == 0) {
            rootName = line.substr(5);

            debugf("Name %s\n", rootName.c_str());
            
            continue;
        }

        // Name of cheat
        // E.g. $Go Solo in Multiplayer
        if (line[0] == '$') {
            commitWildcardOptions();

            // Split title on '\' — last segment is cheat name, rest is group path
            std::vector<std::string> groupSegments;
            std::stringstream ss(line.substr(1));
            std::string seg;
            while (std::getline(ss, seg, '\\')) groupSegments.push_back(seg);

            std::string cheatTitle = groupSegments.back();
            groupSegments.pop_back();

            // Find longest common prefix with current stack
            size_t common = 0;
            while (common < groupSegments.size() && common < stack.size()
                   && stack[common].name == groupSegments[common])
                common++;

            // Pop back to common prefix
            while (stack.size() > common) stack.pop_back();

            // Push new groups for remaining path segments
            for (size_t i = common; i < groupSegments.size(); i++)
                pushGroup(groupSegments[i]);

            // Add cheat to the innermost group
            currentCheatIndex = cheatPool.size();
            cheatPool.push_back({cheatTitle, (uint16_t)codes.size(), 0});
            nodes[currentParent()].items.push_back({false, currentCheatIndex});
        }
        // Cheat code (address + value)
        // E.g. 50000F01 0000
        else if (currentCheatIndex != SIZE_MAX) {
            uint32_t address;
            uint16_t value;

            // "88" codes only apply while the GameShark button is held, which
            // this project has no way to offer. Drop the whole cheat: what's
            // left of it without its activator wouldn't be the same cheat.
            if (line.rfind("88", 0) == 0) {
                codes.resize(cheatPool[currentCheatIndex].codesStartIndex);

                // The cheat is always the most recent thing pushed, both to the
                // pool and to the group holding it
                nodes[currentParent()].items.pop_back();
                cheatPool.pop_back();

                currentCheatIndex = SIZE_MAX;

                pendingOptions.clear();
                hasWildcardCode = false;

                continue;
            }

            // Wildcard values (e.g. "A032DDF9 30??" or "A032DDF9 ????") have
            // their question marks replaced with zeros -> "A032DDF9 3000".
            // The option lines that follow supply the real value.
            bool isOneByteWildcard = line.ends_with("??");
            bool isTwoByteWildcard = line.ends_with("????");

            bool isWildcardCode = isTwoByteWildcard || isOneByteWildcard;

            if (isWildcardCode) {
                size_t markCount = isTwoByteWildcard ? 4 : 2;

                // Replace question makrs with zeroes.
                line.replace(line.size() - markCount, markCount, markCount, '0');

                // A cheat can have any number of wildcard codes, but they all
                // share the one list of options
                hasWildcardCode = true;
                wildcardIsTwoBytes = isTwoByteWildcard;
            }

            // Read `address` and `code` from the line
            if (sscanf(line.c_str(), "%8" SCNx32 " %4" SCNx16, &address, &value) == 2) {
                if (isWildcardCode) {
                    wildcardBaseValue = value;
                }

                codes.push_back({address, value, (uint8_t)(isWildcardCode ? 1 : 0)});
                cheatPool[currentCheatIndex].codesCount++;
            }
        }
    }

    commitWildcardOptions();

    // ------------------------------------------------------------------
    // Pass 1b: drop groups left without any cheats. A file whose "GS Button"
    // group holds nothing but "88" codes would otherwise show as an empty
    // folder.
    // ------------------------------------------------------------------
    std::function<bool(size_t)> pruneEmptyGroups = [&](size_t n) {
        std::vector<TmpItem> kept;

        for (const TmpItem& item : nodes[n].items) {
            if (item.isGroup && pruneEmptyGroups(item.ref)) {
                continue;
            }

            kept.push_back(item);
        }

        nodes[n].items = kept;

        return nodes[n].items.empty();
    };
    pruneEmptyGroups(rootNode);

    // ------------------------------------------------------------------
    // Pass 2a: assign group indices breadth-first so every node's direct
    // children land in a contiguous range (the layout the traversal needs).
    // ------------------------------------------------------------------
    groups.assign(nodes.size(), CheatGroup{});
    std::vector<size_t> nodeToGroup(nodes.size());

    size_t nextGroupIdx = 0;
    nodeToGroup[rootNode] = nextGroupIdx++;
    setTitle(groups[0].title, sizeof(groups[0].title), rootName);

    std::queue<size_t> bfs;
    bfs.push(rootNode);
    while (!bfs.empty()) {
        size_t n = bfs.front(); bfs.pop();
        size_t g = nodeToGroup[n];

        for (const TmpItem& it : nodes[n].items) {
            if (!it.isGroup) continue;
            size_t childGroup = nextGroupIdx++;
            nodeToGroup[it.ref] = childGroup;
            setTitle(groups[childGroup].title, sizeof(groups[childGroup].title), nodes[it.ref].name);
            if (groups[g].groupCount == 0)
                groups[g].groupStartIndex = (uint16_t)childGroup;
            groups[g].groupCount++;
            bfs.push(it.ref);
        }
    }

    // Pruned groups leave unreachable nodes behind, so trim the array back to
    // what the traversal actually reaches
    groups.resize(nextGroupIdx);

    // ------------------------------------------------------------------
    // Pass 2b: emit cheats depth-first in file order, recording each
    // group's full sub-tree cheat span.
    // ------------------------------------------------------------------
    std::function<void(size_t)> emitCheats = [&](size_t n) {
        size_t g = nodeToGroup[n];
        groups[g].cheatStartIndex = (uint16_t)cheats.size();

        for (const TmpItem& it : nodes[n].items) {
            if (it.isGroup) {
                emitCheats(it.ref);
            } else {
                const TmpCheat& tc = cheatPool[it.ref];
                Cheat c{};
                setTitle(c.title, sizeof(c.title), tc.title);
                c.codesStartIndex = tc.codesStartIndex;
                c.codesCount = tc.codesCount;
                c.wildcardStartIndex = tc.wildcardStartIndex;
                c.wildcardCount = tc.wildcardCount;
                cheats.push_back(c);
            }
        }

        groups[g].cheatCount = (uint16_t)cheats.size() - groups[g].cheatStartIndex;
    };
    emitCheats(rootNode);

    return true;
}

std::vector<uint8_t> CheatDatabase::serialize() const {
    std::vector<uint8_t> out;
    writeU16BE(out, (uint16_t)groups.size());
    writeU16BE(out, (uint16_t)cheats.size());
    writeU16BE(out, (uint16_t)codes.size());
    writeU16BE(out, (uint16_t)wildcardOptions.size());

    for (const auto& group : groups) {
        out.insert(out.end(), group.title, group.title + sizeof(group.title));
        writeU16BE(out, group.cheatStartIndex);
        writeU16BE(out, group.cheatCount);
        writeU16BE(out, group.groupStartIndex);
        writeU16BE(out, group.groupCount);
    }

    for (const auto& cheat : cheats) {
        out.insert(out.end(), cheat.title, cheat.title + sizeof(cheat.title));
        writeU16BE(out, cheat.codesStartIndex);
        writeU16BE(out, cheat.codesCount);
        writeU16BE(out, cheat.wildcardStartIndex);
        writeU16BE(out, cheat.wildcardCount);
    }

    for (const auto& code : codes) {
        writeU32BE(out, code.address);
        writeU16BE(out, code.value);
        out.push_back(code.hasWildcard);
    }

    for (const auto& wildcardOption : wildcardOptions) {
        out.insert(out.end(), wildcardOption.title, wildcardOption.title + sizeof(wildcardOption.title));
        writeU16BE(out, wildcardOption.value);
    }

    return out;
}
#endif

void CheatDatabase::printGroupContents(size_t groupIdx, const std::string& prefix) const
{
    const CheatGroup& group = groups[groupIdx];
    uint16_t cheatEnd = group.cheatStartIndex + group.cheatCount;
    uint16_t sgEnd    = group.groupStartIndex + group.groupCount;

    struct Item {
        bool isSG;
        uint16_t idx;
    };

    std::vector<Item> items;
    uint16_t ci       = group.cheatStartIndex;
    uint16_t sgCursor = group.groupStartIndex;

    while (ci < cheatEnd) {
        uint16_t nextSGStart = (sgCursor < sgEnd) ? groups[sgCursor].cheatStartIndex : cheatEnd;
        if (ci == nextSGStart) {
            items.push_back({true, sgCursor});
            ci = groups[sgCursor].cheatStartIndex + groups[sgCursor].cheatCount;
            sgCursor++;
        } else {
            items.push_back({false, ci});
            ci++;
        }
    }

    for (size_t i = 0; i < items.size(); i++) {
        bool isLastItem = (i + 1 == items.size());

        const char* branchCharacter = isLastItem ? "└" : "├";
        std::string childPrefix = prefix + (branchCharacter ? "    " : "│   ");

        if (items[i].isSG) {
            uint16_t sg = items[i].idx;
            PRINTF("%s%s── 📁 [%u] %s\n", prefix.c_str(), branchCharacter, sg, groups[sg].title);
            printGroupContents(sg, childPrefix);
        } else {
            uint16_t ci2 = items[i].idx;
            PRINTF("%s%s── %s (%u codes)\n", prefix.c_str(), branchCharacter,
                cheats[ci2].title, cheats[ci2].codesCount);
        }
    }
}

void CheatDatabase::print() const
{
    PRINTF("📁 [0] %s\n", groups[0].title);
    printGroupContents(0, "");
}
