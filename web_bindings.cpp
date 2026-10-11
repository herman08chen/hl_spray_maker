
#include "wad.h"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <span>
#include <string>
#include <emscripten/emscripten.h>

EM_JS(void, expose_wasm_memory_js, (), {
    Module["wasmMemory"] = wasmMemory;
});

namespace {
std::string last_error;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
void expose_wasm_memory() {
    expose_wasm_memory_js();
}

EMSCRIPTEN_KEEPALIVE
std::uint8_t* create_wad(
    const std::uint8_t* encoded_image,
    std::uint32_t encoded_size,
    std::uint32_t* output_size
) {
    if (output_size == nullptr) {
        return nullptr;
    }

    *output_size = 0;
    last_error.clear();

    try {
        if (encoded_image == nullptr || encoded_size == 0) {
            throw std::invalid_argument("Please select a valid image.");
        }

        const image_t image{
            std::span<const std::uint8_t>(
                encoded_image,
                static_cast<std::size_t>(encoded_size)
            )
        };

        const std::string wad = createWad(image);

        if (wad.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Generated WAD is too large.");
        }

        auto* output = static_cast<std::uint8_t*>(
            std::malloc(wad.size())
        );

        if (output == nullptr) {
            throw std::bad_alloc{};
        }

        std::memcpy(output, wad.data(), wad.size());
        *output_size = static_cast<std::uint32_t>(wad.size());
        return output;
    } catch (const std::exception& error) {
        last_error = error.what();
    } catch (...) {
        last_error = "An unknown error occurred.";
    }

    return nullptr;
}

EMSCRIPTEN_KEEPALIVE
const char* wad_last_error() {
    return last_error.c_str();
}

EMSCRIPTEN_KEEPALIVE
void free_wad(std::uint8_t* buffer) {
    std::free(buffer);
}

}
