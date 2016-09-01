#ifndef VKAUTHENTICATOR_H
#define VKAUTHENTICATOR_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDialog>

class VkAuthenticator : public QObject
{
Q_OBJECT
    enum VkAuthState_t
    {
        NotSet = 0,
        AuthorizeRerquestSent,
        CredentialsSent,
        GrantPermissionSent,
        TokenReceived,
        Error
    };
    public:
        VkAuthenticator(QString appId,
                        QString scope="notify,friends,photos,audio,video,docs,notes,pages,status,wall,groups,messages");
        const QString& getToken(){return _token;}
        const QString& getError(){return _errorMessage;}

    public slots:
        void authenticate();

    signals:
        void stateChanged(VkAuthState_t newState);
        void error(QString err);
        void tokenReceived(QString token);

    private:
        QString _appId;
        QString _scope;
        QNetworkAccessManager _netMgr;
        VkAuthState_t _state;
        QString _errorMessage;
        QString _token;
        void emitError(QString message);
        void setState(VkAuthState_t newState);

    private slots:
        void redirectedSLot(QUrl url);
        void errorOccured(QNetworkReply::NetworkError err);
        void postCredentials(QByteArray &data);
        void messageReceived(QNetworkReply* reply);
};

#endif // VKAUTHENTICATOR_H

