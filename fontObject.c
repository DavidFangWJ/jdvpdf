//
// Created by david on 2021/5/14.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fontObject.h"

#define MAX_NUM_FONTS 128

const char* orderings[5] = {"CNS1", "GB1", "Identity", "Japan1", "Korea1"};

inline static void deleteFont(Font* f)
{
    fclose(f->fontFile);
    free(f->tableRecords);
}

// hash table
struct FontNode {
    char dir[256];
    Font current;
    struct FontNode* next;
};

inline static unsigned hashFromString(char* str)
{
    unsigned hash = 0;
    while (*str)
    {
        hash *= 31;
        hash += *str++;
    }
    return hash % 64;
}

struct FontNode* fontLibrary[64];

void initiateFontLibrary()
{
    for (int i=0; i<64; ++i) fontLibrary[i] = NULL;
}

void deleteFontLibrary()
{
    for (int i=0; i<64; ++i)
    {
        struct FontNode* current = fontLibrary[i], *next;
        while (current)
        {
            next = current->next;
            deleteFont(&current->current);
            free(current);
            current = next;
        }
    }
}

inline static void switchEndian16(uint16_t* num)
{
    static uint8_t c1, c2;
    c1 = *num & 0xFFu;
    c2 = (*num & 0xFF00u) >> 8u;
    *num = (c1 << 8u) + c2;
}

inline static void switchEndian32(uint32_t* num)
{
    static uint8_t c1, c2, c3, c4;
    c1 = *num & 0xFFu;
    c2 = (*num & 0xFF00u) >> 8u;
    c3 = (*num & 0xFF0000u) >> 16u;
    c4 = (*num & 0xFF000000u) >> 24u;
    *num = (c1 << 24u) + (c2 << 16u) + (c3 << 8u) + c4;
}

inline static uint16_t readUint16BE(FILE* f)
{
    static uint8_t c1, c2;
    c1 = fgetc(f);
    c2 = fgetc(f);
    return (c1 << 8u) + c2;
}

inline static uint32_t readUint32BE(FILE* f)
{
    static uint8_t c[4];
    fread(c, 1, 4, f);
    return (c[0] << 24u) + (c[1] << 16u) + (c[2] << 8u) + c[3];
}

uint16_t findIndexOfTable(Font* obj, const char* tagStr)
{
    uint32_t tag = (tagStr[0] << 24) + (tagStr[1] << 16) + (tagStr[2] << 8) + tagStr[3];
    // 二分查找
    uint16_t left = 0, right = obj->numTables - 1, mid = (left + right) / 2;
    while (left < right)
    {
        uint32_t current = obj->tableRecords[mid].tableTag;
        if (current == tag) break;
        if (current > tag) right = mid - 1;
        else left = mid + 1;
        mid = (left + right) / 2;
    }
    return mid;
}

/**
 * 寻找给定Tag的表格偏移量。
 * tagStr必须至少为4字节。
 * @return 以32位无符号整数表示的偏移量
 */
inline static uint32_t findOffsetOfTable(Font* obj, const char* tagStr)
{
    uint16_t index = findIndexOfTable(obj, tagStr);
    return obj->tableRecords[index].offset;
}

struct NameTableHeader {
    uint16_t format;
    uint16_t count;
    uint16_t stringOffset;
};

struct NameRecord {
    uint16_t platformID;
    uint16_t encodingID;
    uint16_t languageID;
    uint16_t nameID;
    uint16_t length;
    uint16_t offset;
};

struct NameTable {
    struct NameTableHeader header;
    struct NameRecord* records;
};

/**
 * 读取字体中的Name表（TTF、OTF通用）。
 * @param fontFile 字体文件
 * @param output 输出到的地址
 */
void readNameTable(FILE* fontFile, struct NameTable* output)
{
    fread(&output->header, sizeof(struct NameTableHeader), 1, fontFile);
    switchEndian16(&output->header.count);
    switchEndian16(&output->header.stringOffset);

    output->records = malloc(output->header.count * sizeof(struct NameRecord));
    fread(output->records, sizeof(struct NameRecord), output->header.count, fontFile);
    struct NameRecord* end = output->records + output->header.count;
    for (struct NameRecord* current = output->records; current != end; ++current)
    {
        switchEndian16(&current->length);
        switchEndian16(&current->offset);
    }
}

/**
 * 从name表中读取Postscript名（TTF用）
 * @param obj 字体文件头的指针
 * @param fontFile 字体文件本身
 * @param output 输出地址，必须有64字节以上的空间
 */
