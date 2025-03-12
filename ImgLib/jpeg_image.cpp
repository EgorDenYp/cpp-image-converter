#include "ppm_image.h"

#include <array>
#include <algorithm>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <setjmp.h>

#include <jpeglib.h>

using namespace std;

namespace img_lib {

// структура из примера LibJPEG
struct my_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

typedef my_error_mgr* my_error_ptr;

// функция из примера LibJPEG
METHODDEF(void)
my_error_exit (j_common_ptr cinfo) {
    my_error_ptr myerr = (my_error_ptr) cinfo->err;
    (*cinfo->err->output_message) (cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

std::vector<JSAMPLE> MakeLineBuffer(const Color* line, int line_width) {
    std::vector<JSAMPLE> result(line_width * 3);
    size_t i = 0;
    std::for_each_n(line, line_width, [&](const Color& pixel) {
        result[i * 3]     = static_cast<JSAMPLE>(pixel.r);
        result[i * 3 + 1] = static_cast<JSAMPLE>(pixel.g);
        result[i * 3 + 2] = static_cast<JSAMPLE>(pixel.b);
        ++i;
    });
    return result;
}


bool SaveJPEG(const Path& file, const Image& image) {
    //example of applying this library lies in file example.c in source code archive of thisl library
    int image_height = image.GetHeight();
    int image_width = image.GetWidth();

    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    FILE * outfile;       /* target file */
    JSAMPROW row_pointer[1];  /* pointer to JSAMPLE row[s] */
    int row_stride;       /* physical row width in image buffer */

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    #ifdef _MSC_VER
        if ((outfile = _wfopen(file.wstring().c_str(), L"wb")) == NULL) 
    #else
        if ((outfile = fopen(file.string().c_str(), "wb")) == NULL)
    #endif
    {
        return false;
    }
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = image_width;  /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;       /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;   /* colorspace of input image */

    jpeg_set_defaults(&cinfo);
    
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image_width * 3; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
        auto line_buf = MakeLineBuffer(image.GetLine(cinfo.next_scanline), image_width);
        row_pointer[0] = line_buf.data();
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    
    int close_result = fclose(outfile);

    jpeg_destroy_compress(&cinfo);

    if (close_result == 0)
        return true;
    else 
        return false;
}

// тип JSAMPLE фактически псевдоним для unsigned char
void SaveScanlineToImage(const JSAMPLE* row, int y, Image& out_image) {
    Color* line = out_image.GetLine(y);
    for (int x = 0; x < out_image.GetWidth(); ++x) {
        const JSAMPLE* pixel = row + x * 3;
        line[x] = Color{std::byte{pixel[0]}, std::byte{pixel[1]}, std::byte{pixel[2]}, std::byte{255}};
    }
}

Image LoadJPEG(const Path& file) {
    jpeg_decompress_struct cinfo;
    my_error_mgr jerr;
    
    FILE* infile;
    JSAMPARRAY buffer;
    int row_stride;

    // Тут не избежать функции открытия файла из языка C,
    // поэтому приходится использовать конвертацию пути к string.
    // Под Visual Studio это может быть опасно, и нужно применить
    // нестандартную функцию _wfopen
#ifdef _MSC_VER
    if ((infile = _wfopen(file.wstring().c_str(), L"rb")) == NULL) {
#else
    if ((infile = fopen(file.string().c_str(), "rb")) == NULL) {
#endif
        return {};
    }

    /* Шаг 1: выделяем память и инициализируем объект декодирования JPEG */

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return {};
    }

    jpeg_create_decompress(&cinfo);

    /* Шаг 2: устанавливаем источник данных */

    jpeg_stdio_src(&cinfo, infile);

    /* Шаг 3: читаем параметры изображения через jpeg_read_header() */

    (void) jpeg_read_header(&cinfo, TRUE);

    /* Шаг 4: устанавливаем параметры декодирования */

    // установим желаемый формат изображения
    cinfo.out_color_space = JCS_RGB;
    cinfo.output_components = 3;

    /* Шаг 5: начинаем декодирование */

    (void) jpeg_start_decompress(&cinfo);
    
    row_stride = cinfo.output_width * cinfo.output_components;
    
    buffer = (*cinfo.mem->alloc_sarray)
                ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    /* Шаг 5a: выделим изображение ImgLib */
    Image result(cinfo.output_width, cinfo.output_height, Color::Black());

    /* Шаг 6: while (остаются строки изображения) */
    /*                     jpeg_read_scanlines(...); */

    while (cinfo.output_scanline < cinfo.output_height) {
        int y = cinfo.output_scanline;
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);

        SaveScanlineToImage(buffer[0], y, result);
    }

    /* Шаг 7: Останавливаем декодирование */

    (void) jpeg_finish_decompress(&cinfo);

    /* Шаг 8: Освобождаем объект декодирования */

    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return result;
}

} // of namespace img_lib