#include "HistoryWidget.h"

#include <AmendWidget.h>
#include <BranchesWidget.h>
#include <CheckBox.h>
#include <CommitHistoryModel.h>
#include <CommitHistoryView.h>
#include <CommitInfo.h>
#include <CommitInfoWidget.h>
#include <FileDiffWidget.h>
#include <FileEditor.h>
#include <FullDiffWidget.h>
#include <GitBase.h>
#include <GitBranches.h>
#include <GitCache.h>
#include <GitConfig.h>
#include <GitHistory.h>
#include <GitLocal.h>
#include <GitMerge.h>
#include <GitQlientSettings.h>
#include <GitQlientStyles.h>
#include <GitRemote.h>
#include <GitRepoLoader.h>
#include <GitWip.h>
#include <RepositoryViewDelegate.h>
#include <WipHelper.h>
#include <WipWidget.h>

#include <QLogger.h>

#include <QApplication>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>

using namespace QLogger;

HistoryWidget::HistoryWidget(const QSharedPointer<GitCache> &cache, const QSharedPointer<GitBase> git,
                             const QSharedPointer<GitQlientSettings> &settings, QWidget *parent)
   : QFrame(parent)
   , mGit(git)
   , mCache(cache)
   , mSettings(settings)
   , mReturnFromFull(new QPushButton(QIcon(":/icons/back"), "", this))
   , mUserName(new QLabel(this))
   , mUserEmail(new QLabel(this))
   , mSplitter(new QSplitter(this))
{
   QLog_Info("Performance", "HistoryWidget loading...");
   setAttribute(Qt::WA_DeleteOnClose);

   QSharedPointer<GitConfig> gitConfig(new GitConfig(mGit));
   gitConfig->getUserNameAsync(true);
   gitConfig->getUserEmailAsync(true);

   connect(gitConfig.get(), &GitConfig::signalNameReceived, this, [this, gitConfig](QString name, bool local) {
      if (!name.isEmpty())
         mUserName->setText(name);
      else if (local)
         gitConfig->getUserNameAsync(false);
   });

   connect(gitConfig.get(), &GitConfig::signalEmailReceived, this, [this, gitConfig](QString email, bool local) {
      if (!email.isEmpty())
         mUserEmail->setText(email);
      else if (local)
         gitConfig->getUserEmailAsync(false);
   });

   const auto wipInfoFrame = new QFrame(this);
   wipInfoFrame->setObjectName("wipInfoFrame");
   const auto wipInfoLayout = new QVBoxLayout(wipInfoFrame);
   wipInfoLayout->setContentsMargins(QMargins());
   wipInfoLayout->setSpacing(10);
   wipInfoLayout->addWidget(mUserName);
   wipInfoLayout->addWidget(mUserEmail);

   mWipWidget = new WipWidget(mCache, mGit, this);
   mAmendWidget = new AmendWidget(mCache, mGit, this);
   mCommitInfoWidget = new CommitInfoWidget(mCache, mGit, this);

   mCommitStackedWidget = new QStackedWidget(this);
   mCommitStackedWidget->setCurrentIndex(0);
   mCommitStackedWidget->addWidget(mCommitInfoWidget);
   mCommitStackedWidget->addWidget(mWipWidget);
   mCommitStackedWidget->addWidget(mAmendWidget);

   const auto wipLayout = new QVBoxLayout();
   wipLayout->setContentsMargins(QMargins());
   wipLayout->setSpacing(5);
   wipLayout->addWidget(wipInfoFrame);
   wipLayout->addWidget(mCommitStackedWidget);

   const auto wipFrame = new QFrame(this);
   wipFrame->setLayout(wipLayout);
   wipFrame->setMinimumWidth(200);
   wipFrame->setMaximumWidth(500);

   connect(mWipWidget, &CommitChangesWidget::signalShowDiff, this, &HistoryWidget::showWipFileDiff);
   connect(mWipWidget, &CommitChangesWidget::changesCommitted, this, &HistoryWidget::returnToView);
   connect(mWipWidget, &CommitChangesWidget::fileStaged, this, &HistoryWidget::returnToViewIfObsolete);
   connect(mWipWidget, &CommitChangesWidget::changesCommitted, this, &HistoryWidget::changesCommitted);
   connect(mWipWidget, &CommitChangesWidget::changesCommitted, this, &HistoryWidget::cleanCommitPanels);
   connect(mWipWidget, &CommitChangesWidget::unstagedFilesChanged, this, &HistoryWidget::onRevertedChanges);
   connect(mWipWidget, &CommitChangesWidget::signalShowFileHistory, this, &HistoryWidget::signalShowFileHistory);
   connect(mWipWidget, &CommitChangesWidget::signalUpdateWip, this, &HistoryWidget::signalUpdateWip);
   connect(mWipWidget, &CommitChangesWidget::changeReverted, this, [this](const QString &revertedFile) {
      if (mWipFileDiff->getCurrentFile().contains(revertedFile))
         returnToView();
   });
   connect(mWipWidget, &CommitChangesWidget::changeReverted, this, &HistoryWidget::onRevertedChanges);

   connect(mAmendWidget, &CommitChangesWidget::logReload, this, &HistoryWidget::logReload);
   connect(mAmendWidget, &CommitChangesWidget::signalShowDiff, this, &HistoryWidget::showWipFileDiff);
   connect(mAmendWidget, &CommitChangesWidget::changesCommitted, this, &HistoryWidget::returnToView);
   connect(mAmendWidget, &CommitChangesWidget::fileStaged, this, &HistoryWidget::returnToViewIfObsolete);
   connect(mAmendWidget, &CommitChangesWidget::changesCommitted, this, &HistoryWidget::changesCommitted);
   connect(mAmendWidget, &CommitChangesWidget::changesCommitted, this, &HistoryWidget::cleanCommitPanels);
   connect(mAmendWidget, &CommitChangesWidget::unstagedFilesChanged, this, &HistoryWidget::onRevertedChanges);
   connect(mAmendWidget, &CommitChangesWidget::signalShowFileHistory, this, &HistoryWidget::signalShowFileHistory);
   connect(mAmendWidget, &CommitChangesWidget::signalUpdateWip, this, &HistoryWidget::signalUpdateWip);
   connect(mAmendWidget, &CommitChangesWidget::signalCancelAmend, this, &HistoryWidget::selectCommit);

   connect(mCommitInfoWidget, &CommitInfoWidget::signalOpenFileCommit, this, &HistoryWidget::signalShowDiff);
   connect(mCommitInfoWidget, &CommitInfoWidget::signalShowFileHistory, this, &HistoryWidget::signalShowFileHistory);

   mSearchInput = new QLineEdit(this);
   mSearchInput->setObjectName("SearchInput");

   mSearchInput->setPlaceholderText(
       tr("Press Return/Enter to search by SHA/message. Press Ctrl+Return/Enter to cherry-pick the SHA."));
   connect(mSearchInput, &QLineEdit::returnPressed, this, &HistoryWidget::search);

   mRepositoryModel = new CommitHistoryModel(mCache, mGit);
   mRepositoryView = new CommitHistoryView(mCache, mGit, mSettings, this);

   connect(mRepositoryView, &CommitHistoryView::fullReload, this, &HistoryWidget::fullReload);
   connect(mRepositoryView, &CommitHistoryView::referencesReload, this, &HistoryWidget::referencesReload);
   connect(mRepositoryView, &CommitHistoryView::logReload, this, &HistoryWidget::logReload);
   connect(mRepositoryView, &CommitHistoryView::signalOpenDiff, this, &HistoryWidget::onOpenFullDiff);
   connect(mRepositoryView, &CommitHistoryView::clicked, this, &HistoryWidget::commitSelected);
   connect(mRepositoryView, &CommitHistoryView::customContextMenuRequested, this, [this](const QPoint &pos) {
      const auto rowIndex = mRepositoryView->indexAt(pos);
      commitSelected(rowIndex);
   });
   connect(mRepositoryView, &CommitHistoryView::signalAmendCommit, this, &HistoryWidget::onAmendCommit);
   connect(mRepositoryView, &CommitHistoryView::signalRebaseConflict, this, &HistoryWidget::signalRebaseConflict);
   connect(mRepositoryView, &CommitHistoryView::signalMergeRequired, this, &HistoryWidget::mergeBranch);
   connect(mRepositoryView, &CommitHistoryView::mergeSqushRequested, this, &HistoryWidget::mergeSquashBranch);
   connect(mRepositoryView, &CommitHistoryView::signalCherryPickConflict, this,
           &HistoryWidget::signalCherryPickConflict);
   connect(mRepositoryView, &CommitHistoryView::signalPullConflict, this, &HistoryWidget::signalPullConflict);

   mRepositoryView->setObjectName("historyGraphView");
   mRepositoryView->setModel(mRepositoryModel);
   mRepositoryView->setItemDelegate(mItemDelegate
                                    = new RepositoryViewDelegate(mCache, mGit, mRepositoryView));
   mRepositoryView->setEnabled(true);

   mBranchesWidget = new BranchesWidget(mCache, mGit, this);

   connect(mBranchesWidget, &BranchesWidget::fullReload, this, &HistoryWidget::fullReload);
   connect(mBranchesWidget, &BranchesWidget::logReload, this, &HistoryWidget::logReload);

   connect(mBranchesWidget, &BranchesWidget::signalSelectCommit, mRepositoryView, &CommitHistoryView::focusOnCommit);
   connect(mBranchesWidget, &BranchesWidget::signalSelectCommit, this, &HistoryWidget::goToSha);
   connect(mBranchesWidget, &BranchesWidget::signalOpenSubmodule, this, &HistoryWidget::signalOpenSubmodule);
   connect(mBranchesWidget, &BranchesWidget::signalMergeRequired, this, &HistoryWidget::mergeBranch);
   connect(mBranchesWidget, &BranchesWidget::mergeSqushRequested, this, &HistoryWidget::mergeSquashBranch);
   connect(mBranchesWidget, &BranchesWidget::signalPullConflict, this, &HistoryWidget::signalPullConflict);
   connect(mBranchesWidget, &BranchesWidget::panelsVisibilityChanged, this, &HistoryWidget::panelsVisibilityChanged);

   const auto cherryPickBtn = new QPushButton(tr("Cherry-pick"), this);
   cherryPickBtn->setEnabled(false);
   cherryPickBtn->setObjectName("cherryPickBtn");
   cherryPickBtn->setToolTip("Cherry-pick the commit");
   cherryPickBtn->setShortcut(Qt::CTRL | Qt::Key_Return);
   cherryPickBtn->setShortcut(Qt::CTRL | Qt::Key_Enter);
   connect(cherryPickBtn, &QPushButton::clicked, this, &HistoryWidget::cherryPickCommit);
   connect(mSearchInput, &QLineEdit::textChanged, this,
           [cherryPickBtn](const QString &text) { cherryPickBtn->setEnabled(!text.isEmpty()); });

   mChShowAllBranches = new CheckBox(tr("Show all branches"), this);
   mChShowAllBranches->setChecked(mSettings->localValue("ShowAllBranches", true).toBool());
   connect(mChShowAllBranches, &CheckBox::toggled, this, &HistoryWidget::onShowAllUpdated);

   const auto graphOptionsLayout = new QHBoxLayout();
   graphOptionsLayout->setContentsMargins(QMargins());
   graphOptionsLayout->setSpacing(10);
   graphOptionsLayout->addWidget(mSearchInput);
   graphOptionsLayout->addWidget(cherryPickBtn);
   graphOptionsLayout->addWidget(mChShowAllBranches);

   const auto viewLayout = new QVBoxLayout();
   viewLayout->setContentsMargins(QMargins());
   viewLayout->setSpacing(5);
   viewLayout->addLayout(graphOptionsLayout);
   viewLayout->addWidget(mRepositoryView);

   mGraphFrame = new QFrame(this);
   mGraphFrame->setLayout(viewLayout);

   mWipFileDiff = new FileDiffWidget(mGit, mCache, this);

   connect(mReturnFromFull, &QPushButton::clicked, this, &HistoryWidget::returnToView);

   mFullDiffWidget = new FullDiffWidget(mGit, mCache);

   const auto fullFrame = new QFrame(this);
   const auto fullLayout = new QGridLayout(fullFrame);
   fullLayout->setSpacing(10);
   fullLayout->setContentsMargins(QMargins());
   fullLayout->addWidget(mReturnFromFull, 0, 0);
   fullLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Fixed), 0, 1);
   fullLayout->addWidget(mFullDiffWidget, 1, 0, 1, 2);

   mCenterStackedWidget = new QStackedWidget(this);
   mCenterStackedWidget->setMinimumWidth(600);
   mCenterStackedWidget->insertWidget(static_cast<int>(Pages::Graph), mGraphFrame);
   mCenterStackedWidget->insertWidget(static_cast<int>(Pages::FileDiff), mWipFileDiff);
   mCenterStackedWidget->insertWidget(static_cast<int>(Pages::FullDiff), fullFrame);

   connect(mWipFileDiff, &FileDiffWidget::exitRequested, this, &HistoryWidget::returnToView);
   connect(mWipFileDiff, &FileDiffWidget::fileStaged, this, &HistoryWidget::signalUpdateWip);
   connect(mWipFileDiff, &FileDiffWidget::fileReverted, this, &HistoryWidget::signalUpdateWip);
   connect(mWipFileDiff, &FileDiffWidget::exitRequested, this, &HistoryWidget::signalUpdateWip);

   mSplitter->insertWidget(0, wipFrame);
   mSplitter->insertWidget(1, mCenterStackedWidget);
   mSplitter->setCollapsible(1, false);
   mSplitter->insertWidget(2, mBranchesWidget);

   const auto minimalActive = mBranchesWidget->isMinimalViewActive();
   const auto branchesWidth = minimalActive ? 50 : 200;

   rearrangeSplittrer(minimalActive);

   connect(mBranchesWidget, &BranchesWidget::minimalViewStateChanged, this, &HistoryWidget::rearrangeSplittrer);

   const auto splitterSate = mSettings->localValue("HistoryWidgetState", QByteArray()).toByteArray();

   if (splitterSate.isEmpty())
      mSplitter->setSizes({ 200, 500, branchesWidth });
   else
      mSplitter->restoreState(splitterSate);

   const auto layout = new QHBoxLayout(this);
   layout->setContentsMargins(QMargins());
   layout->addWidget(mSplitter);

   mCenterStackedWidget->setCurrentIndex(static_cast<int>(Pages::Graph));
   mCenterStackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
   mBranchesWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

