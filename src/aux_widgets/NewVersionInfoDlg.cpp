#include "NewVersionInfoDlg.h"
#include "ui_NewVersionInfoDlg.h"

#include <GitQlientSettings.h>
#include <QScrollArea>
#include <QLabel>

NewVersionInfoDlg::NewVersionInfoDlg(QWidget *parent)
   : QDialog(parent)
   , ui(new Ui::NewVersionInfoDlg)
{
   ui->setupUi(this);
   ui->pbClose->setVisible(false);
   ui->pbPrevious->setVisible(false);
   ui->chNotAgain->setChecked(!GitQlientSettings().globalValue("ShowFeaturesDlg", false).toBool());

   createAddPage(
       tr("1. Plugins support"), { "pluginsPage" },
       tr("After some months of work and refactor, GitQlient can be extended with plugins!<br><br>"
          "The plugin system works by deploying <a style='color: #D89000' "
          "href='https://doc.qt.io/qt-6/plugins-howto.html'>Qt plugins</a> "
          "into the selected folder. GitQlient will automatically read the metadata for the plugins on every restart "
          "and list the available plugins (those that are valid) in the installed plugins panel.<br><br>"
          "All the GitQlient plugins are downloaded from their official GitHub repo page and stored in the folder "
          "selected. Once they are downloaded, a list with checkbox will be shown in the <em>Installed plugins</em> "
          "section where you can enable/disable them. In addition, the icon will be shown in the Controls panel for an "
          "easy access.<br><br>"));

   createAddPage(
       tr("2. New terminal widget (Linux/MacOS)"), { "terminalPage" },
       tr("If you are a Linux or MacOS user, you are lucky!<br><br>"
          "GitQlient now supports the terminal plugin where you can access the same functionalities you have "
          "in your usual terminal. It also keeps the history of commands so now you can take advantage of a "
          "shortcut to update and generate your projects.<br><br>"
          "Once the plugin is enabled, it will show an icon in the Controls panel to access it directly. In "
          "addition, in the <em>GitQlient</em> section of the configuration, you will find a combo box to "
          "select your preferred color scheme for the terminal widget based on the ones available in your OS."));

   createAddPage(
       tr("3. GitHub and Jenkins become plugins"), {},
       tr("The old GitServer view (to connect to GitHub) and Jenkins view (to show and trigger Jenkins jobs and "
          "builds) have been converted into plugins and are now available to be downloaded from their release page in "
          "their own GitHub repo. By having this views detached, GitQlient takes less dependencies (WebEngine, "
          "WebChannel, etc.), allows the release of the GitQlientPlugin for QtCreator and in addition, makes it "
          "possible to improve those plugins independently to GitQlient.<br><br>"));

   createAddPage(tr("4. Hunks view"), { "hunkPage" },
                 tr("Now it's possible to stage by hunk or by line!<br><br>"
                    "There is a new view in when showing the diff of "
                    "a file where GitQlient will show the changes ordered by hunks. Each hunk has its own "
                    "Discard/Stage buttons to apply the changes individually.<br><br>"
                    "In addition, by using the right button "
                    "of the mouse, you will be able to stage or discard changes by line."));

   createAddPage(tr("5. Foldable branches panel"), { "branchesPage" },
                 tr("From now on, all the panels of the Branches widget are foldable. This has been a long requested "
                    "feature that is now included in GitQlient.<br><br>"
                    "This will help you to save a lot of space if you work with small screens. Fold and unfold is as "
                    "easy as clicking in the plus sign, but you can also do it from the config widget."));

   createAddPage(
       tr("6. Shortcuts"), { "shortcutsPage" },
       tr("In this new version of GitQlient, some shortcuts have been introduced so you don't need to "
          "navigate with the mouse the whole time.<br><br>"
          "For now, the shortcuts are used in the Controls panel and in the branches panel (in this case to "
          "use toggle the minimal/normal view). The shortcuts are:"
          "<ul>"
          "<li>Graph view: <strong>Ctrl+1</strong></li><li>Diff view: <strong>Ctrl+2</strong></li><li>Blame view: "
          "<strong>Ctrl+3</strong></li>"
          "<li>Pull: <strong>Ctrl+4</strong></li><li>Push: <strong>Ctrl+5</strong></li><li>Config view: "
          "<strong>Ctrl+6</strong></li>"
          "<li>Refresh/Update: <strong>F5</strong></li><li>Terminal (if enabled): <strong>Ctrl+7<strong></li>"
          "<li>GitHub (if enabled): <strong>Ctrl+8</strong></li><li>Jenkins (if enabled): <strong>Ctrl+9</strong></li>"
          "</ul>"));

   createAddPage(
       tr("7. New version notification"), { "" },
       tr("Until now, whenever a new version of GitQlient was released, the app would notify making it "
          "available for download.<br><br>"
          "This will change from now on since GitQlient is available through different ways: DEB and <a style='color: "
          "#D89000' href='https://src.fedoraproject.org/rpms/gitqlient'>RPM</a> "
          "packages, AppImage, installer for Windows, and DMG/ports for MacOS. Since it's quite hard and error "
          "prone to verify what binary is being used, the icon will notify that a new version have been "
          "released but it the download will have to happen manually.<br><br>"
          "You can of course download the latest version <a style='color: #D89000' "
          "href='https://github.com/francescmm/GitQlient/releases'>from the official GitHub repo</a>."));

   connect(ui->pbPrevious, &QPushButton::clicked, this, &NewVersionInfoDlg::goPreviousPage);
   connect(ui->pbNext, &QPushButton::clicked, this, &NewVersionInfoDlg::goNextPage);
   connect(ui->pbClose, &QPushButton::clicked, this, &QDialog::close);
   connect(ui->chNotAgain, &QCheckBox::stateChanged, this, &NewVersionInfoDlg::saveConfig);
}

