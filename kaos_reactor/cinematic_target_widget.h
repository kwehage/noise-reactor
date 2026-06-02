#pragma once

#include <QPointF>
#include <QWidget>

namespace kaos::reactor {

class CinematicTargetWidget : public QWidget {
    Q_OBJECT
public:
    explicit CinematicTargetWidget(QWidget* parent = nullptr);

    void    set_aspect_ratio(float ar);
    QPointF target() const { return target_; }

signals:
    void target_changed(QPointF normalized_pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QRect frame_rect() const;
    void  update_from_pos(QPoint pos);

    float   ar_{16.f / 9.f};
    QPointF target_{0.5f, 0.5f};
};

} // namespace kaos::reactor
