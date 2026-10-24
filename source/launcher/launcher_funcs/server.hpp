#pragma once
#include <string>
#include <vector>

// Local demonware server for Project BO4.
//
// The server itself is the one published by mwhax7/wine-bo4-server: a Windows
// server.exe (plus demonware/, htdocs/, storage/) that the project ships in a
// Docker container running wine. We do not ship it: it is pulled from that
// repository, then started natively on Windows or through the Proton runtime
// already used for the game on Linux. No Docker needed on a player machine.
namespace bo4server {
    struct PortRequirement {
        int port;
        bool udp;
        std::string purpose;
    };

    struct PortState {
        PortRequirement requirement;
        bool free = false;
    };

    struct InstallState {
        std::string root;
        bool present = false;      // server.exe found
        std::vector<std::string> missing;   // expected entries that are absent
    };

    struct Status {
        bool running = false;
        bool webUiReachable = false;
        std::string detail;
    };

    // <game>/project-bo4/server
    std::string defaultDirectory(const std::string& gameDir);

    const std::vector<PortRequirement>& requiredPorts();

    InstallState inspect(const std::string& root);

    // Real bind test: opens every port, reports the ones already taken.
    std::vector<PortState> checkPorts();

    // Downloads the upstream repository archive and unpacks it into root.
    // Returns an error string, empty on success.
    std::string download(const std::string& root, const std::string& workDir);

    // server-config.json: the two HTTP ports and the bind address
    struct Config {
        int webPort = 8080;
        int adminPort = 8045;
        std::string bindAddress = "0.0.0.0";
    };

    Config loadConfig(const std::string& root);
    bool saveConfig(const std::string& root, const Config& config, std::string* error = nullptr);

    // Starts server.exe. On Linux the runtime settings of the game are reused.
    bool start(const std::string& root, std::string* error = nullptr);
    void stop();
    bool isRunning();

    // Polls http://127.0.0.1:8080, the endpoint the game itself queries.
    Status status();
}
