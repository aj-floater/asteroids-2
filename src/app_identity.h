#pragma once

#include <cstdlib>
#include <string>

namespace AppIdentity {

inline constexpr char kDisplayName[] = "Starshard 05";
inline constexpr char kMenuTitle[] = "STARSHARD 05";
inline constexpr char kBinaryName[] = "starshard-05";
inline constexpr char kDataDirectoryName[] = "starshard-05";
inline constexpr char kAppId[] = "io.github.aj_floater.starshard_05";
inline constexpr char kDesktopFileName[] = "io.github.aj_floater.starshard_05.desktop";
inline constexpr char kHomepageUrl[] = "https://github.com/aj-floater/starshard-05";
inline constexpr char kIssueTrackerUrl[] = "https://github.com/aj-floater/starshard-05/issues";

inline std::string xdg_data_directory() {
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData && xdgData[0] != '\0') {
        return std::string(xdgData) + "/" + kDataDirectoryName;
    }

    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.local/share/" + kDataDirectoryName;
    }

    return ".";
}

}
