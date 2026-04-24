#include "../../include/Device/CameraManager.h"

#include <chrono>

namespace HMVision
{
CameraManager::CameraManager() = default;

CameraManager::~CameraManager()
{
    stopStatusMonitor();
    stopAll();
}

bool CameraManager::addCamera(const CameraConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (config.id.empty() || m_cameras.find(config.id) != m_cameras.end())
    {
        return false;
    }

    auto camera = CameraFactory::getInstance().create(config.type);
    if (!camera || !camera->open(config))
    {
        return false;
    }

    attachCameraCallbacks(config.id, camera);
    m_cameras.emplace(config.id, std::move(camera));
    return true;
}

bool CameraManager::removeCamera(const std::string& cameraId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end())
    {
        return false;
    }

    it->second->stopStream();
    it->second->close();
    m_cameras.erase(it);
    return true;
}

std::shared_ptr<ICamera> CameraManager::getCamera(const std::string& cameraId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_cameras.find(cameraId);
    return it == m_cameras.end() ? nullptr : it->second;
}

bool CameraManager::startCamera(const std::string& cameraId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end())
    {
        return false;
    }
    return it->second->startStream();
}

bool CameraManager::stopCamera(const std::string& cameraId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end())
    {
        return false;
    }
    it->second->stopStream();
    return true;
}

bool CameraManager::startAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    bool ok = true;
    for (const auto& pair : m_cameras)
    {
        ok = pair.second->startStream() && ok;
    }
    return ok;
}

void CameraManager::stopAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_cameras)
    {
        pair.second->stopStream();
    }
}

std::vector<CameraStatus> CameraManager::getAllStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CameraStatus> result;
    result.reserve(m_cameras.size());
    for (const auto& pair : m_cameras)
    {
        result.push_back(pair.second->status());
    }
    return result;
}

CameraStatus CameraManager::getStatus(const std::string& cameraId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end())
    {
        CameraStatus status;
        status.state = CameraConnectionState::Error;
        status.message = "Camera not found.";
        return status;
    }
    return it->second->status();
}

std::size_t CameraManager::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cameras.size();
}

void CameraManager::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameCallback = std::move(callback);
    for (const auto& pair : m_cameras)
    {
        attachCameraCallbacks(pair.first, pair.second);
    }
}

void CameraManager::setStatusCallback(StatusCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCallback = std::move(callback);
    for (const auto& pair : m_cameras)
    {
        attachCameraCallbacks(pair.first, pair.second);
    }
}

void CameraManager::startStatusMonitor(int intervalMs)
{
    if (m_monitorRunning.load())
    {
        return;
    }
    m_monitorIntervalMs = intervalMs > 0 ? intervalMs : 1000;
    m_monitorRunning.store(true);
    m_monitorThread = std::thread(&CameraManager::monitorLoop, this);
}

void CameraManager::stopStatusMonitor()
{
    m_monitorRunning.store(false);
    if (m_monitorThread.joinable())
    {
        m_monitorThread.join();
    }
}

void CameraManager::monitorLoop()
{
    while (m_monitorRunning.load())
    {
        std::vector<std::pair<std::string, CameraStatus>> snapshots;
        StatusCallback callback;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callback = m_statusCallback;
            snapshots.reserve(m_cameras.size());
            for (const auto& pair : m_cameras)
            {
                snapshots.emplace_back(pair.first, pair.second->status());
            }
        }

        if (callback)
        {
            for (const auto& item : snapshots)
            {
                callback(item.first, item.second);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(m_monitorIntervalMs));
    }
}

void CameraManager::attachCameraCallbacks(
    const std::string& cameraId, const std::shared_ptr<ICamera>& camera)
{
    if (!camera)
    {
        return;
    }

    auto frameCb = m_frameCallback;
    auto statusCb = m_statusCallback;
    camera->setFrameCallback([cameraId, frameCb](const std::string&, const std::vector<std::uint8_t>& payload) {
        if (frameCb)
        {
            frameCb(cameraId, payload);
        }
    });
    camera->setStatusCallback([cameraId, statusCb](const std::string&, const CameraStatus& status) {
        if (statusCb)
        {
            statusCb(cameraId, status);
        }
    });
}
} // namespace HMVision
