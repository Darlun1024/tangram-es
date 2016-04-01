#include "texture.h"

#include "platform.h"
#include "util/geom.h"
#include "gl/renderState.h"
#include "gl/hardware.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstring> // for memset

namespace Tangram {

Texture::Texture(unsigned int width, unsigned int height, TextureOptions options, bool generateMipmaps)
    : m_options(options), m_generateMipmaps(generateMipmaps) {

    m_glHandle = 0;
    m_shouldResize = false;
    m_target = GL_TEXTURE_2D;
    m_generation = -1;

    resize(width, height);
}

Texture::Texture(const std::string& file, TextureOptions options, bool generateMipmaps)
    : Texture(0u, 0u, options, generateMipmaps) {

    unsigned int size;
    unsigned char* data;

    data = bytesFromFile(file.c_str(), PathType::resource, &size);

    loadPNG(data, size);

    free(data);
}

Texture::Texture(const unsigned char* data, size_t dataSize, TextureOptions options, bool generateMipmaps)
    : Texture(0u, 0u, options, generateMipmaps) {

    loadPNG(data, dataSize);
}

void Texture::loadPNG(const unsigned char* blob, unsigned int size) {
    if (blob == nullptr || size == 0) {
        LOGE("Texture data is empty!");
        return;
    }

    unsigned char* pixels;
    int width, height, comp;

    pixels = stbi_load_from_memory(blob, size, &width, &height, &comp, STBI_rgb_alpha);

    resize(width, height);
    setData(reinterpret_cast<GLuint*>(pixels), width * height);

    stbi_image_free(pixels);
}

Texture::Texture(Texture&& other) {
    m_glHandle = other.m_glHandle;
    other.m_glHandle = 0;

    m_options = other.m_options;
    m_data = std::move(other.m_data);
    m_dirtyRanges = std::move(other.m_dirtyRanges);
    m_width = other.m_width;
    m_height = other.m_height;
    m_target = other.m_target;
    m_generation = other.m_generation;
    m_generateMipmaps = other.m_generateMipmaps;
}

Texture& Texture::operator=(Texture&& other) {
    m_glHandle = other.m_glHandle;
    other.m_glHandle = 0;

    m_options = other.m_options;
    m_data = std::move(other.m_data);
    m_dirtyRanges = std::move(other.m_dirtyRanges);
    m_width = other.m_width;
    m_height = other.m_height;
    m_target = other.m_target;
    m_generation = other.m_generation;
    m_generateMipmaps = other.m_generateMipmaps;

    return *this;
}

Texture::~Texture() {
    if (m_glHandle) {
        glDeleteTextures(1, &m_glHandle);

        // if the texture is bound, and deleted, the binding defaults to 0
        // according to the OpenGL spec, in this case we need to force the
        // currently bound texture to 0 in the render states
        if (RenderState::texture.compare(m_target, m_glHandle)) {
            RenderState::texture.init(m_target, 0, false);
        }
    }
}

void Texture::setData(const GLuint* data, unsigned int dataSize) {

    if (m_data.size() > 0) { m_data.clear(); }

    m_data.insert(m_data.begin(), data, data + dataSize);

    setDirty(0, m_height);
}

void Texture::setSubData(const GLuint* subData, uint16_t xoff, uint16_t yoff,
                         uint16_t width, uint16_t height, uint16_t stride) {

    size_t bpp = bytesPerPixel();
    size_t divisor = sizeof(GLuint) / bpp;

    // Init m_data if update() was not called after resize()
    if (m_data.size() != (m_width * m_height) / divisor) {
        m_data.resize((m_width * m_height) / divisor);
    }

    // update m_data with subdata
    for (size_t row = 0; row < height; row++) {

        size_t pos = ((yoff + row) * m_width + xoff) / divisor;
        size_t posIn = (row * stride) / divisor;
        std::memcpy(&m_data[pos], &subData[posIn], width * bpp);
    }

    setDirty(yoff, height);
}

void Texture::setDirty(size_t yoff, size_t height) {
    // FIXME: check that dirty range is valid for texture size!
    size_t max = yoff + height;
    size_t min = yoff;

    if (m_dirtyRanges.empty()) {
        m_dirtyRanges.push_back({min, max});
        return;
    }

    auto n = m_dirtyRanges.begin();

    // Find first overlap
    while (n != m_dirtyRanges.end()) {
        if (min > n->max) {
            // this range is after current
            ++n;
            continue;
        }
        if (max < n->min) {
            // this range is before current
            m_dirtyRanges.insert(n, {min, max});
            return;
        }
        // Combine with overlapping range
        n->min = std::min(n->min, min);
        n->max = std::max(n->max, max);
        break;
    }
    if (n == m_dirtyRanges.end()) {
        m_dirtyRanges.push_back({min, max});
        return;
    }

    // Merge up to last overlap
    auto it = n+1;
    while (it != m_dirtyRanges.end() && max >= it->min) {
        n->max = std::max(it->max, max);
        it = m_dirtyRanges.erase(it);
    }
}

void Texture::bind(GLuint unit) {
    RenderState::textureUnit(unit);
    RenderState::texture(m_target, m_glHandle);
}

void Texture::generate(GLuint textureUnit) {
    glGenTextures(1, &m_glHandle);

    bind(textureUnit);

    glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, m_options.filtering.min);
    glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, m_options.filtering.mag);

    glTexParameteri(m_target, GL_TEXTURE_WRAP_S, m_options.wrapping.wraps);
    glTexParameteri(m_target, GL_TEXTURE_WRAP_T, m_options.wrapping.wrapt);

    m_generation = RenderState::generation();
}

