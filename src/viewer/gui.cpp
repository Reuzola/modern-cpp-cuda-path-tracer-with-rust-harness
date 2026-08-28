// ImGui include order matters. No need to use clang format off because it's already alphabetical.
#include "viewer/gui.hpp"
#include "pt/math/scalar.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/util/log.hpp"
#include "viewer/controls.hpp"
#include "viewer/window.hpp"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdexcept>

namespace pt {

Gui::Gui(Window& window, float ui_scale) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    // 1.92 rasterises at the requested size, so scaling stays crisp; ScaleAllSizes handles the metrics.
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(ui_scale);
    style.FontScaleDpi = ui_scale;
    log_info("UI scale: {:.2f}", ui_scale);

    if (!ImGui_ImplGlfw_InitForOpenGL(window.native_handle(), true))
        throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    if (!ImGui_ImplOpenGL3_Init("#version 330"))
        throw std::runtime_error("Failed to initialize ImGui OpenGL3 backend");
}

Gui::~Gui() {
    // Reverse of construction: GL objects go first, the context that owns their handles last.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Gui::begin_frame() noexcept {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Gui::end_frame() noexcept {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool Gui::wants_keyboard() const noexcept {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool Gui::wants_mouse() const noexcept {
    return ImGui::GetIO().WantCaptureMouse;
}

ControlChange Gui::draw_controls(ViewerControls& controls) noexcept {
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) controls_visible_ = !controls_visible_;
    if (!controls_visible_) return {};

    ControlChange change{};
    ImGui::Begin("Controls");

    float exposure = static_cast<float>(controls.tone_map.exposure);
    if (ImGui::SliderFloat("Exposure", &exposure, 0.01f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
        controls.tone_map.exposure = static_cast<Float>(exposure);
        change.display = true;
    }

    // Order mirrors ToneMapOperator; the compiler cannot check this mapping.
    int op = static_cast<int>(controls.tone_map.op);
    if (ImGui::Combo("Tone map", &op, "none\0reinhard\0aces\0")) {
        controls.tone_map.op = static_cast<ToneMapOperator>(op);
        change.display = true;
    }

    if (ImGui::SliderInt("Max depth", &controls.max_depth, 1, 64, "%d", ImGuiSliderFlags_AlwaysClamp)) {
        change.accumulation = true;
    }

    // The engine's real parameter is the N x N stratification grid; spp is derived.
    // Editing spp directly is not invertible: 16 + 1 floors back to 16.
    int sqrt_spp = static_cast<int>(std::lround(std::sqrt(static_cast<double>(controls.target_spp))));
    if (ImGui::InputInt("Passes (N x N)", &sqrt_spp, 1, 4, ImGuiInputTextFlags_EnterReturnsTrue)) {
        sqrt_spp = std::max(sqrt_spp, 1);
        controls.target_spp = sqrt_spp * sqrt_spp;
        change.accumulation = true;
    }
    ImGui::Text("Target spp: %d", controls.target_spp);

    ImGui::End();
    return change;
}

} // namespace pt
