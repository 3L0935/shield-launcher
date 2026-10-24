#include "std_include.hpp"
#include "launcher_main_window.hpp"
#include "style.hpp"
#include "settings_dialog.hpp"
#include "mods_dialog.hpp"
#include "window_utils.hpp"
#include "utils.hpp"
#include "../launcher_funcs/dll_loading.hpp"
#include "../launcher_funcs/json_utils.hpp"
#include "../launcher_funcs/auto_update.hpp"
#include "../launcher_funcs/audio.hpp"
#ifndef _WIN32
#include "../launcher_funcs/launch_linux.hpp"
#endif

namespace fs = std::filesystem;

namespace {
    // Only the launcher folder holds these. project-bo4.json and LPC also exist
    // next to the game executable, so they cannot be used as a signal.
    bool looksLikeLauncherDir(const fs::path& dir) {
        return fs::exists(dir / "images" / "launcher.png") ||
               fs::exists(dir / "solo.zip") ||
               fs::exists(dir / "mp.zip") ||
               fs::exists(dir / "exe.zip");
    }

    fs::path resolveLauncherDir() {
#ifdef _WIN32
        // native wide path: an ANSI conversion breaks install paths with accents
        fs::path exePath(QCoreApplication::applicationFilePath().toStdWString());
#else
        // /proc/self/exe is the resolved target, so a shortcut symlinked from
        // the game folder still points at the real launcher directory
        fs::path exePath;
        std::error_code ec;
        const fs::path procPath = fs::read_symlink("/proc/self/exe", ec);
        if (!ec && !procPath.empty()) {
            exePath = procPath;
        } else {
            exePath = QCoreApplication::applicationFilePath().toStdString();
        }
#endif

        const fs::path base = exePath.parent_path();
        if (looksLikeLauncherDir(base)) {
            return base;
        }

        const fs::path nested = base / "project-bo4" / "launcher";
        if (looksLikeLauncherDir(nested)) {
            return nested;
        }

        return base;
    }

