#include "wad.h"

#include <libimagequant.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr std::uint8_t transparent_index = 255;
constexpr std::uint8_t alpha_threshold = 128;
constexpr std::size_t rgba_channels = 4;
constexpr std::size_t rgb_channels = 3;
constexpr std::size_t wad_palette_size = 256;
constexpr std::size_t opaque_palette_size = 255;
constexpr std::uint32_t miptex_header_size = 40;
constexpr std::size_t mip_level_count = 4;
constexpr std::uint32_t wad_header_size = 12;
constexpr std::uint8_t miptex_directory_type = 0x40;
constexpr color_t transparent_color{0, 0, 255};
constexpr char texture_name[] = "{logo";

struct attr_deleter {
    void operator()(liq_attr* value) const { liq_attr_destroy(value); }
};

struct image_deleter {
    void operator()(liq_image* value) const { liq_image_destroy(value); }
};

struct result_deleter {
    void operator()(liq_result* value) const { liq_result_destroy(value); }
};

using attr_ptr = std::unique_ptr<liq_attr, attr_deleter>;
using liq_image_ptr = std::unique_ptr<liq_image, image_deleter>;
using result_ptr = std::unique_ptr<liq_result, result_deleter>;

struct alpha_summary {
    bool has_opaque_pixels = false;
    bool has_transparent_pixels = false;
};

struct wad_palette {
    std::array<color_t, wad_palette_size> colors{};
    std::array<std::uint8_t, wad_palette_size> liq_to_wad_index{};
    std::size_t color_count = 0;
    unsigned int liq_color_count = 0;
};

void checkLiq(liq_error error, const char* action) {
    if (error == LIQ_OK) return;
    throw std::runtime_error(std::string(action) + " failed (libimagequant error " + std::to_string(error) + ")");
}

std::size_t checkedPixelCount(int width, int height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("Image dimensions must be positive");
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h ||
        w * h > std::numeric_limits<std::size_t>::max() / rgba_channels) {
        throw std::length_error("Image dimensions are too large");
    }
    return w * h;
}

alpha_summary summarizeAlpha(const std::vector<std::uint8_t>& pixels) {
    alpha_summary summary;
    for (std::size_t alpha = 3; alpha < pixels.size(); alpha += 4) {
        summary.has_opaque_pixels = summary.has_opaque_pixels || pixels[alpha] >= alpha_threshold;
        summary.has_transparent_pixels = summary.has_transparent_pixels || pixels[alpha] < alpha_threshold;
    }
    return summary;
}

void configureQuantizer(liq_attr* attr, bool has_transparency) {
    const int color_limit = has_transparency ? 256 : 255;
    checkLiq(liq_set_max_colors(attr, color_limit), "setting palette size");
    checkLiq(liq_set_quality(attr, 0, 100), "setting quality");
    checkLiq(liq_set_speed(attr, 3), "setting speed");
    liq_set_last_index_transparent(attr, has_transparency ? 1 : 0);
}

// liq_image_create_rgba borrows its pixels until the image is destroyed.
// Member order makes the image die before its backing vector.
struct liq_bitmap {
    std::vector<std::uint8_t> pixels;
    liq_image_ptr image{};

    liq_bitmap(const liq_attr* attr, const std::vector<std::uint8_t>& source,
               int width, int height) : pixels(source) {
        normalizeAlpha();
        image.reset(liq_image_create_rgba(attr, pixels.data(), width, height, 0.0));
        if (!image) throw std::runtime_error("libimagequant could not create an image");
    }

    void normalizeAlpha() {
        for (std::size_t alpha = 3; alpha < pixels.size(); alpha += 4) {
            pixels[alpha] = pixels[alpha] < alpha_threshold ? 0 : 255;
        }
    }
};

wad_palette makeWadPalette(const liq_palette* liq_colors) {
    if (!liq_colors || liq_colors->count > wad_palette_size) {
        throw std::runtime_error("libimagequant returned an invalid palette");
    }

    wad_palette palette;
    palette.liq_color_count = liq_colors->count;
    for (unsigned int i = 0; i < liq_colors->count; ++i) {
        const auto& color = liq_colors->entries[i];
        if (color.a < alpha_threshold) {
            palette.liq_to_wad_index[i] = transparent_index;
            continue;
        }

        if (palette.color_count == opaque_palette_size) {
            throw std::runtime_error("libimagequant returned too many opaque colors");
        }
        palette.liq_to_wad_index[i] = static_cast<std::uint8_t>(palette.color_count);
        palette.colors[palette.color_count++] = {color.r, color.g, color.b};
    }

    if (palette.color_count == 0) {
        palette.colors[0] = {0, 0, 0};
        palette.color_count = 1;
    }
    return palette;
}

struct quantizer {
    attr_ptr attributes{liq_attr_create()};
    result_ptr result{};
    wad_palette palette{};

