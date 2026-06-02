#include "kaos/reactor/app.h"

#include "kaos/reactor/cinematic_target_widget.h"
#include "kaos/reactor/audio_analyzer.h"
#include "kaos/reactor/audio_file.h"
#include "kaos/reactor/audio_frame_data.h"
#include "kaos/reactor/preview_widget.h"
#include "kaos/reactor/waveform_widget.h"

#include <QAction>
#include <QAudioOutput>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QRadioButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QMediaPlayer>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <numeric>

namespace kaos::reactor {

App::App(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("kaos::reactor");
    setMinimumSize(1280, 720);

    analysis_watcher_ = new QFutureWatcher<AudioAnalysis>(this);
    connect(analysis_watcher_, &QFutureWatcher<AudioAnalysis>::finished,
            this, &App::on_analysis_done);

    media_player_ = new QMediaPlayer(this);
    audio_output_ = new QAudioOutput(this);
    media_player_->setAudioOutput(audio_output_);
    connect(media_player_, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
                play_button_->setText(
                    state == QMediaPlayer::PlayingState ? "⏸" : "▶");
            });

    build_menu();
    build_layout();

    time_label_ = new QLabel("0:00 / 0:00");
    time_label_->setStyleSheet("color: #666; font-size: 11px; padding-right: 4px;");
    connect(media_player_, &QMediaPlayer::positionChanged, this, [this](qint64 pos_ms) {
        update_time_label(static_cast<int>(pos_ms));
    });

    statusBar()->setStyleSheet("QStatusBar { border: none; }");
    statusBar()->addPermanentWidget(time_label_);
    statusBar()->showMessage("Ready");
}

void App::build_menu() {
    auto* file_menu = menuBar()->addMenu("&File");

    auto* open_audio_action = file_menu->addAction("Open Audio...");
    connect(open_audio_action, &QAction::triggered, this, &App::open_audio);

    auto* open_image_action = file_menu->addAction("Open Image...");
    connect(open_image_action, &QAction::triggered, this, &App::open_image);

    file_menu->addSeparator();

    auto* export_action = file_menu->addAction("Export Video...");
    connect(export_action, &QAction::triggered, this, &App::export_video);

    file_menu->addSeparator();
    file_menu->addAction("Quit", qApp, &QApplication::quit);
}

