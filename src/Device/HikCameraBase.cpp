#include "Device/HikCameraBase.h"

#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <sstream>

#if defined(HMVISION_HAVE_MVS) && HMVISION_HAVE_MVS
#include "MvCameraControl.h"
#include "MvErrorDefine.h"
#include "PixelType.h"
#endif

namespace HMVision
{
namespace
{
std::mutex s_mvsInitMutex;
int s_mvsInitCount{0};

#if defined(HMVISION_HAVE_MVS) && HMVISION_HAVE_MVS
const char* mvsMessage(int c)
{
    switch (c)
    {
    case MV_OK:
        return "OK";
    case MV_E_ACCESS_DENIED:
        return "Access denied (busy / permission).";
    case MV_E_BUSY:
        return "Resource busy (callback, etc).";
    case MV_E_NODATA:
        return "No data / timeout.";
    case MV_E_PARAMETER:
        return "Parameter / feature rejected.";
    default:
        return "MVS call failed (see 0x code).";
    }
}

static void formatIp4FromHikU32(std::uint32_t ip, std::string& out)
{
    const auto* b = reinterpret_cast<const unsigned char*>(&ip);
    out = std::to_string(static_cast<unsigned int>(b[0])) + "."
        + std::to_string(static_cast<unsigned int>(b[1])) + "."
        + std::to_string(static_cast<unsigned int>(b[2])) + "."
        + std::to_string(static_cast<unsigned int>(b[3]));
}
#endif
} // namespace

HikCameraBase::HikCameraBase(CameraConfig config)
    : m_config(std::move(config))
{
}

HikCameraBase::~HikCameraBase()
{
    HikCameraBase::close();
}

void HikCameraBase::setLastError(const std::string& err)
{
    m_lastError = err;
}

void HikCameraBase::setMvsErrorCode(int mvsCode, const char* when)
{
    std::ostringstream o;
    o << when;
#if defined(HMVISION_HAVE_MVS) && HMVISION_HAVE_MVS
    o << " [" << mvsMessage(mvsCode) << "]";
#endif
    o << " (0x" << std::hex << mvsCode << std::dec << ")";
    m_lastError = o.str();
}

static bool toDouble(const CameraParam& p, double& o)
{
    if (const double* d = std::get_if<double>(&p))
    {
        o = *d;
        return true;
    }
    if (const int* i = std::get_if<int>(&p))
    {
        o = static_cast<double>(*i);
        return true;
    }
    return false;
}

static bool toInt(const CameraParam& p, int& o)
{
    if (const int* i = std::get_if<int>(&p))
    {
        o = *i;
        return true;
    }
    if (const double* d = std::get_if<double>(&p))
    {
        o = static_cast<int>(*d);
        return true;
    }
    return false;
}

static bool toBoolParam(const CameraParam& p, bool& o)
{
    if (const bool* b = std::get_if<bool>(&p))
    {
        o = *b;
        return true;
    }
    if (const int* i = std::get_if<int>(&p))
    {
        o = *i != 0;
        return true;
    }
    return false;
}

bool HikCameraBase::createHandleAndOpenFromDeviceInfo(
    const void* pDeviceInfo, std::size_t deviceInfoSize)
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    (void)deviceInfoSize;
    (void)pDeviceInfo;
    setLastError("Build without Hikvision MVS. Install the SDK, set HMVISION_MVS_ROOT, and reconfigure.");
    return false;
#else
    if (!pDeviceInfo)
    {
        setLastError("createHandleAndOpen: null device info");
        return false;
    }
    (void)deviceInfoSize; // if 0, sizeof(MV_CC_DEVICE_INFO) is used implicitly by SDK
    {
        std::lock_guard g(s_mvsInitMutex);
        if (s_mvsInitCount == 0)
        {
            const int r = MV_CC_Initialize();
            if (r != MV_OK)
            {
                setMvsErrorCode(r, "MV_CC_Initialize");
                return false;
            }
        }
        ++s_mvsInitCount;
    }

    if (m_handle)
    {
        (void)releaseHandleNoLock();
    }

    MV_CC_DEVICE_INFO* pMut = const_cast<MV_CC_DEVICE_INFO*>(
        static_cast<const MV_CC_DEVICE_INFO*>(pDeviceInfo));
    int ret = MV_CC_CreateHandle(&m_handle, pMut);
    if (ret != MV_OK || m_handle == nullptr)
    {
        setMvsErrorCode(ret, "MV_CC_CreateHandle");
        m_handle = nullptr;
        std::lock_guard g2(s_mvsInitMutex);
        if (s_mvsInitCount > 0)
        {
            --s_mvsInitCount;
        }
        if (s_mvsInitCount == 0)
        {
            MV_CC_Finalize();
        }
        return false;
    }

