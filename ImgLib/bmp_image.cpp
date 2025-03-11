#include "bmp_image.h"
#include "pack_defines.h"

#include <array>
#include <algorithm>
#include <fstream>
#include <string_view>
#include <cstdint>
#include <vector>

using namespace std;

namespace img_lib {

const static uint32_t INFO_HEADER_SIZE = 40;
const static uint32_t FILE_HEADER_SIZE = 14;

const static uint32_t TOTAL_HEAD_SIZE = INFO_HEADER_SIZE + FILE_HEADER_SIZE;

PACKED_STRUCT_BEGIN BitmapInfoHeader {
    // поля заголовка Bitmap Info Header
    uint32_t head_size = INFO_HEADER_SIZE;
    uint32_t image_width;
    uint32_t image_height;
    uint16_t plane_count = 1;
    uint16_t bits_per_pixel = 24;
    uint32_t compress_type = 0;
    uint32_t data_size;
    uint32_t horiz_dens = 11811;
    uint32_t vert_dens = 11811;
    uint32_t used_colors_count = 0;
    uint32_t signif_colors_count = 0x1000000;
}
PACKED_STRUCT_END

PACKED_STRUCT_BEGIN BitmapFileHeader {
    // поля заголовка Bitmap File Header
    array<char, 2> sign = {'B', 'M'};
    uint32_t   total_size;
    uint32_t  reserved = 0;
    uint32_t data_step = TOTAL_HEAD_SIZE;
}
PACKED_STRUCT_END

// функция вычисления отступа по ширине
static int GetBMPStride(int w) {
    return 4 * ((w * 3 + 3) / 4);
}

uint32_t GetDataSize(const Image& image) {
    int stride = GetBMPStride(image.GetWidth());
    return stride * image.GetHeight();
}

BitmapFileHeader MakeFileHeader(const Image& image) {
    BitmapFileHeader file_header;
    file_header.total_size = TOTAL_HEAD_SIZE + GetDataSize(image);
    return file_header;
}

BitmapInfoHeader MakeInfoHeader(const Image& image) {
    BitmapInfoHeader info_header;
    info_header.image_height = image.GetHeight();
    info_header.image_width = image.GetWidth();
    info_header.data_size = GetDataSize(image);
    return info_header;
}

std::vector<char> MakeLineBuf(const Color* img_line, const int width) {
    std::vector<char> result;
    int stride = GetBMPStride(width);
    result.reserve(stride);
    for_each_n(img_line, width, [&](Color pixel) {
        result.push_back(static_cast<char>(pixel.b));
        result.push_back(static_cast<char>(pixel.g));
        result.push_back(static_cast<char>(pixel.r));
    });
    while (result.size() < stride) {
        result.push_back(0);
    }
    return result;
}

// напишите эту функцию
bool SaveBMP(const Path& file, const Image& image) {
    ofstream out_file(file, ios::binary);
    if (!out_file) {
        return false;
    }

    int w = image.GetWidth();
    int h = image.GetHeight();

    BitmapFileHeader file_header = MakeFileHeader(image);
    out_file.write(reinterpret_cast<char*>(&file_header), FILE_HEADER_SIZE); 
    if (!out_file) {
        return false;
    }

    BitmapInfoHeader info_header = MakeInfoHeader(image);
    out_file.write(reinterpret_cast<char*>(&info_header), INFO_HEADER_SIZE); 
    if (!out_file) {
        return false;
    }

    std::vector<char> buff;
    for (int i = h - 1; i >= 0; --i) {
        const img_lib::Color* line = image.GetLine(i);
        auto buf = MakeLineBuf(line, w);
        out_file.write(buf.data(), buf.size());
    }

    return out_file.good();
}

void FillImgLine (Color* line, int width, const vector<char>& buf) {
    for (int i = 0; i < width; ++i) {
        line[i].r = std::byte(buf[i * 3 + 2]);
        line[i].g = std::byte(buf[i * 3 + 1]);
        line[i].b = std::byte(buf[i * 3]);
    }
}

// напишите эту функцию
Image LoadBMP(const Path& file) {
    ifstream input_file(file, ios::binary);
    if (!input_file) {
        return {};
    }
    BitmapFileHeader file_header;
    input_file.read(reinterpret_cast<char*>(&file_header), FILE_HEADER_SIZE);
    if (!input_file || (file_header.sign != array<char, 2>{'B', 'M'})) {
        return {};
    }
    BitmapInfoHeader info_header;
    input_file.read(reinterpret_cast<char*>(&info_header), INFO_HEADER_SIZE);
    if (!input_file || (info_header.head_size != INFO_HEADER_SIZE)) {
        return {};
    }
    const int w = static_cast<int>(info_header.image_width);
    const int h = static_cast<int>(info_header.image_height);
    const int stride = GetBMPStride(w);

    Image result(w, h, Color::Black());
    vector<char> buf(stride);
    for (int i = h - 1; i >= 0; --i) {
        input_file.read(buf.data(), stride);
        FillImgLine(result.GetLine(i), w, buf);
    }

    return result;
}

}  // namespace img_lib