void App::build_layout() {
    auto* central_widget = new QWidget(this);
    setCentralWidget(central_widget);

    auto* root_layout = new QVBoxLayout(central_widget);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    // ── Main area: Vulkan preview + parameter panel ───────────────────────
    auto* main_splitter = new QSplitter(Qt::Horizontal, central_widget);
    main_splitter->setHandleWidth(1);

    preview_widget_ = new PreviewWidget(main_splitter);
    preview_widget_->setMinimumSize(640, 360);
    preview_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(preview_widget_, &PreviewWidget::image_dropped, this, &App::load_image);
    connect(preview_widget_, &PreviewWidget::audio_dropped, this, &App::load_audio);

    // Parameter panel
    auto* parameter_widget = new QWidget();
    parameter_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    {
        auto* parameter_layout = new QVBoxLayout(parameter_widget);
        parameter_layout->setContentsMargins(10, 10, 14, 10);
        parameter_layout->setSpacing(6);
        parameter_layout->setAlignment(Qt::AlignTop);

        auto* heading_label = new QLabel("Effects");
        heading_label->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: bold; text-transform: uppercase;");
        parameter_layout->addWidget(heading_label);

        struct EffectSpec {
            const char*           name;
            float EffectParams::* field;
            float EffectParams::* feedback_field;  // null = no feedback slider
        };
        const EffectSpec specs[] = {
            {"Wave Warp",         &EffectParams::wave_intensity,         &EffectParams::wave_feedback},
            {"Displacement Warp", &EffectParams::displacement_intensity, &EffectParams::displacement_feedback},
            {"Perlin Warp",       &EffectParams::perlin_intensity,       &EffectParams::perlin_feedback},
            {"Barrel Lens",       &EffectParams::barrel_intensity,       &EffectParams::barrel_feedback},
            {"Ripple",            &EffectParams::ripple_intensity,       &EffectParams::ripple_feedback},
            {"Polar Warp",        &EffectParams::polar_intensity,        &EffectParams::polar_feedback},
            {"Kaleidoscope",      &EffectParams::kaleidoscope_intensity, &EffectParams::kaleidoscope_feedback},
            {"Pixelate",          &EffectParams::pixelate_intensity,     &EffectParams::pixelate_feedback},
            {"Zoom Pulse",        &EffectParams::zoom_intensity,         &EffectParams::zoom_feedback},
            {"Hue Shift",         &EffectParams::hue_shift_intensity,    &EffectParams::hue_shift_feedback},
            {"Saturation",        &EffectParams::saturation_intensity,   &EffectParams::saturation_feedback},
            {"Brightness",        &EffectParams::brightness_intensity,   &EffectParams::brightness_feedback},
            {"Posterize",         &EffectParams::posterize_intensity,    &EffectParams::posterize_feedback},
            {"Solarize",          &EffectParams::solarize_intensity,     &EffectParams::solarize_feedback},
            {"Duotone",           &EffectParams::duotone_intensity,      &EffectParams::duotone_feedback},
            {"Channel Swap",      &EffectParams::channel_swap_intensity, &EffectParams::channel_swap_feedback},
            {"Glow",              &EffectParams::glow_intensity,         &EffectParams::glow_feedback},
            {"Edge Glow",         &EffectParams::edge_glow_intensity,    &EffectParams::edge_glow_feedback},
            {"Emboss",            &EffectParams::emboss_intensity,       &EffectParams::emboss_feedback},
            {"Erode / Dilate",    &EffectParams::morph_intensity,        &EffectParams::morph_feedback},
            {"Scanlines",         &EffectParams::scanline_intensity,     &EffectParams::scanline_feedback},
            {"Vignette",          &EffectParams::vignette_intensity,     &EffectParams::vignette_feedback},
            {"Chroma Split",      &EffectParams::chromatic_intensity,    &EffectParams::chromatic_feedback},
            {"Film Grain",        &EffectParams::film_grain_intensity,   &EffectParams::film_grain_feedback},
            {"Glitch",            &EffectParams::glitch_intensity,       &EffectParams::glitch_feedback},
            {"Trails",            &EffectParams::feedback_intensity,     nullptr},
        };

        const char* slider_style =
            "QSlider::groove:horizontal {"
            "  background: #2a2a2a; height: 3px; border-radius: 1px; }"
            "QSlider::sub-page:horizontal {"
            "  background: #777; height: 3px; border-radius: 1px; }"
            "QSlider::handle:horizontal {"
            "  background: #999; width: 10px; height: 10px;"
            "  margin: -4px 0; border-radius: 5px; }"
            "QSlider::handle:horizontal:hover { background: #bbb; }";

        {
            auto* row = new QHBoxLayout();
            auto* label = new QLabel("Warp Scale");
            label->setFixedWidth(70);
            label->setStyleSheet("color: #aaa; font-size: 11px;");
            auto* slider = new QSlider(Qt::Horizontal);
            slider->setRange(10, 400);
            slider->setValue(100);
            slider->setStyleSheet(slider_style);
            row->addWidget(label);
            row->addWidget(slider);
            parameter_layout->addLayout(row);
            connect(slider, &QSlider::valueChanged, this, [this](int v) {
                effect_params_.warp_scale = v / 100.f;
                update_time_label(static_cast<int>(media_player_->position()));
            });
        }

        for (const auto& spec : specs) {
            // A plain QCheckBox header + collapsible QWidget avoids the empty
            // frame that QGroupBox draws regardless of flat/margin settings.
            auto* checkbox = new QCheckBox(spec.name);
            checkbox->setChecked(false);
            checkbox->setStyleSheet(
                "QCheckBox { color: #ccc; font-size: 11px; padding: 2px 0; }"
                "QCheckBox::indicator { width: 13px; height: 13px; }");

            auto* rows_widget = new QWidget();
            rows_widget->setVisible(false);
            auto* rows_layout = new QVBoxLayout(rows_widget);
            rows_layout->setContentsMargins(8, 2, 4, 4);
            rows_layout->setSpacing(4);

            auto add_row = [&](const char* label_text, int default_value) -> QSlider* {
                auto* row_layout = new QHBoxLayout();
                auto* label = new QLabel(label_text);
                label->setFixedWidth(70);
                label->setStyleSheet("color: #aaa; font-size: 11px;");
                auto* slider = new QSlider(Qt::Horizontal);
                slider->setRange(0, 100);
                slider->setValue(default_value);
                slider->setStyleSheet(slider_style);
                row_layout->addWidget(label);
                row_layout->addWidget(slider);
                rows_layout->addLayout(row_layout);
                return slider;
            };

            auto* intensity_slider = add_row("Intensity", 50);
            QSlider* feedback_slider = nullptr;
            if (spec.feedback_field)
                feedback_slider = add_row("Feedback", 0);

            connect(checkbox, &QCheckBox::toggled, rows_widget, &QWidget::setVisible);

            auto field = spec.field;
            if (field) {
                connect(checkbox, &QCheckBox::toggled, this,
                        [this, intensity_slider, field](bool checked) {
                            effect_params_.*field = checked
                                ? intensity_slider->value() / 100.f : 0.f;
                            update_time_label(static_cast<int>(media_player_->position()));
                        });
                connect(intensity_slider, &QSlider::valueChanged, this,
                        [this, checkbox, intensity_slider, field](int) {
                            if (checkbox->isChecked()) {
                                effect_params_.*field = intensity_slider->value() / 100.f;
                                update_time_label(static_cast<int>(media_player_->position()));
                            }
                        });
            }
            auto fb_field = spec.feedback_field;
            if (fb_field && feedback_slider) {
                connect(feedback_slider, &QSlider::valueChanged, this,
                        [this, feedback_slider, fb_field](int) {
                            effect_params_.*fb_field = feedback_slider->value() / 100.f;
                            update_time_label(static_cast<int>(media_player_->position()));
                        });
            }

            parameter_layout->addWidget(checkbox);
            parameter_layout->addWidget(rows_widget);
        }

        // ── Cinematic Zoom ────────────────────────────────────────────────────
        {
            auto* cin_checkbox = new QCheckBox("Cinematic Zoom");
            cin_checkbox->setChecked(false);
            cin_checkbox->setStyleSheet(
                "QCheckBox { color: #ccc; font-size: 11px; padding: 2px 0; }"
                "QCheckBox::indicator { width: 13px; height: 13px; }");

            auto* cin_rows = new QWidget();
            cin_rows->setVisible(false);
            auto* cin_layout = new QVBoxLayout(cin_rows);
            cin_layout->setContentsMargins(8, 2, 4, 4);
            cin_layout->setSpacing(6);

            cinematic_target_widget_ = new CinematicTargetWidget();
            cin_layout->addWidget(cinematic_target_widget_);

            auto* zoom_row = new QHBoxLayout();
            auto* zoom_label = new QLabel("Zoom");
            zoom_label->setFixedWidth(70);
            zoom_label->setStyleSheet("color: #aaa; font-size: 11px;");
            auto* zoom_slider = new QSlider(Qt::Horizontal);
            zoom_slider->setRange(0, 100);
            zoom_slider->setValue(50);
            zoom_slider->setStyleSheet(slider_style);
            zoom_row->addWidget(zoom_label);
            zoom_row->addWidget(zoom_slider);
            cin_layout->addLayout(zoom_row);

            connect(cin_checkbox, &QCheckBox::toggled, cin_rows, &QWidget::setVisible);
            auto* reverse_checkbox = new QCheckBox("Reverse");
            reverse_checkbox->setChecked(false);
            reverse_checkbox->setStyleSheet(
                "QCheckBox { color: #aaa; font-size: 11px; padding: 2px 0; }"
                "QCheckBox::indicator { width: 13px; height: 13px; }");
            cin_layout->addWidget(reverse_checkbox);

            connect(cin_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
                cinematic_enabled_ = checked;
                update_time_label(static_cast<int>(media_player_->position()));
            });
            connect(reverse_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
                cinematic_reversed_ = checked;
                update_time_label(static_cast<int>(media_player_->position()));
            });
            connect(zoom_slider, &QSlider::valueChanged, this, [this](int v) {
                cinematic_zoom_amount_ = v / 100.f;
                update_time_label(static_cast<int>(media_player_->position()));
            });
            connect(cinematic_target_widget_, &CinematicTargetWidget::target_changed,
                    this, [this](QPointF pos) {
                        cinematic_target_x_ = static_cast<float>(pos.x());
                        cinematic_target_y_ = static_cast<float>(pos.y());
                        update_time_label(static_cast<int>(media_player_->position()));
                    });

            parameter_layout->addWidget(cin_checkbox);
            parameter_layout->addWidget(cin_rows);
        }

        parameter_layout->addSpacing(12);

        // ── Export ────────────────────────────────────────────────────────────
        auto* export_heading = new QLabel("Export");
        export_heading->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: bold; text-transform: uppercase;");
        parameter_layout->addWidget(export_heading);

        export_resolution_combo_ = new QComboBox();
        auto* res_model = new QStandardItemModel(this);

        auto add_res_header = [&](const QString& text) {
            auto* item = new QStandardItem("  " + text);
            item->setEnabled(false);
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            item->setForeground(QColor("#666"));
            res_model->appendRow(item);
        };
        auto add_res_preset = [&](const QString& label, int w, int h) {
            auto* item = new QStandardItem(
                QString("    %1  (%2 × %3)").arg(label).arg(w).arg(h));
            item->setData(QSize(w, h), Qt::UserRole);
            res_model->appendRow(item);
        };

        add_res_header("YouTube / Web");
        add_res_preset("720p HD",       1280, 720);
        add_res_preset("1080p Full HD", 1920, 1080);
        add_res_preset("2K QHD",        2560, 1440);
        add_res_preset("4K UHD",        3840, 2160);
        add_res_header("Instagram Feed");
        add_res_preset("Square (1:1)",   1080, 1080);
        add_res_preset("Portrait (4:5)", 1080, 1350);
        add_res_header("Reels / TikTok / Shorts");
        add_res_preset("Vertical (9:16)", 1080, 1920);

        export_resolution_combo_->setModel(res_model);
        export_resolution_combo_->setCurrentIndex(2);
        export_resolution_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        export_resolution_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        parameter_layout->addWidget(export_resolution_combo_);

        // Aspect ratio visual display
        auto* aspect_box_container = new QWidget();
        aspect_box_container->setFixedHeight(80);
        auto* aspect_box_hl = new QHBoxLayout(aspect_box_container);
        aspect_box_hl->setAlignment(Qt::AlignCenter);
        aspect_box_hl->setContentsMargins(0, 0, 0, 0);
        aspect_display_ = new QFrame();
        aspect_display_->setStyleSheet("QFrame { border: 1px solid #444; background: #1a1a1a; }");
        aspect_box_hl->addWidget(aspect_display_);
        parameter_layout->addWidget(aspect_box_container);

        aspect_ratio_label_ = new QLabel();
        aspect_ratio_label_->setAlignment(Qt::AlignCenter);
        aspect_ratio_label_->setStyleSheet("color: #666; font-size: 10px;");
        parameter_layout->addWidget(aspect_ratio_label_);

        // FPS
        auto* fps_heading = new QLabel("Frame Rate");
        fps_heading->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: bold; text-transform: uppercase;");
        parameter_layout->addWidget(fps_heading);

        auto* fps_row_widget = new QWidget();
        auto* fps_row = new QHBoxLayout(fps_row_widget);
        fps_row->setContentsMargins(0, 0, 0, 0);
        fps_row->setSpacing(6);
        export_fps_group_ = new QButtonGroup(this);
        for (int fps : {24, 30, 60}) {
            auto* rb = new QRadioButton(QString("%1 fps").arg(fps));
            rb->setStyleSheet("QRadioButton { color: #ccc; font-size: 11px; }");
            export_fps_group_->addButton(rb, fps);
            fps_row->addWidget(rb);
            if (fps == 30) rb->setChecked(true);
        }
        fps_row->addStretch();
        parameter_layout->addWidget(fps_row_widget);

        auto* quality_heading = new QLabel("Quality");
        quality_heading->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: bold; text-transform: uppercase;");
        parameter_layout->addWidget(quality_heading);

        auto* quality_row_widget = new QWidget();
        auto* quality_row = new QHBoxLayout(quality_row_widget);
        quality_row->setContentsMargins(0, 0, 0, 0);
        quality_row->setSpacing(6);
        export_quality_group_ = new QButtonGroup(this);
        for (int crf : {18, 23, 28}) {
            const QString label = crf == 18 ? "High" : crf == 23 ? "Medium" : "Low";
            auto* rb = new QRadioButton(label);
            rb->setStyleSheet("QRadioButton { color: #ccc; font-size: 11px; }");
            export_quality_group_->addButton(rb, crf);
            quality_row->addWidget(rb);
            if (crf == 23) rb->setChecked(true);
        }
        quality_row->addStretch();
        parameter_layout->addWidget(quality_row_widget);

        auto* audio_heading = new QLabel("Audio Bitrate");
        audio_heading->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: bold; text-transform: uppercase;");
        parameter_layout->addWidget(audio_heading);

        auto* audio_row_widget = new QWidget();
        auto* audio_row = new QHBoxLayout(audio_row_widget);
        audio_row->setContentsMargins(0, 0, 0, 0);
        audio_row->setSpacing(6);
        export_audio_group_ = new QButtonGroup(this);
        for (int kbps : {192, 128, 96}) {
            auto* rb = new QRadioButton(QString("%1k").arg(kbps));
            rb->setStyleSheet("QRadioButton { color: #ccc; font-size: 11px; }");
            export_audio_group_->addButton(rb, kbps);
            audio_row->addWidget(rb);
            if (kbps == 192) rb->setChecked(true);
        }
        audio_row->addStretch();
        parameter_layout->addWidget(audio_row_widget);

        auto update_aspect_display = [this](int index) {
            auto* m = qobject_cast<QStandardItemModel*>(export_resolution_combo_->model());
            QSize size = m->item(index)->data(Qt::UserRole).toSize();
            if (!size.isValid()) return;

            const int max_w = 220, max_h = 72;
            float scale = std::min(float(max_w) / size.width(), float(max_h) / size.height());
            int w = std::max(20, int(size.width() * scale));
            int h = std::max(10, int(size.height() * scale));
            aspect_display_->setFixedSize(w, h);

            int g = std::gcd(size.width(), size.height());
            aspect_ratio_label_->setText(
                QString("%1 × %2  (%3:%4)")
                    .arg(size.width()).arg(size.height())
                    .arg(size.width() / g).arg(size.height() / g));

            preview_widget_->set_export_resolution(size);
        };

        connect(export_resolution_combo_, &QComboBox::currentIndexChanged,
                this, update_aspect_display);
        connect(export_resolution_combo_, &QComboBox::currentIndexChanged,
                this, [this](int index) {
                    auto* m = qobject_cast<QStandardItemModel*>(export_resolution_combo_->model());
                    QSize size = m->item(index)->data(Qt::UserRole).toSize();
                    if (size.isValid() && cinematic_target_widget_)
                        cinematic_target_widget_->set_aspect_ratio(
                            float(size.width()) / float(size.height()));
                });
        update_aspect_display(export_resolution_combo_->currentIndex());
        {
            auto* m = qobject_cast<QStandardItemModel*>(export_resolution_combo_->model());
            QSize size = m->item(export_resolution_combo_->currentIndex())->data(Qt::UserRole).toSize();
            if (size.isValid() && cinematic_target_widget_)
                cinematic_target_widget_->set_aspect_ratio(float(size.width()) / float(size.height()));
        }

        parameter_layout->addStretch();
    }

    auto* parameter_scroll = new QScrollArea(main_splitter);
    parameter_scroll->setWidget(parameter_widget);
    parameter_scroll->setWidgetResizable(true);
    parameter_scroll->setFixedWidth(260);
    parameter_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    parameter_scroll->setStyleSheet(
        "QScrollArea { border: none; border-left: 1px solid #2a2a2a; }");

    main_splitter->addWidget(preview_widget_);
    main_splitter->addWidget(parameter_scroll);
    main_splitter->setStretchFactor(0, 1);
    main_splitter->setStretchFactor(1, 0);

    root_layout->addWidget(main_splitter, 1);

    // ── Timeline ──────────────────────────────────────────────────────────
    auto* timeline_widget = new QWidget(central_widget);
    timeline_widget->setFixedHeight(90);
    timeline_widget->setStyleSheet("background-color: #161616;");
    {
        auto* timeline_layout = new QVBoxLayout(timeline_widget);
        timeline_layout->setContentsMargins(8, 4, 8, 4);
        timeline_layout->setSpacing(4);

        waveform_widget_ = new WaveformWidget();
        waveform_widget_->setFixedHeight(52);
        timeline_layout->addWidget(waveform_widget_);

        connect(waveform_widget_, &WaveformWidget::seek_requested, this, [this](int ms) {
            media_player_->setPosition(static_cast<qint64>(ms));
            update_time_label(ms);
        });

        play_button_ = new QPushButton("▶");
        play_button_->setFixedSize(26, 26);
        play_button_->setStyleSheet(
            "QPushButton { background: #2e2e2e; border: none; border-radius: 4px; color: #ddd; }"
            "QPushButton:hover { background: #3a3a3a; }");
        connect(play_button_, &QPushButton::clicked, this, &App::toggle_playback);

        auto* play_row = new QHBoxLayout();
        play_row->addStretch();
        play_row->addWidget(play_button_);
        play_row->addStretch();
        timeline_layout->addLayout(play_row);
    }

    root_layout->addWidget(timeline_widget, 0);
}

