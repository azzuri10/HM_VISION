#pragma once

#include "CameraFactory.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace HMVision
{
class CameraManager
{
public:
    using FrameCallback = ICamera::FrameCallback;
    using StatusCallback = ICamera::StatusCallback;

    CameraManager();
    ~CameraManager();

    bool addCamera(const CameraConfig& config);
    bool removeCamera(const std::string& cameraId);
    std::shared_ptr<ICamera> getCamera(const std::string& cameraId) const;

    bool startCamera(const std::string& cameraId);
    bool stopCamera(const std::string& cameraId);
    bool startAll();
    void stopAll();

    std::vector<CameraStatus> getAllStatus() const;
    CameraStatus getStatus(const std::string& cameraId) const;
    std::size_t size() const;

    void setFrameCallback(FrameCallback callback);
    void setStatusCallback(StatusCallback callback);

    void startStatusMonitor(int intervalMs = 1000);
    void stopStatusMonitor();

private:
    void monitorLoop();
    void attachCameraCallbacks(const std::string& cameraId, const std::shared_ptr<ICamera>& camera);

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<ICamera>> m_cameras;
    FrameCallback m_frameCallback;
    StatusCallback m_statusCallback;

    std::atomic<bool> m_monitorRunning{false};
    int m_monitorIntervalMs = 1000;
    std::thread m_monitorThread;
};
} // namespace HMVision
