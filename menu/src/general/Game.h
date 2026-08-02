/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <algorithm>

#include "GameDatabase.h"
#include "LabelLoader.h"
#include "M64File.h"
#include "ROMFile.h"
#include "flashcart/flashcart.h"

class CartLabel;

/**
 * Combined entry containing both ROMFile and database Entry
 */
class Game {
public:
    // Every Game is built around a ROM, so this is never absent. Game owns it and
    // deletes it in ~Game().
    ROMFile& romFile;

    // The entry pulled from our list of known games
    // Can be nullptr
    GameDatabase::Entry* databaseEntry = nullptr;

    bool isFavourite = false;

    CartLabel* cartLabel = nullptr;

    std::vector<M64File*> m64Files;

    // Filename fallback for title(), built on first use. Callers keep the
    // const char* title() returns (LabelReferenceView holds it across frames),
    // so the string it points into has to live as long as the Game does.
    mutable std::string fallbackTitle;

    Game(ROMFile& romFile, GameDatabase::Entry* databaseEntry);
    ~Game();

    /**
     * Get the display title for this game
     * Returns database title if available, otherwise the ROM filename
     */
    const char* title() const;

    /**
     * Get the developer name for this game
     * Returns database developer if available, otherwise "Unknown"
     */
    const char* developer() const;

    // Returns a pointer to a string literal (static storage), so the pointer is
    // stable for the program's lifetime and identical on every call for a given
    // region -- safe to hand directly to LabelReferenceView::stringReference.
    const char* regionString() const {
        switch (romFile.regionCode) {
            case RegionCode::ALL:             return "Multiple Regions";
            case RegionCode::BRAZIL:          return "Brazil";
            case RegionCode::CHINA:           return "China";
            case RegionCode::GERMANY:         return "Germany";
            case RegionCode::NORTH_AMERICA:   return "North America";
            case RegionCode::FRANCE:          return "France";
            case RegionCode::GATEWAY_64_NTSC: return "Gateway 64 (NTSC)";
            case RegionCode::NETHERLANDS:     return "Netherlands";
            case RegionCode::ITALY:           return "Italy";
            case RegionCode::JAPAN:           return "Japan";
            case RegionCode::KOREA:           return "Korea";
            case RegionCode::GATEWAY_64_PAL:  return "Gateway 64 (PAL)";
            case RegionCode::CANADA:          return "Canada";
            case RegionCode::EUROPE:          return "Europe";
            case RegionCode::SPAIN:           return "Spain";
            case RegionCode::AUSTRALIA:       return "Australia";
            case RegionCode::SCANDINAVIA:     return "Scandinavia";
            case RegionCode::EUROPE_X:
            case RegionCode::EUROPE_Y:
            case RegionCode::EUROPE_Z:        return "Europe";
            case RegionCode::UNKNOWN:
            default:                          return "Unknown";
        }
    }

    int version() {
        return romFile.version;
    }

    bool supportsSpeedruns() {
        if (databaseEntry == nullptr) {
            return false;
        }
        else {
            return databaseEntry->supportsSpeedruns;
        }
    }

    std::string versionString() {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "v1.%i", version());

        return std::string(buffer);
    }

    // Unique-per-ROM folder name: the two character game ID, the region letter,
    // then the version as a suffix once there's more than one of them (e.g.
    // "SME", "SME-2").
    std::string directoryName() {
        char buffer[16];

        if (version() > 1) {
            snprintf(buffer, sizeof(buffer), "%.2s%c%i", romFile.uniqueID, (char)romFile.regionCode, version());
        }
        else {
            snprintf(buffer, sizeof(buffer), "%.2s%c", romFile.uniqueID, (char)romFile.regionCode);
        }

        return std::string(buffer);
    }

    bool hasHomebrewGameCode() {
        if (databaseEntry != nullptr) {
            return databaseEntry->hasHomebrewGameCode();
        }
        else {
            return romFile.hasHomebrewGameCode();
        }
    }
};

struct GameGroup {
    GameGroup(Game& game) {
        addGame(game);
    }

    // Non-owning pointers to Games owned by GameLibrary::allGames. Every element
    // comes from addGame(), so none of them are ever null.
    std::vector<Game*> games;
    int preferredGameIndex = 0;

    // Add a regional variant to the group. If it's the North American version,
    // make it the preferred game (the one shown/launched by default).
    void addGame(Game& game) {
        games.push_back(&game);

        // TODO: use regionCode()
        if (game.romFile.regionCode == RegionCode::NORTH_AMERICA) {
            preferredGameIndex = (int)games.size() - 1;
        }
    }

    Game* preferredGame() {
        return games[preferredGameIndex];
    }

    const Game* preferredGame() const {
        return games[preferredGameIndex];
    }

    // Region sort priority: North America first, then Europe, then Japan, then
    // everything else. Lower rank sorts earlier. Europe's X/Y/Z sub-codes count
    // as Europe (they display as "Europe" too).
    static int regionRank(const Game* game) {
        switch (game->romFile.regionCode) {
            case RegionCode::NORTH_AMERICA: return 0;
            case RegionCode::EUROPE:
            case RegionCode::EUROPE_X:
            case RegionCode::EUROPE_Y:
            case RegionCode::EUROPE_Z:      return 1;
            case RegionCode::JAPAN:         return 2;
            default:                        return 3;
        }
    }

    // The group's variants ordered by region preference (North America, Europe,
    // Japan, then others), and within the same region by version number ascending.
    // Returns a new vector; the group's own `games` order is left untouched.
    std::vector<Game*> sortedGames() const {
        std::vector<Game*> sorted = games;

        std::sort(sorted.begin(), sorted.end(), [](const Game* a, const Game* b) {
            int ra = regionRank(a);
            int rb = regionRank(b);

            if (ra != rb) {
                return ra < rb;
            }

            return a->romFile.version < b->romFile.version;
        });

        return sorted;
    }

    // Two groups are the same if they reference the same Game objects. Copies
    // share the same Game* pointers, so this compares identity, not deep content.
    bool operator==(const GameGroup& other) const {
        return games == other.games;
    }
};