    // The Windows folder is called "Sounds", a case sensitive filesystem needs a probe
    fs::path resolveSoundsDir(const fs::path& launcherDir) {
        for (const char* name : {"sounds", "Sounds", "SOUNDS"}) {
            const fs::path candidate = launcherDir / name;
            if (fs::exists(candidate)) {
                return candidate;
            }
        }

        return launcherDir / "sounds";
    }
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent),
launcherDir(resolveLauncherDir().wstring()),
serverIpFile((fs::path(launcherDir) / L"servers.txt").wstring()),
reshadeEnabled(false),
volume(100),
soundPath((resolveSoundsDir(fs::path(launcherDir)) / L"startup_sound.mp3").wstring()),
vanillaButton(nullptr),
onlineButton(nullptr),
offlineButton(nullptr),
nameButton(nullptr),
ipButton(nullptr),
discordButton(nullptr),
wikiButton(nullptr),
modsButton(nullptr),
settingsButton(nullptr),
closeLauncherCheckbox(nullptr),
reshadeCheckbox(nullptr),
volumeLabel(nullptr),
volumeSlider(nullptr),
progressBar(nullptr)
{
    fs::create_directories(fs::path(launcherDir));
    fs::create_directories(resolveSoundsDir(fs::path(launcherDir)));

    setWindowTitle("Project-BO4");
    setFixedSize(800, 600);


    // window / taskbar icon (Win32 resource on Windows, packaged file elsewhere)
    WindowUtils::setWindowIcon(this);
    if (!windowIcon().isNull()) {
        QApplication::setWindowIcon(windowIcon());
    }

    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent;");

    //background label and set it as central widget
    QLabel* backgroundLabel = new QLabel(this);
    backgroundLabel->setFixedSize(800, 600);
    backgroundLabel->move(0, 0);

    //background image relative to exe location
    // the packaged artwork is a png or a jpg depending on the release
    fs::path imagePath = fs::path(launcherDir) / "images" / "launcher.png";
    if (!fs::exists(imagePath)) {
        imagePath = fs::path(launcherDir) / "images" / "launcher.jpg";
    }

    if (fs::exists(imagePath)) {
        QPixmap background(QString::fromStdString(imagePath.string()));
        if (!background.isNull()) {
            background = background.scaled(800, 600, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            backgroundLabel->setPixmap(background);
            backgroundLabel->setAlignment(Qt::AlignCenter);
        }
    }

    QWidget* container = new QWidget(this);
    container->setFixedSize(800, 600);
    container->setAttribute(Qt::WA_TranslucentBackground);
    container->setStyleSheet("background: transparent;");
    setCentralWidget(container);

    //mainlayout for all widgets
    QVBoxLayout* mainLayout = new QVBoxLayout(container);

    QHBoxLayout* topRowLayout = new QHBoxLayout();

    //username and IP buttons
    nameButton = new QPushButton("Change Name", this);
    nameButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    topRowLayout->addWidget(nameButton);

    ipButton = new QPushButton("Change IP Address", this);
    ipButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    topRowLayout->addWidget(ipButton);

    //spacer to push Docs button to the right
    QSpacerItem* spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    topRowLayout->addItem(spacer);

    //docs
    wikiButton = new QPushButton("Shield Docs", this);
    wikiButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    topRowLayout->addWidget(wikiButton);

    modsButton = new QPushButton("Mods", this);
    modsButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    topRowLayout->addWidget(modsButton);

    discordButton = new QPushButton("Discord", this);
    discordButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    topRowLayout->addWidget(discordButton);

    mainLayout->addLayout(topRowLayout);

    mainLayout->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // version display
    QLabel* versionLabel = new QLabel(QString("Version: v%1").arg(QString::fromStdString(updater::get_server_version())), this);
    versionLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    mainLayout->addWidget(versionLabel);

    //settings layout
    QHBoxLayout* settingsLayout = new QHBoxLayout();

    //volume slider
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(volume);
    volumeSlider->setFixedWidth(120);
    volumeSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  height: 6px;"
        "  background: rgba(40, 40, 40, 180);"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #1a3c5e;"
        "  border: 1px solid #ffffff;"
        "  width: 14px;"
        "  margin-top: -4px;"
        "  margin-bottom: -4px;"
        "  border-radius: 7px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #1a3c5e, stop: 1 #2a5c8e);"
        "  border-radius: 3px;"
        "}"
    );
    settingsLayout->addWidget(volumeSlider);

    volumeLabel = new QLabel("Volume:", this);
    volumeLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    settingsLayout->addWidget(volumeLabel);

    //spacer to push settings button to the right
    QSpacerItem* settingsSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    settingsLayout->addItem(settingsSpacer);

	//close launcher on play checkbox
    closeLauncherCheckbox = new QCheckBox("Close Launcher on Play", this);
    closeLauncherCheckbox->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    settingsLayout->addWidget(closeLauncherCheckbox);

    //settings button
    settingsButton = new QPushButton("Client Settings", this);
    settingsButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px; min-width: 80px;");
    settingsButton->setFixedWidth(120);
    settingsLayout->addWidget(settingsButton);

    mainLayout->addLayout(settingsLayout);

#ifndef _WIN32
    // Linux only: pick the Proton/Wine runtime and the wine prefix
    QHBoxLayout* runtimeLayout = new QHBoxLayout();

    runtimeLabel = new QLabel("Proton:", this);
    runtimeLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    runtimeLayout->addWidget(runtimeLabel);

    runtimeCombo = new QComboBox(this);
    runtimeCombo->setFixedWidth(190);
    runtimeCombo->setStyleSheet("background-color: rgba(40, 40, 40, 180); color: white; border: 1px solid rgba(85, 85, 85, 180); padding: 2px;");
    runtimeLayout->addWidget(runtimeCombo);

    prefixButton = new QPushButton("Prefix...", this);
    prefixButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    prefixButton->setToolTip("Wine prefix used to run the game");
    runtimeLayout->addWidget(prefixButton);

    QSpacerItem* runtimeSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    runtimeLayout->addItem(runtimeSpacer);

    mainLayout->addLayout(runtimeLayout);

    refreshRuntimeCombo();

    connect(runtimeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onRuntimeChanged);
    connect(prefixButton, &QPushButton::clicked, this, &MainWindow::choosePrefix);
#endif

    //button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    //game mode buttons
    vanillaButton = new QPushButton("Vanilla", this);
    vanillaButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    buttonLayout->addWidget(vanillaButton);

    offlineButton = new QPushButton("Offline", this);
    offlineButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    buttonLayout->addWidget(offlineButton);

    onlineButton = new QPushButton("Online", this);
    onlineButton->setStyleSheet("background-color: rgba(85, 85, 85, 180); color: white; border: none; padding: 5px;");
    buttonLayout->addWidget(onlineButton);

    mainLayout->addLayout(buttonLayout);

    //progress bar
    progressBar = new QProgressBar(this);
    progressBar->setStyleSheet("QProgressBar { border: 1px solid rgba(68, 68, 68, 180); text-align: center; background: transparent; } QProgressBar::chunk { background-color: rgba(85, 85, 85, 180); }");
    progressBar->setVisible(false);
    mainLayout->addWidget(progressBar);

    QTimer::singleShot(500, this, [this]() {
        updater::check_and_prompt_for_updates(this);
        QTimer::singleShot(1000, this, &MainWindow::playStartupSound);
        });

    //signals
    connect(vanillaButton, &QPushButton::clicked, this, [this]() { startGame(false, true); });
    connect(onlineButton, &QPushButton::clicked, this, [this]() { startGame(true); });
    connect(offlineButton, &QPushButton::clicked, this, [this]() { startGame(false); });
    connect(nameButton, &QPushButton::clicked, this, &MainWindow::setName);
    connect(ipButton, &QPushButton::clicked, this, &MainWindow::setIp);
    connect(discordButton, &QPushButton::clicked, this, &MainWindow::openDiscord);
    connect(wikiButton, &QPushButton::clicked, this, &MainWindow::openDocs);
    connect(modsButton, &QPushButton::clicked, this, &MainWindow::openMods);
    connect(volumeSlider, &QSlider::valueChanged, this, &MainWindow::updateVolume);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(closeLauncherCheckbox, &QCheckBox::stateChanged, this, [this](int state) {
        closeLauncherOnPlay = (state == Qt::Checked);
        saveCloseLauncheronPlay();
        });
    connect(reshadeCheckbox, &QCheckBox::stateChanged, this, [this](int state) {
        reshadeEnabled = (state == Qt::Checked);
        });

    setAttribute(Qt::WA_TranslucentBackground);

    // Load settings from JSON, after creating everything.
    loadVolumeSettings();
    loadCloseLauncheronPlay();
}

