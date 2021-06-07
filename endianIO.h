//
// Created by david on 2021/6/7.
//

#ifndef JDVPDF_ENDIANIO_H
#define JDVPDF_ENDIANIO_H

/**
 * Read an unsigned integer from file in big endian
 * @param file the file to be read from
 * @param size the size of the integer (in byte number)
 */
inline uint32_t readUnsignedFromFileBE(FILE* file, size_t size)
{
    uint8_t buf[sizeof(uint32_t)] = {0};
    fread(buf, 1, size, file);
    return
            (buf[3] << (8 * 0)) +
            (buf[2] << (8 * 1)) +
            (buf[1] << (8 * 2)) +
            (buf[0] << (8 * 3));
}

/**
 * Writes an unsigned integer to file in big endian
 * @param file the file to be written to
 * @param val the integer to be written
 * @param size the size of the integer (in byte number)
 */
inline void writeUnsignedToFileBE(FILE* file, uint32_t val, size_t size)
{
    assert(0 < size && size <= 4);
    switch (size)
    {
        case 1:
            fputc(val, file);
            return;
        case 2:
            fputc(val >> 8, file);
            fputc(val & 0xFF, file);
            return;
        case 3:
            fputc(val >> (8 * 2), file);
            fputc((val >> 8) & 0xFF, file);
            fputc(val & 0xFF, file);
            return;
        case 4:
            fputc(val >> (8 * 3), file);
            fputc((val >> (8 * 2)) & 0xFF, file);
            fputc((val >> 8) & 0xFF, file);
            fputc(val & 0xFF, file);
            return;
    }
}

#endif //JDVPDF_ENDIANIO_H
