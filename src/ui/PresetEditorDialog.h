#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QLabel>
#include "../core/PresetRegistry.h"

namespace uc::ui {

class PresetEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit PresetEditorDialog(core::MediaCategory sourceCategory = core::MediaCategory::Video,
                                const core::ConversionPreset* existingPreset = nullptr,
                                QWidget* parent = nullptr);

    core::ConversionPreset preset() const;

private slots:
    void onCategoryChanged(int idx);
    void onExportClicked();
    void onImportClicked();
    void onSaveClicked();

private:
    void setupUi();
    void loadFromPreset(const core::ConversionPreset& p);

    core::MediaCategory m_sourceCategory;
    QString m_presetId;
    bool m_isEditMode{false};

    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_descEdit{nullptr};
    QComboBox* m_sourceCatCombo{nullptr};
    QLineEdit* m_targetFormatEdit{nullptr};

    // Video options
    QWidget* m_videoOptionsWidget{nullptr};
    QSpinBox* m_videoBitrateSpin{nullptr};
    QSpinBox* m_crfSpin{nullptr};
    QComboBox* m_speedCombo{nullptr};
    QLineEdit* m_videoCodecEdit{nullptr};
    QLineEdit* m_audioCodecEdit{nullptr};
    QSpinBox* m_audioBitrateSpin{nullptr};

    // Image options
    QWidget* m_imageOptionsWidget{nullptr};
    QSpinBox* m_qualitySpin{nullptr};
    QCheckBox* m_stripMetadataCheck{nullptr};
    QCheckBox* m_autoOrientCheck{nullptr};
    QCheckBox* m_preserveAspectRatioCheck{nullptr};

    QVariantMap m_existingOptions;
};

} // namespace uc::ui
