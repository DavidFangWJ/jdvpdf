//
// cffReader module
// Xin-Shiyu 2021/6/5
//


#ifndef JDVPDF_CFFREADER_H
#define JDVPDF_CFFREADER_H

#include <stdint.h>
#include <stdio.h>

#include "cffCommon.h"

typedef struct
{
    Card8 major;
    Card8 minor;
    Card8 hdrSize;
    OffSize offSize;
} CffHeader;

/**
 * Extracts a header
 * Note: affects the file cursor!
 * @param file the file where the header exists
 * @param OUT_cffHeader an out parameter. yields the header
 */
void cffHeaderExtract(FILE* file, CffHeader* OUT_cffHeader);

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
 * Extracts information of an INDEX
 * Note: affects the file cursor!
 * @param file the file where the INDEX exists
 * @param OUT_cffIndex an out parameter. yields information for seeking objects in the INDEX.
 */
void cffIndexExtract(FILE* file, CffIndex* OUT_cffIndex);

/**
 * Finds the place of an object in an INDEX
 * Note: affects the file cursor!
 * @param cffIndex the INDEX structure to access
 * @param indexInArr the index of the object in the offset array
 * @param OUT_beginOffset an out parameter. yields the object's beginning offset in file
 * @param OUT_length an out parameter. yields the length of the object
 */
void cffIndexFindObject(CffIndex* cffIndex, size_t indexInArr, long* OUT_beginOffset, long* OUT_length);

/**
 * Gets the physical size of the INDEX structure
 * Note: affects the file cursor!
 * @param cffIndex the INDEX structure whose size is to be got
 * @returns the size of the INDEX
 */
long cffIndexGetSize(CffIndex* cffIndex);

/**
 * Skips the given INDEX
 * Note: affects the file cursor!
 * @param cffIndex the INDEX structure to skip
 */
void cffIndexSkip(CffIndex* cffIndex);

#define CFF_DICT_INTEGER 0
#define CFF_DICT_REAL    1
#define CFF_DICT_COMMAND 2

typedef struct {
    uint8_t type;
    union {
        int32_t  data;
        uint8_t* str;
    } content;
} CffDictItem;

typedef struct
{
    CffDictItem* begin;
    CffDictItem* end;
} CffDict;

/**
 * Constructs a DICT from a file
 * Note: affects the file cursor!
 * Note: the CffDict should be properly destructed later!
 * Author: 懒懒
 * @param file the file where the dict exists
 * @param size the size of the dict in file
 * @param OUT_cffDict an out parameter. yields the cffDict
 */
void cffDictConstruct(FILE* file, long size, CffDict* OUT_cffDict);

/**
 * Calculates the actual size of a DICT
 * @param cffDict the DICT whose size is to be calculated
 * @returns the size
 */
long cffDictCalcSize(CffDict* cffDict);

/**
 * Destructs a CffDict
 * @param cffDict the CffDict to be destructed
 */
void cffDictDestruct(CffDict* cffDict);


#endif // JDVPDF_CFFREADER_H