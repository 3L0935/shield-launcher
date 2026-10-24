#include "std_include.hpp"
#include "server_dialog.hpp"
#include "style.hpp"
#include "window_utils.hpp"
#include "launcher_funcs/json_utils.hpp"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QFrame>
#include <QScrollArea>
#include <QScreen>
#include <QColor>
#include <QComboBox>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QTemporaryDir>

#include <filesystem>

#ifndef _WIN32
#include "launcher_funcs/launch_linux.hpp"
#endif

namespace fs = std::filesystem;

ServerDialog::ServerDialog(const std::string& gameDir, QWidget* parent)
    : QDialog(parent), gameDir_(gameDir) {
    setWindowTitle("Server wizard");
    setStyleSheet(Style::getDarkStyleSheet());

    WindowUtils::setWindowIcon(this);

    serverDir_ = bo4server::defaultDirectory(gameDir_);

    // The dialog scrolls only when the screen is too small for the content:
    // on a normal desktop everything is visible without touching anything.
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    // --- location
    auto* dirLayout = new QHBoxLayout();
    dirLayout->addWidget(new QLabel("Server folder:"));
    directoryEdit_ = new QLineEdit(QString::fromStdString(serverDir_), this);
    dirLayout->addWidget(directoryEdit_, 1);

    auto* browseButton = new QPushButton("Browse...", this);
    dirLayout->addWidget(browseButton);

    auto* downloadButton = new QPushButton("Download server files", this);
    dirLayout->addWidget(downloadButton);

    layout->addLayout(dirLayout);

    // --- prerequisites
    layout->addWidget(new QLabel("Prerequisites"));
    checksList_ = new QListWidget(this);
    checksList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(checksList_);

    // --- controls
    auto* controlsLayout = new QHBoxLayout();
    startButton_ = new QPushButton("Start server", this);
    stopButton_ = new QPushButton("Stop", this);
    auto* webButton = new QPushButton("Open WebUI", this);
    auto* adminButton = new QPushButton("Open admin panel", this);

    controlsLayout->addWidget(startButton_);
    controlsLayout->addWidget(stopButton_);
    controlsLayout->addWidget(webButton);
    controlsLayout->addWidget(adminButton);
    controlsLayout->addStretch();

    layout->addLayout(controlsLayout);

    statusLabel_ = new QLabel("", this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    // --- simple config editor for server-config.json
    layout->addWidget(new QLabel("Server configuration"));
    auto* configLayout = new QHBoxLayout();

    configLayout->addWidget(new QLabel("WebUI port:"));
    webPortBox_ = new QSpinBox(this);
    webPortBox_->setRange(1, 65535);
    configLayout->addWidget(webPortBox_);

    configLayout->addWidget(new QLabel("Admin port:"));
    adminPortBox_ = new QSpinBox(this);
    adminPortBox_->setRange(1, 65535);
    configLayout->addWidget(adminPortBox_);

    configLayout->addWidget(new QLabel("Bind address:"));
    bindEdit_ = new QLineEdit(this);
    bindEdit_->setMaximumWidth(120);
    configLayout->addWidget(bindEdit_);

    auto* saveConfigButton = new QPushButton("Save", this);
    configLayout->addWidget(saveConfigButton);
    configLayout->addStretch();

    layout->addLayout(configLayout);

    // --- easy setup
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    layout->addWidget(line);

    layout->addWidget(new QLabel("Address to share with friends"));

    auto* ipLayout = new QHBoxLayout();
    ipCombo_ = new QComboBox(this);
    ipCombo_->addItems({"This machine (127.0.0.1)", "LAN", "Public IP", "Custom"});
    ipLayout->addWidget(ipCombo_);

    ipEdit_ = new QLineEdit(this);
    ipLayout->addWidget(ipEdit_, 1);

    auto* lanButton = new QPushButton("Detect LAN", this);
    auto* publicButton = new QPushButton("Detect public", this);
    ipLayout->addWidget(lanButton);
    ipLayout->addWidget(publicButton);

    layout->addLayout(ipLayout);

    auto* applyLayout = new QHBoxLayout();
    auto* applyButton = new QPushButton("Use this address in the game config", this);
    applyLayout->addWidget(applyButton);
    applyLayout->addStretch();
    layout->addLayout(applyLayout);

    inviteEdit_ = new QPlainTextEdit(this);
    inviteEdit_->setReadOnly(true);
    inviteEdit_->setMaximumHeight(90);
    layout->addWidget(inviteEdit_);

    auto* bottomLayout = new QHBoxLayout();
    auto* hint = new QLabel(
        "Ports 8080, 8045, 3074 (TCP+UDP) and 6542 must be reachable from outside. "
        "The launcher cannot verify router forwarding, especially for UDP.", this);
    hint->setWordWrap(true);
    bottomLayout->addWidget(hint, 1);

    auto* closeButton = new QPushButton("Close", this);
    bottomLayout->addWidget(closeButton);
    layout->addLayout(bottomLayout);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(browseButton, &QPushButton::clicked, this, &ServerDialog::browseDirectory);
    connect(downloadButton, &QPushButton::clicked, this, &ServerDialog::downloadServer);
    connect(startButton_, &QPushButton::clicked, this, &ServerDialog::startServer);
    connect(stopButton_, &QPushButton::clicked, this, &ServerDialog::stopServer);
    connect(applyButton, &QPushButton::clicked, this, &ServerDialog::applySelectedIp);
    connect(lanButton, &QPushButton::clicked, this, &ServerDialog::detectLanIp);
    connect(publicButton, &QPushButton::clicked, this, &ServerDialog::detectPublicIp);
    connect(ipCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index == 0) ipEdit_->setText("127.0.0.1");
        if (index == 1) detectLanIp();
        if (index == 2) detectPublicIp();
    });
    connect(webButton, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("http://127.0.0.1:8080"));
    });
    connect(adminButton, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("http://127.0.0.1:8045"));
    });
    connect(saveConfigButton, &QPushButton::clicked, this, &ServerDialog::saveConfig);
    connect(directoryEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        serverDir_ = text.toStdString();
    });

    network_ = new QNetworkAccessManager(this);
    connect(ipEdit_, &QLineEdit::textChanged, this, [this](const QString&) { updateInviteBlock(); });

    // read the address currently stored for the game
    const std::string jsonPath = (fs::path(gameDir_) / "project-bo4" / "launcher" / "project-bo4.json").string();
    ipEdit_->setText(QString::fromStdString(JsonUtils::getJsonItem(jsonPath, "demonware", "ipv4")));
    if (ipEdit_->text().isEmpty()) ipEdit_->setText("127.0.0.1");

    statusTimer_ = new QTimer(this);
    statusTimer_->setInterval(3000);
    connect(statusTimer_, &QTimer::timeout, this, &ServerDialog::refreshStatus);
    statusTimer_->start();

    const auto config = bo4server::loadConfig(serverDir_);
    webPortBox_->setValue(config.webPort);
    adminPortBox_->setValue(config.adminPort);
    bindEdit_->setText(QString::fromStdString(config.bindAddress));

    refreshChecks();
    refreshStatus();
    updateInviteBlock();

    // fit the content when the screen allows it, scroll otherwise
    content->adjustSize();

    const QSize wanted = content->sizeHint();
    QSize limit(1920, 1080);
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        limit = screen->availableGeometry().size();
    }

    resize(qMin(wanted.width() + 32, limit.width() * 9 / 10),
           qMin(wanted.height() + 32, limit.height() * 9 / 10));
    setMinimumSize(qMin(wanted.width() + 32, 640), 420);

    // offer to do the boring part instead of just listing what is missing
    QTimer::singleShot(0, this, &ServerDialog::promptForMissingFiles);
}

