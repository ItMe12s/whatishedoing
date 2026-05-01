#pragma once

#include <Geode/Result.hpp>
#include <array>
#include <cstddef>
#include <string>

namespace profile {

enum class Kind { Bool, Int, String };

struct Tracked {
    char const* key;
    Kind kind;
};

constexpr std::size_t kSlotCount = 10;

std::array<Tracked, 24> const& trackedKeys();

std::array<std::string, kSlotCount> slotNames();
std::string slotNameAt(std::size_t idx);

bool slotIsFilled(std::string const& slot);
void snapshotIntoSlot(std::string const& slot);
void clearSlot(std::string const& slot);
geode::Result<> renameSlot(std::size_t idx, std::string newName);

bool applyProfileNow(std::string const& slot);

} // namespace profile
