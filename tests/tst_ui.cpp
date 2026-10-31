#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QListWidget>
#include <QProgressBar>
#include <QDialogButtonBox>
#include "app/AppSettings.h"
#include "app/ToolchainManager.h"
#include "core/ConversionOrchestrator.h"
#include "core/EngineCapabilityResolver.h"
#include "ui/BatchSetupDialog.h"
#include "ui/FormatSelectorDialog.h"
#include "ui/ImagePreviewDialog.h"
#include "ui/MainWindow.h"
#include "ui/QueueViewWidget.h"
#include "ui/SettingsDialog.h"
#include "ui/WatchFoldersDialog.h"
#include "ui/themes/ThemeManager.h"
using namespace uc;
class UiTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_data;
  private slots:
    void initTestCase() {
        qApp->setProperty("testDataDirectory", m_data.path());
        QVERIFY(m_data.isValid());
        qputenv("XDG_DATA_HOME", m_data.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", m_data.path().toUtf8());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_data.path());
        ui::themes::ThemeManager::applyVertDarkTheme(qApp);
        app::ToolchainManager::instance().scanAll();
    }
    void batchCancelStartsNothing() {
        QFile file(m_data.filePath("input.md"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Hello");
        file.close();
        ui::BatchSetupDialog dialog({file.fileName()});
        dialog.show();
        auto *convert = dialog.findChild<QPushButton *>("primaryButton");
        QVERIFY(convert);
        QVERIFY(!convert->isEnabled());
        QCOMPARE(dialog.entries().size(), 1);
        QVERIFY(dialog.entries().first().output.isEmpty());
        dialog.reject();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(!QFile::exists(m_data.filePath("Converted")));
    }
    void batchReviewEnablesExplicitConversion() {
        if (!app::ToolchainManager::instance().pandoc().isAvailable)
            QSKIP("Pandoc unavailable");
        const QString input = m_data.filePath("review.md");
        QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Review");
        file.close();
        ui::BatchSetupDialog dialog({input});
        dialog.show();
        auto *convert = dialog.findChild<QPushButton *>("primaryButton");
        QVERIFY(!convert->isEnabled());
        QTimer::singleShot(0, []() {
            auto *selector = qobject_cast<ui::FormatSelectorDialog *>(QApplication::activeModalWidget());
            if (!selector)
                return;
            auto *list = selector->findChild<QListWidget *>();
            for (int row = 0; row < list->count(); ++row)
                if (list->item(row)->data(Qt::UserRole).toString() == "html")
                    list->setCurrentRow(row);
            auto *buttons = selector->findChild<QDialogButtonBox *>();
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        for (auto *button : dialog.findChildren<QPushButton *>())
            if (button->text() == "Choose Format / Preset")
                button->click();
        QCOMPARE(dialog.entries().first().output, QString("html"));
        QVERIFY(convert->isEnabled());
        QVERIFY(!QFile::exists(m_data.filePath("Converted/review.html")));
        dialog.reject();
    }
    void blockedRouteHasReason() {
        const auto result = core::EngineCapabilityResolver::instance().resolveRoute("mp4", "docx");
        QVERIFY(!result.available);
        QVERIFY(!result.reason.isEmpty());
        ui::FormatSelectorDialog dialog("md");
        QVERIFY(dialog.findChild<QListWidget *>());
        QSignalSpy refreshed(&core::EngineCapabilityResolver::instance(),
                             &core::EngineCapabilityResolver::capabilitiesChanged);
        core::EngineCapabilityResolver::instance().refresh();
        QCOMPARE(refreshed.size(), 1);
    }
    void settingsCancelPreservesConfiguration() {
        auto &settings = app::AppSettings::instance();
        const auto before = settings.watchFoldersJson();
        ui::SettingsDialog dialog;
        dialog.reject();
        QCOMPARE(settings.watchFoldersJson(), before);
    }
    void queueShowsDistinctOutcomes() {
        auto orchestrator = std::make_shared<core::ConversionOrchestrator>();
        auto job = std::make_shared<core::ConversionJob>("failed", QStringList{"missing.mp4"}, "mp4", "webm",
                                                         m_data.path());
        job->setError("A long diagnostic should stay out of the status column");
        orchestrator->addJob(job);
        ui::QueueViewWidget queue(orchestrator);
        queue.show();
        queue.refreshQueue();
        auto *tree = queue.findChild<QTreeWidget *>();
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItem(0)->text(2), QString("Failed"));
        QVERIFY(orchestrator->summary().contains("1 failed"));
        QVERIFY(!orchestrator->summary().contains("All"));
    }
    void previewFailureDoesNotBlockAcceptance() {
        ui::ImagePreviewDialog dialog(m_data.filePath("missing.png"), "avif");
        dialog.show();
        QTest::qWait(350);
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons);
        QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        dialog.accept();
    }
    void screenshots() {
        const QString directory = qEnvironmentVariable("UC_SCREENSHOT_DIR");
        if (directory.isEmpty())
            QSKIP("Screenshot output not requested");
        QDir().mkpath(directory);
        ui::MainWindow window;
        window.show();
        QTest::qWait(150);
        QVERIFY(window.grab().save(directory + "/empty.png"));
        QFile sample(m_data.filePath("input.md"));QVERIFY(sample.open(QIODevice::WriteOnly));sample.write("# Example document");sample.close();
        ui::BatchSetupDialog batch({m_data.filePath("input.md")});
        batch.show();
        QTest::qWait(50);
        QVERIFY(batch.grab().save(directory + "/batch.png"));
        batch.reject();
        auto orchestrator = std::make_shared<core::ConversionOrchestrator>();
        for (int i = 0; i < 3; ++i) {
            auto job = std::make_shared<core::ConversionJob>(
                QString::number(i), QStringList{m_data.filePath("a-long-example-filename.md")}, "md", "html",
                m_data.path());
            job->transitionTo(core::JobState::Preparing);
            job->transitionTo(core::JobState::Running);
            if (i == 0)
                job->transitionTo(core::JobState::Completed);
            if (i == 1)
                job->setError("Example failure: missing delegate");
            if (i == 2)
                job->transitionTo(core::JobState::Cancelled);
            orchestrator->addJob(job);
        }
        ui::QueueViewWidget queue(orchestrator);
        queue.resize(1100, 400);
        queue.show();
        QTest::qWait(50);
        QVERIFY(queue.grab().save(directory + "/outcomes.png"));
    }
};
QTEST_MAIN(UiTests)
#include "tst_ui.moc"
