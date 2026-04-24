#include "Algorithm/AlgorithmScheduler.h"

#include "Algorithm/AlgorithmFactory.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace HMVision
{
struct AlgorithmScheduler::Impl
{
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

    explicit Impl(std::size_t threadCount)
        : m_threadPool(threadCount == 0 ? 1 : threadCount)
        , m_threadCount(threadCount == 0 ? 1 : threadCount)
    {
    }

    ThreadPool m_threadPool;
    std::priority_queue<TaskPtr, std::vector<TaskPtr>, TaskComparator> m_taskQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCond;
    std::condition_variable m_dispatchCond;
    std::thread m_dispatchThread;
    std::atomic<bool> m_running{false};

    std::size_t m_maxQueueSize = 200;
    std::size_t m_threadCount = 1;
    std::size_t m_inFlightTasks = 0;
    std::size_t m_processedTasks = 0;
    std::size_t m_droppedTasks = 0;
    double m_avgExecutionMs = 0.0;
    double m_maxExecutionMs = 0.0;
    std::size_t m_queuedImageBytes = 0;
    std::uint64_t m_sequenceCounter = 0;

    std::unordered_map<AlgorithmType, AlgorithmParams> m_paramsByType;

    static AlgorithmResult runSingle(Impl& impl, const AlgorithmTask& task);
};

AlgorithmResult AlgorithmScheduler::Impl::runSingle(Impl& impl, const AlgorithmTask& task)
{
    const auto t0 = std::chrono::steady_clock::now();

    AlgorithmResult result;
    auto algorithm = AlgorithmFactory::getInstance().create(task.algoType);
    if (!algorithm)
    {
        result.success = false;
        result.message = "Unknown algorithm type.";
    }
    else
    {
        AlgorithmParams params;
        {
            std::lock_guard<std::mutex> lock(impl.m_queueMutex);
            const auto it = impl.m_paramsByType.find(task.algoType);
            if (it != impl.m_paramsByType.end())
            {
                params = it->second;
            }
        }

        if (!algorithm->initialize(params))
        {
            result.success = false;
            result.message = "Algorithm initialization failed.";
        }
        else
        {
            result = algorithm->process(task.image);
            algorithm->release();
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    {
        std::lock_guard<std::mutex> lock(impl.m_queueMutex);
        ++impl.m_processedTasks;
        if (impl.m_processedTasks == 1)
        {
            impl.m_avgExecutionMs = elapsedMs;
        }
        else
        {
            impl.m_avgExecutionMs = (impl.m_avgExecutionMs * static_cast<double>(impl.m_processedTasks - 1) + elapsedMs)
                / static_cast<double>(impl.m_processedTasks);
        }
        impl.m_maxExecutionMs = std::max(impl.m_maxExecutionMs, elapsedMs);
    }

    result.metadata["cameraId"] = task.cameraId;
    result.metadata["priority"] = std::to_string(task.priority);
    result.metadata["executionMs"] = std::to_string(static_cast<int>(elapsedMs + 0.5));
    return result;
}

AlgorithmScheduler::AlgorithmScheduler(std::size_t threadCount)
    : m_impl(std::make_unique<Impl>(threadCount == 0 ? 1 : threadCount))
{
    m_impl->m_running.store(true);
    m_impl->m_dispatchThread = std::thread(&AlgorithmScheduler::dispatchLoop, this);
}

AlgorithmScheduler::~AlgorithmScheduler()
{
    m_impl->m_running.store(false);
    m_impl->m_queueCond.notify_all();
    m_impl->m_dispatchCond.notify_all();
    if (m_impl->m_dispatchThread.joinable())
    {
        m_impl->m_dispatchThread.join();
    }
}

std::future<AlgorithmResult> AlgorithmScheduler::submitTask(
    const std::string& cameraId, const cv::Mat& image, AlgorithmType type, int priority)
{
    auto task = std::make_shared<Impl::AlgorithmTask>();
    task->cameraId = cameraId;
    task->image = image.clone();
    task->algoType = type;
    task->priority = priority;
    task->estimatedBytes = task->image.empty() ? 0 : task->image.total() * task->image.elemSize();
    std::future<AlgorithmResult> future = task->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(m_impl->m_queueMutex);
        if (m_impl->m_taskQueue.size() >= m_impl->m_maxQueueSize)
        {
            ++m_impl->m_droppedTasks;
            AlgorithmResult result;
            result.success = false;
            result.message = "Task queue is full. Task dropped.";
            task->promise.set_value(std::move(result));
            return future;
        }

        task->sequence = ++m_impl->m_sequenceCounter;
        m_impl->m_queuedImageBytes += task->estimatedBytes;
        m_impl->m_taskQueue.push(std::move(task));
    }
    m_impl->m_queueCond.notify_one();
    return future;
}

void AlgorithmScheduler::setMaxQueueSize(std::size_t size)
{
    std::lock_guard<std::mutex> lock(m_impl->m_queueMutex);
    m_impl->m_maxQueueSize = size > 0 ? size : 1;
}

SchedulerStatus AlgorithmScheduler::getStatus() const
{
    std::lock_guard<std::mutex> lock(m_impl->m_queueMutex);
    SchedulerStatus status;
    status.queueSize = m_impl->m_taskQueue.size();
    status.maxQueueSize = m_impl->m_maxQueueSize;
    status.inFlightTasks = m_impl->m_inFlightTasks;
    status.processedTasks = m_impl->m_processedTasks;
    status.droppedTasks = m_impl->m_droppedTasks;
    status.avgExecutionMs = m_impl->m_avgExecutionMs;
    status.maxExecutionMs = m_impl->m_maxExecutionMs;
    status.queuedImageBytes = m_impl->m_queuedImageBytes;
    return status;
}

void AlgorithmScheduler::setAlgorithmParams(AlgorithmType type, const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_impl->m_queueMutex);
    m_impl->m_paramsByType[type] = params;
}

void AlgorithmScheduler::dispatchLoop()
{
    while (m_impl->m_running.load())
    {
        Impl::TaskPtr task;
        {
            std::unique_lock<std::mutex> lock(m_impl->m_queueMutex);
            m_impl->m_queueCond.wait(lock, [this]() {
                return !m_impl->m_running.load() || !m_impl->m_taskQueue.empty();
            });

            if (!m_impl->m_running.load() && m_impl->m_taskQueue.empty())
            {
                return;
            }

            m_impl->m_dispatchCond.wait(lock, [this]() {
                return !m_impl->m_running.load() || m_impl->m_inFlightTasks < m_impl->m_threadCount;
            });

            if (!m_impl->m_running.load())
            {
                return;
            }

            task = m_impl->m_taskQueue.top();
            m_impl->m_taskQueue.pop();
            m_impl->m_queuedImageBytes -= std::min(m_impl->m_queuedImageBytes, task->estimatedBytes);
            ++m_impl->m_inFlightTasks;
        }

        m_impl->m_threadPool.submit([this, task = std::move(task)]() mutable {
            AlgorithmResult result = Impl::runSingle(*m_impl, *task);
            task->promise.set_value(std::move(result));

            std::lock_guard<std::mutex> lock(m_impl->m_queueMutex);
            if (m_impl->m_inFlightTasks > 0)
            {
                --m_impl->m_inFlightTasks;
            }
            m_impl->m_dispatchCond.notify_one();
        });
    }
}
} // namespace HMVision
