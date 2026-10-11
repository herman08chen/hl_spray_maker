//
// Created by herman on 10/10/26.
//

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

// Keep strict project warnings enabled while excluding vendored stb internals.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif
#include "stb_image.h"
#include "stb_image_resize2.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "image.h"

#include <format>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
constexpr std::size_t rgba_channels = 4;

std::size_t rgbaByteCount(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }

    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h ||
        w * h > std::numeric_limits<std::size_t>::max() / rgba_channels) {
        throw std::length_error("Image dimensions are too large");
    }
    return w * h * rgba_channels;
}
}

image_t::image_t(const std::span<const std::uint8_t> encodedImage) {
    if (encodedImage.empty() || encodedImage.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Encoded image is empty or too large");
    int channels = 0;
    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> data(
        stbi_load_from_memory(
            encodedImage.data(), static_cast<int>(encodedImage.size()),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha
        ),
        &stbi_image_free
    );
    if (!data) {
        throw std::runtime_error(std::format("Image loading failed: {}", stbi_failure_reason()));
    }
    const auto byte_count = rgbaByteCount(width, height);
    pixels.assign(data.get(), data.get() + byte_count);
}

mip_t::mip_t(const image_t& image, const int targetWidth, const int targetHeight)
        : width(targetWidth), height(targetHeight),
          pixels(rgbaByteCount(targetWidth, targetHeight)) {
    const auto source_bytes = rgbaByteCount(image.width, image.height);
    if (image.pixels.size() != source_bytes) {
        throw std::invalid_argument("Source image has invalid RGBA data");
    }

    const bool success = stbir_resize_uint8_linear(
        image.pixels.data(),
        image.width,
        image.height,
        0,                  // Automatic source row stride
        pixels.data(),
        targetWidth,
        targetHeight,
        0,                  // Automatic destination row stride
        STBIR_RGBA
    );

    if (!success) {
        throw std::runtime_error("Failed to resize image");
    }
}
