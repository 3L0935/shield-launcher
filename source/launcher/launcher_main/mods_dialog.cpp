#include "std_include.hpp"
#include "mods_dialog.hpp"
#include "style.hpp"
#include "launcher_funcs/download.hpp"
#include "window_utils.hpp"

#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

ModsDialog::ModsDialog(const std::string& gameDir, QWidget* parent)
    : QDialog(parent), gameDir_(gameDir) {
    setWindowTitle("Mods");
    setMinimumSize(860, 540);
    setStyleSheet(Style::getDarkStyleSheet());

    WindowUtils::setWindowIcon(this);

    catalog_ = mods::catalog();
    network_ = new QNetworkAccessManager(this);
    tempDir_ = new QTemporaryDir();
    tempDir_->setAutoRemove(true);

    auto* layout = new QVBoxLayout(this);

    auto* listsLayout = new QHBoxLayout();

    // available
    auto* availableLayout = new QVBoxLayout();
    availableLayout->addWidget(new QLabel("Available mods"));

    catalogList_ = new QListWidget(this);
    for (const auto& entry : catalog_) {
        catalogList_->addItem(QString::fromStdString(entry.title));
    }
    availableLayout->addWidget(catalogList_, 1);

    descriptionLabel_ = new QLabel("", this);
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setMinimumHeight(56);
    availableLayout->addWidget(descriptionLabel_);

    installButton_ = new QPushButton("Install", this);
    availableLayout->addWidget(installButton_);

    zipButton_ = new QPushButton("Install from ZIP...", this);
    availableLayout->addWidget(zipButton_);

    listsLayout->addLayout(availableLayout, 3);

    // installed
    auto* installedLayout = new QVBoxLayout();
    installedLayout->addWidget(new QLabel("Installed mods"));

    installedList_ = new QListWidget(this);
    installedLayout->addWidget(installedList_, 1);

    warningLabel_ = new QLabel("", this);
    warningLabel_->setWordWrap(true);
    installedLayout->addWidget(warningLabel_);

    toggleButton_ = new QPushButton("Enable / Disable", this);
    installedLayout->addWidget(toggleButton_);

    removeButton_ = new QPushButton("Remove", this);
    installedLayout->addWidget(removeButton_);

    listsLayout->addLayout(installedLayout, 2);

    layout->addLayout(listsLayout, 1);

    progressBar_ = new QProgressBar(this);
    progressBar_->setVisible(false);
    layout->addWidget(progressBar_);

    statusLabel_ = new QLabel("", this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    auto* bottomLayout = new QHBoxLayout();
    auto* hint = new QLabel("Changes apply in game with the reload_mods console command, or after a restart.", this);
    hint->setWordWrap(true);
    bottomLayout->addWidget(hint, 1);

    auto* closeButton = new QPushButton("Close", this);
    bottomLayout->addWidget(closeButton);

    layout->addLayout(bottomLayout);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(installButton_, &QPushButton::clicked, this, &ModsDialog::installSelected);
    connect(zipButton_, &QPushButton::clicked, this, &ModsDialog::installFromFile);
    connect(removeButton_, &QPushButton::clicked, this, &ModsDialog::removeSelected);
    connect(toggleButton_, &QPushButton::clicked, this, &ModsDialog::toggleSelected);
    connect(catalogList_, &QListWidget::itemSelectionChanged, this, &ModsDialog::onSelectionChanged);
    connect(installedList_, &QListWidget::itemSelectionChanged, this, &ModsDialog::refreshButtons);

    refresh();
}

void ModsDialog::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

int ModsDialog::selectedCatalogIndex() const {
    const auto items = catalogList_->selectedItems();
    if (items.isEmpty()) return -1;

    return catalogList_->row(items.first());
}

void ModsDialog::onSelectionChanged() {
    const int index = selectedCatalogIndex();
    if (index < 0 || index >= static_cast<int>(catalog_.size())) {
        descriptionLabel_->clear();
        return;
    }

    const auto& entry = catalog_[index];
    descriptionLabel_->setText(QString::fromStdString(entry.description));
}

void ModsDialog::refreshButtons() {
    const bool hasInstalled = !installedList_->selectedItems().isEmpty();
    removeButton_->setEnabled(hasInstalled);
    toggleButton_->setEnabled(hasInstalled);
}

void ModsDialog::refresh() {
    installed_ = mods::installed(gameDir_);

    QSignalBlocker blocker(installedList_);
    installedList_->clear();

    for (const auto& mod : installed_) {
        const QString label = QString::fromStdString(mod.name)
                            + (mod.enabled ? "" : "  (disabled)");
        installedList_->addItem(label);
    }

    blocker.unblock();
    refreshButtons();

    // files dropped straight into mods/ are never loaded by the client
    const auto stray = mods::unmanagedFiles(gameDir_);
    if (stray.empty()) {
        warningLabel_->clear();
        warningLabel_->setVisible(false);
    } else {
        QString text = QString("%1 file(s) directly in project-bo4/mods are ignored by the game: ")
                           .arg(stray.size());
        for (size_t i = 0; i < stray.size(); ++i) {
            if (i) text += ", ";
            text += QString::fromStdString(stray[i]);
        }
        text += ". A mod must be a folder with a metadata.json.";
        warningLabel_->setText(text);
        warningLabel_->setVisible(true);
    }
}

void ModsDialog::installSelected() {
    const int index = selectedCatalogIndex();
    if (index < 0 || index >= static_cast<int>(catalog_.size())) {
        setStatus("Select a mod first.");
        return;
    }

    if (reply_) {
        setStatus("A download is already running.");
        return;
    }

    const auto& entry = catalog_[index];
    const QString url = QString::fromStdString(entry.url);

    const std::string archivePath = (fs::path(tempDir_->path().toStdString()) / "mod.zip").string();

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "shield-launcher");

    setStatus(QString("Downloading %1 ...").arg(QString::fromStdString(entry.title)));
    progressBar_->setVisible(true);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    installButton_->setEnabled(false);

    pendingArchive_ = archivePath;

    // Qt has no TLS backend in the Windows build (no OpenSSL shipped), every
    // https request dies with "TLS initialization failed". Use the system tool
    // there and keep the Qt path where it works, for the progress bar.
    if (!webdl::qtTlsAvailable()) {
        progressBar_->setRange(0, 0);
        setStatus("Downloading " + QString::fromStdString(entry.title) + " ...");

        const std::string error = webdl::download(entry.url, archivePath);

        progressBar_->setVisible(false);
        installButton_->setEnabled(true);

        if (!error.empty()) {
            setStatus(QString::fromStdString(error));
            return;
        }

        installArchive(archivePath);
        return;
    }

    reply_ = network_->get(request);
    connect(reply_, &QNetworkReply::downloadProgress, this, &ModsDialog::onDownloadProgress);
    connect(reply_, &QNetworkReply::finished, this, &ModsDialog::onDownloadFinished);
}

