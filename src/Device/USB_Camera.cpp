#include "Device/USB_Camera.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>

namespace HMVision
{
USB_Camera::USB_Camera(CameraConfig config)
    : m_config(std::move(config))
{
}

USB_Camera::~USB_Camera()
{
    close();
}

bool USB_Camera::open()
{
    m_lastError.clear();
    if (m_opened.load())
    {
        return true;
    }
    const int idx = m_config.deviceIndex >= 0 ? m_config.deviceIndex : 0;
#ifdef _WIN32
    m_capture = std::make_unique<cv::VideoCapture>(idx, cv::CAP_DSHOW);
#else
    m_capture = std::make_unique<cv::VideoCapture>(idx);
#endif
    if (!m_capture->isOpened())
    {
        m_lastError = "VideoCapture::open failed for index " + std::to_string(idx);
        m_capture.reset();
        return false;
    }
    m_capture->set(cv::CAP_PROP_FRAME_WIDTH, m_config.width);
    m_capture->set(cv::CAP_PROP_FRAME_HEIGHT, m_config.height);
    m_opened.store(true);
    return true;
}

bool USB_Camera::close()
{
    m_grabbing.store(false);
    m_opened.store(false);
    if (m_capture && m_capture->isOpened())
    {
        m_capture->release();
    }
    m_capture.reset();
    return true;
}

bool USB_Camera::isOpen() const
{
    return m_opened.load() && m_capture && m_capture->isOpened();
}

bool USB_Camera::startGrabbing()
{
    if (!isOpen())
    {
        return false;
    }
    m_grabbing.store(true);
    return true;
}

bool USB_Camera::stopGrabbing()
{
    m_grabbing.store(false);
    return true;
}

bool USB_Camera::isGrabbing() const
{
    return m_grabbing.load();
}

bool USB_Camera::grabFrame(CameraFrame& frame, int /*timeoutMs*/)
{
    if (!m_capture || !m_capture->isOpened() || !m_grabbing.load())
    {
        return false;
    }
    cv::Mat raw;
    if (!m_capture->read(raw) || raw.empty())
    {
        return false;
    }
    if (raw.channels() == 1)
    {
        cv::cvtColor(raw, frame.image, cv::COLOR_GRAY2BGR);
    }
    else
    {
        frame.image = raw;
    }
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
    return !frame.image.empty();
}

bool USB_Camera::setParameter(
    const std::string& name, const CameraParam& value)
{
    if (!m_capture)
    {
        return false;
    }
    if (name == "exposure_time" && std::holds_alternative<double>(value))
    {
        m_config.exposureTime = std::get<double>(value);
        m_capture->set(cv::CAP_PROP_EXPOSURE, m_config.exposureTime);
        return true;
    }
    if (name == "gain" && std::holds_alternative<double>(value))
    {
        m_config.gain = std::get<double>(value);
        m_capture->set(cv::CAP_PROP_GAIN, m_config.gain);
        return true;
    }
    if (name == "width" && std::holds_alternative<int>(value))
    {
        m_config.width = std::get<int>(value);
        m_capture->set(cv::CAP_PROP_FRAME_WIDTH, m_config.width);
        return true;
    }
    if (name == "height" && std::holds_alternative<int>(value))
    {
        m_config.height = std::get<int>(value);
        m_capture->set(cv::CAP_PROP_FRAME_HEIGHT, m_config.height);
        return true;
    }
    if (name == "frame_rate" && std::holds_alternative<double>(value))
    {
        m_config.frameRate = std::get<double>(value);
        m_capture->set(cv::CAP_PROP_FPS, m_config.frameRate);
        return true;
    }
    return false;
}

CameraParam USB_Camera::getParameter(const std::string& name) const
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

std::vector<std::string> USB_Camera::getParameterList() const
{
    return { "exposure_time", "gain", "width", "height", "frame_rate" };
}

CameraInfo USB_Camera::getCameraInfo() const
{
    CameraInfo i;
    i.id = m_config.id;
    i.name = m_config.name;
    i.type = m_config.type;
    i.status = m_opened ? (m_grabbing ? CameraStatus::GRABBING : CameraStatus::CONNECTED) : CameraStatus::DISCONNECTED;
    i.lastError = m_lastError;
    return i;
}

CameraConfig USB_Camera::getCameraConfig() const
{
    return m_config;
}

void USB_Camera::setFrameCallback(FrameCallback callback)
{
    std::lock_guard g(m_cbMutex);
    m_frameCallback = std::move(callback);
}

void USB_Camera::removeFrameCallback()
{
    std::lock_guard g(m_cbMutex);
    m_frameCallback = nullptr;
}

void USB_Camera::onCameraEvent(
    const std::string& /*event*/, const std::string& /*data*/)
{
}
} // namespace HMVision
