#pragma once

#include "ICamera.h"

#include <atomic>
#include <thread>

namespace HMVision
{
class GigE_Camera : public ICamera
{
    Q_OBJECT
public:
    explicit GigE_Camera(const QString& id, const QString& endpoint, QObject* parent = nullptr);
    ~GigE_Camera() override;

    CameraInfo info() const override;
    bool open() override;
    void close() override;
    bool isOpened() const override;
    bool startCapture() override;
    void stopCapture() override;
    bool isCapturing() const override;

private:
    void captureLoop();
    QImage buildSyntheticFrame(int frameIndex) const;

    CameraInfo m_info;
    QString m_endpoint;
    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_capturing{false};
    std::thread m_captureThread;
};
} // namespace HMVision
