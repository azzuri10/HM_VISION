#include "HalconAlgorithm.h"

#include <QElapsedTimer>

namespace HMVision
{
QString HalconAlgorithm::name() const
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

AlgorithmResult HalconAlgorithm::process(const QImage& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    AlgorithmResult result;
    result.algorithmName = name();
    if (!m_initialized || frame.isNull())
    {
        result.message = "Halcon algorithm is not initialized or frame is empty.";
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    // Placeholder for Halcon operators/pipelines.
    DetectionBox box;
    box.box = QRectF(20, 20, frame.width() / 4.0, frame.height() / 4.0);
    box.confidence = 0.9F;
    box.classId = 0;
    box.className = "halcon_feature";
    result.detections.push_back(box);

    result.success = true;
    result.elapsedMs = timer.elapsed();
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

cv::Mat HalconAlgorithm::qImageToMat(const QImage& image)
{
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    return cv::Mat(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar*>(rgb.bits()), rgb.bytesPerLine())
        .clone();
}
} // namespace HMVision
