#include "core/CoreException.hpp"
#include <image/BmpImage.hpp>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>

namespace
{
    void writeLE16(std::ofstream& file, uint16_t value)
    {
        const char bytes[2] = {
            static_cast<char>(value & 0xFF),
            static_cast<char>((value >> 8) & 0xFF)
        };
        file.write(bytes, sizeof(bytes));
    }

    void writeLE32(std::ofstream& file, uint32_t value)
    {
        const char bytes[4] = {
            static_cast<char>(value & 0xFF),
            static_cast<char>((value >> 8) & 0xFF),
            static_cast<char>((value >> 16) & 0xFF),
            static_cast<char>((value >> 24) & 0xFF)
        };
        file.write(bytes, sizeof(bytes));
    }

    uint8_t toByte(float channel)
    {
        return static_cast<uint8_t>(std::clamp(channel, 0.f, 1.f) * 255.f + 0.5f);
    }
}

void k3::BmpImage::save(std::string_view filename) const
{
    std::ofstream file(std::string(filename), std::ios::binary);
    if (!file.is_open() || file.bad()) {
        throw FileNotOpen();
    }

    constexpr uint16_t bitsPerPixel   = 24;
    const uint32_t      width         = static_cast<uint32_t>(this->m_width);
    const uint32_t      height        = static_cast<uint32_t>(this->m_height);
    const uint32_t      rowSize       = ((bitsPerPixel * width + 31) / 32) * 4;
    const uint32_t      pixelDataSize = rowSize * height;
    constexpr uint32_t  fileHeaderSize  = 14;
    constexpr uint32_t  dibHeaderSize   = 40;
    constexpr uint32_t  pixelDataOffset = fileHeaderSize + dibHeaderSize;
    const uint32_t      fileSize        = pixelDataOffset + pixelDataSize;

    // --- BMP file header ---
    file.put('B');
    file.put('M');
    writeLE32(file, fileSize);
    writeLE32(file, 0); // reserved
    writeLE32(file, pixelDataOffset);

    // --- DIB header (BITMAPINFOHEADER) ---
    writeLE32(file, dibHeaderSize);
    writeLE32(file, width);
    writeLE32(file, height);
    writeLE16(file, 1); // color planes
    writeLE16(file, bitsPerPixel);
    writeLE32(file, 0); // BI_RGB, no compression
    writeLE32(file, pixelDataSize);
    writeLE32(file, 2835); // x pixels per meter (~72 DPI)
    writeLE32(file, 2835); // y pixels per meter (~72 DPI)
    writeLE32(file, 0); // colors in palette
    writeLE32(file, 0); // important colors

    // --- Pixel data: bottom-up, BGR, rows padded to a multiple of 4 bytes ---
    const char   padding[3]  = {0, 0, 0};
    const size_t paddingSize = rowSize - width * 3;

    for (size_t y = this->m_height; y-- > 0;) {
        for (size_t x = 0; x < this->m_width; ++x) {
            const k3::Pixel& pixel = this->m_pixels[y * this->m_width + x];
            const char bgr[3] = {
                static_cast<char>(toByte(pixel.b)),
                static_cast<char>(toByte(pixel.g)),
                static_cast<char>(toByte(pixel.r))
            };
            file.write(bgr, sizeof(bgr));
        }
        if (paddingSize > 0) {
            file.write(padding, static_cast<std::streamsize>(paddingSize));
        }
    }
}