    ret = MV_CC_OpenDevice(m_handle);
    if (ret != MV_OK)
    {
        setMvsErrorCode(ret, "MV_CC_OpenDevice");
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        std::lock_guard g2(s_mvsInitMutex);
        if (s_mvsInitCount > 0)
        {
            --s_mvsInitCount;
        }
        if (s_mvsInitCount == 0)
        {
            MV_CC_Finalize();
        }
        return false;
    }

    {
        const auto* dev = static_cast<const MV_CC_DEVICE_INFO*>(pDeviceInfo);
        m_deviceSerial.clear();
        m_deviceIp.clear();
        if (dev->nTLayerType == MV_GIGE_DEVICE)
        {
            m_deviceSerial = std::string(
                reinterpret_cast<const char*>(dev->SpecialInfo.stGigEInfo.chSerialNumber),
                strnlen(
                    reinterpret_cast<const char*>(dev->SpecialInfo.stGigEInfo.chSerialNumber),
                    sizeof dev->SpecialInfo.stGigEInfo.chSerialNumber));
            formatIp4FromHikU32(dev->SpecialInfo.stGigEInfo.nCurrentIp, m_deviceIp);
        }
        else if (dev->nTLayerType == MV_USB_DEVICE)
        {
            m_deviceSerial = std::string(
                reinterpret_cast<const char*>(dev->SpecialInfo.stUsb3VInfo.chSerialNumber),
                strnlen(
                    reinterpret_cast<const char*>(dev->SpecialInfo.stUsb3VInfo.chSerialNumber),
                    sizeof dev->SpecialInfo.stUsb3VInfo.chSerialNumber));
        }
    }
    m_opened.store(true);
    (void)applyStartupFromConfig();

    {
        const auto* d = static_cast<const MV_CC_DEVICE_INFO*>(pDeviceInfo);
        if (d->nTLayerType == MV_GIGE_DEVICE)
        {
            int opt = MV_CC_GetOptimalPacketSize(m_handle);
            if (opt > 0)
            {
                if (MV_CC_SetIntValue(m_handle, "GevSCPSPacketSize", static_cast<unsigned int>(opt)) != MV_OK)
                {
                    (void)setGevPacketSize();
                }
            }
            else
            {
                (void)setGevPacketSize();
            }
        }
    }
    return true;
#endif
}

bool HikCameraBase::releaseHandleNoLock()
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    m_opened.store(false);
    m_handle = nullptr;
    return true;
#else
    if (m_handle)
    {
        MV_CC_StopGrabbing(m_handle);
        MV_CC_RegisterImageCallBackEx(m_handle, nullptr, nullptr);
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
    }
    m_opened.store(false);
    m_grabbing.store(false);
    std::lock_guard g2(s_mvsInitMutex);
    if (s_mvsInitCount > 0)
    {
        --s_mvsInitCount;
    }
    if (s_mvsInitCount == 0)
    {
        MV_CC_Finalize();
    }
    return true;
#endif
}

bool HikCameraBase::setGevPacketSize()
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    return true;
#else
    if (m_config.type != CameraType::HIKVISION_GIGE || m_handle == nullptr)
    {
        return true;
    }
    int packet = 1500;
    if (const auto it = m_config.customParams.find("packet_size"); it != m_config.customParams.end())
    {
        if (const int* p = std::get_if<int>(&it->second))
        {
            packet = *p;
        }
    }
    return MV_CC_SetIntValue(m_handle, "GevSCPSPacketSize", static_cast<unsigned int>(packet)) == MV_OK;
#endif
}

