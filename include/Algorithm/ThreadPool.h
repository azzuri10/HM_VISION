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
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency());
    ~ThreadPool();

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

        enqueue([task]() { (*task)(); });
        return future;
    }

    void shutdown();
    std::size_t pendingTaskCount() const;
    std::size_t workerCount() const;

private:
    void enqueue(std::function<void()> task);
    void workerLoop();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_running{false};
};
} // namespace HMVision
