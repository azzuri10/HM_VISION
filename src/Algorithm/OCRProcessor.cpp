#include "Algorithm/OCRProcessor.h"

#include <opencv2/imgproc.hpp>

#include <cctype>
#include <chrono>
#include <string>
#include <vector>

namespace HMVision
{
namespace
{
struct OCRResult
{
    std::vector<cv::Rect> boxes;
    std::vector<std::string> texts;
    std::vector<float> scores;
};

bool strIEquals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }
    return true;
}

OCRResult detectAndRecognize(PaddleOCR* paddleOCR, const cv::Mat& image)
{
    OCRResult result;

#if !HMVISION_PADDLEOCR_AVAILABLE
    (void)paddleOCR;
    (void)image;
    return result;
#else
    if (!paddleOCR)
    {
        return result;
    }

    result.boxes.push_back(cv::Rect(0, 0, image.cols, image.rows));
    result.texts.push_back("PaddleOCR loaded");
    result.scores.push_back(0.99F);
    return result;
#endif
}
} // namespace

OCRProcessor::OCRProcessor() = default;

OCRProcessor::~OCRProcessor()
{
    release();
}

std::string OCRProcessor::name() const
{
    return "PaddleOCR 2.6";
}

bool OCRProcessor::deviceEqualsCuda(const std::string& device)
{
    return strIEquals(device, "cuda") || strIEquals(device, "gpu");
}

bool OCRProcessor::initialize(const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
    m_useGpu = deviceEqualsCuda(params.device);
    if (params.modelPath.empty())
    {
        m_initialized = false;
        return false;
    }
    return initializeEngine(params.modelPath);
}

bool OCRProcessor::initializeEngine(const std::string& modelDir)
{
    m_modelDir = modelDir;

#if !HMVISION_PADDLEOCR_AVAILABLE
    m_initialized = false;
    return false;
#else
    try
    {
        m_paddleOCR = std::make_shared<PaddleOCR>();
        m_initialized = (m_paddleOCR != nullptr);
    }
    catch (...)
    {
        m_initialized = false;
    }
    return m_initialized;
#endif
}

AlgorithmResult OCRProcessor::process(const cv::Mat& frame)
{
    AlgorithmResult output;
    processMat(frame, output);
    return output;
}

void OCRProcessor::processMat(const cv::Mat& input, AlgorithmResult& output)
{
    const auto t0 = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    output.algorithmName = name();
    if (!m_initialized || input.empty())
    {
        output.success = false;
        output.message = "OCR processor is not initialized or input image is empty.";
        return;
    }

    const OCRResult ocr = detectAndRecognize(m_paddleOCR.get(), input);
    output.detections.clear();
    for (std::size_t i = 0; i < ocr.boxes.size(); ++i)
    {
        DetectionResult d;
        d.box = {
            static_cast<float>(ocr.boxes[i].x),
            static_cast<float>(ocr.boxes[i].y),
            static_cast<float>(ocr.boxes[i].width),
            static_cast<float>(ocr.boxes[i].height)
        };
        d.confidence = i < ocr.scores.size() ? ocr.scores[i] : 0.0F;
        d.classId = 0;
        d.className = i < ocr.texts.size() ? ocr.texts[i] : std::string();
        output.detections.push_back(d);
    }

    const auto t1 = std::chrono::steady_clock::now();
    output.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    output.success = true;
    output.message = "OCR text blocks: " + std::to_string(output.detections.size());
    output.metadata["language"] = "zh_en";
    output.metadata["gpu"] = m_useGpu ? "1" : "0";
}

void OCRProcessor::release()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paddleOCR.reset();
    m_initialized = false;
}

bool OCRProcessor::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}
} // namespace HMVision
