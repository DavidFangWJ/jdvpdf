//
// cffReader module
// Xin-Shiyu 2021/6/5
//


#include <assert.h>
#include <stdlib.h>

#include "cffReader.h"
#include "endianIO.h"

void cffHeaderExtract(FILE* file, CffHeader* OUT_cffHeader)
{
    fread(&OUT_cffHeader->major, sizeof(Card8), 1, file);
    fread(&OUT_cffHeader->minor, sizeof(Card8), 1, file);
    fread(&OUT_cffHeader->hdrSize, sizeof(Card8), 1, file);
    fread(&OUT_cffHeader->offSize, sizeof(OffSize), 1, file);
}

void cffIndexExtract(FILE* file, CffIndex* OUT_cffIndex)
{
    assert(file != NULL);
    assert(OUT_cffIndex != NULL);

    Card16 count;
    OffSize offSize;

    OUT_cffIndex->file = file;

    long sectionBegin = ftell(file);
    count = readUnsignedFromFileBE(file, sizeof(count));
    if (count == 0)
    {
        offSize = 0;
    }
    else
    {
        offSize = readUnsignedFromFileBE(file, sizeof(offSize));
    }

    OUT_cffIndex->count = count;
    OUT_cffIndex->offSize = offSize;
    long offsetArrayInFile = ftell(file);
    OUT_cffIndex->offsetArrayInFile = offsetArrayInFile;
    OUT_cffIndex->objectArrayInFile = offsetArrayInFile + count * offSize;
}

void cffIndexFindObject(CffIndex* cffIndex, size_t indexInArr, long* OUT_beginOffset, long* OUT_length)
{
    assert(cffIndex != NULL);
    assert(OUT_beginOffset != NULL);
    assert(OUT_length != NULL);

    assert(0 <= indexInArr && indexInArr < cffIndex->count);

    FILE* file = cffIndex->file;
    OffSize offSize = cffIndex->offSize;

    fseek(file, cffIndex->offsetArrayInFile + indexInArr * offSize, SEEK_SET);

    // Note: these offsets begin from 1
    Offset offsetBegin = readUnsignedFromFileBE(file, offSize); 
    Offset offsetEnd = readUnsignedFromFileBE(file, offSize); // begin of the next is end of this

    *OUT_length = offsetEnd - offsetBegin;
    *OUT_beginOffset = offsetBegin - 1 + cffIndex->objectArrayInFile;
}

long cffIndexGetSize(CffIndex* cffIndex)
{
    assert(cffIndex != NULL);

    if (cffIndex->count == 0)
    {
        return sizeof(cffIndex->count);
    }

    FILE* file = cffIndex->file;
    OffSize offSize = cffIndex->offSize;

    size_t offArrLength = cffIndex->count * offSize;
    fseek(file, cffIndex->offsetArrayInFile + offArrLength, SEEK_SET);
    size_t bodyLength = readUnsignedFromFileBE(file, offSize) - 1;
    
    return 
        sizeof(cffIndex->count) +
        sizeof(cffIndex->offSize) +
        offArrLength +
        bodyLength;
}

void cffIndexSkip(CffIndex* cffIndex)
{
    assert(cffIndex != NULL);

    if (cffIndex->count == 0)
    {
        fseek(cffIndex->file, cffIndex->offsetArrayInFile, SEEK_SET);
        return;
    }

    FILE* file = cffIndex->file;
    OffSize offSize = cffIndex->offSize;

    fseek(file, cffIndex->offsetArrayInFile + cffIndex->count * offSize, SEEK_SET);
    // Note: offset begins from 1
    Offset offsetEnd = readUnsignedFromFileBE(file, offSize);
    fseek(file, cffIndex->objectArrayInFile + offsetEnd - 1, SEEK_SET);
}

static int32_t readDictInt(int32_t current, FILE* f)
{
    if (current < 30)
    {
        size_t numBytes = 1u << (current - 27); // 28时为2位，29时为4位
        return readUnsignedFromFileBE(f, numBytes);
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

/**
 * 从Top DICT INDEX中读取Top DICT的内容。by 懒懒
 */
void cffDictConstruct(FILE* file, long dictLength, CffDict* OUT_cffDict)
{
    assert(file != NULL);
    assert(OUT_cffDict != NULL);

    long beginOffset = ftell(file);
    // 数据和命令至少1字节，因此项数不会超过字节数

    OUT_cffDict->begin = (CffDictItem*)malloc(dictLength * sizeof(CffDictItem));
    CffDictItem* current = OUT_cffDict->begin;

    for (long i = 0; i < dictLength; i = ftell(file) - beginOffset)
    {
        int32_t curByte = fgetc(file);
        if (curByte < 22) // operator
        {
            current->type = CFF_DICT_COMMAND;
            if (curByte == 12) // 两字节
                current->content.data = 0xC00 + fgetc(file);
            else current->content.data = curByte;
        }
        else if (curByte == 30) // 浮点数
        {
            current->type = CFF_DICT_REAL;
            long curPos = ftell(file);
            while ((curByte % 16) != 15) curByte = fgetc(file);
            long length = ftell(file) - curPos;
            current->content.str = malloc(length);
            fseek(file, curPos, SEEK_SET);
            fread(current->content.str, 1, length, file);
        }
        else // 整数
        {
            current->type = CFF_DICT_INTEGER;
            current->content.data = readDictInt(curByte, file);
        }
        ++current;
    }
    OUT_cffDict->end = current;
}

void cffDictDestruct(CffDict* cffDict)
{
    assert(cffDict != NULL);

    for (CffDictItem* it = cffDict->begin; it != cffDict->end; ++it)
    {
        if (it->type == CFF_DICT_REAL)
        {
            free(it->content.str);
        }
    }
    free(cffDict->begin);
}