void Texture::checkValidity() {

    if (!RenderState::isValidGeneration(m_generation)) {
        m_shouldResize = true;
        m_glHandle = 0;
    }
}

bool Texture::isValid() {
    return (RenderState::isValidGeneration(m_generation) && m_glHandle != 0);
}

void Texture::update(GLuint textureUnit) {

    checkValidity();

    if (!m_shouldResize && m_dirtyRanges.empty()) {
        return;
    }

    if (m_glHandle == 0) {
        if (m_data.size() == 0) {
            size_t divisor = sizeof(GLuint) / bytesPerPixel();
            m_data.resize((m_width * m_height) / divisor, 0);
        }
    }

    GLuint* data = m_data.size() > 0 ? m_data.data() : nullptr;

    update(textureUnit, data);
}

void Texture::update(GLuint textureUnit, const GLuint* data) {

    checkValidity();

    if (!m_shouldResize && m_dirtyRanges.empty()) {
        return;
    }

    if (m_glHandle == 0) {
        // texture hasn't been initialized yet, generate it
        generate(textureUnit);
    } else {
        bind(textureUnit);
    }

    // resize or push data
    if (m_shouldResize) {
        if (Hardware::maxTextureSize < m_width || Hardware::maxTextureSize < m_height) {
            LOGW("The hardware maximum texture size is currently reached");
        }

        glTexImage2D(m_target, 0, m_options.internalFormat,
                     m_width, m_height, 0, m_options.format,
                     GL_UNSIGNED_BYTE, data);

        if (data && m_generateMipmaps) {
            // generate the mipmaps for this texture
            glGenerateMipmap(m_target);
        }
        m_shouldResize = false;
        m_dirtyRanges.clear();
        return;
    }
    size_t bpp = bytesPerPixel();
    size_t divisor = sizeof(GLuint) / bpp;

    for (auto& range : m_dirtyRanges) {
        size_t offset =  (range.min * m_width) / divisor;
        glTexSubImage2D(m_target, 0, 0, range.min, m_width, range.max - range.min,
                        m_options.format, GL_UNSIGNED_BYTE,
                        data + offset);
    }
    m_dirtyRanges.clear();
}

void Texture::resize(const unsigned int width, const unsigned int height) {
    m_width = width;
    m_height = height;

    if (!(Hardware::supportsTextureNPOT) &&
        !(isPowerOfTwo(m_width) && isPowerOfTwo(m_height)) &&
        (m_generateMipmaps || isRepeatWrapping(m_options.wrapping))) {
        LOGW("OpenGL ES doesn't support texture repeat wrapping for NPOT textures nor mipmap textures");
        LOGW("Falling back to LINEAR Filtering");
        m_options.filtering = {GL_LINEAR, GL_LINEAR};
        m_generateMipmaps = false;
    }

    m_shouldResize = true;
    m_dirtyRanges.clear();
}

bool Texture::isRepeatWrapping(TextureWrapping wrapping) {
    return wrapping.wraps == GL_REPEAT || wrapping.wrapt == GL_REPEAT;
}

size_t Texture::bytesPerPixel() {
    switch (m_options.internalFormat) {
        case GL_ALPHA:
        case GL_LUMINANCE:
            return 1;
        case GL_LUMINANCE_ALPHA:
            return 2;
        case GL_RGB:
            return 3;
        default:
            return 4;
    }
}

}
