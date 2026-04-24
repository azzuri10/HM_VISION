#pragma once

#include "Device/CameraFactory.h"
#include "Device/ICamera.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace HMVision
{
/// 全局相机单例管理：添加/移除、多路并行采集、线程安全、帧缓冲与事件。
/// 锁约定：\p m_camerasMutex 只保护 m_cameras；\p m_stateMutex 保护统计、错误、事件回调、队列深度配置，避免在持锁时调用用户回调（先拷贝再体外调用）。
class CameraManager
{
public:
    static CameraManager& getInstance();

    /// 在持锁下完成创建、open，避免同 id 重复与半打开泄漏。
    bool addCamera(const CameraConfig& config);
    bool removeCamera(const std::string& cameraId);
    bool startCamera(const std::string& cameraId);
    bool stopCamera(const std::string& cameraId);
    void stopAll();

    ICameraPtr getCamera(const std::string& cameraId) const;
    std::vector<std::string> getCameraList() const;
    std::vector<CameraInfo> getAllCameraInfo() const;

    /// 从每路环形队列取一帧（可阻塞，依赖采集线程与相机 grabFrame）
    bool getFrame(const std::string& cameraId, CameraFrame& frame, int timeoutMs = 1000);
    /// 拷贝最新一帧的图像到 \p frame（`image` 为 clone，不共享内部缓冲）
    bool getLatestFrame(const std::string& cameraId, CameraFrame& frame);

    bool setCameraParameter(
        const std::string& cameraId, const std::string& name, const CameraParam& value);
    CameraParam getCameraParameter(const std::string& cameraId, const std::string& name);

    CameraStatus getCameraStatus(const std::string& cameraId) const;
    std::string getCameraError(const std::string& cameraId) const;

    using CameraEventCallback = std::function<void(
        const std::string& cameraId, const std::string& event, const std::string& data)>;
    /// 与 addCameraEventListener 并存；在通知时二者都会收到（先 legacy 后 listeners）
    void setCameraEventCallback(CameraEventCallback callback);
    void clearCameraEventCallback();
    using CameraEventListenerId = std::uint64_t;
    CameraEventListenerId addCameraEventListener(CameraEventCallback callback);
    void removeCameraEventListener(CameraEventListenerId id);

    /// 每路帧队列最大深度（新加入相机使用当前值；已运行中相机仍用其创建时深度）
    void setFrameQueueSize(std::size_t maxFrames);
    std::size_t getFrameQueueSize() const;

    /// 每路：累计帧、队列溢出丢弃数、取帧/采集失败等错误次数
    struct CameraManagerStats
    {
        std::uint64_t frameCount{0};
        std::uint64_t droppedCount{0};
        std::uint64_t errorCount{0};
    };
    std::optional<CameraManagerStats> getCameraStats(const std::string& cameraId) const;
    void clearFrameQueue(const std::string& cameraId);

    /// 发现 UVC 设备供 UI 使用
    std::vector<CameraInfo> enumerateAvailableCameras();

private:
    CameraManager();
    ~CameraManager();
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    struct CameraContext
    {
        ICameraPtr camera;
        CameraConfig config;
        std::thread grabThread;
        std::atomic<bool> running{false};
        std::atomic<bool> shouldStop{false};

        std::queue<CameraFrame> frameQueue;
        std::mutex queueMutex;
        std::condition_variable queueCond;
        std::size_t maxQueueSize{10U};

        CameraFrame lastFrame;
        std::mutex lastFrameMutex;
    };

    using CameraContextPtr = std::shared_ptr<CameraContext>;

    void grabThreadFunc(const std::string& cameraId, const CameraContextPtr& context);
    static bool stopGrabbingUnlocked(const CameraContextPtr& ctx);
    void pushOrReplaceLatest(const CameraContextPtr& ctx, const CameraFrame& f);
    void notifyFrameEvents(
        const std::string& cameraId, const CameraFrame& f, const std::string& eventData = "ok");
    void onGrabError(const std::string& cameraId, const char* what);

    std::map<std::string, CameraContextPtr> m_cameras;
    mutable std::recursive_mutex m_camerasMutex;

    struct CameraStats
    {
        std::uint64_t frameCount{0};
        std::uint64_t droppedCount{0};
        std::uint64_t errorCount{0};
    };
    struct ErrorEntry
    {
        std::string lastError;
    };
    std::map<std::string, CameraStats> m_stats;
    std::map<std::string, ErrorEntry> m_errors;
    std::map<CameraEventListenerId, CameraEventCallback> m_eventListeners;
    std::unique_ptr<CameraEventCallback> m_legacyEventCallback;
    CameraEventListenerId m_nextListenerId{1};
    std::atomic<std::size_t> m_frameQueueSize{10U};

    mutable std::mutex m_stateMutex;
};
} // namespace HMVision