NewVersionInfoDlg::~NewVersionInfoDlg()
{
   delete ui;
}

void NewVersionInfoDlg::goPreviousPage()
{
   if (ui->stackedWidget->currentIndex() == 1)
      ui->pbPrevious->setVisible(false);

   ui->stackedWidget->setCurrentIndex(ui->stackedWidget->currentIndex() - 1);

   ui->pbClose->setVisible(false);
   ui->pbNext->setVisible(true);
}

void NewVersionInfoDlg::goNextPage()
{
   ui->pbPrevious->setVisible(true);
   ui->stackedWidget->setCurrentIndex(ui->stackedWidget->currentIndex() + 1);

   if (ui->stackedWidget->currentIndex() == ui->stackedWidget->count() - 1)
   {
      ui->pbNext->setVisible(false);
      ui->pbClose->setVisible(true);
   }
}

void NewVersionInfoDlg::createAddPage(const QString &title, const QStringList &imgsSrc, const QString &desc)
{
   QScrollArea *scrollArea = new QScrollArea();
   scrollArea->setWidgetResizable(true);
   scrollArea->setFrameShape(QFrame::NoFrame);

   const auto page = new QWidget();
   scrollArea->setWidget(page);
   const auto titleLabel = new QLabel(title);
   auto font = titleLabel->font();
   font.setPointSize(14);
   font.setBold(true);
   titleLabel->setFont(font);
   titleLabel->setAlignment(Qt::AlignCenter);

   const auto gridLayout = new QVBoxLayout(page);
   gridLayout->setAlignment(Qt::AlignTop);
   gridLayout->addSpacing(30);
   gridLayout->addWidget(titleLabel);

   for (const auto &imgSrc : imgsSrc)
   {
      const auto img = new QLabel();
      img->setAlignment(Qt::AlignCenter);
      img->setPixmap(QPixmap(QString(":/images/NewVersionInfoDlg/%1").arg(imgSrc)));
      gridLayout->addWidget(img);
   }

   const auto description = new QLabel(desc);
   description->setWordWrap(true);
   description->setTextFormat(Qt::RichText);
   gridLayout->addWidget(description);

   ui->stackedWidget->addWidget(scrollArea);
}

void NewVersionInfoDlg::saveConfig()
{
   GitQlientSettings().setGlobalValue("ShowFeaturesDlg", !ui->chNotAgain->isChecked());
}
