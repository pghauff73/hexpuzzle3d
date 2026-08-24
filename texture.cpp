#include "texture.h"

#include <GL/glu.h>
#include <png.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace hexpuzzle {

Texture2D::~Texture2D() {
    reset();
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : id_(std::exchange(other.id_, 0)) {
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        reset();
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}

void Texture2D::loadPng(const std::filesystem::path& path) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    const std::string filename = path.string();
    if (!png_image_begin_read_from_file(&image, filename.c_str())) {
        throw std::runtime_error("cannot read PNG " + filename + ": " + image.message);
    }

    image.format = PNG_FORMAT_RGBA;
    const std::size_t width = image.width;
    const std::size_t height = image.height;
    constexpr std::size_t channelCount = 4;
    if (width > std::numeric_limits<std::size_t>::max() / channelCount ||
        (width != 0 && height > std::numeric_limits<std::size_t>::max() / (width * channelCount))) {
        png_image_free(&image);
        throw std::runtime_error("PNG dimensions are too large: " + filename);
    }
    const std::size_t rowBytes = width * channelCount;
    std::vector<png_byte> pixels(rowBytes * height);
    if (!png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr)) {
        const std::string message = image.message;
        png_image_free(&image);
        throw std::runtime_error("cannot decode PNG " + filename + ": " + message);
    }

    std::vector<png_byte> flipped(pixels.size());
    for (std::size_t row = 0; row < height; ++row) {
        const std::size_t source = row * rowBytes;
        const std::size_t destination = (height - row - 1) * rowBytes;
        std::copy_n(pixels.data() + source, rowBytes, flipped.data() + destination);
    }
    png_image_free(&image);

    reset();
    glGenTextures(1, &id_);
    if (id_ == 0) {
        throw std::runtime_error("OpenGL failed to allocate texture for " + filename);
    }
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (gluBuild2DMipmaps(
            GL_TEXTURE_2D,
            GL_RGBA,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            flipped.data()) != 0) {
        reset();
        throw std::runtime_error("OpenGL failed to upload texture " + filename);
    }
}

GLuint Texture2D::id() const noexcept {
    return id_;
}

bool Texture2D::loaded() const noexcept {
    return id_ != 0;
}

void Texture2D::reset() noexcept {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
}

void TextureLibrary::load(const std::filesystem::path& assetDirectory) {
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        digits_[index].loadPng(assetDirectory / ("n" + std::to_string(index) + ".png"));
    }
}

const Texture2D& TextureLibrary::digit(std::size_t value) const {
    return digits_.at(value);
}

}  // namespace hexpuzzle
