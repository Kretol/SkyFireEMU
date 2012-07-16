/**
  \file G3D/ImageBuffer.h
 
  Copyright 2000-2012, Morgan McGuire.
  All rights reserved.
 */
#ifndef G3D_ImageBuffer_h
#define G3D_ImageBuffer_h

#include "G3D/MemoryManager.h"
#include "G3D/ReferenceCount.h"
#include "G3D/ImageFormat.h"

namespace G3D {

/**
    \brief Provides a general container for transferring CPU pixel data.

    ImageBuffer intentionally does not provide operations on the data. See G3D::Image
    for that.  See G3D::Texture for GPU pixel data.
 */
class ImageBuffer : public ReferenceCountedObject {
public:
    typedef ReferenceCountedPointer<ImageBuffer> Ref;

private:
    MemoryManager::Ref  m_memoryManager;
    void*               m_buffer;

    const ImageFormat*  m_format;
    int                 m_rowAlignment;
    int                 m_rowStride;

    int                 m_width;
    int                 m_height;
    int                 m_depth;

    ImageBuffer(const ImageFormat* format, int width, int height, int depth, int rowAlignment);

    void allocateBuffer(MemoryManager::Ref memoryManager);
    void freeBuffer();

public:
    /** Creates an empty ImageBuffer. */
    static Ref create(int width, int height, const ImageFormat* format, MemoryManager::Ref memoryManager = MemoryManager::create(), int depth = 1, int rowAlignment = 1);

    virtual ~ImageBuffer();

    const ImageFormat* format() const   { return m_format; }

    /** Returns entire size of pixel data in bytes. */
    int size() const                    { return m_height * m_depth * m_rowStride; }

    /** Returns alignment of each row of pixel data in bytes. */
    int rowAlignment() const            { return m_rowAlignment; }

    /** Returns size of each row of pixel data in bytes. */
    int stride() const                  { return m_rowStride; }

    int width() const                   { return m_width; }
    int height() const                  { return m_height; }
    int depth() const                   { return m_depth; }

    /** Returns pointer to raw pixel data */
    void* buffer()                      { return m_buffer; }

    /** Returns pointer to raw pixel data */
    const void* buffer() const          { return m_buffer; }

    /** Return row to raw pixel data at start of row \a y of depth \a d. */
    void* row(int y, int d = 0) {
        debugAssert(y < m_height && d < m_depth);
        return static_cast<uint8*>(m_buffer) + (d * m_height * m_rowStride) + (y * m_rowStride); 
    }

    /** Return row to raw pixel data at start of row \a y of depth \a d. */
    const void* row(int y, int d = 0) const {
        debugAssert(y < m_height && d < m_depth);
        return static_cast<uint8*>(m_buffer) + (d * m_height * m_rowStride) + (y * m_rowStride); 
    }
};

} // namespace G3D

#endif // G3D_IMAGEBUFFER_H
