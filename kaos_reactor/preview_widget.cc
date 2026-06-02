#include "kaos_reactor/preview_widget.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QSet>
#include <QSizePolicy>
#include <QWheelEvent>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <algorithm>
#include <cmath>

namespace kaos::reactor {

static QImage make_placeholder() {
    QImage img(1, 1, QImage::Format_RGBA8888);
    img.fill(QColor(18, 18, 18, 255));
    return img;
}

static QShader load_shader(const char* filename) {
    QFile file(QString(":/shaders/") + filename);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot open shader resource: %s", filename);
    const QShader shader = QShader::fromSerialized(file.readAll());
    if (!shader.isValid())
        qFatal("Invalid shader resource: %s", filename);
    return shader;
}

static bool is_image_file(const QString& path) {
    static const QSet<QString> exts{
        "png", "jpg", "jpeg", "bmp", "tiff", "tif"};
    return exts.contains(QFileInfo(path).suffix().toLower());
}

static bool is_audio_file(const QString& path) {
    static const QSet<QString> exts{
        "wav", "flac", "mp3", "ogg", "aiff", "aif"};
    return exts.contains(QFileInfo(path).suffix().toLower());
}

PreviewWidget::PreviewWidget(QWidget* parent) : QRhiWidget(parent) {
    setMinimumSize(640, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAcceptDrops(true);
    ubo_data_.zoom_scale = 1.f;
}

void PreviewWidget::set_image(const QImage& image) {
    source_image_ = image.convertToFormat(QImage::Format_RGBA8888);
    image_ar_ = source_image_.height() > 0
        ? float(source_image_.width()) / float(source_image_.height()) : 1.f;
    ubo_data_.image_ar = image_ar_;
    prepare_upload();
    image_dirty_ = true;
    update();
}

void PreviewWidget::set_frame_data(const FrameData& frame, const EffectParams& params) {
    ubo_data_.rms                    = frame.rms;
    ubo_data_.bass                   = frame.bass;
    ubo_data_.mid                    = frame.mid;
    ubo_data_.treble                 = frame.treble;
    ubo_data_.spectral_centroid      = frame.spectral_centroid;
    ubo_data_.spectral_flux          = frame.spectral_flux;
    ubo_data_.beat                   = frame.beat  ? 1.f : 0.f;
    ubo_data_.onset                  = frame.onset ? 1.f : 0.f;
    ubo_data_.zoom_intensity         = params.zoom_intensity;
    ubo_data_.wave_intensity         = params.wave_intensity;
    ubo_data_.hue_shift_intensity    = params.hue_shift_intensity;
    ubo_data_.glitch_intensity       = params.glitch_intensity;
    ubo_data_.glow_intensity         = params.glow_intensity;
    ubo_data_.displacement_intensity = params.displacement_intensity;
    ubo_data_.perlin_intensity       = params.perlin_intensity;
    ubo_data_.warp_scale             = params.warp_scale;
    ubo_data_.brightness_intensity   = params.brightness_intensity;
    ubo_data_.saturation_intensity   = params.saturation_intensity;
    ubo_data_.vignette_intensity     = params.vignette_intensity;
    ubo_data_.chromatic_intensity    = params.chromatic_intensity;
    ubo_data_.film_grain_intensity   = params.film_grain_intensity;
    ubo_data_.pixelate_intensity     = params.pixelate_intensity;
    ubo_data_.edge_glow_intensity    = params.edge_glow_intensity;
    ubo_data_.morph_intensity        = params.morph_intensity;
    ubo_data_.barrel_intensity       = params.barrel_intensity;
    ubo_data_.kaleidoscope_intensity = params.kaleidoscope_intensity;
    ubo_data_.polar_intensity        = params.polar_intensity;
    ubo_data_.ripple_intensity       = params.ripple_intensity;
    ubo_data_.posterize_intensity    = params.posterize_intensity;
    ubo_data_.solarize_intensity     = params.solarize_intensity;
    ubo_data_.channel_swap_intensity = params.channel_swap_intensity;
    ubo_data_.duotone_intensity      = params.duotone_intensity;
    ubo_data_.emboss_intensity       = params.emboss_intensity;
    ubo_data_.scanline_intensity     = params.scanline_intensity;
    ubo_data_.feedback_intensity     = params.feedback_intensity;
    ubo_data_.zoom_feedback          = params.zoom_feedback;
    ubo_data_.wave_feedback          = params.wave_feedback;
    ubo_data_.hue_shift_feedback     = params.hue_shift_feedback;
    ubo_data_.glitch_feedback        = params.glitch_feedback;
    ubo_data_.glow_feedback          = params.glow_feedback;
    ubo_data_.displacement_feedback  = params.displacement_feedback;
    ubo_data_.perlin_feedback        = params.perlin_feedback;
    ubo_data_.brightness_feedback    = params.brightness_feedback;
    ubo_data_.saturation_feedback    = params.saturation_feedback;
    ubo_data_.vignette_feedback      = params.vignette_feedback;
    ubo_data_.chromatic_feedback     = params.chromatic_feedback;
    ubo_data_.film_grain_feedback    = params.film_grain_feedback;
    ubo_data_.pixelate_feedback      = params.pixelate_feedback;
    ubo_data_.edge_glow_feedback     = params.edge_glow_feedback;
    ubo_data_.morph_feedback         = params.morph_feedback;
    ubo_data_.barrel_feedback        = params.barrel_feedback;
    ubo_data_.kaleidoscope_feedback  = params.kaleidoscope_feedback;
    ubo_data_.polar_feedback         = params.polar_feedback;
    ubo_data_.ripple_feedback        = params.ripple_feedback;
    ubo_data_.posterize_feedback     = params.posterize_feedback;
    ubo_data_.solarize_feedback      = params.solarize_feedback;
    ubo_data_.channel_swap_feedback  = params.channel_swap_feedback;
    ubo_data_.duotone_feedback       = params.duotone_feedback;
    ubo_data_.emboss_feedback        = params.emboss_feedback;
    ubo_data_.scanline_feedback      = params.scanline_feedback;
}

// ── QRhiWidget lifecycle ──────────────────────────────────────────────────────

void PreviewWidget::prepare_upload() {
    // Y-flip: Vulkan NDC has Y-down so (0,0) is top-left — no flip needed.
    // OpenGL NDC has Y-up so (0,0) maps to bottom-left — flip required.
    if (!source_image_.isNull() && rhi_)
        pending_image_ = rhi_->isYUpInNDC()
            ? source_image_.mirrored(false, true)
            : source_image_;
    else
        pending_image_ = source_image_;
}

void PreviewWidget::initialize(QRhiCommandBuffer* cb) {
    rhi_ = rhi();
    y_up_in_ndc_ = rhi_->isYUpInNDC();
    cleanup();

    // Re-compute Y-flip now that we know the backend.
    prepare_upload();

    const QImage& initial = pending_image_.isNull() ? make_placeholder() : pending_image_;

    texture_ = rhi_->newTexture(QRhiTexture::RGBA8, initial.size(), 1);
    texture_->create();

    sampler_ = rhi_->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                QRhiSampler::None,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler_->create();

    uniform_buffer_ = rhi_->newBuffer(QRhiBuffer::Dynamic,
                                      QRhiBuffer::UniformBuffer,
                                      sizeof(EffectUBO));
    uniform_buffer_->create();

    // Feedback / two-pass resources — sized to match the current render target.
    const QSize fb_size = renderTarget()->pixelSize();

    scene_texture_ = rhi_->newTexture(QRhiTexture::RGBA8, fb_size, 1,
                                      QRhiTexture::RenderTarget |
                                      QRhiTexture::UsedAsTransferSource);
    scene_texture_->create();

    feedback_texture_ = rhi_->newTexture(QRhiTexture::RGBA8, fb_size, 1);
    feedback_texture_->create();

    scene_rt_       = rhi_->newTextureRenderTarget({scene_texture_});
    scene_rp_desc_  = scene_rt_->newCompatibleRenderPassDescriptor();
    scene_rt_->setRenderPassDescriptor(scene_rp_desc_);
    scene_rt_->create();

    rebuild_bindings();
    rebuild_pipeline();
    rebuild_composite_bindings();
    rebuild_composite_pipeline();

    // Black-fill feedback so first frame has no garbage contribution.
    QImage black(fb_size, QImage::Format_RGBA8888);
    black.fill(Qt::black);

    auto* batch = rhi_->nextResourceUpdateBatch();
    batch->uploadTexture(texture_, initial);
    batch->uploadTexture(feedback_texture_, black);
    batch->updateDynamicBuffer(uniform_buffer_, 0, sizeof(EffectUBO), &ubo_data_);
    cb->resourceUpdate(batch);

    image_dirty_ = false;
}

void PreviewWidget::set_cinematic(float zoom, float pan_x, float pan_y) {
    ubo_data_.cinematic_zoom  = zoom;
    ubo_data_.cinematic_pan_x = pan_x;
    ubo_data_.cinematic_pan_y = y_up_in_ndc_ ? -pan_y : pan_y;
}

void PreviewWidget::begin_grab() { grab_in_progress_ = true; }
void PreviewWidget::end_grab()   { grab_in_progress_ = false; }

void PreviewWidget::render(QRhiCommandBuffer* cb) {
    // Block viewport renders that race with export grabs. setUpdatesEnabled(false)
    // suppresses Qt paint events but the compositor can still invoke render() when
    // a sibling widget (e.g. the progress dialog) repaints the parent window. If
    // both a compositor render and grabFramebuffer() record into the same command
    // buffer concurrently the GPU driver crashes. Only allow a full render when we
    // are explicitly inside a begin_grab()/end_grab() bracket.
    if (exporting_ && !grab_in_progress_) {
        cb->beginPass(renderTarget(), QColor(0, 0, 0), {1.0f, 0}, nullptr);
        cb->endPass();
        return;
    }

    time_ += 1.f / 60.f;
    ubo_data_.time = time_;

    // Compute viewport first so fb_uv_* fields are set before the UBO upload.
    const QSize scene_size = scene_rt_->pixelSize();
    QRhiViewport viewport{0, 0, float(scene_size.width()), float(scene_size.height())};
    ubo_data_.fb_uv_offset_x = 0.f;
    ubo_data_.fb_uv_offset_y = 0.f;
    ubo_data_.fb_uv_scale_x  = 1.f;
    ubo_data_.fb_uv_scale_y  = 1.f;
    if (!exporting_ && export_resolution_.isValid()) {
        const float target_ar = float(export_resolution_.width()) / float(export_resolution_.height());
        const float widget_ar = float(scene_size.width()) / float(scene_size.height());
        float vp_w, vp_h, vp_x, vp_y;
        if (widget_ar > target_ar) {
            vp_h = float(scene_size.height());
            vp_w = vp_h * target_ar;
            vp_x = (float(scene_size.width()) - vp_w) * 0.5f;
            vp_y = 0.f;
        } else {
            vp_w = float(scene_size.width());
            vp_h = vp_w / target_ar;
            vp_x = 0.f;
            vp_y = (float(scene_size.height()) - vp_h) * 0.5f;
        }
        viewport = {vp_x, vp_y, vp_w, vp_h};
        ubo_data_.fb_uv_offset_x = vp_x / float(scene_size.width());
        ubo_data_.fb_uv_offset_y = vp_y / float(scene_size.height());
        ubo_data_.fb_uv_scale_x  = vp_w / float(scene_size.width());
        ubo_data_.fb_uv_scale_y  = vp_h / float(scene_size.height());
    }

    auto* batch = rhi_->nextResourceUpdateBatch();

    if (image_dirty_ && !pending_image_.isNull()) {
        if (texture_->pixelSize() != pending_image_.size()) {
            delete texture_;
            texture_ = rhi_->newTexture(QRhiTexture::RGBA8, pending_image_.size(), 1);
            texture_->create();
            rebuild_bindings();
        }
        batch->uploadTexture(texture_, pending_image_);
        image_dirty_ = false;
    }

    batch->updateDynamicBuffer(uniform_buffer_, 0, sizeof(EffectUBO), &ubo_data_);

    // ── Pass 1: render scene (effects + feedback) into scene_texture_ ────────────
    cb->beginPass(scene_rt_, QColor(0, 0, 0), {1.0f, 0}, batch);
    cb->setGraphicsPipeline(pipeline_);
    cb->setViewport(viewport);
    cb->setShaderResources(bindings_);
    cb->draw(6);
    cb->endPass();

    // ── Copy scene → feedback for next frame ─────────────────────────────────────
    auto* copy_batch = rhi_->nextResourceUpdateBatch();
    copy_batch->copyTexture(feedback_texture_, scene_texture_);
    cb->resourceUpdate(copy_batch);

    // ── Pass 2: composite scene_texture_ onto the widget's render target ─────────
    const QSize rt_size = renderTarget()->pixelSize();
    cb->beginPass(renderTarget(), QColor(0, 0, 0), {1.0f, 0}, nullptr);
    cb->setGraphicsPipeline(composite_pipeline_);
    cb->setViewport({0, 0, float(rt_size.width()), float(rt_size.height())});
    cb->setShaderResources(composite_bindings_);
    cb->draw(6);
    cb->endPass();

    if (!exporting_)
        update();
}

void PreviewWidget::set_exporting(bool exporting) {
    exporting_ = exporting;
    setUpdatesEnabled(!exporting);
    if (!exporting)
        update();
}

void PreviewWidget::set_export_resolution(QSize res) {
    export_resolution_ = res;
    viewport_ar_ = (res.isValid() && res.height() > 0)
        ? float(res.width()) / float(res.height()) : image_ar_;
    ubo_data_.viewport_ar = viewport_ar_;
    update();
}

void PreviewWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (exporting_) return;
    if (!event->mimeData()->hasUrls())
        return;
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (is_image_file(path) || is_audio_file(path)) {
            event->acceptProposedAction();
            return;
        }
    }
}

