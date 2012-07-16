/**
  \file Image.cpp
  \author Corey Taylor
  Copyright 2002-2012, Morgan McGuire

  \created 2002-05-27
  \edited  2012-04-21
 */

#include "FreeImagePlus.h"
#include "G3D/platform.h"
#include "G3D/BinaryInput.h"
#include "G3D/Image.h"


namespace G3D {

static const ImageFormat* determineImageFormat(const fipImage* image);
static FREE_IMAGE_TYPE determineFreeImageType(const ImageFormat* imageFormat);

Image::Image()
    : m_image(NULL)
    , m_format(ImageFormat::AUTO()) {

    // todo: if g3d ever has a global init, then this would move there to avoid deinitializing before program exit
    initFreeImage();
    m_image = new fipImage;
}

Image::~Image() {
    if (m_image) {
        delete m_image;
    }
    // This call can deinitialize the plugins if it's the last reference, but they can be re-initialized
    // disabled for now -- initialize FreeImage once in a thread-safe manner, then leave initialize
    //FreeImage_DeInitialise();
}


Image::Ref Image::create(int width, int height, const ImageFormat* imageFormat) {
    FREE_IMAGE_TYPE fiType = determineFreeImageType(imageFormat);
    debugAssertM(fiType != FIT_UNKNOWN, G3D::format("Trying to create Image from unsupported ImageFormat (%s)", imageFormat->name().c_str()));

    if (fiType == FIT_UNKNOWN) {
        // todo: replace Image::Error handling with isNull() and notNull() checks in 
        throw Image::Error(G3D::format("Trying to create Image from unsupported ImageFormat (%s)", imageFormat->name().c_str()));
        return NULL;
    }

    Image* img = new Image();
    if (! img->m_image->setSize(fiType, width, height, imageFormat->cpuBitsPerPixel)) {
        delete img;

        // todo: replace Image::Error handling with isNull() and notNull() checks in 
        throw Image::Error(G3D::format("Unable to allocate FreeImage buffer from ImageFormat (%s)", imageFormat->name().c_str()));
        return NULL;
    }

    img->m_format = imageFormat;

    return img;
}


GMutex Image::s_freeImageMutex;

void Image::initFreeImage() {
    GMutexLock lock(&s_freeImageMutex);
    static bool hasInitialized = false;

    if (! hasInitialized) {
        FreeImage_Initialise();
        hasInitialized = true;
    }
}

bool Image::fileSupported(const std::string& filename, bool allowCheckSignature) {
    initFreeImage();

    bool knownFormat = false;

    if (allowCheckSignature) {
        knownFormat = (FreeImage_GetFileType(filename.c_str(), 0) != FIF_UNKNOWN);
    }
    
    if (! knownFormat) {
        knownFormat = (FreeImage_GetFIFFromFilename(filename.c_str()) != FIF_UNKNOWN);
    }
    
    return knownFormat;
}

Image::Ref Image::fromFile(const std::string& filename, const ImageFormat* imageFormat) {
    debugAssertM(fileSupported(filename, true), G3D::format("Image file format not supported! (%s)", filename.c_str()));
    // Use BinaryInput to allow reading from zip files
    BinaryInput bi(filename, G3D::G3D_LITTLE_ENDIAN);
    return fromBinaryInput(&bi, imageFormat);
}

Image::Ref Image::fromBinaryInput(BinaryInput* bi, const ImageFormat* imageFormat) {
    Image* img = new Image;
    
    fipMemoryIO memoryIO(const_cast<uint8*>(bi->getCArray() + bi->getPosition()), static_cast<DWORD>(bi->getLength() - bi->getPosition()));
    if (! img->m_image->loadFromMemory(memoryIO))
    {
        delete img;

        // todo: replace Image::Error handling with isNull() and notNull() checks in 
        throw Image::Error("Unsupported file format or unable to allocate FreeImage buffer", bi->getFilename());
        return NULL;
    }

    const ImageFormat* detectedFormat = determineImageFormat(img->m_image);
    
    if (! detectedFormat) {
        delete img;

        // todo: replace Image::Error handling with isNull() and notNull() checks in 
        throw Image::Error("Loaded image pixel format does not map to any existing ImageFormat", bi->getFilename());
        return NULL;
    }
    
    if (imageFormat == ImageFormat::AUTO()) {
        img->m_format = detectedFormat;
    } else {
        debugAssert(detectedFormat->canInterpretAs(imageFormat));
        if (! detectedFormat->canInterpretAs(imageFormat)) {
            delete img;

            // todo: replace Image::Error handling with isNull() and notNull() checks in 
            throw Image::Error(G3D::format("Loaded image pixel format is not compatible with requested ImageFormat (%s)", imageFormat->name().c_str()), bi->getFilename());
            return NULL;
        }
        img->m_format = imageFormat;
    }

    // Convert palettized images so row data can be copied easier
    if (img->m_image->getColorType() == FIC_PALETTE) {
        switch (img->m_image->getBitsPerPixel()) 
        {
            case 1:
                img->convertToL8();
                break;

            case 8:
                img->convertToL8();
                break;

            case 24:
                img->convertToRGB8();
                break;

            case 32:
                img->convertToRGBA8();
                break;

            default:
                delete img;

                throw Image::Error("Loaded image data in unsupported palette format", bi->getFilename());
                return NULL;
        }
    }
    
    return img;
}


Image::Ref Image::fromImageBuffer(const ImageBuffer::Ref& buffer) {
    Image::Ref img = create(buffer->width(), buffer->height(), buffer->format());

    BYTE* pixels = img->m_image->accessPixels();
    debugAssert(pixels);

    if (pixels) {
        int rowStride = buffer->width() * (buffer->format()->cpuBitsPerPixel / 8);

        for (int row = 0; row < buffer->height(); ++row) {
            System::memcpy(img->m_image->getScanLine(row), buffer->row(buffer->height() - row - 1), rowStride);
        }
    }

    return img;
}


void Image::toFile(const std::string& filename) const {
    if (! m_image->save(filename.c_str())) {
        debugAssertM(false, G3D::format("Failed to write image to %s", filename.c_str()));
    }
}

// Helper for FreeImageIO to allow seeking
struct _FIBinaryOutputInfo {
    BinaryOutput* bo;
    int64         startPos;
};

// FreeImageIO implementation for writing to BinaryOutput
static unsigned _FIBinaryOutputWrite(void *buffer, unsigned int size, unsigned count, fi_handle handle) {
    _FIBinaryOutputInfo* info = static_cast<_FIBinaryOutputInfo*>(handle);
    
    // Write 'size' number of bytes from 'buffer' for 'count' times
    unsigned numItems = count;
    while (numItems != 0) {
        info->bo->writeBytes(buffer, size);
        --numItems;
    }

    return count;
}

// FreeImageIO implementation for writing to BinaryOutput
static int _FIBinaryOutputSeek(fi_handle handle, long offset, int origin) {
    _FIBinaryOutputInfo* info = static_cast<_FIBinaryOutputInfo*>(handle);

    switch (origin)
    {
        case SEEK_SET:
        {
            info->bo->setPosition(info->startPos + offset);
            break;
        }

        case SEEK_END:
        {
            int64 oldLength = info->bo->length();
            if (offset > 0) {
                info->bo->setLength(oldLength + offset);
            }

            info->bo->setPosition(oldLength + offset);
            break;
        }

        case SEEK_CUR:
        {
            info->bo->setPosition(info->bo->position() + offset);
            break;
        }

        default:
            return -1;
            break;
    }

    return 0;
}

// FreeImageIO implementation for writing to BinaryOutput
static long _FIBinaryOutputTell(fi_handle handle) {
    _FIBinaryOutputInfo* info = static_cast<_FIBinaryOutputInfo*>(handle);

    return static_cast<long>(info->bo->position() - info->startPos);
}

void Image::toBinaryOutput(BinaryOutput* bo, const std::string& fileFormat) const {
    // todo: implement FreeImageIO helpers that wrap BinaryOutput, needs to be thread-safe
    FreeImageIO fiIO;
    fiIO.read_proc = NULL;
    fiIO.seek_proc = _FIBinaryOutputSeek;
    fiIO.tell_proc = _FIBinaryOutputTell;
    fiIO.write_proc = _FIBinaryOutputWrite;

    _FIBinaryOutputInfo info;
    info.bo = bo;
    info.startPos = bo->position();

    if (! m_image->saveToHandle(FreeImage_GetFIFFromFormat(toUpper(fileFormat).c_str()), &fiIO, &info)) {
        debugAssertM(false, "Failed to write image to BinaryOutput");
    }
}


ImageBuffer::Ref Image::toImageBuffer() const {
    ImageBuffer::Ref buffer = ImageBuffer::create(m_image->getWidth(), m_image->getHeight(), m_format, AlignedMemoryManager::create(), 1, 1);

    BYTE* pixels = m_image->accessPixels();
    if (pixels) {
        int rowStride = buffer->width() * (m_format->cpuBitsPerPixel / 8);

        for (int row = 0; row < buffer->height(); ++row) {
            System::memcpy(buffer->row(row), m_image->getScanLine(buffer->height() - row - 1), rowStride);
        }
    }

    return buffer;
}


Image::Ref Image::clone() const {
    Image* c = new Image;
    *(c->m_image) = *m_image;
    c->m_format = m_format;
    return c;
}


int Image::width() const {
    return m_image->getWidth();
}


int Image::height() const {
    return m_image->getHeight();
}


const ImageFormat* Image::format() const {
    return m_format;
}


void Image::flipVertical() {
    m_image->flipVertical();
}


void Image::flipHorizontal() {
    m_image->flipHorizontal();
}


void Image::rotateCW(double radians, int numRotations) {
    while (numRotations > 0) {
        m_image->rotate(toDegrees(radians));
        --numRotations;
    }
}


void Image::rotateCCW(double radians, int numRotations) {
    rotateCW(radians * -1.0, numRotations);
}


bool Image::convertToL8() {
    if (m_image->convertToGrayscale()) {
        m_format = ImageFormat::L8();
        return true;
    }
    return false;
}


bool Image::convertToRGB8() {
    if (m_image->convertTo24Bits()) {
        m_format = ImageFormat::RGB8();
        return true;
    }
    return false;
}


bool Image::convertToRGBA8() {
    if (m_image->convertTo32Bits()) {
        m_format = ImageFormat::RGBA8();
        return true;
    }
    return false;
}


void Image::get(const Point2int32& pos, Color4& color) const {
    Point2int32 fipPos(pos.x, m_image->getHeight() - pos.y - 1);

    BYTE* scanline = m_image->getScanLine(fipPos.y);
    switch (m_image->getImageType()) {
        case FIT_BITMAP:
        {
            if (m_image->isGrayscale()) {
                color.r = (scanline[fipPos.x] / 255.0f);
                color.g = color.r;
                color.b = color.r;
                color.a = 1.0f;
            } else if (m_image->getBitsPerPixel() == 24) {
                scanline += 3 * fipPos.x;

                color.r = (scanline[FI_RGBA_RED] / 255.0f);
                color.g = (scanline[FI_RGBA_GREEN] / 255.0f);
                color.b = (scanline[FI_RGBA_BLUE] / 255.0f);
                color.a = 1.0f;
            } else if (m_image->getBitsPerPixel() == 32) {
                scanline += 4 * fipPos.x;

                color.r = (scanline[FI_RGBA_RED] / 255.0f);
                color.g = (scanline[FI_RGBA_GREEN] / 255.0f);
                color.b = (scanline[FI_RGBA_BLUE] / 255.0f);
                color.a = (scanline[FI_RGBA_ALPHA] / 255.0f);
            }
            break;
        }

        default:
            debugAssertM(false, G3D::format("Image::get does not support pixel format (%s)", m_format->name().c_str()));
            break;
    }
}


void Image::get(const Point2int32& pos, Color3& color) const {
    Color4 c;
    get(pos, c);
    color = Color3(c.r, c.g, c.b);
}


void Image::get(const Point2int32& pos, Color4unorm8& color) const {
    Point2int32 fipPos(pos.x, m_image->getHeight() - pos.y - 1);

    BYTE* scanline = m_image->getScanLine(fipPos.y);
    switch (m_image->getImageType()) {
        case FIT_BITMAP:
        {
            if (m_image->isGrayscale()) {
                color.r = unorm8::fromBits(scanline[fipPos.x]);
                color.g = color.r;
                color.b = color.r;
                color.a = unorm8::fromBits(255);
            } else if (m_image->getBitsPerPixel() == 24) {
                scanline += 3 * fipPos.x;

                color.r = unorm8::fromBits(scanline[FI_RGBA_RED]);
                color.g = unorm8::fromBits(scanline[FI_RGBA_GREEN]);
                color.b = unorm8::fromBits(scanline[FI_RGBA_BLUE]);
                color.a = unorm8::fromBits(255);
            } else if (m_image->getBitsPerPixel() == 32) {
                scanline += 4 * fipPos.x;

                color.r = unorm8::fromBits(scanline[FI_RGBA_RED]);
                color.g = unorm8::fromBits(scanline[FI_RGBA_GREEN]);
                color.b = unorm8::fromBits(scanline[FI_RGBA_BLUE]);
                color.a = unorm8::fromBits(scanline[FI_RGBA_ALPHA]);
            }
            break;
        }

        default:
            debugAssertM(false, G3D::format("Image::get does not support pixel format (%s)", m_format->name().c_str()));
            break;
    }
}


void Image::get(const Point2int32& pos, Color3unorm8& color) const {
    Color4unorm8 c;
    get(pos, c);
    color = c.rgb();
}

void Image::get(const Point2int32& pos, Color1unorm8& color) const {
    Color4unorm8 c;
    get(pos, c);
    // todo (Image upgrade): investigate adding average() to Coor3unorm8 to allow c.rgb().average()
    color.value = c.r;
}

void Image::set(const Point2int32& pos, const Color4& color) {
    Point2int32 fipPos(pos.x, m_image->getHeight() - pos.y - 1);

    BYTE* scanline = m_image->getScanLine(fipPos.y);
    switch (m_image->getImageType())
    {
        case FIT_BITMAP:
        {
            if (m_image->isGrayscale()) {
                scanline[fipPos.x] = static_cast<BYTE>(iClamp(static_cast<int>(color.r * 255.0f), 0, 255));
            } else if (m_image->getBitsPerPixel() == 24) {
                scanline += 3 * fipPos.x;

                scanline[FI_RGBA_RED] = static_cast<BYTE>(iClamp(static_cast<int>(color.r * 255.0f), 0, 255));
                scanline[FI_RGBA_GREEN] = static_cast<BYTE>(iClamp(static_cast<int>(color.g * 255.0f), 0, 255));
                scanline[FI_RGBA_BLUE] = static_cast<BYTE>(iClamp(static_cast<int>(color.b * 255.0f), 0, 255));
            } else if (m_image->getBitsPerPixel() == 32) {
                scanline += 4 * fipPos.x;

                scanline[FI_RGBA_RED] = static_cast<BYTE>(iClamp(static_cast<int>(color.r * 255.0f), 0, 255));
                scanline[FI_RGBA_GREEN] = static_cast<BYTE>(iClamp(static_cast<int>(color.g * 255.0f), 0, 255));
                scanline[FI_RGBA_BLUE] = static_cast<BYTE>(iClamp(static_cast<int>(color.b * 255.0f), 0, 255));
                scanline[FI_RGBA_ALPHA] = static_cast<BYTE>(iClamp(static_cast<int>(color.a * 255.0f), 0, 255));
            }
            break;
        }
        default:
            debugAssertM(false, G3D::format("Image::set does not support pixel format (%s)", m_format->name().c_str()));
            break;
    }
}


void Image::set(const Point2int32& pos, const Color3& color) {
    set(pos, Color4(color));
}


void Image::set(const Point2int32& pos, const Color4unorm8& color) {
    Point2int32 fipPos(pos.x, m_image->getHeight() - pos.y - 1);

    BYTE* scanline = m_image->getScanLine(fipPos.y);
    switch (m_image->getImageType()) {
        case FIT_BITMAP:
        {
            if (m_image->isGrayscale()) {
                // todo (Image upgrade): investigate adding average() to Color3unorm8 to allow c.rgb().average()
                scanline[fipPos.x] = color.r.bits();
            } else if (m_image->getBitsPerPixel() == 24) {
                scanline += 3 * fipPos.x;

                scanline[FI_RGBA_RED]   = color.r.bits();
                scanline[FI_RGBA_GREEN] = color.g.bits();
                scanline[FI_RGBA_BLUE]  = color.b.bits();
            } else if (m_image->getBitsPerPixel() == 32) {
                scanline += 4 * fipPos.x;

                scanline[FI_RGBA_RED]   = color.r.bits();
                scanline[FI_RGBA_GREEN] = color.g.bits();
                scanline[FI_RGBA_BLUE]  = color.b.bits();
                scanline[FI_RGBA_ALPHA] = color.a.bits();
            }
            break;
        }
        default:
            debugAssertM(false, G3D::format("Image::set does not support pixel format (%s)", m_format->name().c_str()));
            break;
    }
}


void Image::set(const Point2int32& pos, const Color3unorm8& color) {
    set(pos, Color4unorm8(color, unorm8::fromBits(255)));
}


void Image::set(const Point2int32& pos, const Color1unorm8& color) {
    set(pos, Color4unorm8(color.value, color.value, color.value, unorm8::fromBits(255)));
}


static const ImageFormat* determineImageFormat(const fipImage* image) {
    debugAssert(image->isValid() && image->getImageType() != FIT_UNKNOWN);
    
    const ImageFormat* imageFormat = NULL;
    switch (image->getImageType())
    {
        case FIT_BITMAP:
        {
            switch (image->getBitsPerPixel())
            {
                case 8:
                    imageFormat = ImageFormat::L8();
                    break;

                case 16:
                    // todo: find matching image format
                    break;

                case 24:
                    imageFormat = ImageFormat::RGB8();
                    break;

                case 32:
                    imageFormat = ImageFormat::RGBA8();
                    break;

                default:
                    debugAssertM(false, "Unsupported bit depth loaded.");
                    break;
            }
            break;
        }

        case FIT_UINT16:
            imageFormat = ImageFormat::L16();
            break;

        case FIT_FLOAT:
            imageFormat = ImageFormat::L32F();
            break;

        case FIT_RGBF:
            imageFormat = ImageFormat::RGB32F();
            break;

        case FIT_RGBAF:
            imageFormat = ImageFormat::RGBA32F();
            break;

        case FIT_INT16:
        case FIT_UINT32:
        case FIT_INT32:
        case FIT_DOUBLE:
        case FIT_RGB16:
        case FIT_RGBA16:
        case FIT_COMPLEX:
        default:
            debugAssertM(false, "Unsupported FreeImage type loaded.");
            break;
    }

    if (image->getColorType() == FIC_CMYK) {
        debugAssertM(false, "Unsupported FreeImage color space (CMYK) loaded.");
        imageFormat = NULL;
    }

    return imageFormat;
}


static FREE_IMAGE_TYPE determineFreeImageType(const ImageFormat* imageFormat) {
    FREE_IMAGE_TYPE fiType = FIT_UNKNOWN;
    if (imageFormat == NULL) {
        return fiType;
    }

    switch (imageFormat->code) {
        case ImageFormat::CODE_L8:
        case ImageFormat::CODE_RGB8:
        case ImageFormat::CODE_RGBA8:
            fiType = FIT_BITMAP;
            break;

        case ImageFormat::CODE_L16:
            fiType = FIT_UINT16;
            break;

        case ImageFormat::CODE_RGB32F:
            fiType = FIT_RGBF;
            break;

        case ImageFormat::CODE_RGBA32F:
            fiType = FIT_RGBAF;
            break;

        default:
            break;
    }

    return fiType;
}

} // namespace G3D
