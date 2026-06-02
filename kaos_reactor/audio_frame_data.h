#pragma once

#include <vector>

namespace kaos::reactor {

struct FrameData {
    float rms{0.f};               // normalized loudness [0, 1]
    float bass{0.f};              // normalized 20–250 Hz energy [0, 1]
    float mid{0.f};               // normalized 250–4000 Hz energy [0, 1]
    float treble{0.f};            // normalized 4000–20000 Hz energy [0, 1]
    float spectral_centroid{0.f}; // normalized brightness [0, 1]
    float spectral_flux{0.f};     // normalized rate of spectral change [0, 1]
    bool  beat{false};
    bool  onset{false};
    std::vector<float> spectrum;  // FFT magnitudes, fft_size/2+1 bins (raw, not normalized)
};

} // namespace kaos::reactor