void PreviewWidget::dropEvent(QDropEvent* event) {
    if (exporting_) return;
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (is_image_file(path)) {
            emit image_dropped(path);
            event->acceptProposedAction();
            return;
        }
        if (is_audio_file(path)) {
            emit audio_dropped(path);
            event->acceptProposedAction();
            return;
        }
    }
}

void PreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (exporting_) return;
    if (event->button() == Qt::LeftButton)
        drag_last_pos_ = event->pos();
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (exporting_) return;
    if (!(event->buttons() & Qt::LeftButton))
        return;
    const QPoint delta = event->pos() - drag_last_pos_;
    drag_last_pos_ = event->pos();

    const float y_sign = y_up_in_ndc_ ? -1.f : 1.f;
    pan_x_ += float(delta.x())          / (float(width())  * zoom_scale_);
    pan_y_ += y_sign * float(delta.y()) / (float(height()) * zoom_scale_);
    ubo_data_.pan_x = pan_x_;
    ubo_data_.pan_y = pan_y_;
    update();
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (exporting_) return;
    if (event->button() == Qt::LeftButton)
        drag_last_pos_ = {};
}

void PreviewWidget::wheelEvent(QWheelEvent* event) {
    if (exporting_) return;
    const float steps = float(event->angleDelta().y()) / 120.f;
    zoom_scale_ = std::clamp(zoom_scale_ * std::pow(1.15f, steps), 0.1f, 20.f);
    ubo_data_.zoom_scale = zoom_scale_;
    update();
}

