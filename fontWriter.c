//
// Created by david on 2021/6/6.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "fontObject.h"
#include "fontWriter.h"

#include "cffReader.h"
#include "cffWriter.h" // for cff subsetting
#include "endianIO.h"

extern FILE* outFile;

inline static void switchEndian32(uint32_t* num)
{
    static uint8_t c1, c2, c3, c4;
    c1 = *num & 0xFFu;
    c2 = (*num & 0xFF00u) >> 8u;
    c3 = (*num & 0xFF0000u) >> 16u;
    c4 = (*num & 0xFF000000u) >> 24u;
    *num = (c1 << 24u) + (c2 << 16u) + (c3 << 8u) + c4;
}

static void fileCopy(FILE* dest, FILE* src, size_t size)
{
    uint8_t buffer[512];
    while (size != 0)
    {
        size_t diff = size & 512;
        fread(buffer, 1, diff, src);
        fwrite(buffer, 1, diff, dest);
        size -= diff;
    }
}

/**
 * 生成一个CFF字体的子集。
 * @param numGID 一共使用的GID数
 * @param GIDs GID列表，以升序排列。
 * @param f 原字体。
 */
void outputSubsetCFF(size_t numGID, uint16_t* GIDs, Font* f)
{
    uint16_t indexCFF = findIndexOfTable(f, "CFF ");
    uint32_t length = f->tableRecords[indexCFF].length;
    uint32_t fileBegin = f->tableRecords[indexCFF].offset;

    FILE* file = f->fontFile;

    // Entry Header
    fseek(file, fileBegin, SEEK_SET);
    CffHeader header;
    cffHeaderExtract(file, &header);

    // Entry Name INDEX
    CffIndex oldNameIndex;
    cffIndexExtract(file, &oldNameIndex);
    long oldNameIndexSize = cffIndexGetSize(&oldNameIndex);
    cffIndexSkip(&oldNameIndex);

    CffIndexModel newNameIndex;
    CffObjectNode* newNameNode = cffObjectNodeNew(strlen(f->CIDFontName));
    strcpy(newNameNode->ext.data, f->CIDFontName);
    cffIndexModelAppend(&newNameIndex, newNameNode);
    long nameIndexSizeDiff = cffIndexModelCalcSize(&newNameIndex) - oldNameIndexSize;

    CffDict topDict;
    CffIndex topDictIndex;
    cffIndexExtract(file, &topDictIndex);
    long oldTopDictBegin, oldTopDictSize;
    cffIndexFindObject(&topDictIndex, 0, &oldTopDictBegin, &oldTopDictSize);
    fseek(file, oldTopDictBegin, SEEK_SET);
    cffDictConstruct(file, oldTopDictSize, &topDict);

    // According to the Data Layout Chapter,
    // Encoding and CharStrings INDEXs are before CharStrings,
    // which means their offsets only rely on the length of the
    // Top DICT itself;
    // However, the Private DICT exists after CharStrings INDEX
    // and thus should be updated according to the CharStrings size
    // 0 -> pCharsetOffset (15)
    // 1 -> pEncodingOffset (16)
    // 2 -> pCharStringsOffset (17)
    // 3 -> pPrivateOffset (18)
    int32_t* pRefOffset[4] = {0};
    const size_t charset = 0, encoding = 1, charStrings = 2, private = 3;
    for (CffDictItem* it = topDict.begin; it != topDict.end; ++it)
    {
        if (it->type == CFF_DICT_COMMAND)
        {
            CffDictItem* arg = it - 1;
            int32_t op = it->content.data;
            if (15 <= op && op <= 18)
            {
                assert(arg->type == CFF_DICT_INTEGER);
                pRefOffset[op - 15] = &arg->content.data;
            }
        }
    }

    // Subsetting CharStrings
    assert(pRefOffset[charStrings] != NULL);
    long oldCharStringsIndexBegin = *pRefOffset[charStrings];
    fseek(file, oldCharStringsIndexBegin, SEEK_SET);
    CffIndex oldCharStringsIndex;
    cffIndexExtract(file, &oldCharStringsIndex);
    CffIndexModel newCharStringsIndex;
    cffIndexModelConstruct(&newCharStringsIndex);
    uint16_t* itPendingGID = GIDs;
    uint16_t* endPendingGID = GIDs + numGID;
    for (size_t i = 0; i < oldCharStringsIndex.count; ++i)
    {
        if (itPendingGID != endPendingGID && *itPendingGID == i)
        {
            ++itPendingGID;
            long objectBegin, objectLength;
            cffIndexFindObject(&oldCharStringsIndex, i, &objectBegin, &objectLength);
            cffIndexModelAppend(&newCharStringsIndex, cffObjectNodeFromFile(file, objectBegin, objectLength));
        }
        else
        {
            cffIndexModelAppendEmpty(&newCharStringsIndex);
        }
    }

    long charStringsSizeDiff = cffIndexModelCalcSize(&newCharStringsIndex) - cffIndexGetSize(&oldCharStringsIndex);
    for (size_t i = 0; i < 4; ++i)
    {
        if (!pRefOffset[i]) continue;
        *pRefOffset[i] += nameIndexSizeDiff;
        if (i != charStrings && *pRefOffset[i] > pRefOffset[charStrings])
        {
            *pRefOffset[i] += charStringsSizeDiff;
        }
    }

    int resizeAttemptTimes = 0;

    long oldTopDictIndexOffSize = topDictIndex.offSize;
    for (;;) // I don't know if this can really work
    {
        long currentTopDictSize = cffDictCalcSize(&topDict);
        long currentTopDictIndexOffSize = cffCalcOffSize(currentTopDictSize);
        long offsetDiff = 
            currentTopDictSize - oldTopDictSize + 
            (currentTopDictIndexOffSize - oldTopDictIndexOffSize) / 2;
        if (offsetDiff > 0)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                if (pRefOffset[i]) *pRefOffset[i] += offsetDiff;
            }
        }
        else if (offsetDiff < 0)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                if (pRefOffset[i]) *pRefOffset[i] -= offsetDiff;
            }
        }
        else break;
        oldTopDictSize = currentTopDictSize;
        oldTopDictIndexOffSize = currentTopDictIndexOffSize;
        ++resizeAttemptTimes;
        assert(resizeAttemptTimes <= 4);
    }

    // Finally!!!

    fseek(file, fileBegin, SEEK_SET);
    fileCopy(outFile, file, 4); // Header
    cffIndexModelWriteToFile(&newNameIndex, outFile); // Name INDEX
    cffIndexModelDestruct(&newNameIndex);

    CffIndexModel newTopDictIndex;
    cffIndexModelConstruct(&newTopDictIndex);
    cffIndexModelAppend(&newTopDictIndex, cffObjectNodeFromDict(&topDict));
    cffDictDestruct(&topDict);
    cffIndexModelWriteToFile(&newTopDictIndex, outFile);
    cffIndexModelDestruct(&newTopDictIndex);

    fseek(file, oldTopDictBegin + oldTopDictSize, SEEK_SET); // Region between Top DICT and CharStrings INDEX
    fileCopy(outFile, file, oldCharStringsIndexBegin - oldTopDictSize - oldTopDictBegin);

    cffIndexModelWriteToFile(&newCharStringsIndex, outFile);
    cffIndexModelDestruct(&newCharStringsIndex);

    long oldCharStringsIndexEnd = oldCharStringsIndexBegin + cffIndexGetSize(&oldCharStringsIndex); // Region after CharStrings INDEX
    fseek(file, oldCharStringsIndexEnd, SEEK_SET);
    fileCopy(outFile, file, fileBegin + length - oldCharStringsIndexEnd);
}

