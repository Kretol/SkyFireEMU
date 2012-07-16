/**
  \file G3D/source/Image1unorm8.cpp

  \maintainer Morgan McGuire, http://graphics.cs.williams.edu

  \created 2007-01-31
  \edited  2011-09-11
*/

#include "G3D/Image1unorm8.h"
#include "G3D/Image3unorm8.h"
#include "G3D/Image1.h"
#include "G3D/Image.h"
#include "G3D/Color1.h"
#include "G3D/Color1unorm8.h"
#include "G3D/Color4.h"
#include "G3D/Color4unorm8.h"
#include "G3D/ImageFormat.h"

namespace G3D {

Image1unorm8::Image1unorm8(int w, int h, WrapMode wrap) : Map2D<Color1unorm8, Color1>(w, h, wrap) {
    setAll(Color1unorm8(unorm8::zero()));
}


Image1unorm8::Ref Image1unorm8::fromImage3unorm8(const ReferenceCountedPointer<class Image3unorm8>& im) {
    return fromArray(im->getCArray(), im->width(), im->height(), im->wrapMode());
}


Image1unorm8::Ref Image1unorm8::fromImage1(const ReferenceCountedPointer<Image1>& im) {
    Ref out = createEmpty(static_cast<WrapMode>(im->wrapMode()));
    out->copyArray(im->getCArray(), im->width(), im->height());

    return out;
}


Image1unorm8::Ref Image1unorm8::createEmpty(int width, int height, WrapMode wrap) {
    return new Type(width, height, wrap);
}


Image1unorm8::Ref Image1unorm8::createEmpty(WrapMode wrap) {
    return createEmpty(0, 0, wrap);
}


Image1unorm8::Ref Image1unorm8::fromFile(const std::string& filename, WrapMode wrap) {
    Ref out = createEmpty(wrap);
    out->load(filename);
    return out;
}


Image1unorm8::Ref Image1unorm8::fromArray(const class Color3unorm8* ptr, int w, int h, WrapMode wrap) {
    Ref out = createEmpty(wrap);
    out->copyArray(ptr, w, h);
    return out;
}


Image1unorm8::Ref Image1unorm8::fromArray(const class Color1* ptr, int w, int h, WrapMode wrap) {
    Ref out = createEmpty(wrap);
    out->copyArray(ptr, w, h);
    return out;
}


Image1unorm8::Ref Image1unorm8::fromArray(const class Color1unorm8* ptr, int w, int h, WrapMode wrap) {
    Ref out = createEmpty(wrap);
    out->copyArray(ptr, w, h);
    return out;
}


Image1unorm8::Ref Image1unorm8::fromArray(const class Color3* ptr, int w, int h, WrapMode wrap) {
    Ref out = createEmpty(wrap);
    out->copyArray(ptr, w, h);
    return out;
}


Image1unorm8::Ref Image1unorm8::fromArray(const class Color4unorm8* ptr, int w, int h, WrapMode wrap) {
    Ref out = createEmpty(wrap);
    out->copyArray(ptr, w, h);
    return out;
}


Image1unorm8::Ref Image1unorm8::fromArray(const class Color4* ptr, int w, int h, WrapMode wrap) {
    Ref out = createEmpty(wrap);
    out->copyArray(ptr, w, h);
    return out;
}


void Image1unorm8::load(const std::string& filename) {
    Image::Ref image = Image::fromFile(filename);
    if (image->format() != ImageFormat::L8()) {
        image->convertToL8();
    }

    switch (image->format()->code)
    {
        case ImageFormat::CODE_L8:
            copyArray(static_cast<const Color1unorm8*>(image->toImageBuffer()->buffer()), image->width(), image->height());
            break;
        case ImageFormat::CODE_L32F:
            copyArray(static_cast<const Color1*>(image->toImageBuffer()->buffer()), image->width(), image->height());
            break;
        case ImageFormat::CODE_RGB8:
            copyArray(static_cast<const Color3unorm8*>(image->toImageBuffer()->buffer()), image->width(), image->height());
            break;
        case ImageFormat::CODE_RGB32F:
            copyArray(static_cast<const Color3*>(image->toImageBuffer()->buffer()), image->width(), image->height());
            break;
        case ImageFormat::CODE_RGBA8:
            copyArray(static_cast<const Color4unorm8*>(image->toImageBuffer()->buffer()), image->width(), image->height());
            break;
        case ImageFormat::CODE_RGBA32F:
            copyArray(static_cast<const Color4*>(image->toImageBuffer()->buffer()), image->width(), image->height());
            break;
        default:
            debugAssertM(false, "Trying to load unsupported image format");
            break;
    }

    setChanged(true);
}


void Image1unorm8::copyArray(const Color3unorm8* src, int w, int h) {
    resize(w, h);
    int N = w * h;

    Color1unorm8* dst = getCArray();
    for (int i = 0; i < N; ++i) {
        dst[i].value = (src[i].r + src[i].g + src[i].b) / 3;
    }
}

void Image1unorm8::copyArray(const Color3* src, int w, int h) {
    resize(w, h);
    int N = w * h;

    Color1unorm8* dst = getCArray();
    for (int i = 0; i < N; ++i) {
        dst[i] = Color1unorm8(Color1(src[i].average()));
    }
}


void Image1unorm8::copyArray(const Color1unorm8* ptr, int w, int h) {
    resize(w, h);
    System::memcpy(getCArray(), ptr, w * h);
}


void Image1unorm8::copyArray(const Color1* src, int w, int h) {
    resize(w, h);
    int N = w * h;

    Color1unorm8* dst = getCArray();
    for (int i = 0; i < N; ++i) {
        dst[i] = Color1unorm8(src[i]);
    }
}


void Image1unorm8::copyArray(const Color4unorm8* ptr, int w, int h) {
    resize(w, h);
    int N = w * h;

    Color1unorm8* dst = getCArray();
    for (int i = 0; i < N; ++i) {
        dst[i].value = (ptr[i].r + ptr[i].g + ptr[i].b) / 3;
    }
}


void Image1unorm8::copyArray(const Color4* src, int w, int h) {
    resize(w, h);
    int N = w * h;

    Color1unorm8* dst = getCArray();
    for (int i = 0; i < N; ++i) {
        dst[i] = Color1unorm8(Color1(src[i].rgb().average()));
    }
}


/** Saves in any of the formats supported by G3D::GImage. */
void Image1unorm8::save(const std::string& filename) {
    ImageBuffer::Ref buffer = ImageBuffer::create(width(), height(), format(), MemoryManager::create());
    System::memcpy(buffer->buffer(), getCArray(), width() * height() * format()->cpuBitsPerPixel / 8);
    Image::Ref image = Image::fromImageBuffer(buffer);
    image->toFile(filename);
}


const ImageFormat* Image1unorm8::format() const {
    return ImageFormat::L8();
}

} // G3D
