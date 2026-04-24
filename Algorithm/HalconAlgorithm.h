#pragma once

#include "IVisionAlgorithm.h"

#include <opencv2/core.hpp>

#include <mutex>

namespace HMVision
{
class HalconAlgorithm : public IVisionAlgorithm
{
public:
    QString name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const QImage& frame) override;
    void release() override;
    bool isInitialized() const override;

private:
    static cv::Mat qImageToMat(const QImage& image);

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    AlgorithmParams m_params;
};
} // namespace HMVision
