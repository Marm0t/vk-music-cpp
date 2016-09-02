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

#include <QLayout>
#include <QFileDialog>
#include <QDir>

#include <QItemSelectionModel>
#include <QModelIndex>
#include <QIcon>
#include <QTextBrowser>

void MainWindow::keyPressEvent(QKeyEvent* ke)
{

    if (ke->key()==Qt::Key_Escape)
    {
        emit escPressed();
    }
    QMainWindow::keyPressEvent(ke); // base class implementation

}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    reconnectButton(0),_table(0),_directory(QDir::currentPath()),
    _dirLabel(0), _dirButton(0), _selectAllButton(0), _unselectAllButton(0), _downloadSelectedButton(0),
    _multiDownloader(0)
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

void MainWindow::showAbout()
{
    QString aVersion = "Version: ";
    aVersion.append(APP_VERSION);
    aVersion.append(" (build ");
    aVersion.append(__DATE__);
    aVersion.append(" ");
    aVersion.append(__TIME__);
    aVersion.append(")");


    QDialog dlg;
    dlg.setWindowTitle("About");
    QLabel *versonLbl = new QLabel(aVersion);
    versonLbl->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

    QPushButton *btn = new QPushButton("OK");
    QPushButton *btnQT = new QPushButton("About QT");
    QTextBrowser *text = new QTextBrowser();

    text->setSource( QUrl("qrc:/other/about.htm"));
    text->setOpenExternalLinks(true);

    connect(btn, SIGNAL(clicked(bool)), &dlg, SLOT(close()));
    connect(btnQT, &QPushButton::clicked, this,  [=]{QMessageBox::aboutQt(0);});

    QVBoxLayout * lay = new QVBoxLayout;
    lay->addWidget(versonLbl);
    lay->addWidget(text);
    lay->addWidget(btn);
    lay->addWidget(btnQT);
    dlg.setLayout(lay);

    dlg.exec();
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
            cleanMainWidget();
            requestAudios();
        break;

        case TokenFailed:
        case AudioListFailed:
            cleanMainWidget();
            reconnectButton = new QPushButton("RECONNECT", ui->_mainWidget);
            connect (reconnectButton, SIGNAL(clicked(bool)), this, SLOT(retryAuthSLot()));
            reconnectButton->show();
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
}

/************************************
 * Token-related methods and slots
 ************************************/

void MainWindow::requestToken()
{
    setState(TokenRequested);
    setStatus("Connecting to vk.com");

    _authenticator = new VkAuthenticator("5601291", "audio");
    connect(_authenticator, SIGNAL(error(QString)), this, SLOT(authenticatorErrorSlot(QString)));
    connect(_authenticator, SIGNAL(tokenReceived(QString)), this, SLOT(authenticatorTokenReceivedSlot(QString)));
    _authenticator->authenticate();
}

void MainWindow::authenticatorErrorSlot(QString str)
{
    setStatus( "Error: " + str);
    setState(TokenFailed);
    _authenticator->deleteLater();
}

void MainWindow::authenticatorTokenReceivedSlot(QString str)
{
    _token = str;
    setStatus( "voila! we got the token!");
    setState(TokenRecvd);
    _authenticator->deleteLater();
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

    if(!_dirLabel)               _dirLabel = new QLineEdit(_directory, ui->_mainWidget);
    if(!_dirButton)              _dirButton = new QPushButton("Browse", ui->_mainWidget);
    if(!_selectAllButton)        _selectAllButton = new QPushButton("Select all", ui->_mainWidget);
    if(!_unselectAllButton)      _unselectAllButton = new QPushButton("Unselect all", ui->_mainWidget);
    if(!_downloadSelectedButton) _downloadSelectedButton = new QPushButton("Download selected", ui->_mainWidget);
    if (!_table)                 _table = new QTableWidget(ui->_mainWidget);

    _dirLabel->setEnabled(false);
    _dirLabel->setAlignment(Qt::AlignLeft);
    QRect aMG = ui->_mainWidget->geometry(); // main widget geometry

    QHBoxLayout * hlay = new QHBoxLayout;

    QPushButton * _settingsButton = new QPushButton();
    _settingsButton->setIcon(QIcon(":img/About.png"));

    hlay->addWidget(_settingsButton);
    hlay->addWidget(_dirLabel);
    hlay->addWidget(_dirButton);
    hlay->addWidget(_selectAllButton);
    hlay->addWidget(_unselectAllButton);
    hlay->addWidget(_downloadSelectedButton);

    _table->setGeometry(0,0,
                        aMG.width(),
                        aMG.height()-_dirButton->geometry().height());

    QVBoxLayout * vlay = new QVBoxLayout;
    vlay->addWidget(_table);
    vlay->addLayout(hlay);
    ui->_mainWidget->setLayout(vlay);

    _table->setColumnCount(3);
    _table->setRowCount(_audioList.size()-1);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::MultiSelection);


    int width = _table->geometry().width();
    _table->setColumnWidth(0, width*.3);
    _table->setColumnWidth(1, width*.475);
    _table->setColumnWidth(2, width*.175);
    QStringList hdrs;
    hdrs.push_back("Artist");
    hdrs.push_back("Title");
    hdrs.push_back("Download");
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

    // connections
    connect(_dirButton, SIGNAL(clicked()), SLOT(browse()));
    connect(_selectAllButton, SIGNAL(clicked()), _table, SLOT(selectAll()));
    connect(_unselectAllButton, SIGNAL(clicked()), _table, SLOT(clearSelection()));
    connect(_table, SIGNAL(cellClicked(int,int)), this, SLOT(audiosTableCellClickedSlot(int,int)));
    connect(_downloadSelectedButton, SIGNAL(clicked(bool)), this, SLOT(audioDownloadAllClickedSlot()));
    connect(_settingsButton, SIGNAL(clicked(bool)), this, SLOT(showAbout()));

    // show everything
    _downloadSelectedButton->show();
    _dirLabel->show();
    _dirButton->show();
    _selectAllButton->show();
    _unselectAllButton->show();
    _table->show();
    setTableButtonsEnabled(true);
}


