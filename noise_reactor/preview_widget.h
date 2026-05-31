#pragma once

#include "noise_reactor/audio_frame_data.h"
#include "noise_reactor/effect_params.h"

#include <QImage>
#include <QPoint>
#include <QRhiWidget>

class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
class QRhiTextureRenderTarget;

namespace noise_reactor {

class PreviewWidget : public QRhiWidget {
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr);

    void set_image(const QImage& image);
    void set_frame_data(const FrameData& frame, const EffectParams& params);
    void set_exporting(bool exporting);
    void set_export_resolution(QSize res);
    void set_cinematic(float zoom, float pan_x, float pan_y);
    void begin_grab();
    void end_grab();
    bool has_image() const { return !source_image_.isNull(); }

signals:
    void image_dropped(const QString& path);
    void audio_dropped(const QString& path);

protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void cleanup();
    void prepare_upload();
    void rebuild_bindings();
    void rebuild_pipeline();
    void rebuild_composite_bindings();
    void rebuild_composite_pipeline();

    // Image + effect resources
    QRhi*                       rhi_{nullptr};
    QRhiTexture*                texture_{nullptr};
    QRhiSampler*                sampler_{nullptr};
    QRhiBuffer*                 uniform_buffer_{nullptr};
    QRhiShaderResourceBindings* bindings_{nullptr};
    QRhiGraphicsPipeline*       pipeline_{nullptr};

    // Feedback / two-pass resources
    QRhiTexture*                scene_texture_{nullptr};   // current frame rendered here
    QRhiTexture*                feedback_texture_{nullptr}; // previous frame, fed back into shader
    QRhiTextureRenderTarget*    scene_rt_{nullptr};
    QRhiRenderPassDescriptor*   scene_rp_desc_{nullptr};
    QRhiShaderResourceBindings* composite_bindings_{nullptr};
    QRhiGraphicsPipeline*       composite_pipeline_{nullptr};

    QImage    source_image_{};
    QImage    pending_image_{};  // possibly Y-flipped for current backend
    QSize     export_resolution_{};
    float     image_ar_{1.f};
    float     viewport_ar_{1.f};
    float     pan_x_{0.f};
    float     pan_y_{0.f};
    float     zoom_scale_{1.f};
    QPoint    drag_last_pos_{};
    bool      y_up_in_ndc_{false};
    bool      image_dirty_{false};
    bool      exporting_{false};
    bool      grab_in_progress_{false};
    EffectUBO ubo_data_{};
    float     time_{0.f};
};

} // namespace noise_reactor
