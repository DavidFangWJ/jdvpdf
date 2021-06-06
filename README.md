# 这是什么？

这是一个将 JDV 文件（自己加了私货的 DVI）转换为 PDF 的项目，用于将来简谱排版器的后端。

# JDV 文件有什么特点？

和一般的 DVI 相比，JDV 有如下改动：

- 增加了对 OTF/TTF 的支持，但是不再支持 TFM
- 字体定义中必须写全文件路径，以避免在排版和输出时调用两次 fontconfig
- 在字体定义时，双数号字体默认为中文字体，单数号默认为西文字体

# 各模块的意义

## `main.c`
主程序。

## `cffReader.c`/`.h`
读取 CFF 文件。

## `fontObject.c`/`.h`
字体处理用到的文件类型。

## `fontWriter.c`/`.h`
输出（子集化的）CFF/SFNT 格式字体。

## `jdvReader.c`/`.h`
读取 JDV 文件。

## `pdfOutput.c`/`.h`
输出 PDF 文件。