void ServerDialog::promptForMissingFiles() {
    const auto state = bo4server::inspect(serverDir_);
    if (state.present) return;

    const auto answer = QMessageBox::question(this, "Create the server",
        QString("No server files in:\n%1\n\nDownload them from github.com/mwhax7/wine-bo4-server now?\n"
                "(about 5 MB, the files stay local)")
            .arg(QString::fromStdString(serverDir_)),
        QMessageBox::Yes | QMessageBox::No);

    if (answer == QMessageBox::Yes) {
        downloadServer();
    }
}

void ServerDialog::saveConfig() {
    bo4server::Config config;
    config.webPort = webPortBox_->value();
    config.adminPort = adminPortBox_->value();
    config.bindAddress = bindEdit_->text().toStdString();

    if (config.bindAddress.empty()) config.bindAddress = "0.0.0.0";

    std::string error;
    if (!bo4server::saveConfig(serverDir_, config, &error)) {
        setStatus(QString::fromStdString(error));
        return;
    }

    refreshChecks();
    setStatus("server-config.json saved. Restart the server to apply.");
}

void ServerDialog::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

void ServerDialog::refreshChecks() {
    checksList_->clear();

    const auto state = bo4server::inspect(serverDir_);

    auto add = [this](bool ok, const QString& text) {
        auto* item = new QListWidgetItem((ok ? "[ok] " : "[ ] ") + text, checksList_);
        item->setForeground(ok ? QColor(120, 220, 140) : QColor(230, 120, 120));
    };

    add(state.present, "server.exe and its files in " + QString::fromStdString(serverDir_));

    if (!state.present && !state.missing.empty()) {
        QString missing;
        for (const auto& entry : state.missing) {
            if (!missing.isEmpty()) missing += ", ";
            missing += QString::fromStdString(entry);
        }
        add(false, "missing: " + missing);
    }

#ifndef _WIN32
    const auto settings = linuxlaunch::loadSettings(
        (fs::path(QCoreApplication::applicationDirPath().toStdString()) / "launcher-config.json").string());
    add(linuxlaunch::isPrefix(settings.prefixPath),
        "wine prefix: " + QString::fromStdString(settings.prefixPath.empty() ? "(none)" : settings.prefixPath));
#else
    add(true, "Windows runtime (native)");
#endif

    for (const auto& port : bo4server::checkPorts()) {
        add(port.free, QString("port %1/%2 free (%3)")
                           .arg(port.requirement.port)
                           .arg(port.requirement.udp ? "udp" : "tcp")
                           .arg(QString::fromStdString(port.requirement.purpose)));
    }

    const bool gamePresent = fs::exists(fs::path(gameDir_) / "BlackOps4.exe");
    add(gamePresent, "BlackOps4.exe in the game folder");

    // no inner scrolling: the list grows with its rows
    checksList_->setFixedHeight(checksList_->count() * 22 + 10);
}

