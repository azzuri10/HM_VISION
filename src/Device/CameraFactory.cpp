#include "Device/CameraFactory.h"

namespace HMVision
{
CameraFactory& CameraFactory::getInstance()
{
    static CameraFactory instance;
    return instance;
}

void CameraFactory::registerCamera(const CameraType type, CameraCreator creator)
{
    if (!creator)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_creators[type] = std::move(creator);
}

ICameraPtr CameraFactory::createCamera(const CameraConfig& config) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_creators.find(config.type);
    if (it == m_creators.end() || !it->second)
    {
        return nullptr;
    }
    return it->second(config);
}
} // namespace HMVision
