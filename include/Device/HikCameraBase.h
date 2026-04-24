#pragma once

#include "Device/ICamera.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace HMVision
{
/// Hikvision MVS: shared open/grab, GenICam parameters, callback.
/// HikGigECamera / HikUSBCamera only implement which device to pick in open().
class HikCameraBase : public ICamera
{
public:
    explicit HikCameraBase(CameraConfig config);
    ~HikCameraBase() override;

    /// Not implemented in base. Implemented by HikGigECamera (GigE) and HikUSBCamera (USB3).
    bool open() override = 0;

    bool close() override;
    bool isOpen() const override;

    bool startGrabbing() override;
    bool stopGrabbing() override;
    bool isGrabbing() const override;

    bool grabFrame(CameraFrame& frame, int timeoutMs = 1000) override;

    bool setParameter(const std::string& name, const CameraParam& value) override;
    CameraParam getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterList() const override;

    CameraInfo getCameraInfo() const override;
    CameraConfig getCameraConfig() const override;

    void setFrameCallback(FrameCallback callback) override;
    void removeFrameCallback() override;
    void onCameraEvent(const std::string& event, const std::string& data) override;

    /// MVS image callback (registered with MV_CC_RegisterImageCallBackEx) calls this; must be
    /// callable from a free C thunk, hence public.
    void notifyFrameFromMvs(
        const unsigned char* data,
        std::uint32_t w,
        std::uint32_t h,
        std::uint32_t pixelType
    );

protected:
    CameraConfig m_config;
    void setMvsErrorCode(int mvsCode, const char* when);
    void setLastError(const std::string& err);

    /// \param pDeviceInfo must point to a valid MVS `MV_CC_DEVICE_INFO` (SDK layout).
    bool createHandleAndOpenFromDeviceInfo(
        const void* pDeviceInfo, std::size_t deviceInfoSize = 0U);

    std::string m_lastError;
    void* m_handle{nullptr};

private:
    bool releaseHandleNoLock();
    bool applyStartupFromConfig();
    bool setGevPacketSize();
    void registerMvsImageCallback();
    void unregisterMvsImageCallback();
    void internalFrameCallbackThunks();
    void pushExternalCallback(const CameraFrame& f);

    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_grabbing{false};
    std::uint64_t m_frameSeq{0};
    std::uint64_t m_lastFrameId{0};

    mutable std::mutex m_infoMutex;
    std::string m_deviceSerial; ///< from opened device, if MVS
    std::string m_deviceIp;     ///< for GigE

    /// Latest frame (BGR) from MVS — grabFrame or user FrameCallback
    std::mutex m_imageMutex;
    std::condition_variable m_imageCond;
    cv::Mat m_lastBgr;
    bool m_hasNewFrame{false};

    std::mutex m_userCbMutex;
    FrameCallback m_userFrameCallback;
};
} // namespace HMVision
