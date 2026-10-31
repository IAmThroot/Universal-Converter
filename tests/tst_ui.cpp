#include <QtTest>
#include <QScopeGuard>
#include <QMenu>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include "ui/DesktopIntegration.h"
#include "ui/HistoryDialog.h"
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
class DesktopUrlReceiver : public QObject {
    Q_OBJECT
  public:
    QList<QUrl> opened;
  public slots:
    void capture(const QUrl &url) { opened << url; }
};

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
    void desktopHostEnvironment() {
        QProcessEnvironment source;
        source.insert("APPIMAGE", "/tmp/converter.AppImage");
        source.insert("LD_LIBRARY_PATH", "/tmp/bundle/lib");
        source.insert("ORIGINAL_LD_LIBRARY_PATH", "/opt/host/lib");
        source.insert("QT_PLUGIN_PATH", "/tmp/bundle/plugins");
        source.insert("QML2_IMPORT_PATH", "/tmp/bundle/qml");
        source.insert("QT_QPA_PLATFORM_PLUGIN_PATH", "/tmp/bundle/platforms");
        source.insert("PATH", QStringList{"/tmp/bundle/bin", "/usr/bin"}.join(QDir::listSeparator()));
        source.insert("UC_ORIGINAL_PATH", "/usr/bin");
        source.insert("DBUS_SESSION_BUS_ADDRESS", "test-session");
        const auto host = ui::DesktopIntegration::hostEnvironment(source);
        QCOMPARE(host.value("LD_LIBRARY_PATH"), QString("/opt/host/lib"));
        QCOMPARE(host.value("PATH"), QString("/usr/bin"));
        QVERIFY(!host.contains("QT_PLUGIN_PATH"));
        QVERIFY(!host.contains("QML2_IMPORT_PATH"));
        QVERIFY(!host.contains("QT_QPA_PLATFORM_PLUGIN_PATH"));
        QCOMPARE(host.value("DBUS_SESSION_BUS_ADDRESS"), QString("test-session"));
        QVERIFY(!host.contains("APPIMAGE"));
        QVERIFY(!host.contains("ORIGINAL_LD_LIBRARY_PATH"));
        QVERIFY(!host.contains("UC_ORIGINAL_PATH"));
        source.insert("APPDIR", "/tmp/bundle");
        source.remove("UC_ORIGINAL_PATH");
        QCOMPARE(ui::DesktopIntegration::hostEnvironment(source).value("PATH"), QString("/usr/bin"));
        source.insert("UC_ORIGINAL_PATH", "/usr/bin");
        source.insert("UC_ORIGINAL_QT_PLUGIN_PATH", "/host/qt/plugins");
        QCOMPARE(ui::DesktopIntegration::hostEnvironment(source).value("QT_PLUGIN_PATH"),
                 QString("/host/qt/plugins"));
        source.insert("ORIGINAL_LD_LIBRARY_PATH", QString());
        QVERIFY(!ui::DesktopIntegration::hostEnvironment(source).contains("LD_LIBRARY_PATH"));
        QProcessEnvironment native;
        native.insert("QT_PLUGIN_PATH", "/custom/plugins");
        QCOMPARE(ui::DesktopIntegration::hostEnvironment(native).value("QT_PLUGIN_PATH"),
                 QString("/custom/plugins"));
    }

    void queueAndHistoryDesktopActions() {
#ifndef Q_OS_LINUX
        QSKIP("Linux desktop launcher regression");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString capture = directory.filePath("capture.txt");
        const QString fakeGio = directory.filePath("gio");
        QFile script(fakeGio);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nprintf '%s\\n' \"$@\" \"$LD_LIBRARY_PATH\" \"$QT_PLUGIN_PATH\" "
                     "\"$QML2_IMPORT_PATH\" > \"$UC_DESKTOP_TEST_CAPTURE\"\n");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QStringList names{"APPIMAGE",         "LD_LIBRARY_PATH",        "ORIGINAL_LD_LIBRARY_PATH",
                                "QT_PLUGIN_PATH",   "QML2_IMPORT_PATH",       "PATH",
                                "UC_ORIGINAL_PATH", "UC_DESKTOP_TEST_CAPTURE"};
        QMap<QString, QByteArray> previous;
        QSet<QString> existed;
        for (const auto &name : names) {
            if (qEnvironmentVariableIsSet(qPrintable(name)))
                existed.insert(name);
            previous[name] = qgetenv(qPrintable(name));
        }
        auto restore = qScopeGuard([&]() {
            for (const auto &name : names) {
                if (existed.contains(name))
                    qputenv(qPrintable(name), previous[name]);
                else
                    qunsetenv(qPrintable(name));
            }
        });
        qputenv("APPIMAGE", "/tmp/fake.AppImage");
        qputenv("LD_LIBRARY_PATH", "/tmp/bundle/lib");
        qputenv("ORIGINAL_LD_LIBRARY_PATH", "");
        qputenv("QT_PLUGIN_PATH", "/tmp/bundle/plugins");
        qputenv("QML2_IMPORT_PATH", "/tmp/bundle/qml");
        qputenv("UC_ORIGINAL_PATH", directory.path().toUtf8());
        qputenv("UC_DESKTOP_TEST_CAPTURE", capture.toUtf8());
        const QString output = directory.filePath("converted # file ü.txt");
        QFile file(output);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("output");
        file.close();
        auto orchestrator = std::make_shared<core::ConversionOrchestrator>();
        auto job = std::make_shared<core::ConversionJob>("desktop-actions", QStringList{output}, "txt", "txt",
                                                         directory.path());
        job->setFinalOutputPath(output);
        job->transitionTo(core::JobState::Preparing);
        job->transitionTo(core::JobState::Running);
        job->transitionTo(core::JobState::Completed);
        orchestrator->addJob(job);
        ui::QueueViewWidget queue(orchestrator);
        queue.resize(1000, 400);
        queue.show();
        auto *tree = queue.findChild<QTreeWidget *>();
        QVERIFY(tree);
        tree->setCurrentItem(tree->topLevelItem(0));
        auto launch = [&](const QString &label) {
            QFile::remove(capture);
            QTimer::singleShot(0, [label]() {
                auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                if (!menu)
                    return;
                for (auto *action : menu->actions())
                    if (action->text() == label) {
                        menu->setActiveAction(action);
                        QTest::keyClick(menu, Qt::Key_Return);
                        return;
                    }
                menu->close();
            });
            emit tree->customContextMenuRequested(tree->visualItemRect(tree->topLevelItem(0)).center());
        };
        auto captured = [&]() {
            QFile result(capture);
            if (!result.open(QIODevice::ReadOnly))
                return QByteArray();
            return result.readAll();
        };
        launch("Open Output");
        QTRY_VERIFY(captured().contains(QUrl::fromLocalFile(output).toEncoded()));
        QVERIFY(!captured().contains("/tmp/bundle"));
        launch("Open Folder");
        QTRY_VERIFY(captured().contains(QUrl::fromLocalFile(directory.path()).toEncoded()));
        core::ConversionHistoryManager::instance().clearHistory();
        core::ConversionHistoryManager::instance().recordJob(job.get(), 10);
        QFile::remove(output);
        QFile::remove(capture);
        ui::HistoryDialog history;
        QVERIFY(QMetaObject::invokeMethod(&history, "onOpenOutputClicked", Qt::DirectConnection));
        QTRY_VERIFY(captured().contains(QUrl::fromLocalFile(directory.path()).toEncoded()));
        QVERIFY(!captured().contains("/tmp/bundle"));
        core::ConversionHistoryManager::instance().clearHistory();
        QFile::remove(capture);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("output");
        file.close();
        emit tree->itemDoubleClicked(tree->topLevelItem(0), 0);
        QTRY_VERIFY(captured().contains(QUrl::fromLocalFile(directory.path()).toEncoded()));

        // A failed desktop dispatch must explain the failure instead of doing nothing.
        QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Truncate));
        script.write("#!/bin/sh\nprintf 'No default application configured' >&2\nexit 3\n");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QString reported;
        QTimer closer;
        connect(&closer, &QTimer::timeout, &queue, [&]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                reported = box->text();
                box->accept();
            }
        });
        closer.start(10);
        ui::DesktopIntegration::openLocalPath(&queue, output);
        QTRY_VERIFY(reported.contains("No default application configured"));
        reported.clear();
        ui::DesktopIntegration::openLocalPath(&queue, directory.filePath("missing.txt"));
        QVERIFY(reported.contains("no longer exists"));

        // Desktops without GIO must use the detached xdg-open fallback.
        QVERIFY(script.remove());
        QFile fallback(directory.filePath("xdg-open"));
        QVERIFY(fallback.open(QIODevice::WriteOnly));
        fallback.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$UC_DESKTOP_TEST_CAPTURE\"\n");
        fallback.close();
        QVERIFY(fallback.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QFile::remove(capture);
        ui::DesktopIntegration::openLocalPath(&queue, output);
        QTRY_VERIFY(captured().contains(QUrl::fromLocalFile(output).toEncoded()));
        QVERIFY(fallback.remove());
        reported.clear();
        ui::DesktopIntegration::openLocalPath(&queue, output);
        QVERIFY(reported.contains("desktop launcher"));
        closer.stop();
#endif
    }

    void nativeDesktopActions() {
#ifdef Q_OS_LINUX
        QSKIP("Native Qt desktop dispatch is used on Windows and macOS");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("converted # file ü.txt");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("output");
        file.close();
        DesktopUrlReceiver receiver;
        QDesktopServices::setUrlHandler("file", &receiver, "capture");
        auto reset = qScopeGuard([]() { QDesktopServices::unsetUrlHandler("file"); });
        ui::DesktopIntegration::openLocalPath(nullptr, path);
        QCOMPARE(receiver.opened.size(), 1);
        QCOMPARE(receiver.opened.last(), QUrl::fromLocalFile(path));
        auto job = std::make_shared<core::ConversionJob>("native-desktop", QStringList{path}, "txt", "txt",
                                                         directory.path());
        job->setFinalOutputPath(path);
        job->transitionTo(core::JobState::Preparing);
        job->transitionTo(core::JobState::Running);
        job->transitionTo(core::JobState::Completed);
        core::ConversionHistoryManager::instance().clearHistory();
        core::ConversionHistoryManager::instance().recordJob(job.get(), 10);
        QVERIFY(file.remove());
        ui::HistoryDialog history;
        QVERIFY(QMetaObject::invokeMethod(&history, "onOpenOutputClicked", Qt::DirectConnection));
        QCOMPARE(receiver.opened.size(), 2);
        QCOMPARE(receiver.opened.last(), QUrl::fromLocalFile(directory.path()));
        core::ConversionHistoryManager::instance().clearHistory();
#endif
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
        QFile sample(m_data.filePath("input.md"));
        QVERIFY(sample.open(QIODevice::WriteOnly));
        sample.write("# Example document");
        sample.close();
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
