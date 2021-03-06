//
// cffWriter module
// Xin-Shiyu 2021/6/6
//


#ifndef JDVPDF_CFFWRITER_H
#define JDVPDF_CFFWRITER_H

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "cffCommon.h"

/**
 * Caculates the offSize to be used according to a max offset
 * @param offset the max offset
 * @returns the offSize to be used
 */
inline OffSize cffCalcOffSize(Offset offset)
{
    if (offset < (1 << (8 * 1))) return 1;
    if (offset < (1 << (8 * 2))) return 2;
    if (offset < (1 << (8 * 3))) return 3;
    return 4;
}

// Represents an object in an INDEX
typedef struct CffObjectNode_
{
    size_t size;
    union
    {
        void* data; // living when size != 0
        uintptr_t emptyNodeCount; // living when size == 0
    } ext;
    struct CffObjectNode_* next;
} CffObjectNode;

/**
 * Creates an object node for an object of the given size
 * @param size the size of the object
 */
CffObjectNode* cffObjectNodeNew(size_t size);

/**
 * Creates an object node with proper format from a DICT representation
 * @param cffDict the DICT representation
 */
CffObjectNode* cffObjectNodeFromDict(CffDict* cffDict);

/**
 * Creates an object node from a slice of file
 * Note: the return value should be properly freed!
 * Note: affects the file cursor!
 * @param file the file where the slice exists
 * @param begin the beginning of the slice
 * @param size the size of the slice
 * @returns A pointer to the newly created object node
 */
CffObjectNode* cffObjectNodeFromFile(FILE* file, long begin, size_t size);

/**
 * Frees an object node
 * @param node node to be freed
 */
void cffObjectNodeFree(CffObjectNode* node);

// Represents an INDEX structure
// should be allocated on stack
typedef struct
{
    size_t size;
    Card16 count;
    CffObjectNode* head;
    CffObjectNode* tail;
} CffIndexModel;

/**
 * Constructs a CffIndexModel
 * @param model the model to be constructed
 */
void cffIndexModelConstruct(CffIndexModel* model);

/**
 * Destructs a CffIndexModel
 * @param model the model to be destructed
 */
void cffIndexModelDestruct(CffIndexModel* model);

/**
 * Appends an object node to an INDEX model
 * @param model the model to be appended to
 * @param node the node to be appended
 */
void cffIndexModelAppend(CffIndexModel* model, CffObjectNode* node);

/**
 * Appends an empty object to an INDEX model
 * @param model the model to be appended to
 */
void cffIndexModelAppendEmpty(CffIndexModel* model);

/**
 * Calculates the estimated size of an INDEX
 * @param model the INDEX model whose size is to be calculated
 * @returns the size
 */
inline long cffIndexModelCalcSize(CffIndexModel* model)
{
    // According to the spec: 
    // - Offsets in the offset array are relative to the byte
    // that precedes the object data;
    // - An additional offset is added at the end of the offset
    // array so the length of the last object may be determined.
    //
    // That is to say, if there is only one object of size 1,
    // Then the offsets are 1 and 2, the second pointing to after
    // the end of the last object.
    // It can be seen that 1 is the smallest offset, and the
    // biggest offset is 1 + size;
    OffSize offsize = cffCalcOffSize(1 + model->size);
    return 
        sizeof(Card16) + // Card16 count
        sizeof(OffSize) + // OffSize offSize
        offsize * (model->count + 1) + // Offset offset[count + 1]
        model->size;
}

/**
 * Writes the INDEX structure to file in proper format
 * @param model INDEX model to be written
 * @param file file to be written to
 */
void cffIndexModelWriteToFile(CffIndexModel* model, FILE* file);

/**
 * Calculates the actual size of a DICT
 * @param cffDict the DICT whose size is to be calculated
 * @returns the size
 */
long cffDictCalcSize(CffDict* cffDict);

/**
 * Writes a DICT item in proper format into a byte buffer
 * @param out the output iterator
 * @param item the item
 * @returns the size of the item
 */
size_t cffDictWriteItem(void* out, CffDictItem* item);

#endif // JDVPDF_CFFWRITER_H