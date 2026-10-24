#include "std_include.hpp"
#include "launch_linux.hpp"

#ifndef _WIN32

#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {
    std::string homeDir() {
        const char* home = std::getenv("HOME");
        return home ? std::string(home) : std::string();
    }

    bool hasProtonScript(const fs::path& dir) {
        return fs::exists(dir / "proton");
    }

    // A bare wine build exposes bin/wine, a Proton build files/bin/wine
    fs::path wineBinary(const std::string& runtimePath) {
        fs::path dir(runtimePath);
        if (fs::exists(dir / "files" / "bin" / "wine")) {
            return dir / "files" / "bin" / "wine";
        }
        if (fs::exists(dir / "bin" / "wine")) {
            return dir / "bin" / "wine";
        }
        return "wine";
    }

    bool prefixContainsBattleNet(const fs::path& prefix) {
        const fs::path battleNet = prefix / "drive_c" / "Program Files (x86)" / "Battle.net";
        return fs::exists(battleNet);
    }

    void addRuntime(std::vector<linuxlaunch::Runtime>& out, const fs::path& dir) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) return;

        const bool proton = hasProtonScript(dir);
        const bool wine = fs::exists(dir / "files" / "bin" / "wine") || fs::exists(dir / "bin" / "wine");
        if (!proton && !wine) return;

        const std::string canonical = fs::weakly_canonical(dir, ec).string();
        for (const auto& existing : out) {
            if (existing.path == canonical) return;
        }

        linuxlaunch::Runtime runtime;
        runtime.name = dir.filename().string();
        runtime.path = canonical;
        runtime.kind = proton ? "proton" : "wine";
        out.push_back(runtime);
    }

    void addRuntimesFrom(std::vector<linuxlaunch::Runtime>& out, const fs::path& parent) {
        std::error_code ec;
        if (!fs::is_directory(parent, ec)) return;

        for (const auto& entry : fs::directory_iterator(parent, ec)) {
            if (entry.is_directory()) addRuntime(out, entry.path());
        }
    }
}

namespace linuxlaunch {
    std::vector<Runtime> detectRuntimes() {
        std::vector<Runtime> runtimes;
        const std::string home = homeDir();
        if (home.empty()) return runtimes;

        const fs::path h(home);

        // Steam
        addRuntimesFrom(runtimes, h / ".local/share/Steam/steamapps/common");
        addRuntimesFrom(runtimes, h / ".steam/root/steamapps/common");
        addRuntimesFrom(runtimes, h / ".local/share/Steam/compatibilitytools.d");
        addRuntimesFrom(runtimes, h / ".steam/root/compatibilitytools.d");

        // Heroic / umu
        addRuntimesFrom(runtimes, h / ".config/heroic/tools/proton");
        addRuntimesFrom(runtimes, h / ".config/heroic/tools/wine");
        addRuntimesFrom(runtimes, h / ".local/share/umu/compatibilitytools");

        std::sort(runtimes.begin(), runtimes.end(), [](const Runtime& a, const Runtime& b) {
            return a.name < b.name;
        });

        return runtimes;
    }

    std::string detectExistingPrefix() {
        const std::string home = homeDir();
        if (home.empty()) return {};

        const fs::path h(home);
        std::error_code ec;

        // Heroic layout: Prefixes/<store>/<prefix>
        for (const fs::path& root : {h / "Games/Heroic/Prefixes", h / ".config/heroic/prefixes"}) {
            if (!fs::is_directory(root, ec)) continue;

            for (const auto& store : fs::directory_iterator(root, ec)) {
                if (!store.is_directory()) continue;

                for (const auto& prefix : fs::directory_iterator(store.path(), ec)) {
                    if (!prefix.is_directory()) continue;
                    if (prefixContainsBattleNet(prefix.path())) return prefix.path().string();
                }
            }
        }

        // Steam layout: compatdata/<appid>/pfx
        const fs::path compatData = h / ".local/share/Steam/steamapps/compatdata";
        if (fs::is_directory(compatData, ec)) {
            for (const auto& app : fs::directory_iterator(compatData, ec)) {
                if (!app.is_directory()) continue;

                const fs::path pfx = app.path() / "pfx";
                if (prefixContainsBattleNet(pfx)) return pfx.string();
            }
        }

        return {};
    }

    bool isPrefix(const std::string& dir) {
        if (dir.empty()) return false;

        const fs::path p(dir);
        std::error_code ec;
        if (!fs::is_directory(p, ec)) return false;

        // Wine writes system.reg/user.reg at the prefix root. Heroic points
        // "pfx" back at the same directory, so both layouts are handled.
        return fs::exists(p / "system.reg") || fs::exists(p / "pfx" / "system.reg");
    }