void App::open_audio() {
    QString path = QFileDialog::getOpenFileName(this, "Open Audio", {},
        "Audio Files (*.wav *.flac *.mp3 *.ogg *.aiff *.aif)",
        nullptr, QFileDialog::DontUseNativeDialog);
    if (!path.isEmpty())
        load_audio(path);
}

void App::load_audio(const QString& path) {
    if (analysis_watcher_->isRunning()) {
        statusBar()->showMessage("Analysis already in progress...");
        return;
    }

    audio_path_ = path;
    media_player_->setSource(QUrl::fromLocalFile(path));
    statusBar()->showMessage("Analyzing: " + path + "...");

    auto future = QtConcurrent::run([p = path.toStdString()]() -> AudioAnalysis {
        auto file = AudioFile::load(p);
        if (!file)
            return {};
        return AudioAnalyzer::analyze(*file);
    });

    analysis_watcher_->setFuture(future);
}

void App::on_analysis_done() {
    analysis_ = analysis_watcher_->result();
    if (analysis_.empty()) {
        statusBar()->showMessage("Failed to load audio: " + audio_path_);
        return;
    }

    waveform_widget_->set_analysis(analysis_);

    int beats = 0, onsets = 0;
    for (const auto& frame : analysis_.frames) {
        if (frame.beat)  ++beats;
        if (frame.onset) ++onsets;
    }

    statusBar()->showMessage(
        QString("Loaded %1 frames @ %2 fps — %3 beats, %4 onsets")
            .arg(analysis_.frames.size())
            .arg(analysis_.fps)
            .arg(beats)
            .arg(onsets));
}

