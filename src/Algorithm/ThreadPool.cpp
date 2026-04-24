#include "../../include/Algorithm/ThreadPool.h"

namespace HMVision
{
ThreadPool::ThreadPool(std::size_t threadCount)
{
    if (threadCount == 0)
    {
        threadCount = 1;
    }

    m_running.store(true);
    m_workers.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i)
    {
        m_workers.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running.load())
        {
            return;
        }
        m_running.store(false);
    }

    m_condition.notify_all();
    for (auto& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    m_workers.clear();
}

std::size_t ThreadPool::pendingTaskCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tasks.size();
}

std::size_t ThreadPool::workerCount() const
{
    return m_workers.size();
}

void ThreadPool::enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running.load())
        {
            throw std::runtime_error("ThreadPool has been shut down.");
        }
        m_tasks.emplace(std::move(task));
    }
    m_condition.notify_one();
}

void ThreadPool::workerLoop()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this]() {
                return !m_running.load() || !m_tasks.empty();
            });

            if (!m_running.load() && m_tasks.empty())
            {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        task();
    }
}
} // namespace HMVision
