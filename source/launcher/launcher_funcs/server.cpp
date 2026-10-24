#include "std_include.hpp"
#include "server.hpp"
#include "zip_archive.hpp"
#include "download.hpp"

#include <QProcess>
#include <QProcessEnvironment>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>

#include <filesystem>
#include <fstream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#ifndef _WIN32
#include "launch_linux.hpp"
#endif

namespace fs = std::filesystem;

namespace {
    QProcess* serverProcess = nullptr;

    const std::vector<std::string> expectedEntries = {
        "server.exe",
        "demonware",
        "htdocs",
        "storage",
        "server-config.json",
    };

    std::string upstreamArchiveUrl() {
        return "https://codeload.github.com/mwhax7/wine-bo4-server/zip/refs/heads/main";
    }
}

namespace bo4server {
    std::string defaultDirectory(const std::string& gameDir) {
        return (fs::path(gameDir) / "project-bo4" / "server").string();
    }

    const std::vector<PortRequirement>& requiredPorts() {
        static const std::vector<PortRequirement> ports = {
            {8080, false, "WebUI, the game queries it"},
            {8045, false, "Admin panel"},
            {3074, false, "Game connections (TCP)"},
            {3074, true,  "Game connections (UDP)"},
            {6542, false, "Network features"},
        };

        return ports;
    }

    InstallState inspect(const std::string& root) {
        InstallState state;
        state.root = root;

        std::error_code ec;
        if (!fs::is_directory(root, ec)) {
            state.missing = expectedEntries;
            return state;
        }

        for (const auto& entry : expectedEntries) {
            if (!fs::exists(fs::path(root) / entry)) {
                state.missing.push_back(entry);
            }
        }

        state.present = fs::exists(fs::path(root) / "server.exe");
        return state;
    }

    std::vector<PortState> checkPorts() {
        std::vector<PortState> result;

        for (const auto& requirement : requiredPorts()) {
            PortState state;
            state.requirement = requirement;

            if (requirement.udp) {
                QUdpSocket socket;
                state.free = socket.bind(QHostAddress::Any, static_cast<quint16>(requirement.port),
                                         QUdpSocket::DontShareAddress);
                if (state.free) socket.close();
            } else {
                QTcpServer socket;
                state.free = socket.listen(QHostAddress::Any, static_cast<quint16>(requirement.port));
                if (state.free) socket.close();
            }

            result.push_back(state);
        }

        return result;
    }

