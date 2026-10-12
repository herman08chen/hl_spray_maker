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
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "stb_image.h"
#include "stb_image_resize2.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "image.h"

#include <algorithm>
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

    int resized_width = image.width;
    int resized_height = image.height;
    if (image.width > targetWidth || image.height > targetHeight) {
        const bool width_limited = static_cast<std::int64_t>(targetWidth) * image.height <=
                                   static_cast<std::int64_t>(targetHeight) * image.width;
        if (width_limited) {
            resized_width = targetWidth;
            const auto scaled_height = static_cast<std::int64_t>(image.height) * targetWidth;
            resized_height = std::max(1, static_cast<int>(
                (scaled_height + image.width / 2) / image.width));
            resized_height = std::min(resized_height, targetHeight);
        } else {
            resized_height = targetHeight;
            const auto scaled_width = static_cast<std::int64_t>(image.width) * targetHeight;
            resized_width = std::max(1, static_cast<int>(
                (scaled_width + image.height / 2) / image.height));
            resized_width = std::min(resized_width, targetWidth);
        }
    }
    std::vector<std::uint8_t> resized(rgbaByteCount(resized_width, resized_height));
    const bool success = stbir_resize_uint8_linear(
        image.pixels.data(),
        image.width,
        image.height,
        0,                  // Automatic source row stride
        resized.data(),
        resized_width,
        resized_height,
        0,                  // Automatic destination row stride
        STBIR_RGBA
    );

    if (!success) {
        throw std::runtime_error("Failed to resize image");
    }
    const int left = (targetWidth - resized_width) / 2;
    const int top = (targetHeight - resized_height) / 2;
    for (int y = 0; y < resized_height; ++y) {
        const auto src = static_cast<std::size_t>(y) * static_cast<std::size_t>(resized_width) * rgba_channels;
        const auto dst = (static_cast<std::size_t>(y + top) * static_cast<std::size_t>(targetWidth) +
                          static_cast<std::size_t>(left)) * rgba_channels;
        std::copy_n(resized.data() + src, static_cast<std::size_t>(resized_width) * rgba_channels,
                    pixels.data() + dst);
    }
}
