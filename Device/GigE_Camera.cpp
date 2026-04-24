#include "GigE_Camera.h"

#include <QColor>
#include <QPainter>
#include <QThread>

namespace HMVision
{
GigE_Camera::GigE_Camera(const QString& id, const QString& endpoint, QObject* parent)
    : ICamera(parent)
    , m_endpoint(endpoint)
{
    m_info.id = id;
    m_info.name = QString("GigE Camera (%1)").arg(endpoint);
}

GigE_Camera::~GigE_Camera()
{
    stopCapture();
    close();
}

CameraInfo GigE_Camera::info() const
{
    return m_info;
}

bool GigE_Camera::open()
{
    if (m_opened.load())
    {
        return true;
    }

    m_info.status = DeviceStatus::Connecting;
    emit statusChanged(m_info.status);

    // Placeholder for GenICam/custom network handshake.
    if (m_endpoint.isEmpty())
    {
        m_info.status = DeviceStatus::Error;
        emit statusChanged(m_info.status);
        return false;
    }

    m_opened.store(true);
    m_info.status = DeviceStatus::Connected;
    emit statusChanged(m_info.status);
    return true;
}

void GigE_Camera::close()
{
    stopCapture();
    m_opened.store(false);
    m_info.status = DeviceStatus::Disconnected;
    emit statusChanged(m_info.status);
}

bool GigE_Camera::isOpened() const
{
    return m_opened.load();
}

bool GigE_Camera::startCapture()
{
    if (!m_opened.load() || m_capturing.load())
    {
        return m_capturing.load();
    }

    m_capturing.store(true);
    m_captureThread = std::thread(&GigE_Camera::captureLoop, this);
    return true;
}

void GigE_Camera::stopCapture()
{
    m_capturing.store(false);
    if (m_captureThread.joinable())
    {
        m_captureThread.join();
    }
}

bool GigE_Camera::isCapturing() const
{
    return m_capturing.load();
}

void GigE_Camera::captureLoop()
{
    int frameIndex = 0;
    while (m_capturing.load())
    {
        // This synthetic frame generator can be replaced by GenICam grabs.
        QImage image = buildSyntheticFrame(frameIndex++);
        emit frameReady(m_info.id, image);
        QThread::msleep(33);
    }
}

QImage GigE_Camera::buildSyntheticFrame(int frameIndex) const
{
    QImage image(640, 480, QImage::Format_RGB888);
    image.fill(QColor(20, 20, 20));

    QPainter painter(&image);
    painter.setPen(Qt::green);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawText(20, 40, QString("GigE endpoint: %1").arg(m_endpoint));
    painter.drawText(20, 70, QString("Frame: %1").arg(frameIndex));
    painter.drawRect(10, 10, image.width() - 20, image.height() - 20);
    painter.end();

    return image;
}
} // namespace HMVision