HistoryWidget::~HistoryWidget()
{
   mSettings->setLocalValue("HistoryWidgetState", mSplitter->saveState());

   delete mItemDelegate;
   delete mRepositoryModel;
}

void HistoryWidget::clear()
{
   mRepositoryView->clear();
   resetWip();
   mBranchesWidget->clear();
   mCommitInfoWidget->clear();
   mAmendWidget->clear();

   mCommitStackedWidget->setCurrentIndex(mCommitStackedWidget->currentIndex());
}

void HistoryWidget::resetWip()
{
   mWipWidget->clear();
}

void HistoryWidget::loadBranches(bool fullReload)
{
   if (fullReload)
      mBranchesWidget->showBranches();
   else
      mBranchesWidget->refreshCurrentBranchLink();
}

void HistoryWidget::updateUiFromWatcher()
{
   if (const auto widget = dynamic_cast<CommitChangesWidget *>(mCommitStackedWidget->currentWidget()))
      widget->reload();

   if (const auto widget = dynamic_cast<IDiffWidget *>(mCenterStackedWidget->currentWidget()))
      widget->reload();
}

void HistoryWidget::focusOnCommit(const QString &sha)
{
   mRepositoryView->focusOnCommit(sha);
}

void HistoryWidget::updateGraphView(int totalCommits)
{
   mRepositoryModel->onNewRevisions(totalCommits);

   const auto currentSha = mRepositoryView->getCurrentSha();
   selectCommit(currentSha);
   focusOnCommit(currentSha);
}

void HistoryWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Shift)
      mReverseSearch = true;

   QFrame::keyPressEvent(event);
}

void HistoryWidget::keyReleaseEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Shift)
      mReverseSearch = false;

   QFrame::keyReleaseEvent(event);
}

void HistoryWidget::onOpenFullDiff(const QString &sha)
{
   if (sha == ZERO_SHA)
   {
      const auto commit = mCache->commitInfo(ZERO_SHA);
      QScopedPointer<GitHistory> git(new GitHistory(mGit));
      const auto ret = git->getCommitDiff(ZERO_SHA, commit.firstParent());

      if (ret.success && !ret.output.isEmpty())
      {
         mFullDiffWidget->loadDiff(ZERO_SHA, commit.firstParent(), ret.output);
         mCenterStackedWidget->setCurrentIndex(static_cast<int>(Pages::FullDiff));
      }
      else
         QMessageBox::warning(this, tr("No diff available!"), tr("There is no diff to show."));
   }
   else
      emit signalOpenDiff(sha);
}

void HistoryWidget::rearrangeSplittrer(bool minimalActive)
{
   if (minimalActive)
   {
      mBranchesWidget->setFixedWidth(50);
      mSplitter->setCollapsible(2, false);
   }
   else
   {
      mBranchesWidget->setMinimumWidth(250);
      mBranchesWidget->setMaximumWidth(500);
      mSplitter->setCollapsible(2, true);
   }
}

