#pragma once

#include "Device/HikCameraBase.h"

namespace HMVision
{
class HikUSBCamera final : public HikCameraBase
{
public:
    explicit HikUSBCamera(CameraConfig config);
    ~HikUSBCamera() override = default;

    bool open() override;
};
} // namespace HMVision
