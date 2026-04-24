#pragma once

#include "Algorithm/AlgorithmFactory.h"
#include "Algorithm/ThreadPool.h"
#include "Common/AlgorithmParams.h"
#include "Common/AlgorithmResult.h"

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>

namespace cv
{
class Mat;
}

namespace HMVision
{
struct SchedulerStatus
{
    std::size_t queueSize = 0;
    std::size_t maxQueueSize = 0;
    std::size_t inFlightTasks = 0;
    std::size_t processedTasks = 0;
    std::size_t droppedTasks = 0;
    double avgExecutionMs = 0.0;
    double maxExecutionMs = 0.0;
    std::size_t queuedImageBytes = 0;
};

class AlgorithmScheduler
{
public:
    explicit AlgorithmScheduler(std::size_t threadCount = 4);
    ~AlgorithmScheduler();

    AlgorithmScheduler(const AlgorithmScheduler&) = delete;
    AlgorithmScheduler& operator=(const AlgorithmScheduler&) = delete;

    std::future<AlgorithmResult> submitTask(
        const std::string& cameraId, const cv::Mat& image, AlgorithmType type, int priority = 0);

    void setMaxQueueSize(std::size_t size);
    SchedulerStatus getStatus() const;
    void setAlgorithmParams(AlgorithmType type, const AlgorithmParams& params);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    void dispatchLoop();
};
} // namespace HMVision
