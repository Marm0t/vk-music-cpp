#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUrlQuery>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


#include <QTableWidget>
#include <QTableWidgetItem>
#include <QScrollBar>

#include <QFile>
#include <QDataStream>
#include <QMessageBox>

void MainWindow::keyPressEvent(QKeyEvent* ke)
{

    if (ke->key()==Qt::Key_Escape && _state==AudioDownloading)
    {
        emit escPressed();
    }
    QMainWindow::keyPressEvent(ke); // base class implementation

}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _authWebView(0),reconnectButton(0),cacheButton(0),_table(0)
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
            // nothing to do here
            break;
        case AudioDownloading:

            break;
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
    aReply->deleteLater();
}


void MainWindow::showAudioTable()
{
    setStatus("Building audios table");
    if (!_table)
        _table = new QTableWidget(ui->_mainWidget);
    _table->setGeometry(0,0,
                        ui->_mainWidget->geometry().width(),
                        ui->_mainWidget->geometry().height());

    _table->setColumnCount(3);
    _table->setRowCount(_audioList.size()-1);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::MultiSelection);


    int width = _table->geometry().width();
    _table->setColumnWidth(0, width*.3);
    _table->setColumnWidth(1, width*.5);
    _table->setColumnWidth(2, width*.175);
    QStringList hdrs;
    hdrs.push_back("Artist");
    hdrs.push_back("Title");
    hdrs.push_back("");
    _table->setHorizontalHeaderLabels(hdrs);
    _table->verticalHeader()->hide();
    int row = 0;
    foreach (QJsonValue obj, _audioList) {
        QTableWidgetItem *artistItem = new QTableWidgetItem(obj.toObject().value("artist").toString());
        artistItem->setFlags(Qt::NoItemFlags|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsSelectable	);
        _table->setItem(row, 0, artistItem);
        QTableWidgetItem *titleItem = new QTableWidgetItem(obj.toObject().value("title").toString());
        titleItem->setFlags(Qt::NoItemFlags|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsSelectable	);
        _table->setItem(row, 1, titleItem);
        QTableWidgetItem *dldItem = new QTableWidgetItem(obj.toObject().value("url").toString());
        dldItem->setFlags(Qt::NoItemFlags|Qt::ItemIsEnabled	);
        _table->setItem(row, 2, dldItem);
        ++row;
    }
    connect(_table, SIGNAL(cellClicked(int,int)), this, SLOT(audiosTableCellClickedSlot(int,int)));
    _table->show();
    setStatus("Select audios to download or download them one by one");
}

void MainWindow::audiosTableCellClickedSlot(int row, int column)
{
    // download audio if third columd clicked
    if (column == 2)
    {
        _table->setEnabled(false);

        QString aDir = "";
        QString aFilename = aDir
                + _audioList.at(row).toObject().value("artist").toString()
                + " - "
                + _audioList.at(row).toObject().value("title").toString()
                + ".mp3";
        QString aUrl = _audioList.at(row).toObject().value("url").toString();
        setStatus("Downloading " + aFilename + " from " + aUrl);

        FileDownloader* aDownloader = new FileDownloader(aFilename, aUrl, _netManager);
        // connect all signals to slots
        connect(aDownloader, SIGNAL(downloaded(bool,QString)), this, SLOT(audioDownloaded(bool,QString)));
        connect(aDownloader, SIGNAL(progressSignal(qint64,qint64,QString)), this, SLOT(audioDownloadingProgress(qint64,qint64,QString)));
        connect(this, SIGNAL(escPressed()), aDownloader, SLOT(fileDownloadAbort()));

        aDownloader->download();
        setState(AudioDownloading);
    }
}


void MainWindow::audioDownloaded(bool success, QString reason)
{
    if(!success)
    {
        QMessageBox::warning(this, "OOPS!", reason);
    }
    _table->setEnabled(true);
    setStatus(reason);
    setState(AudioListDisplayed);
}



/*******************
 * FileDownloader
 * *****************/
void FileDownloader::download()
{
    // check if file already exist
    if (QFile::exists(_filename))
    {
        emit downloaded(false, "File "+_filename+" already exists." );
        return;
    }
    // start download
    _reply = _netManager->get(QNetworkRequest(_url));

    // connect all sugnals/slots for reply object
    connect(_reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(fileDownloadProgressSLot(qint64, qint64)) );
    connect(_reply, SIGNAL(finished()), this, SLOT(fileDownloadedSlot()));
}


void FileDownloader::fileDownloadedSlot()
{
    QNetworkReply *aReply = qobject_cast<QNetworkReply*>(this->sender());
    int statusCode = aReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray aData = aReply->readAll();
    qDebug() << "Downloaded " << aData.size() << " bytes with status "<< statusCode << " for file " << _filename;
    qDebug() << "error" << aReply->error();

    if(aReply->error() != QNetworkReply::NoError)
    {
        emit downloaded(false, aReply->errorString());
        return;
    }
    if (statusCode != 200)
    {
        emit downloaded(false, aReply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        return;
    }
    if (QFile::exists(_filename))
    {
        emit downloaded(false, "File "+_filename+" already exists");
        return;
    }

    QFile file(_filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        emit downloaded(false, "Cannot create file "+_filename);
        return;
    }
    qint64 aRes = file.write(aData);
    if (aRes>1)
    {
        emit downloaded(true, "File saved ["+ _filename+"]");
        file.close();

    }else
    {
        emit downloaded(false, "Cannot write to the file ["+ _filename+"]: "+QString::number(aRes));
        file.remove();
    }
    this->deleteLater();
}


void FileDownloader::fileDownloadProgressSLot(qint64 iRcvd, qint64 iTotal)
{
    emit progressSignal(iRcvd, iTotal, _filename);
}

void FileDownloader::fileDownloadAbort()
{
    _reply->abort();
    _reply->deleteLater();
    this->deleteLater();
}
