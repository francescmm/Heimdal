#include "Controls.h"

#include <BranchDlg.h>
#include <GitBase.h>
#include <GitCache.h>
#include <GitConfig.h>
#include <GitQlientSettings.h>
#include <GitQlientStyles.h>
#include <GitQlientUpdater.h>
#include <GitRemote.h>
#include <GitStashes.h>
#include <PomodoroButton.h>
#include <QLogger.h>

#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

using namespace QLogger;

Controls::Controls(const QSharedPointer<GitCache> &cache, const QSharedPointer<GitBase> &git, QWidget *parent)
   : QFrame(parent)
   , mCache(cache)
   , mGit(git)
   , mHistory(new QToolButton(this))
   , mDiff(new QToolButton(this))
   , mBlame(new QToolButton(this))
   , mPullBtn(new QToolButton(this))
   , mPullOptions(new QToolButton(this))
   , mPushBtn(new QToolButton(this))
   , mRefreshBtn(new QToolButton(this))
   , mConfigBtn(new QToolButton(this))
   , mPomodoro(new PomodoroButton(mGit, this))
   , mVersionCheck(new QToolButton(this))
   , mMergeWarning(
         new QPushButton(tr("WARNING: There is a merge pending to be committed! Click here to solve it."), this))
   , mUpdater(new GitQlientUpdater(this))
   , mBtnGroup(new QButtonGroup(this))
   , mLastSeparator(new QFrame(this))
{
   GitQlientSettings settings(mGit->getGitDir());

   setAttribute(Qt::WA_DeleteOnClose);

   connect(mUpdater, &GitQlientUpdater::newVersionAvailable, this, [this]() {
      mVersionCheck->setVisible(true);
      mLastSeparator->setVisible(mPomodoro->isVisible() || mVersionCheck->isVisible());
   });

   mHistory->setCheckable(true);
   mHistory->setIcon(QIcon(":/icons/git_orange"));
   mHistory->setIconSize(QSize(22, 22));
   mHistory->setToolTip(tr("View"));
   mHistory->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mHistory->setShortcut(Qt::CTRL | Qt::Key_1);
   mBtnGroup->addButton(mHistory, static_cast<int>(ControlsMainViews::History));

   mDiff->setCheckable(true);
   mDiff->setIcon(QIcon(":/icons/diff"));
   mDiff->setIconSize(QSize(22, 22));
   mDiff->setToolTip(tr("Diff"));
   mDiff->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mDiff->setEnabled(false);
   mDiff->setShortcut(Qt::CTRL | Qt::Key_2);
   mBtnGroup->addButton(mDiff, static_cast<int>(ControlsMainViews::Diff));

   mBlame->setCheckable(true);
   mBlame->setIcon(QIcon(":/icons/blame"));
   mBlame->setIconSize(QSize(22, 22));
   mBlame->setToolTip(tr("Blame"));
   mBlame->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mBlame->setShortcut(Qt::CTRL | Qt::Key_3);
   mBtnGroup->addButton(mBlame, static_cast<int>(ControlsMainViews::Blame));

   const auto menu = new QMenu(mPullOptions);
   menu->installEventFilter(this);

   auto action = menu->addAction(tr("Fetch all"));
   connect(action, &QAction::triggered, this, &Controls::fetchAll);

   action = menu->addAction(tr("Prune"));
   connect(action, &QAction::triggered, this, &Controls::pruneBranches);
   menu->addSeparator();

   mPullBtn->setIconSize(QSize(22, 22));
   mPullBtn->setToolTip(tr("Pull"));
   mPullBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mPullBtn->setPopupMode(QToolButton::InstantPopup);
   mPullBtn->setIcon(QIcon(":/icons/git_pull"));
   mPullBtn->setObjectName("ToolButtonAboveMenu");
   mPullBtn->setShortcut(Qt::CTRL | Qt::Key_4);

   mPullOptions->setMenu(menu);
   mPullOptions->setIcon(QIcon(":/icons/arrow_down"));
   mPullOptions->setIconSize(QSize(22, 22));
   mPullOptions->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mPullOptions->setPopupMode(QToolButton::InstantPopup);
   mPullOptions->setToolTip("Remote actions");
   mPullOptions->setObjectName("ToolButtonWithMenu");

   const auto pullLayout = new QVBoxLayout();
   pullLayout->setContentsMargins(QMargins());
   pullLayout->setSpacing(0);
   pullLayout->addWidget(mPullBtn);
   pullLayout->addWidget(mPullOptions);

   mPushBtn->setIcon(QIcon(":/icons/git_push"));
   mPushBtn->setIconSize(QSize(22, 22));
   mPushBtn->setToolTip(tr("Push"));
   mPushBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mPushBtn->setShortcut(Qt::CTRL | Qt::Key_5);

   mRefreshBtn->setIcon(QIcon(":/icons/refresh"));
   mRefreshBtn->setIconSize(QSize(22, 22));
   mRefreshBtn->setToolTip(tr("Refresh"));
   mRefreshBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mRefreshBtn->setShortcut(Qt::Key_F5);

   mConfigBtn->setCheckable(true);
   mConfigBtn->setIcon(QIcon(":/icons/config"));
   mConfigBtn->setIconSize(QSize(22, 22));
   mConfigBtn->setToolTip(tr("Config"));
   mConfigBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mConfigBtn->setShortcut(Qt::CTRL | Qt::Key_6);
   mBtnGroup->addButton(mConfigBtn, static_cast<int>(ControlsMainViews::Config));

   const auto separator = new QFrame(this);
   separator->setObjectName("orangeSeparator");
   separator->setFixedHeight(20);

   const auto separator2 = new QFrame(this);
   separator2->setObjectName("orangeSeparator");
   separator2->setFixedHeight(20);

   const auto hLayout = new QHBoxLayout();
   hLayout->setContentsMargins(QMargins());
   hLayout->addStretch();
   hLayout->setSpacing(5);
   hLayout->addWidget(mHistory);
   hLayout->addWidget(mDiff);
   hLayout->addWidget(mBlame);
   hLayout->addWidget(separator);
   hLayout->addLayout(pullLayout);
   hLayout->addWidget(mPushBtn);
   hLayout->addWidget(separator2);

   const auto isVisible = settings.localValue("Pomodoro/Enabled", true);
   mPomodoro->setVisible(isVisible.toBool());

   mVersionCheck->setIcon(QIcon(":/icons/get_gitqlient"));
   mVersionCheck->setIconSize(QSize(22, 22));
   mVersionCheck->setText(tr("New version"));
   mVersionCheck->setObjectName("longToolButton");
   mVersionCheck->setToolButtonStyle(Qt::ToolButtonIconOnly);
   mVersionCheck->setVisible(false);

   mUpdater->checkNewGitQlientVersion();

   hLayout->addWidget(mRefreshBtn);
   hLayout->addWidget(mConfigBtn);

   mLastSeparator->setObjectName("orangeSeparator");
   mLastSeparator->setFixedHeight(20);
   mLastSeparator->setVisible(mPomodoro->isVisible() || mVersionCheck->isVisible());

   hLayout->addWidget(mLastSeparator);
   hLayout->addWidget(mPomodoro);
   hLayout->addWidget(mVersionCheck);
   hLayout->addStretch();

   mMergeWarning->setObjectName("WarningButton");
   mMergeWarning->setVisible(false);
   mBtnGroup->addButton(mMergeWarning, static_cast<int>(ControlsMainViews::Merge));

   const auto vLayout = new QVBoxLayout(this);
   vLayout->setContentsMargins(0, 5, 0, 0);
   vLayout->setSpacing(10);
   vLayout->addLayout(hLayout);
   vLayout->addWidget(mMergeWarning);

   connect(mHistory, &QToolButton::clicked, this, &Controls::signalGoRepo);
   connect(mDiff, &QToolButton::clicked, this, &Controls::signalGoDiff);
   connect(mBlame, &QToolButton::clicked, this, &Controls::signalGoBlame);
   connect(mPullBtn, &QToolButton::clicked, this, &Controls::pullCurrentBranch);
   connect(mPushBtn, &QToolButton::clicked, this, &Controls::pushCurrentBranch);
   connect(mRefreshBtn, &QToolButton::clicked, this, &Controls::requestFullReload);
   connect(mMergeWarning, &QPushButton::clicked, this, &Controls::signalGoMerge);
   connect(mVersionCheck, &QToolButton::clicked, mUpdater, &GitQlientUpdater::showInfoMessage);
   connect(mConfigBtn, &QToolButton::clicked, this, &Controls::goConfig);

   enableButtons(false);
}

