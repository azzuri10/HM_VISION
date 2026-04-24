#pragma once

#include "AlgorithmFactory.h"
#include "ThreadPool.h"
#include "../Common/AlgorithmParams.h"
#include "../Common/AlgorithmResult.h"

#include <opencv2/core.hpp>

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

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

    std::future<AlgorithmResult> submitTask(
        const std::string& cameraId, const cv::Mat& image, AlgorithmType type, int priority = 0);

    void setMaxQueueSize(std::size_t size);
    SchedulerStatus getStatus() const;
    void setAlgorithmParams(AlgorithmType type, const AlgorithmParams& params);

private:
    struct AlgorithmTask
    {
        std::uint64_t sequence = 0;
        std::string cameraId;
        cv::Mat image;
        AlgorithmType algoType = AlgorithmType::Detection;
        int priority = 0;
        std::promise<AlgorithmResult> promise;
        std::size_t estimatedBytes = 0;
    };

    using TaskPtr = std::shared_ptr<AlgorithmTask>;

    struct TaskComparator
    {
        bool operator()(const TaskPtr& lhs, const TaskPtr& rhs) const
        {
            if (lhs->priority == rhs->priority)
            {
                return lhs->sequence > rhs->sequence;
            }
            return lhs->priority < rhs->priority;
        }
    };

    void dispatchLoop();
    AlgorithmResult runTask(const AlgorithmTask& task);
    static QImage matToImage(const cv::Mat& image);

    ThreadPool m_threadPool;
    std::priority_queue<TaskPtr, std::vector<TaskPtr>, TaskComparator> m_taskQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCond;
    std::condition_variable m_dispatchCond;
    std::thread m_dispatchThread;
    std::atomic<bool> m_running{false};

    std::size_t m_maxQueueSize = 200;
    std::size_t m_threadCount = 4;
    std::size_t m_inFlightTasks = 0;
    std::size_t m_processedTasks = 0;
    std::size_t m_droppedTasks = 0;
    double m_avgExecutionMs = 0.0;
    double m_maxExecutionMs = 0.0;
    std::size_t m_queuedImageBytes = 0;
    std::uint64_t m_sequenceCounter = 0;

    std::unordered_map<AlgorithmType, AlgorithmParams> m_paramsByType;
};
} // namespace HMVision
