#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStatusBar>
#include <QDebug>
#include <QWebEngineView>
#include <QUrl>



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

    void setState(State_t iNewState){qDebug() << "New state: " <<iNewState; _state = iNewState; emit stateChangedSignal(iNewState);}
    void setStatus(const QString& iStatus){statusBar()->showMessage(iStatus);}


    void cleanMainWidget();

signals:
    void stateChangedSignal(State_t newState);


private slots:
    void statusMessageChangedSlot(const QString& iNewStatus){qDebug() << "New status message: " << iNewStatus;}
    void stateChangedSlot(State_t iNewState);

    // token view slots
    void requestToken();
    void tokenViewLoadStartedSlot(){setStatus("Start load for "+_authWebView->url().host());}
    void tokenViewloadProgressSlot(int iProgress){setStatus(QString::number(iProgress)+"% "+_authWebView->url().host());}
    void tokenViewLoadFinishedSlot(bool iResult);
    void tokenViewUrlChangedSlot(const QUrl &url);

};

#endif // MAINWINDOW_H

