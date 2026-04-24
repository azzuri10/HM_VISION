#include "DetectionAlgorithm.h"

#include <opencv2/imgproc.hpp>

#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>

#include <vector>

namespace HMVision
{
QString DetectionAlgorithm::name() const
{
    return "YOLOv8 Detection";
}

bool DetectionAlgorithm::initialize(const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (params.modelPath.isEmpty())
    {
        return false;
    }

    try
    {
        m_net = cv::dnn::readNetFromONNX(params.modelPath.toStdString());
        if (params.device.compare("cuda", Qt::CaseInsensitive) == 0)
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
        if (m_params.labels.isEmpty() && !m_params.classesPath.isEmpty())
        {
            m_params.labels = loadLabelsFromFile(m_params.classesPath);
        }
        m_initialized = true;
        return true;
    }
    catch (const cv::Exception&)
    {
        m_initialized = false;
        return false;
    }
}

AlgorithmResult DetectionAlgorithm::process(const QImage& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    AlgorithmResult result;
    result.algorithmName = name();
    if (!m_initialized || frame.isNull())
    {
        result.message = "Algorithm not initialized or frame is empty.";
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    cv::Mat input = qImageToMat(frame);
    const int originalWidth = input.cols;
    const int originalHeight = input.rows;

    cv::Mat blob = cv::dnn::blobFromImage(
        input,
        1.0 / 255.0,
        cv::Size(m_params.inputWidth, m_params.inputHeight),
        cv::Scalar(),
        true,
        false);

    m_net.setInput(blob);
    cv::Mat output = m_net.forward();
    result = postProcess(output, originalWidth, originalHeight, timer.elapsed());
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

cv::Mat DetectionAlgorithm::qImageToMat(const QImage& image)
{
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar*>(rgb.bits()), rgb.bytesPerLine());
    return mat.clone();
}

QStringList DetectionAlgorithm::loadLabelsFromFile(const QString& path)
{
    QStringList labels;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return labels;
    }

    QTextStream in(&file);
    while (!in.atEnd())
    {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty())
        {
            labels.push_back(line);
        }
    }
    return labels;
}

AlgorithmResult DetectionAlgorithm::postProcess(
    const cv::Mat& output, int originalWidth, int originalHeight, qint64 elapsedMs) const
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
        cv::transpose(squeezed, prediction); // numBoxes x channels
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
        DetectionBox detection;
        detection.box = QRectF(rect.x, rect.y, rect.width, rect.height);
        detection.confidence = confidences[index];
        detection.classId = classIds[index];
        if (detection.classId >= 0 && detection.classId < m_params.labels.size())
        {
            detection.className = m_params.labels[detection.classId];
        }
        else
        {
            detection.className = QString("class_%1").arg(detection.classId);
        }
        result.detections.push_back(detection);
    }

    result.success = true;
    result.message = QString("Detections: %1").arg(result.detections.size());
    return result;
}
} // namespace HMVision
