//
// Created by david on 2021/5/17.
//

/*
 * PDF文件架构大概这样：
 * 1～3号对象为文件头（1号顶点，2号Pages，3号ProcSet）
 * 4～7号对象用于Symbols这个CID字体，存储各种特殊符号。
 * 一个CID字体需要5个对象存储，按顺序分别为（其他字体同理）
 *     第1个：Type0字体
 *     第2个：CID Type 0字体
 *     第3个：FontDescriptor
 *     第4个：存储字体内容的stream
 *     第5个：stream的长度
 * Symbols因长度已知，且不需要子集化，因此不需要把长度另外占一个object。
 * 自8号对象起，每三个对象对应一页，分别为页面、页面内容（stream）、stream的长度。
 * 在所有页面对象结束之后，储存用到的其他OTF/TTF字体，仍旧用5个对象存储一个字体。
 * 最后一个对象是一个dictionary，它储存了所有字体的名称及其对象索引。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fontObject.h"
#include "pdfOutput.h"

unsigned objCount;

FILE* outFile;

long startByte[512];
char buffer[1024];

#define RECORD_OBJ_POS startByte[objCount++] = ftell(outFile)

extern int numPage;
int numFont;
extern int paperWidth, paperHeight;

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

void initiatePdfOutput(FILE* f)
{
    outFile = f;

    numPage = 5; // 测试用

    // 文件头
    fputs("%PDF-1.4\n", outFile);

    // 1号对象
    fputs("1 0 obj\n<</Type /Catalog /Pages 3 0 R>>\nendobj\n", outFile);

    // 2号对象
    fputs("2 0 obj\n[/PDF /Text]\nendobj\n", outFile);

    // 3号对象
    fputs("3 0 obj\n<</Type /Pages /Kids [", outFile);
    for (int i=0; i<numPage; ++i)
        fprintf(outFile, "%d 0 R ", i*3+8);
    fprintf(outFile, "] /Count %d>>\nendobj\n", numPage);

    // 已经测试出的固定值
    objCount = 3;
    startByte[0] = 9;
    startByte[1] = 56;
    startByte[2] = 84;

}

void outputPage()
{
    // 页面顶
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n<</Type /Page /Parent 3 0 R /MediaBox [0 0 %d %d] /Contents %d 0 R "
                     "/Resources <</ProcSet 2 0 R /Fonts %d 0 R>>\n>>\nendobj\n",
            objCount, paperWidth, paperHeight, objCount + 1, 7 + 3 * numPage + 5 * numFont);

    // 页面内容
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n<</Length %d 0 R>>\nstream\n", objCount, objCount + 1);
    int streamLen = ftell(outFile) * -1;
    // 待完成部分

    streamLen += ftell(outFile);
    fputs("\nendstream\nendobj\n", outFile);

    // 文件长度
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n%d\nendobj\n", objCount, streamLen);
}

struct dictItemOffset {
    char* pos;
    uint32_t num;
};

/**
 * 生成一个CFF字体的子集。辛时雨快来写！
 * @param numGID 一共使用的GID数
 * @param GIDs GID列表，以升序排列。
 * @param f 原字体。
 */
void outputSubsetCFF(size_t numGID, uint16_t* GIDs, Font* f)
{
    uint16_t indexCFF = findIndexOfTable(f, "CFF ");
    uint32_t length = f->tableRecords[indexCFF].length;
    uint32_t absOffset = f->tableRecords[indexCFF].offset;
    static uint8_t header[8] = {1,0,4,4,0,1,1,1};// 文件头和name dic的开头4字节
    uint32_t tmp;
    char* topDictBuffer;

    fwrite(header, 1, 8, outFile);

    // name dict
    tmp = strlen(f->CIDFontName);
    fputc(tmp + 1, outFile);
    fwrite(f->CIDFontName, 1, tmp, outFile);

    // top dict
    fseek(f->fontFile, absOffset + 8, SEEK_SET);
    tmp = fgetc(f->fontFile);

    //tmp = readUint16BE(f->fontFile);
}

