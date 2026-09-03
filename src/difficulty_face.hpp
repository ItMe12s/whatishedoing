#pragma once

#include <fmt/format.h>
#include <optional>
#include <string>

// Thank you prevter for this awesome code
inline int getLevelRating(int featured, int isEpic, int stars) {
    switch (isEpic) {
        case 1: return 3; // Epic
        case 2: return 4; // Legendary
        case 3: return 5; // Mythic
        default: break;
    }
    if (stars == 0 && featured <= 0) {
        return 0; // Unrated
    }
    return featured <= 0 ? 1 : 2; // Rated / Featured
}

inline std::optional<std::string> getDifficultyFace(
    int difficulty, int demonDifficulty, int stars, int rating = 0
) {
    std::optional<std::string> base;
    auto const demonBase = [demonDifficulty]() -> std::optional<std::string> {
        switch (demonDifficulty) {
            case 0: return "demon-hard"; // Also demon with no demon difficulty set
            case 3: return "demon-easy";
            case 4: return "demon-medium";
            case 5: return "demon-insane";
            case 6: return "demon-extreme";
            default: return std::nullopt;
        }
    };
    if (stars <= 0) {
        base = "unrated";
    }
    else if (difficulty == 6) {
        base = demonBase();
    }
    else if ((difficulty == 0 || difficulty == -1) && stars > 0) {
        if (stars == 10) {
            base = demonBase();
        }
        else if (stars == 1) {
            base = "auto";
        }
        else if (stars == 2) {
            base = "easy";
        }
        else if (stars == 3) {
            base = "normal";
        }
        else if (stars <= 5) {
            base = "hard";
        }
        else if (stars <= 7) {
            base = "harder";
        }
        else if (stars <= 9) {
            base = "insane";
        }
        else {
            return std::nullopt; // Wtf Robtop
        }
    }
    else {
        switch (difficulty) {
            case -1: base = "unrated"; break;
            case 0: base = "auto"; break;
            case 1: base = "easy"; break;
            case 2: base = "normal"; break;
            case 3: base = "hard"; break;
            case 4: base = "harder"; break;
            case 5: base = "insane"; break;
            case 6: base = "demon-hard"; break;
            case 7: base = "demon-easy"; break;
            case 8: base = "demon-medium"; break;
            case 9: base = "demon-insane"; break;
            case 10: base = "demon-extreme"; break;
            default: return std::nullopt;
        }
    }
    if (!base) {
        return std::nullopt;
    }
    switch (rating) {
        case 2: return fmt::format("{}-featured", *base);
        case 3: return fmt::format("{}-epic", *base);
        case 4: return fmt::format("{}-legendary", *base);
        case 5: return fmt::format("{}-mythic", *base);
        default: return base;
    }
}

inline std::string footerWithLevelId(std::string const& elapsed, bool showLevelID, int levelID) {
    if (!showLevelID) {
        return std::string(elapsed);
    }
    if (elapsed.empty()) {
        return fmt::format("Level ID: {}", levelID);
    }
    return fmt::format("{} \u2022 Level ID: {}", elapsed, levelID);
}
