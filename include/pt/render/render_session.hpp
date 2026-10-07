#pragma once
#include "pt/render/accumulator.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace pt {

class ThreadPool;

// Everything the estimator depends on that a frontend may change; any change restarts accumulation.
struct SessionParameters {
    CameraSettings camera{};
    int max_depth{};
    int samples_per_pixel{};
};

struct FrameInfo {
    std::uint64_t generation{}; // The update() whose parameters produced the image; 0 is the constructor's.
    int sample_count{};
    int target_spp{};
    double elapsed_seconds{}; // Wall time from the parameters taking effect to the image's last pass.
};

// Progressive rendering on a thread of its own, for an interactive frontend. The
// frontend calls update() and take_frame(); the camera, integrator, renderer and
// accumulator belong to the session thread, which changes them only between passes.
// While a session lives, only its thread submits work to the pool: a frontend that
// waited on the same pool would pick up render tiles and stall for a whole pass.
// The scene and the pool must outlive the session.
class RenderSession final {
public:
    RenderSession(const Scene& scene, ThreadPool& pool, const SessionParameters& initial);
    ~RenderSession();

    RenderSession(const RenderSession&) = delete;
    RenderSession& operator=(const RenderSession&) = delete;
    RenderSession(RenderSession&&) = delete;
    RenderSession& operator=(RenderSession&&) = delete;

    // Replaces the parameters and cancels the pass in flight; returns the generation that frames rendered with them will carry.
    std::uint64_t update(const SessionParameters& params);

    // Swaps the waiting image into 'image' and returns its info, or nullopt if none is waiting.
    // Never blocks on rendering. Rethrows a failure of the session thread.
    [[nodiscard]] std::optional<FrameInfo> take_frame(Film& image);

    // The pool's workers plus the session thread; the frontend's thread is not counted.
    [[nodiscard]] int thread_count() const noexcept { return thread_count_; }

private:
    // Session thread only, after construction.
    int width_{};
    int height_{};
    int thread_count_{};
    Camera camera_;
    PathIntegrator integrator_;
    Renderer renderer_;
    Accumulator acc_;
    std::uint64_t applied_generation_{};
    int published_count_{};
    std::chrono::steady_clock::time_point epoch_start_;
    double last_pass_elapsed_{};

    // Shared with the frontend; guarded by mutex_.
    std::mutex mutex_;
    std::condition_variable_any wake_; // The session thread sleeps here.
    SessionParameters pending_{};
    bool has_pending_{};
    std::uint64_t generation_{};
    std::stop_source epoch_stop_; // One per epoch: a stop_source cannot be un-requested.
    Film mailbox_;
    FrameInfo mailbox_info_{};
    bool mailbox_full_{};
    std::exception_ptr failure_;
    std::jthread thread_; // Declared last: joined before anything the loop uses is destroyed.

    void run(const std::stop_token& quit) noexcept;

    void loop(const std::stop_token& quit);

    void apply(const SessionParameters& params, std::uint64_t generation);

    void publish_if_wanted();

    [[nodiscard]] bool converged() const noexcept;
};

} // namespace pt
