#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace HMVision
{
class ThreadPool
{
public:
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency())
    {
        if (threadCount == 0)
        {
            threadCount = 1;
        }
        m_running.store(true);
        m_workers.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i)
        {
            m_workers.emplace_back([this]() {
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
            });
        }
    }

    ~ThreadPool()
    {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto submit(F&& function, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using ReturnType = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(function), std::forward<Args>(args)...));

        std::future<ReturnType> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running.load())
            {
                throw std::runtime_error("ThreadPool has been shut down.");
            }
            m_tasks.emplace([task]() { (*task)(); });
        }
        m_condition.notify_one();
        return future;
    }

    void shutdown()
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

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_running{false};
};
} // namespace HMVision
