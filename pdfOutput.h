//
// Created by david on 2021/5/17.
//

#ifndef JDVPDF_PDFOUTPUT_H
#define JDVPDF_PDFOUTPUT_H

void initiatePdfOutput(FILE*);

void outputFont(Font*, _Bool);

void finalizePdfOutput();

#endif //JDVPDF_PDFOUTPUT_H
