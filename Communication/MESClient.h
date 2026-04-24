#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace HMVision
{
class MESClient : public QObject
{
    Q_OBJECT
public:
    explicit MESClient(QObject* parent = nullptr);
    ~MESClient() override;

    void setBaseUrl(const QString& baseUrl);
    bool postInspectionResult(const QJsonObject& payload);
    bool fetchWorkOrder(const QString& workOrderId, QJsonObject& response);

private:
    bool performRequest(
        const QString& path,
        const QByteArray& payload,
        const QString& method,
        QByteArray& responseData);

    QString m_baseUrl;
    QNetworkAccessManager* m_networkManager = nullptr;
};
} // namespace HMVision
