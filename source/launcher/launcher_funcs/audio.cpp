#include "std_include.hpp"
#include "audio.hpp"

#ifdef _WIN32
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#else
#include <QMediaPlayer>
#include <QAudioOutput>
#endif

namespace launcher_audio {
#ifdef _WIN32
    void playStartupSound(QWidget* parent, const std::wstring& path, int volume) {
        if (!QFile::exists(QString::fromStdWString(path))) {
            return;
        }

        // Close any existing MP3 playback
        mciSendStringW(L"close mp3", NULL, 0, NULL);

        std::wstring openCmd = L"open \"" + path + L"\" type mpegvideo alias mp3";
        MCIERROR openError = mciSendStringW(openCmd.c_str(), NULL, 0, NULL);

        if (openError != 0) {
            wchar_t errorText[256];
            mciGetErrorStringW(openError, errorText, 256);
            QMessageBox::warning(parent, "Sound Error", "Failed to open sound file: " +
                                QString::fromWCharArray(errorText));
            return;
        }

        setVolume(volume);

        MCIERROR playError = mciSendStringW(L"play mp3", NULL, 0, NULL);

        if (playError != 0) {
            wchar_t errorText[256];
            mciGetErrorStringW(playError, errorText, 256);
            QMessageBox::warning(parent, "Sound Error", "Failed to play sound file: " +
                                QString::fromWCharArray(errorText));
        }
    }

    void setVolume(int volume) {
        // MCI volume range is 0-1000, ours is 0-100
        std::wstring volumeCmd = L"setaudio mp3 volume to " + std::to_wstring(volume * 10);
        mciSendStringW(volumeCmd.c_str(), NULL, 0, NULL);
    }

    void stop() {
        mciSendStringW(L"close mp3", NULL, 0, NULL);
    }
#else
    namespace {
        QMediaPlayer* player = nullptr;
        QAudioOutput* output = nullptr;

        void ensurePlayer() {
            if (player) return;

            player = new QMediaPlayer();
            output = new QAudioOutput();
            player->setAudioOutput(output);
        }
    }

    void playStartupSound(QWidget* /*parent*/, const std::wstring& path, int volume) {
        const QString qPath = QString::fromStdWString(path);
        if (!QFile::exists(qPath)) {
            return;
        }

        ensurePlayer();
        setVolume(volume);
        player->setSource(QUrl::fromLocalFile(qPath));
        player->play();
    }

    void setVolume(int volume) {
        if (!output) return;

        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        output->setVolume(static_cast<float>(volume) / 100.0f);
    }

    void stop() {
        if (player) player->stop();
    }
#endif
}
