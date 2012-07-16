/**
 \file G3D/ParseOBJ.h

 \maintainer Morgan McGuire, http://graphics.cs.williams.edu

 \created 2011-07-19
 \edited  2011-08-19

 Copyright 2002-2011, Morgan McGuire.
 All rights reserved.
*/

#ifndef G3D_ParseOBJ_h
#define G3D_ParseOBJ_h

#include "G3D/platform.h"
#include <cstdio>
#include "G3D/ReferenceCount.h"
#include "G3D/Array.h"
#include "G3D/SmallArray.h"
#include "G3D/Table.h"
#include "G3D/Vector2.h"
#include "G3D/Vector3.h"
#include "G3D/Vector4.h"
#include "G3D/ParseMTL.h"
#include "G3D/ParseError.h"
#include "G3D/stringutils.h"

namespace G3D {

class BinaryInput;

/** \brief Parses OBJ files with polygonal data and their associated MTL files.

Ignores groups, smoothing groups, surfaces, object names. Assumes
that each face is in exactly one group.  Note that group information may be useful
for object-level culling.

\cite http://www.martinreddy.net/gfx/3d/OBJ.spec

Uses a special text parser instead of G3D::TextInput for peak performance (about 30x faster
than TextInput).

\sa G3D::ParseMTL, G3D::ParsePLY, G3D::Parse3DS, G3D::ArticulatedModel
*/
class ParseOBJ {
public:
    static const int UNDEFINED = -1;

    /** Set of indices into the vertex attribute arrays.  Note that OBJ
        format allows a separate index for each attribute, unlike OpenGL. */
    class Index {
    public:
        /** 0-based index into vertexArray */
        int             vertex;

        /** 0-based index into normalArray */
        int             normal;

        /** 0-based index into texCoordArray */
        int             texCoord;

        Index() : vertex(UNDEFINED), normal(UNDEFINED), texCoord(UNDEFINED) {}
    };

    /** A polygon, which is expected to be a triangle or quadrilateral 
        but is unlimited in OBJ format.         
        */
    // We reserve 5 sides before going to heap allocation because
    // that was observed to save 1/2 second when loading Crytek Sponza.
    typedef SmallArray<Index, 5> Face;
    
    /** Part of a group that uses a single material. */
    class Mesh : public ReferenceCountedObject {
    public:
        typedef ReferenceCountedPointer<Mesh> Ref;

        /** Need a material instead of a material name
            because technically the material library can
            change during load.
         */
        ParseMTL::Material::Ref   material;
        Array<Face>     faceArray;

    private:

        Mesh() {}

    public:

        static Ref create() {
            return new Mesh();
        }
    };

    typedef Table<ParseMTL::Material::Ref, Mesh::Ref> MeshTable;

    /** An OBJ group, created with the "g" command. 
    */
    class Group : public ReferenceCountedObject {
    public:
        typedef ReferenceCountedPointer<Group> Ref;

        std::string     name;

        /** Maps ParseMTL::Material%s to Mesh%ss within this group */
        MeshTable       meshTable;

    private:

        Group() {}

    public:

        static Ref create() {
            return new Group();
        }    
    };


    typedef Table<std::string, Group::Ref> GroupTable;

    Array<Point3>       vertexArray;
    Array<Vector3>      normalArray;

    /** Texture coordinates in OBJ coordinate frame, where (0, 0) is the LOWER-left.*/
    Array<Point2>       texCoordArray;

    /** Maps group names to groups. */
    GroupTable          groupTable;

private:

    std::string         m_filename;

    /** Pointer to the next character in buffer */
    const char*         nextCharacter;

    /** Number of characters left */
    int                 remainingCharacters;

    /** Line in the file, starting from 1.  For debugging and error reporting */
    int                 m_line;


    /** The material library can be replaced during load, although rarely is. */
    ParseMTL            m_currentMaterialLibrary;

    /** Paths are interpreted relative to this */
    std::string         m_basePath;

    /** Group to which we are currently adding elements */
    Group::Ref          m_currentGroup;

    /** Mesh within m_currentGroup to which we are currently adding elements. 
      Determined by the material name. */
    Mesh::Ref           m_currentMesh;

    /** Material specified by the last useMtl command */
    ParseMTL::Material::Ref  m_currentMaterial;

    void processCommand(TextInput& ti, const std::string& cmd);

    /** Processes the "f" command.  Called from processCommand. */
    void processFace(TextInput& ti);

    ParseMTL::Material::Ref getMaterial(const std::string& materialName);
    
    /** Consume one character */
    inline void consumeCharacter() {
        ++nextCharacter;
        --remainingCharacters;
    }
    
    /** Reads until the end of file or newline, but does not consume
        the newline */
    void readUntilNewline() {
        while ((remainingCharacters > 0) && 
               (*nextCharacter != '\r') &&
               (*nextCharacter != '\n')) {
            consumeCharacter();
        }
    }

    /** Feads the vertex index list for one face. */
    void readFace();

    /** Consume whitespace and comments, if there are any.  Leaves the
    pointer on the first non-whitespace character.  Returns true if an
    end-of-line was passed or the end of file was reached. */
    bool maybeReadWhitespace();