inline static void readLoca(int locaFormat, uint16_t numGlyphs, FILE* f, struct FontTableRecord* record,
                            uint32_t* dest)
{
    fseek(f, record->offset, SEEK_SET);
    for (int i=0; i<=numGlyphs; ++i)
    {
        if (locaFormat == 0)
            dest[i] = readUnsignedFromFileBE(f, 2) * 2;
        else
            dest[i] = readUnsignedFromFileBE(f, 4);
    }
}

void getNewGlyfLoca(int locaFormat, size_t numGID, uint16_t numGlyphs, uint32_t fileOffset, FILE* in,
                    uint16_t* GIDs, uint32_t* locaOld, uint32_t* locaNew, uint8_t* glyfNew)
{
    uint8_t* pointerGlyf = glyfNew;
    uint16_t* pointerGid = GIDs, *end = GIDs + numGID;

    locaNew[0] = 0;
    for (int i=0; i<numGlyphs; ++i)
    {
        uint32_t curLength = 0;
        if (pointerGid != end && *pointerGid == i) // 用短路逻辑运算符避免可能的段错误？
        {
            curLength = locaOld[GIDs[i+1]] - locaOld[GIDs[i]];
            ++pointerGid;
            if (curLength > 0) // 即使原字体，也可能长度为0
            {
                fseek(in, fileOffset + locaOld[GIDs[i]], SEEK_SET); // 算出偏移量
                fread(pointerGlyf, curLength, 1, in);
                if (locaFormat == 0 && curLength % 2) ++curLength; // 确保偶数
                pointerGlyf += curLength;
            }
        }
        locaNew[i+1] = locaOld[i] + curLength;
    }
}

