#include "viewer/display.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <glad/glad.h>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<GLuint, unsigned int>, "GLuint must be unsigned int");

namespace pt {

namespace {

constexpr const char* vertex_shader_source = R"(#version 330 core
out vec2 v_uv;
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_uv = vec2(pos.x, 1.0 - pos.y);
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* fragment_shader_source = R"(#version 330 core
in vec2 v_uv;
out vec4 frag_color;
uniform sampler2D u_image;
void main() {
    frag_color = texture(u_image, v_uv);
}
)";

GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        std::string info_log(static_cast<std::size_t>(log_len), '\0');
        glGetShaderInfoLog(shader, log_len, nullptr, info_log.data());
        if (!info_log.empty()) info_log.pop_back();
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }

    return shader;
}

} // namespace

Display::Display(int width, int height) : width_(width), height_(height) {
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    program_ = glCreateProgram();

    glAttachShader(program_, vs);
    glAttachShader(program_, fs);

    glLinkProgram(program_);

    GLint success = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint log_len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_len);
        std::string info_log(static_cast<std::size_t>(log_len), '\0');
        glGetProgramInfoLog(program_, log_len, nullptr, info_log.data());
        if (!info_log.empty()) info_log.pop_back();

        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program_);
        throw std::runtime_error("Program link failed: " + info_log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "u_image"), 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB8,
        width,
        height,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glGenVertexArrays(1, &vao_);
}

Display::~Display() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
    glDeleteTextures(1, &texture_);
}

void Display::upload(std::span<const std::uint8_t> pixels) {
    assert(pixels.size() == static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 3);

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
}

void Display::draw(int framebuffer_width, int framebuffer_height) const {
    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glUseProgram(program_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace pt
