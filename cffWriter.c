//
// cffWriter module
// Xin-Shiyu 2021/6/6
//


#include "cffWriter.h"

#include <stdlib.h>
#include <assert.h>

CffObjectNode* cffObjectNodeFromFile(FILE* file, size_t size)
{
    assert(file != NULL);

    void* data = malloc(size);
    fread(data, 1, size, file);
    CffObjectNode* ret = (CffObjectNode*)malloc(sizeof(CffObjectNode));
    ret->next = NULL;
    ret->data = data;
    ret->size = size;
    return ret;
}

void cffObjectNodeFree(CffObjectNode* node)
{
    free(node->data);
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

void cffIndexModelWriteToFile(CffIndexModel* model, FILE* file)
{
    writeUnsignedToFileBE(file, model->count, sizeof(Card16)); // Card16 count
    OffSize offSize = cffCalcOffSize(model->size + 1); // Note: see cffIndexModelCalcSize
    writeUnsignedToFileBE(file, offSize, sizeof(offSize)); // OffSize offSize

    // Write offset array
    Offset currentOffset = 1;
    for (CffObjectNode* it = model->head; it; it = it->next)
    {
        writeUnsignedToFileBE(file, currentOffset, offSize);
        currentOffset += it->size;
    }
    writeUnsignedToFileBE(file, currentOffset, offSize); // "+ 1" in "offset[count + 1]"

    // Write objects
    for (CffObjectNode* it = model->head; it; it = it->next)
    {
        fwrite(it->data, 1, it->size, file);
    }
} 