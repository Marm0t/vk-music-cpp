#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUrlQuery>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


#include <QTableWidget>
#include <QTableWidgetItem>
#include <QScrollBar>


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
        case TokenRequested:        // nothing to do here
        case AudioListRequested:    // nothing to do here
            break;

        case TokenRecvd:
            //reconnectButton->deleteLater();
            //cacheButton->deleteLater();
            cleanMainWidget();
            _authWebView->deleteLater();
            requestAudios();
        break;

        case TokenFailed:
        case AudioListFailed:
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

        case AudioListRcvd:
            showAudioTable();

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
 * Audios-related methods and slots
 ************************************/

void MainWindow::requestAudios()
{
     QUrl aUrl(QString("https://api.vk.com/method/audio.get?access_token="+_token+"&need_user=1"));
     setStatus("Request audios from " + aUrl.host());
     QNetworkReply* aReply = _netManager->get(QNetworkRequest(aUrl));
     connect(aReply, SIGNAL(finished()), this, SLOT(audiosFinishedSlot()));
     connect(aReply, SIGNAL(sslErrors(QList<QSslError>)), aReply, SLOT(ignoreSslErrors()));
     connect(_netManager, SIGNAL(encrypted(QNetworkReply*)), this, SLOT(encryptedSlot(QNetworkReply*)));
     setState(AudioListRequested);
}

void MainWindow::audiosFinishedSlot()
{
    QNetworkReply *aReply = qobject_cast<QNetworkReply*>(this->sender());
    int statusCode = aReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    setStatus("Audio reply received. Status "+aReply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());

    QByteArray aStrReply = aReply->readAll();
    qDebug() << "Reply ["<<statusCode<<"] of " << aStrReply.size() << " symbols";
    qDebug() << "headers" << aReply->rawHeaderList();
    qDebug() << "error" << aReply->error();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(aStrReply, &err);

    if (err.error != QJsonParseError::NoError)
    {
        setStatus("Error: " + err.errorString());
        setState(AudioListFailed);
        return;
    }
    if (doc.object().contains("error"))
    {
        setStatus("Error: " + doc.object().value("error").toObject().value("error_msg").toString());
        setState(AudioListFailed);
        return;
    }
    if (!doc.object().contains("response"))
    {
        setStatus("Error: incorrect reply format");
        setState(AudioListFailed);
        return;
    }
    _audioList = doc.object().value("response").toArray();
    _audioList.removeFirst();
    setStatus(QString::number(_audioList.size()-1)
              + " audios found for "
              + doc.object().value("response").toArray().at(0).toObject().value("name").toString());

    setState(AudioListRcvd);
}


void MainWindow::showAudioTable()
{
    setStatus("Building audios table");
    QTableWidget *aTable = new QTableWidget(ui->_mainWidget);
    aTable->setGeometry(0,0,
                        ui->_mainWidget->geometry().width(),
                        ui->_mainWidget->geometry().height());

    aTable->setColumnCount(3);
    aTable->setRowCount(_audioList.size()-1);


    int width = aTable->geometry().width();
    aTable->setColumnWidth(0, width*.3);
    aTable->setColumnWidth(1, width*.5);
    aTable->setColumnWidth(2, width*.175);
    QStringList hdrs;
    hdrs.push_back("Artist");
    hdrs.push_back("Title");
    hdrs.push_back("");
    aTable->setHorizontalHeaderLabels(hdrs);
    aTable->verticalHeader()->hide();


    int row = 0;
    foreach (QJsonValue obj, _audioList) {
        QTableWidgetItem *artistItem = new QTableWidgetItem(obj.toObject().value("artist").toString());
        artistItem->setFlags(Qt::NoItemFlags|Qt::ItemIsEnabled);
        aTable->setItem(row, 0, artistItem);
        QTableWidgetItem *titleItem = new QTableWidgetItem(obj.toObject().value("title").toString());
        titleItem->setFlags(Qt::NoItemFlags|Qt::ItemIsEnabled);
        aTable->setItem(row, 1, titleItem);
        ++row;
    }
    aTable->show();
}


