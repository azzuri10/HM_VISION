#pragma once

#include <QString>

namespace HMVision
{
enum class DeviceStatus
{
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct CameraInfo
{
    QString id;
    QString name;
    DeviceStatus status = DeviceStatus::Disconnected;
};
} // namespace HMVision
