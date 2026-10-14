#include "DropZoneWidget.h"
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QCursor>
#include <QTimer>

namespace uc::ui {

DropZoneWidget::DropZoneWidget(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    m_iconLabel = new QLabel("⇪", this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("font-size: 38px; color: #10b981; font-weight: 300;");

    m_titleLabel = new QLabel("Drop files here or click to browse", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("font-size: 15px; font-weight: 600; color: #f4f4f5;");

    m_subtitleLabel = new QLabel("Supports 100+ Video, Audio, Image & Document formats • 100% Offline", this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setStyleSheet("font-size: 12px; color: #71717a;");

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_subtitleLabel);

    setHoverState(false);
}

void DropZoneWidget::setHoverState(bool hovered) {
    if (hovered) {
        setStyleSheet(
            "DropZoneWidget {"
            "    background-color: #18181b;"
            "    border: 2px dashed #10b981;"
            "    border-radius: 14px;"
            "}"
        );
    } else {
        setStyleSheet(
            "DropZoneWidget {"
            "    background-color: #121215;"
            "    border: 2px dashed #27272a;"
            "    border-radius: 14px;"
            "}"
            "DropZoneWidget:hover {"
            "    background-color: #141418;"
            "    border-color: #3f3f46;"
            "}"
        );
    }
}

void DropZoneWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setHoverState(true);
    }
}

void DropZoneWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    setHoverState(false);
}

void DropZoneWidget::dropEvent(QDropEvent* event) {
    setHoverState(false);
    if (!event->mimeData()->hasUrls()) return;

    QStringList filePaths;
    const auto urls = event->mimeData()->urls();
    for (const auto& url : urls) {
        if (url.isLocalFile()) {
            filePaths << url.toLocalFile();
        }
    }

    // Accept proposed drop action immediately to release the source (e.g. Windows Explorer)
    // without blocking its OLE drag-drop thread during subsequent modal dialogs.
    event->acceptProposedAction();

    if (!filePaths.isEmpty()) {
        QTimer::singleShot(0, this, [this, filePaths]() {
            emit filesDropped(filePaths);
        });
    }
}

void DropZoneWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

} // namespace uc::ui
