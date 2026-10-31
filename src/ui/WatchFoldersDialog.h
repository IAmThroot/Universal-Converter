#pragma once
#include <QDialog>
#include <QTableWidget>
#include "../core/WatchFolderService.h"
namespace uc::ui {
class WatchFoldersDialog : public QDialog {
    Q_OBJECT
  public:
    explicit WatchFoldersDialog(core::WatchFolderService *service, QWidget *parent = nullptr);

  private:
    void refresh();
    void editFolder(bool add);
    void save();
    core::WatchFolderService *m_service;
    QTableWidget *m_table;
};
} // namespace uc::ui
