#include "Device/CameraManager.h"

#include <opencv2/videoio.hpp>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <map>
#include <utility>
#include <variant>

namespace HMVision
{
namespace
{
std::size_t queueSizeFromConfig(
    const CameraConfig& c, const std::atomic<std::size_t>& globalDefault)
{
    const auto it = c.customParams.find("queue_size");
    if (it == c.customParams.end())
    {
        return std::max<std::size_t>(1U, globalDefault.load());
    }
    if (const int* p = std::get_if<int>(&it->second))
    {
        return static_cast<std::size_t>(std::max(1, *p));
    }
    return std::max<std::size_t>(1U, globalDefault.load());
}
} // namespace

CameraManager& CameraManager::getInstance()
{
    static CameraManager instance;
    return instance;
}

CameraManager::CameraManager() = default;

CameraManager::~CameraManager()
{
    stopAll();
    std::lock_guard lock(m_camerasMutex);
    m_cameras.clear();
}

void CameraManager::setFrameQueueSize(const std::size_t maxFrames)
{
    m_frameQueueSize.store(std::max<std::size_t>(1U, maxFrames), std::memory_order_relaxed);
}

std::size_t CameraManager::getFrameQueueSize() const
{
    return m_frameQueueSize.load(std::memory_order_relaxed);
}

bool CameraManager::addCamera(const CameraConfig& config)
{
    if (config.id.empty())
    {
        return false;
    }

    ICameraPtr cam;
    {
        std::lock_guard lock(m_camerasMutex);
        if (m_cameras.find(config.id) != m_cameras.end())
        {
            {
                std::lock_guard s(m_stateMutex);
                m_errors[config.id].lastError = "Camera id already exists.";
            }
            return false;
        }
        cam = CameraFactory::getInstance().createCamera(config);
        if (!cam)
        {
            {
                std::lock_guard s(m_stateMutex);
                m_errors[config.id].lastError = "No factory for camera type.";
            }
            return false;
        }
        if (!cam->open())
        {
            std::string detail = "open() failed.";
            const std::string fromCam = cam->getCameraInfo().lastError;
            if (!fromCam.empty())
            {
                detail += " " + fromCam;
            }
            {
                std::lock_guard s(m_stateMutex);
                m_errors[config.id].lastError = std::move(detail);
            }
            return false;
        }

        auto ctx = std::make_shared<CameraContext>();
        ctx->camera = std::move(cam);
        ctx->config = config;
        ctx->maxQueueSize = queueSizeFromConfig(config, m_frameQueueSize);

        m_cameras.emplace(config.id, std::move(ctx));
        {
            std::lock_guard s(m_stateMutex);
            m_errors[config.id].lastError.clear();
        }
    }
    return true;
}

bool CameraManager::removeCamera(const std::string& cameraId)
{
    CameraContextPtr ctx;
    {
        std::lock_guard lock(m_camerasMutex);
        const auto it = m_cameras.find(cameraId);
        if (it == m_cameras.end())
        {
            return false;
        }
        ctx = it->second;
        m_cameras.erase(it);
    }
    (void)stopGrabbingUnlocked(ctx);
    if (ctx && ctx->camera)
    {
        ctx->camera->close();
    }
    {
        std::lock_guard s(m_stateMutex);
        m_stats.erase(cameraId);
        m_errors.erase(cameraId);
    }
    {
        std::lock_guard lq(ctx->queueMutex);
        while (!ctx->frameQueue.empty())
        {
            ctx->frameQueue.pop();
        }
    }
    ctx->queueCond.notify_all();
    return true;
}

static std::vector<CameraManager::CameraEventCallback> copyCallbacksLocked(
    const std::map<CameraManager::CameraEventListenerId, CameraManager::CameraEventCallback>&
        listeners,
    const std::unique_ptr<CameraManager::CameraEventCallback>& legacy)
{
    std::vector<CameraManager::CameraEventCallback> out;
    if (legacy && *legacy)
    {
        out.push_back(*legacy);
    }
    for (const auto& p : listeners)
    {
        if (p.second)
        {
            out.push_back(p.second);
        }
    }
    return out;
}

void CameraManager::notifyFrameEvents(
    const std::string& cameraId, const CameraFrame& f, const std::string& eventData)
{
    std::vector<CameraEventCallback> cbs;
    {
        std::lock_guard s(m_stateMutex);
        cbs = copyCallbacksLocked(m_eventListeners, m_legacyEventCallback);
    }
    for (const CameraEventCallback& cb : cbs)
    {
        if (cb)
        {
            std::string payload = eventData;
            if (f.frameId > 0U)
            {
                payload += " frame_id=" + std::to_string(f.frameId);
            }
            cb(cameraId, "frame", payload);
        }
    }
}

void CameraManager::onGrabError(const std::string& cameraId, const char* what)
{
    {
        std::lock_guard s(m_stateMutex);
        m_errors[cameraId].lastError = what ? what : "grab";
        const auto st = m_stats.find(cameraId);
        if (st != m_stats.end())
        {
            ++st->second.errorCount;
        }
    }
    std::vector<CameraEventCallback> cbs;
    {
        std::lock_guard s(m_stateMutex);
        cbs = copyCallbacksLocked(m_eventListeners, m_legacyEventCallback);
    }
    for (const CameraEventCallback& cb : cbs)
    {
        if (cb)
        {
            cb(cameraId, "error", what ? what : "grab");
        }
    }
}

bool CameraManager::startCamera(const std::string& cameraId)
{
    CameraContextPtr ctx;
    {
        std::lock_guard lock(m_camerasMutex);
        const auto it = m_cameras.find(cameraId);
        if (it == m_cameras.end() || !it->second->camera)
        {
            return false;
        }
        if (it->second->running.load())
        {
            return true;
        }
        ctx = it->second;
    }
    if (!ctx->camera->startGrabbing())
    {
        onGrabError(cameraId, "startGrabbing failed");
        return false;
    }
    ctx->shouldStop.store(false);
    ctx->running.store(true);
    ctx->grabThread
        = std::thread(&CameraManager::grabThreadFunc, this, cameraId, ctx);
    {
        std::lock_guard s(m_stateMutex);
        m_errors[cameraId].lastError.clear();
    }
    return true;
}

bool CameraManager::stopCamera(const std::string& cameraId)
{
    CameraContextPtr ctx;
    {
        std::lock_guard lock(m_camerasMutex);
        const auto it = m_cameras.find(cameraId);
        if (it == m_cameras.end())
        {
            return false;
        }
        ctx = it->second;
    }
    return stopGrabbingUnlocked(ctx);
}

void CameraManager::stopAll()
{
    std::vector<std::string> ids;
    {
        std::lock_guard lock(m_camerasMutex);
        ids.reserve(m_cameras.size());
        for (const auto& p : m_cameras)
        {
            ids.push_back(p.first);
        }
    }
    for (const auto& id : ids)
    {
        (void)stopCamera(id);
    }
}

ICameraPtr CameraManager::getCamera(const std::string& cameraId) const
{
    std::lock_guard lock(m_camerasMutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end() || !it->second)
    {
        return nullptr;
    }
    return it->second->camera;
}

std::vector<std::string> CameraManager::getCameraList() const
{
    std::lock_guard lock(m_camerasMutex);
    std::vector<std::string> out;
    out.reserve(m_cameras.size());
    for (const auto& p : m_cameras)
    {
        out.push_back(p.first);
    }
    return out;
}

std::vector<CameraInfo> CameraManager::getAllCameraInfo() const
{
    std::lock_guard lock(m_camerasMutex);
    std::vector<CameraInfo> out;
    out.reserve(m_cameras.size());
    for (const auto& p : m_cameras)
    {
        if (p.second->camera)
        {
            out.push_back(p.second->camera->getCameraInfo());
        }
    }
    return out;
}

void CameraManager::pushOrReplaceLatest(
    const CameraContextPtr& ctx, const CameraFrame& f)
{
    std::lock_guard g(ctx->lastFrameMutex);
    f.image.copyTo(ctx->lastFrame.image);
    ctx->lastFrame.cameraId = f.cameraId;
    ctx->lastFrame.frameId = f.frameId;
    ctx->lastFrame.timestamp = f.timestamp;
    ctx->lastFrame.isTriggered = f.isTriggered;
    ctx->lastFrame.exposureTime = f.exposureTime;
    ctx->lastFrame.gain = f.gain;
}

bool CameraManager::getFrame(
    const std::string& cameraId, CameraFrame& frame, const int timeoutMs)
{
    CameraContextPtr ctx;
    {
        std::lock_guard lock(m_camerasMutex);
        const auto it = m_cameras.find(cameraId);
        if (it == m_cameras.end() || !it->second)
        {
            return false;
        }
        ctx = it->second;
    }

    std::unique_lock ql(ctx->queueMutex);
    if (!ctx->queueCond.wait_for(
            ql, std::chrono::milliseconds(timeoutMs), [ctx] { return !ctx->frameQueue.empty(); }))
    {
        return false;
    }
    frame = std::move(ctx->frameQueue.front());
    ctx->frameQueue.pop();
    return true;
}

bool CameraManager::getLatestFrame(const std::string& cameraId, CameraFrame& frame)
{
    CameraContextPtr ctx;
    {
        std::lock_guard lock(m_camerasMutex);
        const auto it = m_cameras.find(cameraId);
        if (it == m_cameras.end() || !it->second)
        {
            return false;
        }
        ctx = it->second;
    }
    std::lock_guard g(ctx->lastFrameMutex);
    if (ctx->lastFrame.image.empty())
    {
        return false;
    }
    frame = ctx->lastFrame;
    frame.image = ctx->lastFrame.image.clone();
    return true;
}

bool CameraManager::setCameraParameter(
    const std::string& cameraId, const std::string& name, const CameraParam& value)
{
    std::lock_guard lock(m_camerasMutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end() || !it->second->camera)
    {
        return false;
    }
    return it->second->camera->setParameter(name, value);
}

CameraParam CameraManager::getCameraParameter(
    const std::string& cameraId, const std::string& name)
{
    std::lock_guard lock(m_camerasMutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end() || !it->second->camera)
    {
        return 0;
    }
    return it->second->camera->getParameter(name);
}

CameraStatus CameraManager::getCameraStatus(const std::string& cameraId) const
{
    std::lock_guard lock(m_camerasMutex);
    const auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end() || !it->second->camera)
    {
        return CameraStatus::DISCONNECTED;
    }
    return it->second->camera->getCameraInfo().status;
}

