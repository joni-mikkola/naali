#include "G3dwhDialog.h"
#include "ui_G3dwhDialog.h"

#include <QDir>
#include <QFile>
#include <QString>

#include "quazip/quazip.h"
#include "quazip/quazipfile.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QDebug>

#include "LoggingFunctions.h"

G3dwhDialog::G3dwhDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::G3dwhDialog)
{
    ui->setupUi(this);


    toolBar = new QToolBar(this);
    toolBar->setObjectName(QString::fromUtf8("toolBar"));

    ui->gridLayout->addWidget(toolBar);

    ui->warehouseView->load(QUrl("http://sketchup.google.com/3dwarehouse/"));
    ui->warehouseView->show();

    downloadAborted=false;
    ui->downloadProgress->setValue(0);

    updateDownloads();

    ui->warehouseView->page()->setForwardUnsupportedContent(true);

    infoLabel=new QLabel(this);

    QDir downloadDir("models");
    if(!downloadDir.exists())
    {
        QDir().mkdir("models");
    }

    addButton = new QPushButton(this);
    addButton->setText("ADD TO SCENE");
    addButton->setToolTip("Add the selected model to scene");
    connect(addButton, SIGNAL(clicked()), this, SLOT(on_addButton_Clicked()));

    removeButton = new QPushButton(this);
    removeButton->setText("DELETE");
    removeButton->setToolTip("Delete the downloaded model");
    connect(removeButton, SIGNAL(clicked()), this, SLOT(on_removeButton_Clicked()));

    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Back));
    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Forward));
    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Reload));
    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Stop));

    toolBar->addWidget(addButton);
    toolBar->addWidget(removeButton);
    toolBar->addWidget(infoLabel);

    connect(ui->warehouseView->page(), SIGNAL(unsupportedContent(QNetworkReply*)), this, SLOT(unsupportedContent(QNetworkReply*)));

    connect(ui->warehouseView, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));
    connect(ui->warehouseView->page(), SIGNAL(downloadRequested(const QNetworkRequest&)), this, SLOT(downloadRequested(const QNetworkRequest&)));
    connect(ui->warehouseView, SIGNAL(titleChanged(QString)), this, SLOT(titleChanged(QString)));
    connect(ui->warehouseView, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));
    connect(ui->warehouseView->page(), SIGNAL(linkHovered(QString,QString,QString)), SLOT(linkHovered(QString,QString,QString)));
}

G3dwhDialog::~G3dwhDialog()
{
    delete ui;
}

void G3dwhDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void G3dwhDialog::on_downloadList_itemClicked(QListWidgetItem *item)
{
    QString filename="models/"+item->text();
    //addToScene(filename);
    QString searchUrl = item->text();
    searchUrl.replace("_","+");
    searchUrl.replace(".zip","");

    if (searchUrl.contains("&"))
    {
        searchUrl.replace("&","%26");
        QUrl url = QUrl::fromEncoded("http://sketchup.google.com/3dwarehouse/search?q=\"" +searchUrl.toAscii() +"\"&styp=m&btnG=Search");
        ui->warehouseView->load(url);
    }
    else
        ui->warehouseView->load(QUrl("http://sketchup.google.com/3dwarehouse/search?q=\"" +searchUrl +"\"&styp=m&btnG=Search"));

    //qDebug()<<QUrl("http://sketchup.google.com/3dwarehouse/search?q=\"" +searchUrl +"\"&styp=m&btnG=Search");

}

void G3dwhDialog::on_addButton_Clicked()
{
    if(ui->downloadList->currentItem())
    {
        QString filename="models/"+ui->downloadList->currentItem()->text();
        addToScene(filename);
    }
}

void G3dwhDialog::on_removeButton_Clicked()
{
    QString filename="models/"+ui->downloadList->currentItem()->text();
    QFile file(filename);
    file.remove();
    updateDownloads();
}

void G3dwhDialog::addToScene(QString pathToFile)
{
    unpackDownload(pathToFile);
}

void G3dwhDialog::downloadRequested(const QNetworkRequest &request)
{
    qDebug()<<"Download requested";
    //QString fileName=request.url().toString();
    //QString fileName="testi.zip";
    QNetworkRequest newRequest = request;
    newRequest.setAttribute(QNetworkRequest::User,fileName);
    QNetworkAccessManager *networkManager = ui->warehouseView->page()->networkAccessManager();
    QNetworkReply *reply = networkManager->get(newRequest);
    qDebug()<<reply->rawHeader("Content-Disposition");

    connect(reply, SIGNAL(metaDataChanged()),this, SLOT(readMetaData()));
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)),this, SLOT(downloadProgress(qint64, qint64)));
    connect(reply, SIGNAL(finished()),this, SLOT(downloadFinished()));

}

