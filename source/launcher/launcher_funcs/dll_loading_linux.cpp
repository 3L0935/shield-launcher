#include "std_include.hpp"
#include "dll_loading.hpp"

#ifndef _WIN32

#include "launch_linux.hpp"

#include <QCoreApplication>
#include <filesystem>

namespace fs = std::filesystem;

namespace DllLoading {
    bool isGameRunning() {
        return linuxlaunch::isGameRunning();
    }

    bool launchGame(const std::string& gameExePath, bool isOnline) {
        // The launcher lives in <game>/project-bo4/launcher, its config sits
        // next to the binary and the settings live in the "linux" section.
        const fs::path configPath = fs::path(QCoreApplication::applicationDirPath().toStdString())
                                  / "launcher-config.json";

        linuxlaunch::Settings settings = linuxlaunch::loadSettings(configPath.string());

        if (settings.prefixPath.empty()) {
            std::cout << "No wine prefix configured" << std::endl;
            return false;
        }

        if (!linuxlaunch::isPrefix(settings.prefixPath)) {
            std::cout << "Creating wine prefix: " << settings.prefixPath << std::endl;

            std::string error;
            if (!linuxlaunch::createPrefix(settings, &error)) {
                std::cout << "Prefix creation failed: " << error << std::endl;
                return false;
            }
        }

        const std::string arguments = isOnline ? "-multiplayer" : "-zombies";
        std::cout << "Launching " << gameExePath << " " << arguments
                  << " through " << settings.backend
                  << " (" << settings.runtimePath << ")" << std::endl;

        std::string error;
        if (!linuxlaunch::launch(gameExePath, arguments, settings, &error)) {
            std::cout << "Launch failed: " << error << std::endl;
            return false;
        }

        return true;
    }
}

#endif // _WIN32
