#pragma once

#include "gl.h"

#include "util/fastmap.h"

#include <vector>
#include <memory>
#include <string>

namespace Tangram {

class ShaderProgram;

class VertexLayout {

public:

    struct VertexAttrib {
        std::string name;
        GLint size;
        GLenum type;
        GLboolean normalized;
        size_t offset; // Can be left as zero; value is overwritten in constructor of VertexLayout
    };

    VertexLayout(std::vector<VertexAttrib> attribs);

    virtual ~VertexLayout();

    void enable(ShaderProgram& program, size_t byteOffset, void* ptr = nullptr);

    void enable(const fastmap<std::string, GLuint>& locations, size_t bytOffset);

    GLint getStride() const { return m_stride; };

    const std::vector<VertexAttrib> getAttribs() const { return m_attribs; }

    size_t getOffset(std::string attribName);

    static void clearCache();

private:

    static fastmap<GLint, GLuint> s_enabledAttribs; // Map from attrib locations to bound shader program

    std::vector<VertexAttrib> m_attribs;
    GLint m_stride;

};

}
