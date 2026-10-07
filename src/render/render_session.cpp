#include "pt/render/render_session.hpp"
#include "pt/render/accumulator.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/util/thread_pool.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace pt {

namespace {

[[nodiscard]] RenderSettings with_spp(RenderSettings settings, int spp) noexcept {
    settings.samples_per_pixel = spp;
    return settings;
}

} // namespace

RenderSession::RenderSession(const Scene& scene, ThreadPool& pool, const SessionParameters& initial)
    : width_(scene.render.image_width),
      height_(scene.render.image_height),
      thread_count_(static_cast<int>(pool.worker_count()) + 1),
      camera_(initial.camera, width_, height_),
      integrator_(scene.world(), scene.media(), scene.importance_targets(), scene.render.background, initial.max_depth),
      renderer_(camera_, integrator_, with_spp(scene.render, initial.samples_per_pixel), pool),
      acc_(width_, height_),
      epoch_start_(std::chrono::steady_clock::now()),
      mailbox_(width_, height_),
      thread_([this](const std::stop_token& quit) { run(quit); }) {
    // The thread is already running here: the body may read, never write.
    assert(initial.samples_per_pixel > 0 && initial.max_depth > 0);
}

RenderSession::~RenderSession() {
    // Stop first, then cancel: a cancelled pass sends the loop back to its wait, which must already see the stop.
    thread_.request_stop();
    {
        const std::scoped_lock lock(mutex_);
        epoch_stop_.request_stop();
    }
}

std::uint64_t RenderSession::update(const SessionParameters& params) {
    assert(params.samples_per_pixel > 0 && params.max_depth > 0);
    std::uint64_t generation{};
    {
        const std::scoped_lock lock(mutex_);
        pending_ = params;
        has_pending_ = true;
        generation = ++generation_;
        mailbox_full_ = false;      // The waiting image predates these parameters.
        epoch_stop_.request_stop(); // The pass in flight stops at its next tile boundary.
    }
    wake_.notify_one();
    return generation;
}

std::optional<FrameInfo> RenderSession::take_frame(Film& image) {
    std::optional<FrameInfo> info;
    {
        const std::scoped_lock lock(mutex_);
        if (failure_) std::rethrow_exception(failure_);
        if (!mailbox_full_) return std::nullopt;
        assert(image.width() == width_ && image.height() == height_);

        std::swap(image, mailbox_);
        mailbox_full_ = false;
        info = mailbox_info_;
    }
    // An idle session may hold a newer image that was waiting for an empty mailbox.
    wake_.notify_one();
    return info;
}

void RenderSession::run(const std::stop_token& quit) noexcept {
    try {
        loop(quit);
    } catch (...) {
        // An exception escaping a jthread calls std::terminate; carry it to the frontend instead.
        const std::scoped_lock lock(mutex_);
        failure_ = std::current_exception();
    }
}

void RenderSession::loop(const std::stop_token& quit) {
    std::stop_token epoch;
    {
        const std::scoped_lock lock(mutex_);
        epoch = epoch_stop_.get_token();
    }
    while (true) {
        std::optional<SessionParameters> edit;
        std::uint64_t edit_generation{};
        {
            std::unique_lock lock(mutex_);
            static_cast<void>(wake_.wait(lock, quit, [this] {
                return has_pending_ || !converged() || (!mailbox_full_ && published_count_ < acc_.sample_count());
            }));

            // wait() reports the predicate, not the stop, so check the stop itself.
            if (quit.stop_requested()) return;

            if (has_pending_) {
                edit = pending_;
                edit_generation = generation_;
                has_pending_ = false;
                epoch_stop_ = std::stop_source{};
                epoch = epoch_stop_.get_token();
            }
        }

        if (edit) apply(*edit, edit_generation);

        if (!converged()) {
            // Cancelled: the edit or the stop that caused it is taken at the top; the edit resets the partial pass.
            if (!renderer_.render_pass(acc_, acc_.sample_count(), epoch)) continue;
            last_pass_elapsed_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - epoch_start_).count();
        }
        publish_if_wanted();
    }
}

void RenderSession::apply(const SessionParameters& params, std::uint64_t generation) {
    // Between passes, on the session thread: the only point where state the workers read may change.
    camera_ = Camera(params.camera, width_, height_);
    integrator_.set_max_depth(params.max_depth);
    renderer_.set_samples_per_pixel(params.samples_per_pixel);
    acc_.reset();
    applied_generation_ = generation;
    published_count_ = 0;
    epoch_start_ = std::chrono::steady_clock::now();
    last_pass_elapsed_ = 0.0;
}

void RenderSession::publish_if_wanted() {
    {
        const std::scoped_lock lock(mutex_);
        if (mailbox_full_ || published_count_ >= acc_.sample_count()) return;
    }

    // Outside the lock: the frontend never waits on a resolve.
    Film image = acc_.resolve();
    const FrameInfo info{
        .generation = applied_generation_,
        .sample_count = acc_.sample_count(),
        .target_spp = renderer_.samples_per_pixel(),
        .elapsed_seconds = last_pass_elapsed_,
    };
    {
        const std::scoped_lock lock(mutex_);

        // An edit arrived during the resolve: this image is already stale.
        if (has_pending_) return;
        mailbox_ = std::move(image);
        mailbox_info_ = info;
        mailbox_full_ = true;
    }

    published_count_ = info.sample_count;
}

bool RenderSession::converged() const noexcept {
    return acc_.sample_count() >= renderer_.samples_per_pixel();
}

} // namespace pt