bool HikCameraBase::applyStartupFromConfig()
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    return true;
#else
    if (m_handle == nullptr)
    {
        return false;
    }
    (void)MV_CC_SetFloatValue(
        m_handle, "ExposureTime", static_cast<float>(m_config.exposureTime));
    (void)MV_CC_SetFloatValue(m_handle, "Gain", static_cast<float>(m_config.gain));
    (void)MV_CC_SetFloatValue(
        m_handle, "AcquisitionFrameRate", static_cast<float>(m_config.frameRate));
    if (m_config.width > 0)
    {
        (void)MV_CC_SetIntValue(
            m_handle, "Width", static_cast<unsigned int>(m_config.width));
    }
    if (m_config.height > 0)
    {
        (void)MV_CC_SetIntValue(
            m_handle, "Height", static_cast<unsigned int>(m_config.height));
    }
    if (m_config.offsetX > 0)
    {
        (void)MV_CC_SetIntValue(
            m_handle, "OffsetX", static_cast<unsigned int>(m_config.offsetX));
    }
    if (m_config.offsetY > 0)
    {
        (void)MV_CC_SetIntValue(
            m_handle, "OffsetY", static_cast<unsigned int>(m_config.offsetY));
    }
    return true;
#endif
}

#if defined(HMVISION_HAVE_MVS) && HMVISION_HAVE_MVS
static void __stdcall s_imageCallbackEx(
    unsigned char* pData, MV_FRAME_OUT_INFO_EX* pInfo, void* pUser)
{
    auto* self = static_cast<HikCameraBase*>(pUser);
    if (!self || !pData || !pInfo)
    {
        return;
    }
    self->notifyFrameFromMvs(pData, pInfo->nWidth, pInfo->nHeight, pInfo->enPixelType);
}
#endif

void HikCameraBase::registerMvsImageCallback()
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
#else
    if (m_handle)
    {
        (void)MV_CC_RegisterImageCallBackEx(m_handle, s_imageCallbackEx, this);
    }
#endif
}

void HikCameraBase::unregisterMvsImageCallback()
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
#else
    if (m_handle)
    {
        (void)MV_CC_RegisterImageCallBackEx(m_handle, nullptr, nullptr);
    }
#endif
}

void HikCameraBase::pushExternalCallback(const CameraFrame& f)
{
    std::lock_guard g(m_userCbMutex);
    if (m_userFrameCallback)
    {
        m_userFrameCallback(f);
    }
}

void HikCameraBase::notifyFrameFromMvs(
    const unsigned char* data, std::uint32_t w, std::uint32_t h, std::uint32_t pixelType)
{
    if (data == nullptr || w == 0U || h == 0U)
    {
        return;
    }
    cv::Mat bgr;
#if defined(HMVISION_HAVE_MVS) && HMVISION_HAVE_MVS
    const unsigned int pt = static_cast<unsigned int>(pixelType);
    if (pt == PixelType_Gvsp_Mono8)
    {
        cv::Mat m(
            static_cast<int>(h),
            static_cast<int>(w),
            CV_8UC1,
            const_cast<unsigned char*>(data),
            static_cast<std::size_t>(w));
        cv::cvtColor(m, bgr, cv::COLOR_GRAY2BGR);
    }
    else if (pt == PixelType_Gvsp_BayerGR8)
    {
        cv::Mat raw(
            static_cast<int>(h),
            static_cast<int>(w),
            CV_8UC1,
            const_cast<unsigned char*>(data),
            w);
        cv::cvtColor(raw, bgr, cv::COLOR_BayerGR2BGR);
    }
    else if (pt == PixelType_Gvsp_BayerRG8)
    {
        cv::Mat raw(
            static_cast<int>(h),
            static_cast<int>(w),
            CV_8UC1,
            const_cast<unsigned char*>(data),
            w);
        cv::cvtColor(raw, bgr, cv::COLOR_BayerRG2BGR);
    }
    else if (pt == PixelType_Gvsp_BayerGB8)
    {
        cv::Mat raw(
            static_cast<int>(h),
            static_cast<int>(w),
            CV_8UC1,
            const_cast<unsigned char*>(data),
            w);
        cv::cvtColor(raw, bgr, cv::COLOR_BayerGB2BGR);
    }
    else if (pt == PixelType_Gvsp_BayerBG8)
    {
        cv::Mat raw(
            static_cast<int>(h),
            static_cast<int>(w),
            CV_8UC1,
            const_cast<unsigned char*>(data),
            w);
        cv::cvtColor(raw, bgr, cv::COLOR_BayerBG2BGR);
    }
    else if (pt == PixelType_Gvsp_RGB8_Packed)
    {
        cv::Mat rgb(
            static_cast<int>(h),
            static_cast<int>(w),
            CV_8UC3,
            const_cast<unsigned char*>(data),
            w * 3U);
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    }
    else if (pt == PixelType_Gvsp_BGR8_Packed)
    {
        bgr
            = cv::Mat(
                  static_cast<int>(h),
                  static_cast<int>(w),
                  CV_8UC3,
                  const_cast<unsigned char*>(data),
                  w * 3U)
                  .clone();
    }
    else
    {
        (void)pt;
        return;
    }
#else
    (void)pixelType;
    return;
#endif
    std::uint64_t fid = 0U;
    {
        std::lock_guard g(m_imageMutex);
        bgr.copyTo(m_lastBgr);
        m_hasNewFrame = true;
        m_lastFrameId = ++m_frameSeq;
        fid = m_lastFrameId;
    }
    m_imageCond.notify_all();

    CameraFrame out;
    out.image = bgr;
    out.cameraId = m_config.id;
    out.frameId = fid;
    out.timestamp = std::chrono::system_clock::now();
    out.exposureTime = m_config.exposureTime;
    out.gain = m_config.gain;
    pushExternalCallback(out);
}

