//
// cffReader module
// Xin-Shiyu 2021/6/5
//


#include "assert.h"
#include "cffReader.h"

void cffIndexExtractFromFile(FILE* file, long sectionBegin, CffIndex* OUT_cffIndex)
{
    assert(OUT_cffIndex != NULL);

    Card16 count;
    OffSize offSize;

    fseek(file, sectionBegin, SEEK_SET);
    count = readUnsignedFromFileBE(file, sizeof(count));
    offSize = readUnsignedFromFileBE(file, sizeof(offSize));

    OUT_cffIndex->file = file;
    OUT_cffIndex->count = count;
    OUT_cffIndex->offSize = offSize;
    OUT_cffIndex->offsetArrayInFile = sectionBegin + sizeof(count) + sizeof(offSize);
    OUT_cffIndex->objectArrayInFile = offSize + count * offSize;
}

void cffIndexFindObject(CffIndex* cffIndex, Offset indexInArr, long* OUT_beginOffset, long* OUT_length)
{
    FILE* file = cffIndex->file;
    OffSize offSize = cffIndex->offSize;

    fseek(file, cffIndex->offsetArrayInFile + indexInArr * offSize, SEEK_SET);

    // Note: these offsets begins from 1
    Offset offsetBegin = readUnsignedFromFileBE(file, offSize); 
    Offset offsetEnd = readUnsignedFromFileBE(file, offSize); // begin of the next is end of this

    *OUT_length = offsetEnd - offsetBegin;
    *OUT_beginOffset = offsetBegin - 1 + cffIndex->objectArrayInFile;
}