#pragma once

#include "ConfigManager.h"
#include "../Device/CameraManager.h"

#include <QObject>
#include <memory>

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
    std::unique_ptr<CameraManager> m_cameraManager;
};
} // namespace HMVision
