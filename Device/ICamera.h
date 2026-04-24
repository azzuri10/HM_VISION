#pragma once

#include "../Common/Types.h"

#include <QImage>
#include <QObject>

namespace HMVision
{
class ICamera : public QObject
{
    Q_OBJECT
public:
    explicit ICamera(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~ICamera() override = default;

    virtual CameraInfo info() const = 0;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpened() const = 0;
    virtual bool startCapture() = 0;
    virtual void stopCapture() = 0;
    virtual bool isCapturing() const = 0;

signals:
    void frameReady(const QString& cameraId, const QImage& frame);
    void statusChanged(HMVision::DeviceStatus status);
};
} // namespace HMVision
