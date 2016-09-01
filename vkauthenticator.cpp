#include "vkauthenticator.h"
#include <QLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QNetworkRequest>
#include <QRegExp>
#include <QUrlQuery>
#include <QMessageBox>
#include <QLineEdit>

class UserPassDialog : public QDialog
{

public:
    UserPassDialog(QWidget *parent = 0);
    ~UserPassDialog();

    const QString& getUsername() ;
    const QString& getPassword() ;

private:
    QLineEdit *_usernameLineEdit;
    QLineEdit *_passLineEdit;
    QString _username;
    QString _password;
};



VkAuthenticator::VkAuthenticator(QString appId, QString scope)
        :_appId(appId), _scope(scope), _state(NotSet)
{
    // setup connections
    connect(&_netMgr, SIGNAL(finished(QNetworkReply*)), this, SLOT(messageReceived(QNetworkReply*)));
}

void VkAuthenticator::setState(VkAuthState_t newState)
{
    qDebug()<< "VkAuthenticator: State changed from "<<_state<<" to "<<newState; _state = newState;
}

void VkAuthenticator::emitError(QString message)
{
    setState(Error);
    _errorMessage = message;
    qDebug() << "VkAuthenticator: " << _errorMessage;
    emit error(_errorMessage);
}


void VkAuthenticator::redirectedSLot(QUrl url)
{
    qDebug()<<"REDIRECTED TO " << url;
    //_netMgr.post(QNetworkRequest(url), QByteArray());
    _netMgr.get(QNetworkRequest(url));
}

void VkAuthenticator::authenticate()
{
    if(_state==Error)_state=NotSet;
    if(_state!=NotSet)
    {
        emitError("Authenticate in wrong state: " + QString::number(_state));
        return;
    }
    // Request 1:
    QString url = "https://oauth.vk.com/authorize"
                  "?client_id="+_appId+
                  "&redirect_uri=https://oauth.vk.com/blank.html"+
                  "&display=mobile"+
                  "&scope="+_scope+
                  "&response_type=token"+
                  "&v=5.53";
    qDebug()<<"VkAuthenticator: Sending" << url;

    QNetworkReply * reply = _netMgr.get(QNetworkRequest(url));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorOccured(QNetworkReply::NetworkError)));
    setState(AuthorizeRerquestSent);
}

void VkAuthenticator::errorOccured(QNetworkReply::NetworkError err)
{
    qDebug()<<"VkAuthenticator: Error: " << err;
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(this->sender());
    if(reply)
    {
        _errorMessage = reply->errorString();
    }
    setState(Error);
    emit error(QString::number(err) +" " + _errorMessage);
}

void VkAuthenticator::postCredentials(QByteArray &data)
{
    //qDebug()<<"POSTCREDENTIALS";
    //retrieve form from data and generate request
    int formBeginPos = data.indexOf("<form");
    int formEndPos = data.indexOf("</form>")+7; // 7 = length of "</form>"
    if (formBeginPos<0 || formEndPos<0)
    {
        emitError("Cannot find form on page: " + QString::number(formBeginPos)+" " +QString::number(formEndPos));
        return;
    }
    QStringList formlist = QString(data.mid(formBeginPos, formEndPos-formBeginPos)).split("\n");
    QString pattern = "<input.*name=\"(.*)\".*value=\"(.*)\".*>";
    QRegExp re(pattern);

    QRegExp urlRe("<form.*action=\"(.*)\".*>");
    QString url;

    if (urlRe.indexIn(formlist.first()) != -1)
    {
        url = urlRe.cap(1);
        qDebug()<< "VkAuthenticator: Auth request will be sent to " << url;
    }
    else
    {
        emitError("Cannot get URL for authentication");
        return;
    }

    QUrlQuery postData;
    foreach(QString str, formlist)
    {
        int pos = 0;
        while ((pos = re.indexIn(str, pos)) != -1) {
            //qDebug() << re.cap(1) << "\t" << re.cap(2);
            pos += re.matchedLength();
            postData.addQueryItem(re.cap(1), re.cap(2));
        }
    }
    postData.removeAllQueryItems("email");
    postData.removeAllQueryItems("pass");
    qDebug()<< "VkAuthenticator: Post data (before username and password requested)" << postData.toString();

    // show username/pass dialog
    UserPassDialog dialog;
    if (dialog.exec())
    {
        postData.addQueryItem("email", dialog.getUsername());
        postData.addQueryItem("pass", dialog.getPassword());
    }
    else
    {
        emitError("User denied login");
        return;
    }

    //send auth request to the server
    QNetworkRequest authReq(url);
    authReq.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    authReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    //qDebug()<<"VkAuthenticator: Sending POST with User credentials";
    QNetworkReply *rpl = _netMgr.post(authReq, postData.toString().toUtf8());
    connect(rpl, SIGNAL(redirected(QUrl)), this, SLOT(redirectedSLot(QUrl)));
    setState(CredentialsSent);
}

