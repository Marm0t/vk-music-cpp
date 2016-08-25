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
    AudioListDisplayed
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


    void setState(State_t iNewState){qDebug() << "New state: " <<iNewState; _state = iNewState; emit stateChangedSignal(iNewState);}
    void setStatus(const QString& iStatus){statusBar()->showMessage(iStatus);}
    void cleanMainWidget();

    void requestToken();
    void requestAudios();


signals:
    void stateChangedSignal(State_t newState);


private slots:
    void statusMessageChangedSlot(const QString& iNewStatus){qDebug() << "New status message: " << iNewStatus;}
    void stateChangedSlot(State_t iNewState);
    void replyReceivedSlot(QNetworkReply* iReply){qDebug() << "Reply received: "<< iReply->readAll();}

    // token view slots
    void tokenViewLoadStartedSlot(){setStatus("Start load for "+_authWebView->url().host());}
    void tokenViewloadProgressSlot(int iProgress){setStatus(QString::number(iProgress)+"% "+_authWebView->url().host());}
    void tokenViewLoadFinishedSlot(bool iResult);
    void tokenViewUrlChangedSlot(const QUrl &url);
    void retryAuthSLot(){setState(NotStarted);}
    void clearCacheSlot();

    // request audios slots
    void audiosDownloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal){"Getting audios list: "+QString::number(100 * bytesReceived/bytesTotal)+"%";}
    void audiosFinishedSlot()
    {
        QNetworkReply *aReply = qobject_cast<QNetworkReply*>(this->sender());
        int statusCode = aReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Reply ["<<statusCode<<"]: " << aReply->readAll();
        // if good:
        // setState(AudioListRequested);
        // else:
        // setState(AudioListFailed);
    }

    void encryptedSlot(QNetworkReply* reply)
    {
        qDebug() << "ENCRYPTED!";
    }
};

#endif // MAINWINDOW_H

