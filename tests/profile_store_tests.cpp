#include "app_identity.h"
#include "profile_store.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::optional<std::string> get_env(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

void set_env_value(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void unset_env_value(const char* name) {
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

struct ScopedEnvVar {
    explicit ScopedEnvVar(const char* key)
        : key_(key), previousValue_(get_env(key)) {
    }

    ~ScopedEnvVar() {
        if (previousValue_.has_value()) {
            set_env_value(key_.c_str(), *previousValue_);
        } else {
            unset_env_value(key_.c_str());
        }
    }

    std::string key_;
    std::optional<std::string> previousValue_;
};

std::filesystem::path make_temp_root() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("starshard_05_profile_store_tests_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    return root;
}

std::filesystem::path profiles_path_for_root(const std::filesystem::path& root) {
    return root / AppIdentity::kDataDirectoryName / "profiles.txt";
}

void test_load_defaults_when_profiles_file_missing() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    ProfileStore store;
    store.load();

    expect(store.active_slot_index().has_value(), "missing profile file should create an active slot");
    expect(*store.active_slot_index() == 0, "missing profile file should default active slot to 0");
    expect(store.slot(0).exists, "missing profile file should create profile slot 0");
    expect(
        !std::filesystem::exists(profiles_path_for_root(tempRoot)),
        "missing profile file should not create a save until something is persisted"
    );

    std::filesystem::remove_all(tempRoot);
}

void test_save_and_reload_roundtrip() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    ProfileStore store;
    store.load();
    store.create_slot(1);
    store.set_active_slot(1);
    store.increment_runs_played(1);
    store.commit_run(1, RunSummary{
        .finalScore = 4200,
        .finalWave = 6,
        .asteroidsDestroyed = 37,
        .playTimeSeconds = 95.5f,
    });

    ProfileStore reloaded;
    reloaded.load();

    expect(reloaded.active_slot_index().has_value(), "saved profile should restore active slot");
    expect(*reloaded.active_slot_index() == 1, "saved active slot should survive reload");
    expect(reloaded.slot(1).exists, "created profile slot should survive reload");
    expect(reloaded.slot(1).stats.bestScore == 4200, "best score should survive reload");
    expect(reloaded.slot(1).stats.bestWave == 6, "best wave should survive reload");
    expect(reloaded.slot(1).stats.runsPlayed == 1, "runs played should survive reload");
    expect(reloaded.slot(1).stats.totalScore == 4200, "total score should survive reload");
    expect(
        reloaded.slot(1).stats.totalAsteroidsDestroyed == 37,
        "destroyed asteroid count should survive reload"
    );
    expect(
        std::abs(reloaded.slot(1).stats.totalPlayTimeSeconds - 95.5f) < 0.0001f,
        "play time should survive reload"
    );
    expect(
        std::filesystem::exists(profiles_path_for_root(tempRoot)),
        "saving profiles should create a profiles file"
    );

    std::filesystem::remove_all(tempRoot);
}

}

int main() {
    const std::vector<std::pair<const char*, void(*)()>> tests = {
        {"load_defaults_when_profiles_file_missing", test_load_defaults_when_profiles_file_missing},
        {"save_and_reload_roundtrip", test_save_and_reload_roundtrip},
    };

    for (const auto& [name, test] : tests) {
        try {
            test();
        } catch (const std::exception& exception) {
            std::cerr << "profile_store test '" << name << "' failed: "
                      << exception.what() << '\n';
            return 1;
        }
    }

    return 0;
}
