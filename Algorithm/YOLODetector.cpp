#include "YOLODetector.h"

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <QElapsedTimer>
#include <QFileInfo>

#include <algorithm>
#include <fstream>
#include <memory>

#if HMVISION_TRT_AVAILABLE
#include <NvInferRuntime.h>
#endif

namespace HMVision
{
#if HMVISION_TRT_AVAILABLE
namespace
{
class TrtLogger : public nvinfer1::ILogger
{
public:
    void log(Severity severity, const char* msg) noexcept override
    {
        if (severity <= Severity::kWARNING && msg != nullptr)
        {
            (void)msg;
        }
    }
};

TrtLogger g_trtLogger;
} // namespace
#endif

YOLODetector::YOLODetector() = default;

YOLODetector::~YOLODetector()
{
    release();
}

QString YOLODetector::name() const
{
    return m_variant == ModelVariant::YOLOv26 ? "YOLOv26 TensorRT" : "YOLOv8 TensorRT";
}

bool YOLODetector::initialize(const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    release();
    m_params = params;

    const QString lowerPath = params.modelPath.toLower();
    m_variant = lowerPath.contains("v26") ? ModelVariant::YOLOv26 : ModelVariant::YOLOv8;

#if !HMVISION_TRT_AVAILABLE
    m_initialized = false;
    return false;
#else
    const QFileInfo info(params.modelPath);
    if (!info.exists())
    {
        return false;
    }

    m_initialized = loadEngine(params.modelPath.toStdString());
    return m_initialized;
#endif
}

AlgorithmResult YOLODetector::process(const QImage& frame)
{
    AlgorithmResult output;
    if (frame.isNull())
    {
        output.message = "Input frame is empty.";
        return output;
    }

    cv::Mat input(frame.height(), frame.width(), CV_8UC4, const_cast<uchar*>(frame.bits()), frame.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(input, bgr, cv::COLOR_RGBA2BGR);
    process(bgr, output);
    return output;
}

void YOLODetector::process(const cv::Mat& input, AlgorithmResult& output)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    output.algorithmName = name();

    if (!m_initialized || input.empty())
    {
        output.success = false;
        output.message = "Detector is not initialized or input is empty.";
        return;
    }

    QElapsedTimer timer;
    timer.start();

#if HMVISION_TRT_AVAILABLE
    preprocess(input, static_cast<float*>(m_buffers[m_inputIndex]));
    inference(static_cast<float*>(m_buffers[m_inputIndex]), static_cast<float*>(m_buffers[m_outputIndex]));
    postprocess(
        static_cast<float*>(m_buffers[m_outputIndex]), output, input.cols, input.rows);
    output.elapsedMs = timer.elapsed();
    output.success = true;
#else
    (void)timer;
    output.success = false;
    output.message = "TensorRT/CUDA is unavailable in current build environment.";
#endif
}

void YOLODetector::release()
{
#if HMVISION_TRT_AVAILABLE
    for (void* buffer : m_buffers)
    {
        if (buffer != nullptr)
        {
            cudaFree(buffer);
        }
    }
    m_buffers.clear();
    m_hostOutput.clear();

    if (m_stream != nullptr)
    {
        cudaStreamDestroy(m_stream);
        m_stream = nullptr;
    }
    if (m_context != nullptr)
    {
        m_context->destroy();
        m_context = nullptr;
    }
    if (m_engine != nullptr)
    {
        m_engine->destroy();
        m_engine = nullptr;
    }
    if (m_runtime != nullptr)
    {
        m_runtime->destroy();
        m_runtime = nullptr;
    }
#endif
    m_initialized = false;
}

bool YOLODetector::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

bool YOLODetector::loadEngine(const std::string& enginePath)
{
#if !HMVISION_TRT_AVAILABLE
    (void)enginePath;
    return false;
#else
    std::ifstream file(enginePath, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    file.seekg(0, std::ifstream::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ifstream::beg);
    if (size <= 0)
    {
        return false;
    }

    std::vector<char> engineData(static_cast<std::size_t>(size));
    if (!file.read(engineData.data(), size))
    {
        return false;
    }

    m_runtime = nvinfer1::createInferRuntime(g_trtLogger);
    if (m_runtime == nullptr)
    {
        return false;
    }
    m_engine = m_runtime->deserializeCudaEngine(engineData.data(), engineData.size());
    if (m_engine == nullptr)
    {
        return false;
    }
    m_context = m_engine->createExecutionContext();
    if (m_context == nullptr)
    {
        return false;
    }

    m_inputIndex = m_engine->getBindingIndex("images");
    m_outputIndex = m_engine->getBindingIndex("output0");
    if (m_inputIndex < 0)
    {
        m_inputIndex = 0;
    }
    if (m_outputIndex < 0)
    {
        m_outputIndex = 1;
    }

    const int bindingCount = m_engine->getNbBindings();
    m_buffers.assign(static_cast<std::size_t>(bindingCount), nullptr);

    auto inputDims = m_engine->getBindingDimensions(m_inputIndex);
    if (inputDims.d[0] < 0)
    {
        inputDims.d[0] = 1;
        inputDims.d[1] = 3;
        inputDims.d[2] = m_params.inputHeight;
        inputDims.d[3] = m_params.inputWidth;
        m_context->setBindingDimensions(m_inputIndex, inputDims);
    }

    m_inputElements = 1;
    for (int i = 0; i < inputDims.nbDims; ++i)
    {
        m_inputElements *= std::max(1, inputDims.d[i]);
    }

    auto outputDims = m_context->getBindingDimensions(m_outputIndex);
    m_outputElements = 1;
    for (int i = 0; i < outputDims.nbDims; ++i)
    {
        m_outputElements *= std::max(1, outputDims.d[i]);
    }

    if (cudaStreamCreate(&m_stream) != cudaSuccess)
    {
        return false;
    }
    if (cudaMalloc(&m_buffers[m_inputIndex], static_cast<std::size_t>(m_inputElements) * sizeof(float))
        != cudaSuccess)
    {
        return false;
    }
    if (cudaMalloc(&m_buffers[m_outputIndex], static_cast<std::size_t>(m_outputElements) * sizeof(float))
        != cudaSuccess)
    {
        return false;
    }
    m_hostOutput.resize(static_cast<std::size_t>(m_outputElements), 0.0F);
    return true;
#endif
}

void YOLODetector::preprocess(const cv::Mat& image, float* gpuInput)
{
#if HMVISION_TRT_AVAILABLE
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(m_params.inputWidth, m_params.inputHeight));
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    std::vector<float> chw(static_cast<std::size_t>(m_inputElements), 0.0F);
    const int imageArea = m_params.inputWidth * m_params.inputHeight;
    for (int y = 0; y < m_params.inputHeight; ++y)
    {
        for (int x = 0; x < m_params.inputWidth; ++x)
        {
            const cv::Vec3f pixel = rgb.at<cv::Vec3f>(y, x);
            const int index = y * m_params.inputWidth + x;
            chw[static_cast<std::size_t>(index)] = pixel[0];
            chw[static_cast<std::size_t>(imageArea + index)] = pixel[1];
            chw[static_cast<std::size_t>(2 * imageArea + index)] = pixel[2];
        }
    }

    cudaMemcpyAsync(
        gpuInput,
        chw.data(),
        static_cast<std::size_t>(m_inputElements) * sizeof(float),
        cudaMemcpyHostToDevice,
        m_stream);
#else
    (void)image;
    (void)gpuInput;
#endif
}

void YOLODetector::inference(float* gpuInput, float* gpuOutput)
{
#if HMVISION_TRT_AVAILABLE
    (void)gpuInput;
    (void)gpuOutput;
    m_context->enqueueV2(m_buffers.data(), m_stream, nullptr);
    cudaMemcpyAsync(
        m_hostOutput.data(),
        m_buffers[m_outputIndex],
        static_cast<std::size_t>(m_outputElements) * sizeof(float),
        cudaMemcpyDeviceToHost,
        m_stream);
    cudaStreamSynchronize(m_stream);
#else
    (void)gpuInput;
    (void)gpuOutput;
#endif
}

void YOLODetector::postprocess(
    float* gpuOutput, AlgorithmResult& result, int originalWidth, int originalHeight)
{
    (void)gpuOutput;
    result.detections.clear();

    if (m_hostOutput.empty())
    {
        result.message = "Empty output.";
        return;
    }

    // Compatible with common YOLO export shape: [num_boxes, 4 + num_classes]
    const int stride = m_params.labels.isEmpty() ? 84 : (4 + m_params.labels.size());
    const int numBoxes = stride > 0 ? m_outputElements / stride : 0;

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    for (int i = 0; i < numBoxes; ++i)
    {
        const float* row = &m_hostOutput[static_cast<std::size_t>(i * stride)];
        if (stride < 6)
        {
            break;
        }

        int bestClass = -1;
        float bestScore = 0.0F;
        for (int c = 4; c < stride; ++c)
        {
            if (row[c] > bestScore)
            {
                bestScore = row[c];
                bestClass = c - 4;
            }
        }
        if (bestScore < m_params.confidenceThreshold)
        {
            continue;
        }

        const float x = row[0];
        const float y = row[1];
        const float w = row[2];
        const float h = row[3];
        const float sx = static_cast<float>(originalWidth) / static_cast<float>(m_params.inputWidth);
        const float sy = static_cast<float>(originalHeight) / static_cast<float>(m_params.inputHeight);

        const int left = static_cast<int>((x - 0.5F * w) * sx);
        const int top = static_cast<int>((y - 0.5F * h) * sy);
        const int bw = static_cast<int>(w * sx);
        const int bh = static_cast<int>(h * sy);

        boxes.emplace_back(left, top, bw, bh);
        confidences.push_back(bestScore);
        classIds.push_back(bestClass);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, m_params.confidenceThreshold, m_params.nmsThreshold, indices);

    for (int idx : indices)
    {
        DetectionBox det;
        const auto& box = boxes[static_cast<std::size_t>(idx)];
        det.box = QRectF(box.x, box.y, box.width, box.height);
        det.confidence = confidences[static_cast<std::size_t>(idx)];
        det.classId = classIds[static_cast<std::size_t>(idx)];
        if (det.classId >= 0 && det.classId < m_params.labels.size())
        {
            det.className = m_params.labels[det.classId];
        }
        else
        {
            det.className = QString("class_%1").arg(det.classId);
        }
        result.detections.push_back(det);
    }
    result.success = true;
    result.message = QString("Detections: %1").arg(result.detections.size());
}

QImage YOLODetector::matToQImage(const cv::Mat& image)
{
    if (image.empty())
    {
        return {};
    }
    cv::Mat rgb;
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888)
        .copy();
}
} // namespace HMVision
