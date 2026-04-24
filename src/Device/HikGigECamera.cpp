#include "Device/HikGigECamera.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#if defined(HMVISION_HAVE_MVS) && HMVISION_HAVE_MVS
#include "MvCameraControl.h"
#endif

namespace HMVision
{
namespace
{
static void formatIp4FromHikU32(std::uint32_t ip, std::string& out)
{
    const auto* b = reinterpret_cast<const unsigned char*>(&ip);
    out = std::to_string(static_cast<unsigned int>(b[0])) + "."
        + std::to_string(static_cast<unsigned int>(b[1])) + "."
        + std::to_string(static_cast<unsigned int>(b[2])) + "."
        + std::to_string(static_cast<unsigned int>(b[3]));
}

static std::string trim(std::string_view s)
{
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string_view::npos)
    {
        return {};
    }
    const auto b = s.find_last_not_of(" \t");
    return std::string(s.substr(a, b - a + 1U));
}

static bool isDottedV4(const std::string& s)
{
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    if (4 != std::sscanf(s.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d))
    {
        return false;
    }
    for (int v : { a, b, c, d })
    {
        if (v < 0 || v > 255)
        {
            return false;
        }
    }
    return true;
}

/// 将 a.b.c.d 打包为与常见 nCurrentIp 可比较的 32 位值（大端网序与常见小端各一种）
static void packIpv4TwoWays(
    int a, int b, int c, int d, std::uint32_t& nbo, std::uint32_t& leAsOctets)
{
    nbo = (static_cast<std::uint32_t>(a) << 24U) | (static_cast<std::uint32_t>(b) << 16U)
        | (static_cast<std::uint32_t>(c) << 8U) | static_cast<std::uint32_t>(d);
    leAsOctets = static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8U)
        | (static_cast<std::uint32_t>(c) << 16U) | (static_cast<std::uint32_t>(d) << 24U);
}

static bool ipv4DottedEqualsHikCurrentIp(
    const std::string& key, const char* dIpFromFormat, std::uint32_t nCurrentIp)
{
    if (dIpFromFormat != nullptr && key == std::string(dIpFromFormat))
    {
        return true;
    }
    int a = 0;
    int b2 = 0;
    int c2 = 0;
    int d2 = 0;
    if (4 != std::sscanf(key.c_str(), "%d.%d.%d.%d", &a, &b2, &c2, &d2))
    {
        return false;
    }
    if (a < 0
        || a > 255
        || b2 < 0
        || b2 > 255
        || c2 < 0
        || c2 > 255
        || d2 < 0
        || d2 > 255)
    {
        return false;
    }
    std::uint32_t nbo = 0U;
    std::uint32_t le = 0U;
    packIpv4TwoWays(a, b2, c2, d2, nbo, le);
    if (nCurrentIp == nbo || nCurrentIp == le)
    {
        return true;
    }
    // 与 Hik 侧 formatIp4FromHikU32 一致：按设备内存小端 4 字节读
    const auto* bytes = reinterpret_cast<const unsigned char*>(&nCurrentIp);
    if (a == static_cast<int>(bytes[0]) && b2 == static_cast<int>(bytes[1])
        && c2 == static_cast<int>(bytes[2]) && d2 == static_cast<int>(bytes[3]))
    {
        return true;
    }
    return false;
}
} // namespace

HikGigECamera::HikGigECamera(CameraConfig config)
    : HikCameraBase(std::move(config))
{
    m_config.type = CameraType::HIKVISION_GIGE;
}

bool HikGigECamera::open()
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
    const int retEnum = MV_CC_EnumDevices(MV_GIGE_DEVICE, &stList);
    if (retEnum != MV_OK)
    {
        setLastError("MV_CC_EnumDevices (GigE) failed. Check MVS and drivers (0x" + std::to_string(retEnum) + ").");
        return false;
    }
    if (stList.pDeviceInfo == nullptr || stList.nDeviceNum == 0U)
    {
        setLastError("No Hikvision GigE device. connectionString: " + m_config.connectionString);
        return false;
    }

    const std::string key = trim(m_config.connectionString);
    const bool keyIsIp = !key.empty() && isDottedV4(key);

    int best = -1;
    if (!key.empty())
    {
        for (unsigned int i = 0U; i < stList.nDeviceNum; ++i)
        {
            const MV_CC_DEVICE_INFO* pdi = stList.pDeviceInfo[i];
            if (pdi == nullptr)
            {
                continue;
            }
            if (pdi->nTLayerType != MV_GIGE_DEVICE)
            {
                continue;
            }
            const auto& g = pdi->SpecialInfo.stGigEInfo;
            if (keyIsIp)
            {
                std::string dIp;
                formatIp4FromHikU32(g.nCurrentIp, dIp);
                if (ipv4DottedEqualsHikCurrentIp(key, dIp.c_str(), g.nCurrentIp))
                {
                    best = static_cast<int>(i);
                    break;
                }
            }
            else
            {
                const char* ser
                    = reinterpret_cast<const char*>(g.chSerialNumber);
                if (key
                    == std::string(ser, strnlen(ser, sizeof g.chSerialNumber)))
                {
                    best = static_cast<int>(i);
                    break;
                }
                const char* udn
                    = reinterpret_cast<const char*>(g.chUserDefinedName);
                if (key
                    == std::string(udn, strnlen(udn, sizeof g.chUserDefinedName)))
                {
                    best = static_cast<int>(i);
                    break;
                }
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
            if (pdi->nTLayerType != MV_GIGE_DEVICE)
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
            if (pdi->nTLayerType == MV_GIGE_DEVICE)
            {
                best = static_cast<int>(i);
                break;
            }
        }
    }

    if (best < 0)
    {
        std::string seen;
        for (unsigned int j = 0U; j < stList.nDeviceNum; ++j)
        {
            const MV_CC_DEVICE_INFO* pdj = stList.pDeviceInfo[j];
            if (pdj == nullptr)
            {
                continue;
            }
            if (pdj->nTLayerType != MV_GIGE_DEVICE)
            {
                continue;
            }
            std::string one;
            formatIp4FromHikU32(
                pdj->SpecialInfo.stGigEInfo.nCurrentIp, one);
            if (!seen.empty())
            {
                seen += ", ";
            }
            seen += one;
        }
        setLastError("No matching GigE for \"" + m_config.connectionString
            + "\". Enumerated IP(s): " + (seen.empty() ? "(none)" : seen)
            + ". Set IP/serial to match MVS, or use deviceIndex.");
        return false;
    }

    MV_CC_DEVICE_INFO* pCho = stList.pDeviceInfo[static_cast<std::size_t>(best)];
    if (pCho == nullptr)
    {
        setLastError("GigE device list entry is null.");
        return false;
    }
    if (!createHandleAndOpenFromDeviceInfo(pCho, sizeof(MV_CC_DEVICE_INFO)))
    {
        if (m_lastError.empty())
        {
            setLastError("open GigE: create handle / open device failed");
        }
        return false;
    }
    return true;
#endif
}
} // namespace HMVision
