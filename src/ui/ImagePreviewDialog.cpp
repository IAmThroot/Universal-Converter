#include "ImagePreviewDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QBuffer>
#include "../core/MediaEstimator.h"

namespace uc::ui {

ImagePreviewDialog::ImagePreviewDialog(const QString& sourceImagePath,
                                       const QString& targetFormatId,
                                       QWidget* parent)
    : QDialog(parent)
    , m_sourcePath(sourceImagePath)
    , m_targetFormatId(targetFormatId)
{
    setWindowTitle("Before & After Image Quality Preview");
    resize(780, 560);
    setStyleSheet("background-color: #0f172a; color: #f8fafc; font-size: 13px;");

    m_sourceImage.load(m_sourcePath);
    m_sourceSizeBytes = QFileInfo(m_sourcePath).size();

    setupUi();
    updatePreview();
}

void ImagePreviewDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(18, 18, 18, 18);

    auto* previewLayout = new QHBoxLayout();
    previewLayout->setSpacing(14);

    // Left: Original
    auto* leftBox = new QVBoxLayout();
    auto* leftTitle = new QLabel("Original Source", this);
    leftTitle->setStyleSheet("font-weight: bold; color: #94a3b8;");
    leftBox->addWidget(leftTitle);

    m_originalImgLabel = new QLabel(this);
    m_originalImgLabel->setFixedSize(360, 280);
    m_originalImgLabel->setAlignment(Qt::AlignCenter);
    m_originalImgLabel->setStyleSheet("background-color: #090d16; border: 1px solid #334155; border-radius: 6px;");
    leftBox->addWidget(m_originalImgLabel);

    m_originalInfoLabel = new QLabel(this);
    m_originalInfoLabel->setStyleSheet("color: #64748b; font-size: 12px;");
    m_originalInfoLabel->setText(QString("Resolution: %1x%2 | Size: %3")
                                     .arg(m_sourceImage.width())
                                     .arg(m_sourceImage.height())
                                     .arg(core::MediaEstimator::formatBytes(m_sourceSizeBytes)));
    leftBox->addWidget(m_originalInfoLabel);
    previewLayout->addLayout(leftBox);

    // Right: Target Preview
    auto* rightBox = new QVBoxLayout();
    auto* rightTitle = new QLabel(QString("Target Preview (%1)").arg(m_targetFormatId.toUpper()), this);
    rightTitle->setStyleSheet("font-weight: bold; color: #34d399;");
    rightBox->addWidget(rightTitle);

    m_previewImgLabel = new QLabel(this);
    m_previewImgLabel->setFixedSize(360, 280);
    m_previewImgLabel->setAlignment(Qt::AlignCenter);
    m_previewImgLabel->setStyleSheet("background-color: #090d16; border: 1px solid #334155; border-radius: 6px;");
    rightBox->addWidget(m_previewImgLabel);

    m_previewInfoLabel = new QLabel(this);
    m_previewInfoLabel->setStyleSheet("color: #34d399; font-size: 12px;");
    rightBox->addWidget(m_previewInfoLabel);
    previewLayout->addLayout(rightBox);

    layout->addLayout(previewLayout);

    // Quality slider controls
    auto* controlLayout = new QHBoxLayout();
    auto* qualityLabel = new QLabel("Compression Quality:", this);
    qualityLabel->setStyleSheet("font-weight: 600; color: #e2e8f0;");
    controlLayout->addWidget(qualityLabel);

    m_qualitySlider = new QSlider(Qt::Horizontal, this);
    m_qualitySlider->setRange(10, 100);
    m_qualitySlider->setValue(85);
    m_qualitySlider->setStyleSheet(
        "QSlider::groove:horizontal { background: #1e293b; height: 6px; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #10b981; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #34d399; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
    );
    controlLayout->addWidget(m_qualitySlider, 1);

    m_qualityValueLabel = new QLabel("85%", this);
    m_qualityValueLabel->setFixedWidth(40);
    m_qualityValueLabel->setStyleSheet("font-weight: bold; color: #10b981;");
    controlLayout->addWidget(m_qualityValueLabel);

    layout->addLayout(controlLayout);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);

    connect(m_qualitySlider, &QSlider::valueChanged, this, &ImagePreviewDialog::onQualitySliderChanged);

    if (!m_sourceImage.isNull()) {
        m_originalImgLabel->setPixmap(QPixmap::fromImage(m_sourceImage).scaled(350, 270, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ImagePreviewDialog::onQualitySliderChanged(int value) {
    m_quality = value;
    m_qualityValueLabel->setText(QString("%1%").arg(value));
    updatePreview();
}

void ImagePreviewDialog::updatePreview() {
    if (m_sourceImage.isNull()) return;

    // Simulate in-memory encoded compression
    QByteArray bufferData;
    QBuffer buffer(&bufferData);
    buffer.open(QIODevice::WriteOnly);

    const QString fmtUpper = m_targetFormatId.toUpper();
    const char* qtFormat = "JPEG";
    if (fmtUpper == "WEBP") qtFormat = "WEBP";
    else if (fmtUpper == "PNG") qtFormat = "PNG";
    else if (fmtUpper == "BMP") qtFormat = "BMP";

    m_sourceImage.save(&buffer, qtFormat, m_quality);

    QImage compressedImg;
    compressedImg.loadFromData(bufferData);

    if (!compressedImg.isNull()) {
        m_previewImgLabel->setPixmap(QPixmap::fromImage(compressedImg).scaled(350, 270, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    qint64 simulatedBytes = bufferData.size();
    if (simulatedBytes == 0) {
        core::MediaEstimationInput input;
        input.sourceSizeBytes = m_sourceSizeBytes;
        input.sourceWidth = m_sourceImage.width();
        input.sourceHeight = m_sourceImage.height();
        input.targetFormatId = m_targetFormatId;
        input.targetQuality = m_quality;
        simulatedBytes = core::MediaEstimator::estimateMediaOutput(input).estimatedSizeBytes;
    }

    double deltaPercent = 0.0;
    if (m_sourceSizeBytes > 0) {
        deltaPercent = ((static_cast<double>(simulatedBytes) - m_sourceSizeBytes) / m_sourceSizeBytes) * 100.0;
    }

    QString deltaStr = (deltaPercent > 0) ? QString("+%1%").arg(QString::number(deltaPercent, 'f', 1))
                                          : QString("%1%").arg(QString::number(deltaPercent, 'f', 1));

    m_previewInfoLabel->setText(QString("Est. Size: %1 (%2)")
                                    .arg(core::MediaEstimator::formatBytes(simulatedBytes), deltaStr));
}

} // namespace uc::ui
