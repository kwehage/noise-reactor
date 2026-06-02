#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kaos::reactor {

class AudioFile {
public:
    static std::optional<AudioFile> load(const std::filesystem::path& path);

    const std::vector<float>& samples()      const { return samples_; }
    const std::vector<float>& mono_samples() const { return mono_; }
    int    sample_rate() const { return sample_rate_; }
    int    channels()    const { return channels_; }
    double duration()    const { return duration_; }
    const std::string& error() const { return error_; }

private:
    AudioFile() = default;
    void mix_to_mono();

    std::vector<float> samples_;   // interleaved, all channels
    std::vector<float> mono_;      // mixed to mono, used for analysis
    int         sample_rate_{0};
    int         channels_{0};
    double      duration_{0.0};
    std::string error_;
};

} // namespace kaos::reactor
