#pragma once

#include "std_include.hpp"
#include <string>

#include "launcher_funcs/server.hpp"

class QLineEdit;
class QLabel;
class QListWidget;
class QComboBox;
class QPushButton;
class QTimer;
class QNetworkAccessManager;
class QPlainTextEdit;
class QSpinBox;

// "Server wizard": host a local demonware server and hand the address to friends.
class ServerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ServerDialog(const std::string& gameDir, QWidget* parent = nullptr);

private slots:
    void browseDirectory();
    void downloadServer();
    void startServer();
    void stopServer();
    void refreshChecks();
    void refreshStatus();
    void applySelectedIp();
    void detectLanIp();
    void detectPublicIp();
    void onIpReplyFinished();
    void saveConfig();
    void promptForMissingFiles();

private:
    void setStatus(const QString& text);
    void updateInviteBlock();

    std::string gameDir_;
    std::string serverDir_;

    QLineEdit* directoryEdit_ = nullptr;
    QListWidget* checksList_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QSpinBox* webPortBox_ = nullptr;
    QSpinBox* adminPortBox_ = nullptr;
    QLineEdit* bindEdit_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QComboBox* ipCombo_ = nullptr;
    QLineEdit* ipEdit_ = nullptr;
    QPlainTextEdit* inviteEdit_ = nullptr;
    QTimer* statusTimer_ = nullptr;
    QNetworkAccessManager* network_ = nullptr;
};
