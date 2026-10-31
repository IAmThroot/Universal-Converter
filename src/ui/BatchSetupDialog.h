#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVariantMap>
#include "../app/AppSettings.h"
namespace uc::ui {
struct BatchEntry {
    QString input;
    QString output;
    QVariantMap options;
};
class BatchSetupDialog : public QDialog {
    Q_OBJECT
  public:
    explicit BatchSetupDialog(const QStringList &paths, QWidget *parent = nullptr);
    QList<BatchEntry> entries() const;
    app::DestinationMode destinationMode() const;
    QString destination() const;

  private:
    void refresh();
    QTableWidget *m_table;
    QComboBox *m_mode;
    QLineEdit *m_destination;
    QPushButton *m_convert;
    QList<BatchEntry> m_entries;
};
} // namespace uc::ui
