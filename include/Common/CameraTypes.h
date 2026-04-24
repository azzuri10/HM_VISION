#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace HMVision
{
enum class CameraType
{
    Unknown = 0,
    USB,
    GigE,
    Virtual
};

enum class CameraConnectionState
{
    Disconnected = 0,
    Connecting,
    Connected,
    Streaming,
    Error
};

struct CameraStatus
{
    CameraConnectionState state = CameraConnectionState::Disconnected;
    double fps = 0.0;
    std::uint64_t frameCount = 0;
    std::string message;
    std::int64_t lastHeartbeatMs = 0;
};

struct CameraConfig
{
    std::string id;
    std::string name;
    CameraType type = CameraType::Unknown;
    std::string ip;
    int port = 0;
    int deviceIndex = -1;
    int width = 1920;
    int height = 1080;
    double exposure = 0.0;
    double gain = 0.0;
    std::map<std::string, std::string> extraParams;
};
} // namespace HMVision
