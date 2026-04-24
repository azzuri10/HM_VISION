#pragma once

#include "IVisionAlgorithm.h"

#include <opencv2/core.hpp>

#include <mutex>
#include <string>
#include <vector>

#if __has_include(<NvInfer.h>) && __has_include(<cuda_runtime.h>)
#define HMVISION_TRT_AVAILABLE 1
#include <NvInfer.h>
#include <cuda_runtime.h>
#else
#define HMVISION_TRT_AVAILABLE 0
namespace nvinfer1
{
class ICudaEngine;
class IExecutionContext;
class IRuntime;
} // namespace nvinfer1
using cudaStream_t = void*;
#endif

namespace HMVision
{
class YOLODetector : public IVisionAlgorithm
{
public:
    YOLODetector();
    ~YOLODetector() override;

    QString name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const QImage& frame) override;
    void release() override;
    bool isInitialized() const override;

    void process(const cv::Mat& input, AlgorithmResult& output);

private:
    enum class ModelVariant
    {
        YOLOv8,
        YOLOv26
    };

    bool loadEngine(const std::string& enginePath);
    void preprocess(const cv::Mat& image, float* gpuInput);
    void inference(float* gpuInput, float* gpuOutput);
    void postprocess(float* gpuOutput, AlgorithmResult& result, int originalWidth, int originalHeight);
    static QImage matToQImage(const cv::Mat& image);

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    AlgorithmParams m_params;
    ModelVariant m_variant = ModelVariant::YOLOv8;

    nvinfer1::IRuntime* m_runtime = nullptr;
    nvinfer1::ICudaEngine* m_engine = nullptr;
    nvinfer1::IExecutionContext* m_context = nullptr;
    cudaStream_t m_stream = nullptr;
    std::vector<void*> m_buffers;
    std::vector<float> m_hostOutput;
    int m_inputIndex = -1;
    int m_outputIndex = -1;
    int m_inputElements = 0;
    int m_outputElements = 0;
};
} // namespace HMVision
