/**
 * @copyright 2026 - Lee McDermott
 * @license AGPLv3
 */
#include "CheatDatabase.h"
#include <cinttypes>
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

static void writeU8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
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
        uint16_t codeWithWildcardIndex;
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
    size_t currentWildcardOptionIndex = 0;
    size_t currentCodeIndex = 0;

    bool expecting1ByteWildcard = false;
    bool expecting2ByteWildcard = false;

    while (std::getline(stream, line)) {
        if (line.empty()) {
            expecting1ByteWildcard = false;
            expecting2ByteWildcard = false;
            continue;
        }
        
        if (line.rfind("Note=", 0) == 0) {
            continue;
        }

        if (expecting1ByteWildcard) {
            // Wildcard option line, e.g. "04 Big Boo's Haunt":
            // first two chars are a hex byte, then a space, then the name.
            uint8_t optionValue;
            if (sscanf(line.c_str(), "%2" SCNx8, &optionValue) == 1) {
                std::string optionName = line.substr(3);

                // PRINTF("Wildcard option: 0x%02X %s\n", optionValue, optionName.c_str());

                CheatWildcardOption option;
                setTitle(option.title, sizeof(option.title), optionName);
                
                wildcardOptions.push_back(option);
                cheatPool[currentCheatIndex].wildcardCount++;
                currentWildcardOptionIndex++;
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

            // Wildcard values (e.g. "A032DDF9 30??") have their trailing
            // "??" replaced with zeros -> "A032DDF9 3000"
            if (line.ends_with("????")) {
                continue;
            }

            if (line.ends_with("??")) {
                line[line.size() - 2] = '0';
                line[line.size() - 1] = '0';

                debugf("Found wildcard\n");
                expecting1ByteWildcard = true;
                
                cheatPool[currentCheatIndex].codeWithWildcardIndex = cheatPool[currentCheatIndex].codesCount;
                cheatPool[currentCheatIndex].wildcardStartIndex = currentWildcardOptionIndex;
            }
            
            if (sscanf(line.c_str(), "%8" SCNx32 " %4" SCNx16, &address, &value) == 2) {
                codes.push_back({address, value});
                cheatPool[currentCheatIndex].codesCount++;
            }
        }
    }

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
                c.codeWithWildcardIndex = tc.codeWithWildcardIndex;
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
        writeU16BE(out, cheat.codeWithWildcardIndex);
        writeU16BE(out, cheat.wildcardStartIndex);
        writeU16BE(out, cheat.wildcardCount);
    }

    for (const auto& code : codes) {
        writeU32BE(out, code.address);
        writeU16BE(out, code.value);
    }

    for (const auto& wildcardOption : wildcardOptions) {
        out.insert(out.end(), wildcardOption.title, wildcardOption.title + sizeof(wildcardOption.title));
        writeU8(out, wildcardOption.value);
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