void ServerDialog::browseDirectory() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Choose the server folder",
                                                          QString::fromStdString(serverDir_));
    if (dir.isEmpty()) return;

    serverDir_ = dir.toStdString();
    directoryEdit_->setText(dir);
    refreshChecks();
}

void ServerDialog::downloadServer() {
    setStatus("Downloading the server files from github.com/mwhax7/wine-bo4-server ...");
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QTemporaryDir temp;
    const std::string error = bo4server::download(serverDir_, temp.path().toStdString());

    QApplication::restoreOverrideCursor();
    refreshChecks();

    if (error.empty()) {
        const auto config = bo4server::loadConfig(serverDir_);
        webPortBox_->setValue(config.webPort);
        adminPortBox_->setValue(config.adminPort);
        bindEdit_->setText(QString::fromStdString(config.bindAddress));
    }

    setStatus(error.empty() ? "Server files installed." : QString::fromStdString(error));
}

void ServerDialog::startServer() {
    std::string error;
    if (!bo4server::start(serverDir_, &error)) {
        setStatus(QString::fromStdString(error.empty() ? "The server did not start." : error));
        return;
    }

    setStatus("Server starting, waiting for the WebUI on port 8080 ...");
}

void ServerDialog::stopServer() {
    bo4server::stop();
    refreshStatus();
}

void ServerDialog::refreshStatus() {
    const auto state = bo4server::status();

    const bool running = state.running;
    startButton_->setEnabled(!running);
    stopButton_->setEnabled(running);

    setStatus(QString::fromStdString(state.detail));
}

void ServerDialog::detectLanIp() {
    for (const auto& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol) continue;
        if (address.isLoopback()) continue;

        ipEdit_->setText(address.toString());
        ipCombo_->setCurrentIndex(1);
        updateInviteBlock();
        return;
    }

    setStatus("No LAN address found.");
}

void ServerDialog::detectPublicIp() {
    setStatus("Asking the public IP ...");
    QNetworkRequest request{QUrl("https://api.ipify.org")};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, &ServerDialog::onIpReplyFinished);
}

void ServerDialog::onIpReplyFinished() {
    auto* reply = qobject_cast<QNetworkReply*>(QObject::sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        const QString ip = QString::fromUtf8(reply->readAll()).trimmed();
        if (!ip.isEmpty()) {
            ipEdit_->setText(ip);
            ipCombo_->setCurrentIndex(2);
            updateInviteBlock();
            setStatus("Public address: " + ip);
        }
    } else {
        setStatus("Could not reach the public IP service: " + reply->errorString());
    }

    reply->deleteLater();
}

void ServerDialog::applySelectedIp() {
    const std::string ip = ipEdit_->text().toStdString();
    if (ip.empty()) return;

    const std::string jsonPath = (fs::path(gameDir_) / "project-bo4" / "launcher" / "project-bo4.json").string();

    if (JsonUtils::replaceJsonValue(jsonPath, ip, "demonware", "ipv4")) {
        setStatus("Address saved in project-bo4.json, launch the game to use it.");
    } else {
        setStatus("Could not write project-bo4.json.");
    }

    updateInviteBlock();
}

void ServerDialog::updateInviteBlock() {
    const QString ip = ipEdit_->text();

    const auto config = bo4server::loadConfig(serverDir_);

    inviteEdit_->setPlainText(
        QString("Server address: %1\n"
                "1. install the Project BO4 launcher\n"
                "2. Change IP Address -> %1\n"
                "3. start the game\n"
                "\n"
                "The server runs on the host machine and has to stay online while you play.\n"
                "Ports to open/forward on the host router: %2/tcp (WebUI), %3/tcp (admin), "
                "3074 tcp+udp, 6542/tcp")
            .arg(ip)
            .arg(config.webPort)
            .arg(config.adminPort));
}
