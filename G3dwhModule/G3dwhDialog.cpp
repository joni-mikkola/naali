#include "G3dwhDialog.h"
#include "ui_G3dwhDialog.h"
#include "G3dwhModule.h"

#include <QDir>
#include <QFile>
#include <QString>
#include <QDirIterator>
#include <QMouseEvent>

#include <quazip.h>
#include <quazipfile.h>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QDebug>

#include "../SceneStructureModule/SceneStructureModule.h"

G3dwhDialog::G3dwhDialog(Foundation::Framework * framework, std::string modelPath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::G3dwhDialog)
{
    ui->setupUi(this);

    framework_ = framework;
    modelDir=QString::fromUtf8(modelPath.c_str());

    toolBar = new QToolBar(this);
    toolBar->setObjectName(QString::fromUtf8("toolBar"));

    ui->gridLayout->addWidget(toolBar);

    ui->warehouseView->load(QUrl("http://sketchup.google.com/3dwarehouse/"));
    ui->warehouseView->show();

    multiSelectionList.clear();

    downloadAborted=false;
    ui->downloadProgress->setValue(0);

    updateDownloads();

    ui->warehouseView->page()->setForwardUnsupportedContent(true);

    infoLabel=new QLabel(this);

    QDir downloadDir(modelDir+"/models");
    if(!downloadDir.exists())
    {
        QDir().mkdir(modelDir+"/models");
    }


    addButton = new QPushButton(this);
    addButton->setText("ADD TO SCENE");
    addButton->setToolTip("Add the selected model to scene");
    connect(addButton, SIGNAL(clicked()), this, SLOT(addButton_Clicked()));

    removeButton = new QPushButton(this);
    removeButton->setText("DELETE");
    removeButton->setToolTip("Delete the downloaded model");
    connect(removeButton, SIGNAL(clicked()), this, SLOT(removeButton_Clicked()));

    menuButton = new QPushButton(this);
    menuButton->setText("SETTINGS");
    menuButton->setToolTip("Import Settings");
    connect(menuButton, SIGNAL(clicked()), this, SLOT(menuButton_Clicked()));

    settingsMenu = new QMenu();
    testSettingA =  settingsMenu->addAction("TEST SETTING 1");
    testSettingA->setCheckable(true);
    testSettingA->setChecked(false);
    settingsMenu->addSeparator();
    testSettingB =  settingsMenu->addAction("TEST SETTING 2");
    settingsMenu->addSeparator();
    testSettingC =  settingsMenu->addAction("TEST SETTING 3");

    connect(testSettingA, SIGNAL(triggered()), this, SLOT(settingsMenuAction()));

    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Back));
    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Forward));
    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Reload));
    toolBar->addAction(ui->warehouseView->pageAction(QWebPage::Stop));

    toolBar->addWidget(addButton);
    toolBar->addWidget(removeButton);
    toolBar->addWidget(menuButton);
    toolBar->addWidget(infoLabel);

    ui->warehouseView->installEventFilter(this);

    connect(ui->warehouseView->page(), SIGNAL(unsupportedContent(QNetworkReply*)), this, SLOT(unsupportedContent(QNetworkReply*)));

    connect(ui->warehouseView, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));
    connect(ui->warehouseView->page(), SIGNAL(downloadRequested(const QNetworkRequest&)), this, SLOT(downloadRequested(const QNetworkRequest&)));
    connect(ui->warehouseView, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));
    connect(ui->warehouseView->page(), SIGNAL(linkHovered(QString,QString,QString)), SLOT(linkHovered(QString,QString,QString)));

}

G3dwhDialog::~G3dwhDialog()
{
    delete ui;
}

void G3dwhDialog::keyPressEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Control)
    {
        multiSelectionList.append("models/"+ui->downloadList->currentItem()->text());
        fileName.clear();
        ui->downloadList->setSelectionMode(QAbstractItemView::MultiSelection);
        multiSelection=true;
    }
}

void G3dwhDialog::keyReleaseEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Control)
    {
        multiSelectionList.clear();
        ui->downloadList->clearSelection();
        ui->downloadList->setSelectionMode(QAbstractItemView::SingleSelection);
        multiSelection=false;
    }
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