bool HikCameraBase::close()
{
    m_grabbing.store(false);
    stopGrabbing();
    std::lock_guard g(m_infoMutex);
    (void)releaseHandleNoLock();
    {
        std::lock_guard g2(m_imageMutex);
        m_lastBgr.release();
        m_hasNewFrame = false;
    }
    return true;
}

bool HikCameraBase::isOpen() const
{
    return m_opened.load();
}

bool HikCameraBase::startGrabbing()
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    setLastError("MVS not available (rebuild with MVS or enable HMVISION_HAVE_MVS).");
    return false;
#else
    if (m_handle == nullptr || !m_opened.load())
    {
        setLastError("Not open");
        return false;
    }
    registerMvsImageCallback();
    const int r = MV_CC_StartGrabbing(m_handle);
    if (r != MV_OK)
    {
        setMvsErrorCode(r, "MV_CC_StartGrabbing");
        return false;
    }
    m_grabbing.store(true);
    return true;
#endif
}

bool HikCameraBase::stopGrabbing()
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    m_grabbing.store(false);
    return true;
#else
    m_grabbing.store(false);
    if (m_handle)
    {
        (void)MV_CC_StopGrabbing(m_handle);
        (void)MV_CC_RegisterImageCallBackEx(m_handle, nullptr, nullptr);
    }
    return true;
#endif
}

bool HikCameraBase::isGrabbing() const
{
    return m_grabbing.load();
}

bool HikCameraBase::grabFrame(CameraFrame& frame, int timeoutMs)
{
    std::unique_lock lk(m_imageMutex);
    if (m_imageCond.wait_for(
            lk, std::chrono::milliseconds(timeoutMs), [this] { return m_hasNewFrame; }))
    {
        if (m_lastBgr.empty())
        {
            m_hasNewFrame = false;
            return false;
        }
        m_lastBgr.copyTo(frame.image);
        m_hasNewFrame = false;
        frame.cameraId = m_config.id;
        frame.frameId = m_lastFrameId;
        frame.timestamp = std::chrono::system_clock::now();
        frame.exposureTime = m_config.exposureTime;
        frame.gain = m_config.gain;
        return true;
    }
    return false;
}

bool HikCameraBase::setParameter(const std::string& name, const CameraParam& value)
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    (void)name;
    (void)value;
    return false;
