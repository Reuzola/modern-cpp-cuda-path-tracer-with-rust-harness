#include "pt/util/thread_pool.hpp"
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace pt {

ThreadPool::ThreadPool(unsigned worker_count) {
    workers_.reserve(worker_count);

    for (unsigned i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this](const std::stop_token& token) {
            worker_loop(token);
        });
    }
}

unsigned ThreadPool::default_worker_count() noexcept {
    const unsigned hc = std::thread::hardware_concurrency();
    return hc > 1 ? hc - 1 : 0;
}

void ThreadPool::worker_loop(const std::stop_token& stop_token) {
    while (true) {
        std::unique_lock lock(mutex_);
        if (!queue_cv_.wait(lock, stop_token, [this] { return !queue_.empty(); })) return;

        const Task task = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        task();
    }
}

void ThreadPool::enqueue(Task task) {
    {
        const std::scoped_lock lock(mutex_);
        queue_.push_back(std::move(task));
    }
    queue_cv_.notify_one();
}

bool ThreadPool::try_run_one() {
    Task task;
    {
        const std::scoped_lock lock(mutex_);
        if (queue_.empty()) return false;

        task = std::move(queue_.front());
        queue_.pop_front();
    }
    task();
    return true;
}

void ThreadPool::wait_for_work(const std::atomic<std::size_t>& pending) {
    std::unique_lock lock(mutex_);

    queue_cv_.wait(lock, [this, &pending] {
        return !queue_.empty() || pending.load(std::memory_order_acquire) == 0;
    });
}

void ThreadPool::notify_waiters() {
    {
        // Empty critical section: a waiter checks its predicate under mutex_, so taking it
        // here orders this notify after that check and the wakeup cannot be lost.
        const std::scoped_lock lock(mutex_);
    }
    queue_cv_.notify_all();
}

TaskGroup::~TaskGroup() { drain(); }

TaskGroup::TaskGroup(ThreadPool& pool) noexcept : pool_(pool) {}

void TaskGroup::wait() {
    drain();
    std::exception_ptr e_ptr;
    {
        const std::scoped_lock lock(exception_mutex_);
        e_ptr = std::exchange(first_exception_, nullptr);
    }
    if (e_ptr) std::rethrow_exception(e_ptr);
}

void TaskGroup::on_task_done() noexcept {
    // Read before the decrement: once pending_ reaches zero the waiter may return
    // and destroy this group, so no member may be touched after fetch_sub.
    ThreadPool& pool = pool_;
    if (pending_.fetch_sub(1, std::memory_order_release) == 1) pool.notify_waiters();
}

void TaskGroup::capture_exception() noexcept {
    const std::scoped_lock lock(exception_mutex_);
    if (!first_exception_) first_exception_ = std::current_exception();
}

void TaskGroup::drain() {
    while (pending_.load(std::memory_order_acquire) != 0) {
        if (!pool_.try_run_one()) pool_.wait_for_work(pending_);
    }
}

} // namespace pt
