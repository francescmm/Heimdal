#include "CommitHistoryContextMenu.h"

#include <BranchDlg.h>
#include <CommitInfo.h>
#include <GitBase.h>
#include <GitBranches.h>
#include <GitCache.h>
#include <GitConfig.h>
#include <GitHistory.h>
#include <GitLocal.h>
#include <GitMerge.h>
#include <GitPatches.h>
#include <GitQlientSettings.h>
#include <GitQlientStyles.h>
#include <GitRemote.h>
#include <GitStashes.h>
#include <PullDlg.h>
#include <SquashDlg.h>
#include <TagDlg.h>
#include <WipHelper.h>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>

#include <QLogger.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#   include <QRegularExpression>
#endif

using namespace QLogger;

CommitHistoryContextMenu::CommitHistoryContextMenu(const QSharedPointer<GitCache> &cache,
                                                   const QSharedPointer<GitBase> &git, const QStringList &shas,
                                                   QWidget *parent)
   : QMenu(parent)
   , mCache(cache)
   , mGit(git)
   , mShas(shas)
{
   setAttribute(Qt::WA_DeleteOnClose);

   if (shas.count() == 1)
      createIndividualShaMenu();
   else
      createMultipleShasMenu();
}

void CommitHistoryContextMenu::createIndividualShaMenu()
{
   const auto singleSelection = mShas.count() == 1;

   if (singleSelection)
   {
      const auto sha = mShas.first();

      if (sha == ZERO_SHA)
      {
         const auto stashMenu = addMenu(tr("Stash"));
         const auto stashAction = stashMenu->addAction(tr("Push"));
         connect(stashAction, &QAction::triggered, this, &CommitHistoryContextMenu::stashPush);

         const auto popAction = stashMenu->addAction(tr("Pop"));
         connect(popAction, &QAction::triggered, this, &CommitHistoryContextMenu::stashPop);
      }
      else
      {
         const auto commitAction = addAction(tr("Show diff"));
         connect(commitAction, &QAction::triggered, this, [this]() { emit signalOpenDiff(mShas.first()); });

         const auto createMenu = addMenu(tr("Create"));

         const auto createBranchAction = createMenu->addAction(tr("Branch"));
         connect(createBranchAction, &QAction::triggered, this, &CommitHistoryContextMenu::createBranch);

         const auto createTagAction = createMenu->addAction(tr("Tag"));
         connect(createTagAction, &QAction::triggered, this, &CommitHistoryContextMenu::createTag);

         const auto exportAsPatchAction = addAction(tr("Export as patch"));
         connect(exportAsPatchAction, &QAction::triggered, this, &CommitHistoryContextMenu::exportAsPatch);

         addSeparator();

         const auto checkoutCommitAction = addAction(tr("Checkout commit"));
         connect(checkoutCommitAction, &QAction::triggered, this, &CommitHistoryContextMenu::checkoutCommit);

         addBranchActions(sha);

         QScopedPointer<GitBranches> git(new GitBranches(mGit));

         if (auto ret = git->getLastCommitOfBranch(mGit->getCurrentBranch()); ret.success)
         {
            const auto lastShaStr = ret.output.trimmed();

            if (lastShaStr == sha)
            {
               const auto amendCommitAction = addAction(tr("Amend (edit last commit)"));
               amendCommitAction->setToolTip(tr("Edit the last commit of the branch."));
               connect(amendCommitAction, &QAction::triggered, this,
                       [this]() { emit signalAmendCommit(mShas.first()); });

               const auto amendNoEditCommitAction = addAction(tr("Amend without edit"));
               amendNoEditCommitAction->setToolTip(tr("Edit the last commit of the branch."));
               connect(amendNoEditCommitAction, &QAction::triggered, this, &CommitHistoryContextMenu::amendNoEdit);

               const auto applyMenu = addMenu(tr("Apply"));

               const auto applyPatchAction = applyMenu->addAction(tr("Patch"));
               connect(applyPatchAction, &QAction::triggered, this, &CommitHistoryContextMenu::applyPatch);

               const auto applyCommitAction = applyMenu->addAction(tr("Commit"));
               connect(applyCommitAction, &QAction::triggered, this, &CommitHistoryContextMenu::applyCommit);

               const auto pushAction = addAction(tr("Push"));
               connect(pushAction, &QAction::triggered, this, &CommitHistoryContextMenu::push);

               const auto pullAction = addAction(tr("Pull"));
               connect(pullAction, &QAction::triggered, this, &CommitHistoryContextMenu::pull);

               const auto fetchAction = addAction(tr("Fetch"));
               connect(fetchAction, &QAction::triggered, this, &CommitHistoryContextMenu::fetch);

               const auto revertCommitAction = addAction(tr("Revert commit"));
               revertCommitAction->setToolTip(tr("Reverts the selected commit of the branch."));
               connect(revertCommitAction, &QAction::triggered, this, &CommitHistoryContextMenu::revertCommit);
            }
            else if (git->isCommitInCurrentGeneologyTree(mShas.first()))
            {
               const auto pushAction = addAction(tr("Push"));
               connect(pushAction, &QAction::triggered, this, &CommitHistoryContextMenu::push);

               const auto revertCommitAction = addAction(tr("Revert commit"));
               revertCommitAction->setToolTip(tr("Reverts the selected commit of the branch."));
               connect(revertCommitAction, &QAction::triggered, this, &CommitHistoryContextMenu::revertCommit);
            }
         }

         const auto resetMenu = addMenu(tr("Reset"));

         const auto resetSoftAction = resetMenu->addAction(tr("Soft (keep changes)"));
         resetSoftAction->setToolTip(tr("Point to the selected commit <strong>keeping all changes</strong>."));
         connect(resetSoftAction, &QAction::triggered, this, &CommitHistoryContextMenu::resetSoft);

         const auto resetMixedAction = resetMenu->addAction(tr("Mixed (keep files, reset their changes)"));
         resetMixedAction->setToolTip(
             tr("Point to the selected commit <strong>keeping all changes but resetting the file status<strong>."));
         connect(resetMixedAction, &QAction::triggered, this, &CommitHistoryContextMenu::resetMixed);

         const auto resetHardAction = resetMenu->addAction(tr("Hard (discard chanbges)"));
         resetHardAction->setToolTip(tr("Point to the selected commit <strong>losing all changes<strong>."));
         connect(resetHardAction, &QAction::triggered, this, &CommitHistoryContextMenu::resetHard);

         addSeparator();

         const auto copyMenu = addMenu(tr("Copy"));

         const auto copyShaAction = copyMenu->addAction(tr("Commit SHA"));
         connect(copyShaAction, &QAction::triggered, this,
                 [this]() { QApplication::clipboard()->setText(mShas.first()); });

         const auto copyTitleAction = copyMenu->addAction(tr("Commit title"));
         connect(copyTitleAction, &QAction::triggered, this, [this]() {
            const auto title = mCache->commitInfo(mShas.first()).shortLog;
            QApplication::clipboard()->setText(title);
         });
      }
   }
}

