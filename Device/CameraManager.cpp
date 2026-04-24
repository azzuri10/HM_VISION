#include "CameraManager.h"

namespace HMVision
{
CameraManager::CameraManager(QObject* parent)
    : QObject(parent)
{
}

bool CameraManager::registerCamera(const QString& id, std::shared_ptr<ICamera> camera)
{
    if (!camera || id.isEmpty() || m_cameras.contains(id))
    {
        return false;
    }

    connect(camera.get(), &ICamera::statusChanged, this, [this, id](DeviceStatus status) {
        emit cameraStatusChanged(id, status);
    });
    connect(camera.get(), &ICamera::frameReady, this, [this](const QString& cameraId, const QImage& frame) {
        emit cameraFrameReady(cameraId, frame);
    });

    m_cameras.insert(id, camera);
    emit cameraRegistered(id);
    return true;
}

bool CameraManager::removeCamera(const QString& id)
{
    auto iter = m_cameras.find(id);
    if (iter == m_cameras.end())
    {
        return false;
    }

    iter.value()->stopCapture();
    iter.value()->close();
    m_cameras.erase(iter);
    emit cameraRemoved(id);
    return true;
}

std::shared_ptr<ICamera> CameraManager::camera(const QString& id) const
{
    return m_cameras.value(id);
}

QList<CameraInfo> CameraManager::cameraList() const
{
    QList<CameraInfo> result;
    result.reserve(m_cameras.size());

    for (const auto& camera : m_cameras)
    {
        result.push_back(camera->info());
    }
    return result;
}

bool CameraManager::startCamera(const QString& id)
{
    const auto camera = m_cameras.value(id);
    if (!camera)
    {
        return false;
    }

    if (!camera->isOpened() && !camera->open())
    {
        return false;
    }

    return camera->startCapture();
}

void CameraManager::stopCamera(const QString& id)
{
    const auto camera = m_cameras.value(id);
    if (!camera)
    {
        return;
    }

    camera->stopCapture();
    camera->close();
}

bool CameraManager::startAll()
{
    bool ok = true;
    for (const auto& camera : m_cameras)
    {
        if (!camera->isOpened() && !camera->open())
        {
            ok = false;
            continue;
        }
        ok = camera->startCapture() && ok;
    }
    return ok;
}

void CameraManager::stopAll()
{
    for (const auto& camera : m_cameras)
    {
        camera->stopCapture();
        camera->close();
    }
}
} // namespace HMVision
