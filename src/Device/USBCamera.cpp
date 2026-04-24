#include "../../include/Device/ICamera.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace HMVision
{
class USBCamera : public ICamera
{
public:
    enum class TriggerMode
    {
        Continuous,
        Software
    };

    USBCamera() = default;

    ~USBCamera() override
    {
        stopStream();
        close();
    }

    static std::vector<int> enumerateDevices(int maxProbeCount = 16)
    {
        std::vector<int> result;
        for (int i = 0; i < maxProbeCount; ++i)
        {
            cv::VideoCapture cap(i, cv::CAP_ANY);
            if (cap.isOpened())
            {
                result.push_back(i);
                cap.release();
            }
        }
        return result;
    }

    std::string id() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config.id;
    }

    CameraType type() const override
    {
        return CameraType::USB;
    }

    bool open(const CameraConfig& config) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_opened.load())
        {
            return true;
        }

        m_status.state = CameraConnectionState::Connecting;
        m_status.message = "Opening USB camera...";
        notifyStatusNoLock();

        m_config = config;
        if (m_config.deviceIndex < 0)
        {
            m_status.state = CameraConnectionState::Error;
            m_status.message = "Invalid USB device index.";
            notifyStatusNoLock();
            return false;
        }

        m_capture = std::make_unique<cv::VideoCapture>(m_config.deviceIndex, cv::CAP_ANY);
        if (!m_capture->isOpened())
        {
            m_capture.reset();
            m_status.state = CameraConnectionState::Error;
            m_status.message = "Failed to open VideoCapture.";
            notifyStatusNoLock();
            return false;
        }

        // Camera parameter setup
        if (m_config.width > 0)
        {
            m_capture->set(cv::CAP_PROP_FRAME_WIDTH, static_cast<double>(m_config.width));
        }
        if (m_config.height > 0)
        {
            m_capture->set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(m_config.height));
        }
        if (m_config.exposure > 0.0)
        {
            m_capture->set(cv::CAP_PROP_EXPOSURE, m_config.exposure);
        }
        if (m_config.gain > 0.0)
        {
            m_capture->set(cv::CAP_PROP_GAIN, m_config.gain);
        }
        auto fpsIt = m_config.extraParams.find("fps");
        if (fpsIt != m_config.extraParams.end())
        {
            try
            {
                m_capture->set(cv::CAP_PROP_FPS, std::stod(fpsIt->second));
            }
            catch (...)
            {
            }
        }

        auto modeIt = m_config.extraParams.find("triggerMode");
        if (modeIt != m_config.extraParams.end() && modeIt->second == "software")
        {
            m_triggerMode = TriggerMode::Software;
        }
        else
        {
            m_triggerMode = TriggerMode::Continuous;
        }

        m_opened.store(true);
        m_status.state = CameraConnectionState::Connected;
        m_status.message = "USB camera opened.";
        m_status.frameCount = 0;
        m_status.fps = 0.0;
        notifyStatusNoLock();
        return true;
    }

    void close() override
    {
        stopStream();
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_capture && m_capture->isOpened())
        {
            m_capture->release();
        }
        m_capture.reset();
        m_opened.store(false);
        m_status.state = CameraConnectionState::Disconnected;
        m_status.message = "Camera closed.";
        notifyStatusNoLock();
    }

    bool startStream() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_opened.load())
        {
            return false;
        }
        if (m_streaming.load())
        {
            return true;
        }

        m_stopRequested.store(false);
        m_streaming.store(true);
        m_status.state = CameraConnectionState::Streaming;
        m_status.message = "Streaming started.";
        notifyStatusNoLock();

        m_captureThread = std::thread(&USBCamera::captureLoop, this);
        return true;
    }

    void stopStream() override
    {
        m_stopRequested.store(true);
        m_triggerCv.notify_all();
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_streaming.load())
        {
            m_streaming.store(false);
            if (m_opened.load())
            {
                m_status.state = CameraConnectionState::Connected;
                m_status.message = "Streaming stopped.";
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

    // For software trigger mode.
    void softwareTrigger()
    {
        if (m_triggerMode != TriggerMode::Software)
        {
            return;
        }
        m_triggerPending.store(true);
        m_triggerCv.notify_one();
    }

private:
    void captureLoop()
    {
        auto lastFpsTick = std::chrono::steady_clock::now();
        std::uint64_t localFrames = 0;

        while (!m_stopRequested.load())
        {
            if (m_triggerMode == TriggerMode::Software)
            {
                std::unique_lock<std::mutex> waitLock(m_triggerMutex);
                m_triggerCv.wait(waitLock, [this]() {
                    return m_stopRequested.load() || m_triggerPending.load();
                });
                if (m_stopRequested.load())
                {
                    break;
                }
                m_triggerPending.store(false);
            }

            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_capture || !m_capture->isOpened())
                {
                    break;
                }
                if (!m_capture->read(frame) || frame.empty())
                {
                    m_status.state = CameraConnectionState::Error;
                    m_status.message = "Frame capture failed.";
                    notifyStatusNoLock();
                    continue;
                }
            }

            ++localFrames;
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTick).count();
            if (elapsedMs >= 1000)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_status.fps = static_cast<double>(localFrames) * 1000.0 / static_cast<double>(elapsedMs);
                m_status.frameCount += localFrames;
                m_status.lastHeartbeatMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();
                localFrames = 0;
                lastFpsTick = now;
                notifyStatusNoLock();
            }

            // Send BGR image bytes to callback.
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

            if (m_triggerMode == TriggerMode::Continuous)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        m_streaming.store(false);
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
    std::unique_ptr<cv::VideoCapture> m_capture;
    FrameCallback m_frameCallback;
    StatusCallback m_statusCallback;

    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_streaming{false};
    std::atomic<bool> m_stopRequested{false};
    std::thread m_captureThread;

    TriggerMode m_triggerMode = TriggerMode::Continuous;
    std::mutex m_triggerMutex;
    std::condition_variable m_triggerCv;
    std::atomic<bool> m_triggerPending{false};
};

// Optional helper for factory registration from outside.
std::shared_ptr<ICamera> createUSBCamera()
{
    return std::make_shared<USBCamera>();
}
} // namespace HMVision