void HistoryWidget::cleanCommitPanels()
{
   mWipWidget->clearStaged();
   mAmendWidget->clearStaged();
}

void HistoryWidget::onRevertedChanges()
{
   WipHelper::update(mGit, mCache);

   updateUiFromWatcher();
}

void HistoryWidget::onCommitTitleMaxLenghtChanged()
{
   mWipWidget->setCommitTitleMaxLength();
   mAmendWidget->setCommitTitleMaxLength();
}

void HistoryWidget::onPanelsVisibilityChanged()
{
   mBranchesWidget->onPanelsVisibilityChaned();
}

void HistoryWidget::onDiffFontSizeChanged()
{
   mWipFileDiff->updateFontSize();
}

void HistoryWidget::search()
{
   if (const auto text = mSearchInput->text(); !text.isEmpty())
   {
      auto commitInfo = mCache->commitInfo(text);

      if (commitInfo.isValid())
         goToSha(text);
      else
      {
         auto selectedItems = mRepositoryView->selectedIndexes();
         auto startingRow = 0;

         if (!selectedItems.isEmpty())
         {
            std::sort(selectedItems.begin(), selectedItems.end(),
                      [](const QModelIndex index1, const QModelIndex index2) { return index1.row() <= index2.row(); });
            startingRow = selectedItems.constFirst().row();
         }

         commitInfo = mCache->searchCommitInfo(text, startingRow + 1, mReverseSearch);

         if (commitInfo.isValid())
            goToSha(commitInfo.sha);
         else
            QMessageBox::information(this, tr("Not found!"), tr("No commits where found based on the search text."));
      }
   }
}

