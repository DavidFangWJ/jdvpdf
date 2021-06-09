//
// cffCommon module
// Xin-Shiyu 2021/6/6
//

#ifndef JDVPDF_CFFCOMMON_H
#define JDVPDF_CFFCOMMON_H

#include <stdint.h>

// Data Types as defined in Chapter 3 of CFF spec
typedef uint8_t Card8;
typedef uint16_t Card16;
typedef uint8_t OffSize;
// Note: the size of an Offset can be 1, 2, 3 or 4 bytes
// Here we use a 4 byte integer to store it
typedef uint32_t Offset;

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

#endif // JDVPDF_CFFCOMMON_H