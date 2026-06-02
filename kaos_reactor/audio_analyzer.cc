#include "kaos_reactor/audio_analyzer.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <numbers>
#include <fftw3.h>
#include <numeric>

namespace kaos::reactor {

namespace {

std::vector<float> make_hann_window(int n) {
    std::vector<float> w(n);
    const float twopi_over_nm1 = 2.f * std::numbers::pi_v<float> / (n - 1);
    for (int i = 0; i < n; ++i)
        w[i] = 0.5f * (1.f - std::cos(twopi_over_nm1 * i));
    return w;
}

int freq_to_bin(float freq, int sr, int fft_size) {
    int bin = static_cast<int>(std::round(freq * fft_size / sr));
    return std::clamp(bin, 0, fft_size / 2);
}

float band_energy(const float* mag, int lo, int hi) {
    float e = 0.f;
    for (int i = lo; i <= hi; ++i)
        e += mag[i] * mag[i];
    return e;
}

} // namespace

AudioAnalysis AudioAnalyzer::analyze(const AudioFile& file) {
    return analyze(file, Config{});
}

AudioAnalysis AudioAnalyzer::analyze(const AudioFile& file, Config cfg) {
    const auto& samples  = file.mono_samples();
    const int   sr       = file.sample_rate();
    const int   hop      = std::max(1, static_cast<int>(sr / cfg.fps));
    const int   n_frames = static_cast<int>(samples.size()) / hop;
    const int   n_bins   = cfg.fft_size / 2 + 1;

    AudioAnalysis out;
    out.fps         = cfg.fps;
    out.duration    = static_cast<float>(file.duration());
    out.sample_rate = sr;
    out.fft_size    = cfg.fft_size;

    if (n_frames <= 0)
        return out;

    out.frames.resize(n_frames);

    float*         fft_in  = fftwf_alloc_real(cfg.fft_size);
    fftwf_complex* fft_out = fftwf_alloc_complex(n_bins);
    fftwf_plan     plan    = fftwf_plan_dft_r2c_1d(cfg.fft_size, fft_in, fft_out, FFTW_ESTIMATE);

    const auto hann = make_hann_window(cfg.fft_size);
    const int  half = cfg.fft_size / 2;

    const int bass_lo   = freq_to_bin(20.f,    sr, cfg.fft_size);
    const int bass_hi   = freq_to_bin(250.f,   sr, cfg.fft_size);
    const int mid_lo    = freq_to_bin(250.f,   sr, cfg.fft_size);
    const int mid_hi    = freq_to_bin(4000.f,  sr, cfg.fft_size);
    const int treble_lo = freq_to_bin(4000.f,  sr, cfg.fft_size);
    const int treble_hi = freq_to_bin(20000.f, sr, cfg.fft_size);

    std::vector<float> mag(n_bins);
    std::vector<float> prev_mag(n_bins, 0.f);

    // ── Pass 1: FFT and feature extraction ───────────────────────────────
    for (int f = 0; f < n_frames; ++f) {
        const int center = f * hop;
        const int start  = center - half;

        for (int i = 0; i < cfg.fft_size; ++i) {
            int idx    = start + i;
            float s    = (idx >= 0 && idx < static_cast<int>(samples.size()))
                         ? samples[idx] : 0.f;
            fft_in[i]  = s * hann[i];
        }

        fftwf_execute(plan);

        const float norm = 1.f / cfg.fft_size;
        for (int i = 0; i < n_bins; ++i)
            mag[i] = std::hypot(fft_out[i][0], fft_out[i][1]) * norm;

        FrameData& fd = out.frames[f];
        fd.spectrum.assign(mag.begin(), mag.end());

        // RMS over hop window
        {
            float sum = 0.f;
            int   n   = 0;
            for (int i = center; i < center + hop && i < static_cast<int>(samples.size()); ++i, ++n)
                sum += samples[i] * samples[i];
            fd.rms = n > 0 ? std::sqrt(sum / n) : 0.f;
        }

        // Band energy (sqrt brings to perceptually linear scale)
        fd.bass   = std::sqrt(band_energy(mag.data(), bass_lo,   bass_hi));
        fd.mid    = std::sqrt(band_energy(mag.data(), mid_lo,    mid_hi));
        fd.treble = std::sqrt(band_energy(mag.data(), treble_lo, treble_hi));

        // Spectral centroid
        {
            float num = 0.f, den = 0.f;
            for (int i = 1; i < n_bins; ++i) {
                float freq = static_cast<float>(i * sr) / cfg.fft_size;
                num += freq * mag[i];
                den += mag[i];
            }
            fd.spectral_centroid = den > 1e-8f ? num / den : 0.f;
        }

        // Spectral flux: sum of positive magnitude increases
        {
            float flux = 0.f;
            for (int i = 0; i < n_bins; ++i) {
                float d = mag[i] - prev_mag[i];
                if (d > 0.f) flux += d;
            }
            fd.spectral_flux = flux;
        }
        prev_mag = mag;
    }

    fftwf_destroy_plan(plan);
    fftwf_free(fft_out);
    fftwf_free(fft_in);

    // ── Pass 2: Normalize scalar features to [0, 1] ───────────────────
    {
        float max_rms = 0, max_bass = 0, max_mid = 0, max_treble = 0;
        float max_cent = 0, max_flux = 0;
        for (const auto& fd : out.frames) {
            max_rms    = std::max(max_rms,    fd.rms);
            max_bass   = std::max(max_bass,   fd.bass);
            max_mid    = std::max(max_mid,    fd.mid);
            max_treble = std::max(max_treble, fd.treble);
            max_cent   = std::max(max_cent,   fd.spectral_centroid);
            max_flux   = std::max(max_flux,   fd.spectral_flux);
        }
        auto div = [](float v, float m) { return m > 1e-8f ? v / m : 0.f; };
        for (auto& fd : out.frames) {
            fd.rms               = div(fd.rms,               max_rms);
            fd.bass              = div(fd.bass,              max_bass);
            fd.mid               = div(fd.mid,               max_mid);
            fd.treble            = div(fd.treble,            max_treble);
            fd.spectral_centroid = div(fd.spectral_centroid, max_cent);
            fd.spectral_flux     = div(fd.spectral_flux,     max_flux);
        }
    }

    // ── Pass 3: Beat detection on normalized RMS ──────────────────────
    {
        std::deque<float> history;
        const int min_gap  = std::max(1, static_cast<int>(cfg.fps * 0.25f));
        int       last_beat = -min_gap;

        for (int f = 0; f < n_frames; ++f) {
            float e = out.frames[f].rms;
            history.push_back(e);
            if (static_cast<int>(history.size()) > cfg.beat_history)
                history.pop_front();

            float mean = std::accumulate(history.begin(), history.end(), 0.f)
                         / static_cast<float>(history.size());

            if (e > 0.1f && e > cfg.beat_sensitivity * mean && (f - last_beat) >= min_gap) {
                out.frames[f].beat = true;
                last_beat = f;
            }
        }
    }

    // ── Pass 4: Onset detection on normalized spectral flux ───────────
    {
        const int min_gap   = std::max(1, static_cast<int>(cfg.fps * 0.05f));
        int       last_onset = -min_gap;

        for (int f = 0; f < n_frames; ++f) {
            if (out.frames[f].spectral_flux > cfg.onset_threshold
                && (f - last_onset) >= min_gap)
            {
                out.frames[f].onset = true;
                last_onset = f;
            }
        }
    }

    return out;
}

} // namespace kaos::reactor
