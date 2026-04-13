#include "settings_store.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
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
        ("asteroids_settings_store_tests_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    return root;
}

void test_load_defaults_when_settings_file_missing() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.load();

    expect(
        std::abs(store.settings().sfxVolume - 1.0f) < 0.0001f,
        "missing settings file should default SFX volume to 100%"
    );
    expect(!store.settings().fullscreen, "missing settings file should default to windowed mode");

    std::filesystem::remove_all(tempRoot);
}

void test_save_and_reload_roundtrip() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.set_sfx_volume(0.4f);
    store.set_fullscreen(true);

    SettingsStore reloaded;
    reloaded.load();

    expect(
        std::abs(reloaded.settings().sfxVolume - 0.4f) < 0.0001f,
        "saved SFX volume should survive reload"
    );
    expect(reloaded.settings().fullscreen, "saved fullscreen setting should survive reload");
    expect(
        std::filesystem::exists(tempRoot / "asteroids" / "settings.txt"),
        "saving settings should create settings file"
    );

    std::filesystem::remove_all(tempRoot);
}

void test_sfx_volume_is_clamped_before_save() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.set_sfx_volume(2.0f);
    expect(std::abs(store.settings().sfxVolume - 1.0f) < 0.0001f, "volume above max should clamp to 1");

    store.set_sfx_volume(-1.0f);
    expect(std::abs(store.settings().sfxVolume - 0.0f) < 0.0001f, "volume below min should clamp to 0");

    SettingsStore reloaded;
    reloaded.load();
    expect(std::abs(reloaded.settings().sfxVolume - 0.0f) < 0.0001f, "clamped value should persist");

    std::filesystem::remove_all(tempRoot);
}

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"load_defaults_when_settings_file_missing", test_load_defaults_when_settings_file_missing},
        {"save_and_reload_roundtrip", test_save_and_reload_roundtrip},
        {"sfx_volume_is_clamped_before_save", test_sfx_volume_is_clamped_before_save},
    };

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& exception) {
            std::cerr << "[FAIL] " << name << ": " << exception.what() << '\n';
            return 1;
        }
    }

    return 0;
}
