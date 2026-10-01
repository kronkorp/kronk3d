/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Fixed-size thread pool running data-parallel loops
*/
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace k3::sw
{

    class ThreadPool
    {
        public:
            // `threads` counts the calling thread too; 0 means one per hardware thread.
            explicit ThreadPool(unsigned threads = 0);
            ~ThreadPool();

            ThreadPool(const ThreadPool&) = delete;
            ThreadPool& operator=(const ThreadPool&) = delete;

            [[nodiscard]] unsigned size() const noexcept { return static_cast<unsigned>(m_workers.size()) + 1; }

            // Calls fn(i) for every i in [0, count), spread over the workers and the calling thread, and
            // returns once all calls are done. Indices are handed out one by one (dynamic scheduling), so
            // uneven work balances itself. `fn` must not throw.
            void parallelFor(std::size_t count, const std::function<void(std::size_t)>& fn);

        private:
            void workerLoop();
            void runJobs();

            std::vector<std::thread>                  m_workers;
            std::mutex                                m_mutex;
            std::condition_variable                   m_wake;
            std::condition_variable                   m_done;
            const std::function<void(std::size_t)>*   m_job = nullptr;
            std::size_t                               m_count = 0;
            std::atomic<std::size_t>                  m_next{0};
            std::size_t                               m_generation = 0;
            unsigned                                  m_busy = 0;
            bool                                      m_stop = false;
    };

}