bool G3dwhDialog::eventFilter(QObject *o, QEvent *e)
{
    if(e->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(e);
        if (mouseEvent->button() == Qt::XButton1)
        {
            ui->warehouseView->triggerPageAction(QWebPage::Back, false);
            mouseEvent->accept();
            return true;
        }
        if (mouseEvent->button() == Qt::XButton2)
        {
            ui->warehouseView->triggerPageAction(QWebPage::Forward, false);
            mouseEvent->accept();
            return true;
        }
    }
    return false;
}

void G3dwhDialog::on_downloadList_itemClicked(QListWidgetItem *item)
{
    if(multiSelection)
    {
        multiSelectionList.append("models/"+item->text());
        loadHtmlPath(modelDir+"/"+multiSelectionList.last());
    }
    else
    {
        QString filename=modelDir+"/models/"+item->text();
        loadHtmlPath(filename);
    }
}

void G3dwhDialog::addButton_Clicked()
{
        if(multiSelection)
        {
            for(int i=0;i<multiSelectionList.length();i++)
            {
                QString filename=multiSelectionList.at(i);
                addToScene(filename);
            }
        }
        else
        {
            QString filename="models/"+ui->downloadList->currentItem()->text();
            addToScene(filename);
        }
}

void G3dwhDialog::removeButton_Clicked()
{
    if(ui->downloadList->count()!=0)
    {
        QStringList removeList;
        if(multiSelection)
        {
            removeList=multiSelectionList;
        }
        else
            removeList.append(modelDir+"/models/"+ui->downloadList->currentItem()->text());

        for(int i=0;i<removeList.length();i++)
        {
            QString filename=modelDir+"/"+removeList.at(i);
            QFile file(filename);
            file.remove();
            updateDownloads();

            QFile htmlSources(modelDir+"/models/sources");
            if (!htmlSources.open(QIODevice::ReadOnly | QIODevice::Text))
                return;

            QStringList fileContent;

            while (!htmlSources.atEnd())
            {
                QByteArray fileInput = htmlSources.readLine();
                QString line = fileInput;
                if (!line.contains(filename,Qt::CaseInsensitive))
                {
                    fileContent.append(line);
                }
            }

            htmlSources.remove();

            if (!htmlSources.open(QIODevice::Append | QIODevice::Text))
                return;

            QTextStream out(&htmlSources);
            for(int i=0; i<fileContent.length(); i++)
            {
                out << fileContent[i];
            }
        }
    }
}


void G3dwhDialog::menuButton_Clicked()
{
    QPoint displayPoint = QCursor::pos();
    settingsMenu->popup(displayPoint,0);
}

void G3dwhDialog::settingsMenuAction()
{
    if (testSettingA->isChecked())
        qDebug()<<"nyan";
    else
        qDebug()<<"oh noes";
}

void G3dwhDialog::addToScene(QString pathToFile)
{
    QString daeRef;
    // get dae file reference to daeref
    unpackDownload(pathToFile, daeRef);
    checkDirStructure(pathToFile, daeRef);
    bool clearScene = false;
    qDebug()<<multiSelectionList;
    SceneStructureModule *sceneStruct = framework_->GetModule<SceneStructureModule>();

    if (sceneStruct)
        sceneStruct->InstantiateContent(daeRef, Vector3df(), clearScene);
}

void G3dwhDialog::checkDirStructure(QString pathToDir,QString &daeRef)
{
    QString targetDirName = sceneDir+pathToDir.replace(".zip","/");

    QDir imagesDir(targetDirName+"/images");
    QDir modelsDir(targetDirName+"/models");
    if(imagesDir.exists() && modelsDir.exists())
        return;
    else
    {
        QDir().mkpath(targetDirName+"/images");
        QDir().mkpath(targetDirName+"/models");

        QString dirContent;
        QStringList filter;
        filter<<"*.dae"<<"*.jpg"<<"*.png"<<"*.gif"<<"*.jpeg";
        QDirIterator dirIterator(targetDirName,filter,QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);

        while(dirIterator.hasNext())
        {
            dirIterator.next();
            dirContent=dirIterator.fileName();

            if(dirContent.contains(".dae"))
            {
                QFile daeFile(targetDirName+dirContent);
                daeFile.rename(targetDirName+"/models/"+dirContent);
                daeRef=targetDirName+"/models/"+dirContent;
            }
            for(int i=1;i<filter.length();i++)
            {
                QString imageFilter=filter[i];
                imageFilter.replace("*","");
                if(dirContent.contains(imageFilter))
                {
                    QFile imageFile(targetDirName+dirContent);
                    imageFile.rename(targetDirName+"/images/"+dirContent);
                }
            }
        }
    }


}

