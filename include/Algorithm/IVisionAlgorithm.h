#pragma once

#include "Common/AlgorithmParams.h"
#include "Common/AlgorithmResult.h"

#include <opencv2/core.hpp>

#include <string>

namespace HMVision
{
class IVisionAlgorithm
{
public:
    virtual ~IVisionAlgorithm() = default;

    virtual std::string name() const = 0;
    virtual bool initialize(const AlgorithmParams& params) = 0;
    virtual AlgorithmResult process(const cv::Mat& frame) = 0;
    virtual void release() = 0;
    virtual bool isInitialized() const = 0;
};
} // namespace HMVision
