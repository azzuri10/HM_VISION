#pragma once

#include "Algorithm/IVisionAlgorithm.h"

#include <memory>
#include <mutex>
#include <string>

#if __has_include(<paddleocr.h>)
#define HMVISION_PADDLEOCR_AVAILABLE 1
#include <paddleocr.h>
#elif __has_include(<PaddleOCR/paddleocr.h>)
#define HMVISION_PADDLEOCR_AVAILABLE 1
#include <PaddleOCR/paddleocr.h>
#else
#define HMVISION_PADDLEOCR_AVAILABLE 0
class PaddleOCR;
#endif

namespace cv
{
class Mat;
}

namespace HMVision
{
class OCRProcessor : public IVisionAlgorithm
{
public:
    OCRProcessor();
    ~OCRProcessor() override;

    std::string name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const cv::Mat& frame) override;
    void release() override;
    bool isInitialized() const override;

private:
    bool initializeEngine(const std::string& modelDir);
    void processMat(const cv::Mat& input, AlgorithmResult& output);

    static bool deviceEqualsCuda(const std::string& device);

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_useGpu = false;
    std::string m_modelDir;
    AlgorithmParams m_params;

    std::shared_ptr<PaddleOCR> m_paddleOCR;
};
} // namespace HMVision