void getNameTtf(Font* f)
{
    FILE* fontFile = f->fontFile;
    uint32_t offset = findOffsetOfTable(f, "name");
    fseek(fontFile, offset, SEEK_SET);

    struct NameTable table;
    readNameTable(fontFile, &table);

    offset += table.header.stringOffset;
    uint16_t index = 0xFFFFu;
    // 寻找表示PostScript名的项
    for (uint16_t i = 0; i < table.header.count; ++i)
        if (table.records[i].platformID == 0x0300u && table.records[i].nameID == 0x0600u) // 是Postscript名
        {
            index = i;
            break;
        }
    fseek(fontFile, offset + table.records[index].offset, SEEK_SET);
    uint16_t length = table.records[index].length;
    char* temp = malloc(length);
    for (uint16_t i = 0; i < length; ++i)
    {
        char currentChar = fgetc(fontFile);
        temp[i] = currentChar;
    }
    for (uint16_t i = 1; i < length; i += 2)
        f->CIDFontName[i / 2] = temp[i];
    free(temp);
    f->CIDFontName[length / 2] = '\0';
    free(table.records);
}

inline static int readIndexInt(int size, FILE* f)
{
    int result = 0;
    for (int i=0; i<size; ++i)
        result = (result << 8) + fgetc(f);
    return result;
}

/**
 * 读取CFF字体Dict中用压缩手段保存的数字。
 * @pre 第一字节在正常范围内
 * @param current 已经读取出的第一字节
 * @param f 字体文件
 */
int readDictInt(int current, FILE* f)
{
    if (current < 30)
    {
        int numBytes = 1 << (current - 27); // 28时为2位，29时为4位
        current = 0;
        for (int i=0; i<numBytes; ++i)
            current = (current << 8) + fgetc(f);
    }
    else if (current == 30) // 坑人的十进制浮点数我懒得做了
    {
        while ((current % 16) != 15) current = fgetc(f);
        current = INT32_MAX;
    }
    else if (current < 247) current -= 139; // 一字节
    else // 二字节
    {
        if (current < 251)
            current = (current - 247 << 8) + 108;
        else current = (251 - current << 8) - 108;
        current += fgetc(f);
    }
    return current;
}

// 读取CFF格式中的字体名，并确定它是否为CID字体
void getNameCff(Font* f)
{
    uint16_t indexCFF = findIndexOfTable(f, "CFF ");
    uint32_t length = f->tableRecords[indexCFF].length;
    uint32_t absOffset = f->tableRecords[indexCFF].offset;

    int32_t numStack[3];
    int stackTop = 0;
    char buffer[9];

    fseek(f->fontFile, absOffset + 2l, SEEK_SET);
    uint8_t headerSize = fgetc(f->fontFile);
    fseek(f->fontFile, headerSize + 1l, SEEK_CUR);

    /*
     * 读取CFF字体名。OTF用的CFF子集中，Name Index只能包括一个名字，且一定是256字节以内，因此前四个字节一定是00 01 01 01。
     */
    int tmp = fgetc(f->fontFile) - 1;
    fread(f->CIDFontName, tmp, 1, f->fontFile);
    f->CIDFontName[tmp] = '\0';

    // 确定是不是CID字体
    fseek(f->fontFile, 2l, SEEK_CUR);
    tmp = fgetc(f->fontFile); // 偏移量长度
    fseek(f->fontFile, tmp, SEEK_CUR); // 第1个值必定是1，不用考虑
    tmp = readIndexInt(tmp, f->fontFile); // Top DICT的长度
    tmp += ftell(f->fontFile) - 1;
    while (1)
    {
        int cur = fgetc(f->fontFile);
        if (cur > 27) numStack[stackTop++] = readDictInt(cur, f->fontFile);
        else if (cur != 12) return;
        else
        {
            cur = fgetc(f->fontFile);
            if (cur != 30) return;
            break;
        }
    }
    // 此时该字体必然是CID字体
    f->isCID = 1;
    fseek(f->fontFile, tmp, SEEK_SET);
    int numStr = readIndexInt(2, f->fontFile);
    tmp = fgetc(f->fontFile); // 偏移量长度
    numStack[1] -= 391; // 预先定义的字符串中并没有CID相关的，因此一定在string dict里
    fseek(f->fontFile, tmp * numStack[1], SEEK_CUR); // 跳到相应的偏移量处
    int strOffset = readIndexInt(tmp, f->fontFile); // 字符串偏移量
    int strLen = readIndexInt(tmp, f->fontFile) - strOffset; // 字符串长度
    fseek(f->fontFile, tmp * (numStr - numStack[1]) + strOffset - 3, SEEK_CUR); // 跳到字符串的开始
    fread(buffer, 1, strLen, f->fontFile);
    buffer[strLen] = 0;
    for (int i=0; i<5; ++i)
        if (!strcmp(orderings[i], buffer))
        {
            f->ROS = i << 8;
            break;
        }
    f->ROS += numStack[2];
}

