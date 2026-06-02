#pragma once

#include "kaos_reactor/audio_analysis.h"
#include "kaos_reactor/effect_params.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QAudioOutput)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QFrame)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QMediaPlayer)
QT_FORWARD_DECLARE_CLASS(QProcess)
QT_FORWARD_DECLARE_CLASS(QProgressDialog)
QT_FORWARD_DECLARE_CLASS(QPushButton)

namespace kaos::reactor {

class CinematicTargetWidget;
class PreviewWidget;
class WaveformWidget;

class App : public QMainWindow {
    Q_OBJECT

public:
    explicit App(QWidget* parent = nullptr);

    void load_image(const QString& path);
    void load_audio(const QString& path);

private slots:
    void open_audio();
    void open_image();
    void export_video();
    void export_tick();
    void on_analysis_done();
    void toggle_playback();

private:
    void build_menu();
    void build_layout();
    void update_time_label(int ms);
    void export_cleanup();

    PreviewWidget*  preview_widget_{nullptr};
    WaveformWidget* waveform_widget_{nullptr};

    QMediaPlayer* media_player_{nullptr};
    QAudioOutput* audio_output_{nullptr};

    QLabel*      time_label_{nullptr};
    QPushButton* play_button_{nullptr};

    QComboBox*    export_resolution_combo_{nullptr};
    QButtonGroup* export_fps_group_{nullptr};
    QButtonGroup* export_quality_group_{nullptr};
    QButtonGroup* export_audio_group_{nullptr};
    QFrame*       aspect_display_{nullptr};
    QLabel*       aspect_ratio_label_{nullptr};

    QFutureWatcher<AudioAnalysis>* analysis_watcher_{nullptr};
    AudioAnalysis analysis_;
    QString       audio_path_;
    EffectParams  effect_params_;

    CinematicTargetWidget* cinematic_target_widget_{nullptr};
    bool  cinematic_enabled_{false};
    bool  cinematic_reversed_{false};
    float cinematic_target_x_{0.5f};
    float cinematic_target_y_{0.5f};
    float cinematic_zoom_amount_{0.5f};

    QProcess*        export_ffmpeg_{nullptr};
    QProgressDialog* export_progress_{nullptr};
    int              export_frame_{0};
    int              export_total_frames_{0};
    int              export_fps_{0};
    bool             export_was_playing_{false};
    QString          export_output_path_;
};

} // namespace kaos::reactor