#else
    if (m_handle == nullptr)
    {
        return false;
    }
    if (name == "exposure_time")
    {
        double v = 0.0;
        if (!toDouble(value, v))
        {
            return false;
        }
        m_config.exposureTime = v;
        return MV_CC_SetFloatValue(
                   m_handle, "ExposureTime", static_cast<float>(v))
            == MV_OK;
    }
    if (name == "gain")
    {
        double v2 = 0.0;
        if (!toDouble(value, v2))
        {
            return false;
        }
        m_config.gain = v2;
        return MV_CC_SetFloatValue(m_handle, "Gain", static_cast<float>(v2)) == MV_OK;
    }
    if (name == "frame_rate")
    {
        double v3 = 0.0;
        if (!toDouble(value, v3))
        {
            return false;
        }
        m_config.frameRate = v3;
        return MV_CC_SetFloatValue(
                   m_handle, "AcquisitionFrameRate", static_cast<float>(v3))
            == MV_OK;
    }
    if (name == "width" || name == "height" || name == "offset_x" || name == "offset_y")
    {
        int n = 0;
        if (!toInt(value, n))
        {
            return false;
        }
        const bool wasG = m_grabbing.load();
        if (wasG)
        {
            m_grabbing.store(false);
            (void)MV_CC_StopGrabbing(m_handle);
        }
        int ret = MV_OK;
        if (name == "width")
        {
            m_config.width = n;
            ret = MV_CC_SetIntValue(m_handle, "Width", static_cast<unsigned int>(n));
        }
        else if (name == "height")
        {
            m_config.height = n;
            ret = MV_CC_SetIntValue(m_handle, "Height", static_cast<unsigned int>(n));
        }
        else if (name == "offset_x")
        {
            m_config.offsetX = n;
            ret = MV_CC_SetIntValue(m_handle, "OffsetX", static_cast<unsigned int>(n));
        }
        else
        {
            m_config.offsetY = n;
            ret = MV_CC_SetIntValue(m_handle, "OffsetY", static_cast<unsigned int>(n));
        }
        if (wasG)
        {
            registerMvsImageCallback();
            if (MV_CC_StartGrabbing(m_handle) == MV_OK)
            {
                m_grabbing.store(true);
            }
            else
            {
                m_grabbing.store(false);
            }
        }
        return ret == MV_OK;
    }
    if (name == "trigger_mode")
    {
        bool b = false;
        if (!toBoolParam(value, b))
        {
            return false;
        }
        m_config.triggerMode = b;
        const unsigned int onOff = b ? 1U : 0U;
        return MV_CC_SetEnumValue(m_handle, "TriggerMode", onOff) == MV_OK;
    }
    if (name == "trigger_source")
    {
        int s = 0;
        if (!toInt(value, s))
        {
            return false;
        }
        m_config.triggerSource = s;
        return MV_CC_SetEnumValue(m_handle, "TriggerSource", static_cast<unsigned int>(s)) == MV_OK;
    }
    return false;
#endif
}

CameraParam HikCameraBase::getParameter(const std::string& name) const
{
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    (void)name;
    return int{0};
#else
    if (m_handle == nullptr)
    {
        return int{0};
    }
    if (name == "exposure_time")
    {
        MVCC_FLOATVALUE fe{};
        if (MV_CC_GetFloatValue(m_handle, "ExposureTime", &fe) == MV_OK)
        {
            return static_cast<double>(fe.fCurValue);
        }
    }
    if (name == "gain")
    {
        MVCC_FLOATVALUE gv{};
        if (MV_CC_GetFloatValue(m_handle, "Gain", &gv) == MV_OK)
        {
            return static_cast<double>(gv.fCurValue);
        }
    }
    if (name == "width")
    {
        MVCC_INTVALUE v{};
        if (MV_CC_GetIntValue(m_handle, "Width", &v) == MV_OK)
        {
            return static_cast<int>(v.nCurValue);
        }
    }
    if (name == "height")
    {
        MVCC_INTVALUE v2{};
        if (MV_CC_GetIntValue(m_handle, "Height", &v2) == MV_OK)
        {
            return static_cast<int>(v2.nCurValue);
        }
    }
    (void)name;
    return int{0};
#endif
}

std::vector<std::string> HikCameraBase::getParameterList() const
{
    return { "exposure_time",  "gain",    "frame_rate", "width",  "height",
             "offset_x",       "offset_y", "trigger_mode", "trigger_source" };
}

CameraInfo HikCameraBase::getCameraInfo() const
{
    CameraInfo i;
    i.id = m_config.id;
    i.name = m_config.name;
    i.type = m_config.type;
    i.serialNumber = m_deviceSerial;
    i.ipAddress = m_deviceIp.empty() ? m_config.connectionString : m_deviceIp;
    if (m_opened.load() && m_grabbing.load())
    {
        i.status = CameraStatus::GRABBING;
    }
    else if (m_opened.load())
    {
        i.status = CameraStatus::CONNECTED;
    }
    else
    {
        i.status = CameraStatus::DISCONNECTED;
    }
    i.lastError = m_lastError;
    return i;
}

CameraConfig HikCameraBase::getCameraConfig() const
{
    return m_config;
}

void HikCameraBase::setFrameCallback(FrameCallback callback)
{
    std::lock_guard g(m_userCbMutex);
    m_userFrameCallback = std::move(callback);
}

void HikCameraBase::removeFrameCallback()
{
    std::lock_guard g(m_userCbMutex);
    m_userFrameCallback = nullptr;
}

void HikCameraBase::onCameraEvent(const std::string& /*event*/, const std::string& /*data*/)
{
}
} // namespace HMVision
