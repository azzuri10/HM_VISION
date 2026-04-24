#pragma once

#include "Device/HikCameraBase.h"

namespace HMVision
{
class HikGigECamera final : public HikCameraBase
{
public:
    explicit HikGigECamera(CameraConfig config);
    ~HikGigECamera() override = default;

    bool open() override;
};
} // namespace HMVision