std::string CameraManager::getCameraError(const std::string& cameraId) const
{
    std::scoped_lock lk(m_camerasMutex, m_stateMutex);
    const bool inMap = m_cameras.find(cameraId) != m_cameras.end();
    const auto eit = m_errors.find(cameraId);
    if (eit != m_errors.end() && !eit->second.lastError.empty())
    {
        return eit->second.lastError; ///< addCamera 失败时也会写入，此时 id 还不在 m_cameras
    }
    if (!inMap)
    {
        return "Unknown camera id.";
    }
    return {};
}

void CameraManager::setCameraEventCallback(CameraEventCallback callback)
{
    std::lock_guard s(m_stateMutex);
    if (callback)
    {
        m_legacyEventCallback
            = std::make_unique<CameraEventCallback>(std::move(callback));
    }
    else
    {
        m_legacyEventCallback.reset();
    }
}

void CameraManager::clearCameraEventCallback()
{
    std::lock_guard s(m_stateMutex);
    m_legacyEventCallback.reset();
}

CameraManager::CameraEventListenerId CameraManager::addCameraEventListener(
    CameraEventCallback callback)
{
    if (!callback)
    {
        return 0U;
    }
    std::lock_guard s(m_stateMutex);
    const CameraEventListenerId id = m_nextListenerId++;
    m_eventListeners[id] = std::move(callback);
    return id;
}

