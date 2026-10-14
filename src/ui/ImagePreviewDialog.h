#pragma once

#include <QDialog>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QImage>

namespace uc::ui {

class ImagePreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImagePreviewDialog(const QString& sourceImagePath,
                                const QString& targetFormatId,
                                QWidget* parent = nullptr);

    int quality() const { return m_quality; }

private slots:
    void onQualitySliderChanged(int value);
    void updatePreview();

private:
    void setupUi();

    QString m_sourcePath;
    QString m_targetFormatId;
    int m_quality{85};

    QLabel* m_originalImgLabel{nullptr};
    QLabel* m_previewImgLabel{nullptr};
    QLabel* m_originalInfoLabel{nullptr};
    QLabel* m_previewInfoLabel{nullptr};
    QSlider* m_qualitySlider{nullptr};
    QLabel* m_qualityValueLabel{nullptr};

    QImage m_sourceImage;
    qint64 m_sourceSizeBytes{0};
};

} // namespace uc::ui