Controls::~Controls()
{
   delete mBtnGroup;
}

void Controls::toggleButton(ControlsMainViews view)
{
   mBtnGroup->button(static_cast<int>(view))->setChecked(true);
}

void Controls::enableButtons(bool enabled)
{
   mHistory->setEnabled(enabled);
   mBlame->setEnabled(enabled);
   mPullBtn->setEnabled(enabled);
   mPullOptions->setEnabled(enabled);
   mPushBtn->setEnabled(enabled);
   mRefreshBtn->setEnabled(enabled);
   mConfigBtn->setEnabled(enabled);
}

void Controls::pullCurrentBranch()
{
   GitQlientSettings settings(mGit->getGitDir());
   const auto updateOnPull = settings.localValue("UpdateOnPull", true).toBool();

   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto ret = git->pull(updateOnPull);
   QApplication::restoreOverrideCursor();

   if (ret.success)
   {
      if (ret.output.contains("merge conflict", Qt::CaseInsensitive))
         emit signalPullConflict();
      else
         emit requestFullReload();
   }
   else
   {
      if (ret.output.contains("error: could not apply", Qt::CaseInsensitive)
          && ret.output.contains("causing a conflict", Qt::CaseInsensitive))
      {
         emit signalPullConflict();
      }
      else
      {
         QMessageBox msgBox(QMessageBox::Critical, tr("Error while pulling"),
                            QString(tr("There were problems during the pull operation. Please, see the detailed "
                                       "description for more information.")),
                            QMessageBox::Ok, this);
         msgBox.setDetailedText(ret.output);
         msgBox.setStyleSheet(GitQlientStyles::getStyles());
         msgBox.exec();
      }
   }
}

