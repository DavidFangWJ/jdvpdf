//
// cffWriter module
// Xin-Shiyu 2021/6/6
//


#include "cffWriter.h"
#include "endianIO.h"

#include <stdlib.h>
#include <assert.h>

CffObjectNode* cffObjectNodeNew(size_t size)
{
    assert(size != 0);
    
    void* data = malloc(size);
    CffObjectNode* ret = (CffObjectNode*)malloc(sizeof(CffObjectNode));
    ret->next = NULL;
    ret->ext.data = data;
    ret->size = size;
    return ret;
}

CffObjectNode* cffObjectNodeFromDict(CffDict* cffDict)
{
    CffObjectNode* node = cffObjectNodeNew(cffDictCalcSize(cffDict));
    uint8_t* o = (uint8_t*)node->ext.data; // output iterator
    for (CffDictItem* it = cffDict->begin; it != cffDict->end; ++it)
    {
        o += cffDictWriteItem(o, it);
    }
    return node;
}

CffObjectNode* cffObjectNodeFromFile(FILE* file, long begin, size_t size)
{
    assert(file != NULL);
    assert(size != 0);

    CffObjectNode* ret = cffObjectNodeNew(size);
    fseek(file, begin, SEEK_SET);
    fread(ret->ext.data, 1, size, file);
    return ret;
}

void cffObjectNodeFree(CffObjectNode* node)
{
    if (node->size != 0) free(node->ext.data);
    free(node);
}

void cffIndexModelConstruct(CffIndexModel* model)
{
    assert(model != NULL);

    model->count = 0;
    model->head = NULL;
    model->tail = NULL;
}

void cffIndexModelDestruct(CffIndexModel* model)
{
    assert(model != NULL);

    CffObjectNode* it = model->head;
    while (it)
    {
        CffObjectNode* node = it;
        it = it->next;
        cffObjectNodeFree(node);
    }
}

void cffIndexModelAppend(CffIndexModel* model, CffObjectNode* node)
{
    assert(model != NULL);
    assert(node != NULL);

    ++model->count;
    model->size += node->size;

    if (!model->head)
    {
        model->head = node;
        model->tail = node;
    }
    else
    {
        model->tail->next = node;
        model->tail = node;
    }
}

void cffIndexModelAppendEmpty(CffIndexModel* model)
{
    assert(model != NULL);
    ++model->count;

    if (!model->head || model->tail->size != 0)
    {
        CffObjectNode* node = (CffObjectNode*)malloc(sizeof(CffObjectNode));
        node->size = 0;
        node->next = NULL;
        node->ext.emptyNodeCount = 1;
        if (!model->head)
        {
            model->head = node;
            model->tail = node;
        }
        else
        {
            model->tail->next = node;
            model->tail = node;
        }
    }
    else
    {
        ++model->tail->ext.emptyNodeCount;
    }
}

void cffIndexModelWriteToFile(CffIndexModel* model, FILE* file)
{
    writeUnsignedToFileBE(file, model->count, sizeof(Card16)); // Card16 count
    OffSize offSize = cffCalcOffSize(model->size + 1); // Note: see cffIndexModelCalcSize
    writeUnsignedToFileBE(file, offSize, sizeof(offSize)); // OffSize offSize

    // Write offset array
    Offset currentOffset = 1;
    for (CffObjectNode* it = model->head; it; it = it->next)
    {
        for (;;)
        {
            writeUnsignedToFileBE(file, currentOffset, offSize);
            if (it->size != 0) break;
            if (--it->ext.emptyNodeCount == 0) break;
        }
        currentOffset += it->size;
    }
    writeUnsignedToFileBE(file, currentOffset, offSize); // "+ 1" in "offset[count + 1]"

    // Write objects
    for (CffObjectNode* it = model->head; it; it = it->next)
    {
        if (it->size != 0) fwrite(it->ext.data, 1, it->size, file);
    }
} 

/**
 * 计算Top DICT所需的长度。by 懒懒
 */
long cffDictCalcSize(CffDict* cffDict)
{
    long length = 0;
    for (CffDictItem* p = cffDict->begin; p != cffDict->end; ++p)
    {
        if (p->type == CFF_DICT_COMMAND) // 命令，1或2字节
            length += (p->content.data > 256) ? 2 : 1;
        else if (p->type == CFF_DICT_INTEGER) // 实数，1～5字节
        {
            if (p->content.data >= -107 && p->content.data <= 107) // 一字节
                ++length;
            else if (p->content.data >= -1131 && p->content.data <= 1131) // 两字节
                length += 2;
            else if (p->content.data >= -32768 && p->content.data <= 32767) // 三字节
                length += 3;
            else length += 5; // 五字节
        }
        else if (p->type == CFF_DICT_REAL) // 实数，字节数不定
            for (uint8_t* c = p->content.str;; ++c)
            {
                ++length;
                if (*c % 16 == 15) break;
            }
    }
    return length;
}

size_t cffDictWriteItem(void* out, CffDictItem* item)
{
    uint8_t* o = (uint8_t*)out;
    size_t diff = 0;
    switch (item->type)
    {

    case CFF_DICT_COMMAND:
    {
        int32_t d = item->content.data;
        if (d > 0xFF)
        {
            o[diff++] = d >> 8;
        }
        o[diff++] = d & 0xFF;
    }
    break;

    case CFF_DICT_INTEGER:
    {
        int32_t d = item->content.data;
        if (d >= -107 && d <= 107)
        {
            o[diff++] = d + 139;
        }
        else if (d >= 108 && d <= 1131)
        {
            o[diff++] = d / 256 + 247;
            o[diff++] = d % 256 - 108;
        }
        else if (d <= 108 && d >= -1131)
        {
            o[diff++] = (-d) / 256 + 251;
            o[diff++] = (-d) + 108;
        }
        else if (d >= -32768 && d <= 32767)
        {
            o[diff++] = 28;
            o[diff++] = d >> 8;
            o[diff++] = d & 0xFF;
        }
        else if (d >= -0x80000000 && d <= 0x80000000 - 1)
        {
            o[diff++] = 29;
            o[diff++] = d >> 24;
            o[diff++] = (d >> 16) & 0xFF;
            o[diff++] = (d >> 8) & 0xFF;
            o[diff++] = d & 0xFF;
        }
    }
    break;

    case CFF_DICT_REAL:
    {
        uint8_t* it = item->content.str;
        do
        {
            o[diff++] = it++;
        } 
        while (*it & 0x0F != 0x0F);
    }
    break;

    } // switch end

    return diff;
}