#pragma once

#include "kaos_reactor/audio_analysis.h"

#include <QWidget>
#include <vector>

namespace kaos::reactor {

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);

    void set_analysis(const AudioAnalysis& analysis);
    void set_position(float t);  // normalised 0–1

signals:
    void seek_requested(int ms);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    int pixel_to_ms(int x) const;

    std::vector<float> rms_;
    float              duration_ms_{0.f};
    float              position_{0.f};
};

} // namespace kaos::reactor