void G3dwhDialog::downloadRequested(const QNetworkRequest &request)
{
    QNetworkRequest newRequest = request;
    newRequest.setAttribute(QNetworkRequest::User,"tmpFileName");
    QNetworkAccessManager *networkManager = ui->warehouseView->page()->networkAccessManager();
    QNetworkReply *reply = networkManager->get(newRequest);

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
        QFile fileTest(fileName);

        int index=0;

        while(fileTest.exists())
        {
            QString indexStr;
            indexStr.setNum(index);
            QStringList newFileName=fileTest.fileName().split(".");
            QString modelName = newFileName[0];
            modelName.replace(QRegExp("_[0-9]{1,9}$"),"");

            fileTest.setFileName(modelName+"_"+indexStr+"."+newFileName[1]);
            fileName=fileTest.fileName();
            index++;
        }

        QFile file(fileName);
        if (file.open(QFile::ReadWrite))
            file.write(reply->readAll());
        file.close();
    }

    downloadAborted=false;
    saveHtmlPath();
    updateDownloads();

}

void G3dwhDialog::urlChanged(QUrl url)
{
    QString newUrl=url.toString();

    if(!newUrl.contains("sketchup.google.com/3dwarehouse"))
    {
        //      ui->warehouseView->load(QUrl("http://sketchup.google.com/3dwarehouse/"));
    }
}

void G3dwhDialog::saveHtmlPath()
{
    QFile htmlSources(modelDir+"/models/sources");
    if (!htmlSources.open(QIODevice::Append | QIODevice::Text))
        return;

    QTextStream out(&htmlSources);
    out << htmlSource+"|"+fileName+"\n";
}

void G3dwhDialog::loadHtmlPath(QString file)
{
    QFile htmlSources(modelDir+"/models/sources");
    if (!htmlSources.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while (!htmlSources.atEnd())
    {
        QByteArray fileInput = htmlSources.readLine();
        QString line = fileInput;
        if (line.contains(file,Qt::CaseInsensitive))
        {
            QStringList parseList=line.split("|");
            ui->warehouseView->load(QUrl(parseList[0]));
        }
    }

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
    QString dataType=reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (dataType != "application/zip")
    {
        downloadAborted=true;
        reply->close();
        infoLabel->setText("Wrong format, select Collada if available.");
    }
    else
    {
        QStringList titleList = ui->warehouseView->title().split(" by");
        QStringList parseList = titleList[0].split(" ");
        fileName=modelDir+"/models/"+parseList.join("_")+".zip";
        fileName.replace(",","");
        htmlSource = ui->warehouseView->url().toString();

    }
}

void G3dwhDialog::updateDownloads()
{
    ui->downloadList->clear();

    QDir dir(modelDir+"/models");
    QStringList nameFilters;
    nameFilters.append("*.zip");
    dir.setNameFilters(nameFilters);
    QStringList downloadedItems=dir.entryList();

    ui->downloadList->addItems(downloadedItems);
}

void G3dwhDialog::unsupportedContent(QNetworkReply *reply)
{
    QNetworkRequest request(reply->url());
    downloadRequested(request);
}

void G3dwhDialog::setScenePath(QString scenePath)
{
    sceneDir = scenePath;
}

int G3dwhDialog::unpackDownload(QString file, QString & daeRef)
{
    QFile inFile(modelDir+"/"+file);
    QFile outFile;

    QString targetName = file.replace(".zip","/");

    QDir testDir(sceneDir+targetName);
    QStringList filter;
    filter<<"*.dae";
    if(testDir.exists())
    {
        QDirIterator dirIterator(testDir.absolutePath(),filter,QDir::AllDirs|QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);

        while(dirIterator.hasNext())
        {
            dirIterator.next();

            if(dirIterator.fileName().endsWith(".dae"))
                daeRef=dirIterator.filePath();
        }
        return true;
    }
    else
    {
        QDir().mkpath(sceneDir+targetName);
    }

    QuaZip zip(inFile.fileName());

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

        if (name.endsWith(".dae"))
        {
            name.replace(" ","");
            daeRef = targetName + name;
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
        while (zFile.getChar(&c))
        {
            // we could just do this, but it's about 40% slower:
            // out.putChar(c);
            buf[len++] = c;
            if (len >= 4096)
            {
                outFile.write(buf, len);
                len = 0;
            }
        }
        if (len > 0)
        {
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