void HistoryWidget::goToSha(const QString &sha)
{
   mRepositoryView->focusOnCommit(sha);

   selectCommit(sha);
}

void HistoryWidget::commitSelected(const QModelIndex &index)
{
   const auto sha = mRepositoryModel->sha(index.row());

   selectCommit(sha);
}

void HistoryWidget::onShowAllUpdated(bool showAll)
{
   GitQlientSettings settings(mGit->getGitDir());
   settings.setLocalValue("ShowAllBranches", showAll);

   emit signalAllBranchesActive(showAll);
   emit logReload();
}

void HistoryWidget::mergeBranch(const QString &current, const QString &branchToMerge)
{
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitMerge> git(new GitMerge(mGit));
   const auto ret = git->merge(current, { branchToMerge });

   if (ret.success)
      WipHelper::update(mGit, mCache);

   WipHelper::update(mGit, mCache);

   QApplication::restoreOverrideCursor();

   processMergeResponse(ret);
}

void HistoryWidget::mergeSquashBranch(const QString &current, const QString &branchToMerge)
{
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitMerge> git(new GitMerge(mGit));
   const auto ret = git->squashMerge(current, { branchToMerge });

   if (ret.success)
      WipHelper::update(mGit, mCache);

   WipHelper::update(mGit, mCache);

   QApplication::restoreOverrideCursor();

   processMergeResponse(ret);
}

void HistoryWidget::processMergeResponse(const GitExecResult &ret)
{
   if (!ret.success)
   {
      QMessageBox msgBox(
          QMessageBox::Critical, tr("Merge failed"),
          QString(tr("There were problems during the merge. Please, see the detailed description for more "
                     "information.<br><br>GitQlient will show the merge helper tool.")),
          QMessageBox::Ok, this);
      msgBox.setDetailedText(ret.output);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();

      emit signalMergeConflicts();
   }
   else
   {
      if (!ret.output.isEmpty())
      {
         if (ret.output.contains("error: could not apply", Qt::CaseInsensitive)
             || ret.output.contains(" conflict", Qt::CaseInsensitive))
         {
            QMessageBox msgBox(
                QMessageBox::Warning, tr("Merge status"),
                tr("There were problems during the merge. Please, see the detailed description for more information."),
                QMessageBox::Ok, this);
            msgBox.setDetailedText(ret.output);
            msgBox.setStyleSheet(GitQlientStyles::getStyles());
            msgBox.exec();

            emit signalMergeConflicts();
         }
         else
         {
            emit fullReload();

            QMessageBox msgBox(
                QMessageBox::Information, tr("Merge successful"),
                tr("The merge was successfully done. See the detailed description for more information."),
                QMessageBox::Ok, this);
            msgBox.setDetailedText(ret.output);
            msgBox.setStyleSheet(GitQlientStyles::getStyles());
            msgBox.exec();
         }
      }
   }
}