void PreviewWidget::releaseResources() {
    cleanup();
    rhi_ = nullptr;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void PreviewWidget::cleanup() {
    delete composite_pipeline_;  composite_pipeline_  = nullptr;
    delete composite_bindings_;  composite_bindings_  = nullptr;
    delete scene_rp_desc_;       scene_rp_desc_       = nullptr;
    delete scene_rt_;            scene_rt_            = nullptr;
    delete feedback_texture_;    feedback_texture_    = nullptr;
    delete scene_texture_;       scene_texture_       = nullptr;
    delete pipeline_;            pipeline_            = nullptr;
    delete bindings_;            bindings_            = nullptr;
    delete uniform_buffer_;      uniform_buffer_      = nullptr;
    delete sampler_;             sampler_             = nullptr;
    delete texture_;             texture_             = nullptr;
}

void PreviewWidget::rebuild_bindings() {
    delete bindings_;
    bindings_ = rhi_->newShaderResourceBindings();
    bindings_->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::FragmentStage, texture_, sampler_),
        QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::FragmentStage, uniform_buffer_),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, feedback_texture_, sampler_),
    });
    bindings_->create();
}

void PreviewWidget::rebuild_pipeline() {
    delete pipeline_;
    pipeline_ = rhi_->newGraphicsPipeline();
    pipeline_->setShaderStages({
        { QRhiShaderStage::Vertex,   load_shader("image.vert.qsb") },
        { QRhiShaderStage::Fragment, load_shader("image.frag.qsb") },
    });
    pipeline_->setVertexInputLayout({});
    pipeline_->setShaderResourceBindings(bindings_);
    pipeline_->setRenderPassDescriptor(scene_rp_desc_);
    pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
    pipeline_->setCullMode(QRhiGraphicsPipeline::None);
    pipeline_->create();
}

void PreviewWidget::rebuild_composite_bindings() {
    delete composite_bindings_;
    composite_bindings_ = rhi_->newShaderResourceBindings();
    composite_bindings_->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::FragmentStage, scene_texture_, sampler_),
    });
    composite_bindings_->create();
}

void PreviewWidget::rebuild_composite_pipeline() {
    delete composite_pipeline_;
    composite_pipeline_ = rhi_->newGraphicsPipeline();
    composite_pipeline_->setShaderStages({
        { QRhiShaderStage::Vertex,   load_shader("image.vert.qsb") },
        { QRhiShaderStage::Fragment, load_shader("composite.frag.qsb") },
    });
    composite_pipeline_->setVertexInputLayout({});
    composite_pipeline_->setShaderResourceBindings(composite_bindings_);
    composite_pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    composite_pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
    composite_pipeline_->setCullMode(QRhiGraphicsPipeline::None);
    composite_pipeline_->create();
}

} // namespace kaos::reactor
