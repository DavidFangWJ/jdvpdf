#include <stdio.h>
#include <stdint.h>
#include "fontObject.h"
#include "pdfOutput.h"
//#include "jdvReader.h"

int main() {
    //initiateFontLibrary();
    //Font* testFont = fontFromFile("/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",2);
    //Font* testFont2 = fontFromFile("/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",2);
    //Font* testFont3 = fontFromFile("/usr/share/fonts/opentype/freefont/FreeSerif.otf",2);

    FILE* outFile = fopen("test.txt", "wb");
    initiatePdfOutput(outFile);
    //outputFont(testFont, 0);
    finalizePdfOutput();

    //deleteFontLibrary();
    //fclose(outFile);

    //parse1("/home/david/文档/新软件五线谱、简谱排版需求.dvi");

    return 0;
}