void MainWindow::startGame(bool isOnline, bool isVanilla) {

    vanillaButton->setEnabled(false);
    onlineButton->setEnabled(false);
    offlineButton->setEnabled(false);

    progressBar->setVisible(true);
    progressBar->setValue(0);

    // not really needed for multiple instances
    /*
    if (DllLoading::isGameRunning()) {
        showMessageBox(QMessageBox::Warning, "Warning", "Game is already running!");
        progressBar->setVisible(false);

		vanillaButton->setEnabled(true);
        onlineButton->setEnabled(true);
        offlineButton->setEnabled(true);
        return;
    }
    */

    progressBar->setValue(15);

    copyLPCFolder();

    progressBar->setValue(25);

    std::string currentDir = QCoreApplication::applicationDirPath().toStdString();

    if (!isVanilla)
    {
        auto handleExtractionError = [&](DllLoading::Result result, const QString& missingFileMsg) {
            QString errorMsg;
            switch(result) {
            case DllLoading::Result::FileNotFound:
                errorMsg = missingFileMsg;
                break;
            case DllLoading::Result::InvalidGamePath:
                errorMsg = "BlackOps4.exe not found! Make sure the launcher shortcut is in the game directory.";
                break;
            case DllLoading::Result::ZipError:
                errorMsg = "Failed to extract files!";
                break;
            default:
                errorMsg = "Unknown error occurred while extracting files.";
            }
            showMessageBox(QMessageBox::Critical, "Error", errorMsg);
            progressBar->setVisible(false);

            vanillaButton->setEnabled(true);
            onlineButton->setEnabled(true);
            offlineButton->setEnabled(true);
        };

        auto dllResult = DllLoading::extractDlls(currentDir, isOnline, reshadeEnabled);
        if(dllResult != DllLoading::Result::Success) {
            handleExtractionError(dllResult, QString("Required files not found! Make sure %1 exists in the launcher directory.")
                .arg(isOnline ? "mp.zip" : "solo.zip"));
            return;
        }

        auto exeResult = DllLoading::extractExe(currentDir);
        if(exeResult != DllLoading::Result::Success) {
            handleExtractionError(exeResult, "Required files not found! Make sure exe.zip exists in the launcher directory.");
            return;
        }
    }

    progressBar->setValue(75);

	std::string game_exe = "BlackOps4.exe";

    // bnet's
    if (isVanilla)
    {
        game_exe = "Black Ops 4 Launcher.exe";

		// delete our dll if it exist.
        fs::path xinputPath = fs::path(currentDir) / "XInput9_1_0.dll";
        if (fs::exists(xinputPath)) {
            fs::remove(xinputPath);
		}
    }

    fs::path currentPath = fs::path(currentDir);
    fs::path gameExePath;

    if (fs::exists(currentPath / game_exe)) {
        gameExePath = currentPath / game_exe;
    }
    else if (fs::exists(currentPath.parent_path().parent_path() / game_exe)) {
        gameExePath = currentPath.parent_path().parent_path() / game_exe;
    }
    else {
        showMessageBox(QMessageBox::Critical, "Error", "Could not locate BlackOps4.exe!");
        progressBar->setVisible(false);

		vanillaButton->setEnabled(true);
        onlineButton->setEnabled(true);
        offlineButton->setEnabled(true);
        return;
    }

    if (!DllLoading::launchGame(gameExePath.string(), isOnline)) {
        showMessageBox(QMessageBox::Critical, "Error", "Failed to start the game!");
        progressBar->setVisible(false);

		vanillaButton->setEnabled(true);
        onlineButton->setEnabled(true);
        offlineButton->setEnabled(true);
        return;
    }

    progressBar->setValue(100);
    progressBar->setVisible(false);

	vanillaButton->setEnabled(true);
    onlineButton->setEnabled(true);
    offlineButton->setEnabled(true);

	// close launcher after starting game
    if (closeLauncherOnPlay)
	    QTimer::singleShot(1000, this, &MainWindow::close);
}