void App::update_time_label(int ms) {
    auto format_time = [](int total_ms) -> QString {
        return QString("%1:%2")
            .arg(total_ms / 60000)
            .arg((total_ms % 60000) / 1000, 2, 10, QChar('0'));
    };
    const int total_ms = static_cast<int>(analysis_.duration * 1000.f);
    time_label_->setText(format_time(ms) + " / " + format_time(total_ms));

    if (!analysis_.empty()) {
        const float fade_duration_ms = 500.f;
        const float total_ms_f       = float(total_ms);
        float fade = 1.0f;
        if (float(ms) < fade_duration_ms)
            fade = float(ms) / fade_duration_ms;
        else if (float(ms) > total_ms_f - fade_duration_ms)
            fade = (total_ms_f - float(ms)) / fade_duration_ms;
        fade = std::clamp(fade, 0.f, 1.f);

        EffectParams faded = effect_params_;
        faded.zoom_intensity         *= fade;
        faded.wave_intensity         *= fade;
        faded.hue_shift_intensity    *= fade;
        faded.glitch_intensity       *= fade;
        faded.glow_intensity         *= fade;
        faded.displacement_intensity *= fade;
        faded.perlin_intensity       *= fade;

        preview_widget_->set_frame_data(
            analysis_.frames[analysis_.frame_for_time(ms / 1000.f)], faded);
        waveform_widget_->set_position(float(ms) / (analysis_.duration * 1000.f));
    } else {
        preview_widget_->set_frame_data(FrameData{}, effect_params_);
    }

    // Cinematic zoom — purely time-based, independent of audio features.
    // Interpolates from identity (t=0) to the user-selected target (t=1).
    if (cinematic_enabled_ && analysis_.duration > 0.f) {
        const float t    = std::clamp(float(ms) / (analysis_.duration * 1000.f), 0.f, 1.f);
        const float prog = cinematic_reversed_ ? 1.f - t : t;
        const float zoom = 1.f + cinematic_zoom_amount_ * prog;
        const float px   = (0.5f - cinematic_target_x_) * (zoom - 1.f) / zoom;
        const float py   = (0.5f - cinematic_target_y_) * (zoom - 1.f) / zoom;
        preview_widget_->set_cinematic(zoom, px, py);
    } else {
        preview_widget_->set_cinematic(1.f, 0.f, 0.f);
    }
}

