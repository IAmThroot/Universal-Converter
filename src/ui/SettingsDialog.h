#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>

#include <QTableWidget>

namespace uc::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit SettingsDialog(QWidget *parent = nullptr);

  private slots:
    void onSaveClicked();
    void onBrowseCustomDest();
    void onBrowseFfmpeg();
    void onBrowseMagick();
    void onBrowsePandoc();

  private:
    void loadSettings();

    QSpinBox *m_concurrencySpin;
    QCheckBox *m_hwAccelCheck;
    QComboBox *m_destModeCombo;
    QLineEdit *m_customDestEdit;
    QPushButton *m_browseCustomDestBtn;

    QLineEdit *m_ffmpegEdit;
    QLineEdit *m_magickEdit;
    QLineEdit *m_pandocEdit;
};

} // namespace uc::ui