void MainWindow::browse()
{
    QString directory = QFileDialog::getExistingDirectory(this,
                            tr("Find Files"), QDir::currentPath());
    if (!directory.isEmpty()) _directory = directory;
    _dirLabel->setText(_directory);

}

void MainWindow::audiosTableCellClickedSlot(int row, int column)
{
    // download audio if third columd clicked
    if (column == 2)
    {
        setTableButtonsEnabled(false);

        QString aFilename = QDir(_directory).filePath(
                _audioList.at(row).toObject().value("artist").toString().remove(':')
                + " - "
                + _audioList.at(row).toObject().value("title").toString().remove(':')
                + ".mp3");

        QString aUrl = _audioList.at(row).toObject().value("url").toString();
        setStatus("Downloading " + aFilename + " from " + aUrl);

        FileDownloader* aDownloader = new FileDownloader(aFilename, aUrl, _netManager);
        // connect all signals to slots
        connect(aDownloader, SIGNAL(downloaded(bool,QString, QString)), this, SLOT(audioDownloaded(bool,QString,QString)));
        connect(aDownloader, SIGNAL(progressSignal(qint64,qint64,QString)),
                this, SLOT(audioDownloadingProgress(qint64,qint64,QString)));
        connect(this, SIGNAL(escPressed()), aDownloader, SLOT(fileDownloadAbort()));

        aDownloader->download();
        setState(AudioDownloading);
    }
}


void MainWindow::audioDownloaded(bool success, QString reason, QString filename)
{
    if(!success)
    {
        QMessageBox::warning(this, "OOPS!", "Cannot download " + filename+"\n"+reason);
    }
    setTableButtonsEnabled(true);
    setStatus(reason);
    setState(AudioListDisplayed);
}


void MainWindow::setTableButtonsEnabled(bool val)
{
    _table->setEnabled(val);
    _dirButton->setEnabled(val);
    _selectAllButton->setEnabled(val);
    _unselectAllButton->setEnabled(val);
    _downloadSelectedButton->setEnabled(val);
    if(val) setStatus("Select audios to download or download them one by one");
}


void MainWindow::audioDownloadAllClickedSlot()
{
    // build list of files to be downloaded
    QItemSelectionModel * model = _table->selectionModel();
    QList<QPair<QString, QString> > aListToDownload;

    foreach (QModelIndex idx, model->selectedRows()) {
        qDebug() << "Selected row: " << idx.row();
        aListToDownload.append(
                    QPair<QString, QString>(
                        _audioList.at( idx.row()).toObject().value("url").toString(),
                        QDir(_directory).filePath(
                            _audioList.at( idx.row()).toObject().value("artist").toString()
                            + " - "
                            + _audioList.at( idx.row()).toObject().value("title").toString()
                            + ".mp3") ));

    }
    qDebug()<<"list to download: " << aListToDownload;

    // disable current interface
    setTableButtonsEnabled(false);

    _multiDownloader = new MultiDownloader(aListToDownload, this);
    // setup connections with multidownloader window
    connect(_multiDownloader, &MultiDownloader::finished, this,  [=]{this->setTableButtonsEnabled(true);});
    connect(_multiDownloader, SIGNAL(oneFileDownloaded(bool,QString,QString)),
            this, SLOT(audioDownloadedUpdateStatusSlot(bool,QString,QString)));
    connect(this, SIGNAL(escPressed()), _multiDownloader, SLOT(close()));

    _multiDownloader->show();
    _multiDownloader->downloadOneItem();
}


