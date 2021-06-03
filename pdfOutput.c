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

/**
 * 生成一个SFNT字体的子集。辛时雨快来写！
 * @param numGID 一共使用的GID数
 * @param GIDs GID列表，以升序排列。
 * @param f 原字体。
 */
void outputSubsetSFNT(size_t numGID, uint16_t* GIDs, Font* f)
{

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