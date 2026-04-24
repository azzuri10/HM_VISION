#include "Device/GigE_Camera.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <thread>

namespace HMVision
{
GigE_Camera::GigE_Camera(CameraConfig config)
    : m_config(std::move(config))
{
}

GigE_Camera::~GigE_Camera()
{
    close();
}

bool GigE_Camera::open()
{
    if (m_config.connectionString.empty())
    {
        m_lastError = "GigE (OpenCV placeholder): set connectionString to device IP.";
        m_opened.store(false);
        return false;
    }
    m_lastError.clear();
    m_opened.store(true);
    return true;
}

bool GigE_Camera::close()
{
    m_grabbing.store(false);
    m_opened.store(false);
    return true;
}

bool GigE_Camera::isOpen() const
{
    return m_opened.load();
}

bool GigE_Camera::startGrabbing()
{
    m_grabbing.store(m_opened.load());
    return m_grabbing.load();
}

bool GigE_Camera::stopGrabbing()
{
    m_grabbing.store(false);
    return true;
}

bool GigE_Camera::isGrabbing() const
{
    return m_grabbing.load();
}

cv::Mat GigE_Camera::makeSyntheticFrame() const
{
    const int w = std::max(320, m_config.width);
    const int h = std::max(240, m_config.height);
    cv::Mat bgr(h, w, CV_8UC3, cv::Scalar(20, 20, 20));
    const std::string t1 = "OPENCV_GIGE (sim) " + m_config.connectionString;
    const std::string t2 = "Frame " + std::to_string(m_frameIndex);
    cv::putText(
        bgr, t1, cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    cv::putText(
        bgr, t2, cv::Point(20, 70), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    return bgr;
}

bool GigE_Camera::grabFrame(CameraFrame& frame, int /*timeoutMs*/)
{
    if (!m_opened.load() || !m_grabbing.load())
    {
        return false;
    }
    frame.image = makeSyntheticFrame();
    frame.cameraId = m_config.id;
    frame.frameId = ++m_frameId;
    frame.timestamp = std::chrono::system_clock::now();
    frame.exposureTime = m_config.exposureTime;
    frame.gain = m_config.gain;
    {
        std::lock_guard g(m_cbMutex);
        if (m_frameCallback)
        {
            m_frameCallback(frame);
        }
    }
    ++m_frameIndex;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return !frame.image.empty();
}

bool GigE_Camera::setParameter(
    const std::string& name, const CameraParam& value)
{
    if (name == "exposure_time" && std::holds_alternative<double>(value))
    {
        m_config.exposureTime = std::get<double>(value);
        return true;
    }
    if (name == "gain" && std::holds_alternative<double>(value))
    {
        m_config.gain = std::get<double>(value);
        return true;
    }
    if (name == "width" && std::holds_alternative<int>(value))
    {
        m_config.width = std::get<int>(value);
        return true;
    }
    if (name == "height" && std::holds_alternative<int>(value))
    {
        m_config.height = std::get<int>(value);
        return true;
    }
    if (name == "frame_rate" && std::holds_alternative<double>(value))
    {
        m_config.frameRate = std::get<double>(value);
        return true;
    }
    return false;
}

CameraParam GigE_Camera::getParameter(const std::string& name) const
{
    if (name == "exposure_time")
    {
        return m_config.exposureTime;
    }
    if (name == "gain")
    {
        return m_config.gain;
    }
    if (name == "width")
    {
        return m_config.width;
    }
    if (name == "height")
    {
        return m_config.height;
    }
    if (name == "frame_rate")
    {
        return m_config.frameRate;
    }
    return 0;
}

std::vector<std::string> GigE_Camera::getParameterList() const
{
    return { "exposure_time", "gain", "width", "height", "frame_rate" };
}

CameraInfo GigE_Camera::getCameraInfo() const
{
    CameraInfo i;
    i.id = m_config.id;
    i.name = m_config.name;
    i.type = m_config.type;
    i.ipAddress = m_config.connectionString;
    i.status
        = m_opened ? (m_grabbing ? CameraStatus::GRABBING : CameraStatus::CONNECTED) : CameraStatus::DISCONNECTED;
    i.lastError = m_lastError;
    return i;
}

CameraConfig GigE_Camera::getCameraConfig() const
{
    return m_config;
}

void GigE_Camera::setFrameCallback(FrameCallback callback)
{
    std::lock_guard g(m_cbMutex);
    m_frameCallback = std::move(callback);
}

void GigE_Camera::removeFrameCallback()
{
    std::lock_guard g(m_cbMutex);
    m_frameCallback = nullptr;
}

void GigE_Camera::onCameraEvent(
    const std::string& /*event*/, const std::string& /*data*/)
{
}
} // namespace HMVision