/*******************
 * FileDownloader
 * *****************/
void FileDownloader::download()
{
    // check if file already exist
    if (QFile::exists(_filename))
    {
        emit downloaded(false, "File "+_filename+" already exists.", _filename );
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
        emit downloaded(false, aReply->errorString(), _filename);
        return;
    }
    if (statusCode != 200)
    {
        emit downloaded(false, aReply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString(), _filename);
        return;
    }
    if (QFile::exists(_filename))
    {
        emit downloaded(false, "File "+_filename+" already exists", _filename);
        return;
    }

    QFile file(_filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        emit downloaded(false, "Cannot create file "+_filename, _filename);
        return;
    }
    qint64 aRes = file.write(aData);
    if (aRes>1)
    {
        emit downloaded(true, "File saved ["+ _filename+"]", _filename);
        file.close();

    }else
    {
        emit downloaded(false, "Cannot write to the file ["+ _filename+"]: "+QString::number(aRes), _filename);
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


/******************
 * MultiDownloader
 *****************/
MultiDownloader::MultiDownloader(const ListFilesToDownload_t& iList, QWidget* parent):
    QWidget(parent, Qt::Window),
    _list(iList), _finished(false), _countTotal(iList.size()), _countDone(0)
{
    setWindowFlags( Qt::Tool );
    setWindowModality( Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Download");
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    if(parent)
        setMaximumWidth(parent->width());

    _netMgr = new QNetworkAccessManager(this);

    label = new QLabel("Downloading...");
    currentLabel = new QLabel();
    currentBar = new QProgressBar();
    totalLabel = new QLabel();
    totalBar = new QProgressBar();
    cancelButton = new QPushButton("Cancel");

    connect(cancelButton, SIGNAL(clicked(bool)), this, SLOT(close()));

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(label);
    layout->addWidget(currentLabel);
    layout->addWidget(currentBar);
    layout->addWidget(totalLabel);
    layout->addWidget(totalBar);
    layout->addWidget(cancelButton);

    setLayout(layout);
    show();
}
MultiDownloader::~MultiDownloader(){_netMgr->deleteLater();}

void MultiDownloader::downloadOneItem()
{
    if (_list.empty())
    {
        qDebug() << "Nothing to download... Closing multiloader";
        _finished = true;
        this->close();
        return;
    }

    QString url = _list.begin()->first;
    QString filename = _list.begin()->second;
    _list.removeFirst();
    FileDownloader * loader = new FileDownloader(filename, url, _netMgr);
    connect(loader, SIGNAL(progressSignal(qint64,qint64,QString)), this, SLOT(progressSlot(qint64,qint64,QString)));
    connect(loader, SIGNAL(downloaded(bool,QString,QString)), this, SLOT(downloadedSlot(bool,QString,QString)));
    qDebug()<< "Downloading " << filename;
    currentLabel->setText("Downloading " + filename);
    loader->download();
}


void MultiDownloader::closeEvent(QCloseEvent* event)
{
    if(_finished)
    {
        emit finished();
        event->accept();
        return;
    }
    // ask if the user it sure he wants to cancel the download
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Cancel download",
                                  "Are you sure you want to cancel download?\n"
                                  "Downloaded files won't be deleted.",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        event->ignore();
    }else{
        qDebug() << "Closing multidownloader window";
        emit finished();
        event->accept();
    }
}

void MultiDownloader::progressSlot(qint64 iRcvd, qint64 iTotal, QString filename)
{
    currentBar->setValue(100*iRcvd/iTotal);
    totalBar->setValue( _countDone*100/_countTotal +  ((double)iRcvd/iTotal) * 100/_countTotal );
    currentLabel->setText(filename+": "+QString::number(iRcvd/1024)+" out of "+QString::number(iTotal/1024)+ " Kbytes");
    totalLabel->setText("File "+QString::number(_countDone+1)+" out of "+QString::number(_countTotal));
}
void MultiDownloader::downloadedSlot(bool success, QString reason, QString filename)
{
    if (!success)
    {
        qDebug()<< "Download error: "+reason;
    }else qDebug()<< "Success";
    emit oneFileDownloaded(success, reason, filename);
    _countDone++;
    downloadOneItem();
}