void App::open_image() {
    QFileDialog dialog(this, "Open Image", {},
        "Image Files (*.png *.jpg *.jpeg *.bmp *.tiff *.tif)");
    dialog.setOption(QFileDialog::DontUseNativeDialog);

    auto* preview_label = new QLabel(&dialog);
    preview_label->setFixedSize(200, 200);
    preview_label->setAlignment(Qt::AlignCenter);
    preview_label->setStyleSheet("background: #1a1a1a; border: 1px solid #2a2a2a; color: #555;");
    preview_label->setText("No preview");

    if (auto* grid = qobject_cast<QGridLayout*>(dialog.layout()))
        grid->addWidget(preview_label, 0, grid->columnCount(), grid->rowCount(), 1);

    connect(&dialog, &QFileDialog::currentChanged, &dialog,
            [preview_label](const QString& path) {
                QPixmap px(path);
                if (px.isNull()) {
                    preview_label->setPixmap({});
                    preview_label->setText("No preview");
                } else {
                    preview_label->setText({});
                    preview_label->setPixmap(
                        px.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            });

    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty())
        load_image(dialog.selectedFiles().first());
}

void App::load_image(const QString& path) {
    QImage image(path);
    if (!image.isNull())
        preview_widget_->set_image(image);
}

void App::toggle_playback() {
    if (media_player_->playbackState() == QMediaPlayer::PlayingState)
        media_player_->pause();
    else
        media_player_->play();
}

void App::export_video() {
    if (analysis_.empty() || !preview_widget_->has_image()) {
        QMessageBox::warning(this, "Export",
            "Load both an image and an audio file before exporting.");
        return;
    }

    const QString ffmpeg_bin = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg_bin.isEmpty()) {
        QMessageBox::critical(this, "Export",
            "ffmpeg was not found in PATH. Install it to enable export.");
        return;
    }

    auto* res_model = qobject_cast<QStandardItemModel*>(export_resolution_combo_->model());
    QSize res = res_model->item(export_resolution_combo_->currentIndex())
                    ->data(Qt::UserRole).toSize();
    if (!res.isValid()) {
        QMessageBox::warning(this, "Export", "Select a valid resolution before exporting.");
        return;
    }
    const int fps     = export_fps_group_->checkedId();
    const int crf     = export_quality_group_->checkedId();
    const int audio_k = export_audio_group_->checkedId();

    QString output_path = QFileDialog::getSaveFileName(
        this, "Export Video", QDir::homePath() + "/output.mp4",
        "Video Files (*.mp4)", nullptr, QFileDialog::DontUseNativeDialog);
    if (output_path.isEmpty()) return;
    if (!output_path.endsWith(".mp4", Qt::CaseInsensitive))
        output_path += ".mp4";

    export_was_playing_ = media_player_->playbackState() == QMediaPlayer::PlayingState;
    if (export_was_playing_)
        media_player_->pause();

    export_frame_        = 0;
    export_total_frames_ = static_cast<int>(analysis_.duration * fps);
    export_fps_          = fps;
    export_output_path_  = output_path;

    preview_widget_->set_exporting(true);
    preview_widget_->setFixedColorBufferSize(res);

    export_ffmpeg_ = new QProcess(this);
    export_ffmpeg_->start(ffmpeg_bin, {
        "-y",
        "-f",           "rawvideo",
        "-pixel_format","rgba",
        "-video_size",  QString("%1x%2").arg(res.width()).arg(res.height()),
        "-framerate",   QString::number(fps),
        "-i",           "pipe:0",
        "-i",           audio_path_,
        "-map",         "0:v:0",
        "-map",         "1:a:0",
        "-vcodec",      "libx264",
        "-preset",      "slow",
        "-crf",         QString::number(crf),
        "-tune",        "animation",
        "-acodec",      "aac",
        "-b:a",         QString("%1k").arg(audio_k),
        "-pix_fmt",     "yuv420p",
        "-shortest",
        output_path,
    });

    if (!export_ffmpeg_->waitForStarted(5000)) {
        QMessageBox::critical(this, "Export", "Failed to start ffmpeg.");
        export_ffmpeg_->deleteLater();
        export_ffmpeg_ = nullptr;
        preview_widget_->set_exporting(false);
        preview_widget_->setFixedColorBufferSize(QSize());
        if (export_was_playing_)
            media_player_->play();
        return;
    }

    export_progress_ = new QProgressDialog("Exporting video…", "Cancel", 0, export_total_frames_, this);
    export_progress_->setWindowModality(Qt::WindowModal);
    export_progress_->setMinimumDuration(0);
    export_progress_->show();

    QTimer::singleShot(0, this, &App::export_tick);
}

