#include "GitLocal.h"

#include <GitBase.h>

#include <QLogger.h>
#include <QBenchmark.h>

using namespace QLogger;
using namespace QBenchmark;

namespace
{
static QString quote(const QStringList &sl)
{
   QString q(sl.join(QString("$%1$").arg(' ')));
   q.prepend("$").append("$");
   return q;
}
}

GitLocal::GitLocal(const QSharedPointer<GitBase> &gitBase)
   : QObject()
   , mGitBase(gitBase)
{
}

GitExecResult GitLocal::cherryPickCommit(const QString &sha) const
{
   QBenchmarkStart();

   QLog_Debug("Git", QString("Executing cherryPickCommit: {%1}").arg(sha));

   const auto ret = mGitBase->run(QString("git cherry-pick %1").arg(sha));

   QBenchmarkEnd();

   return ret;
}

GitExecResult GitLocal::cherryPickAbort() const
{
   QBenchmarkStart();

   QLog_Debug("Git", QString("Aborting cherryPick"));

   const auto ret = mGitBase->run("git cherry-pick --abort");

   QBenchmarkEnd();

   return ret;
}

GitExecResult GitLocal::cherryPickContinue() const
{
   QBenchmarkStart();

   QLog_Debug("Git", QString("Applying cherryPick"));

   const auto ret = mGitBase->run("git cherry-pick --continue");

   QBenchmarkEnd();

   return ret;
}

GitExecResult GitLocal::checkoutCommit(const QString &sha) const
{
   QBenchmarkStart();

   QLog_Debug("Git", QString("Executing checkoutCommit: {%1}").arg(sha));

   const auto ret = mGitBase->run(QString("git checkout %1").arg(sha));

   if (ret.success)
      mGitBase->updateCurrentBranch();

   QBenchmarkEnd();

   return ret;
}

GitExecResult GitLocal::markFileAsResolved(const QString &fileName) const
{
   QBenchmarkStart();

   QLog_Debug("Git", QString("Executing markFileAsResolved: {%1}").arg(fileName));

   const auto ret = mGitBase->run(QString("git add %1").arg(fileName));

   if (ret.success)
      emit signalWipUpdated();

   QBenchmarkEnd();

   return ret;
}

bool GitLocal::checkoutFile(const QString &fileName) const
{
   if (fileName.isEmpty())
   {
      QLog_Warning("Git", QString("Executing checkoutFile with an empty file.").arg(fileName));

      return false;
   }

   QBenchmarkStart();

   QLog_Debug("Git", QString("Executing checkoutFile: {%1}").arg(fileName));

   const auto ret = mGitBase->run(QString("git checkout %1").arg(fileName)).success;

   QBenchmarkEnd();

   return ret;
}

GitExecResult GitLocal::resetFile(const QString &fileName) const
{
   QBenchmarkStart();

   QLog_Debug("Git", QString("Executing resetFile: {%1}").arg(fileName));

   const auto ret = mGitBase->run(QString("git reset %1").arg(fileName));

   QBenchmarkEnd();

   return ret;
}

bool GitLocal::resetCommit(const QString &sha, CommitResetType type) const
{
   QBenchmarkStart();

   QString typeStr;

   switch (type)
   {
      case CommitResetType::SOFT:
         typeStr = "soft";
         break;
      case CommitResetType::MIXED:
         typeStr = "mixed";
         break;
      case CommitResetType::HARD:
         typeStr = "hard";
         break;
   }

   QLog_Debug("Git", QString("Executing resetCommit: {%1} type {%2}").arg(sha, typeStr));

   const auto ret = mGitBase->run(QString("git reset --%1 %2").arg(typeStr, sha));

   if (ret.success)
      emit signalWipUpdated();

   QBenchmarkEnd();

   return ret.success;
}

GitExecResult GitLocal::commitFiles(QStringList &selFiles, const RevisionFiles &allCommitFiles,
                                    const QString &msg) const
{
   QBenchmarkStart();

   QStringList notSel;

   for (auto i = 0; i < allCommitFiles.count(); ++i)
   {
      const QString &fp = allCommitFiles.getFile(i);
      if (selFiles.indexOf(fp) == -1 && allCommitFiles.statusCmp(i, RevisionFiles::IN_INDEX))
         notSel.append(fp);
   }

   if (!notSel.empty())
   {
      const auto ret = mGitBase->run("git reset -- " + quote(notSel));

      if (!ret.success)
      {
         QBenchmarkEnd();
         return ret;
      }
   }

   // call git reset to remove not selected files from index
   const auto updIdx = updateIndex(allCommitFiles, selFiles);

   if (!updIdx.success)
   {
      QBenchmarkEnd();
      return updIdx;
   }

   QLog_Debug("Git", QString("Commiting files"));

   const auto ret = mGitBase->run(QString("git commit -m \"%1\"").arg(msg));

   QBenchmarkEnd();

   return ret;
}

GitExecResult GitLocal::ammendCommit(const QStringList &selFiles, const RevisionFiles &allCommitFiles,
                                     const QString &msg, const QString &author) const
{
   QBenchmarkStart();

   QStringList notSel;

   for (auto i = 0; i < allCommitFiles.count(); ++i)
   {
      const QString &fp = allCommitFiles.getFile(i);
      if (selFiles.indexOf(fp) == -1 && allCommitFiles.statusCmp(i, RevisionFiles::IN_INDEX))
         notSel.append(fp);
   }

   if (!notSel.empty())
   {
      const auto ret = mGitBase->run("git reset -- " + quote(notSel));

      if (!ret.success)
      {
         QBenchmarkEnd();
         return ret;
      }
   }

   // call git reset to remove not selected files from index
   const auto updIdx = updateIndex(allCommitFiles, selFiles);

   if (!updIdx.success)
   {
      QBenchmarkEnd();
      return updIdx;
   }

   QLog_Debug("Git", QString("Amending files"));

   QString cmtOptions;

   if (!author.isEmpty())
      cmtOptions.append(QString(" --author \"%1\"").arg(author));

   const auto ret = mGitBase->run(QString("git commit --amend" + cmtOptions + " -m \"%1\"").arg(msg));

   QBenchmarkEnd();

   return ret;
}

GitExecResult GitLocal::updateIndex(const RevisionFiles &files, const QStringList &selFiles) const
{
   QBenchmarkStart();

   QStringList toAdd, toRemove;

   for (const auto &file : selFiles)
   {
      const auto index = files.mFiles.indexOf(file);

      if (index != -1 && files.statusCmp(index, RevisionFiles::DELETED))
         toRemove << file;
      else
         toAdd << file;
   }

   if (!toRemove.isEmpty())
   {
      const auto ret = mGitBase->run("git rm --cached --ignore-unmatch -- " + quote(toRemove));

      if (!ret.success)
      {
         QBenchmarkEnd();
         return ret;
      }
   }

   if (!toAdd.isEmpty())
   {
      const auto ret = mGitBase->run("git add -- " + quote(toAdd));

      if (!ret.success)
      {
         QBenchmarkEnd();
         return ret;
      }
   }

   const auto ret = GitExecResult(true, "Indexes updated");

   QBenchmarkEnd();

   return ret;
}
