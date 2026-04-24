#include "AlgorithmScheduler.h"

#include <QElapsedTimer>

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace HMVision
{
AlgorithmScheduler::AlgorithmScheduler(std::size_t threadCount)
    : m_threadPool(threadCount == 0 ? 1 : threadCount)
    , m_threadCount(threadCount == 0 ? 1 : threadCount)
{
    m_running.store(true);
    m_dispatchThread = std::thread(&AlgorithmScheduler::dispatchLoop, this);
}

AlgorithmScheduler::~AlgorithmScheduler()
{
    m_running.store(false);
    m_queueCond.notify_all();
    m_dispatchCond.notify_all();
    if (m_dispatchThread.joinable())
    {
        m_dispatchThread.join();
    }
}

std::future<AlgorithmResult> AlgorithmScheduler::submitTask(
    const std::string& cameraId, const cv::Mat& image, AlgorithmType type, int priority)
{
    auto task = std::make_shared<AlgorithmTask>();
    task->cameraId = cameraId;
    task->image = image.clone();
    task->algoType = type;
    task->priority = priority;
    task->estimatedBytes = task->image.empty() ? 0 : task->image.total() * task->image.elemSize();
    std::future<AlgorithmResult> future = task->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_taskQueue.size() >= m_maxQueueSize)
        {
            ++m_droppedTasks;
            AlgorithmResult result;
            result.success = false;
            result.message = "Task queue is full. Task dropped.";
            task->promise.set_value(std::move(result));
            return future;
        }

        task->sequence = ++m_sequenceCounter;
        m_queuedImageBytes += task->estimatedBytes;
        m_taskQueue.push(std::move(task));
    }
    m_queueCond.notify_one();
    return future;
}

void AlgorithmScheduler::setMaxQueueSize(std::size_t size)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_maxQueueSize = size > 0 ? size : 1;
}

SchedulerStatus AlgorithmScheduler::getStatus() const
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    SchedulerStatus status;
    status.queueSize = m_taskQueue.size();
    status.maxQueueSize = m_maxQueueSize;
    status.inFlightTasks = m_inFlightTasks;
    status.processedTasks = m_processedTasks;
    status.droppedTasks = m_droppedTasks;
    status.avgExecutionMs = m_avgExecutionMs;
    status.maxExecutionMs = m_maxExecutionMs;
    status.queuedImageBytes = m_queuedImageBytes;
    return status;
}

void AlgorithmScheduler::setAlgorithmParams(AlgorithmType type, const AlgorithmParams& params)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_paramsByType[type] = params;
}

void AlgorithmScheduler::dispatchLoop()
{
    while (m_running.load())
    {
        TaskPtr task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCond.wait(lock, [this]() {
                return !m_running.load() || !m_taskQueue.empty();
            });

            if (!m_running.load() && m_taskQueue.empty())
            {
                return;
            }

            m_dispatchCond.wait(lock, [this]() {
                return !m_running.load() || m_inFlightTasks < m_threadCount;
            });

            if (!m_running.load())
            {
                return;
            }

            task = m_taskQueue.top();
            m_taskQueue.pop();
            m_queuedImageBytes -= std::min(m_queuedImageBytes, task->estimatedBytes);
            ++m_inFlightTasks;
        }

        m_threadPool.submit([this, task = std::move(task)]() mutable {
            AlgorithmResult result = runTask(*task);
            task->promise.set_value(std::move(result));

            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_inFlightTasks > 0)
            {
                --m_inFlightTasks;
            }
            m_dispatchCond.notify_one();
        });
    }
}

AlgorithmResult AlgorithmScheduler::runTask(const AlgorithmTask& task)
{
    QElapsedTimer timer;
    timer.start();

    AlgorithmResult result;
    auto algorithm = AlgorithmFactory::create(task.algoType);
    if (!algorithm)
    {
        result.success = false;
        result.message = "Unknown algorithm type.";
    }
    else
    {
        AlgorithmParams params;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            const auto it = m_paramsByType.find(task.algoType);
            if (it != m_paramsByType.end())
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
            result = algorithm->process(matToImage(task.image));
            algorithm->release();
        }
    }

    const double elapsedMs = static_cast<double>(timer.elapsed());
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ++m_processedTasks;
        if (m_processedTasks == 1)
        {
            m_avgExecutionMs = elapsedMs;
        }
        else
        {
            m_avgExecutionMs = (m_avgExecutionMs * static_cast<double>(m_processedTasks - 1) + elapsedMs)
                / static_cast<double>(m_processedTasks);
        }
        m_maxExecutionMs = std::max(m_maxExecutionMs, elapsedMs);
    }

    result.metadata.insert("cameraId", QString::fromStdString(task.cameraId));
    result.metadata.insert("priority", task.priority);
    result.metadata.insert("executionMs", elapsedMs);
    return result;
}

QImage AlgorithmScheduler::matToImage(const cv::Mat& image)
{
    if (image.empty())
    {
        return {};
    }

    cv::Mat rgb;
    if (image.channels() == 1)
    {
        cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
    }
    else
    {
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    }

    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888)
        .copy();
}
} // namespace HMVision
