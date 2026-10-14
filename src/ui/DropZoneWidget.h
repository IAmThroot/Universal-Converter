#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QStringList>

namespace uc::ui {

class DropZoneWidget : public QWidget {
    Q_OBJECT

public:
    explicit DropZoneWidget(QWidget* parent = nullptr);

signals:
    void filesDropped(const QStringList& filePaths);
    void clicked();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setHoverState(bool hovered);

    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_subtitleLabel;
};

} // namespace uc::ui
