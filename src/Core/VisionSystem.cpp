#include "Core/VisionSystem.h"
#include "Core/Logger.h"
#include "Device/CameraManager.h"

namespace HMVision
{
VisionSystem::VisionSystem(QObject* parent)
    : QObject(parent)
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
    CameraManager::getInstance().stopAll();
    m_initialized = false;
    emit stopped();
}

ConfigManager& VisionSystem::configManager()
{
    return ConfigManager::getInstance();
}

CameraManager& VisionSystem::cameraManager()
{
    return CameraManager::getInstance();
}
} // namespace HMVision
