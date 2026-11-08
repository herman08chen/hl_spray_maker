# hl_spray_maker

Convert an image into a GoldSrc (Half-Life) spray WAD file.

Given any supported input image, the program produces `tempdecal.wad`: a WAD3
file containing a single GoldSrc miptexture named `{logo`, with four mip
levels, a 256-color palette, and alpha-tested transparency. The output can be
used directly as a spray in GoldSrc games.

There are two builds:

- **Native CLI** (`hl_spray_maker`): converts one image or a whole directory.
- **Web** (`spray_maker`): Emscripten/WebAssembly build with a browser UI that
  lets you pick an image and download the generated `tempdecal.wad`.

A live version is hosted at https://herman08chen.github.io/hl_spray_maker/.

## Features

- Reads common image formats (PNG, JPEG, BMP, TGA, GIF, PSD, HDR, PIC, PPM,
  PGM) via stb_image.
- Quantizes RGBA pixels to a 256-color palette with libimagequant (up to 255
  opaque colors; palette index 255 is reserved for transparency).
- Alpha threshold of 128: pixels below it become fully transparent.
- Scales the image aspect-preservingly to fit the chosen GoldSrc spray
  resolution, then centers it with transparent padding (no cropping).
- Generates four mipmap levels, each half the size of the previous (minimum
  1x1).
- Picks the best supported resolution automatically: the one preserving the
  most detail, preferring greater occupancy, then smaller area, then smaller
  width/height on ties.
- Writes a valid WAD3 file (`WAD3` header, miptex directory entry, mip
  offsets, palette).
- Batch mode: convert every image in a directory into
  `<name>_wad/tempdecal.wad`.
- Includes a CTest suite (`resolution_tests`) covering resolution selection,
  mip padding, and WAD serialization.

## How it works

The pipeline runs in three stages:

1. **Decode** (`image.cpp`): the input file is decoded with stb_image into an
   RGBA pixel buffer (`image_t`).
2. **Quantize and mipmap** (`wad.cpp`): libimagequant builds a palette from the
   image. For each of the four mip levels, the image is resized
   aspect-preservingly with stb_image_resize, padded to the target resolution
   with transparent pixels, and remapped to palette indices. Transparent pixels
   map to index 255.
3. **Serialize** (`wad.cpp`): the mip levels and palette are packed into a
   miptexture lump named `{logo`, wrapped in a WAD3 container, and written to
   `tempdecal.wad`.

Resolution selection (`chooseDimensions`) considers the supported GoldSrc spray
sizes (16x16 through 256x256, subject to an area limit of strictly less than
12,288 pixels) and chooses the best fit for the source image. Both the
`alphatest` and `indexalpha` spray formats expose the same resolution table;
the native build uses `alphatest`.

## WAD format

The output is a **WAD3** archive — the package format used by GoldSrc
(Half-Life) for textures, sounds, models, and sprays. It contains:

- **Header:** magic `"WAD3"`, a lump count, and the byte offset to the directory.
- **Directory:** one entry per lump, storing offset, size, type, and a 16-byte
  name. For textures the type is `0x40` (miptex).
- **Lump data:** a single miptexture lump named `{logo`, containing the texture
  name, width and height, four mip-level offsets, the four mipmap levels stored
  as palette indices (each half the size of the previous, down to 1×1), and a
  256-entry RGB palette.

Palette index **255** is reserved for transparency; its palette entry is bright
blue `(0, 0, 255)`. GoldSrc renders pixels at index 255 as transparent and all
other indices as opaque (alpha testing).

## How a file becomes a WAD

1. **Decode** — The input file is decoded with stb_image into an RGBA pixel
   buffer (`image_t`).
2. **Choose resolution** — Picks the best supported spray size, favoring the one
   that preserves the most source detail, then greater screen occupancy, then
   smaller area.
3. **Quantize** — libimagequant builds a 256-color palette. Pixels with alpha
   below 128 become transparent (index 255); opaque pixels map to one of up to
   255 palette colors.
4. **Build mipmaps** — For each of the four levels, the image is resized
   aspect-preservingly with stb_image_resize and centered in the target
   resolution with transparent padding (no cropping). Pixels are remapped to
   palette indices.
5. **Serialize** — The miptexture lump (name, dimensions, offsets, mip data,
   palette) is packed and wrapped in a WAD3 container with a directory entry.
6. **Write** — The result is saved as `tempdecal.wad`.

## Project structure

```
hl_spray_maker/
├── main.cpp          # Native CLI entry point (single file or --debug batch)
├── image.h / image.cpp   # Image decoding (stb_image) and mipmap generation (stbir)
├── wad.h / wad.cpp       # WAD3 serialization, palette quantization (libimagequant)
├── web_bindings.cpp      # Emscripten bindings for the web build
├── web/                  # Web UI (index.html, app.js, style.css)
├── docs/                 # Published web site copy (index.html, app.js, style.css, WASM)
├── third_party/          # Vendored dependencies (stb, libimagequant static libs)
├── test_images/          # Sample inputs and pre-converted WADs
├── resolution_tests.cpp  # CTest suite
├── CMakeLists.txt        # Build configuration
└── README.md
```

## Dependencies

All dependencies are vendored under `third_party/` except the toolchain:

- **stb_image** and **stb_image_resize2** — image decoding and resizing.
  https://github.com/nothings/stb
  Header-only, included directly in `image.cpp`.
- **libimagequant** — palette quantization.
  https://github.com/imageoptim/libimagequant
  Prebuilt static libraries are committed for native
  (`third_party/libimagequant/lib/libimagequant_sys.a`) and WebAssembly
  (`third_party/libimagequant/wasm/libimagequant_sys.a`) builds, with headers
  in `third_party/libimagequant/include`.

### Build requirements

- CMake 3.20+
- C++23 compiler (GCC or Clang; the code uses `std::format`, `std::span`, and
  other C++23 facilities)
- For the web build: Emscripten SDK

## Building

### Native (CLI)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This produces `build/hl_spray_maker`. Run `cmake --build build --target
resolution_tests` and `ctest --test-dir build` to build and run the test suite.

### Web (Emscripten)

```bash
emcmake cmake -S . -B build-wasm -DEMSCRIPTEN=ON
cmake --build build-wasm
```

The build outputs `spray_maker.js`, `spray_maker.wasm`, and copies the web UI
into `build-wasm/web/`. Serve that directory with any static HTTP server (the
WASM module must be fetched over HTTP, not opened as `file://`).

## Usage

### Native CLI

Convert a single image to `tempdecal.wad` in the current directory:

```bash
hl_spray_maker photo.png
```

Batch-convert every image in a directory. Each image `<name>.ext` becomes
`<name>_wad/tempdecal.wad`:

```bash
hl_spray_maker --debug /path/to/image/directory
```

The program prints one line per conversion and exits non-zero if any file
fails.

### Web

1. Open the hosted site (or serve `build-wasm/web/` locally).
2. Choose an image file.
3. Click **Generate WAD**.
4. Download `tempdecal.wad`.

## Output

The generated file is always named `tempdecal.wad` and is a WAD3 archive with
one lump:

- Texture name: `{logo`
- Format: GoldSrc miptexture, 4 mip levels
- Palette: 256 RGB entries (index 255 = transparent, bright blue `0, 0, 255`)
- Transparency: alpha-tested, threshold 128

Place `tempdecal.wad` in your GoldSrc spray folder (typically
`valve/spray/` or a mod-specific spray directory) and select it in-game.

## License

See [LICENSE](LICENSE).