void Controls::fetchAll()
{
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   GitQlientSettings settings(mGit->getGitDir());
   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto ret = git->fetch(settings.localValue("PruneOnFetch").toBool());
   QApplication::restoreOverrideCursor();

   if (!ret)
      emit requestFullReload();
}

void Controls::activateMergeWarning()
{
   mMergeWarning->setVisible(true);
}

void Controls::disableMergeWarning()
{
   mMergeWarning->setVisible(false);
}

void Controls::disableDiff()
{
   mDiff->setDisabled(true);
}

void Controls::enableDiff()
{
   mDiff->setEnabled(true);
}

ControlsMainViews Controls::getCurrentSelectedButton() const
{
   return mBlame->isChecked() ? ControlsMainViews::Blame : ControlsMainViews::History;
}

void Controls::changePomodoroVisibility()
{
   GitQlientSettings settings(mGit->getGitDir());
   const auto isVisible = settings.localValue("Pomodoro/Enabled", true);
   mPomodoro->setVisible(isVisible.toBool());
}

void Controls::pushCurrentBranch()
{
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto ret = git->push();
   QApplication::restoreOverrideCursor();

   if (ret.output.contains("has no upstream branch"))
   {
      const auto currentBranch = mGit->getCurrentBranch();
      BranchDlg dlg({ currentBranch, BranchDlgMode::PUSH_UPSTREAM, mCache, mGit });
      const auto dlgRet = dlg.exec();

      if (dlgRet == QDialog::Accepted)
         emit signalRefreshPRsCache();
   }
   else if (ret.success)
   {
      const auto currentBranch = mGit->getCurrentBranch();
      QScopedPointer<GitConfig> git(new GitConfig(mGit));
      const auto remote = git->getRemoteForBranch(currentBranch);

      if (remote.success)
      {
         const auto oldSha = mCache->getShaOfReference(QString("%1/%2").arg(remote.output, currentBranch),
                                                       References::Type::RemoteBranches);
         const auto sha = mCache->getShaOfReference(currentBranch, References::Type::LocalBranch);
         mCache->deleteReference(oldSha, References::Type::RemoteBranches,
                                 QString("%1/%2").arg(remote.output, currentBranch));
         mCache->insertReference(sha, References::Type::RemoteBranches,
                                 QString("%1/%2").arg(remote.output, currentBranch));
         emit mCache->signalCacheUpdated();
         emit signalRefreshPRsCache();
      }
   }
   else
   {
      QMessageBox msgBox(
          QMessageBox::Critical, tr("Error while pushing"),
          QString(tr("There were problems during the push operation. Please, see the detailed description "
                     "for more information.")),
          QMessageBox::Ok, this);
      msgBox.setDetailedText(ret.output);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
}

void Controls::pruneBranches()
{
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto ret = git->prune();
   QApplication::restoreOverrideCursor();

   if (!ret.success)
      emit requestReferencesReload();
}

bool Controls::eventFilter(QObject *obj, QEvent *event)
{
   if (const auto menu = qobject_cast<QMenu *>(obj); menu && event->type() == QEvent::Show)
   {
      auto localPos = menu->parentWidget()->pos();
      localPos.setX(localPos.x());
      auto pos = mapToGlobal(localPos);
      menu->show();
      pos.setY(pos.y() + menu->parentWidget()->height());
      menu->move(pos);
      return true;
   }

   return false;
}
