#pragma once

#include "Core/ConfigManager.h"
#include "Device/CameraManager.h"

#include <QObject>

namespace HMVision
{
class VisionSystem : public QObject
{
    Q_OBJECT
public:
    explicit VisionSystem(QObject* parent = nullptr);

    bool initialize();
    void shutdown();

    ConfigManager& configManager();
    CameraManager& cameraManager();

signals:
    void initialized();
    void stopped();

private:
    bool m_initialized = false;
};
} // namespace HMVision