void CommitHistoryContextMenu::createMultipleShasMenu()
{
   if (!mShas.contains(ZERO_SHA))
   {
      const auto exportAsPatchAction = addAction(tr("Export as patch"));
      connect(exportAsPatchAction, &QAction::triggered, this, &CommitHistoryContextMenu::exportAsPatch);

      const auto copyShaAction = addAction(tr("Copy all SHA"));
      connect(copyShaAction, &QAction::triggered, this,
              [this]() { QApplication::clipboard()->setText(mShas.join(',')); });

      auto shasInCurrenTree = 0;
      QScopedPointer<GitBranches> git(new GitBranches(mGit));

      for (const auto &sha : std::as_const(mShas))
         shasInCurrenTree += git->isCommitInCurrentGeneologyTree(sha);

      if (shasInCurrenTree == 0)
      {
         const auto cherryPickAction = addAction(tr("Cherry pick ALL commits"));
         connect(cherryPickAction, &QAction::triggered, this, &CommitHistoryContextMenu::cherryPickCommit);
      }
      else if (shasInCurrenTree == mShas.count())
      {
         const auto cherryPickAction = addAction(tr("Squash commits"));
         connect(cherryPickAction, &QAction::triggered, this, &CommitHistoryContextMenu::showSquashDialog);
      }
   }
   else
      QLog_Warning("UI", "WIP selected as part of a series of SHAs");
}

