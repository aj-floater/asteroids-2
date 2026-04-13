#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

static constexpr std::size_t kProfileSlotCount = 3;

// Summary of a completed run, produced by GameState and consumed by ProfileStore.
struct RunSummary {
    std::uint32_t finalScore           = 0;
    std::uint32_t finalWave            = 0;
    std::uint32_t asteroidsDestroyed   = 0;
    float         playTimeSeconds      = 0.0f;
};

struct ProfileStats {
    std::uint32_t bestScore                 = 0;
    std::uint32_t bestWave                  = 0;
    std::uint32_t runsPlayed                = 0;
    std::uint32_t totalScore                = 0;
    std::uint32_t totalAsteroidsDestroyed   = 0;
    float         totalPlayTimeSeconds      = 0.0f;
};

struct ProfileSlot {
    bool         exists = false;
    ProfileStats stats{};

    // Fixed display name: "PROFILE 1", "PROFILE 2", "PROFILE 3"
    static std::string default_name(std::size_t index);
};

class ProfileStore {
public:
    // Load profiles from disk. Creates empty state if no file exists.
    void load();
    // Persist profiles to disk.
    void save() const;

    // Slot management
    void create_slot(std::size_t index);
    void delete_slot(std::size_t index);
    void reset_slot_stats(std::size_t index);
    void set_active_slot(std::size_t index);

    // Called when a new run begins (before any gameplay); increments runsPlayed and saves.
    void increment_runs_played(std::size_t index);
    // Called when a run ends; updates best/totals and saves.
    void commit_run(std::size_t index, const RunSummary& summary);

    // Accessors
    std::optional<std::size_t>   active_slot_index() const { return activeSlotIndex_; }
    const ProfileSlot&           slot(std::size_t index)   const { return slots_[index]; }

private:
    std::string save_path() const;

    std::array<ProfileSlot, kProfileSlotCount> slots_{};
    std::optional<std::size_t>                 activeSlotIndex_;
};
