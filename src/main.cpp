#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QTimer>

#include <GitQlient.h>
#include <QLogger.h>

using namespace QLogger;

int main(int argc, char *argv[])
{
   qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");

   QApplication app(argc, argv);
   QStringList arguments;

   auto argNum = argc;

   while (argNum--)
      arguments.prepend(QString::fromUtf8(argv[argNum]));

   QApplication::setOrganizationName("CescSoftware");
   QApplication::setOrganizationDomain("francescmm.com");
   QApplication::setApplicationName("GitQlient");
   QApplication::setApplicationVersion(VER);
   QApplication::setWindowIcon(QIcon(":/icons/GitQlientLogoIco"));

   QFontDatabase::addApplicationFont(":/DejaVuSans");
   QFontDatabase::addApplicationFont(":/DejaVuSansMono");

   QStringList repos;
   if (GitQlient::parseArguments(arguments, &repos))
   {
      GitQlient mainWin;
      mainWin.setRepositories(repos);
      mainWin.show();

      QTimer::singleShot(500, &mainWin, &GitQlient::restorePinnedRepos);

      return app.exec();
   }

   return 0;
}
