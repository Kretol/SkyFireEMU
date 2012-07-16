#include "G3D/BinaryInput.h"
#include "G3D/BinaryOutput.h"
#include "G3D/SpeedLoad.h"

namespace G3D {
namespace SpeedLoad { 

void readHeader(BinaryInput& b, const std::string& expectedString) {

    const std::string header = b.readString(SpeedLoad::HEADER_LENGTH);

    (void)header;
    alwaysAssertM(header == expectedString, 
                  format("SpeedLoad expected to read chunk \"%s\" but found chunk \"%s\"",
                         expectedString.c_str(), header.c_str()));
}


void writeHeader(BinaryOutput& b, const std::string& header) {
    debugAssertM(header.size() < HEADER_LENGTH, "This header string is too long");
    b.writeString(header, SpeedLoad::HEADER_LENGTH);
}

} // SpeedLoad
} // G3D
