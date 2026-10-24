#pragma once
#include <string>
#include <vector>

// Linux launch backend: runs the Windows game through Proton (via umu-launcher)
// or through a plain Wine build, using a user selected prefix.
namespace linuxlaunch {
    struct Runtime {
        std::string name;   // display name, e.g. "GE-Proton11-9"
        std::string path;   // directory holding the runtime
        std::string kind;   // "proton" or "wine"
    };

    struct Settings {
        std::string runtimePath;  // selected Runtime::path
        std::string prefixPath;   // WINEPREFIX
        std::string backend;      // "umu" or "wine"
    };

    // Scans Steam, compatibilitytools.d and Heroic for usable runtimes.
    std::vector<Runtime> detectRuntimes();

    // Best guess for a prefix that already holds Battle.net, empty when none.
    std::string detectExistingPrefix();

    // Loads/saves the "linux" section of launcher-config.json
    Settings loadSettings(const std::string& configJsonPath);
    bool saveSettings(const std::string& configJsonPath, const Settings& settings);

    bool isPrefix(const std::string& dir);

    // Creates a wine prefix in an empty directory. Returns false with a reason
    // in *error when the runtime could not be started.
    bool createPrefix(const Settings& settings, std::string* error = nullptr);

    // Starts the game detached. cwd is the executable's directory.
    bool launch(const std::string& exePath, const std::string& arguments,
                const Settings& settings, std::string* error = nullptr);

    bool isGameRunning();
}