void App::export_tick() {
    if (export_progress_->wasCanceled()) {
        export_ffmpeg_->kill();
        export_ffmpeg_->waitForFinished(5000);
        export_cleanup();
        return;
    }

    if (export_frame_ >= export_total_frames_) {
        export_progress_->setLabelText("Finalizing…");
        export_progress_->setMaximum(0);
        export_ffmpeg_->closeWriteChannel();
        connect(export_ffmpeg_, &QProcess::finished, this,
            [this](int exit_code, QProcess::ExitStatus) {
                const bool cancelled = export_progress_->wasCanceled();
                const QByteArray err = (!cancelled && exit_code != 0)
                    ? export_ffmpeg_->readAllStandardError() : QByteArray{};
                export_cleanup();
                if (!cancelled && exit_code == 0)
                    statusBar()->showMessage("Export complete: " + export_output_path_);
                else if (!cancelled)
                    QMessageBox::warning(this, "Export",
                        "ffmpeg exited with errors:\n" + err);
            }, Qt::SingleShotConnection);
        return;
    }

    const float time_s = float(export_frame_) / float(export_fps_);
    const auto& frame  = analysis_.frames[analysis_.frame_for_time(time_s)];

    // Fade effects in/out over the first and last 0.5s to mask the anomalous
    // spectral features produced by the zero-padded FFT windows at both ends.
    const int fade_frames = export_fps_ / 2;
    float fade = 1.0f;
    if (export_frame_ < fade_frames)
        fade = float(export_frame_) / float(fade_frames);
    else if (export_frame_ >= export_total_frames_ - fade_frames)
        fade = float(export_total_frames_ - export_frame_) / float(fade_frames);

    EffectParams faded = effect_params_;
    faded.zoom_intensity         *= fade;
    faded.wave_intensity         *= fade;
    faded.hue_shift_intensity    *= fade;
    faded.glitch_intensity       *= fade;
    faded.glow_intensity         *= fade;
    faded.displacement_intensity *= fade;
    faded.perlin_intensity       *= fade;

    preview_widget_->set_frame_data(frame, faded);

    // Cinematic zoom for this export frame — time-based, independent of audio.
    if (cinematic_enabled_ && analysis_.duration > 0.f) {
        const float t    = std::clamp(time_s / analysis_.duration, 0.f, 1.f);
        const float zoom = 1.f + cinematic_zoom_amount_ * t;
        const float px   = (0.5f - cinematic_target_x_) * (zoom - 1.f) / zoom;
        const float py   = (0.5f - cinematic_target_y_) * (zoom - 1.f) / zoom;
        preview_widget_->set_cinematic(zoom, px, py);
    } else {
        preview_widget_->set_cinematic(1.f, 0.f, 0.f);
    }

    preview_widget_->begin_grab();
    const QImage img = preview_widget_->grabFramebuffer();
    preview_widget_->end_grab();
    if (!img.isNull())
        export_ffmpeg_->write(reinterpret_cast<const char*>(img.constBits()),
                              static_cast<qint64>(img.sizeInBytes()));

    export_progress_->setValue(++export_frame_);
    QTimer::singleShot(0, this, &App::export_tick);
}

void App::export_cleanup() {
    preview_widget_->set_exporting(false);
    preview_widget_->setFixedColorBufferSize(QSize());

    if (export_was_playing_)
        media_player_->play();

    export_ffmpeg_->deleteLater();
    export_ffmpeg_ = nullptr;

    export_progress_->close();
    export_progress_->deleteLater();
    export_progress_ = nullptr;
}

} // namespace kaos::reactor