void CommitHistoryContextMenu::stashPush()
{
   QScopedPointer<GitStashes> git(new GitStashes(mGit));
   const auto ret = git->stash();

   if (ret.success)
      emit logReload();
}

void CommitHistoryContextMenu::stashPop()
{
   QScopedPointer<GitStashes> git(new GitStashes(mGit));
   const auto ret = git->pop();

   if (ret.success)
      emit logReload();
}

void CommitHistoryContextMenu::createBranch()
{
   BranchDlg dlg({ mShas.first(), BranchDlgMode::CREATE_FROM_COMMIT, mCache, mGit });
   dlg.exec();
}

void CommitHistoryContextMenu::createTag()
{
   TagDlg dlg(QSharedPointer<GitBase>::create(mGit->getWorkingDir()), mShas.first());
   const auto ret = dlg.exec();

   if (ret == QDialog::Accepted)
      emit referencesReload(); // TODO: Optimize
}

void CommitHistoryContextMenu::exportAsPatch()
{
   QScopedPointer<GitPatches> git(new GitPatches(mGit));
   const auto ret = git->exportPatch(mShas);

   if (ret.success)
   {
      const auto action = QMessageBox::information(parentWidget(), tr("Patch generated"),
                                                   tr("<p>The patch has been generated!</p>"
                                                      "<p><b>Commit:</b></p><p>%1</p>"
                                                      "<p><b>Destination:</b> %2</p>"
                                                      "<p><b>File names:</b></p><p>%3</p>")
                                                       .arg(mShas.join("<br>"), mGit->getWorkingDir(), ret.output),
                                                   QMessageBox::Ok, QMessageBox::Open);

      if (action == QMessageBox::Open)
      {
         QString fileBrowser;

#ifdef Q_OS_LINUX
         fileBrowser.append("xdg-open");
#elif defined(Q_OS_WIN)
         fileBrowser.append("explorer.exe");
#endif

         QProcess::startDetached(fileBrowser, { mGit->getWorkingDir() });
      }
   }
}