void CameraManager::removeCameraEventListener(
    const CameraEventListenerId id)
{
    if (id == 0U)
    {
        return;
    }
    std::lock_guard s(m_stateMutex);
    m_eventListeners.erase(id);
}

std::optional<CameraManager::CameraManagerStats> CameraManager::getCameraStats(
    const std::string& cameraId) const
{
    std::lock_guard s(m_stateMutex);
    const auto it = m_stats.find(cameraId);
    if (it == m_stats.end())
    {
        return std::nullopt;
    }
    CameraManager::CameraManagerStats o;
    o.frameCount = it->second.frameCount;
    o.droppedCount = it->second.droppedCount;
    o.errorCount = it->second.errorCount;
    return o;
}

void CameraManager::clearFrameQueue(const std::string& cameraId)
{
    CameraContextPtr ctx;
    {
        std::lock_guard lock(m_camerasMutex);
        const auto it = m_cameras.find(cameraId);
        if (it == m_cameras.end() || !it->second)
        {
            return;
        }
        ctx = it->second;
    }
    std::lock_guard q(ctx->queueMutex);
    while (!ctx->frameQueue.empty())
    {
        ctx->frameQueue.pop();
    }
    ctx->queueCond.notify_all();
}

std::vector<CameraInfo> CameraManager::enumerateAvailableCameras()
{
    std::vector<CameraInfo> list;
    for (int i = 0; i < 6; ++i)
    {
#ifdef _WIN32
        cv::VideoCapture cap(i, cv::CAP_DSHOW);
#else
        cv::VideoCapture cap(i);
#endif
        if (cap.isOpened())
        {
            CameraInfo info;
            info.id = "opencv_" + std::to_string(i);
            info.name = "UVC / OpenCV index " + std::to_string(i);
            info.type = CameraType::OPENCV_USB;
            info.status = CameraStatus::DISCONNECTED;
            list.push_back(std::move(info));
            cap.release();
        }
    }
    return list;
}

