#include "USB_Camera.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <QThread>

namespace HMVision
{
USB_Camera::USB_Camera(const QString& id, int deviceIndex, QObject* parent)
    : ICamera(parent)
    , m_deviceIndex(deviceIndex)
{
    m_info.id = id;
    m_info.name = QString("USB Camera %1").arg(deviceIndex);
}

USB_Camera::~USB_Camera()
{
    stopCapture();
    close();
}

CameraInfo USB_Camera::info() const
{
    return m_info;
}

bool USB_Camera::open()
{
    if (m_opened.load())
    {
        return true;
    }

    m_info.status = DeviceStatus::Connecting;
    emit statusChanged(m_info.status);

    m_capture = std::make_unique<cv::VideoCapture>(m_deviceIndex);
    if (!m_capture->isOpened())
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

void USB_Camera::close()
{
    stopCapture();
    if (m_capture && m_capture->isOpened())
    {
        m_capture->release();
    }
    m_capture.reset();
    m_opened.store(false);
    m_info.status = DeviceStatus::Disconnected;
    emit statusChanged(m_info.status);
}

bool USB_Camera::isOpened() const
{
    return m_opened.load();
}

bool USB_Camera::startCapture()
{
    if (!m_opened.load() || m_capturing.load())
    {
        return m_capturing.load();
    }

    m_capturing.store(true);
    m_captureThread = std::thread(&USB_Camera::captureLoop, this);
    return true;
}

void USB_Camera::stopCapture()
{
    m_capturing.store(false);
    if (m_captureThread.joinable())
    {
        m_captureThread.join();
    }
}

bool USB_Camera::isCapturing() const
{
    return m_capturing.load();
}

void USB_Camera::captureLoop()
{
    while (m_capturing.load())
    {
        if (!m_capture || !m_capture->isOpened())
        {
            QThread::msleep(20);
            continue;
        }

        cv::Mat frame;
        if (!m_capture->read(frame) || frame.empty())
        {
            QThread::msleep(10);
            continue;
        }

        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        emit frameReady(m_info.id, image.copy());

        QThread::msleep(5);
    }
}
} // namespace HMVision