    explicit quantizer(const image_t& source) {
        if (!attributes) throw std::bad_alloc();
        const auto pixel_count = checkedPixelCount(source.width, source.height);
        if (source.pixels.size() != pixel_count * rgba_channels) {
            throw std::invalid_argument("Image has invalid RGBA data");
        }

        const auto alpha = summarizeAlpha(source.pixels);
        configureQuantizer(attributes.get(), alpha.has_transparent_pixels);
        if (!alpha.has_opaque_pixels) {
            palette.colors[0] = {0, 0, 0};
            palette.color_count = 1;
            return;
        }
        generatePalette(source);
    }

    void generatePalette(const image_t& source) {
        liq_bitmap input(attributes.get(), source.pixels, source.width, source.height);
        liq_result* generated = nullptr;
        const auto status = liq_image_quantize(input.image.get(), attributes.get(), &generated);
        result.reset(generated);
        checkLiq(status, "palette generation");
        if (!result) throw std::runtime_error("libimagequant returned no result");

        palette = makeWadPalette(liq_get_palette(result.get()));
        checkLiq(liq_set_dithering_level(result.get(), 0.8f), "setting dithering");
    }

    [[nodiscard]] std::vector<std::uint8_t> convert(const mip_t& mip) const {
        const auto pixel_count = checkedPixelCount(mip.width, mip.height);
        if (mip.pixels.size() != pixel_count * rgba_channels) {
            throw std::invalid_argument("Mip has invalid RGBA data");
        }
        if (!result) return std::vector<std::uint8_t>(pixel_count, transparent_index);

        auto indices = remapWithLibimagequant(mip, pixel_count);
        mapToWadPalette(mip, indices);
        return indices;
    }

    [[nodiscard]] std::vector<std::uint8_t> remapWithLibimagequant(const mip_t& mip, std::size_t pixel_count) const {
        std::vector<std::uint8_t> indices(pixel_count);
        liq_bitmap input(attributes.get(), mip.pixels, mip.width, mip.height);
        checkLiq(liq_write_remapped_image(
                     result.get(), input.image.get(), indices.data(), indices.size()),
                 "pixel quantization");
        return indices;
    }

    void mapToWadPalette(const mip_t& mip, std::vector<std::uint8_t>& indices) const {
        for (std::size_t pixel = 0; pixel < indices.size(); ++pixel) {
            const std::size_t rgba = pixel * rgba_channels;
            if (mip.pixels[rgba + 3] < alpha_threshold) {
                indices[pixel] = transparent_index;
                continue;
            }
            indices[pixel] = mapOpaqueIndex(indices[pixel], &mip.pixels[rgba]);
        }
    }

    std::uint8_t mapOpaqueIndex(std::uint8_t liq_index, const std::uint8_t* pixel) const {
        if (static_cast<std::size_t>(liq_index) >= palette.liq_color_count) {
            throw std::runtime_error("libimagequant returned an invalid pixel index");
        }
        const auto wad_index = palette.liq_to_wad_index[liq_index];
        return wad_index == transparent_index ? nearestOpaqueIndex(pixel) : wad_index;
    }

    std::uint8_t nearestOpaqueIndex(const std::uint8_t* pixel) const {
        std::uint32_t best_distance = std::numeric_limits<std::uint32_t>::max();
        std::uint8_t best_index = 0;
        for (std::size_t i = 0; i < palette.color_count; ++i) {
            const int red = static_cast<int>(pixel[0]) - palette.colors[i].r;
            const int green = static_cast<int>(pixel[1]) - palette.colors[i].g;
            const int blue = static_cast<int>(pixel[2]) - palette.colors[i].b;
            const auto distance = static_cast<std::uint32_t>(red * red + green * green + blue * blue);
            if (distance >= best_distance) continue;
            best_distance = distance;
            best_index = static_cast<std::uint8_t>(i);
        }
        return best_index;
    }
};

std::array<std::uint8_t, wad_palette_size * 3> serializePalette(const wad_palette& palette) {
    std::array<std::uint8_t, wad_palette_size * 3> bytes{};
    for (std::size_t i = 0; i < wad_palette_size; ++i) {
        const color_t color = i == transparent_index
            ? transparent_color
            : palette.colors[std::min(i, palette.color_count - 1)];
        const std::size_t rgb = i * rgb_channels;
        bytes[rgb] = color.r;
        bytes[rgb + 1] = color.g;
        bytes[rgb + 2] = color.b;
    }
    return bytes;
}

