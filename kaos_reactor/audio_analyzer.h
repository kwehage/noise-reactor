#pragma once

#include "kaos_reactor/audio_analysis.h"
#include "kaos_reactor/audio_file.h"

namespace kaos::reactor {

class AudioAnalyzer {
public:
    struct Config {
        float fps              = 60.f;
        int   fft_size         = 2048;
        float beat_sensitivity = 1.5f;  // local RMS must exceed this × windowed mean
        int   beat_history     = 43;    // frames of history for mean (~700 ms at 60 fps)
        float onset_threshold  = 0.15f; // normalized flux threshold for onset
    };

    static AudioAnalysis analyze(const AudioFile& file);
    static AudioAnalysis analyze(const AudioFile& file, Config cfg);
};

} // namespace kaos::reactor
