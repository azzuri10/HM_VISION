#include "Device/HikUSBCamera.h"

#include <cstring>
#include <string>

#if defined(HMVISION_HAVE_MVS) && HMVISION_HAVE_MVS
#include "MvCameraControl.h"
#endif

namespace HMVision
{
HikUSBCamera::HikUSBCamera(CameraConfig config)
    : HikCameraBase(std::move(config))
{
    m_config.type = CameraType::HIKVISION_USB;
}

bool HikUSBCamera::open()
{
    if (isOpen())
    {
        return true;
    }
#if !defined(HMVISION_HAVE_MVS) || !HMVISION_HAVE_MVS
    setLastError("Hik MVS not linked. Set HMVISION_MVS_ROOT and reconfigure (HMVISION_HAVE_MVS).");
    return false;
#else
    MV_CC_DEVICE_INFO_LIST stList{};
    stList.nDeviceNum = 0U;
    const int retEnum = MV_CC_EnumDevices(MV_USB_DEVICE, &stList);
    if (retEnum != MV_OK)
    {
        setLastError("MV_CC_EnumDevices (USB) failed (MVS=0x" + std::to_string(retEnum) + ").");
        return false;
    }
    if (stList.pDeviceInfo == nullptr || stList.nDeviceNum == 0U)
    {
        setLastError("No Hikvision USB3 camera. connectionString / deviceIndex: "
            + m_config.connectionString + " index=" + std::to_string(m_config.deviceIndex));
        return false;
    }

    int best = -1;
    const std::string& key = m_config.connectionString;
    if (!key.empty())
    {
        for (unsigned int i = 0U; i < stList.nDeviceNum; ++i)
        {
            const MV_CC_DEVICE_INFO* pdi = stList.pDeviceInfo[i];
            if (pdi == nullptr)
            {
                continue;
            }
            if (pdi->nTLayerType != MV_USB_DEVICE)
            {
                continue;
            }
            const char* s = reinterpret_cast<const char*>(
                pdi->SpecialInfo.stUsb3VInfo.chSerialNumber);
            const std::string ser(
                s,
                strnlen(
                    s,
                    sizeof pdi->SpecialInfo.stUsb3VInfo.chSerialNumber));
            if (ser == key)
            {
                best = static_cast<int>(i);
                break;
            }
        }
    }
    if (best < 0)
    {
        for (unsigned int i = 0U; i < stList.nDeviceNum; ++i)
        {
            const MV_CC_DEVICE_INFO* pdi = stList.pDeviceInfo[i];
            if (pdi == nullptr)
            {
                continue;
            }
            if (pdi->nTLayerType != MV_USB_DEVICE)
            {
                continue;
            }
            if (m_config.deviceIndex >= 0 && static_cast<int>(i) == m_config.deviceIndex)
            {
                best = static_cast<int>(i);
                break;
            }
        }
    }

    if (best < 0)
    {
        for (unsigned int i = 0U; i < stList.nDeviceNum; ++i)
        {
            const MV_CC_DEVICE_INFO* pdi = stList.pDeviceInfo[i];
            if (pdi == nullptr)
            {
                continue;
            }
            if (pdi->nTLayerType == MV_USB_DEVICE)
            {
                best = static_cast<int>(i);
                break;
            }
        }
    }

    if (best < 0)
    {
        setLastError("USB3 device list has no valid USB3 entry.");
        return false;
    }

    MV_CC_DEVICE_INFO* pCho = stList.pDeviceInfo[static_cast<std::size_t>(best)];
    if (pCho == nullptr)
    {
        setLastError("USB3 device list entry is null.");
        return false;
    }
    if (!createHandleAndOpenFromDeviceInfo(pCho, sizeof(MV_CC_DEVICE_INFO)))
    {
        if (m_lastError.empty())
        {
            setLastError("open USB: create handle failed");
        }
        return false;
    }
    return true;
#endif
}
} // namespace HMVision