void MainWindow::setName() {

    QDialog dialog(this);
    dialog.setWindowTitle("Set Name");
    dialog.setFixedWidth(400);

    WindowUtils::setWindowIcon(&dialog);

    dialog.setStyleSheet(Style::getDarkStyleSheet());

    QVBoxLayout layout(&dialog);

    std::string currentDir = QCoreApplication::applicationDirPath().toStdString();
    fs::path gamePath = fs::path(currentDir);
    fs::path gameExePath = gamePath / "BlackOps4.exe";

    if (!fs::exists(gameExePath)) {

        fs::path parentPath = gamePath.parent_path().parent_path();
        gameExePath = parentPath / "BlackOps4.exe";

        if (fs::exists(gameExePath)) {
            gamePath = parentPath;
        }
    }

    std::string jsonPath = (gamePath / "project-bo4.json").string();
    std::string currentName = JsonUtils::getJsonItem(jsonPath, "identity", "name");

    QLabel label("Enter your name:");
    layout.addWidget(&label);

    QLineEdit lineEdit;
    lineEdit.setText(QString::fromStdString(currentName));
    layout.addWidget(&lineEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        std::string newName = lineEdit.text().toStdString();
        if (newName.empty()) {
            QMessageBox msgBox(QMessageBox::Warning, "Warning", "Name cannot be empty!", QMessageBox::Ok, this);
            WindowUtils::setWindowIcon(&msgBox);
            msgBox.exec();
            return;
        }

        if (!JsonUtils::replaceJsonValue(jsonPath, newName, "identity", "name")) {
            QMessageBox msgBox(QMessageBox::Critical, "Error", "Failed to save name!", QMessageBox::Ok, this);
            WindowUtils::setWindowIcon(&msgBox);
            msgBox.exec();
        }
    }
}

