#include "bmp_image.h"
#include "pack_defines.h"

#include <array>
#include <fstream>
#include <string_view>

using namespace std;

namespace img_lib {

PACKED_STRUCT_BEGIN BitmapFileHeader{
    uint16_t bf_type; // Символы "BM"
    uint32_t bf_size; // Размер файла
    uint16_t bf_reserved_1; // Зарезервированное поле = 0
    uint16_t bf_reserved_2; // Зарезервированное поле = 0
    uint32_t bf_off_bits; // Отступ данных от начала файла (размер структуры BitmapFileHeader + BitmapInfoHeader)
}
PACKED_STRUCT_END

PACKED_STRUCT_BEGIN BitmapInfoHeader{
    uint32_t bi_size; // Размер информационного заголовка
    int32_t bi_width; // Ширина в пикселях
    int32_t bi_height; // Высота в пикселях
    uint16_t bi_planes; // Количество плоскостей = 1
    uint16_t bi_bit_count; // Количество бит на пиксель = 24
    uint32_t bi_compression; // Тип сжатия = 0 (отсутствие сжатия)
    uint32_t bi_size_image; // Количество байт в данных
    int32_t bi_x_pels_per_meter; // Горизонтальное разрешение, пикселей на метр 300DPI (11811)
    int32_t bi_y_pels_per_meter; // Вертикальное разрешение, пикселей на метр 300DPI(11811)
    int32_t bi_clr_used; // Количество использованных цветов = 0
    int32_t bi_clr_important; // Количество значимых цветов = 0x1000000
}
PACKED_STRUCT_END

// функция вычисления отступа по ширине
static int GetBMPStride(int width) {
    const int alignment = 4;
    const int byte_per_pix = 3;
    return alignment * ((width * byte_per_pix + byte_per_pix) / alignment);
}

bool SaveBMP(const Path& file, const Image& image) {
    // открываем поток с флагом ios::binary
    // поскольку будем записывать данные в двоичном формате
    ofstream out(file, ios::binary);

    // Получаем отступ по ширине
    int BMPStride = GetBMPStride(image.GetWidth());
    // Высчитываем размер файла
    uint32_t image_size = BMPStride * image.GetHeight();

    static int32_t DPI_300 = 11811;

    // Отступ данных от начала файла
    uint32_t off_bits = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

    BitmapFileHeader file_header{
        .bf_type = 0x4d42, // строка ASCII "BM"
        .bf_size = off_bits + image_size,
        .bf_reserved_1 = 0,
        .bf_reserved_2 = 0,
        .bf_off_bits = off_bits
    };

    out.write((char*)&file_header, sizeof(BitmapFileHeader));

    BitmapInfoHeader info_header{
        .bi_size = sizeof(BitmapInfoHeader),
        .bi_width = image.GetWidth(),
        .bi_height = image.GetHeight(),
        .bi_planes = 1,
        .bi_bit_count = 24,
        .bi_compression = 0,
        .bi_size_image = image_size,
        .bi_x_pels_per_meter = DPI_300,
        .bi_y_pels_per_meter = DPI_300,
        .bi_clr_used = 0,
        .bi_clr_important = 0x1000000
    };

    out.write((char*)&info_header, sizeof(BitmapInfoHeader));

    const int w = image.GetWidth();
    const int h = image.GetHeight();
    std::vector<char> buff(BMPStride);

    for (int y = h - 1; y >= 0; --y) {
        const Color* line = image.GetLine(y);
        for (int x = 0; x < w; ++x) {
            buff[x * 3 + 0] = static_cast<char>(line[x].b);
            buff[x * 3 + 1] = static_cast<char>(line[x].g);
            buff[x * 3 + 2] = static_cast<char>(line[x].r);
        }
        std::fill(buff.begin() + w * 3, buff.end(), 0);
        out.write(buff.data(), BMPStride);
        }

    return out.good();

}

Image LoadBMP(const Path& file) {
    // открываем поток с флагом ios::binary
    // поскольку будем читать данные в двоичном формате
    ifstream ifs(file, ios::binary);
        
    // Создаем и читаем из потока BitmapFileHeader
    BitmapFileHeader file_header;

    auto file_size = filesystem::file_size(file);
    uint32_t off_bits = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
    
    ifs.read((char*)&file_header, sizeof(BitmapFileHeader));

    // Проверка состояния потока и корректности файлового заголовка
    if (!ifs.good() ||
        file_header.bf_type != 0x4d42 /*строка ASCII "BM"*/ ||
        file_header.bf_size != file_size || 
        file_header.bf_reserved_1 != 0 ||
        file_header.bf_reserved_2 != 0 ||
        file_header.bf_off_bits != off_bits)
    {
        return {};
    }

    // Создаем и читаем из потока BitmapInfoHeader
    BitmapInfoHeader info_header;

    ifs.read((char*)&info_header, sizeof(BitmapInfoHeader));

    // Проверка состояния потока размера информационного заголовка
    if (!ifs.good() || info_header.bi_size != sizeof(BitmapInfoHeader))
    {
        return {};
    }

    Image result(info_header.bi_width, info_header.bi_height, Color::Black());

    int BMPStride = GetBMPStride(info_header.bi_width);
    std::vector<char> buff(BMPStride);

    for (int y = info_header.bi_height - 1; y >= 0; --y) {
        Color* line = result.GetLine(y);
        ifs.read(buff.data(), BMPStride);
        if (!ifs.good()) {
            return {};
        }
        for (int x = 0; x < info_header.bi_width; ++x) {
            line[x].b = static_cast<byte>(buff[x * 3 + 0]);
            line[x].g = static_cast<byte>(buff[x * 3 + 1]);
            line[x].r = static_cast<byte>(buff[x * 3 + 2]);
        }
    }

    return result;
}

}  // namespace img_lib