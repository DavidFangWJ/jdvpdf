//
// cffReader module
// Xin-Shiyu 2021/6/5
//


#ifndef JDVPDF_CFFREADER_H
#define JDVPDF_CFFREADER_H

#include "stdint.h"
#include "stdio.h"


// Data Types as defined in Chapter 3 of CFF spec

typedef uint8_t Card8;
typedef uint16_t Card16;
typedef uint8_t OffSize;
// Note: the size of an Offset can be 1, 2, 3 or 4 bytes
// Here we use a 4 byte integer to store it
typedef uint32_t Offset;

// An info type for dealing with an INDEX structure in CFF
typedef struct
{
    FILE* file;
    Card16 count;
    OffSize offSize;
    long offsetArrayInFile; // file offset
    long objectArrayInFile; // file offset
} CffIndex;

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
 * Extracts information of an INDEX
 * Note: affects the file cursor!
 * @param file the file where the INDEX exists
 * @param sectionBegin the offset in file where the INDEX begins
 * @param OUT_cffIndex an out parameter. yields information for seeking objects in the INDEX.
 */
void cffIndexExtractFromFile(FILE* file, long sectionBegin, CffIndex* OUT_cffIndex);

/**
 * Finds the place of an object in an INDEX
 * Note: affects the file cursor!
 * @param cffIndex the INDEX structure to access
 * @param indexInArr the index of the object in the offset array
 * @param OUT_beginOffset an out parameter. yields the object's beginning offset in file
 * @param OUT_length an out parameter. yields the length of the object
 */
void cffIndexFindObject(CffIndex* cffIndex, Offset indexInArr, long* OUT_beginOffset, long* OUT_length);

#endif // JDVPDF_CFFREADER_H