std::pair<int, int> fittedDimensions(const image_t& image) {
    const auto pixels = checkedPixelCount(image.width, image.height);
    if (image.pixels.size() != pixels * rgba_channels) {
        throw std::invalid_argument("Image has invalid RGBA data");
    }

    constexpr double max_dimension = 128.0;
    constexpr double max_area = 14336.0;
    constexpr int alignment = 16;
    const auto width = static_cast<double>(image.width);
    const auto height = static_cast<double>(image.height);
    const double scale = std::min({
        1.0,
        max_dimension / std::max(width, height),
        std::sqrt(max_area / (width * height))
    });

    const auto alignedSize = [scale, alignment](int size) {
        const int scaled = static_cast<int>(static_cast<double>(size) * scale);
        return std::max(alignment, (scaled / alignment) * alignment);
    };

    int fitted_width = alignedSize(image.width);
    int fitted_height = alignedSize(image.height);
    while (static_cast<std::int64_t>(fitted_width) * fitted_height >
           static_cast<std::int64_t>(max_area)) {
        if (fitted_width >= fitted_height && fitted_width > alignment) {
            fitted_width -= alignment;
        } else if (fitted_height > alignment) {
            fitted_height -= alignment;
        } else {
            break;
        }
    }
    return {fitted_width, fitted_height};
}

std::uint32_t toU32(std::size_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("WAD data exceeds the format's size limit");
    }
    return static_cast<std::uint32_t>(value);
}

void appendU16(std::string& output, std::uint16_t value) {
    output.push_back(static_cast<char>(value));
    output.push_back(static_cast<char>(value >> 8));
}

void appendU32(std::string& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<char>(value >> shift));
    }
}

void appendBytes(std::string& output, const void* data, std::size_t size) {
    output.append(static_cast<const char*>(data), size);
}

std::array<std::uint32_t, mip_level_count> mipOffsets(
    const std::array<std::vector<std::uint8_t>, mip_level_count>& mipmaps) {
    std::array<std::uint32_t, mip_level_count> offsets{};
    std::uint32_t offset = miptex_header_size;
    for (std::size_t i = 0; i < mipmaps.size(); ++i) {
        offsets[i] = offset;
        const auto size = toU32(mipmaps[i].size());
        if (size > std::numeric_limits<std::uint32_t>::max() - offset) {
            throw std::length_error("Mip texture exceeds the format's size limit");
        }
        offset += size;
    }
    return offsets;
}

std::array<std::vector<std::uint8_t>, mip_level_count> makeMipmaps(
    const image_t& image, int width, int height, const quantizer& colors) {
    std::array<std::vector<std::uint8_t>, mip_level_count> mipmaps;
    for (auto& mipmap : mipmaps) {
        mipmap = colors.convert(mip_t(image, width, height));
        width = std::max(1, width / 2);
        height = std::max(1, height / 2);
    }
    return mipmaps;
}

std::string serializeMipTexture(
    const std::array<char, 16>& name,
    std::uint32_t width,
    std::uint32_t height,
    const std::array<std::uint32_t, mip_level_count>& offsets,
    const std::array<std::vector<std::uint8_t>, mip_level_count>& mipmaps,
    const std::array<std::uint8_t, wad_palette_size * 3>& palette) {
    std::string lump;
    appendBytes(lump, name.data(), name.size());
    appendU32(lump, width);
    appendU32(lump, height);
    for (const auto offset : offsets) appendU32(lump, offset);
    for (const auto& mipmap : mipmaps) appendBytes(lump, mipmap.data(), mipmap.size());
    appendU16(lump, static_cast<std::uint16_t>(wad_palette_size));
    appendBytes(lump, palette.data(), palette.size());
    return lump;
}

std::string wrapInWad3(const std::string& lump) {
    std::string wad("WAD3", 4);
    appendU32(wad, 1);
    appendU32(wad, toU32(wad_header_size + lump.size()));
    appendBytes(wad, lump.data(), lump.size());

    appendU32(wad, wad_header_size);
    appendU32(wad, toU32(lump.size()));
    appendU32(wad, toU32(lump.size()));
    wad.push_back(static_cast<char>(miptex_directory_type));
    wad.push_back('\0');
    appendU16(wad, 0);
    appendBytes(wad, lump.data(), 16);
    return wad;
}
}

wad_t::wad_t(const image_t& image) {
    const auto [fitted_width, fitted_height] = fittedDimensions(image);
    std::copy_n(texture_name, sizeof(texture_name) - 1, name.begin());
    width = static_cast<std::uint32_t>(fitted_width);
    height = static_cast<std::uint32_t>(fitted_height);

    const quantizer colors(image);
    palette = serializePalette(colors.palette);
    mipmaps = makeMipmaps(image, fitted_width, fitted_height, colors);
    offsets = mipOffsets(mipmaps);
}

std::string wad_t::write() const {
    const auto lump = serializeMipTexture(name, width, height, offsets, mipmaps, palette);
    return wrapInWad3(lump);
}

std::string createWad(const image_t& image) {
    return wad_t{image}.write();
}
