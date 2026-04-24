#pragma once

#include "Device/ICamera.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace HMVision
{
using CameraCreator = std::function<ICameraPtr(const CameraConfig&)>;

/// 注册各 \p CameraType 的创建器。单例，\p registerCamera / \p createCamera 内部加锁，可从多线程调用。
class CameraFactory
{
public:
    static CameraFactory& getInstance();

    void registerCamera(CameraType type, CameraCreator creator);
    ICameraPtr createCamera(const CameraConfig& config) const;

private:
    CameraFactory() = default;
    CameraFactory(const CameraFactory&) = delete;
    CameraFactory& operator=(const CameraFactory&) = delete;

    mutable std::mutex m_mutex;
    std::map<CameraType, CameraCreator> m_creators;
};
} // namespace HMVision
