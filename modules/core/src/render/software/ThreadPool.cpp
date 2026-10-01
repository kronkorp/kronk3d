#include "ThreadPool.hpp"
#include <algorithm>

k3::sw::ThreadPool::ThreadPool(unsigned threads)
{
    if (threads == 0)
        threads = std::max(1u, std::thread::hardware_concurrency());

    m_workers.reserve(threads - 1);
    for (unsigned i = 1; i < threads; ++i)
        m_workers.emplace_back([this] { workerLoop(); });
}

k3::sw::ThreadPool::~ThreadPool()
{
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    for (auto& worker : m_workers)
        worker.join();
}

void k3::sw::ThreadPool::parallelFor(std::size_t count, const std::function<void(std::size_t)>& fn)
{
    if (count == 0)
        return;
    if (m_workers.empty() || count == 1) {
        for (std::size_t i = 0; i < count; ++i)
            fn(i);
        return;
    }

    {
        std::lock_guard lock(m_mutex);
        m_job = &fn;
        m_count = count;
        m_next.store(0, std::memory_order_relaxed);
        m_busy = static_cast<unsigned>(m_workers.size());
        ++m_generation;
    }
    m_wake.notify_all();

    runJobs();

    std::unique_lock lock(m_mutex);
    m_done.wait(lock, [this] { return m_busy == 0; });
    m_job = nullptr;
}

void k3::sw::ThreadPool::runJobs()
{
    for (std::size_t i = m_next.fetch_add(1, std::memory_order_relaxed); i < m_count; i = m_next.fetch_add(1, std::memory_order_relaxed))
        (*m_job)(i);
}

void k3::sw::ThreadPool::workerLoop()
{
    std::size_t seen = 0;

    for (;;) {
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [&] { return m_stop || m_generation != seen; });
            if (m_stop)
                return;
            seen = m_generation;
        }

        runJobs();

        {
            std::lock_guard lock(m_mutex);
            if (--m_busy == 0)
                m_done.notify_one();
        }
    }
}
