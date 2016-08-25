#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUrlQuery>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _authWebView(0),reconnectButton(0),cacheButton(0)
{
    // configure UI
    ui->setupUi(this);
    statusBar()->setSizeGripEnabled(false);
    setFixedSize(size());

    // initialize network manager
    _netManager = new QNetworkAccessManager(this);

    // configure all connections
    connect(statusBar(), SIGNAL(messageChanged(QString)), this, SLOT(statusMessageChangedSlot(QString)));
    connect(this, SIGNAL(stateChangedSignal(State_t)), this, SLOT(stateChangedSlot(State_t)));
    connect(_netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyReceivedSlot(QNetworkReply*)));

    // set initial state
    setState(NotStarted);
    setStatus("Starting...");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::stateChangedSlot(State_t iNewState)
{
    switch (iNewState)
    {
        case NotStarted:
            cleanMainWidget();
            requestToken();
            break;
        case TokenRequested: // nothing to do here
            break;

        case TokenFailed:
            cleanMainWidget();
            reconnectButton = new QPushButton("RECONNECT", ui->_mainWidget);
            connect (reconnectButton, SIGNAL(clicked(bool)), this, SLOT(retryAuthSLot()));
            reconnectButton->show();
            cacheButton = new QPushButton("CLEAR CACHE", ui->_mainWidget);
            cacheButton->setGeometry(reconnectButton->geometry().x()+reconnectButton->geometry().width() + 5,
                                     reconnectButton->geometry().y(),
                                     reconnectButton->geometry().width(),
                                     reconnectButton->geometry().height());
            connect (cacheButton, SIGNAL(clicked(bool)), this, SLOT(clearCacheSlot()));
            cacheButton->show();
        break;

        case TokenRecvd:
            //reconnectButton->deleteLater();
            //cacheButton->deleteLater();
            cleanMainWidget();
            _authWebView->deleteLater();
            requestAudios();
        break;

        case AudioListRequested:
        case AudioListRcvd:
        case AudioListFailed:
        case AudioListDisplayed:
            qDebug()<< "Not implemented state was set: " << iNewState;
    }
}

void MainWindow::cleanMainWidget()
{
    if (reconnectButton)
    {
        delete reconnectButton;
        reconnectButton = 0;
    }
    if (cacheButton)
    {
        delete cacheButton;
        cacheButton = 0;
    }

/*
   while ( QWidget* w = ui->_mainWidget->findChild<QWidget*>() )
   {
       //delete w;
       //w->close();
   }
*/
}

/************************************
 * Token-related methods and slots
 ************************************/

void MainWindow::requestToken()
{
    setState(TokenRequested);
    //if (_authWebView) {_authWebView->deleteLater();}

    _authWebView = new QWebEngineView(ui->_mainWidget);
    connect(_authWebView, SIGNAL(loadStarted()), this, SLOT(tokenViewLoadStartedSlot()));
    connect(_authWebView, SIGNAL(loadProgress(int)), this, SLOT(tokenViewloadProgressSlot(int)));
    connect(_authWebView, SIGNAL(loadFinished(bool)), this, SLOT(tokenViewLoadFinishedSlot(bool)));
    connect(_authWebView, SIGNAL(urlChanged(QUrl)), this, SLOT(tokenViewUrlChangedSlot(QUrl)));

    _authWebView->load(QUrl("https://oauth.vk.com/authorize"
                    "?client_id=5601291"
                    "&display=page"
                    "&redirect_uri=https://oauth.vk.com/blank.html"
                    "&scope=friends,photos,audio,video,docs,status,wall,groups,messages,email"
                    "&response_type=token"
                    "&v=5.53"));
    _authWebView->show();
}

void MainWindow::tokenViewLoadFinishedSlot(bool iResult)
{
    if (!iResult)
    {
        setStatus("Cannot connect to "+_authWebView->url().host());
        _authWebView->stop();
        _authWebView->deleteLater();
        setState(TokenFailed);
    }
    else
    {
        setStatus("Loaded "+ _authWebView->url().host());
    }
}

void MainWindow::tokenViewUrlChangedSlot(const QUrl &url)
{
    qDebug()<<"New url: " << url.toString();
    // hacky replace as VK returns token separated with #
    QString aStr = url.toString().replace("#", "?");
    QUrlQuery aQuery(  QUrl(aStr).query() );

    if (aQuery.hasQueryItem("access_token"))
    {
        _authWebView->stop();
        _authWebView->deleteLater();
        setStatus( "voila! we got the token!");
        _token = aQuery.queryItemValue("access_token");
        setState(TokenRecvd);
    }
    if(aQuery.hasQueryItem("error"))
    {
        _lastError = aQuery.queryItemValue("error") + ": " + aQuery.queryItemValue("error_description");
        _authWebView->stop();
        _authWebView->deleteLater();
        setStatus( "Error: " + _lastError);
        setState(TokenFailed);
    }
}

void MainWindow::clearCacheSlot()
{
    QWebEngineProfile::defaultProfile()->clearHttpCache();
    QWebEngineProfile::defaultProfile()->clearAllVisitedLinks();
    QWebEngineProfile::defaultProfile()->cookieStore()->deleteAllCookies();
}



/************************************
 * Token-related methods and slots
 ************************************/

void MainWindow::requestAudios()
{
    QString aUrlStr = "https://api.vk.com/method/audio.get?access_token="+_token;

//    qDebug() << "Conenct to host encrypted" ;
//    _netManager->connectToHostEncrypted("api.vk.com");

    qDebug() << "GET from " << aUrlStr;
     QNetworkReply* aReply = _netManager->get(QNetworkRequest(QUrl(aUrlStr)));
     connect(aReply, SIGNAL(finished()), this, SLOT(audiosFinishedSlot()));
     connect(aReply, SIGNAL(sslErrors(QList<QSslError>)), aReply, SLOT(ignoreSslErrors()));
     connect(_netManager, SIGNAL(encrypted(QNetworkReply*)), this, SLOT(encryptedSlot(QNetworkReply*)));
     setState(AudioListRequested);
}
