#pragma once

#include <GL/gl.h>

#include <array>
#include <cstddef>
#include <filesystem>

namespace hexpuzzle {

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    void loadPng(const std::filesystem::path& path);
    GLuint id() const noexcept;
    bool loaded() const noexcept;

private:
    void reset() noexcept;

    GLuint id_ = 0;
};

class TextureLibrary {
public:
    void load(const std::filesystem::path& assetDirectory);

    const Texture2D& digit(std::size_t value) const;

private:
    std::array<Texture2D, 10> digits_;
};

}  // namespace hexpuzzle
