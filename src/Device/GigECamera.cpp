#include "../../include/Device/ICamera.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace HMVision
{
class GigECamera : public ICamera
{
public:
    GigECamera() = default;

    ~GigECamera() override
    {
        stopStream();
        close();
    }

    std::string id() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config.id;
    }

    CameraType type() const override
    {
        return CameraType::GigE;
    }

    bool open(const CameraConfig& config) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_opened.load())
        {
            return true;
        }

        m_status.state = CameraConnectionState::Connecting;
        m_status.message = "Connecting GigE camera...";
        notifyStatusNoLock();

        m_config = config;
        if (m_config.ip.empty())
        {
            // Auto IP configuration (simulated APIPA-style)
            const std::size_t hash = std::hash<std::string>{}(m_config.id.empty() ? "gige" : m_config.id);
            const int host = static_cast<int>(hash % 200) + 20;
            m_config.ip = "169.254.1." + std::to_string(host);
        }
        if (m_config.port <= 0)
        {
            m_config.port = 3956;
        }

        // GenTL simulated handshake.
        m_genTlReady = simulateGenTlHandshakeNoLock();
        if (!m_genTlReady)
        {
            m_status.state = CameraConnectionState::Error;
            m_status.message = "GenTL handshake failed.";
            notifyStatusNoLock();
            return false;
        }

        m_opened.store(true);
        m_status.state = CameraConnectionState::Connected;
        m_status.message = "GigE camera connected.";
        m_status.frameCount = 0;
        m_status.fps = 0.0;
        m_status.lastHeartbeatMs = currentEpochMs();
        notifyStatusNoLock();

        startHeartbeatNoLock();
        return true;
    }

    void close() override
    {
        stopStream();
        stopHeartbeat();

        std::lock_guard<std::mutex> lock(m_mutex);
        m_opened.store(false);
        m_genTlReady = false;
        m_status.state = CameraConnectionState::Disconnected;
        m_status.message = "GigE camera closed.";
        notifyStatusNoLock();
    }

    bool startStream() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_opened.load() || !m_genTlReady)
        {
            return false;
        }
        if (m_streaming.load())
        {
            return true;
        }

        m_stopStreamRequested.store(false);
        m_streaming.store(true);
        m_status.state = CameraConnectionState::Streaming;
        m_status.message = "GigE streaming started.";
        notifyStatusNoLock();

        m_streamThread = std::thread(&GigECamera::streamLoop, this);
        return true;
    }

    void stopStream() override
    {
        m_stopStreamRequested.store(true);
        if (m_streamThread.joinable())
        {
            m_streamThread.join();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_streaming.load())
        {
            m_streaming.store(false);
            if (m_opened.load())
            {
                m_status.state = CameraConnectionState::Connected;
                m_status.message = "GigE streaming stopped.";
            }
            notifyStatusNoLock();
        }
    }

    bool isOpened() const override
    {
        return m_opened.load();
    }

    bool isStreaming() const override
    {
        return m_streaming.load();
    }

    CameraStatus status() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_status;
    }

    CameraConfig config() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config;
    }

    void setFrameCallback(FrameCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_frameCallback = std::move(callback);
    }

    void setStatusCallback(StatusCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusCallback = std::move(callback);
    }

private:
    bool simulateGenTlHandshakeNoLock()
    {
        // Simulate a GenTL producer/consumer negotiation.
        // In real implementation, this should discover interface/device/stream modules.
        return true;
    }

    void startHeartbeatNoLock()
    {
        if (m_heartbeatRunning.load())
        {
            return;
        }
        m_heartbeatRunning.store(true);
        m_heartbeatThread = std::thread(&GigECamera::heartbeatLoop, this);
    }

    void stopHeartbeat()
    {
        m_heartbeatRunning.store(false);
        if (m_heartbeatThread.joinable())
        {
            m_heartbeatThread.join();
        }
    }

    void heartbeatLoop()
    {
        while (m_heartbeatRunning.load())
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_opened.load())
                {
                    break;
                }

                // Simulate keepalive/heartbeat packet exchange.
                m_status.lastHeartbeatMs = currentEpochMs();
                if (m_status.state != CameraConnectionState::Error)
                {
                    m_status.message = "Heartbeat OK (" + m_config.ip + ")";
                }
                notifyStatusNoLock();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    void streamLoop()
    {
        auto fpsTick = std::chrono::steady_clock::now();
        std::uint64_t localFrames = 0;

        const int width = m_config.width > 0 ? m_config.width : 1280;
        const int height = m_config.height > 0 ? m_config.height : 720;
        int frameIndex = 0;

        while (!m_stopStreamRequested.load())
        {
            // Simulated network image stream processing.
            cv::Mat frame(height, width, CV_8UC3, cv::Scalar(30, 30, 30));
            cv::putText(
                frame,
                "GigE: " + m_config.ip,
                cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX,
                1.0,
                cv::Scalar(0, 255, 0),
                2);
            cv::putText(
                frame,
                "Frame: " + std::to_string(frameIndex++),
                cv::Point(20, 80),
                cv::FONT_HERSHEY_SIMPLEX,
                1.0,
                cv::Scalar(0, 255, 255),
                2);

            FrameCallback frameCb;
            std::string cameraId;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                frameCb = m_frameCallback;
                cameraId = m_config.id;
            }

            if (frameCb)
            {
                const std::size_t bytes = frame.total() * frame.elemSize();
                std::vector<std::uint8_t> payload(bytes);
                std::memcpy(payload.data(), frame.data, bytes);
                frameCb(cameraId, payload);
            }

            ++localFrames;
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsTick).count();
            if (elapsedMs >= 1000)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_status.fps = static_cast<double>(localFrames) * 1000.0 / static_cast<double>(elapsedMs);
                m_status.frameCount += localFrames;
                m_status.message = "GigE streaming active.";
                localFrames = 0;
                fpsTick = now;
                notifyStatusNoLock();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        m_streaming.store(false);
    }

    static std::int64_t currentEpochMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    void notifyStatusNoLock()
    {
        auto cb = m_statusCallback;
        const auto statusSnapshot = m_status;
        const auto cameraId = m_config.id;
        if (cb)
        {
            cb(cameraId, statusSnapshot);
        }
    }

    mutable std::mutex m_mutex;
    CameraConfig m_config;
    CameraStatus m_status;
    FrameCallback m_frameCallback;
    StatusCallback m_statusCallback;

    bool m_genTlReady = false;
    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_streaming{false};

    std::atomic<bool> m_stopStreamRequested{false};
    std::thread m_streamThread;

    std::atomic<bool> m_heartbeatRunning{false};
    std::thread m_heartbeatThread;
};

std::shared_ptr<ICamera> createGigECamera()
{
    return std::make_shared<GigECamera>();
}
} // namespace HMVision
