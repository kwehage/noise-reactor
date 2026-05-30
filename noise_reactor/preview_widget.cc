#include "noise_reactor/preview_widget.h"

#include <QCoreApplication>
#include <QFile>
#include <QSizePolicy>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

namespace noise_reactor {

static QImage make_placeholder() {
    QImage img(1, 1, QImage::Format_RGBA8888);
    img.fill(QColor(18, 18, 18, 255));
    return img;
}

static QShader load_shader(const char* filename) {
    const QString path = QCoreApplication::applicationDirPath() + "/" + filename;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot open shader: %s", qPrintable(path));
    const QShader shader = QShader::fromSerialized(file.readAll());
    if (!shader.isValid())
        qFatal("Invalid shader: %s", qPrintable(path));
    return shader;
}

PreviewWidget::PreviewWidget(QWidget* parent) : QRhiWidget(parent) {
    setMinimumSize(640, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PreviewWidget::set_image(const QImage& image) {
    source_image_ = image.convertToFormat(QImage::Format_RGBA8888);
    // Defer backend-aware Y-flip to prepare_upload(); rhi_ may not exist yet.
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
}

// ── QRhiWidget lifecycle ──────────────────────────────────────────────────────

void PreviewWidget::prepare_upload() {
    // Y-flip: Vulkan NDC has Y-down so (0,0) is top-left — no flip needed.
    // OpenGL NDC has Y-up so (0,0) maps to bottom-left — flip required.
    if (!source_image_.isNull() && rhi_)
        pending_image_ = rhi_->isYUpInNDC()
            ? source_image_.flipped(Qt::Vertical)
            : source_image_;
    else
        pending_image_ = source_image_;
}

void PreviewWidget::initialize(QRhiCommandBuffer* cb) {
    rhi_ = rhi();
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

    rebuild_bindings();
    rebuild_pipeline();

    auto* batch = rhi_->nextResourceUpdateBatch();
    batch->uploadTexture(texture_, initial);
    batch->updateDynamicBuffer(uniform_buffer_, 0, sizeof(EffectUBO), &ubo_data_);
    cb->resourceUpdate(batch);

    image_dirty_ = false;
}

void PreviewWidget::render(QRhiCommandBuffer* cb) {
    time_ += 1.f / 60.f;
    ubo_data_.time = time_;

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

    const QSize rt_size = renderTarget()->pixelSize();
    cb->beginPass(renderTarget(), QColor(18, 18, 18), {1.0f, 0}, batch);

    cb->setGraphicsPipeline(pipeline_);
    cb->setViewport({0, 0, float(rt_size.width()), float(rt_size.height())});
    cb->setShaderResources(bindings_);
    cb->draw(6);

    cb->endPass();

    update();
}

void PreviewWidget::releaseResources() {
    cleanup();
    rhi_ = nullptr;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void PreviewWidget::cleanup() {
    delete pipeline_;       pipeline_       = nullptr;
    delete bindings_;       bindings_       = nullptr;
    delete uniform_buffer_; uniform_buffer_ = nullptr;
    delete sampler_;        sampler_        = nullptr;
    delete texture_;        texture_        = nullptr;
}

void PreviewWidget::rebuild_bindings() {
    delete bindings_;
    bindings_ = rhi_->newShaderResourceBindings();
    bindings_->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::FragmentStage, texture_, sampler_),
        QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::FragmentStage, uniform_buffer_),
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
    pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    pipeline_->setTopology(QRhiGraphicsPipeline::Triangles);
    pipeline_->setCullMode(QRhiGraphicsPipeline::None);
    pipeline_->create();
}

} // namespace noise_reactor
