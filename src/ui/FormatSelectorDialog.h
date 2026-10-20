#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QMap>
#include <QVariantMap>
#include "../core/FormatRegistry.h"
#include "../core/PresetRegistry.h"
#include "../core/EngineCapabilityResolver.h"

namespace uc::ui {

class FormatSelectorDialog : public QDialog {
    Q_OBJECT

public:
    explicit FormatSelectorDialog(const QString& sourceFormatId, QWidget* parent = nullptr, int batchCount = 1);

    QString selectedFormatId() const { return m_selectedFormatId; }
    QString selectedPresetId() const { return m_selectedPresetId; }
    QVariantMap selectedPresetOptions() const { return m_selectedPresetOptions; }
    bool applyToAllRemaining() const { return m_applyToAllRemaining; }

private slots:
    void onFilterTextChanged(const QString& text);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onPresetSelected(int index);
    void onItemSelectionChanged();
    void onNewPresetClicked();
    void onEditPresetClicked();
    void onDeletePresetClicked();

private:
    void setupPresetSection(QVBoxLayout* layout);
    void refreshPresets();
    void populateFormats();
    void updateDetailPanel(const QString& formatId);

    QString m_sourceFormatId;
    QString m_selectedFormatId;
    QString m_selectedPresetId;
    QVariantMap m_selectedPresetOptions;
    bool m_applyToAllRemaining{false};

    QComboBox* m_presetCombo{nullptr};
    QPushButton* m_editPresetBtn{nullptr};
    QPushButton* m_deletePresetBtn{nullptr};
    QLabel* m_presetDescLabel{nullptr};
    QLineEdit* m_searchEdit{nullptr};
    QListWidget* m_listWidget{nullptr};
    QLabel* m_sourceInfoLabel{nullptr};
    QLabel* m_detailLabel{nullptr};
    QCheckBox* m_applyToAllCheck{nullptr};

    QList<core::FormatDefinition> m_availableFormats;
};

} // namespace uc::ui
