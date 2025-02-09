#pragma once

#include <QButtonGroup>
#include <QMap>
#include <QWidget>

class GitBase;
class QTimer;
class FileEditor;
class QPushButton;
class QLabel;
class QAbstractButton;

namespace Ui
{
class ConfigWidget;
}

class ConfigWidget : public QWidget
{
   Q_OBJECT

signals:
   void reloadView();
   void reloadDiffFont();
   void commitTitleMaxLenghtChanged();
   void panelsVisibilityChanged();
   void pomodoroVisibilityChanged();
   void moveLogsAndClose();
   void autoFetchChanged(int minutes);
   void autoRefreshChanged(int seconds);

public:
   explicit ConfigWidget(const QSharedPointer<GitBase> &git, QWidget *parent = nullptr);
   ~ConfigWidget();

   void onPanelsVisibilityChanged();

private:
   Ui::ConfigWidget *ui;
   QSharedPointer<GitBase> mGit;
   int mOriginalRepoOrder = 0;
   bool mShowResetMsg = false;
   QTimer *mFeedbackTimer = nullptr;
   QPushButton *mSave = nullptr;
   FileEditor *mLocalGit = nullptr;
   FileEditor *mGlobalGit = nullptr;
   QButtonGroup *mDownloadButtons = nullptr;
   QVector<QWidget *> mPluginWidgets;
   QStringList mPluginNames;
   QPushButton *mPbFeaturesTour;

   void clearCache();
   void clearLogs();
   void clearFolder(const QString &folder, QLabel *label);
   void calculateLogsSize();
   uint64_t calculateDirSize(const QString &dirPath);
   void enableWidgets();
   void saveFile();
   void showCredentialsDlg();
   void selectFolder();
   void selectEditor();
   void useDefaultLogsFolder();
   void showFeaturesTour();
   void fillLanguageBox() const;

private slots:
   void saveConfig();
   void onCredentialsOptionChanged(QAbstractButton *button);
   void onPullStrategyChanged(int index);
};