    Settings loadSettings(const std::string& configJsonPath) {
        Settings settings;
        settings.backend = "umu";

        std::ifstream file(configJsonPath);
        if (!file) {
            settings.runtimePath.clear();
            settings.prefixPath = detectExistingPrefix();
            return settings;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        rapidjson::Document doc;
        doc.Parse(buffer.str().c_str());
        if (doc.HasParseError() || !doc.IsObject()) return settings;

        const auto linuxIt = doc.FindMember("linux");
        if (linuxIt != doc.MemberEnd() && linuxIt->value.IsObject()) {
            const auto& linux = linuxIt->value;
            if (linux.HasMember("runtimePath") && linux["runtimePath"].IsString()) {
                settings.runtimePath = linux["runtimePath"].GetString();
            }
            if (linux.HasMember("prefixPath") && linux["prefixPath"].IsString()) {
                settings.prefixPath = linux["prefixPath"].GetString();
            }
            if (linux.HasMember("backend") && linux["backend"].IsString()) {
                settings.backend = linux["backend"].GetString();
            }
        }

        if (settings.runtimePath.empty()) {
            const auto all = detectRuntimes();
            if (!all.empty()) settings.runtimePath = all.front().path;
        }
        if (settings.prefixPath.empty()) {
            settings.prefixPath = detectExistingPrefix();
        }

        return settings;
    }

    bool saveSettings(const std::string& configJsonPath, const Settings& settings) {
        rapidjson::Document doc;
        doc.SetObject();

        {
            std::ifstream file(configJsonPath);
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();

                const std::string content = buffer.str();
                if (!content.empty()) {
                    doc.Parse(content.c_str());
                }
            }
        }

        if (doc.HasParseError() || !doc.IsObject()) {
            doc.SetObject();
        }

        auto& allocator = doc.GetAllocator();

        rapidjson::Value linux(rapidjson::kObjectType);
        linux.AddMember("runtimePath", rapidjson::Value(settings.runtimePath.c_str(), allocator).Move(), allocator);
        linux.AddMember("prefixPath", rapidjson::Value(settings.prefixPath.c_str(), allocator).Move(), allocator);
        linux.AddMember("backend", rapidjson::Value(settings.backend.c_str(), allocator).Move(), allocator);

        if (doc.HasMember("linux")) {
            doc["linux"] = linux;
        } else {
            doc.AddMember("linux", linux, allocator);
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream out(configJsonPath, std::ios::trunc);
        if (!out) return false;

        out << buffer.GetString();
        return true;
    }

    bool createPrefix(const Settings& settings, std::string* error) {
        if (settings.prefixPath.empty()) {
            if (error) *error = "No prefix directory selected.";
            return false;
        }

        std::error_code ec;
        fs::create_directories(settings.prefixPath, ec);

        const fs::path wine = wineBinary(settings.runtimePath);

        QProcess process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", QString::fromStdString(settings.prefixPath));
        env.insert("WINEDEBUG", "-all");
        process.setProcessEnvironment(env);
        process.setProgram(QString::fromStdString(wine.string()));
        process.setArguments({"wineboot", "-u"});

        process.start();
        if (!process.waitForStarted(15000)) {
            if (error) *error = "Could not start " + wine.string();
            return false;
        }

        // wineboot spawns wineserver in the background and returns quickly
        process.waitForFinished(180000);
        process.waitForFinished(5000);

        if (!isPrefix(settings.prefixPath)) {
            if (error) *error = "The prefix was not created in " + settings.prefixPath;
            return false;
        }

        return true;
    }

    bool launch(const std::string& exePath, const std::string& arguments,
                const Settings& settings, std::string* error) {
        if (!fs::exists(exePath)) {
            if (error) *error = "Game executable not found: " + exePath;
            return false;
        }

        if (!isPrefix(settings.prefixPath)) {
            if (error) *error = "Not a wine prefix: " + settings.prefixPath;
            return false;
        }

        const fs::path gameDir = fs::path(exePath).parent_path();

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", QString::fromStdString(settings.prefixPath));
        // the support dll sits next to the exe and must win over wine's builtin
        env.insert("WINEDLLOVERRIDES", "XInput9_1_0.dll=n,b");
        env.insert("WINEDEBUG", "-all");

        QProcess process;
        process.setProcessEnvironment(env);
        process.setWorkingDirectory(QString::fromStdString(gameDir.string()));

        if (settings.backend == "umu") {
            env.insert("PROTONPATH", QString::fromStdString(settings.runtimePath));
            env.insert("GAMEID", "0");
            env.insert("STORE", "none");
            process.setProcessEnvironment(env);

            process.setProgram("umu-run");
            process.setArguments({QString::fromStdString(exePath)});
        } else {
            const fs::path wine = wineBinary(settings.runtimePath);
            process.setProgram(QString::fromStdString(wine.string()));
            process.setArguments({QString::fromStdString(exePath)});
        }

        QStringList extra;
        std::stringstream stream(arguments);
        std::string token;
        while (stream >> token) {
            extra << QString::fromStdString(token);
        }
        process.setArguments(process.arguments() + extra);

        if (!process.startDetached()) {
            if (error) *error = "Failed to start the game through " + settings.backend;
            return false;
        }

        return true;
    }

    bool isGameRunning() {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator("/proc", ec)) {
            if (!entry.is_directory()) continue;

            const std::string name = entry.path().filename().string();
            if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) continue;

            std::ifstream cmdline(entry.path() / "cmdline", std::ios::binary);
            if (!cmdline) continue;

            std::string content((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());
            if (content.find("BlackOps4.exe") != std::string::npos) {
                return true;
            }
        }

        return false;
    }
}

#endif // _WIN32
