#pragma once

#include "Device/ICamera.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace HMVision
{
class VirtualCamera : public ICamera
{
public:
    explicit VirtualCamera(CameraConfig config);
    ~VirtualCamera() override;

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
    void fillPattern(cv::Mat& bgr) const;

    CameraConfig m_config;
    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_grabbing{false};
    mutable std::uint64_t m_frameId{0};
    std::mutex m_cbMutex;
    FrameCallback m_frameCallback;
};
} // namespace HMVision
