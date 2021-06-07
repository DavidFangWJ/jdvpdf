//
// cffWriter module
// Xin-Shiyu 2021/6/6
//


#include "cffWriter.h"
#include "endianIO.h"

#include <stdlib.h>
#include <assert.h>

CffObjectNode* cffObjectNodeFromFile(FILE* file, size_t size)
{
    assert(file != NULL);
    assert(size != 0);

    void* data = malloc(size);
    fread(data, 1, size, file);
    CffObjectNode* ret = (CffObjectNode*)malloc(sizeof(CffObjectNode));
    ret->next = NULL;
    ret->ext.data = data;
    ret->size = size;
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