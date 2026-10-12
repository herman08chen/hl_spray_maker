
#include "wad.h"

#include <emscripten/emscripten.h>
#include <emscripten/heap.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <span>
#include <string>

EM_JS(void, expose_wasm_memory_js, (), {
    Module["wasmMemory"] = wasmMemory;
});

namespace {
std::array<char, 512> last_error{};

void setLastError(const char* message) noexcept {
    std::snprintf(last_error.data(), last_error.size(), "%s", message ? message : "Unknown error.");
}

bool isValidHeapRange(const void* pointer, const std::size_t size, const std::size_t alignment = 1) {
    if (pointer == nullptr) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto heap_size = emscripten_get_heap_size();
    return address % alignment == 0 && address <= heap_size && size <= heap_size - address;
}
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
    if (!isValidHeapRange(output_size, sizeof(*output_size), alignof(std::uint32_t))) {
        setLastError("Invalid output-size pointer.");
        return nullptr;
    }

    *output_size = 0;
    setLastError("");

    try {
        if (encoded_size == 0 || !isValidHeapRange(encoded_image, encoded_size)) {
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
        setLastError(error.what());
    } catch (...) {
        setLastError("An unknown error occurred.");
    }

    return nullptr;
}

EMSCRIPTEN_KEEPALIVE
const char* wad_last_error() {
    return last_error.data();
}

EMSCRIPTEN_KEEPALIVE
void free_wad(std::uint8_t* buffer) {
    std::free(buffer);
}

}
