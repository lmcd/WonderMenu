/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <string>
#include <cstdio>

/*
[history]
0_primary_path=sd:/Super Mario 64.z64
0_secondary_path=
0_type=1
1_primary_path=sd:/Banjo-Kazooie.z64
1_secondary_path=
1_type=1
...

[favorite]
0_primary_path=sd:/Banjo-Kazooie.z64
0_secondary_path=
0_type=1
1_primary_path=
1_secondary_path=
1_type=0
...
*/
class INIParser {
public:
    struct Entry {
        std::string primaryPath;
        std::string secondaryPath;
        int type;
    };

    INIParser(const char* iniPath);
    ~INIParser();

    /**
     * Seek to the given section. Returns false if the section was not found.
     * Can be called again after reading a section to move to another one.
     */
    bool beginSection(const char* section);

    /**
     * Read the next entry from the current section.
     * Returns false when there are no more entries.
     */
    bool read(Entry& entry);

    void close();

private:
    FILE* file = nullptr;
    char sectionHeader[64];
    bool inSection = false;

    int currentIndex = -1;
    Entry pendingEntry;
    bool hasPendingEntry = false;

    void applyField(Entry& entry, const char* field, const char* value);
};
