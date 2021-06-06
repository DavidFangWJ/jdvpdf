//
// cffReader module
// Xin-Shiyu 2021/6/5
//


#include <assert.h>
#include "cffReader.h"

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

    // Note: these offsets begins from 1
    Offset offsetBegin = readUnsignedFromFileBE(file, offSize); 
    Offset offsetEnd = readUnsignedFromFileBE(file, offSize); // begin of the next is end of this

    *OUT_length = offsetEnd - offsetBegin;
    *OUT_beginOffset = offsetBegin - 1 + cffIndex->objectArrayInFile;
}

void cffIndexSkip(CffIndex* cffIndex)
{
    assert(cffIndex != NULL);

    if (cffIndex->count == 0)
    {
        fseek(cffIndex, cffIndex->offsetArrayInFile, SEEK_SET);
        return;
    }

    FILE* file = cffIndex->file;
    OffSize offSize = cffIndex->offSize;

    fseek(file, cffIndex->offsetArrayInFile + cffIndex->count * offSize, SEEK_SET);
    // Note: offset begins from 1
    Offset offsetEnd = readUnsignedFromFileBE(file, offSize);
    fseek(file, cffIndex->objectArrayInFile + offsetEnd - 1, SEEK_SET);
}