void CameraManager::grabThreadFunc(
    const std::string& cameraId, const CameraContextPtr& context)
{
    while (!context->shouldStop.load())
    {
        if (!context->camera)
        {
            break;
        }
        if (!context->camera->isGrabbing())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        CameraFrame f;
        if (context->camera->grabFrame(f, 50))
        {
            f.cameraId = cameraId;
            pushOrReplaceLatest(context, f);
            {
                std::lock_guard ql(context->queueMutex);
                std::uint64_t dropped = 0U;
                while (context->frameQueue.size() >= context->maxQueueSize)
                {
                    context->frameQueue.pop();
                    ++dropped;
                }
                context->frameQueue.push(f);
                if (dropped > 0U)
                {
                    std::lock_guard s(m_stateMutex);
                    m_stats[cameraId].droppedCount += dropped;
                }
            }
            context->queueCond.notify_all();

            {
                std::lock_guard s(m_stateMutex);
                ++m_stats[cameraId].frameCount;
            }
            notifyFrameEvents(cameraId, f, "ok");
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

bool CameraManager::stopGrabbingUnlocked(const CameraContextPtr& ctx)
{
    if (!ctx)
    {
        return false;
    }
    ctx->shouldStop.store(true);
    if (ctx->grabThread.joinable())
    {
        ctx->queueCond.notify_all();
        ctx->grabThread.join();
    }
    ctx->running.store(false);
    if (ctx->camera)
    {
        ctx->camera->stopGrabbing();
    }
    return true;
}
} // namespace HMVision
