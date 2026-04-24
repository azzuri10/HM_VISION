#include "MESClient.h"

#include "../Core/Logger.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace HMVision
{
MESClient::MESClient(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

MESClient::~MESClient() = default;

void MESClient::setBaseUrl(const QString& baseUrl)
{
    m_baseUrl = baseUrl;
}

bool MESClient::postInspectionResult(const QJsonObject& payload)
{
    QByteArray response;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return performRequest("/inspection/results", body, "POST", response);
}

bool MESClient::fetchWorkOrder(const QString& workOrderId, QJsonObject& response)
{
    QByteArray responseData;
    const bool ok = performRequest(QString("/workorders/%1").arg(workOrderId), {}, "GET", responseData);
    if (!ok)
    {
        return false;
    }

    const auto doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject())
    {
        return false;
    }
    response = doc.object();
    return true;
}

bool MESClient::performRequest(
    const QString& path, const QByteArray& payload, const QString& method, QByteArray& responseData)
{
    if (m_baseUrl.isEmpty())
    {
        Logger::warning("MES base URL is empty.");
        return false;
    }

    const QUrl url(QString("%1%2").arg(m_baseUrl, path));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nullptr;
    if (method == "POST")
    {
        reply = m_networkManager->post(request, payload);
    }
    else if (method == "PUT")
    {
        reply = m_networkManager->put(request, payload);
    }
    else
    {
        reply = m_networkManager->get(request);
    }

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    responseData = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok)
    {
        Logger::warning(QString("MES request failed: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
    return ok;
}
} // namespace HMVision