void MainWindow::setIp() {

    QDialog dialog(this);
    dialog.setWindowTitle("Set Server IP");
    dialog.setFixedWidth(400);

    WindowUtils::setWindowIcon(&dialog);

    dialog.setStyleSheet(Style::getDarkStyleSheet());

    QVBoxLayout layout(&dialog);

    std::string currentDir = QCoreApplication::applicationDirPath().toStdString();
    fs::path gamePath = fs::path(currentDir);
    fs::path gameExePath = gamePath / "BlackOps4.exe";

    if (!fs::exists(gameExePath)) {
        fs::path parentPath = gamePath.parent_path().parent_path();
        gameExePath = parentPath / "BlackOps4.exe";

        if (fs::exists(gameExePath)) {
            gamePath = parentPath;
        }
    }

    std::string jsonPath = (gamePath / "project-bo4.json").string();
    std::string currentIp = JsonUtils::getJsonItem(jsonPath, "demonware", "ipv4");

    QLabel label("Enter server IP address:");
    layout.addWidget(&label);

    QLineEdit lineEdit;
    lineEdit.setText(QString::fromStdString(currentIp));
    layout.addWidget(&lineEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        std::string newIp = lineEdit.text().toStdString();
        if (newIp.empty()) {
            QMessageBox msgBox(QMessageBox::Warning, "Warning", "IP address cannot be empty!", QMessageBox::Ok, this);
            WindowUtils::setWindowIcon(&msgBox);
            msgBox.exec();
            return;
        }

        if (!JsonUtils::replaceJsonValue(jsonPath, newIp, "demonware", "ipv4")) {
            QMessageBox msgBox(QMessageBox::Critical, "Error", "Failed to save IP address!", QMessageBox::Ok, this);
            WindowUtils::setWindowIcon(&msgBox);
            msgBox.exec();
        }
    }
}

void MainWindow::openDiscord() {

    QDesktopServices::openUrl(QUrl("https://discord.gg/AXECAzJJGU"));
}

void MainWindow::openMods() {
    const fs::path gameDir = fs::path(launcherDir).parent_path().parent_path();

    ModsDialog dialog(gameDir.string(), this);
    dialog.exec();
}

void MainWindow::openDocs() {

    QDesktopServices::openUrl(QUrl("https://shield-bo4.gitbook.io/"));
}

void MainWindow::updateVolume(int value) {
    // Only update the internal volume variable, don't change system volume
    volume = value;
    volumeLabel->setText(QString("Volume: %1%").arg(value));

    // If the MP3 is currently playing, update its volume
    launcher_audio::setVolume(value);

    // Save the volume setting
    saveVolumeSettings();
}

void MainWindow::openSettings() {

    fs::path gameDir = fs::path(launcherDir).parent_path().parent_path();
    std::string jsonPath = (gameDir / "project-bo4.json").string();

    SettingsDialog settingsDialog(this);
    settingsDialog.setJsonPath(jsonPath);
    settingsDialog.exec();
}

void MainWindow::playStartupSound() {

    if (!fs::exists(soundPath)) {
        QMessageBox::information(this, "Debug", "Sound file not found at: " + QString::fromStdWString(soundPath));
        return;
    }

    launcher_audio::playStartupSound(this, soundPath, volume);
}

