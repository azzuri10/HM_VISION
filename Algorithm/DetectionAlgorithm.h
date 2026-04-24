#pragma once

#include "IVisionAlgorithm.h"

#include <opencv2/dnn.hpp>

#include <mutex>

namespace HMVision
{
class DetectionAlgorithm : public IVisionAlgorithm
{
public:
    QString name() const override;
    bool initialize(const AlgorithmParams& params) override;
    AlgorithmResult process(const QImage& frame) override;
    void release() override;
    bool isInitialized() const override;

private:
    static cv::Mat qImageToMat(const QImage& image);
    static QStringList loadLabelsFromFile(const QString& path);
    AlgorithmResult postProcess(
        const cv::Mat& output, int originalWidth, int originalHeight, qint64 elapsedMs) const;

    mutable std::mutex m_mutex;
    cv::dnn::Net m_net;
    AlgorithmParams m_params;
    bool m_initialized = false;
};
} // namespace HMVision
