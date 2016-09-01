#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStatusBar>
#include <QDebug>
#include <QUrl>
#include <QPushButton>
#include <QNetworkReply>
#include <QJsonArray>
#include <QTableWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>

#include <QList>
#include <QPair>
#include <QWidget>
#include <QProgressBar>
#include <QLayout>
#include <QMessageBox>
#include <QSizePolicy>
#include <QFileInfo>
#include "vkauthenticator.h"

namespace Ui {
class MainWindow;
}

enum State_t
{
    NotStarted = 0,
    TokenRequested,
    TokenRecvd,
    TokenFailed,
    AudioListRequested,
    AudioListRcvd,
    AudioListFailed,
    AudioListDisplayed,
    AudioDownloading
};

class MultiDownloader;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    State_t _state;
    QString _token;
    QString _lastError;

    QPushButton *reconnectButton;

    QNetworkAccessManager *_netManager;

    QJsonArray _audioList;
    QTableWidget *_table;

    QString _directory;
    QLineEdit* _dirLabel;
    QPushButton* _dirButton;
    QPushButton* _selectAllButton;
    QPushButton* _unselectAllButton;
    QPushButton* _downloadSelectedButton;
    MultiDownloader* _multiDownloader;
    VkAuthenticator* _authenticator;

    void setState(State_t iNewState){qDebug() << "New state: " <<iNewState; _state = iNewState; emit stateChangedSignal(iNewState);}
    void setStatus(const QString& iStatus){statusBar()->showMessage(iStatus);}
    void cleanMainWidget();

    void requestToken();
    void requestAudios();

    void showAudioTable();

signals:
    void stateChangedSignal(State_t newState);
    void escPressed();

public slots:
    void keyPressEvent(QKeyEvent*);
    void setTableButtonsEnabled(bool);

private slots:
    void showAbout();
    void statusMessageChangedSlot(const QString& iNewStatus){qDebug() << "New status message: " << iNewStatus;}
    void stateChangedSlot(State_t iNewState);
    void replyReceivedSlot(QNetworkReply* iReply){qDebug() << "Reply received from " << iReply->url().host();}
    void encryptedSlot(QNetworkReply* ){qDebug() << "ENCRYPTED!";}
    void browse();

    // token view slots
    void authenticatorErrorSlot(QString str);
    void authenticatorTokenReceivedSlot(QString str);
    void retryAuthSLot(){setState(NotStarted);}

    // request audios slots
    void audiosListDownloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal){setStatus("Downloading list of audios: "+QString::number(100 * bytesReceived/bytesTotal)+"%");}
    void audiosFinishedSlot();

    // download audio slots
    void audiosTableCellClickedSlot(int row, int column);
    void audioDownloadingProgress(qint64 iRcvd, qint64 iTotal, QString filename){setStatus("Downloading "+QFileInfo(filename).fileName()+" "+QString::number(100 * iRcvd/iTotal)+"%");}
    void audioDownloaded(bool success, QString reason, QString filename);
    void audioDownloadAllClickedSlot();
    void audioDownloadedUpdateStatusSlot(bool success, QString reason, QString name){setStatus(QFileInfo(name).fileName()+" "+((success)?" saved":(" "+reason)));}
};


class FileDownloader: public QObject
{
    Q_OBJECT

public:
    FileDownloader(QString iFilename, QString iUrl, QNetworkAccessManager *iManager)
        :_filename(iFilename), _url(iUrl), _netManager(iManager){}
    void download();

signals:
    void progressSignal(qint64 iRcvd, qint64 iTotal, QString filename);
    void downloaded(bool success, QString reason, QString filename);

private slots:
    void fileDownloadProgressSLot(qint64 iRcvd, qint64 iTotal);
    void fileDownloadedSlot();
    void fileDownloadAbort();

private:
    QString _filename;
    QString _url;
    QNetworkAccessManager* _netManager;
    QNetworkReply * _reply;
};


typedef QList<QPair<QString, QString> > ListFilesToDownload_t;

class MultiDownloader: public QWidget
{
    Q_OBJECT
public:
    MultiDownloader(const ListFilesToDownload_t& iList, QWidget* parent);
    ~MultiDownloader();

    void downloadOneItem();
protected:
    void closeEvent(QCloseEvent* event);
signals:
    void finished();
    void oneFileDownloaded(bool success, QString reason, QString filename);

private slots:
    void progressSlot(qint64 iRcvd, qint64 iTotal, QString filename);
    void downloadedSlot(bool success, QString reason, QString filename);

private:
    ListFilesToDownload_t _list;
    QNetworkAccessManager* _netMgr;
    QLabel * label;
    QLabel * currentLabel;
    QProgressBar * currentBar;
    QLabel * totalLabel;
    QProgressBar * totalBar;
    QPushButton * cancelButton;

    bool _finished;
    int _countTotal;
    int _countDone;

};

#endif // MAINWINDOW_H


