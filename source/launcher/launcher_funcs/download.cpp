#include "std_include.hpp"
#include "download.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QCoreApplication>
#include <QSslSocket>
#include <QProcess>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
    std::string downloadWithQt(const std::string& url, const std::string& destPath) {
        QNetworkAccessManager manager;
        QNetworkRequest request{QUrl(QString::fromStdString(url))};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, "shield-launcher");

        QNetworkReply* reply = manager.get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const bool failed = reply->error() != QNetworkReply::NoError;
        const QString errorText = reply->errorString();
        const QByteArray payload = failed ? QByteArray() : reply->readAll();
        reply->deleteLater();

        if (failed) {
            return "Download failed: " + errorText.toStdString();
        }

        if (payload.isEmpty()) {
            return "Download failed: empty response.";
        }

        std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            return "Could not write " + destPath;
        }

        out.write(payload.constData(), payload.size());
        if (!out) {
            return "Could not write " + destPath;
        }

        return {};
    }

    bool runTool(const QString& program, const QStringList& arguments, QString* error) {
        QProcess process;
        process.setProgram(program);
        process.setArguments(arguments);
        process.start();
        if (!process.waitForStarted(10000)) {
            if (error) *error = program + " could not start";
            return false;
        }

        // pump the event loop instead of blocking: a 100 MB mod download must
        // not look like a frozen window
        while (process.state() != QProcess::NotRunning) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            process.waitForFinished(50);
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            if (error) {
                QString detail = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
                if (detail.isEmpty()) {
                    detail = program + " exited with code " + QString::number(process.exitCode());
                }
                *error = detail;
            }
            return false;
        }

        return true;
    }

    // curl ships with Windows 10 1803+ and every Linux, PowerShell is the last
    // resort so an old Windows install still works.
    std::string downloadWithTool(const std::string& url, const std::string& destPath) {
        const QString target = QString::fromStdString(destPath);
        const QString source = QString::fromStdString(url);

        std::error_code ec;
        if (fs::exists(destPath, ec)) {
            fs::remove(destPath, ec);
        }

        QString error;
        if (runTool("curl", {"-fsSL", "--retry", "2", "-o", target, source}, &error)) {
            return {};
        }

        const QString curlError = error;

        if (runTool("powershell",
                    {"-NoProfile", "-NonInteractive", "-Command",
                     "Invoke-WebRequest -UseBasicParsing -Uri '" + source + "' -OutFile '" + target + "'"},
                    &error)) {
            return {};
        }

        return "Download failed: " + curlError.toStdString() + " / " + error.toStdString();
    }
}

namespace webdl {
    bool qtTlsAvailable() {
        return QSslSocket::supportsSsl();
    }

    std::string download(const std::string& url, const std::string& destPath, bool forceExternal) {
        std::error_code ec;
        const fs::path parent = fs::path(destPath).parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, ec);
        }

        if (!forceExternal && qtTlsAvailable()) {
            const std::string error = downloadWithQt(url, destPath);
            if (error.empty()) {
                return {};
            }

            // Qt is there but the transfer failed: still try the system tool
            const std::string fallback = downloadWithTool(url, destPath);
            return fallback.empty() ? std::string() : fallback;
        }

        return downloadWithTool(url, destPath);
    }
}
