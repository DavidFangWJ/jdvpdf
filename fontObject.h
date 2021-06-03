//
// Created by david on 2021/5/14.
//

#ifndef JDVPDF_FONTOBJECT_H
#define JDVPDF_FONTOBJECT_H

extern const char* orderings[5];

struct FontTableRecord {
    uint32_t tableTag;
    uint32_t checkSum;
    uint32_t offset;
    uint32_t length;
};

struct _FontObject {
    FILE* fontFile;
    _Bool isOTF;
    _Bool isCID;
    char CIDFontName[64];
    char T0FontName[64];
    uint16_t numTables;
    struct FontTableRecord* tableRecords;
    // 以下用于PDF输出
    uint16_t ROS;
    int16_t BBox[4];
    int16_t ascent;
    int16_t descent;
    int16_t capsHeight;
};

typedef struct _FontObject Font;

void initiateFontLibrary();
void deleteFontLibrary();

uint16_t findIndexOfTable(Font*, const char*);

Font* fontFromFile(char*, int);

#endif //JDVPDF_FONTOBJECT_H