void VkAuthenticator::messageReceived(QNetworkReply* reply)
{
    QByteArray data = reply->readAll();
    qDebug() << "VkAuthenticator: ==> Message received. Code " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
             << "\nURL: " << reply->url().toString()
             << "state: " << _state;
    //qDebug() << QString(data).split("\n");

    if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302)
    {
        qDebug() << "VkAuthenticator: Redirected: Found.";
        //qDebug() << "URL of reply             : " <<  reply->url();
        //qDebug() << "URL of request           : " <<  reply->request().url();
        qDebug() << "VkAuthenticator: URL of redirection target: " <<  reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        _netMgr.get(QNetworkRequest(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString()));
        return;
    }

    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 &&
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 0) // special case for token redirection
    {
        emitError(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        return;
    }
    // split reply message to a list (to make parsing more easy)
    QStringList replyList;
    foreach(QString str, QString(data).split("\n")){ replyList += str.split(", "); }

    switch(_state)
    {
    case NotSet:
        // nothing to do here
        qDebug() << "VkAuthenticator: Something received while state is NotSet";
        break;
    case AuthorizeRerquestSent:
        postCredentials(data);
        break;
    case CredentialsSent:
        // check if credentials are correct
        {
            QRegExp reServiceWarning("<div class=\"service_msg service_msg_warning\">(.*)</div>");
            QRegExp reCaptchaWarning("<img id=\"captcha\".*src=\"(.*)\" />");
            foreach(QString str, replyList)
            {
                if(reServiceWarning.indexIn(str, 0) != -1)
                {
                    //qDebug() << "VkAuthenticator: OPENING warning";
                    QMessageBox::warning(0, "Warning", reServiceWarning.cap(1));
                    postCredentials(data);
                    return;
                }
                if(reCaptchaWarning.indexIn(str, 0) != -1)
                {
                    //qDebug() << "VkAuthenticator: CAPTCHA warning";
                    QMessageBox::warning(0, "CAPTCHA Warning", "CAPTCHA is requested.\n"
                                                               "But CAPTCHA entering is not implemented\n"
                                                               "Try one more time\n"
                                                               "And please, dont click buttons that often :)");
                    postCredentials(data);
                    return;
                }
            }
            qDebug() << "VkAuthenticator: Credentials seems to be correct";
        }

        // check if permissions needed and request them
        if(QString(data).indexOf("act=grant_access")>0)
        {
            QRegExp re("<span class=\"oauth_access_item\">(.+)</span>");
            QString msgToShow = "Application is requesting access to";

            QRegExp permissionsActionUrlRE("<form method=\"post\" action=\"(.*)\">");
            QString permissionsActionUrl;

            foreach(QString str, replyList)
            {
                //qDebug() << "CHECKING LINE" << str;
                int pos = 0;
                while ((pos = re.indexIn(str, pos)) != -1)
                {
                    pos += re.matchedLength();
                    if (!re.cap(1).isEmpty()) msgToShow.append("\n"+re.cap(1));
                }
                if(permissionsActionUrl.isEmpty())
                {
                    if(permissionsActionUrlRE.indexIn(str, 0) != -1)
                        permissionsActionUrl = permissionsActionUrlRE.cap(1);
                }
            }
            msgToShow.append("\nAre you agree to grant access permissions to the application?");
            //qDebug() << "MESsGE: " << msgToShow;

            // ask user if he wants to grant permissions
            int agree = QMessageBox::question(0, "Permissions needed",msgToShow, QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);
            if (agree==QMessageBox::No)
            {
                qDebug()<< "VkAuthenticator: Not OK to give permissions";
                emitError("User denied permissions request");
                return;
            }else{
                qDebug()<< "VkAuthenticator: Permissions granted. Sending POST to " << permissionsActionUrl;
                QNetworkRequest aReq;
                aReq.setUrl(permissionsActionUrl);
                aReq.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
                _netMgr.get(aReq);
                setState(GrantPermissionSent);
            }
        }// end if permisisons needed
        // no break on purpose, because permisisons could be already granted
    case GrantPermissionSent:
        //qDebug() << "VkAuthenticator: CHECK IF WE FIND TOKEN IN URL";
        // at this point access token supposed to be in reply's url
        QString aStr = reply->url().toString().replace("#", "?");
        QUrlQuery aQuery(  QUrl(aStr).query() );
        _token = aQuery.queryItemValue("access_token");
        if (!_token.isEmpty())
        {
            qDebug() << "VkAuthenticator: TOKEN RECIEVED! " << _token;
            setState(TokenReceived);
            emit tokenReceived(_token);
        }
        else
        {
            qDebug() << "VkAuthenticator: Token not found";
        }
        break;
    }
}


UserPassDialog::UserPassDialog(QWidget *parent)
    :QDialog(parent)
{
    setWindowFlags( Qt::Tool );
    this->setWindowTitle("Gain Access to VK");
    QLabel* _justLabel = new QLabel("You should log in to VK to proceed.");
    QLabel* _usernameLabel = new QLabel("Phone or email:");
    _usernameLineEdit = new QLineEdit();

    QLabel* _passwordLabel = new QLabel("Password:");
    _passLineEdit = new QLineEdit();
    _passLineEdit->setEchoMode(QLineEdit::Password);
    _passLineEdit->setInputMethodHints(Qt::ImhHiddenText| Qt::ImhNoPredictiveText|Qt::ImhNoAutoUppercase);

    QPushButton *loginButton = new QPushButton(tr("Login"));
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    loginButton->setDefault(true);
    loginButton->setAutoDefault(true);
    cancelButton->setDefault(false);
    cancelButton->setAutoDefault(false);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
    buttonBox->addButton(loginButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *vlay = new QVBoxLayout;
    vlay->addWidget(_justLabel);
    vlay->addSpacing(15);
    vlay->addWidget(_usernameLabel);
    vlay->addWidget(_usernameLineEdit);
    vlay->addWidget(_passwordLabel);
    vlay->addWidget(_passLineEdit);
    vlay->addWidget(buttonBox);

    this->setLayout(vlay);
}
UserPassDialog::~UserPassDialog()
{
    //cleanup password
    _username.replace(0, _username.size(), "\0");
    _password.replace(0, _username.size(), "\0");
    _usernameLineEdit->setText(_username);
    _passLineEdit->setText(_password);
}

const QString& UserPassDialog::getUsername(){_username=_usernameLineEdit->text(); return _username;}
const QString& UserPassDialog::getPassword(){_password=_passLineEdit->text(); return _password;}