inline static uint32_t calculateChecksum(int length, const uint8_t* data)
{
    uint32_t checkSums[4] = {0,0,0,0};
    for (int i=0; i<length; ++i)
        checkSums[i%4] += (uint8_t) data[i];
    return (checkSums[0] << 24u) + (checkSums[1] << 16u) + (checkSums[2] << 8u) + checkSums[3];
}

#define NEXT_MULT_OF_4(x) (((x)+3)&~3)
#define GLYF 1
#define HEAD 2
#define LOCA 5
#define MAXP 6

/**
 * 生成一个SFNT字体的子集。
 * @param numGID 一共使用的GID数
 * @param GIDs GID列表，以升序排列。
 * @param f 原字体。
 */
void outputSubsetSFNT(size_t numGID, uint16_t* GIDs, Font* f)
{
    // Truetype字体必需的表有9个：cmap、glyf、head、hhea、hmtx、loca、maxp、name、post。
    unsigned char header[12] = {0,1,0,0, 0,9,      0,128,      0,3,          0,16};
    //                          TTF      numTables searchRange entrySelector rangeShift
    char requiredTag[9][5] = {"cmap", "glyf", "head", "hhea", "hmtx", "loca", "maxp", "name", "post"};
    struct FontTableRecord newRecord[9];
    int oldOffset[9];
    for (int i=0; i<9; ++i)
    {
        int origIndex = findIndexOfTable(f, requiredTag[i]);
        newRecord[i] = f->tableRecords[origIndex];
        oldOffset[i] = newRecord[i].offset;
    }

    // 读取loca表样式及字符数
    fseek(f->fontFile, oldOffset[HEAD] + 50, SEEK_SET);
    int locaFormat = fgetc(f->fontFile);
    fseek(f->fontFile, oldOffset[MAXP] + 4, SEEK_SET);
    uint16_t numGlyphs = readUnsignedFromFileBE(f->fontFile, 2);
    uint32_t* locaOld = malloc((numGlyphs + 1) * sizeof(int));
    uint32_t* locaNew = malloc((numGlyphs + 1) * sizeof(int));

    // 读取旧的loca表
    readLoca(locaFormat, numGlyphs, f->fontFile, newRecord + LOCA, locaOld);

    // 计算偏移量
    int shortOffset = 0;
    int longOffset = 0;
    for (int i=0; i<numGID; ++i)
    {
        uint32_t curLength = locaOld[GIDs[i+1]] - locaOld[GIDs[i]];
        longOffset += curLength;
        shortOffset += curLength;
        if (curLength % 2 == 1)
            shortOffset += 1;
    }
    locaFormat = shortOffset > 131072; // 短式偏移量最大能表示的是65536WORD，即131072B

    // 生成新的glyph和loca表
    uint8_t* glyfNew = calloc(newRecord[GLYF].length, 1); // 默认为0
    getNewGlyfLoca(locaFormat, numGID, numGlyphs, oldOffset[GLYF],
                   f->fontFile, GIDs, locaOld, locaNew, glyfNew);
    newRecord[GLYF].length = locaNew[numGlyphs]; // 新glyf表的长度等于locaNew的最后一项

    // loca表
    newRecord[LOCA].length = (numGlyphs + 1) * (locaFormat ? 4 : 2);
    uint8_t* locaData = calloc(NEXT_MULT_OF_4(newRecord[LOCA].length), 1);
    if (locaFormat) // 长版
    {
        for (int i=0; i<=numGlyphs; ++i)
            switchEndian32(locaNew + i);
        memcpy(locaData, locaNew, 4 * (numGlyphs + 1));
    }
    else for (int i=0; i<=numGlyphs; ++i) // 短版
        {
            locaData[2*i] = locaNew[2*i] >> 9u;
            locaData[2*i+1] = locaNew[2*i] >> 1u;
        }

    // 调整offset
    newRecord[0].offset = 156; // 12+9×16
    for (int i=1; i<9; ++i)
        newRecord[i].offset = newRecord[i-1].offset + NEXT_MULT_OF_4(newRecord[i-1].length);

    // 计算checksum和head表里的checksumAdjustment
    newRecord[GLYF].checkSum = calculateChecksum(newRecord[GLYF].length, glyfNew);
    newRecord[LOCA].checkSum = calculateChecksum(newRecord[LOCA].length, locaData);
    uint32_t checkSum = 0x000D0090; // 前12字节的checkSum
    for (int i=0; i<9; ++i) // 索引及各表
        checkSum += newRecord[i].checkSum;
    for (uint32_t* p = (uint32_t*) newRecord; p < (uint32_t*) newRecord + 9; ++p)
        checkSum += *p;

    // head表
    char* headTable = malloc(NEXT_MULT_OF_4(newRecord[HEAD].length));
    fseek(f->fontFile, oldOffset[HEAD], SEEK_SET);
    fread(headTable, NEXT_MULT_OF_4(newRecord[HEAD].length), 1, f->fontFile);
    *(uint32_t*) (headTable + 8) = 0xB1B0AFBA - checkSum; // 先把checksum adjustment置0

    // 输出
    fwrite(header, 1, 12, outFile);
    for (uint32_t* p = (uint32_t*) newRecord; p < (uint32_t*) newRecord + 9; ++p)
        writeUnsignedToFileBE(outFile, *p, 4);
    for (int i=0; i<9; ++i)
    {
        if (i == GLYF) fwrite(glyfNew, 1, NEXT_MULT_OF_4(newRecord[GLYF].length), outFile);
        else if (i == LOCA) fwrite(locaData, 1, NEXT_MULT_OF_4(newRecord[LOCA].length), outFile);
        else if (i == HEAD) fwrite(headTable, 1, NEXT_MULT_OF_4(newRecord[HEAD].length), outFile);
        else
        {
            char* tmp = calloc(NEXT_MULT_OF_4(newRecord[i].length), 1);
            fseek(f->fontFile, oldOffset[i], SEEK_SET);
            fread(tmp, 1, newRecord[i].length, f->fontFile);
            fwrite(tmp, 1, NEXT_MULT_OF_4(newRecord[i].length), outFile);
            free(tmp);
        }
    }

    // 析构
    free(locaOld);
    free(locaNew);
    free(locaData);
    free(glyfNew);
    free(headTable);
}