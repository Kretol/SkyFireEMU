/**
  \file G3D/ImageConvert.h

  \created 2012-05-24
  \edited  2012-05-24
*/

#ifndef G3D_ImageConvert_H
#define G3D_ImageConvert_H

#include "G3D/ImageBuffer.h"

namespace G3D {


/**
    Image conversion utility methods
*/
class ImageConvert {
private:
    ImageConvert();
    
    typedef ImageBuffer::Ref (*ConvertFunc)(const ImageBuffer::Ref& src, const ImageFormat* dstFormat);
    static ConvertFunc FindConverter(const ImageBuffer::Ref& src, const ImageFormat* dstFormat);

    // Converters
    ImageBuffer::Ref ConvertRGBAddAlpha(const ImageBuffer::Ref& src, const ImageFormat* dstFormat);

public:
    /** Converts image buffer to another format if supported, otherwise returns null ref */
    static ImageBuffer::Ref convertBuffer(const ImageBuffer::Ref& src, const ImageFormat* dstFormat);
};

} // namespace G3D

#endif // GLG3D_ImageConvert_H
