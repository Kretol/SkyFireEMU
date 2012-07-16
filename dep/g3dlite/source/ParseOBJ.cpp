/**
 \file G3D/source/ParseOBJ.cpp

 \author Morgan McGuire, http://graphics.cs.williams.edu
 \created 2011-07-16
 \edited  2011-08-22
 
 Copyright 2000-2012, Morgan McGuire.
 All rights reserved.
*/
#include "G3D/ParseOBJ.h"
#include "G3D/BinaryInput.h"
#include "G3D/FileSystem.h"
#include "G3D/stringutils.h"
#include "G3D/TextInput.h"

namespace G3D {


void ParseOBJ::parse(const char* ptr, size_t len, const std::string& basePath) {
    vertexArray.clear();
    normalArray.clear();
    texCoordArray.clear();
    groupTable.clear();
    m_basePath = "";
    m_currentGroup = NULL;
    m_currentMesh = NULL;
    m_currentMaterial = NULL;

    m_basePath = basePath;

    nextCharacter = ptr;
    alwaysAssertM(len < 0xFFFFFFFF, "Cannot handle more than 4GB of input text.");
    remainingCharacters = (int)len;
    m_line = 1;

    while (remainingCharacters > 0) {
        // Process leading whitespace
        maybeReadWhitespace();

        const Command command = readCommand();
        processCommand(command);

        if (m_line % 100000 == 0) {
            debugPrintf("  ParseOBJ at line %d\n", m_line);
        }
    }        
}


void ParseOBJ::parse(BinaryInput& bi, const std::string& basePath) {
    m_filename = bi.getFilename();

    std::string bp = basePath;
    if (bp == "<AUTO>") {
        bp = FilePath::parent(FileSystem::resolve(m_filename));
    }    

    parse((const char*)bi.getCArray() + bi.getPosition(),
          bi.getLength() - bi.getPosition(), bp);
}


ParseMTL::Material::Ref ParseOBJ::getMaterial(const std::string& materialName) {
    bool created = false;
    ParseMTL::Material::Ref& m =
        m_currentMaterialLibrary.materialTable.getCreate(materialName);

    if (created) {
        m = ParseMTL::Material::create();
        debugPrintf("Warning: missing material %s used.\n", materialName.c_str());
    }
    return m;
}


bool ParseOBJ::maybeReadWhitespace() {
    bool changedLines = false;

    while (remainingCharacters > 0) {
        switch (*nextCharacter) {
        case '\n':
        case '\r':
            {
                char c = *nextCharacter;
                consumeCharacter();
                ++m_line;
                changedLines = true;
                if ((remainingCharacters > 0) && (c != *nextCharacter) && 
                    ((*nextCharacter == '\r') || (*nextCharacter == '\n'))) {
                    // This is part of a two-character, e.g., Mac or
                    // Windows.  Consume the next character as well.
                    consumeCharacter();
                }
            }
            break;

        case ' ':
        case '\t':
            // Consume whitespace
            consumeCharacter();
            break;

        case '#':
            // Consume comment
            readUntilNewline();
            // Don't consume the newline; we'll catch it on the next
            // iteration
            break;

        default:
            return changedLines;
        }
    }

    return true;
}


ParseOBJ::Command ParseOBJ::readCommand() {
    if (remainingCharacters == 0) {
        return UNKNOWN;
    }

    // Explicit finite automata parser
    switch (*nextCharacter) {
    case 'f':
        consumeCharacter();
        if (isSpace(*nextCharacter)) {
            return FACE;
        } else {
            return UNKNOWN;
        }
        break;

    case 'v':
        consumeCharacter();
        switch (*nextCharacter) {
        case ' ':
        case '\t':
            return VERTEX;

        case 'n':
            consumeCharacter();
            if (isSpace(*nextCharacter)) {
                return NORMAL;
            } else {
                return UNKNOWN;
            }
            break;

        case 't':
            consumeCharacter();
            if (isSpace(*nextCharacter)) {
                return TEXCOORD;
            } else {
                return UNKNOWN;
            }
            break;

        default:
            return UNKNOWN;
        }
        break;

    case 'm':
        if ((remainingCharacters > 6) && (memcmp(nextCharacter, "mtllib", 6) == 0)) {
            nextCharacter += 6; remainingCharacters -= 6;
            if (isSpace(*nextCharacter)) {
                return MTLLIB;
            } else {
                return UNKNOWN;
            }
        } else {
            return UNKNOWN;
        }
        break;

    case 'u':
        if ((remainingCharacters > 6) && (memcmp(nextCharacter, "usemtl", 6) == 0)) {
            nextCharacter += 6; remainingCharacters -= 6;
            if (isSpace(*nextCharacter)) {
                return USEMTL;
            } else {
                return UNKNOWN;
            }
        } else {
            return UNKNOWN;
        }
        break;

    case 'g':
        consumeCharacter();
        if (isSpace(*nextCharacter)) {
            return GROUP;
        } else {
            return UNKNOWN;
        }
        break;

    default:
        return UNKNOWN;
    }
}


void ParseOBJ::readFace() {
    // Ensure that we have a material
    if (m_currentMaterial.isNull()) {
        m_currentMaterial = m_currentMaterialLibrary.materialTable["default"];
    }

    // Mnsure that we have a group
    if (m_currentGroup.isNull()) {
        // Create a group named "default", per the OBJ specification
        m_currentGroup = Group::create();
        m_currentGroup->name = "default";
        groupTable.set(m_currentGroup->name, m_currentGroup);

        // We can't have a mesh without a group, but conservatively reset this anyway
        m_currentMesh = NULL;
    }

    // Ensure that we have a mesh
    if (m_currentMesh.isNull()) {
        bool created = false;
        Mesh::Ref& m = m_currentGroup->meshTable.getCreate(m_currentMaterial, created);

        if (created) {
            m = Mesh::create();
            m->material = m_currentMaterial;
        }
        m_currentMesh = m;
    }

    Face& face = m_currentMesh->faceArray.next();

    const int vertexArraySize   = vertexArray.size();
    const int texCoordArraySize = texCoordArray.size();
    const int normalArraySize   = normalArray.size();

    // Consume leading whitespace
    bool done = maybeReadWhitespace();
    while (! done) {
        Index& index = face.next();

        // Read index
        index.vertex = readInt();
        if (index.vertex > 0) {
            // Make 0-based
            --index.vertex;
        } else {
            // Negative; make relative to the current end of the array.
            // -1 will be the last element, so just add the size of the array.
            index.vertex += vertexArraySize;
        }

        if ((remainingCharacters > 0) && (*nextCharacter == '/')) {
            // Read the slash
            consumeCharacter();

            if (remainingCharacters > 0) {
                if (*nextCharacter != '/') {
                    // texcoord index
                    index.texCoord = readInt();
                    if (index.texCoord > 0) {
                        // Make 0-based
                        --index.texCoord;
                    } else {
                        // Negative; make relative to the current end
                        // of the array.  -1 will be the last element,
                        // so just add the size of the array.
                        index.texCoord += texCoordArraySize;
                    }
                }

                if ((remainingCharacters > 0) && (*nextCharacter == '/')) {
                    // Consume the slash
                    consumeCharacter();

                    // normal index
                    index.normal = readInt();
                    if (index.normal > 0) {
                        // Make 0-based
                        --index.normal;
                    } else {
                        // Negative; make relative to the current
                        // end of the array.  -1 will be the last
                        // element, so just add the size of the
                        // array.
                        index.normal += normalArraySize;
                    }       
                }
            }
        }

        // Read remaining whitespace
        done = maybeReadWhitespace();
    }
}


void ParseOBJ::processCommand(const Command command) {
    switch (command) {
    case VERTEX:
        maybeReadWhitespace();
        vertexArray.append(readVector3());
        // Consume anything else on this line
        readUntilNewline();
        break;

    case TEXCOORD:
        maybeReadWhitespace();
        texCoordArray.append(readVector2());
        // Consume anything else on this line
        readUntilNewline();
        break;

    case NORMAL:
        maybeReadWhitespace();
        normalArray.append(readVector3());
        // Consume anything else on this line
        readUntilNewline();
        break;

    case FACE:
        readFace();
        // Faces consume newlines by themselves
        break;

    case GROUP:
        {
            // Change group
            std::string groupName = readName();

            Group::Ref& g = groupTable.getCreate(groupName);

            if (g.isNull()) {
                // Newly created
                g = Group::create();
                g->name = groupName;
            }

            m_currentGroup = g;
        }
        // Consume anything else on this line
        readUntilNewline();
        break;

    case USEMTL:
        {
            // Change the mesh within the group
            const std::string& materialName = readName();
            m_currentMaterial = getMaterial(materialName);

            // Force re-obtaining or creating of the appropriate mesh
            m_currentMesh = NULL;
        }
        // Consume anything else on this line
        readUntilNewline();
        break;

    case MTLLIB:
        {
            // Specify material library 
            std::string mtlFilename = readName();
            mtlFilename = FilePath::concat(m_basePath, mtlFilename);

            TextInput ti2(mtlFilename);
            m_currentMaterialLibrary.parse(ti2);
        }
        // Consume anything else on this line
        readUntilNewline();
        break;

    case UNKNOWN:
        // Nothing to do
        readUntilNewline();
        break;
    }
}

} // namespace G3D