void HistoryWidget::selectCommit(const QString &goToSha)
{
   const auto isWip = goToSha == ZERO_SHA;
   mCommitStackedWidget->setCurrentIndex(isWip);

   QLog_Info("UI", QString("Selected commit {%1}").arg(goToSha));

   if (isWip)
      mWipWidget->reload();
   else
      mCommitInfoWidget->configure(goToSha);
}

void HistoryWidget::onAmendCommit(const QString &sha)
{
   mCommitStackedWidget->setCurrentIndex(2);
   mAmendWidget->configure(sha);
}

void HistoryWidget::returnToView()
{
   mCenterStackedWidget->setCurrentIndex(static_cast<int>(Pages::Graph));
   mBranchesWidget->returnToSavedView();
}

void HistoryWidget::returnToViewIfObsolete(const QString &fileName)
{
   if (mWipFileDiff->getCurrentFile().contains(fileName, Qt::CaseInsensitive))
      returnToView();
}

void HistoryWidget::cherryPickCommit()
{
   auto error = false;
   QString errorStr;

   if (auto commit = mCache->commitInfo(mSearchInput->text()); commit.isValid())
   {
      const auto lastShaBeforeCommit = mGit->getLastCommit().output.trimmed();
      const auto git = QScopedPointer<GitLocal>(new GitLocal(mGit));
      const auto ret = git->cherryPickCommit(commit.sha);

      if (ret.success)
      {
         mSearchInput->clear();

         commit.sha = mGit->getLastCommit().output.trimmed();

         mCache->insertCommit(commit);
         mCache->deleteReference(lastShaBeforeCommit, References::Type::LocalBranch, mGit->getCurrentBranch());
         mCache->insertReference(commit.sha, References::Type::LocalBranch, mGit->getCurrentBranch());

         QScopedPointer<GitHistory> gitHistory(new GitHistory(mGit));
         const auto ret = gitHistory->getDiffFiles(commit.sha, lastShaBeforeCommit);

         mCache->insertRevisionFiles(commit.sha, lastShaBeforeCommit, RevisionFiles(ret.output));

         emit mCache->signalCacheUpdated();
         emit logReload();
      }
      else
      {
         if (ret.output.contains("error: could not apply", Qt::CaseInsensitive)
             || ret.output.contains(" conflict", Qt::CaseInsensitive))
         {
            emit signalCherryPickConflict(QStringList());
         }
         else
         {
            error = true;
            errorStr = ret.output;
         }
      }
   }
   else
   {
      const auto lastShaBeforeCommit = mGit->getLastCommit().output.trimmed();
      const auto git = QScopedPointer<GitLocal>(new GitLocal(mGit));
      const auto ret = git->cherryPickCommit(mSearchInput->text());

      if (ret.success)
      {
         mSearchInput->clear();

         commit.sha = mGit->getLastCommit().output.trimmed();

         mCache->insertCommit(commit);
         mCache->deleteReference(lastShaBeforeCommit, References::Type::LocalBranch, mGit->getCurrentBranch());
         mCache->insertReference(commit.sha, References::Type::LocalBranch, mGit->getCurrentBranch());

         QScopedPointer<GitHistory> gitHistory(new GitHistory(mGit));
         const auto ret = gitHistory->getDiffFiles(commit.sha, lastShaBeforeCommit);

         mCache->insertRevisionFiles(commit.sha, lastShaBeforeCommit, RevisionFiles(ret.output));

         emit mCache->signalCacheUpdated();

         emit logReload();
      }
      else
      {
         error = true;
         errorStr = ret.output;
      }
   }

   if (error)
   {
      QMessageBox msgBox(QMessageBox::Critical, tr("Error while cherry-pick"),
                         tr("There were problems during the cherry-pick operation. Please, see the detailed "
                            "description for more information."),
                         QMessageBox::Ok, this);
      msgBox.setDetailedText(errorStr);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
}

void HistoryWidget::showWipFileDiff(const QString &fileName, bool isCached)
{
   mWipFileDiff->setup(fileName, isCached);
   mCenterStackedWidget->setCurrentIndex(static_cast<int>(Pages::FileDiff));
   mBranchesWidget->forceMinimalView();
}
