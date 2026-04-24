#pragma once

#include "ICamera.h"

#include <atomic>
#include <memory>
#include <thread>

namespace cv
{
class VideoCapture;
}

namespace HMVision
{
class USB_Camera : public ICamera
{
    Q_OBJECT
public:
    explicit USB_Camera(const QString& id, int deviceIndex, QObject* parent = nullptr);
    ~USB_Camera() override;

    CameraInfo info() const override;
    bool open() override;
    void close() override;
    bool isOpened() const override;
    bool startCapture() override;
    void stopCapture() override;
    bool isCapturing() const override;

private:
    void captureLoop();

    CameraInfo m_info;
    int m_deviceIndex = 0;
    std::unique_ptr<cv::VideoCapture> m_capture;
    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_capturing{false};
    std::thread m_captureThread;
};
} // namespace HMVision
