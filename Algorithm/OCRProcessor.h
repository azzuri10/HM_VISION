#pragma once

#include "IVisionAlgorithm.h"

#include <opencv2/core.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

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

namespace HMVision
{
class OCRProcessor : public IVisionAlgorithm
{
public:
    OCRProcessor();
    ~OCRProcessor() override;

    QString name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const QImage& frame) override;
    void release() override;
    bool isInitialized() const override;

    bool initialize(const std::string& modelDir);
    void process(const cv::Mat& input, AlgorithmResult& output);

private:
    struct OCRResult
    {
        std::vector<cv::Rect> boxes;
        std::vector<std::string> texts;
        std::vector<float> scores;
    };

    OCRResult detectAndRecognize(const cv::Mat& image);
    static cv::Mat qImageToMat(const QImage& image);

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_useGpu = false;
    std::string m_modelDir;
    AlgorithmParams m_params;

    std::shared_ptr<PaddleOCR> m_paddleOCR;
};
} // namespace HMVision
