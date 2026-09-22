#pragma once
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace pt {

// Fixed-size worker pool. Tasks are submitted through a TaskGroup and are only
// guaranteed to have run once that group's wait() returns; a waiting thread
// runs pending tasks itself, so nested submit-and-wait cannot deadlock and
// worker_count() == 0 degrades to running everything on the caller.
class ThreadPool final {
public:
    using Task = std::function<void()>;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    ~ThreadPool() = default;

    explicit ThreadPool(unsigned worker_count = default_worker_count());

    [[nodiscard]] static unsigned default_worker_count() noexcept;

    [[nodiscard]] unsigned worker_count() const noexcept {
        return static_cast<unsigned>(workers_.size());
    }

private:
    std::mutex mutex_;
    std::condition_variable_any queue_cv_;
    std::deque<Task> queue_;

    // Declared last: members are destroyed in reverse, so the workers are
    // stopped and joined before the queue and mutex they use are gone.
    std::vector<std::jthread> workers_;

    void worker_loop(const std::stop_token& stop_token);

    void enqueue(Task task);

    [[nodiscard]] bool try_run_one();

    void wait_for_work(const std::atomic<std::size_t>& pending);

    void notify_waiters();

    friend class TaskGroup;
};

// Counts the tasks it submitted; wait() runs pending work until they have all finished.
// A task must not wait on the group it belongs to (it would wait for itself).
// The pool must outlive every group created on it.
class TaskGroup final {
public:
    TaskGroup(const TaskGroup&) = delete;
    TaskGroup& operator=(const TaskGroup&) = delete;
    TaskGroup(TaskGroup&&) = delete;
    TaskGroup& operator=(TaskGroup&&) = delete;
    ~TaskGroup();

    explicit TaskGroup(ThreadPool& pool) noexcept;

    template <typename F>
        requires std::copy_constructible<std::decay_t<F>> && std::invocable<std::decay_t<F>&>
    void run(F&& f) {
        pending_.fetch_add(1, std::memory_order_relaxed);

        auto task = [this, fn = std::forward<F>(f)]() mutable {
            try {
                fn();
            } catch (...) {
                capture_exception();
            }
            on_task_done();
        };

        try {
            pool_.enqueue(std::move(task));
        } catch (...) {
            on_task_done();
            throw;
        }
    }

    void wait();

private:
    ThreadPool& pool_;
    std::atomic<std::size_t> pending_{};

    // Tasks may throw (e.g. std::bad_alloc): the first exception is kept and
    // rethrown by wait(), and the remaining tasks still run.
    std::mutex exception_mutex_;
    std::exception_ptr first_exception_;

    void on_task_done() noexcept;

    void capture_exception() noexcept;

    void drain();
};

} // namespace pt
