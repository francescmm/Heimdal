#pragma once

#include <JenkinsViewInfo.h>
#include <JenkinsJobInfo.h>
#include <IFetcher.h>

#include <QFrame>

class QVBoxLayout;
class QTreeWidgetItem;
class QListWidget;
class QLabel;
class QHBoxLayout;

namespace Jenkins
{
class JenkinsJobPanel;

class JobContainer : public QFrame
{
   Q_OBJECT

signals:
   void signalJobAreViews(const QVector<JenkinsViewInfo> &views);

public:
   explicit JobContainer(const IFetcher::Config &config, const JenkinsViewInfo &viewInfo, QWidget *parent = nullptr);

private:
   JenkinsViewInfo mView;
   QHBoxLayout *mMainLayout = nullptr;
   QVBoxLayout *mJobListLayout = nullptr;
   JenkinsJobPanel *mJobPanel = nullptr;

   void addJobs(const QMultiMap<QString, JenkinsJobInfo> &jobs);
   void showJobInfo(QTreeWidgetItem *item, int column);
   QIcon getIconForJob(JenkinsJobInfo job) const;
   void createHeader(const QString &name, QListWidget *listWidget);
   void onHeaderClicked(QListWidget *listWidget, QLabel *mTagArrow);
};
}
