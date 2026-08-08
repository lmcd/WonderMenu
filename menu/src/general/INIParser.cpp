/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "INIParser.h"

#include <cstring>

INIParser::INIParser(const char* iniPath) {
    sectionHeader[0] = '\0';
    file = fopen(iniPath, "r");
}

INIParser::~INIParser() {
    close();
}

bool INIParser::beginSection(const char* section) {
    if (!file) {
        return false;
    }

    rewind(file);
    inSection = false;
    hasPendingEntry = false;
    pendingEntry = {};
    currentIndex = -1;

    snprintf(sectionHeader, sizeof(sectionHeader), "[%s]", section);

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);

        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        if (strcmp(line, sectionHeader) == 0) {
            inSection = true;
            return true;
        }
    }

    return false;
}

void INIParser::close() {
    if (file) {
        fclose(file);
        file = nullptr;
    }
    inSection = false;
    hasPendingEntry = false;
}

void INIParser::applyField(Entry& entry, const char* field, const char* value) {
    if (strcmp(field, "primary_path") == 0) {
        entry.primaryPath = value;
    }
    else if (strcmp(field, "secondary_path") == 0) {
        entry.secondaryPath = value;
    }
    else if (strcmp(field, "type") == 0) {
        entry.type = atoi(value);
    }
}

bool INIParser::read(Entry& entry) {
    if (file == nullptr || !inSection) {
        return false;
    }

    char line[512];

    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);

        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        if (length == 0) {
            continue;
        }

        if (line[0] == '[') {
            // Hit the next section — stop reading
            inSection = false;
            break;
        }

        // Split at '='
        char* equals = strchr(line, '=');

        if (!equals) {
            continue;
        }

        *equals = '\0';

        const char* value = equals + 1;

        // Split key into index and field: "0_primary_path" → 0, "primary_path"
        char* underscore = strchr(line, '_');
        if (!underscore) {
            continue;
        }

        *underscore = '\0';
        
        int index = atoi(line);
        const char* field = underscore + 1;

        if (!hasPendingEntry) {
            currentIndex = index;
            hasPendingEntry = true;
            applyField(pendingEntry, field, value);
        }
        else if (index == currentIndex) {
            applyField(pendingEntry, field, value);
        }
        else {
            // Index changed — return the completed entry and start the next one
            entry = std::move(pendingEntry);
            pendingEntry = {};
            currentIndex = index;
            hasPendingEntry = true;
            applyField(pendingEntry, field, value);

            return true;
        }
    }

    // End of section or EOF — return the last accumulated entry if any
    if (hasPendingEntry) {
        entry = std::move(pendingEntry);
        hasPendingEntry = false;
        return true;
    }

    return false;
}
