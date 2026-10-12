//
// Created by herman on 10/10/26.
//

#ifndef HL_SPRAY_MAKER_IMAGE_H
#define HL_SPRAY_MAKER_IMAGE_H
#include <cstdint>
#include <span>
#include <vector>

struct color_t {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

class image_t {
public:
    image_t() = default;
    int width = 0;
    int height = 0;

    // Four bytes per pixel: R, G, B, A
    std::vector<std::uint8_t> pixels{};

    // Decode encoded image bytes when constructing the object.
    explicit image_t(std::span<const std::uint8_t> encodedImage);
};

struct mip_t {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;

    mip_t(const image_t& image, int targetWidth, int targetHeight);
};

#endif //HL_SPRAY_MAKER_IMAGE_H
