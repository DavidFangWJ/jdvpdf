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

#endif // JDVPDF_CFFCOMMON_H