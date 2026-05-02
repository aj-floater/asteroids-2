#include "profile_store.h"

#include "app_identity.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool parse_uint32(std::string_view sv, std::uint32_t& out) {
    auto result = std::from_chars(sv.data(), sv.data() + sv.size(), out);
    return result.ec == std::errc{};
}

static bool parse_float(std::string_view sv, float& out) {
    // std::from_chars for float is C++17 but may not be available everywhere;
    // fall back to strtof which is always present.
    char buf[64];
    std::size_t n = std::min(sv.size(), sizeof(buf) - 1u);
    std::copy(sv.begin(), sv.begin() + static_cast<std::ptrdiff_t>(n), buf);
    buf[n] = '\0';
    char* end = nullptr;
    out = std::strtof(buf, &end);
    return end != buf;
}

static bool parse_bool(std::string_view sv, bool& out) {
    if (sv == "true")  { out = true;  return true; }
    if (sv == "false") { out = false; return true; }
    return false;
}

// ---------------------------------------------------------------------------
// ProfileSlot
// ---------------------------------------------------------------------------

std::string ProfileSlot::default_name(std::size_t index) {
    return "PROFILE " + std::to_string(index + 1);
}

// ---------------------------------------------------------------------------
// ProfileStore
// ---------------------------------------------------------------------------

std::string ProfileStore::save_path() const {
    return AppIdentity::xdg_data_directory() + "/profiles.txt";
}

void ProfileStore::load() {
    // Reset to defaults first.
    for (auto& s : slots_) {
        s = ProfileSlot{};
    }
    activeSlotIndex_.reset();

    std::ifstream f(save_path());
    if (!f.is_open()) {
        // Fresh install: auto-create slot 0 as active.
        slots_[0].exists = true;
        activeSlotIndex_ = 0;
        return;
    }

    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string_view key(line.data(), eq);
        std::string_view val(line.data() + eq + 1, line.size() - eq - 1);

        if (key == "activeSlot") {
            std::uint32_t idx = 0;
            if (parse_uint32(val, idx) && idx < kProfileSlotCount) {
                activeSlotIndex_ = static_cast<std::size_t>(idx);
            }
            continue;
        }

        // Expect "slot<N>.<field>"
        if (key.size() < 6 || key.substr(0, 4) != "slot") continue;
        std::size_t slotIdx = key[4] - '0';
        if (slotIdx >= kProfileSlotCount) continue;
        std::string_view field = key.substr(6); // after "slot<N>."

        ProfileSlot& s = slots_[slotIdx];
        auto& st = s.stats;

        if (field == "exists") {
            parse_bool(val, s.exists);
        } else if (field == "bestScore") {
            parse_uint32(val, st.bestScore);
        } else if (field == "bestWave") {
            parse_uint32(val, st.bestWave);
        } else if (field == "runsPlayed") {
            parse_uint32(val, st.runsPlayed);
        } else if (field == "totalScore") {
            parse_uint32(val, st.totalScore);
        } else if (field == "totalAsteroidsDestroyed") {
            parse_uint32(val, st.totalAsteroidsDestroyed);
        } else if (field == "totalPlayTimeSeconds") {
            parse_float(val, st.totalPlayTimeSeconds);
        }
    }

    // Sanity: if no active slot loaded, default to first existing slot.
    if (!activeSlotIndex_.has_value()) {
        for (std::size_t i = 0; i < kProfileSlotCount; ++i) {
            if (slots_[i].exists) {
                activeSlotIndex_ = i;
                break;
            }
        }
    }
    // If no slots exist at all (corrupt file), create slot 0.
    bool anyExists = false;
    for (const auto& s : slots_) { anyExists = anyExists || s.exists; }
    if (!anyExists) {
        slots_[0].exists = true;
        activeSlotIndex_ = 0;
    }
}

void ProfileStore::save() const {
    std::string dir = AppIdentity::xdg_data_directory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    // If directory creation fails, we try to write anyway (working-dir fallback).

    std::ofstream f(save_path(), std::ios::trunc);
    if (!f.is_open()) return;

    f << "version=1\n";
    if (activeSlotIndex_.has_value()) {
        f << "activeSlot=" << *activeSlotIndex_ << '\n';
    }

    for (std::size_t i = 0; i < kProfileSlotCount; ++i) {
        const auto& s = slots_[i];
        char prefix[8];
        std::snprintf(prefix, sizeof(prefix), "slot%zu.", i);
        f << prefix << "exists=" << (s.exists ? "true" : "false") << '\n';
        if (s.exists) {
            const auto& st = s.stats;
            f << prefix << "bestScore="                << st.bestScore                << '\n';
            f << prefix << "bestWave="                 << st.bestWave                 << '\n';
            f << prefix << "runsPlayed="               << st.runsPlayed               << '\n';
            f << prefix << "totalScore="               << st.totalScore               << '\n';
            f << prefix << "totalAsteroidsDestroyed="  << st.totalAsteroidsDestroyed  << '\n';
            f << prefix << "totalPlayTimeSeconds="     << st.totalPlayTimeSeconds      << '\n';
        }
    }
}

void ProfileStore::create_slot(std::size_t index) {
    if (index >= kProfileSlotCount) return;
    slots_[index] = ProfileSlot{};
    slots_[index].exists = true;
    save();
}

void ProfileStore::delete_slot(std::size_t index) {
    if (index >= kProfileSlotCount) return;
    slots_[index] = ProfileSlot{};
    // If deleted slot was active, move to another existing slot.
    if (activeSlotIndex_ == index) {
        activeSlotIndex_.reset();
        for (std::size_t i = 0; i < kProfileSlotCount; ++i) {
            if (slots_[i].exists) { activeSlotIndex_ = i; break; }
        }
    }
    save();
}

void ProfileStore::reset_slot_stats(std::size_t index) {
    if (index >= kProfileSlotCount || !slots_[index].exists) return;
    slots_[index].stats = ProfileStats{};
    save();
}

void ProfileStore::set_active_slot(std::size_t index) {
    if (index >= kProfileSlotCount || !slots_[index].exists) return;
    activeSlotIndex_ = index;
    save();
}

void ProfileStore::increment_runs_played(std::size_t index) {
    if (index >= kProfileSlotCount || !slots_[index].exists) return;
    ++slots_[index].stats.runsPlayed;
    save();
}

void ProfileStore::commit_run(std::size_t index, const RunSummary& summary) {
    if (index >= kProfileSlotCount || !slots_[index].exists) return;
    auto& st = slots_[index].stats;
    if (summary.finalScore > st.bestScore) st.bestScore = summary.finalScore;
    if (summary.finalWave  > st.bestWave)  st.bestWave  = summary.finalWave;
    st.totalScore               += summary.finalScore;
    st.totalAsteroidsDestroyed  += summary.asteroidsDestroyed;
    st.totalPlayTimeSeconds     += summary.playTimeSeconds;
    save();
}
