#pragma once

#include "Device/ICamera.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace HMVision
{
class USB_Camera : public ICamera
{
public:
    explicit USB_Camera(CameraConfig config);
    ~USB_Camera() override;

    bool open() override;
    bool close() override;
    bool isOpen() const override;

    bool startGrabbing() override;
    bool stopGrabbing() override;
    bool isGrabbing() const override;

    bool grabFrame(CameraFrame& frame, int timeoutMs = 1000) override;

    bool setParameter(const std::string& name, const CameraParam& value) override;
    CameraParam getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterList() const override;

    CameraInfo getCameraInfo() const override;
    CameraConfig getCameraConfig() const override;

    void setFrameCallback(FrameCallback callback) override;
    void removeFrameCallback() override;
    void onCameraEvent(const std::string& event, const std::string& data) override;

private:
    CameraConfig m_config;
    std::string m_lastError;
    std::unique_ptr<cv::VideoCapture> m_capture;
    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_grabbing{false};
    std::uint64_t m_frameId{0};
    std::mutex m_cbMutex;
    FrameCallback m_frameCallback;
};
} // namespace HMVision
