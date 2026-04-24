#include "Device/VirtualCamera.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

#include <chrono>
#include <thread>

namespace HMVision
{
VirtualCamera::VirtualCamera(CameraConfig config)
    : m_config(std::move(config))
{
    if (m_config.width < 32)
    {
        m_config.width = 640;
    }
    if (m_config.height < 32)
    {
        m_config.height = 480;
    }
}

VirtualCamera::~VirtualCamera()
{
    close();
}

bool VirtualCamera::open()
{
    m_opened.store(true);
    return true;
}

bool VirtualCamera::close()
{
    m_grabbing.store(false);
    m_opened.store(false);
    return true;
}

bool VirtualCamera::isOpen() const
{
    return m_opened.load();
}

bool VirtualCamera::startGrabbing()
{
    m_grabbing.store(m_opened.load());
    return m_grabbing.load();
}

bool VirtualCamera::stopGrabbing()
{
    m_grabbing.store(false);
    return true;
}

bool VirtualCamera::isGrabbing() const
{
    return m_grabbing.load();
}

void VirtualCamera::fillPattern(cv::Mat& bgr) const
{
    const int w = m_config.width;
    const int h = m_config.height;
    bgr = cv::Mat::zeros(h, w, CV_8UC3);
    const int step = 40;
    for (int y = 0; y < h; y += step)
    {
        for (int x = 0; x < w; x += step)
        {
            const uchar g = static_cast<uchar>((
                (x + y + static_cast<int>(m_frameId) * 7) * 3) & 0xFF);
            cv::rectangle(
                bgr, cv::Rect(x, y, std::min(step, w - x), std::min(step, h - y)), cv::Scalar(20, g, 255 - g), -1);
        }
    }
    cv::putText(
        bgr,
        "VIRTUAL " + m_config.id,
        cv::Point(20, 40),
        cv::FONT_HERSHEY_SIMPLEX,
        0.7,
        cv::Scalar(200, 255, 200),
        1);
}

bool VirtualCamera::grabFrame(CameraFrame& frame, int timeoutMs)
{
    (void)timeoutMs;
    if (!m_opened.load() || !m_grabbing.load())
    {
        return false;
    }
    fillPattern(frame.image);
    frame.cameraId = m_config.id;
    frame.frameId = ++m_frameId;
    frame.timestamp = std::chrono::system_clock::now();
    frame.isTriggered = false;
    frame.exposureTime = m_config.exposureTime;
    frame.gain = m_config.gain;
    {
        std::lock_guard g(m_cbMutex);
        if (m_frameCallback)
        {
            m_frameCallback(frame);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return !frame.image.empty();
}

bool VirtualCamera::setParameter(const std::string& name, const CameraParam& value)
{
    if (name == "width" && std::holds_alternative<int>(value))
    {
        m_config.width = std::get<int>(value);
    }
    else if (name == "height" && std::holds_alternative<int>(value))
    {
        m_config.height = std::get<int>(value);
    }
    else if (name == "exposure_time" && std::holds_alternative<double>(value))
    {
        m_config.exposureTime = std::get<double>(value);
    }
    else if (name == "gain" && std::holds_alternative<double>(value))
    {
        m_config.gain = std::get<double>(value);
    }
    else
    {
        return false;
    }
    return true;
}

CameraParam VirtualCamera::getParameter(const std::string& name) const
{
    if (name == "width")
    {
        return m_config.width;
    }
    if (name == "height")
    {
        return m_config.height;
    }
    if (name == "exposure_time")
    {
        return m_config.exposureTime;
    }
    if (name == "gain")
    {
        return m_config.gain;
    }
    return 0;
}

std::vector<std::string> VirtualCamera::getParameterList() const
{
    return { "width", "height", "exposure_time", "gain" };
}

CameraInfo VirtualCamera::getCameraInfo() const
{
    CameraInfo i;
    i.id = m_config.id;
    i.name = m_config.name;
    i.type = m_config.type;
    i.status = m_opened ? (m_grabbing ? CameraStatus::GRABBING : CameraStatus::CONNECTED) : CameraStatus::DISCONNECTED;
    return i;
}

CameraConfig VirtualCamera::getCameraConfig() const
{
    return m_config;
}

void VirtualCamera::setFrameCallback(FrameCallback callback)
{
    std::lock_guard g(m_cbMutex);
    m_frameCallback = std::move(callback);
}

void VirtualCamera::removeFrameCallback()
{
    std::lock_guard g(m_cbMutex);
    m_frameCallback = nullptr;
}

void VirtualCamera::onCameraEvent(const std::string& /*event*/, const std::string& /*data*/)
{
}
} // namespace HMVision
