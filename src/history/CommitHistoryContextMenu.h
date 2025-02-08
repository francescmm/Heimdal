#pragma once

/****************************************************************************************
 ** GitQlient is an application to manage and operate one or several Git repositories. With
 ** GitQlient you will be able to add commits, branches and manage all the options Git provides.
 ** Copyright (C) 2021  Francesc Martinez
 **
 ** LinkedIn: www.linkedin.com/in/cescmm/
 ** Web: www.francescmm.com
 **
 ** This program is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public
 ** License as published by the Free Software Foundation; either
 ** version 2 of the License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this library; if not, write to the Free Software
 ** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***************************************************************************************/

#include <QMenu>

class GitCache;
class GitBase;

/*!
 \brief This class configures the context menu that will be shown when the user right-click over a commit in the
 repository view.

 \class CommitHistoryContextMenu CommitHistoryContextMenu.h "CommitHistoryContextMenu.h"
*/
class CommitHistoryContextMenu : public QMenu
{
   Q_OBJECT

signals:
   void fullReload();
   void referencesReload();
   void logReload();

   /**
    * @brief Signal triggered when the user wants to open the diff of a commit compared to its parent.
    * @param sha The SHA to diff.
    */
   void signalOpenDiff(const QString &sha);

   /*!
    \brief Signal triggered when the user wants to amend a commit.

    \param sha The SHA of the commit to amend.
   */
   void signalAmendCommit(const QString &sha);

   /**
    * @brief Signal triggered when trying to rebase and a conflict happens.
    */
   void signalRebaseConflict();

   /*!
    \brief Signal triggered when a merge has been requested. Since it involves a lot of changes at UI level this action
    is not performed here.

    \param origin The branch to merge from.
    \param destination The branch to merge into.
   */
   void signalMergeRequired(const QString &origin, const QString &destination);

   /**
    * @brief Signal triggered when a merge with squash behavior has been requested. Since it involves a lot of changes
    * at UI level this action is not performed here.
    *
    * @param origin The branch to merge from.
    * @param destination The branch to merge into.
    */
   void mergeSqushRequested(const QString &origin, const QString &destination);

   /*!
    * \brief signalConflict Signal triggered when trying to cherry-pick and a conflict happens.
    */
   void signalCherryPickConflict(const QStringList &pendingShas = QStringList());
   /*!
    * \brief signalPullConflict Signal triggered when trying to pull and a conflict happens.
    */
   void signalPullConflict();
   /**
    * @brief signalRefreshPRsCache Signal that refreshes PRs cache.
    */
   void signalRefreshPRsCache();

public:
   /*!
    \brief Default constructor.

    \param cache The cache for the current repository.
    \param git The git object to execute Git commands.
    \param shas The list of SHAs selected.
    \param parent The parent widget if needed.
   */
   explicit CommitHistoryContextMenu(const QSharedPointer<GitCache> &cache, const QSharedPointer<GitBase> &git,
                                     const QStringList &shas, QWidget *parent = nullptr);

private:
   QSharedPointer<GitCache> mCache;
   QSharedPointer<GitBase> mGit;
   QStringList mShas;

   /*!
    \brief This method creates all the actions that will appear when only one SHA is selected.
   */
   void createIndividualShaMenu();
   /*!
    \brief This method creates all the actions that will appear when more than one SHA is selected.
   */
   void createMultipleShasMenu();
   /*!
    \brief Pushes the changes to a stash.
   */
   void stashPush();
   /*!
    \brief Pops the changes stored in a stash.
   */
   void stashPop();
   /*!
    \brief Creates a branch at the selected commit.
   */
   void createBranch();
   /*!
    \brief Creates a tag at the selected commit.
   */
   void createTag();
   /*!
    \brief Export the selected commit/s as patches. If multiple commits are selected they are enumerated sequentially.
   */
   void exportAsPatch();
   /*!
    \brief Checks out to the selected branch.
   */
   void checkoutBranch();
   /**
    * @brief createCheckoutBranch Creates and checks out a branch.
    */
   void createCheckoutBranch();
   /*!
    \brief Checks out to the selected commit.
   */
   void checkoutCommit();
   /*!
    \brief Cherry-picks the selected commit into the current branch.
   */
   void cherryPickCommit();
   /*!
    \brief Applies a patch loaded by the user but doesn't commit it.
   */
   void applyPatch();
   /*!
    \brief Applies the changes from a patch in the form of a commit.
   */
   void applyCommit();
   /*!
    \brief Pushes the local commits into remote.
   */
   void push();
   /*!
    \brief Pulls the changes from remote.
   */
   void pull();
   /*!
    \brief Fetches the changes from remote.
   */
   void fetch();

   void revertCommit();

   /*!
    \brief Resets the current branch reference into the selected commit keeping all changes.
   */
   void resetSoft();
   /*!
    \brief Resets the current branch reference into the selected commit.
   */
   void resetMixed();
   /*!
    \brief Resets the current branch reference into the selected commit overriding all changes.
   */
   void resetHard();

   /**
    * @brief Rebases the current branch over the selected branch.
    */
   void rebase();

   /*!
    \brief Merges the \p branchFrom into the current branch.

    \param branchFrom The branch that will be merge into the current one.
   */
   void merge();

   /**
    * @brief mergeSquash Merges the @p branchFrom into the current branch squashing all the commits.
    * @param branchFrom The branch that will be merge into the current one.
    */
   void mergeSquash();
   /*!
    \brief Method that adds all the branch related actions.

    \param sha The SHA of the current commit.
   */
   void addBranchActions(const QString &sha);

   void showSquashDialog();

   void amendNoEdit();
};