void G3dwhDialog::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    ui->downloadProgress->setMaximum(bytesTotal);
    ui->downloadProgress->setValue(bytesReceived);
}

void G3dwhDialog::downloadFinished()
{
    ui->downloadProgress->setValue(0);
    QNetworkReply *reply = ((QNetworkReply*)sender());

    if( downloadAborted == false )
    {
        QFile file(fileName);
        if (file.open(QFile::ReadWrite))
        file.write(reply->readAll());
        addToScene(fileName);
        file.close();
    }

    downloadAborted=false;
    updateDownloads();


}

void G3dwhDialog::titleChanged(QString title)
{
    QStringList titleList = title.split(" by");
    //QDir().mkdir("models//"+titleList[0]);
    QStringList parseList = titleList[0].split(" ");
    fileName="models//"+parseList.join("_")+".zip";
}

void G3dwhDialog::urlChanged(QUrl url)
{
    QString newUrl=url.toString();

    if(!newUrl.contains("sketchup.google.com/3dwarehouse"))
    {
        ui->warehouseView->load(QUrl("http://sketchup.google.com/3dwarehouse/"));
    }

    qDebug()<<url;
}

void G3dwhDialog::loadFinished()
{
   infoLabel->setText("");
}

void G3dwhDialog::linkHovered(QString url, QString title, QString content)
{
    //qDebug()<<url;
}

void G3dwhDialog::readMetaData()
{
    QNetworkReply *reply = ((QNetworkReply*)sender());
    qDebug()<<"MetaData changed";
    QString dataType=reply->header(QNetworkRequest::ContentTypeHeader).toString();
    qDebug()<<dataType;
    if (dataType != "application/zip")
    {
        downloadAborted=true;
        reply->close();
        infoLabel->setText("Wrong format, select Collada if available.");
    }

}

void G3dwhDialog::updateDownloads()
{
    ui->downloadList->clear();
    //infoLabel->setText("");

    QDir dir("./models");
    QStringList nameFilters;
    nameFilters.append("*.zip");
    dir.setNameFilters(nameFilters);
    QStringList downloadedItems=dir.entryList();
    ui->downloadList->addItems(downloadedItems);
}

void G3dwhDialog::unsupportedContent(QNetworkReply *reply)
{
    qDebug()<<"Unsupported content";
    qDebug()<<reply->url();
    QNetworkRequest request(reply->url());
    downloadRequested(request);
    qDebug()<<reply->rawHeaderList();

}

void G3dwhDialog::setScenePath(QString scenePath)
{
    sceneDir = scenePath;
}

int G3dwhDialog::unpackDownload(QString file)
{
    QFile inFile(file);
    QFile outFile;

    QString targetName = file.replace(".zip","/");

    QuaZip zip(&inFile);

    if(!zip.open(QuaZip::mdUnzip)) {
      qWarning("testRead(): zip.open(): %d", zip.getZipError());
      return false;
    }

    QuaZipFile zFile(&zip);
    QString name;
    char c;
    for(bool more=zip.goToFirstFile(); more; more=zip.goToNextFile())
    {
      if(!zFile.open(QIODevice::ReadOnly)) {
        qWarning("testRead(): file.open(): %d", zFile.getZipError());
        return false;
      }
      name=zFile.getActualFileName();
      if(zFile.getZipError()!=UNZ_OK) {
        qWarning("testRead(): file.getFileName(): %d", zFile.getZipError());
        return false;
      }
      QString dirn = sceneDir + targetName + name;

      if (name.contains('/')) { // subdirectory support
        // there must be a more elegant way of doing this
        // but I couldn't find anything useful in QDir
        dirn.chop(dirn.length() - dirn.lastIndexOf("/"));
        QDir().mkpath(dirn);
      }
      outFile.setFileName( sceneDir + targetName + name);
      outFile.open(QIODevice::WriteOnly);
      char buf[4096];
      int len = 0;
      while (zFile.getChar(&c)) {
        // we could just do this, but it's about 40% slower:
        // out.putChar(c);
        buf[len++] = c;
        if (len >= 4096) {
          outFile.write(buf, len);
          len = 0;
        }
      }
      if (len > 0) {
        outFile.write(buf, len);
      }
      outFile.close();
      if(zFile.getZipError()!=UNZ_OK) {
        qWarning("testRead(): file.getFileName(): %d", zFile.getZipError());
        return false;
      }
      if(!zFile.atEnd()) {
        qWarning("testRead(): read all but not EOF");
        return false;
      }
      zFile.close();
      if(zFile.getZipError()!=UNZ_OK) {
        qWarning("testRead(): file.close(): %d", zFile.getZipError());
        return false;
      }
    }
    zip.close();
    if(zip.getZipError()!=UNZ_OK) {
      qWarning("testRead(): zip.close(): %d", zip.getZipError());
      return false;
    }

    return true;

}

