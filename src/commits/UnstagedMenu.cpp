#include "UnstagedMenu.h"

#include <GitBase.h>
#include <GitLocal.h>
#include <GitQlientSettings.h>
#include <GitSyncProcess.h>
#include <QLogger.h>

#include <QDir>
#include <QFile>
#include <QMessageBox>

using namespace QLogger;

UnstagedMenu::UnstagedMenu(const QSharedPointer<GitBase> &git, const QString &fileName, QWidget *parent)
   : QMenu(parent)
   , mGit(git)
   , mFileName(fileName)
{
   setAttribute(Qt::WA_DeleteOnClose);

   connect(addAction(tr("See changes")), &QAction::triggered, this, [this]() { emit signalShowDiff(mFileName); });
   connect(addAction(tr("Blame")), &QAction::triggered, this, [this]() { emit signalShowFileHistory(mFileName); });
   connect(addAction(tr("Open containing folder")), &QAction::triggered, this, &UnstagedMenu::openFileExplorer);

   addSeparator();

   connect(addAction(tr("Stage file")), &QAction::triggered, this, &UnstagedMenu::signalStageFile);

   connect(addAction(tr("Revert file changes")), &QAction::triggered, this, [this]() {
      const auto msgBoxRet
          = QMessageBox::question(this, tr("Ignoring file"), tr("Are you sure you want to revert the changes?"));

      if (msgBoxRet == QMessageBox::Yes)
      {
         QScopedPointer<GitLocal> git(new GitLocal(mGit));
         const auto ret = git->checkoutFile(mFileName);

         if (ret)
            emit changeReverted(mFileName);
      }
   });

   addSeparator();

   const auto ignoreMenu = addMenu(tr("Ignore"));

   connect(ignoreMenu->addAction(tr("Ignore file")), &QAction::triggered, this, [this]() {
      const auto ret = QMessageBox::question(this, tr("Ignoring file"),
                                             tr("Are you sure you want to add the file to the black list?"));

      if (ret == QMessageBox::Yes)
      {
         const auto gitRet = addEntryToGitIgnore(mFileName);

         if (gitRet)
            emit signalCheckedOut();
      }
   });

   connect(addAction(tr("Delete file")), &QAction::triggered, this, &UnstagedMenu::onDeleteFile);

   connect(addAction(tr("Delete ALL untracked files")), &QAction::triggered, this, &UnstagedMenu::deleteUntracked);

   connect(ignoreMenu->addAction(tr("Ignore extension")), &QAction::triggered, this, [this]() {
      const auto msgBoxRet = QMessageBox::question(
          this, tr("Ignoring extension"), tr("Are you sure you want to add the file extension to the black list?"));

      if (msgBoxRet == QMessageBox::Yes)
      {
         auto fileParts = mFileName.split(".");
         fileParts.takeFirst();
         const auto extension = QString("*.%1").arg(fileParts.join("."));
         const auto ret = addEntryToGitIgnore(extension);

         if (ret)
            emit signalCheckedOut();
      }
   });

   connect(ignoreMenu->addAction(tr("Ignore containing folder")), &QAction::triggered, this, [this]() {
      const auto msgBoxRet = QMessageBox::question(
          this, tr("Ignoring folder"), tr("Are you sure you want to add the containing folder to the black list?"));

      if (msgBoxRet == QMessageBox::Yes)
      {
         const auto path = mFileName.lastIndexOf("/");
         const auto folder = mFileName.left(path);
         const auto extension = QString("%1/*").arg(folder);
         const auto ret = addEntryToGitIgnore(extension);

         if (ret)
            emit signalCheckedOut();
      }
   });

   addSeparator();

   connect(addAction(tr("Add all files to commit")), &QAction::triggered, this, &UnstagedMenu::signalCommitAll);
   connect(addAction(tr("Revert all changes")), &QAction::triggered, this, [this]() {
      const auto msgBoxRet
          = QMessageBox::question(this, tr("Ignoring file"), tr("Are you sure you want to undo all the changes?"));
      if (msgBoxRet == QMessageBox::Yes)
         emit signalRevertAll();
   });
}

bool UnstagedMenu::addEntryToGitIgnore(const QString &entry)
{
   auto entryAdded = false;
   QDir d(mGit->getWorkingDir());
   QFile f(d.absolutePath() + "/.gitignore");

   if (!f.exists())
   {
      if (f.open(QIODevice::ReadWrite))
         f.close();
   }

   if (f.open(QIODevice::Append))
   {
      const auto bytesWritten = f.write(entry.toUtf8() + "\n");

      if (bytesWritten != -1)
      {
         QMessageBox::information(this, tr("File added to .gitignore"),
                                  tr("The file has been added to the ignore list in the file .gitignore."));

         entryAdded = true;
      }
      else
         QMessageBox::critical(this, tr("Unable to add the entry"),
                               tr("It was impossible to add the entry in the .gitignore file."));

      f.close();
   }

   return entryAdded;
}

void UnstagedMenu::onDeleteFile()
{
   QLog_Info("UI", "Removing path: " + mFileName);

   QProcess p;
   p.setWorkingDirectory(mGit->getWorkingDir());
   p.start("rm", { "-rf", mFileName });

   if (p.waitForFinished())
      emit signalCheckedOut();
}

void UnstagedMenu::openFileExplorer()
{
   QString absoluteFilePath = mGit->getWorkingDir() + QString("/") + mFileName;
   absoluteFilePath = absoluteFilePath.left(absoluteFilePath.lastIndexOf("/"));
   QString app;
   QStringList arguments;
#ifdef Q_OS_LINUX
   const auto fileExplorer = GitQlientSettings().globalValue("FileExplorer", "xdg-open").toString();

   if (fileExplorer.isEmpty())
   {
      QMessageBox::critical(parentWidget(), tr("Error opening file explorer"),
                            tr("The file explorer value in the settings is invalid. Please define what file explorer "
                               "you want to use to open file locations."));
      return;
   }

   arguments = fileExplorer.split(" ");
   arguments.append(absoluteFilePath);
   app = arguments.takeFirst();
#elif defined(Q_OS_WIN)
   app = QString::fromUtf8("explorer.ext");
   arguments = QStringList { "/select", QDir::toNativeSeparators(absoluteFilePath) };
#elif defined(Q_OS_MACOS)
   app = QString::fromUtf8("/usr/bin/open");
   arguments = QStringList { "-R", absoluteFilePath };
#endif

   const auto ret = QProcess::startDetached(app, arguments);

   if (!ret)
   {
      QMessageBox::critical(parentWidget(), tr("Error opening file explorer"),
                            tr("There was a problem opening the file explorer."));
   }
}

void UnstagedMenu::deleteUntracked()
{
   QScopedPointer<GitLocal> git(new GitLocal(mGit));

   if (const auto ret = git->cleanUntracked(); ret.success)
      emit untrackedDeleted();
   else
   {
      QMessageBox::warning(parentWidget(), tr("Error cleaning untracked files"),
                           tr("There was a problem removing all untracked files."));
   }
}