    enum Command {MTLLIB, GROUP, USEMTL, VERTEX, TEXCOORD, NORMAL, FACE, UNKNOWN};

    /** Returns true for space and tab, but not newline */
    static inline bool isSpace(const char c) {
        return (c == ' ') || (c == '\t');
    }

    /** Reads the next command.  Assumes that it is at the start of a command. 
      Leaves the pointer at the first character after the end of the command name.*/
    Command readCommand();

    template <class T>
    T readNumber(const char* fmt) {
        // Scan for the end of the token
        char old = '\0';
        int i = 0;
        while (i < remainingCharacters) {
            switch (nextCharacter[i]) {
            case ' ':
            case '\n':
            case '\r':
            case '\t':
            case '/':
            case '#':
                old = nextCharacter[i];
                // Overwrite with string terminator to stop sscanf
                const_cast<char&>(nextCharacter[i]) = '\0';
                // For performance, jump out directly rather than adding a
                // boolean (we can't "break" from inside of a switch).
                goto exitloop;
                break;

            default:;
                ++i;
                // Continue on
            }
        }
    exitloop:

        T n = 0;
        const int numRead = ::sscanf(nextCharacter, fmt, &n);

        if (numRead == 0) {
            // Something went wrong
            debugBreak();
            throw ParseError(m_filename, m_line, 0, 
                             format("Expected a number of C format \"%s\" on this line", fmt));
        }

        if (old != '\0') {
            // Restore the old character
            const_cast<char&>(nextCharacter[i]) = old;
        }

        // Jump to character i
        nextCharacter += i;
        remainingCharacters -= i;

        return n;    
    }
#if 0
    inline float readFloat() {
        return readNumber<float>("%f");
    }
#endif

    /*
    inline int readInt() {
        return readNumber<int>("%d");
    }
    */

    inline int readUnsignedInt() {
        int i = 0;
        while ((remainingCharacters > 0) && isDigit(*nextCharacter)) {
            i = i * 10 + (*nextCharacter) - '0';
            --remainingCharacters;
            ++nextCharacter;
        }
        return i;
    }


    /** Substantially (2x) faster than sscanf */
    int readInt() {
        // Consume leading sign
        const bool isNegative = (*nextCharacter == '-');
        if (isNegative || (*nextCharacter == '+')) {
            consumeCharacter();
        }

        int i = readUnsignedInt();

        return isNegative ? -i : i;
    }

    /** Substantially (2x) faster than sscanf */
    float readFloat() {
        // Consume leading sign
        const bool isNegative = (*nextCharacter == '-');
        if (isNegative || (*nextCharacter == '+')) {
            consumeCharacter();
        }

        // Read the integer part
        float f = float(readUnsignedInt());

        // Read optional fractional part
        if (*nextCharacter == '.') {
            // Consume the decimal
            consumeCharacter();

            // Read the fractional part
            int i = 0;
            int magnitude = 1;
            while ((remainingCharacters > 0) && isDigit(*nextCharacter)) {
                magnitude *= 10;
                i = i * 10 + (*nextCharacter) - '0';
                --remainingCharacters;
                ++nextCharacter;
            }

            const float fracPart = float(i) / float(magnitude);
        
            f += fracPart;
        }

        // Read optional exponent
        if (*nextCharacter == 'e') {
            consumeCharacter();
            const int e = readInt();
            f *= powf(10.0f, float(e));
        }

        return isNegative ? -f : f;
    }

    /** Reads until newline and removes leading and trailing space (ignores comments) */
    std::string readName() {
        // Read leading whitespace
        bool passedNewline = maybeReadWhitespace();
        if (passedNewline) {
            throw ParseError(m_filename, m_line, 0, "Expected a group or file name on this line");
        }

        // Read until EOF or EOLN.  OBJ does not allow comments unless
        // they are at the beginning of a line.
        std::string s;
        while ((remainingCharacters > 0) && 
               (*nextCharacter != '\r') && 
               (*nextCharacter != '\n')) {

            s += *nextCharacter;
            consumeCharacter();
        }

        return trimWhitespace(s);
    }

    /** Consume space and tab characters */
    void readWhitespace() {
        while ((remainingCharacters > 0) &&
               ((*nextCharacter == ' ') || (*nextCharacter == '\t'))) {
            consumeCharacter();
        }
    }

    Vector3 readVector3() {
        Vector3 v;
        v.x = readFloat();
        readWhitespace();
        v.y = readFloat();
        readWhitespace();
        v.z = readFloat();
        return v;
    }

    Vector2 readVector2() {
        Vector2 v;
        v.x = readFloat();
        readWhitespace();
        v.y = readFloat();
        return v;
    }

    void processCommand(const Command command);

public:

    void parse(const char* ptr, size_t len, const std::string& basePath);

    void parse(BinaryInput& bi, const std::string& basePath = "<AUTO>");
};

} // namespace G3D



template <> struct HashTrait<G3D::ParseOBJ::Index> {
    static size_t hashCode(const G3D::ParseOBJ::Index& k) { 
        return HashTrait<int>::hashCode(k.vertex + (k.normal << 8) + (k.texCoord << 16) + (k.texCoord >> 16)); 
    }
};


#endif // G3D_ParseOBJ_h
