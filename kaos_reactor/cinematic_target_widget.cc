#include "kaos_reactor/cinematic_target_widget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <algorithm>

namespace kaos::reactor {

CinematicTargetWidget::CinematicTargetWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(88);
    setCursor(Qt::CrossCursor);
}

void CinematicTargetWidget::set_aspect_ratio(float ar) {
    ar_ = ar > 0.f ? ar : 16.f / 9.f;
    update();
}

QRect CinematicTargetWidget::frame_rect() const {
    const int pad = 2;
    const int w   = width()  - pad * 2;
    const int h   = height() - pad * 2;
    int fw = w;
    int fh = static_cast<int>(fw / ar_);
    if (fh > h) {
        fh = h;
        fw = static_cast<int>(fh * ar_);
    }
    return QRect(pad + (w - fw) / 2, pad + (h - fh) / 2, fw, fh);
}

void CinematicTargetWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect fr = frame_rect();

    p.fillRect(rect(), QColor(18, 18, 18));
    p.fillRect(fr, QColor(28, 28, 28));

    // Rule-of-thirds grid
    p.setPen(QPen(QColor(52, 52, 52), 1));
    for (int i = 1; i <= 2; ++i) {
        p.drawLine(fr.x() + fr.width() * i / 3, fr.y(),
                   fr.x() + fr.width() * i / 3, fr.y() + fr.height());
        p.drawLine(fr.x(), fr.y() + fr.height() * i / 3,
                   fr.x() + fr.width(), fr.y() + fr.height() * i / 3);
    }

    // Frame border
    p.setPen(QPen(QColor(75, 75, 75), 1));
    p.drawRect(fr.adjusted(0, 0, -1, -1));

    // Full-width/height crosshair lines through target
    const int tx = fr.x() + static_cast<int>(target_.x() * fr.width());
    const int ty = fr.y() + static_cast<int>(target_.y() * fr.height());

    p.setPen(QPen(QColor(160, 160, 160), 1));
    p.drawLine(fr.x(), ty, fr.x() + fr.width() - 1, ty);
    p.drawLine(tx, fr.y(), tx, fr.y() + fr.height() - 1);

    // Target circle
    p.setPen(QPen(QColor(220, 220, 220), 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPoint(tx, ty), 5, 5);
}

void CinematicTargetWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        update_from_pos(event->pos());
}

void CinematicTargetWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton)
        update_from_pos(event->pos());
}

void CinematicTargetWidget::update_from_pos(QPoint pos) {
    const QRect fr = frame_rect();
    if (fr.isEmpty()) return;
    const float nx = std::clamp(float(pos.x() - fr.x()) / float(fr.width()),  0.f, 1.f);
    const float ny = std::clamp(float(pos.y() - fr.y()) / float(fr.height()), 0.f, 1.f);
    target_ = QPointF(nx, ny);
    update();
    emit target_changed(target_);
}

} // namespace kaos::reactor
