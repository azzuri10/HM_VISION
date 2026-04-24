#pragma once

#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace HMVision
{
/// Camera vendor / transport
enum class CameraType
{
    UNKNOWN = 0,
    HIKVISION_GIGE, ///< Hikvision GigE (MVS SDK)
    HIKVISION_USB,  ///< Hikvision USB (MVS SDK)
    OPENCV_USB,     ///< UVC / DirectShow via OpenCV (dev kit fallback)
    OPENCV_GIGE,    ///< OpenCV/placeholder (no true GigE in OpenCV; synthetic/test)
    VIRTUAL,        ///< Offline test pattern
};

/// FSM for UI / health
enum class CameraStatus
{
    DISCONNECTED,
    CONNECTED,
    GRABBING,
    ERROR
};

using CameraParam = std::variant<int, double, bool, std::string>;

struct CameraInfo
{
    std::string id;
    std::string name;
    CameraType type = CameraType::UNKNOWN;
    std::string serialNumber;
    std::string ipAddress; ///< for GigE
    CameraStatus status = CameraStatus::DISCONNECTED;
    std::string lastError;
    std::chrono::system_clock::time_point connectTime{};
};

struct CameraConfig
{
    std::string id;
    std::string name;
    CameraType type = CameraType::UNKNOWN;

    /// Generic connection (IP, serial, or other string from UI/JSON)
    std::string connectionString;
    int deviceIndex = 0; ///< OpenCV / USB index

    int width = 1920;
    int height = 1080;
    int offsetX = 0;
    int offsetY = 0;
    std::string pixelFormat; ///< e.g. Mono8, RGB8

    double frameRate = 30.0;
    double exposureTime = 10000.0; ///< µs
    double gain = 0.0;
    bool autoExposure = false;
    bool autoGain = false;
    bool triggerMode = false;
    int triggerSource = 0;
    int triggerDelay = 0;

    /// JSON "advanced" / plugin-specific: packet_size, stream_buffer_count, etc.
    std::map<std::string, CameraParam> customParams;
};

struct CameraFrame
{
    cv::Mat image;
    std::string cameraId;
    std::uint64_t frameId = 0;
    std::chrono::system_clock::time_point timestamp{};
    bool isTriggered = false;
    double exposureTime = 0.0;
    double gain = 0.0;
};

using FrameCallback = std::function<void(const CameraFrame&)>;

class ICamera
{
public:
    virtual ~ICamera() = default;

    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual bool isOpen() const = 0;

    virtual bool startGrabbing() = 0;
    virtual bool stopGrabbing() = 0;
    virtual bool isGrabbing() const = 0;

    virtual bool grabFrame(CameraFrame& frame, int timeoutMs = 1000) = 0;

    virtual bool setParameter(const std::string& name, const CameraParam& value) = 0;
    virtual CameraParam getParameter(const std::string& name) const = 0;
    virtual std::vector<std::string> getParameterList() const = 0;

    virtual CameraInfo getCameraInfo() const = 0;
    virtual CameraConfig getCameraConfig() const = 0;

    virtual void setFrameCallback(FrameCallback callback) = 0;
    virtual void removeFrameCallback() = 0;

    virtual void onCameraEvent(const std::string& event, const std::string& data) = 0;
};

using ICameraPtr = std::shared_ptr<ICamera>;
} // namespace HMVision