void MainWindow::showMessageBox(QMessageBox::Icon icon, const QString& title, const QString& text) {

    QMessageBox msgBox(this);
    msgBox.setIcon(icon);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void MainWindow::saveCloseLauncheronPlay() {
    try {

        std::string jsonPath = (fs::path(launcherDir) / "launcher-config.json").string();

        rapidjson::Document document;
        if(fs::exists(jsonPath)) {
            std::ifstream file(jsonPath);
            std::string jsonContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if(!jsonContent.empty()) {
                document.Parse(jsonContent.c_str());
            }
        }

        if(!document.IsObject()) {
            document.SetObject();
        }

        if(!document.HasMember("launcher")) {
            rapidjson::Value audioObject(rapidjson::kObjectType);
            document.AddMember("launcher", audioObject, document.GetAllocator());
        }

        rapidjson::Value& audioObject = document["launcher"];
        if(audioObject.HasMember("closeLauncherOnPlay")) {
            audioObject["closeLauncherOnPlay"] = closeLauncherOnPlay;
        }
        else {
            rapidjson::Value closeValue(closeLauncherOnPlay);
            audioObject.AddMember("closeLauncherOnPlay", closeValue, document.GetAllocator());
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        std::ofstream outFile(jsonPath);
        outFile << buffer.GetString();
        outFile.close();
    }
    catch(const std::exception& e) {

    }
}

void MainWindow::loadCloseLauncheronPlay() {
    try {
        std::string jsonPath = (fs::path(launcherDir) / "launcher-config.json").string();

        if(fs::exists(jsonPath)) {
            std::ifstream file(jsonPath);
            std::string jsonContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if(!jsonContent.empty()) {
                rapidjson::Document document;
                document.Parse(jsonContent.c_str());

                if(document.IsObject() && document.HasMember("launcher") &&
                    document["launcher"].IsObject() && document["launcher"].HasMember("closeLauncherOnPlay") &&
                    document["launcher"]["closeLauncherOnPlay"].IsBool()) {
                    closeLauncherOnPlay = document["launcher"]["closeLauncherOnPlay"].GetBool();

                    if(closeLauncherCheckbox) {
                        closeLauncherCheckbox->setChecked(closeLauncherOnPlay);
                    }
                }
            }
        }
    }
    catch(const std::exception& e) {
    }
}

void MainWindow::saveVolumeSettings() {
    try {

        std::string jsonPath = (fs::path(launcherDir) / "launcher-config.json").string();

        rapidjson::Document document;
        if (fs::exists(jsonPath)) {
            std::ifstream file(jsonPath);
            std::string jsonContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (!jsonContent.empty()) {
                document.Parse(jsonContent.c_str());
            }
        }

        if (!document.IsObject()) {
            document.SetObject();
        }

        if (!document.HasMember("audio")) {
            rapidjson::Value audioObject(rapidjson::kObjectType);
            document.AddMember("audio", audioObject, document.GetAllocator());
        }

        rapidjson::Value& audioObject = document["audio"];
        if (audioObject.HasMember("volume")) {
            audioObject["volume"] = volume;
        }
        else {
            rapidjson::Value volumeValue(volume);
            audioObject.AddMember("volume", volumeValue, document.GetAllocator());
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        std::ofstream outFile(jsonPath);
        outFile << buffer.GetString();
        outFile.close();
    }
    catch (const std::exception& e) {

    }
}

void MainWindow::loadVolumeSettings() {
    try {
        std::string jsonPath = (fs::path(launcherDir) / "launcher-config.json").string();

        if (fs::exists(jsonPath)) {
            std::ifstream file(jsonPath);
            std::string jsonContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (!jsonContent.empty()) {
                rapidjson::Document document;
                document.Parse(jsonContent.c_str());

                if (document.IsObject() && document.HasMember("audio") &&
                    document["audio"].IsObject() && document["audio"].HasMember("volume") &&
                    document["audio"]["volume"].IsInt()) {
                    volume = document["audio"]["volume"].GetInt();

                    if (volume < 0) volume = 0;
                    if (volume > 100) volume = 100;

                    if (volumeSlider) {
                        volumeSlider->setValue(volume);
                        volumeLabel->setText(QString("Volume: %1%").arg(volume));
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
    }
}


#ifndef _WIN32
namespace {
    std::string launcherConfigPath() {
        return (fs::path(QCoreApplication::applicationDirPath().toStdString()) / "launcher-config.json").string();
    }
}

void MainWindow::updatePrefixLabel() {
    if (!prefixButton) return;

    const auto settings = linuxlaunch::loadSettings(launcherConfigPath());

    if (settings.prefixPath.empty()) {
        prefixButton->setText("Prefix...");
        prefixButton->setToolTip("No wine prefix selected yet");
        return;
    }

    std::string name = fs::path(settings.prefixPath).filename().string();
    if (name.empty()) name = settings.prefixPath;

    if (name.size() > 16) {
        name = name.substr(0, 15) + "...";
    }

    prefixButton->setText(QString::fromStdString(name));
    prefixButton->setToolTip(QString::fromStdString(settings.prefixPath));
}

void MainWindow::refreshRuntimeCombo() {
    if (!runtimeCombo) return;

    const auto settings = linuxlaunch::loadSettings(launcherConfigPath());
    const auto runtimes = linuxlaunch::detectRuntimes();

    runtimeCombo->blockSignals(true);
    runtimeCombo->clear();

    for (const auto& runtime : runtimes) {
        runtimeCombo->addItem(QString::fromStdString(runtime.name), QString::fromStdString(runtime.path));
        runtimeCombo->setItemData(runtimeCombo->count() - 1,
                                  QString::fromStdString(runtime.kind), Qt::UserRole + 1);
    }

    int index = runtimeCombo->findData(QString::fromStdString(settings.runtimePath));
    if (index < 0 && runtimes.size() > 0) index = 0;

    if (index >= 0) {
        runtimeCombo->setCurrentIndex(index);
    }

    if (runtimes.empty()) {
        runtimeCombo->addItem("No Proton found");
        runtimeCombo->setToolTip("No Proton or Wine build was detected. Pick one with the Prefix button.");
    }

    runtimeCombo->blockSignals(false);

    updatePrefixLabel();
}

void MainWindow::onRuntimeChanged(int index) {
    if (index < 0 || !runtimeCombo) return;

    const QString path = runtimeCombo->itemData(index).toString();
    const QString kind = runtimeCombo->itemData(index, Qt::UserRole + 1).toString();
    if (path.isEmpty()) return;

    auto settings = linuxlaunch::loadSettings(launcherConfigPath());
    settings.runtimePath = path.toStdString();
    settings.backend = (kind == "wine") ? "wine" : "umu";
    linuxlaunch::saveSettings(launcherConfigPath(), settings);
}

void MainWindow::choosePrefix() {
    auto settings = linuxlaunch::loadSettings(launcherConfigPath());

    QString start = QString::fromStdString(settings.prefixPath);
    if (start.isEmpty()) {
        const char* home = std::getenv("HOME");
        start = home ? QString::fromStdString(std::string(home)) : QString();
    }

    const QString chosen = QFileDialog::getExistingDirectory(this, "Choose a Wine prefix folder", start);
    if (chosen.isEmpty()) return;

    const std::string path = chosen.toStdString();

    if (!linuxlaunch::isPrefix(path)) {
        // Nothing usable in there: either it is empty or a plain folder
        const auto answer = QMessageBox::question(this, "Create a prefix",
            QString("No wine prefix found in:\n%1\n\nCreate one there?")
                .arg(QString::fromStdString(path)),
            QMessageBox::Yes | QMessageBox::No);

        if (answer != QMessageBox::Yes) return;

        QApplication::setOverrideCursor(Qt::WaitCursor);
        std::string error;
        const bool created = linuxlaunch::createPrefix(settings, &error);
        QApplication::restoreOverrideCursor();

        if (!created) {
            showMessageBox(QMessageBox::Critical, "Error",
                           QString::fromStdString(error.empty() ? "Prefix creation failed" : error));
            return;
        }
    }

    settings.prefixPath = path;
    linuxlaunch::saveSettings(launcherConfigPath(), settings);
    updatePrefixLabel();
}
#else
// Declared for the moc table on every platform, unused on Windows.
void MainWindow::onRuntimeChanged(int /*index*/) {}
void MainWindow::choosePrefix() {}
#endif

bool MainWindow::copyLPCFolder() {

    std::string currentDir = QCoreApplication::applicationDirPath().toStdString();
    fs::path currentPath = fs::path(currentDir);
    fs::path sourceLPCPath = currentPath / "LPC";
    fs::path gameDir = currentPath.parent_path().parent_path();
    fs::path destLPCPath = gameDir / "LPC";

    if (!fs::exists(sourceLPCPath)) {
        return false;
    }

    return utils::copyDirectoryRecursive(sourceLPCPath.string(), destLPCPath.string());
}