// 生成子集化需要的字体名
inline static void subroutineFontName(Font* f)
{
    strcpy(f->T0FontName, f->CIDFontName);
    strcpy(f->CIDFontName + 7, f->T0FontName);
    char* p = f->CIDFontName;
    for (int i = 0; i<2; ++i)
    {
        unsigned tmp = rand();
        for (int j=0; j<3; ++j)
        {
            *p = (tmp % 26) + 'A';
            tmp /= 26;
            ++p;
        }
    }
    f->CIDFontName[6] = '+';
}

// 读取FontDescriptor需要的内容
void readFDContent(Font* f)
{
    uint16_t index = findIndexOfTable(f, "head");
    uint32_t offset = f->tableRecords[index].offset + 36;
    fseek(f->fontFile, offset, SEEK_SET);
    fread(f->BBox, 2, 4, f->fontFile);
    for (int i=0; i<4; ++i)
        switchEndian16(f->BBox + i);

    index = findIndexOfTable(f, "OS/2");
    offset = f->tableRecords[index].offset + 68;
    fseek(f->fontFile, offset, SEEK_SET);
    f->ascent = readUint16BE(f->fontFile);
    f->descent = readUint16BE(f->fontFile);
    fseek(f->fontFile, 16, SEEK_CUR);
    f->capsHeight = readUint16BE(f->fontFile);
}

// 读取SFNT格式的字体
Font* fontFromFile(char* dir, int index)
{
    unsigned hash = hashFromString(dir);
    struct FontNode* current = fontLibrary[hash];
    // 如果没有这个hash值，自然不会进入循环
    while (current)
    {
        if (!strcmp(current->dir, dir)) return &current->current;
        current = current->next;
    }
    struct FontNode* new = malloc(sizeof(struct FontNode));
    // 把新节点加入链表
    new->next = fontLibrary[hash];
    fontLibrary[hash] = new;

    Font* curFont = &new->current;
    curFont->fontFile = fopen(dir, "rb");

    // 读取magic number，确定是不是OTF字体
    uint32_t tmp;
    fread(&tmp, 4, 1, curFont->fontFile);
    if (tmp == 0x66637474U)
    {
        fseek(curFont->fontFile, 8l, SEEK_SET);
        tmp = readUint32BE(curFont->fontFile); // numFonts
        if (tmp <= index) return NULL;
        fseek(curFont->fontFile, index * 4l, SEEK_CUR);
        tmp = readUint32BE(curFont->fontFile); // proper index
        fseek(curFont->fontFile, tmp, SEEK_SET);
        fread(&tmp, sizeof(uint32_t), 1, curFont->fontFile);
    }
    curFont->isOTF = tmp == 0x4F54544F;
    curFont->isCID = 0;

    // 读取各表索引
    curFont->numTables = readUint16BE(curFont->fontFile);
    curFont->tableRecords = malloc(curFont->numTables * sizeof(struct FontTableRecord));
    fseek(curFont->fontFile, 6l, SEEK_CUR);
    fread(curFont->tableRecords, sizeof(struct FontTableRecord), curFont->numTables, curFont->fontFile);
    for (uint32_t* p = (uint32_t*) curFont->tableRecords;
            p < (uint32_t*) curFont->tableRecords + curFont->numTables;
            ++p)
        switchEndian32(p);

    // 如果是OTF字体，则使用CFF表内的名字；顺便确定是否为CID字体
    if (curFont->isOTF) getNameCff(curFont);
    // 如果是TTF字体，则使用name表里的PS名称
    else getNameTtf(curFont);
    if (!curFont->isOTF || !curFont->isCID)
        curFont->ROS = 512; // Adobe-Identity-0

    //subroutineFontName(curFont);

    // 所有字体统一使用Identity-H的CMap（字符编码到cid/gid已经在排版时完成）
    strcpy(curFont->T0FontName, curFont->CIDFontName);
    strcpy(curFont->T0FontName + strlen(curFont->CIDFontName), "-Identity-H");

    // 生成将来font descriptor用的内容
    readFDContent(curFont);

    strcpy(new->dir, dir);
    return curFont;
}