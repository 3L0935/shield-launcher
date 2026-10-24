#pragma once

#include "std_include.hpp"
#include <string>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:

    void startGame(bool isOnline, bool isVanilla = false);
    void setName();
    void setIp();
    void openDiscord();
    void openDocs();
    void updateVolume(int value);
    void openSettings();
#ifndef _WIN32
    void onRuntimeChanged(int index);
    void choosePrefix();
#endif

private:

    void showMessageBox(QMessageBox::Icon icon, const QString& title, const QString& text);
    bool copyLPCFolder();
    void playStartupSound();
    void saveCloseLauncheronPlay();
    void loadCloseLauncheronPlay();
    void saveVolumeSettings();
    void loadVolumeSettings();
#ifndef _WIN32
    void refreshRuntimeCombo();
    void updatePrefixLabel();
#endif

    std::wstring launcherDir;
    std::wstring serverIpFile;
    bool reshadeEnabled;
	bool closeLauncherOnPlay;
    int volume;
    std::wstring soundPath;

    //gui
    QPushButton* vanillaButton;
    QPushButton* onlineButton;
    QPushButton* offlineButton;
    QPushButton* nameButton;
    QPushButton* ipButton;
    QPushButton* discordButton;
    QPushButton* wikiButton;
    QPushButton* settingsButton;
    QCheckBox* closeLauncherCheckbox;
    QCheckBox* reshadeCheckbox;
    QLabel* volumeLabel;
    QSlider* volumeSlider;
    QProgressBar* progressBar;

#ifndef _WIN32
    QComboBox* runtimeCombo;
    QPushButton* prefixButton;
    QLabel* runtimeLabel;
#endif
};