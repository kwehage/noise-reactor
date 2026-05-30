#include "noise_reactor/app.h"

#include "noise_reactor/audio_analyzer.h"
#include "noise_reactor/audio_file.h"
#include "noise_reactor/audio_frame_data.h"
#include "noise_reactor/preview_widget.h"
#include "noise_reactor/waveform_widget.h"

#include <QAction>
#include <QAudioOutput>
#include <QApplication>
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

namespace noise_reactor {

App::App(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Noise Reactor");
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
    statusBar()->setStyleSheet("QStatusBar { border: none; }");
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

    // Parameter panel
    auto* parameter_widget = new QWidget();
    parameter_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    {
        auto* parameter_layout = new QVBoxLayout(parameter_widget);
        parameter_layout->setContentsMargins(10, 10, 10, 10);
        parameter_layout->setSpacing(6);
        parameter_layout->setAlignment(Qt::AlignTop);

        auto* heading_label = new QLabel("Effects");
        heading_label->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: bold; text-transform: uppercase;");
        parameter_layout->addWidget(heading_label);

        struct EffectSpec {
            const char*           name;
            float EffectParams::* field;  // null = not yet implemented
        };
        const EffectSpec specs[] = {
            {"Zoom Pulse",   &EffectParams::zoom_intensity},
            {"Wave Warp",    &EffectParams::wave_intensity},
            {"Displacement", &EffectParams::displacement_intensity},
            {"Perlin Warp",  &EffectParams::perlin_intensity},
            {"Hue Shift",    &EffectParams::hue_shift_intensity},
            {"Glow",         &EffectParams::glow_intensity},
            {"Glitch",       &EffectParams::glitch_intensity},
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
            add_row("Smoothing", 30);

            connect(checkbox, &QCheckBox::toggled, rows_widget, &QWidget::setVisible);

            auto field = spec.field;
            if (field) {
                connect(checkbox, &QCheckBox::toggled, this,
                        [this, intensity_slider, field](bool checked) {
                            effect_params_.*field = checked
                                ? intensity_slider->value() / 100.f : 0.f;
                            update_time_label(scrubber_->value());
                        });
                connect(intensity_slider, &QSlider::valueChanged, this,
                        [this, checkbox, intensity_slider, field](int) {
                            if (checkbox->isChecked()) {
                                effect_params_.*field = intensity_slider->value() / 100.f;
                                update_time_label(scrubber_->value());
                            }
                        });
            }

            parameter_layout->addWidget(checkbox);
            parameter_layout->addWidget(rows_widget);
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
    timeline_widget->setFixedHeight(102);
    timeline_widget->setStyleSheet("background-color: #161616;");
    {
        auto* timeline_layout = new QVBoxLayout(timeline_widget);
        timeline_layout->setContentsMargins(8, 4, 8, 4);
        timeline_layout->setSpacing(4);

        waveform_widget_ = new WaveformWidget();
        waveform_widget_->setFixedHeight(52);
        timeline_layout->addWidget(waveform_widget_);

        connect(waveform_widget_, &WaveformWidget::seek_requested, this, [this](int ms) {
            scrubber_->setValue(ms);
            media_player_->setPosition(static_cast<qint64>(ms));
        });

        auto* transport_row = new QHBoxLayout();
        transport_row->setSpacing(6);

        play_button_ = new QPushButton("▶");
        play_button_->setFixedSize(26, 26);
        play_button_->setStyleSheet(
            "QPushButton { background: #2e2e2e; border: none; border-radius: 4px; color: #ddd; }"
            "QPushButton:hover { background: #3a3a3a; }");

        scrubber_ = new QSlider(Qt::Horizontal);
        scrubber_->setRange(0, 1000);
        scrubber_->setValue(0);

        time_label_ = new QLabel("0:00 / 0:00");
        time_label_->setFixedWidth(88);
        time_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        time_label_->setStyleSheet("color: #666; font-size: 11px;");

        connect(scrubber_, &QSlider::valueChanged, this, &App::update_time_label);
        connect(scrubber_, &QSlider::sliderMoved, this, [this](int ms) {
            media_player_->setPosition(static_cast<qint64>(ms));
        });
        connect(media_player_, &QMediaPlayer::positionChanged, this, [this](qint64 pos_ms) {
            if (!scrubber_->isSliderDown())
                scrubber_->setValue(static_cast<int>(pos_ms));
        });
        connect(play_button_, &QPushButton::clicked, this, &App::toggle_playback);

        transport_row->addWidget(play_button_);
        transport_row->addWidget(scrubber_, 1);
        transport_row->addWidget(time_label_);
        timeline_layout->addLayout(transport_row);
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

    const int duration_ms = static_cast<int>(analysis_.duration * 1000.f);
    scrubber_->setRange(0, duration_ms);
    scrubber_->setValue(0);
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
        preview_widget_->set_frame_data(
            analysis_.frames[analysis_.frame_for_time(ms / 1000.f)], effect_params_);
        waveform_widget_->set_position(float(ms) / (analysis_.duration * 1000.f));
    } else {
        preview_widget_->set_frame_data(FrameData{}, effect_params_);
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
    QFileDialog::getSaveFileName(this, "Export Video", "output.mp4",
        "Video Files (*.mp4)", nullptr, QFileDialog::DontUseNativeDialog);
}

} // namespace noise_reactor
