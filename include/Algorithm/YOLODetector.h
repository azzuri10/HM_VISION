#pragma once

#include "IVisionAlgorithm.h"

#include <opencv2/dnn.hpp>

#include <mutex>

namespace HMVision
{
class YOLODetector : public IVisionAlgorithm
{
public:
    std::string name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const cv::Mat& frame) override;
    void release() override;
    bool isInitialized() const override;

private:
    AlgorithmResult postprocess(
        const cv::Mat& output, int originalWidth, int originalHeight, std::int64_t elapsedMs) const;

    mutable std::mutex m_mutex;
    cv::dnn::Net m_net;
    AlgorithmParams m_params;
    bool m_initialized = false;
};
} // namespace HMVision