void ModsDialog::onDownloadProgress(qint64 received, qint64 total) {
    if (total <= 0) {
        progressBar_->setRange(0, 0);
        return;
    }

    progressBar_->setRange(0, 100);
    progressBar_->setValue(static_cast<int>((received * 100) / total));
}

void ModsDialog::onDownloadFinished() {
    if (!reply_) return;

    QNetworkReply* reply = reply_;
    reply_ = nullptr;

    const bool failed = reply->error() != QNetworkReply::NoError;
    const QString errorText = reply->errorString();
    const QByteArray payload = failed ? QByteArray() : reply->readAll();

    reply->deleteLater();

    installButton_->setEnabled(true);

    if (failed) {
        progressBar_->setVisible(false);
        setStatus("Download failed: " + errorText);
        return;
    }

    if (payload.isEmpty()) {
        progressBar_->setVisible(false);
        setStatus("Download failed: empty response.");
        return;
    }

    {
        std::ofstream out(pendingArchive_, std::ios::binary | std::ios::trunc);
        if (!out) {
            progressBar_->setVisible(false);
            setStatus("Could not write the downloaded archive.");
            return;
        }
        out.write(payload.constData(), payload.size());
    }

    installArchive(pendingArchive_);
}

void ModsDialog::installArchive(const std::string& archivePath) {
    setStatus("Installing ...");

    const std::string workDir = (fs::path(tempDir_->path().toStdString()) / "extract").string();
    std::string installError;
    const std::string target = mods::installArchive(gameDir_, archivePath, workDir, &installError);

    progressBar_->setVisible(false);
    installButton_->setEnabled(true);
    refresh();

    if (target.empty()) {
        setStatus(QString::fromStdString(installError.empty() ? "Install failed." : installError));
        return;
    }

    setStatus(QString("Installed in %1. Reload mods in game to apply.")
                  .arg(QString::fromStdString(fs::path(target).filename().string())));
}

void ModsDialog::installFromFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Select a mod archive", QString(), "Zip archives (*.zip)");
    if (path.isEmpty()) return;

    const std::string workDir = (fs::path(tempDir_->path().toStdString()) / "extract-file").string();
    std::string installError;
    const std::string target = mods::installArchive(gameDir_, path.toStdString(), workDir, &installError);

    refresh();

    if (target.empty()) {
        setStatus(QString::fromStdString(installError.empty() ? "Install failed." : installError));
        return;
    }

    setStatus(QString("Installed in %1. Reload mods in game to apply.")
                  .arg(QString::fromStdString(fs::path(target).filename().string())));
}

void ModsDialog::removeSelected() {
    const auto items = installedList_->selectedItems();
    if (items.isEmpty()) return;

    const int row = installedList_->row(items.first());
    if (row < 0 || row >= static_cast<int>(installed_.size())) return;

    const auto mod = installed_[row];

    const auto answer = QMessageBox::question(this, "Remove mod",
        QString("Delete \"%1\" from project-bo4/mods?").arg(QString::fromStdString(mod.name)),
        QMessageBox::Yes | QMessageBox::No);

    if (answer != QMessageBox::Yes) return;

    if (!mods::removeMod(mod)) {
        setStatus("Could not remove the mod folder.");
        return;
    }

    refresh();
    setStatus("Mod removed.");
}

void ModsDialog::toggleSelected() {
    const auto items = installedList_->selectedItems();
    if (items.isEmpty()) return;

    const int row = installedList_->row(items.first());
    if (row < 0 || row >= static_cast<int>(installed_.size())) return;

    const auto mod = installed_[row];
    const bool enable = !mod.enabled;

    if (!mods::setEnabled(gameDir_, mod, enable)) {
        setStatus("Could not move the mod folder.");
        return;
    }

    refresh();
    setStatus(enable ? "Mod enabled." : "Mod disabled (moved to project-bo4/mods_disabled).");
}
