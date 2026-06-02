#pragma once

#include "kaos_reactor/audio_frame_data.h"

#include <vector>

namespace kaos::reactor {

struct AudioAnalysis {
    std::vector<FrameData> frames;
    float fps{60.f};
    float duration{0.f};
    int   sample_rate{0};
    int   fft_size{2048};

    bool empty() const { return frames.empty(); }

    int frame_for_time(float t) const {
        if (frames.empty()) return 0;
        int idx = static_cast<int>(t * fps);
        if (idx < 0) return 0;
        if (idx >= static_cast<int>(frames.size()))
            return static_cast<int>(frames.size()) - 1;
        return idx;
    }
};

} // namespace kaos::reactor
