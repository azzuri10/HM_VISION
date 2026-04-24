#pragma once

#include "Algorithm/IVisionAlgorithm.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

namespace HMVision
{
class DetectionAlgorithm : public IVisionAlgorithm
{
public:
    DetectionAlgorithm();
    ~DetectionAlgorithm() override;

    std::string name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const cv::Mat& frame) override;
    void release() override;
    bool isInitialized() const override;

private:
    static std::vector<std::string> loadLabelsFromFile(const std::string& path);
    static bool deviceEqualsCuda(const std::string& device);

    AlgorithmResult postProcess(
        const cv::Mat& output, int originalWidth, int originalHeight, std::int64_t elapsedMs) const;

    mutable std::mutex m_mutex;
    cv::dnn::Net m_net;
    AlgorithmParams m_params;
    bool m_initialized = false;
};
} // namespace HMVision
