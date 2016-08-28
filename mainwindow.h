#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStatusBar>
#include <QDebug>
#include <QWebEngineView>
#include <QUrl>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QPushButton>
#include <QNetworkReply>
#include <QJsonArray>
#include <QTableWidget>
#include <QKeyEvent>

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    State_t _state;
    QWebEngineView *_authWebView;
    QString _token;
    QString _lastError;

    QPushButton *reconnectButton;
    QPushButton *cacheButton;

    QNetworkAccessManager *_netManager;

    QJsonArray _audioList;
    QTableWidget *_table;


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

private slots:
    void statusMessageChangedSlot(const QString& iNewStatus){qDebug() << "New status message: " << iNewStatus;}
    void stateChangedSlot(State_t iNewState);
    void replyReceivedSlot(QNetworkReply* iReply){qDebug() << "Reply received from " << iReply->url().host();}
    void encryptedSlot(QNetworkReply* ){qDebug() << "ENCRYPTED!";}

    // token view slots
    void tokenViewLoadStartedSlot(){setStatus("Loading from "+_authWebView->url().host());}
    void tokenViewloadProgressSlot(int iProgress){setStatus(QString::number(iProgress)+"% "+_authWebView->url().host());}
    void tokenViewLoadFinishedSlot(bool iResult);
    void tokenViewUrlChangedSlot(const QUrl &url);
    void retryAuthSLot(){setState(NotStarted);}
    void clearCacheSlot();

    // request audios slots
    void audiosListDownloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal){setStatus("Downloading list of audios: "+QString::number(100 * bytesReceived/bytesTotal)+"%");}
    void audiosFinishedSlot();

    // download audio slots
    void audiosTableCellClickedSlot(int row, int column);
    void audioDownloadingProgress(qint64 iRcvd, qint64 iTotal, QString filename){setStatus("Downloading "+filename+" "+QString::number(100 * iRcvd/iTotal)+"%");}
    void audioDownloaded(bool success, QString reason);
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
    void downloaded(bool success, QString reason);

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


#endif // MAINWINDOW_H

