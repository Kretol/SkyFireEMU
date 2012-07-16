/**
  \file G3D/ImageConvert.cpp

  \created 2012-05-24
  \edited  2012-05-24
*/

#include "G3D/ImageConvert.h"


namespace G3D {


ImageBuffer::Ref ImageConvert::convertBuffer(const ImageBuffer::Ref& src, const ImageFormat* dstFormat) {
    ConvertFunc converter = FindConverter(src, dstFormat);
    if (converter)
    {
        return (*converter)(src, dstFormat);
    }
    else
    {
        return NULL;
    }
}

ImageConvert::ConvertFunc ImageConvert::FindConverter(const ImageBuffer::Ref& src, const ImageFormat* dstFormat) {
    return NULL;
}

ImageBuffer::Ref ImageConvert::ConvertRGBAddAlpha(const ImageBuffer::Ref& src, const ImageFormat* dstFormat) {
    return NULL;
}

} // namespace G3D
