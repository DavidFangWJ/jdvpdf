//
// Created by david on 2021/6/6.
//

#ifndef JDVPDF_FONTWRITER_H
#define JDVPDF_FONTWRITER_H

#include "stdint.h"
#include "fontObject.h"

void outputSubsetCFF(size_t, uint16_t*, Font*);
void outputSubsetSFNT(size_t, uint16_t*, Font*);

#endif //JDVPDF_FONTWRITER_H
