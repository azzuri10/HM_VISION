#pragma once

#include "../Common/CameraTypes.h"

#include <functional>
#include <string>
#include <vector>

namespace HMVision
{
class ICamera
{
public:
    using FrameCallback = std::function<void(const std::string& cameraId, const std::vector<std::uint8_t>& payload)>;
    using StatusCallback = std::function<void(const std::string& cameraId, const CameraStatus& status)>;

    virtual ~ICamera() = default;

    virtual std::string id() const = 0;
    virtual CameraType type() const = 0;
    virtual bool open(const CameraConfig& config) = 0;
    virtual void close() = 0;
    virtual bool startStream() = 0;
    virtual void stopStream() = 0;
    virtual bool isOpened() const = 0;
    virtual bool isStreaming() const = 0;
    virtual CameraStatus status() const = 0;
    virtual CameraConfig config() const = 0;

    virtual void setFrameCallback(FrameCallback callback) = 0;
    virtual void setStatusCallback(StatusCallback callback) = 0;
};
} // namespace HMVision
