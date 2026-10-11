#ifndef HL_SPRAY_MAKER_WAD_H
#define HL_SPRAY_MAKER_WAD_H

#include "image.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Builds a WAD3 file containing one GoldSrc miptexture suitable for a spray.
class wad_t {
public:
    explicit wad_t(const image_t& image);

    [[nodiscard]] std::string write() const;

private:
    std::array<char, 16> name{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::array<std::uint32_t, 4> offsets{};
    std::array<std::vector<std::uint8_t>, 4> mipmaps{};
    std::array<std::uint8_t, 256 * 3> palette{};
};

[[nodiscard]] std::string createWad(const image_t& image);

#endif