void CommitHistoryContextMenu::checkoutBranch()
{
   const auto action = qobject_cast<QAction *>(sender());
   auto isLocal = action->data().toBool();
   auto branchName = action->text();

   if (isLocal)
      branchName.remove("origin/");

   QScopedPointer<GitBranches> git(new GitBranches(mGit));
   const auto ret = isLocal ? git->checkoutLocalBranch(branchName) : git->checkoutRemoteBranch(branchName);
   const auto output = ret.output;

   if (ret.success)
   {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
      static QRegExp rx("by \\d+ commits");
      rx.indexIn(ret.output);
      auto value = rx.capturedTexts().constFirst().split(" ");
#else
      static QRegularExpression rx("by \\d+ commits");
      const auto texts = rx.match(ret.output).capturedTexts();
      auto value = texts.isEmpty() ? QStringList() : texts.constFirst().split(" ");
#endif

      if (value.count() == 3 && output.contains("your branch is behind", Qt::CaseInsensitive))
      {
         const auto commits = value.at(1).toUInt();
         (void)commits;

         PullDlg pull(mGit, output.split('\n').first());
         connect(&pull, &PullDlg::signalRepositoryUpdated, this, &CommitHistoryContextMenu::fullReload);
         connect(&pull, &PullDlg::signalPullConflict, this, &CommitHistoryContextMenu::signalPullConflict);
      }

      emit logReload();
   }
   else
   {
      QMessageBox msgBox(QMessageBox::Critical, tr("Error while checking out"),
                         tr("There were problems during the checkout operation. Please, see the detailed "
                            "description for more information."),
                         QMessageBox::Ok, parentWidget());
      msgBox.setDetailedText(ret.output);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
}

void CommitHistoryContextMenu::createCheckoutBranch()
{
   BranchDlg dlg({ mShas.constFirst(), BranchDlgMode::CREATE_CHECKOUT_FROM_COMMIT, mCache, mGit });

   if (dlg.exec() == QDialog::Accepted)
      emit logReload();
}

void CommitHistoryContextMenu::checkoutCommit()
{
   const auto sha = mShas.first();
   QLog_Info("UI", QString("Checking out the commit {%1}").arg(sha));

   QScopedPointer<GitLocal> git(new GitLocal(mGit));
   const auto ret = git->checkoutCommit(sha);

   if (ret.success)
      emit logReload();
   else
   {
      QMessageBox msgBox(QMessageBox::Critical, tr("Error while checking out"),
                         tr("There were problems during the checkout operation. Please, see the detailed "
                            "description for more information."),
                         QMessageBox::Ok, parentWidget());
      msgBox.setDetailedText(ret.output);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
}

void CommitHistoryContextMenu::cherryPickCommit()
{
   auto shas = mShas;
   for (const auto &sha : std::as_const(mShas))
   {
      const auto lastShaBeforeCommit = mGit->getLastCommit().output.trimmed();
      QScopedPointer<GitLocal> git(new GitLocal(mGit));
      const auto ret = git->cherryPickCommit(sha);

      shas.takeFirst();

      if (ret.success && shas.isEmpty())
      {
         auto commit = mCache->commitInfo(sha);
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
      else if (!ret.success)
      {
         const auto errorMsg = ret.output;

         if (errorMsg.contains("error: could not apply", Qt::CaseInsensitive)
             && errorMsg.contains("after resolving the conflicts", Qt::CaseInsensitive))
         {
            emit signalCherryPickConflict(shas);
         }
         else
         {
            QMessageBox msgBox(QMessageBox::Critical, tr("Error while cherry-pick"),
                               tr("There were problems during the cherry-pich operation. Please, see the detailed "
                                  "description for more information."),
                               QMessageBox::Ok, parentWidget());
            msgBox.setDetailedText(errorMsg);
            msgBox.setStyleSheet(GitQlientStyles::getStyles());
            msgBox.exec();
         }
      }
   }
}

void CommitHistoryContextMenu::rebase()
{
   const auto action = qobject_cast<QAction *>(sender());
   const auto branchToRebase = action->data().toString();

   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitMerge> git(new GitMerge(mGit));
   const auto ret = git->rebase(branchToRebase);
   QApplication::restoreOverrideCursor();

   if (ret.success)
   {
      if (!ret.output.isEmpty())
      {
         if (ret.output.contains("error: could not apply", Qt::CaseInsensitive)
             || ret.output.contains(" conflict", Qt::CaseInsensitive))
         {
            QMessageBox msgBox(
                QMessageBox::Warning, tr("Rebase status"),
                QString(tr(
                    "There were problems during the rebase. Please, see the detailed description for more "
                    "information.<br><br>GitQlient cannot handle these conflicts at the moment.<br><br>The rebase will "
                    "be aborted.")),
                QMessageBox::Ok, parentWidget());
            msgBox.setDetailedText(ret.output);
            msgBox.setStyleSheet(GitQlientStyles::getStyles());
            msgBox.exec();

            git->rebaseAbort();

            // TODO: For future implementation of manage rebase conflics
            // emit signalRebaseConflict();
         }
      }
      else
      {
         WipHelper::update(mGit, mCache);

         emit fullReload();
      }
   }
   else
   {
      QMessageBox msgBox(
          QMessageBox::Critical, tr("Rebase failed"),
          QString(
              tr("There were problems during the rebase. Please, see the detailed description for more "
                 "information.<br><br>GitQlient cannot handle these conflicts at the moment.<br><br>The rebase will be "
                 "aborted.")),
          QMessageBox::Ok, parentWidget());
      msgBox.setDetailedText(ret.output);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();

      git->rebaseAbort();

      // TODO: For future implementation of manage rebase conflics
      // emit signalRebaseConflict();
   }
}

void CommitHistoryContextMenu::applyPatch()
{
   const QString fileName(QFileDialog::getOpenFileName(parentWidget(), tr("Select a patch to apply")));
   QScopedPointer<GitPatches> git(new GitPatches(mGit));

   if (!fileName.isEmpty() && git->applyPatch(fileName).success)
      emit logReload();
}

void CommitHistoryContextMenu::applyCommit()
{
   const QString fileName(QFileDialog::getOpenFileName(parentWidget(), "Select a patch to apply"));
   QScopedPointer<GitPatches> git(new GitPatches(mGit));

   if (!fileName.isEmpty() && git->applyPatch(fileName, true).success)
      emit logReload();
}

void CommitHistoryContextMenu::push()
{
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto ret = git->pushCommit(mShas.first(), mGit->getCurrentBranch());
   QApplication::restoreOverrideCursor();

   if (ret.output.contains("has no upstream branch"))
   {
      const auto currentBranch = mGit->getCurrentBranch();
      BranchDlg dlg({ currentBranch, BranchDlgMode::PUSH_UPSTREAM, mCache, mGit });
      const auto ret = dlg.exec();

      if (ret == QDialog::Accepted)
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
      QMessageBox msgBox(QMessageBox::Critical, tr("Error while pushing"),
                         tr("There were problems during the push operation. Please, see the detailed description "
                            "for more information."),
                         QMessageBox::Ok, parentWidget());
      msgBox.setDetailedText(ret.output);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
}

void CommitHistoryContextMenu::pull()
{
   GitQlientSettings settings(mGit->getGitDir());
   const auto updateOnPull = settings.localValue("UpdateOnPull", true).toBool();

   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto ret = git->pull(updateOnPull);
   QApplication::restoreOverrideCursor();

   if (ret.success)
      emit fullReload();
   else
   {
      const auto errorMsg = ret.output;

      if (errorMsg.contains("error: could not apply", Qt::CaseInsensitive)
          && errorMsg.contains("causing a conflict", Qt::CaseInsensitive))
      {
         emit signalPullConflict();
      }
      else
      {
         QMessageBox msgBox(QMessageBox::Critical, tr("Error while pulling"),
                            tr("There were problems during the pull operation. Please, see the detailed "
                               "description for more information."),
                            QMessageBox::Ok, parentWidget());
         msgBox.setDetailedText(errorMsg);
         msgBox.setStyleSheet(GitQlientStyles::getStyles());
         msgBox.exec();
      }
   }
}

void CommitHistoryContextMenu::fetch()
{
   GitQlientSettings settings(mGit->getGitDir());
   const auto pruneOnFetch = settings.localValue("PruneOnFetch", true).toBool();

   QScopedPointer<GitRemote> git(new GitRemote(mGit));

   if (git->fetch(pruneOnFetch))
      emit fullReload();
}

void CommitHistoryContextMenu::revertCommit()
{
   QScopedPointer<GitLocal> git(new GitLocal(mGit));
   const auto previousSha = mGit->getLastCommit().output.trimmed();

   if (git->revert(mShas.first()).success)
   {
      const auto revertedCommit = mCache->commitInfo(mShas.constFirst());
      const auto currentSha = mGit->getLastCommit().output.trimmed();
      QScopedPointer<GitConfig> gitConfig(new GitConfig(mGit));
      auto committer = gitConfig->getLocalUserInfo();

      if (committer.mUserEmail.isEmpty() || committer.mUserName.isEmpty())
         committer = gitConfig->getGlobalUserInfo();

      CommitInfo newCommit { currentSha,
                             { previousSha },
                             std::chrono::seconds(QDateTime::currentDateTime().toSecsSinceEpoch()),
                             tr("Revert \"%1\"").arg(revertedCommit.shortLog) };

      newCommit.committer = QString("%1<%2>").arg(committer.mUserName, committer.mUserEmail);
      newCommit.author = QString("%1<%2>").arg(committer.mUserName, committer.mUserEmail);
      newCommit.longLog = QString("%1\n\n%2 %3")
                              .arg(newCommit.shortLog, QString::fromUtf8("This reverts commit"), revertedCommit.sha);

      mCache->insertCommit(newCommit);
      mCache->deleteReference(previousSha, References::Type::LocalBranch, mGit->getCurrentBranch());
      mCache->insertReference(currentSha, References::Type::LocalBranch, mGit->getCurrentBranch());

      QScopedPointer<GitHistory> gitHistory(new GitHistory(mGit));
      const auto ret = gitHistory->getDiffFiles(currentSha, previousSha);

      mCache->insertRevisionFiles(currentSha, previousSha, RevisionFiles(ret.output));

      WipHelper::update(mGit, mCache);

      emit mCache->signalCacheUpdated();
      emit logReload();
   }
}

void CommitHistoryContextMenu::resetSoft()
{
   QScopedPointer<GitLocal> git(new GitLocal(mGit));
   const auto previousSha = mGit->getLastCommit().output.trimmed();

   if (git->resetCommit(mShas.first(), GitLocal::CommitResetType::SOFT))
   {
      mCache->deleteReference(previousSha, References::Type::LocalBranch, mGit->getCurrentBranch());
      mCache->insertReference(mShas.first(), References::Type::LocalBranch, mGit->getCurrentBranch());

      emit logReload();
   }
}

void CommitHistoryContextMenu::resetMixed()
{
   QScopedPointer<GitLocal> git(new GitLocal(mGit));
   const auto previousSha = mGit->getLastCommit().output.trimmed();

   if (git->resetCommit(mShas.first(), GitLocal::CommitResetType::MIXED))
   {
      mCache->deleteReference(previousSha, References::Type::LocalBranch, mGit->getCurrentBranch());
      mCache->insertReference(mShas.first(), References::Type::LocalBranch, mGit->getCurrentBranch());

      emit logReload();
   }
}

void CommitHistoryContextMenu::resetHard()
{
   const auto retMsg = QMessageBox::warning(
       parentWidget(), "Reset hard requested!", "Are you sure you want to reset the branch to this commit in a <b>hard</b> way?",
       QMessageBox::Ok, QMessageBox::Cancel);

   if (retMsg == QMessageBox::Ok)
   {
      const auto previousSha = mGit->getLastCommit().output.trimmed();
      QScopedPointer<GitLocal> git(new GitLocal(mGit));

      if (git->resetCommit(mShas.first(), GitLocal::CommitResetType::HARD))
      {
         mCache->deleteReference(previousSha, References::Type::LocalBranch, mGit->getCurrentBranch());
         mCache->insertReference(mShas.first(), References::Type::LocalBranch, mGit->getCurrentBranch());

         emit logReload();
      }
   }
}

void CommitHistoryContextMenu::merge()
{
   const auto action = qobject_cast<QAction *>(sender());
   const auto fromBranch = action->data().toString();

   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto currentBranch = mGit->getCurrentBranch();

   emit signalMergeRequired(currentBranch, fromBranch);
}

void CommitHistoryContextMenu::mergeSquash()
{
   const auto action = qobject_cast<QAction *>(sender());
   const auto fromBranch = action->data().toString();

   QScopedPointer<GitRemote> git(new GitRemote(mGit));
   const auto currentBranch = mGit->getCurrentBranch();

   emit mergeSqushRequested(currentBranch, fromBranch);
}

void CommitHistoryContextMenu::addBranchActions(const QString &sha)
{
   auto remoteBranches = mCache->getReferences(sha, References::Type::RemoteBranches);
   const auto localBranches = mCache->getReferences(sha, References::Type::LocalBranch);

   QMap<QString, bool> branchTracking;

   if (remoteBranches.isEmpty())
   {
      for (const auto &branch : localBranches)
         branchTracking.insert(branch, true);
   }
   else
   {
      for (const auto &local : localBranches)
      {
         const auto iter = std::find_if(remoteBranches.begin(), remoteBranches.end(), [local](const QString &remote) {
            if (remote.contains(local))
               return true;
            return false;
         });

         branchTracking.insert(local, true);

         if (iter != remoteBranches.end())
            remoteBranches.erase(iter);
      }
      for (const auto &remote : remoteBranches)
         branchTracking.insert(remote, false);
   }

   QList<QAction *> branchesToCheckout;
   const auto currentBranch = mGit->getCurrentBranch();

   for (const auto &pair : branchTracking.toStdMap())
   {
      if (!branchTracking.isEmpty() && pair.first != currentBranch
          && pair.first != QString("origin/%1").arg(currentBranch))
      {
         const auto checkoutCommitAction = new QAction(QString(tr("%1")).arg(pair.first));
         checkoutCommitAction->setData(pair.second);
         connect(checkoutCommitAction, &QAction::triggered, this, &CommitHistoryContextMenu::checkoutBranch);
         branchesToCheckout.append(checkoutCommitAction);
      }
   }

   const auto branchMenu = !branchesToCheckout.isEmpty() ? addMenu(tr("Checkout branch")) : this;
   const auto newBranchAction
       = branchMenu->addAction(!branchesToCheckout.isEmpty() ? tr("New Branch") : tr("Checkout new branch"));
   connect(newBranchAction, &QAction::triggered, this, &CommitHistoryContextMenu::createCheckoutBranch);

   if (!branchesToCheckout.isEmpty())
   {
      branchMenu->addSeparator();
      branchMenu->addActions(branchesToCheckout);
   }

   if (QScopedPointer<GitBranches> git(new GitBranches(mGit)); !git->isCommitInCurrentGeneologyTree(sha))
   {
      const auto rebaseMenu = !branchTracking.isEmpty() ? addMenu(tr("Rebase")) : this;
      const auto mergeMenu = !branchTracking.isEmpty() ? addMenu(tr("Merge")) : this;
      const auto squashMergeMenu = !branchTracking.isEmpty() ? addMenu(tr("Squash-merge")) : this;

      for (const auto &pair : branchTracking.toStdMap())
      {
         if (!pair.first.isEmpty() && pair.first != currentBranch
             && pair.first != QString("origin/%1").arg(currentBranch))
         {
            // If is the last commit of a branch
            const auto rebaseBranchAction = rebaseMenu->addAction(QString(tr("%1")).arg(pair.first));
            rebaseBranchAction->setData(pair.first);
            connect(rebaseBranchAction, &QAction::triggered, this, &CommitHistoryContextMenu::rebase);

            const auto mergeBranchAction = mergeMenu->addAction(QString(tr("%1")).arg(pair.first));
            mergeBranchAction->setData(pair.first);
            connect(mergeBranchAction, &QAction::triggered, this, &CommitHistoryContextMenu::merge);

            const auto mergeSquashBranchAction = squashMergeMenu->addAction(QString(tr("%1")).arg(pair.first));
            mergeSquashBranchAction->setData(pair.first);
            connect(mergeSquashBranchAction, &QAction::triggered, this, &CommitHistoryContextMenu::mergeSquash);
         }
      }

      addSeparator();

      const auto cherryPickAction = addAction(tr("Cherry pick commit"));
      connect(cherryPickAction, &QAction::triggered, this, &CommitHistoryContextMenu::cherryPickCommit);
   }
   else
      addSeparator();
}

void CommitHistoryContextMenu::showSquashDialog()
{
   if (mCache->pendingLocalChanges())
   {
      QMessageBox::warning(parentWidget(), tr("Squash not possible"),
                           tr("Please, make sure there are no pending changes to be committed."));
   }
   else
   {
      const auto squash = new SquashDlg(mGit, mCache, mShas, this);
      connect(squash, &SquashDlg::changesCommitted, this, &CommitHistoryContextMenu::fullReload);
      squash->exec();
   }
}

void CommitHistoryContextMenu::amendNoEdit()
{
   QScopedPointer<GitLocal> git(new GitLocal(mGit));
   const auto ret = git->amend();
   emit logReload();

   if (ret.success)
   {
      const auto newSha = mGit->getLastCommit().output.trimmed();
      auto commit = mCache->commitInfo(mShas.first());
      const auto oldSha = commit.sha;
      commit.sha = newSha;

      mCache->updateCommit(oldSha, std::move(commit));

      QScopedPointer<GitHistory> git(new GitHistory(mGit));
      const auto ret = git->getDiffFiles(mShas.first(), commit.firstParent());

      mCache->insertRevisionFiles(mShas.first(), commit.firstParent(), RevisionFiles(ret.output));
   }
   else
   {
      QMessageBox msgBox(QMessageBox::Critical, tr("Error when amending"),
                         tr("There were problems during the commit "
                            "operation. Please, see the detailed "
                            "description for more information."),
                         QMessageBox::Ok, parentWidget());
      msgBox.setDetailedText(ret.output);
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
}
