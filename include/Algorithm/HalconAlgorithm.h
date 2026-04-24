#pragma once

#include "Algorithm/IVisionAlgorithm.h"

#include <mutex>
#include <string>

namespace HMVision
{
class HalconAlgorithm : public IVisionAlgorithm
{
public:
    std::string name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const cv::Mat& frame) override;
    void release() override;
    bool isInitialized() const override;

private:
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    AlgorithmParams m_params;
};
} // namespace HMVision
