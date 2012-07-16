/**
  \file G3D/Image.h 
  \author Corey Taylor

  Copyright 2000-2012, Morgan McGuire.
  All rights reserved.
 */
#ifndef G3D_Image_h
#define G3D_Image_h

#include "G3D/platform.h"
#include "G3D/Color4.h"
#include "G3D/Color4unorm8.h"
#include "G3D/Color1unorm8.h"
#include "G3D/GMutex.h"
#include "G3D/ImageBuffer.h"
#include "G3D/Vector2int32.h"
#include "G3D/ReferenceCount.h"

// non-G3D forward declarations
class fipImage;

namespace G3D {

// Forward declarations
class BinaryInput;
class ImageFormat;

/**
    \brief Provides general image loading, saving, conversion and pixel access.

    Image allows you to load a variety of supported file formats
    in their native pixel format with very few exceptions.  Users are
    responsible for converting pixel data to the desired format after loading.

    Image will also try to save directly to a file in the same pixel format
    as it is stored internally.  If a file format does not support that pixel format
    the user is responsible for converting before saving.

    Image::Error exception is thrown if a file cannot be loaded.

    \beta
*/
class Image : public ReferenceCountedObject {
public:
    typedef ReferenceCountedPointer<Image> Ref;

    class Error {
    public:
        Error
        (const std::string& reason,
         const std::string& filename = "") :
        reason(reason), filename(filename) {}
        
        std::string reason;
        std::string filename;
    };

private:
    fipImage*           m_image;
    const ImageFormat*  m_format;

    /** Ensures that FreeImage is not initialized on multiple threads simultaneously. */
    static GMutex       s_freeImageMutex;

    Image();

    // Intentionally not implemented to prevent copy construction
    Image(const Image&);
    Image& operator=(const Image&);

    /** Initialize the FreeImage library on first use */
    static void initFreeImage();

public:
    virtual ~Image();

    static Ref create(int width, int height, const ImageFormat* imageFormat);

    /** Determines if a file format is supported.  Does not check if pixel format is supported. */
    static bool fileSupported(const std::string& filename, bool allowCheckSignature = false);

    /** Loads an image from file specified by \a filename.  Sets internal pixel format to imageFormat but does not convert. */
    static Ref fromFile(const std::string& filename, const ImageFormat* imageFormat = ImageFormat::AUTO());

    /** Loads an image from existing BinaryInput \a bi.  Sets internal pixel format to imageFormat but does not convert. */
    static Ref fromBinaryInput(BinaryInput* bi, const ImageFormat* imageFormat = ImageFormat::AUTO());

    /** Loads an image from existing ImageBuffer \a buffer.  Performs a copy of pixel data. */
    static Ref fromImageBuffer(const ImageBuffer::Ref& buffer);

    /** Saves internal pixel data to file specified by \a filename.  Does not convert pixel format before saving. */
    void toFile(const std::string& filename) const;

    /** Saves internal pixel data to existing BinaryOutput \a bo.  Does not convert pixel format before saving. */
    void toBinaryOutput(BinaryOutput* bo, const std::string& fileFormat) const;

    /** Extracts a copy of the pixel data. */
    ImageBuffer::Ref toImageBuffer() const;

    /** Copies the underlying pixel data */
    Ref clone() const;

    /** Width in pixels */
    int width() const;

    /** Height in pixels */
    int height() const;

    const ImageFormat* format() const;

    void flipVertical();
    void flipHorizontal();
    void rotateCW(double radians, int numRotations = 1);
    void rotateCCW(double radians, int numRotations = 1);

    /// Direct replacements for old GImage functions for now
    bool convertToL8();
    bool convertToRGB8();
    bool convertToRGBA8();

    void get(const Point2int32& pos, Color4& color) const;
    void get(const Point2int32& pos, Color3& color) const;
    void get(const Point2int32& pos, Color4unorm8& color) const;
    void get(const Point2int32& pos, Color3unorm8& color) const;
    void get(const Point2int32& pos, Color1unorm8& color) const;

    void set(const Point2int32& pos, const Color4& color);
    void set(const Point2int32& pos, const Color3& color);
    void set(const Point2int32& pos, const Color4unorm8& color);
    void set(const Point2int32& pos, const Color3unorm8& color);
    void set(const Point2int32& pos, const Color1unorm8& color);

};

} // namespace G3D

#endif // G3D_IMAGE_h
