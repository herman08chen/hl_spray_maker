#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

    std::vector<char> contents((std::istreambuf_iterator<char>(input)), {});
    if (input.bad()) {
        throw std::runtime_error("Failed while reading input image: " + path.string());
    }
    return contents;
}

void writeFile(const fs::path& path, const std::string& contents) {
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
        if (!entry.is_regular_file()) continue;

        const fs::path outputDirectory = directory / (entry.path().filename().string() + "_wad");
        try {
            const std::string wadData = createWadFromFile(entry.path());
            fs::create_directories(outputDirectory);
            const fs::path outputPath = outputDirectory / "tempdecal.wad";
            writeFile(outputPath, wadData);
            std::println("Converted {} -> {}", entry.path().string(), outputPath.string());
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
