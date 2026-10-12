#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "wad.h"

namespace {
namespace fs = std::filesystem;

std::vector<char> readFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open input image: " + path.string());
    }

    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) >
        static_cast<std::uintmax_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("Input image is too large: " + path.string());
    }

    std::vector<char> contents(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!contents.empty()) {
        input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    }
    if (!input || input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error("Failed while reading input image: " + path.string());
    }
    return contents;
}

void writeFile(const fs::path& path, const std::string& contents) {
    if (contents.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("Output WAD is too large to write: " + path.string());
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not open output WAD for writing: " + path.string());
    }

    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error("Failed while writing output WAD: " + path.string());
    }
}

std::string createWadFromFile(const fs::path& inputPath) {
    const auto encoded = readFile(inputPath);
    const auto image = image_t{std::span(
        reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size())};
    return createWad(image);
}

void convertImage(const fs::path& inputPath, const fs::path& outputPath) {
    writeFile(outputPath, createWadFromFile(inputPath));
}

int convertDirectory(const fs::path& directory) {
    if (!fs::is_directory(directory)) {
        throw std::runtime_error("Debug path is not a directory: " + directory.string());
    }

    std::size_t converted = 0;
    std::size_t failed = 0;
    for (const auto& entry : fs::directory_iterator(directory)) {
        try {
            if (!entry.is_regular_file()) continue;
            const auto input_path = entry.path();
            // Keep one stable output directory per image, independent of its extension.
            const fs::path outputDirectory = directory / (input_path.stem().string() + "_wad");
            const std::string wadData = createWadFromFile(input_path);
            fs::create_directories(outputDirectory);
            const fs::path outputPath = outputDirectory / "tempdecal.wad";
            writeFile(outputPath, wadData);
            std::println("Converted {} -> {}", input_path.string(), outputPath.string());
            ++converted;
        } catch (const std::exception& e) {
            std::cerr << "Failed to convert " << entry.path() << ": " << e.what() << '\n';
            ++failed;
        }
    }

    std::println("Batch complete: {} converted, {} failed", converted, failed);
    return failed == 0 ? 0 : 1;
}

int run(int argc, char* argv[]) {
    if (argc == 3 && std::string(argv[1]) == "--debug") {
        return convertDirectory(argv[2]);
    }
    if (argc != 2) {
        throw std::invalid_argument(
            "Usage: " + std::string(argv[0]) + " <image> | --debug <directory>");
    }

    convertImage(argv[1], "tempdecal.wad");
    return 0;
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
