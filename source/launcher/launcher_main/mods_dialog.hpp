#pragma once

#include "std_include.hpp"
#include <string>
#include <vector>

#include "launcher_funcs/mods.hpp"

class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryDir;

class ModsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModsDialog(const std::string& gameDir, QWidget* parent = nullptr);

private slots:
    void refreshButtons();
    void installSelected();
    void installFromFile();
    void removeSelected();
    void toggleSelected();
    void refresh();
    void onSelectionChanged();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void installArchive(const std::string& archivePath);

private:
    void setStatus(const QString& text);
    int selectedCatalogIndex() const;

    std::string gameDir_;
    std::vector<mods::CatalogEntry> catalog_;
    std::vector<mods::InstalledMod> installed_;

    QListWidget* catalogList_ = nullptr;
    QListWidget* installedList_ = nullptr;
    QLabel* descriptionLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* warningLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* installButton_ = nullptr;
    QPushButton* zipButton_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QPushButton* toggleButton_ = nullptr;

    QNetworkAccessManager* network_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    QTemporaryDir* tempDir_ = nullptr;
    std::string pendingArchive_;
};