    std::string download(const std::string& root, const std::string& workDir) {
        std::error_code ec;
        fs::create_directories(workDir, ec);
        fs::create_directories(root, ec);

        const fs::path archive = fs::path(workDir) / "wine-bo4-server.zip";

        // goes through webdl: the Windows build has no Qt TLS backend
        const std::string downloadError = webdl::download(upstreamArchiveUrl(), archive.string());
        if (!downloadError.empty()) {
            return downloadError;
        }

        if (!zip::extractAll(archive.string(), workDir)) {
            return "The downloaded archive could not be unpacked.";
        }

        // github archives wrap everything in wine-bo4-server-<branch>/
        fs::path unpacked;
        for (const auto& entry : fs::directory_iterator(workDir, ec)) {
            if (entry.is_directory()) {
                unpacked = entry.path();
                break;
            }
        }

        if (unpacked.empty()) {
            return "The archive layout was not recognised.";
        }

        for (const auto& entry : fs::directory_iterator(unpacked, ec)) {
            const fs::path destination = fs::path(root) / entry.path().filename();
            fs::remove_all(destination, ec);
            fs::rename(entry.path(), destination, ec);
            if (ec) {
                ec.clear();
                fs::copy(entry.path(), destination,
                         fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            }
        }

        fs::remove_all(workDir, ec);

        const auto state = inspect(root);
        if (!state.present) {
            return "server.exe is still missing after unpacking.";
        }

        return {};
    }

    Config loadConfig(const std::string& root) {
        Config config;

        std::ifstream file(fs::path(root) / "server-config.json");
        if (!file) return config;

        std::stringstream buffer;
        buffer << file.rdbuf();

        rapidjson::Document doc;
        doc.Parse(buffer.str().c_str());
        if (doc.HasParseError() || !doc.IsObject()) return config;

        if (const auto http = doc.FindMember("httpserver"); http != doc.MemberEnd() && http->value.IsObject()) {
            const auto& server = http->value;
            if (server.HasMember("port") && server["port"].IsInt()) config.webPort = server["port"].GetInt();
            if (server.HasMember("access_ip") && server["access_ip"].IsString()) {
                config.bindAddress = server["access_ip"].GetString();
            }
        }

        if (const auto admin = doc.FindMember("Adminpanel"); admin != doc.MemberEnd() && admin->value.IsObject()) {
            if (admin->value.HasMember("port") && admin->value["port"].IsInt()) {
                config.adminPort = admin->value["port"].GetInt();
            }
        }

        return config;
    }

    bool saveConfig(const std::string& root, const Config& config, std::string* error) {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        rapidjson::Value http(rapidjson::kObjectType);
        http.AddMember("port", config.webPort, allocator);
        http.AddMember("access_ip", rapidjson::Value(config.bindAddress.c_str(), allocator).Move(), allocator);

        rapidjson::Value admin(rapidjson::kObjectType);
        admin.AddMember("port", config.adminPort, allocator);
        admin.AddMember("admin_access_ip", rapidjson::Value(config.bindAddress.c_str(), allocator).Move(), allocator);

        doc.AddMember("httpserver", http, allocator);
        doc.AddMember("Adminpanel", admin, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream out(fs::path(root) / "server-config.json", std::ios::trunc);
        if (!out) {
            if (error) *error = "Could not write server-config.json";
            return false;
        }

        out << buffer.GetString();
        return true;
    }

    bool start(const std::string& root, std::string* error) {
        if (isRunning()) {
            if (error) *error = "The server is already running.";
            return false;
        }

        const auto state = inspect(root);
        if (!state.present) {
            if (error) *error = "server.exe not found in " + root;
            return false;
        }

        if (serverProcess) {
            serverProcess->deleteLater();
            serverProcess = nullptr;
        }

        serverProcess = new QProcess();
        serverProcess->setWorkingDirectory(QString::fromStdString(root));

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEDEBUG", "-all");

#ifdef _WIN32
        serverProcess->setProcessEnvironment(env);
        serverProcess->setProgram(QString::fromStdString((fs::path(root) / "server.exe").string()));
        serverProcess->setArguments({});
#else
        // reuse the runtime and prefix configured for the game
        const fs::path configPath = fs::path(QCoreApplication::applicationDirPath().toStdString())
                                  / "launcher-config.json";
        const auto settings = linuxlaunch::loadSettings(configPath.string());

        if (settings.prefixPath.empty() || !linuxlaunch::isPrefix(settings.prefixPath)) {
            if (error) *error = "No wine prefix configured, set it in the launcher first.";
            return false;
        }

        env.insert("WINEPREFIX", QString::fromStdString(settings.prefixPath));
        serverProcess->setProcessEnvironment(env);

        if (settings.backend == "umu") {
            env.insert("PROTONPATH", QString::fromStdString(settings.runtimePath));
            env.insert("GAMEID", "0");
            env.insert("STORE", "none");
            serverProcess->setProcessEnvironment(env);
            serverProcess->setProgram("umu-run");
        } else {
            serverProcess->setProgram("wine");
        }

        serverProcess->setArguments({QString::fromStdString((fs::path(root) / "server.exe").string())});
#endif

        serverProcess->start();
        if (!serverProcess->waitForStarted(20000)) {
            if (error) *error = "The server process did not start.";
            return false;
        }

        return true;
    }

    void stop() {
        if (!serverProcess) return;

        serverProcess->terminate();
        if (!serverProcess->waitForFinished(8000)) {
            serverProcess->kill();
            serverProcess->waitForFinished(5000);
        }

        serverProcess->deleteLater();
        serverProcess = nullptr;
    }

    bool isRunning() {
        return serverProcess && serverProcess->state() != QProcess::NotRunning;
    }

    Status status() {
        Status result;
        result.running = isRunning();

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, 8080);
        if (!socket.waitForConnected(800)) {
            result.detail = result.running ? "Process up, WebUI not answering yet"
                                           : "Server not running";
            return result;
        }

        socket.write("GET / HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n");
        socket.waitForBytesWritten(800);
        socket.waitForReadyRead(1500);

        const QByteArray answer = socket.readAll();
        socket.close();

        result.webUiReachable = answer.startsWith("HTTP/");
        if (result.webUiReachable) {
            const int statusEnd = answer.indexOf("\r\n");
            result.detail = "WebUI answers: " +
                            QString(answer.left(statusEnd > 0 ? statusEnd : 24)).toStdString();
        } else {
            result.detail = "WebUI did not answer";
        }

        return result;
    }
}
