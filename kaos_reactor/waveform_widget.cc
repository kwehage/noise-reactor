#include "kaos_reactor/waveform_widget.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace kaos::reactor {

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
}

void WaveformWidget::set_analysis(const AudioAnalysis& analysis) {
    rms_.clear();
    rms_.reserve(analysis.frames.size());
    for (const auto& frame : analysis.frames)
        rms_.push_back(frame.rms);
    duration_ms_ = analysis.duration * 1000.f;
    update();
}

void WaveformWidget::set_position(float t) {
    position_ = t;
    update();
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0e, 0x0e, 0x0e));

    if (rms_.empty()) return;

    const int w        = width();
    const int h        = height();
    const int mid      = h / 2;
    const int n        = static_cast<int>(rms_.size());
    const int playhead = static_cast<int>(position_ * w);

    auto amplitude = [&](int x) -> int {
        int idx = std::clamp(int(float(x) / float(w) * n), 0, n - 1);
        return int(rms_[idx] * (mid - 1) * 0.9f);
    };

    // Unplayed portion (dark)
    p.setPen(QColor(0x3a, 0x3a, 0x3a));
    for (int x = playhead; x < w; ++x) {
        int a = amplitude(x);
        p.drawLine(x, mid - a, x, mid + a);
    }

    // Played portion (brighter)
    p.setPen(QColor(0x72, 0x72, 0x72));
    for (int x = 0; x < playhead; ++x) {
        int a = amplitude(x);
        p.drawLine(x, mid - a, x, mid + a);
    }

    // Playhead
    p.setPen(QPen(QColor(0xe0, 0xe0, 0xe0), 1));
    p.drawLine(playhead, 0, playhead, h);
}

void WaveformWidget::mousePressEvent(QMouseEvent* event) {
    emit seek_requested(pixel_to_ms(static_cast<int>(event->position().x())));
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton)
        emit seek_requested(pixel_to_ms(static_cast<int>(event->position().x())));
}

int WaveformWidget::pixel_to_ms(int x) const {
    if (width() == 0 || duration_ms_ <= 0.f) return 0;
    float t = std::clamp(float(x) / float(width()), 0.f, 1.f);
    return static_cast<int>(t * duration_ms_);
}

} // namespace kaos::reactor
