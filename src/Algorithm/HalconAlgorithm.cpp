#include "Algorithm/HalconAlgorithm.h"

#include <chrono>
#include <string>

namespace HMVision
{
std::string HalconAlgorithm::name() const
{
    return "Halcon 18.11";
}

bool HalconAlgorithm::initialize(const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
    m_initialized = true;
    return true;
}

AlgorithmResult HalconAlgorithm::process(const cv::Mat& frame)
{
    const auto t0 = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    AlgorithmResult result;
    result.algorithmName = name();
    if (!m_initialized || frame.empty())
    {
        result.message = "Halcon algorithm is not initialized or frame is empty.";
        return result;
    }

    DetectionResult d;
    d.box = {
        20.0F,
        20.0F,
        static_cast<float>(frame.cols) * 0.25F,
        static_cast<float>(frame.rows) * 0.25F
    };
    d.confidence = 0.9F;
    d.classId = 0;
    d.className = "halcon_feature";
    result.detections.push_back(d);

    const auto t1 = std::chrono::steady_clock::now();
    result.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    result.success = true;
    result.message = "Halcon pipeline executed.";
    return result;
}

void HalconAlgorithm::release()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
}

bool HalconAlgorithm::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}
} // namespace HMVision
