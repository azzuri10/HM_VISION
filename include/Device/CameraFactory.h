#pragma once

#include "ICamera.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace HMVision
{
class CameraFactory
{
public:
    using Creator = std::function<std::shared_ptr<ICamera>()>;

    static CameraFactory& getInstance()
    {
        static CameraFactory instance;
        return instance;
    }

    void registerCreator(CameraType type, Creator creator)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_creators[type] = std::move(creator);
    }

    std::shared_ptr<ICamera> create(CameraType type) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_creators.find(type);
        if (it == m_creators.end() || !it->second)
        {
            return nullptr;
        }
        return it->second();
    }

private:
    CameraFactory() = default;
    CameraFactory(const CameraFactory&) = delete;
    CameraFactory& operator=(const CameraFactory&) = delete;

    mutable std::mutex m_mutex;
    std::unordered_map<CameraType, Creator> m_creators;
};
} // namespace HMVision
