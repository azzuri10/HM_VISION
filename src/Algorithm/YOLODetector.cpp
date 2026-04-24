#include "../../include/Algorithm/YOLODetector.h"

#include <opencv2/dnn.hpp>

#include <chrono>

namespace HMVision
{
std::string YOLODetector::name() const
{
    return "YOLODetector";
}

bool YOLODetector::initialize(const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (params.modelPath.empty())
    {
        m_initialized = false;
        return false;
    }

    try
    {
        m_net = cv::dnn::readNetFromONNX(params.modelPath);
        if (params.device == "cuda")
        {
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        }
        else
        {
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }
    }
    catch (...)
    {
        m_initialized = false;
        return false;
    }

    m_params = params;
    m_initialized = true;
    return true;
}

AlgorithmResult YOLODetector::process(const cv::Mat& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    AlgorithmResult result;
    result.algorithmName = name();

    if (!m_initialized || frame.empty())
    {
        result.success = false;
        result.message = "YOLODetector not initialized or empty frame.";
        return result;
    }

    const auto start = std::chrono::steady_clock::now();
    cv::Mat blob = cv::dnn::blobFromImage(
        frame,
        1.0 / 255.0,
        cv::Size(m_params.inputWidth, m_params.inputHeight),
        cv::Scalar(),
        true,
        false);

    m_net.setInput(blob);
    cv::Mat output = m_net.forward();

    const auto end = std::chrono::steady_clock::now();
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return postprocess(output, frame.cols, frame.rows, elapsedMs);
}

void YOLODetector::release()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_net = cv::dnn::Net();
    m_initialized = false;
}

bool YOLODetector::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

AlgorithmResult YOLODetector::postprocess(
    const cv::Mat& output, int originalWidth, int originalHeight, std::int64_t elapsedMs) const
{
    AlgorithmResult result;
    result.algorithmName = name();
    result.elapsedMs = elapsedMs;

    if (output.empty())
    {
        result.success = false;
        result.message = "Model output is empty.";
        return result;
    }

    cv::Mat prediction;
    if (output.dims == 3)
    {
        const int channels = output.size[1];
        const int boxes = output.size[2];
        cv::Mat reshaped(channels, boxes, CV_32F, const_cast<float*>(output.ptr<float>()));
        cv::transpose(reshaped, prediction); // boxes x channels
    }
    else if (output.dims == 2)
    {
        prediction = output;
    }
    else
    {
        result.success = false;
        result.message = "Unsupported YOLO output shape.";
        return result;
    }

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    const float xScale = static_cast<float>(originalWidth) / static_cast<float>(m_params.inputWidth);
    const float yScale = static_cast<float>(originalHeight) / static_cast<float>(m_params.inputHeight);

    for (int i = 0; i < prediction.rows; ++i)
    {
        if (prediction.cols < 6)
        {
            continue;
        }

        const float* row = prediction.ptr<float>(i);
        cv::Mat scores(1, prediction.cols - 4, CV_32F, const_cast<float*>(row + 4));

        cv::Point classIdPoint;
        double maxClassScore = 0.0;
        cv::minMaxLoc(scores, nullptr, &maxClassScore, nullptr, &classIdPoint);
        const float confidence = static_cast<float>(maxClassScore);

        if (confidence < m_params.confidenceThreshold)
        {
            continue;
        }

        const float cx = row[0];
        const float cy = row[1];
        const float w = row[2];
        const float h = row[3];

        const int left = static_cast<int>((cx - 0.5F * w) * xScale);
        const int top = static_cast<int>((cy - 0.5F * h) * yScale);
        const int width = static_cast<int>(w * xScale);
        const int height = static_cast<int>(h * yScale);

        boxes.emplace_back(left, top, width, height);
        confidences.push_back(confidence);
        classIds.push_back(classIdPoint.x);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(
        boxes, confidences, m_params.confidenceThreshold, m_params.nmsThreshold, indices);

    for (int idx : indices)
    {
        DetectionResult detection;
        const cv::Rect& rect = boxes[static_cast<std::size_t>(idx)];
        detection.box.x = static_cast<float>(rect.x);
        detection.box.y = static_cast<float>(rect.y);
        detection.box.width = static_cast<float>(rect.width);
        detection.box.height = static_cast<float>(rect.height);
        detection.confidence = confidences[static_cast<std::size_t>(idx)];
        detection.classId = classIds[static_cast<std::size_t>(idx)];
        if (detection.classId >= 0
            && detection.classId < static_cast<int>(m_params.labels.size()))
        {
            detection.className = m_params.labels[static_cast<std::size_t>(detection.classId)];
        }
        else
        {
            detection.className = "class_" + std::to_string(detection.classId);
        }

        result.detections.push_back(std::move(detection));
    }

    result.success = true;
    result.message = "Detection complete.";
    return result;
}
} // namespace HMVision
