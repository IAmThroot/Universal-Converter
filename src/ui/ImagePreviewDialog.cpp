#include "ImagePreviewDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QPainter>
#include <QImageReader>
#include <QBuffer>
#include <QScrollArea>
#include <QComboBox>
#include "../app/ToolchainManager.h"
#include "../engines/ImageMagickArgBuilder.h"
#include "../core/MediaEstimator.h"

namespace uc::ui {

ImagePreviewDialog::ImagePreviewDialog(const QString &sourceImagePath, const QString &targetFormatId,
                                       QWidget *parent)
    : QDialog(parent), m_sourcePath(sourceImagePath), m_targetFormatId(targetFormatId) {
    setWindowTitle("Before & After Image Quality Preview");
    resize(780, 560);

    QImageReader reader(m_sourcePath);
    m_sourceResolution = reader.size();
    if (!m_sourceResolution.isEmpty())
        reader.setScaledSize(m_sourceResolution.scaled(700, 540, Qt::KeepAspectRatio));
    m_sourceImage = reader.read();
    m_sourceSizeBytes = QFileInfo(m_sourcePath).size();
    if (!m_sourceImage.isNull()) {
        m_previewBaseImage = m_sourceImage.scaled(700, 540, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    setupUi();
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(200);
    connect(&m_debounce, &QTimer::timeout, this, &ImagePreviewDialog::updatePreview);
    m_debounce.start();
}

void ImagePreviewDialog::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *previewLayout = new QHBoxLayout();
    previewLayout->setSpacing(14);

    // Left: Original
    auto *leftBox = new QVBoxLayout();
    auto *leftTitle = new QLabel("Original Source", this);

    leftBox->addWidget(leftTitle);

    m_originalImgLabel = new QLabel(this);
    m_originalImgLabel->setMinimumSize(340, 260);
    m_originalImgLabel->setAlignment(Qt::AlignCenter);

    auto *originalScroll = new QScrollArea(this);
    originalScroll->setWidget(m_originalImgLabel);
    originalScroll->setWidgetResizable(true);
    leftBox->addWidget(originalScroll, 1);

    m_originalInfoLabel = new QLabel(this);

    m_originalInfoLabel->setText(QString("Resolution: %1x%2 | Size: %3")
                                     .arg(m_sourceResolution.width())
                                     .arg(m_sourceResolution.height())
                                     .arg(core::MediaEstimator::formatBytes(m_sourceSizeBytes)));
    leftBox->addWidget(m_originalInfoLabel);
    previewLayout->addLayout(leftBox);

    // Right: Target Preview
    auto *rightBox = new QVBoxLayout();
    auto *rightTitle = new QLabel(QString("Target Preview (%1)").arg(m_targetFormatId.toUpper()), this);

    rightBox->addWidget(rightTitle);

    m_previewImgLabel = new QLabel(this);
    m_previewImgLabel->setMinimumSize(340, 260);
    m_previewImgLabel->setAlignment(Qt::AlignCenter);

    auto *previewScroll = new QScrollArea(this);
    previewScroll->setWidget(m_previewImgLabel);
    previewScroll->setWidgetResizable(true);
    rightBox->addWidget(previewScroll, 1);

    m_previewInfoLabel = new QLabel(this);

    rightBox->addWidget(m_previewInfoLabel);
    previewLayout->addLayout(rightBox);

    layout->addWidget(new QLabel("Reduced-resolution engine sample. Final size is an estimate. Preview "
                                 "availability does not block conversion.",
                                 this));
    layout->addLayout(previewLayout, 1);
    auto *zoom = new QComboBox(this);
    zoom->addItems({"100% sample", "200% sample", "400% sample"});
    connect(zoom, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_zoom = 100 * (1 << index);
        renderImages();
    });
    layout->addWidget(zoom);

    // Quality slider controls
    auto *controlLayout = new QHBoxLayout();
    auto *qualityLabel = new QLabel("Compression Quality:", this);

    controlLayout->addWidget(qualityLabel);

    m_qualitySlider = new QSlider(Qt::Horizontal, this);
    m_qualitySlider->setRange(10, 100);
    m_qualitySlider->setValue(85);
    m_qualitySlider->setStyleSheet(
        "QSlider::groove:horizontal { background: #1e293b; height: 6px; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #10b981; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #34d399; width: 14px; margin-top: -4px; margin-bottom: "
        "-4px; border-radius: 7px; }");
    controlLayout->addWidget(m_qualitySlider, 1);

    m_qualityValueLabel = new QLabel("85%", this);
    m_qualityValueLabel->setFixedWidth(40);

    controlLayout->addWidget(m_qualityValueLabel);

    layout->addLayout(controlLayout);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);

