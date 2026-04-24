#pragma once

#include "ICamera.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <memory>

namespace HMVision
{
class CameraManager : public QObject
{
    Q_OBJECT
public:
    explicit CameraManager(QObject* parent = nullptr);

    bool registerCamera(const QString& id, std::shared_ptr<ICamera> camera);
    bool removeCamera(const QString& id);
    std::shared_ptr<ICamera> camera(const QString& id) const;
    QList<CameraInfo> cameraList() const;
    bool startCamera(const QString& id);
    void stopCamera(const QString& id);
    bool startAll();
    void stopAll();

signals:
    void cameraRegistered(const QString& id);
    void cameraRemoved(const QString& id);
    void cameraStatusChanged(const QString& id, HMVision::DeviceStatus status);
    void cameraFrameReady(const QString& id, const QImage& frame);

private:
    QMap<QString, std::shared_ptr<ICamera>> m_cameras;
};
} // namespace HMVision
