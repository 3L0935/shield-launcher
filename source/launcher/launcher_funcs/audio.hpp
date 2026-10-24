#pragma once
#include <string>

class QWidget;

namespace launcher_audio {
    // The launcher only plays one short mp3 at startup. Windows relies on the
    // MCI API, every other platform on QtMultimedia; both take a 0..100 volume.
    void playStartupSound(QWidget* parent, const std::wstring& path, int volume);
    void setVolume(int volume);
    void stop();
}