inline static void readLoca(int locaFormat, uint16_t numGlyphs, FILE* f, struct FontTableRecord* record,
                            uint32_t* dest)
{
    fseek(f, record->offset, SEEK_SET);
    for (int i=0; i<=numGlyphs; ++i)
    {
        if (locaFormat == 0)
            dest[i] = readUint16BE(f) * 2;
        else
            dest[i] = readUint32BE(f);
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
    uint16_t numGlyphs = readUint16BE(f->fontFile);
    uint32_t* locaOld = malloc((numGlyphs + 1) * sizeof(int));
    uint32_t* locaNew = malloc((numGlyphs + 1) * sizeof(int));

    // 读取旧的loca表
    readLoca(locaFormat, numGlyphs, f->fontFile, newRecord + LOCA, locaOld);

    // 计算偏移量
    int shortOffset = 0;
    int longOffset = 0;
    for (int i=0; i<numGID; ++i)
    {
        int curLength = locaOld[GIDs[i+1]] - locaOld[GIDs[i]];
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
            locaData[2*i] = locaNew[2*i] >> 9;
            locaData[2*i+1] = locaNew[2*i] >> 1;
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
    {
        checkSum += *p;
        switchEndian32(p);
    }

    // head表
    char* headTable = malloc(NEXT_MULT_OF_4(newRecord[HEAD].length));
    fseek(f->fontFile, oldOffset[HEAD], SEEK_SET);
    fread(headTable, NEXT_MULT_OF_4(newRecord[HEAD].length), 1, f->fontFile);
    *(uint32_t*) (headTable + 8) = 0xB1B0AFBA - checkSum; // 先把checksum adjustment置0

    // 输出
    fwrite(header, 1, 12, outFile);
    fwrite(newRecord, 16, 9, outFile);
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

/**
 * 按照是否子集化输出字体。
 * @param f 字体对象
 * @param subset 是否子集化
 */
void outputFont(Font* f, _Bool subset)
{
    // Type0字体
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n<</Type /Font /Subtype /Type0 /BaseFont /%s /Encoding /Identity-H "
                    "/DescendantFonts %d 0 R>>\nendobj\n", objCount, f->T0FontName, objCount + 1);

    // CID字体
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n<</Type /Font /Subtype /CIDFontType%d /BaseFont /%s\n"
                    "/CIDSystemInfo << /Registry (Adobe) /Ordering (%s) /Supplement %d>>\n"
                    "/FontDescriptor %d 0 R>>\nendobj\n", objCount, f->isOTF?0:2, f->CIDFontName,
            orderings[f->ROS / 256], f->ROS % 256, objCount + 1);

    // FontDescriptor
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n<</Type /FontDescriptor /FontName /%s /Flags 4 /FontBBox [%d %d %d %d] "
                    "/ItalicAngle 0 /Ascent %d /Descent %d /CapHeight %d /StemV 0 /FontFile%d %d 0 R>>\n"
                    "endobj\n", objCount, f->CIDFontName, f->BBox[0], f->BBox[1], f->BBox[2], f->BBox[3],
            f->ascent, f->descent, f->capsHeight, f->isOTF?3:2, objCount + 1);

    // 嵌入文件
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n<<", objCount);
    int32_t streamLen = 0;
    if (f->isOTF)
    {
        fprintf(outFile, "/Length %d 0 R /Subtype /CIDFontType0C>>\nstream\n", objCount + 1);
        streamLen = ftell(outFile) * -1;
        outputSubsetCFF(0, NULL, f);
    }
    else
    {
        fprintf(outFile, "/Length %d 0 R /Length1 %d 0 R>>\nstream\n", objCount + 1, objCount + 1);
        streamLen = ftell(outFile) * -1;
        outputSubsetSFNT(0, NULL, f);
    }
    streamLen += ftell(outFile);
    fputs("\nendstream\nendobj\n", outFile);

    // 文件长度
    RECORD_OBJ_POS;
    fprintf(outFile, "%d 0 obj\n%d\nendobj\n", objCount, streamLen);
}

void finalizePdfOutput()
{
    // 输出交叉引用表
    long xrefPos = ftell(outFile);
    fprintf(outFile, "xref\n0 %d\n0000000000 65535 f\n", objCount + 1);
    for (int i=0; i<objCount; ++i)
        fprintf(outFile, "%010ld 00000 n\n", startByte[i]);

    // 输出trailer
    fprintf(outFile, "trailer\n<</Size %d /Root 1 0 R>>\nstartxref\n%ld\n%%%%EOF",
            objCount + 1, xrefPos);
}