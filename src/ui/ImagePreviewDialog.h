#pragma once

#include <QDialog>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QImage>
#include <QTimer>
#include <QTemporaryDir>
#include "../core/ProcessRunner.h"

namespace uc::ui {

class ImagePreviewDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ImagePreviewDialog(const QString &sourceImagePath, const QString &targetFormatId,
                                QWidget *parent = nullptr);

    int quality() const { return m_quality; }

  private slots:
    void onQualitySliderChanged(int value);
    void updatePreview();

  private:
    void setupUi();
    void renderImages();
    QTimer m_debounce;
    QTemporaryDir m_temp;
    std::unique_ptr<core::ProcessRunner> m_runner;
    int m_generation{0};
    int m_zoom{100};
    QImage m_compressedImage;

    QString m_sourcePath;
    QString m_targetFormatId;
    int m_quality{85};

    QLabel *m_originalImgLabel{nullptr};
    QLabel *m_previewImgLabel{nullptr};
    QLabel *m_originalInfoLabel{nullptr};
    QLabel *m_previewInfoLabel{nullptr};
    QSlider *m_qualitySlider{nullptr};
    QLabel *m_qualityValueLabel{nullptr};

    QSize m_sourceResolution;
    QImage m_sourceImage;
    QImage m_previewBaseImage;
    qint64 m_sourceSizeBytes{0};
};

} // namespace uc::ui
