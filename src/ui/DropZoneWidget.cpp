#include "DropZoneWidget.h"
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QCursor>
#include <QTimer>
#include <QStyle>
#include <QKeyEvent>

namespace uc::ui {

DropZoneWidget::DropZoneWidget(QWidget *parent) : QWidget(parent) {
    setObjectName("dropZone");
    setAttribute(Qt::WA_StyledBackground, true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("Add files for conversion");
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    m_iconLabel = new QLabel("⇪", this);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    m_titleLabel = new QLabel("Drop files here or click to browse", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_subtitleLabel = new QLabel("Supports 100+ Video, Audio, Image & Document formats • 100% Offline", this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_subtitleLabel);

    setHoverState(false);
}

void DropZoneWidget::setHoverState(bool hovered) {
    setProperty("dragActive", hovered);
    style()->unpolish(this);
    style()->polish(this);
}

void DropZoneWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) {
        emit clicked();
        event->accept();
    } else
        QWidget::keyPressEvent(event);
}

void DropZoneWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setHoverState(true);
    }
}

void DropZoneWidget::dragLeaveEvent(QDragLeaveEvent *event) {
    Q_UNUSED(event);
    setHoverState(false);
}

void DropZoneWidget::dropEvent(QDropEvent *event) {
    setHoverState(false);
    if (!event->mimeData()->hasUrls())
        return;

    QStringList filePaths;
    const auto urls = event->mimeData()->urls();
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            filePaths << url.toLocalFile();
        }
    }

    // Accept proposed drop action immediately to release the source (e.g. Windows Explorer)
    // without blocking its OLE drag-drop thread during subsequent modal dialogs.
    event->acceptProposedAction();

    if (!filePaths.isEmpty()) {
        QTimer::singleShot(0, this, [this, filePaths]() { emit filesDropped(filePaths); });
    }
}

void DropZoneWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

} // namespace uc::ui
