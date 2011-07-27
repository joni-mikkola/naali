#ifndef G3DWHDIALOG_H
#define G3DWHDIALOG_H

#include <QDialog>
#include <QtGui/QToolBar>
#include <QtWebKit/QtWebKit>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>

#include "SceneFwd.h"
#include "AssetReference.h"
#include "IModule.h"

namespace Ui {
    class G3dwhDialog;
}

class G3dwhDialog : public QDialog {
    Q_OBJECT
public:
    G3dwhDialog(Foundation::Framework * framework, QWidget *parent = 0);
    ~G3dwhDialog();

    void setScenePath(QString scenePath);




protected:
    void changeEvent(QEvent *e);

private:
    Foundation::Framework * framework_;
    Ui::G3dwhDialog *ui;

    QToolBar *toolBar;

    QPushButton *addButton;
    QPushButton *removeButton;
    QLabel *infoLabel;

    QString fileName;
    QString sceneDir;
    bool downloadAborted;

    void updateDownloads();
    int unpackDownload(QString file, QString & daeRef);
    void addToScene(QString pathToFile);
    void saveHtmlPath();
    void loadHtmlPath(QString file);

private slots:
    void downloadRequested(const QNetworkRequest &);
    void downloadFinished();
    void downloadProgress(qint64, qint64);
    void titleChanged(QString);
    void urlChanged(QUrl);
    void linkHovered(QString,QString,QString);
    void readMetaData();
    void loadFinished();
    void unsupportedContent(QNetworkReply*);

    void on_downloadList_itemClicked(QListWidgetItem *item);
    void on_addButton_Clicked();
    void on_removeButton_Clicked();
};

#endif // G3DWHDIALOG_H
