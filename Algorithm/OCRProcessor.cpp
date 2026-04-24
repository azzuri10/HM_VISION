#include "OCRProcessor.h"

#include <QElapsedTimer>

#include <opencv2/imgproc.hpp>

namespace HMVision
{
OCRProcessor::OCRProcessor() = default;

OCRProcessor::~OCRProcessor()
{
    release();
}

QString OCRProcessor::name() const
{
    return "PaddleOCR 2.6";
}

bool OCRProcessor::initialize(const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
    m_useGpu = params.device.compare("cuda", Qt::CaseInsensitive) == 0;
    if (params.modelPath.isEmpty())
    {
        m_initialized = false;
        return false;
    }
    return initialize(params.modelPath.toStdString());
}

bool OCRProcessor::initialize(const std::string& modelDir)
{
    m_modelDir = modelDir;

#if !HMVISION_PADDLEOCR_AVAILABLE
    m_initialized = false;
    return false;
#else
    // Expected layout for PaddleOCR 2.6:
    // modelDir/det, modelDir/rec, modelDir/cls (optional), modelDir/keys.txt
    // Actual constructor signature may vary by integration package.
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

AlgorithmResult OCRProcessor::process(const QImage& frame)
{
    AlgorithmResult output;
    if (frame.isNull())
    {
        output.success = false;
        output.message = "Input frame is empty.";
        output.algorithmName = name();
        return output;
    }

    process(qImageToMat(frame), output);
    return output;
}

void OCRProcessor::process(const cv::Mat& input, AlgorithmResult& output)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    output.algorithmName = name();
    if (!m_initialized || input.empty())
    {
        output.success = false;
        output.message = "OCR processor is not initialized or input image is empty.";
        return;
    }

    QElapsedTimer timer;
    timer.start();

    const OCRResult ocr = detectAndRecognize(input);
    output.detections.clear();
    for (std::size_t i = 0; i < ocr.boxes.size(); ++i)
    {
        DetectionBox box;
        box.box = QRectF(
            ocr.boxes[i].x, ocr.boxes[i].y, ocr.boxes[i].width, ocr.boxes[i].height);
        box.confidence = i < ocr.scores.size() ? ocr.scores[i] : 0.0F;
        box.classId = 0;
        box.className = i < ocr.texts.size() ? QString::fromStdString(ocr.texts[i]) : QString();
        output.detections.push_back(box);
    }

    output.elapsedMs = timer.elapsed();
    output.success = true;
    output.message = QString("OCR text blocks: %1").arg(output.detections.size());
    output.metadata.insert("language", "zh_en");
    output.metadata.insert("gpu", m_useGpu);
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

OCRProcessor::OCRResult OCRProcessor::detectAndRecognize(const cv::Mat& image)
{
    OCRResult result;

#if !HMVISION_PADDLEOCR_AVAILABLE
    (void)image;
    return result;
#else
    // Two-stage pipeline placeholder:
    // 1) Text detection (DB detector)
    // 2) Text recognition (CRNN/SVTR)
    //
    // Actual PaddleOCR integration APIs vary between releases and wrappers.
    // Keep this adapter boundary stable for real SDK call-in.
    if (!m_paddleOCR)
    {
        return result;
    }

    // Fallback demo behavior for integration stage:
    // produce one full-image region to keep downstream UI and pipeline active.
    result.boxes.push_back(cv::Rect(0, 0, image.cols, image.rows));
    result.texts.push_back("PaddleOCR loaded");
    result.scores.push_back(0.99F);
    return result;
#endif
}

cv::Mat OCRProcessor::qImageToMat(const QImage& image)
{
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar*>(rgb.bits()), rgb.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone();
}
} // namespace HMVision
