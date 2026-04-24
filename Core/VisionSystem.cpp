#include "VisionSystem.h"
#include "Logger.h"

namespace HMVision
{
VisionSystem::VisionSystem(QObject* parent)
    : QObject(parent)
    , m_cameraManager(std::make_unique<CameraManager>())
{
}

bool VisionSystem::initialize()
{
    if (m_initialized)
    {
        return true;
    }

    Logger::info("Initializing vision system...");
    m_initialized = true;
    emit initialized();
    return true;
}

void VisionSystem::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    Logger::info("Shutting down vision system...");
    m_cameraManager->stopAll();
    m_initialized = false;
    emit stopped();
}

ConfigManager& VisionSystem::configManager()
{
    return ConfigManager::getInstance();
}

CameraManager& VisionSystem::cameraManager()
{
    return *m_cameraManager;
}
} // namespace HMVision
