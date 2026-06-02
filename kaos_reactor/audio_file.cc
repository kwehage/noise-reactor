#include "kaos_reactor/audio_file.h"

#include <sndfile.h>

namespace kaos::reactor {

std::optional<AudioFile> AudioFile::load(const std::filesystem::path& path) {
    SF_INFO info{};
#ifdef _WIN32
    SNDFILE* sf = sf_wchar_open(path.c_str(), SFM_READ, &info);
#else
    SNDFILE* sf = sf_open(path.c_str(), SFM_READ, &info);
#endif
    if (!sf)
        return std::nullopt;

    AudioFile file;
    file.sample_rate_ = info.samplerate;
    file.channels_    = info.channels;
    file.duration_    = static_cast<double>(info.frames) / info.samplerate;

    file.samples_.resize(static_cast<size_t>(info.frames) * info.channels);
    sf_count_t read = sf_readf_float(sf, file.samples_.data(), info.frames);
    sf_close(sf);

    if (read <= 0)
        return std::nullopt;

    file.samples_.resize(static_cast<size_t>(read) * info.channels);
    file.mix_to_mono();
    return file;
}

void AudioFile::mix_to_mono() {
    const size_t n_frames = samples_.size() / channels_;
    mono_.resize(n_frames);

    if (channels_ == 1) {
        mono_ = samples_;
        return;
    }

    const float scale = 1.f / channels_;
    for (size_t i = 0; i < n_frames; ++i) {
        float sum = 0.f;
        for (int c = 0; c < channels_; ++c)
            sum += samples_[i * channels_ + c];
        mono_[i] = sum * scale;
    }
}

} // namespace kaos::reactor