    connect(m_qualitySlider, &QSlider::valueChanged, this, &ImagePreviewDialog::onQualitySliderChanged);

    if (!m_sourceImage.isNull()) {
        m_originalImgLabel->setPixmap(QPixmap::fromImage(m_sourceImage)
                                          .scaled(350, 270, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ImagePreviewDialog::onQualitySliderChanged(int value) {
    m_quality = value;
    m_qualityValueLabel->setText(QString("%1%").arg(value));
    ++m_generation;
    if (m_runner)
        m_runner->cancel(0);
    m_debounce.start();
}

void ImagePreviewDialog::updatePreview() {
    const auto tool = app::ToolchainManager::instance().magick();
    if (!tool.isAvailable || !m_temp.isValid()) {
        m_previewInfoLabel->setText("Preview unavailable: ImageMagick is missing");
        return;
    }
    const int generation = ++m_generation;
    m_runner = std::make_unique<core::ProcessRunner>();
    const QString output = m_temp.filePath(QString("sample_%1.%2").arg(generation).arg(m_targetFormatId));
    const QString decoded = m_temp.filePath(QString("decoded_%1.png").arg(generation));
    engines::ImageConversionOptions options;
    options.targetFormatId = m_targetFormatId;
    options.quality = m_quality;
    options.width = 700;
    options.height = 540;
    options.preserveAspectRatio = true;
    options.preserveAnimation = false;
    auto args = engines::ImageMagickArgBuilder::buildArguments(m_sourcePath + "[0]", output, options);
    m_previewInfoLabel->setText("Rendering sample…");
    m_previewImgLabel->clear();
    connect(m_runner.get(), &core::ProcessRunner::finished, this,
            [this, generation, output, decoded, tool](const core::ProcessResult &result) {
                if (generation != m_generation)
                    return;
                if (!result.isSuccess()) {
                    m_previewInfoLabel->setText("Preview unavailable; conversion can still proceed");
                    m_previewInfoLabel->setToolTip(result.errorMessage);
                    return;
                }
                // Decode with the same engine, including formats without a Qt plugin.
                auto *decoder = new core::ProcessRunner(this);
                connect(
                    decoder, &core::ProcessRunner::finished, this,
                    [this, decoder, generation, decoded, output](const core::ProcessResult &result) {
                        decoder->deleteLater();
                        if (generation != m_generation)
                            return;
                        if (!result.isSuccess() || !m_compressedImage.load(decoded)) {
                            m_previewInfoLabel->setText("Preview unavailable: sample could not be decoded");
                            return;
                        }
                        const qint64 sampleBytes = QFileInfo(output).size();
                        QString info =
                            QString("Engine sample: %1").arg(core::MediaEstimator::formatBytes(sampleBytes));
                        if (m_sourceResolution.width() > 0 && m_sourceResolution.height() > 0 &&
                            !m_compressedImage.isNull()) {
                            const double ratio =
                                double(m_sourceResolution.width()) * m_sourceResolution.height() /
                                (double(m_compressedImage.width()) * m_compressedImage.height());
                            info += QString(" · Estimated full image: ~%1 (before optimization)")
                                        .arg(core::MediaEstimator::formatBytes(qint64(sampleBytes * ratio)));
                        }
                        m_previewInfoLabel->setText(info);
                        m_previewInfoLabel->setWordWrap(true);
                        renderImages();
                    });
                decoder->start(tool.executablePath, {output + "[0]", decoded}, 15000);
            });
    m_runner->start(tool.executablePath, args.arguments, 15000);
}

void ImagePreviewDialog::renderImages() {
    auto display = [this](QLabel *label, const QImage &source) {
        if (source.isNull())
            return;
        QImage image =
            source.scaled(source.size() * m_zoom / 100, Qt::KeepAspectRatio, Qt::FastTransformation);
        QPixmap background(image.size());
        background.fill(QColor("#27272a"));
        QPainter painter(&background);
        for (int y = 0; y < image.height(); y += 12)
            for (int x = 0; x < image.width(); x += 12)
                painter.fillRect(x, y, 12, 12,
                                 ((x / 12 + y / 12) % 2) ? QColor("#52525b") : QColor("#27272a"));
        painter.drawImage(0, 0, image);
        painter.end();
        label->setPixmap(background);
        label->resize(background.size());
    };
    display(m_originalImgLabel, m_previewBaseImage);
    display(m_previewImgLabel, m_compressedImage);
}

} // namespace uc::ui
