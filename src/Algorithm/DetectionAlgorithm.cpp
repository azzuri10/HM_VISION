#include "Algorithm/DetectionAlgorithm.h"

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace HMVision
{
namespace
{
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
} // namespace

DetectionAlgorithm::DetectionAlgorithm() = default;

DetectionAlgorithm::~DetectionAlgorithm() = default;

std::string DetectionAlgorithm::name() const
{
    return "YOLOv8 Detection";
}

bool DetectionAlgorithm::deviceEqualsCuda(const std::string& device)
{
    return strIEquals(device, "cuda") || strIEquals(device, "gpu");
}

bool DetectionAlgorithm::initialize(const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (params.modelPath.empty())
    {
        return false;
    }

    try
    {
        m_net = cv::dnn::readNetFromONNX(params.modelPath);
        if (deviceEqualsCuda(params.device))
        {
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        }
        else
        {
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }

        m_params = params;
        if (m_params.labels.empty() && !m_params.classesFile.empty())
        {
            m_params.labels = loadLabelsFromFile(m_params.classesFile);
        }
        m_initialized = true;
        return true;
    }
    catch (const cv::Exception&)
    {
        m_initialized = false;
        m_net = cv::dnn::Net();
        return false;
    }
}

AlgorithmResult DetectionAlgorithm::process(const cv::Mat& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto t0 = std::chrono::steady_clock::now();

    AlgorithmResult result;
    result.algorithmName = name();
    if (!m_initialized || frame.empty())
    {
        result.message = "Algorithm not initialized or frame is empty.";
        return result;
    }

    const int originalWidth = frame.cols;
    const int originalHeight = frame.rows;

    cv::Mat input = frame;
    if (input.channels() == 4)
    {
        cv::cvtColor(input, input, cv::COLOR_BGRA2BGR);
    }
    else if (input.channels() == 1)
    {
        cv::cvtColor(input, input, cv::COLOR_GRAY2BGR);
    }

    cv::Mat blob = cv::dnn::blobFromImage(
        input,
        1.0 / 255.0,
        cv::Size(m_params.inputWidth, m_params.inputHeight),
        cv::Scalar(),
        true,
        false);

    m_net.setInput(blob);
    cv::Mat output = m_net.forward();
    const auto t1 = std::chrono::steady_clock::now();
    const std::int64_t elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    result = postProcess(output, originalWidth, originalHeight, elapsed);
    result.algorithmName = name();
    return result;
}

void DetectionAlgorithm::release()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_net = cv::dnn::Net();
    m_initialized = false;
}

bool DetectionAlgorithm::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

std::vector<std::string> DetectionAlgorithm::loadLabelsFromFile(const std::string& path)
{
    std::vector<std::string> labels;
    std::ifstream f(path);
    if (!f)
    {
        return labels;
    }
    std::string line;
    while (std::getline(f, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            labels.push_back(line);
        }
    }
    return labels;
}

AlgorithmResult DetectionAlgorithm::postProcess(
    const cv::Mat& output, int originalWidth, int originalHeight, std::int64_t elapsedMs) const
{
    AlgorithmResult result;
    result.elapsedMs = elapsedMs;

    if (output.empty())
    {
        result.message = "Empty output tensor.";
        return result;
    }

    cv::Mat prediction;
    if (output.dims == 3)
    {
        const int channels = output.size[1];
        const int numBoxes = output.size[2];
        cv::Mat squeezed(channels, numBoxes, CV_32F, const_cast<float*>(output.ptr<float>()));
        cv::transpose(squeezed, prediction);
    }
    else if (output.dims == 2)
    {
        prediction = output;
    }
    else
    {
        result.message = "Unsupported output tensor shape.";
        return result;
    }

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    const float xScale = static_cast<float>(originalWidth) / static_cast<float>(m_params.inputWidth);
    const float yScale = static_cast<float>(originalHeight) / static_cast<float>(m_params.inputHeight);

    for (int i = 0; i < prediction.rows; ++i)
    {
        const float* row = prediction.ptr<float>(i);
        if (prediction.cols < 6)
        {
            continue;
        }
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
        const float width = row[2];
        const float height = row[3];

        const int left = static_cast<int>((cx - width * 0.5F) * xScale);
        const int top = static_cast<int>((cy - height * 0.5F) * yScale);
        const int boxWidth = static_cast<int>(width * xScale);
        const int boxHeight = static_cast<int>(height * yScale);

        boxes.emplace_back(left, top, boxWidth, boxHeight);
        confidences.push_back(confidence);
        classIds.push_back(classIdPoint.x);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, m_params.confidenceThreshold, m_params.nmsThreshold, indices);

    for (int index : indices)
    {
        const cv::Rect& rect = boxes[index];
        DetectionResult d;
        d.box = {
            static_cast<float>(rect.x),
            static_cast<float>(rect.y),
            static_cast<float>(rect.width),
            static_cast<float>(rect.height)
        };
        d.confidence = confidences[index];
        d.classId = classIds[index];
        if (d.classId >= 0
            && d.classId < static_cast<int>(m_params.labels.size()))
        {
            d.className = m_params.labels[static_cast<std::size_t>(d.classId)];
        }
        else
        {
            d.className = "class_" + std::to_string(d.classId);
        }
        result.detections.push_back(d);
    }

    result.success = true;
    {
        std::ostringstream oss;
        oss << "Detections: " << result.detections.size();
        result.message = oss.str();
    }
    return result;
}
} // namespace HMVision
