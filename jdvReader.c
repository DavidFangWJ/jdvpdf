//
// Created by david on 2021/5/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "fontObject.h"
#include "jdvReader.h"

#define SET1        128
#define SET_RULE    132
#define BOP         139
#define EOP         140
#define XXX1        239
#define FONT_DEF1   243
#define PRE         247

int numPage;

int paperWidth = 595;
int paperHeight = 842;

FILE* inFile;

struct FontTable {
    int size;
    Font* font;
} fontTable[64];

inline static int readJdvInt(int size, FILE* f)
{
    int result = 0;
    for (int i=0; i<size; ++i)
        result = (result << 8) + fgetc(f);
    return result;
}

/**
 * 第一次扫描。用于记录页数和所有字体命令。
 * @param fileName 文件名
 */
void parse1(const char* fileName)
{
    int32_t pointer;
    int tmp;
    char buffer[512];

    inFile = fopen(fileName, "rb");
    if (!inFile)
    {
        fputs("找不到指定的文件。", stderr);
        exit(1);
    }
    // 寻找文件尾
    fseek(inFile, -5, SEEK_END); // 跳过固定的4字节
    while (fgetc(inFile) == 223)
        fseek(inFile, -2, SEEK_CUR); // 向前一字节
    // 新读的这个字节肯定是identification byte
    fseek(inFile, -5, SEEK_CUR);
    pointer = readJdvInt(4, inFile); // postamble的第一字节
    fseek(inFile, pointer + 1, SEEK_SET);
    numPage = 0;
    while ((pointer = readJdvInt(4, inFile)) != -1)
    {
        ++numPage;
        fseek(inFile, pointer + 41, SEEK_SET);
    }
    printf("%d", numPage);

    // 从头开始寻找各类font_def命令
    fseek(inFile, 0x0El, SEEK_SET);
    tmp = fgetc(inFile); // comment长度
    fseek(inFile, tmp, SEEK_CUR); // 第一个BOP位置
    while ((tmp = fgetc(inFile)) != -1)
    {
        if (tmp < SET1){} // 1～127号，输出字符
        else if (tmp < SET_RULE) // 128～131号，127以后的字符
            fseek(inFile, tmp - 127, SEEK_CUR);
        else if (tmp == BOP) // 139号命令表示BOP
            fseek(inFile, 44l, SEEK_CUR);
        else if (tmp >= XXX1 && tmp < FONT_DEF1) // 239～242；注释、special
        {
            tmp = readJdvInt(tmp - XXX1 + 1, inFile);
            fseek(inFile, tmp, SEEK_CUR);
        }
        else if (tmp >= FONT_DEF1 && tmp < PRE) // 字体定义
        {
            tmp = readJdvInt(tmp - FONT_DEF1 + 1, inFile);
            struct FontTable* p = fontTable + tmp; // 指向相应的序号
            fseek(inFile, 4l, SEEK_CUR); // 不再用checksum
            p->size = readJdvInt(4, inFile);
            fseek(inFile, 4l, SEEK_CUR); // 不再用TFM中的size
            tmp = fgetc(inFile);
            tmp += fgetc(inFile); // 整个目录的长度
            fread(buffer, 1, tmp, inFile);
            buffer[tmp] = 0; // 字符串结尾
            if (*buffer == ':') // 表示有TTC中的字体序号
            {
                char* pos = strchr(buffer, ':');
                *pos = 0;
                p->font = fontFromFile(pos+1, atoi(buffer + 1));
            }
            else p->font = fontFromFile(buffer, 0);
        }
    }
}