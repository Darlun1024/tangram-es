#pragma once

#include "textUtil.h"

// For textParameters
#include "style/textStyle.h"
#include "labels/textLabel.h"

#include "alfons/alfons.h"
#include "alfons/fontManager.h"
#include "alfons/atlas.h"
#include "alfons/textBatch.h"
#include "alfons/textShaper.h"
#include "alfons/font.h"
#include "alfons/inputSource.h"

#include "gl/texture.h"

#include <bitset>
#include <mutex>

namespace Tangram {

namespace alf = alfons;

struct FontMetrics {
    float ascender, descender, lineHeight;
};

// TODO could be a shared_ptr<Texture>
struct GlyphTexture {

    static constexpr int size = 256;

    GlyphTexture() : texture(size, size) {
        texData.resize(size * size);
    }

    std::vector<unsigned char> texData;
    Texture texture;

    bool dirty;
    size_t refCount = 0;
};

class FontContext : public alf::TextureCallback {

public:

    static constexpr int max_textures = 64;

    FontContext();

    /* Synchronized on m_mutex on tile-worker threads
     * Called from alfons when a texture atlas needs to be created
     * Triggered from TextStyleBuilder::prepareLabel
     */
    void addTexture(alf::AtlasID id, uint16_t width, uint16_t height) override;

    /* Synchronized on m_mutex, called tile-worker threads
     * Called from alfons when a glyph needs to be added the the atlas identified by id
     * Triggered from TextStyleBuilder::prepareLabel
     */
    void addGlyph(alf::AtlasID id, uint16_t gx, uint16_t gy, uint16_t gw, uint16_t gh,
                  const unsigned char* src, uint16_t pad) override;

    void releaseAtlas(std::bitset<max_textures> _refs);

    void lockAtlas(std::bitset<max_textures> _refs);

    alf::GlyphAtlas& atlas() { return m_atlas; }

    /* Update all textures batches, uploads the data to the GPU */
    void updateTextures();

    std::shared_ptr<alf::Font> getFont(const std::string& _name, const std::string& _style,
                                       const std::string& _weight, float _size);

    size_t glyphTextureCount() {
        return m_textures.size();
    }

    void bindTexture(alf::AtlasID _id, GLuint _unit);

    float maxStrokeWidth() { return m_sdfRadius; }

    bool layoutText(TextStyle::Parameters& _params, const std::string& _text,
                    std::vector<GlyphQuad>& _quads, glm::vec2& _bbox);

    struct ScratchBuffer : public alfons::MeshCallback {
        void drawGlyph(const alfons::Quad& q, const alfons::AtlasGlyph& altasGlyph) override {}
        void drawGlyph(const alfons::Rect& q, const alfons::AtlasGlyph& atlasGlyph) override;
        std::vector<GlyphQuad>* quads;
    };

private:
    float m_sdfRadius;
    ScratchBuffer m_scratch;

    std::mutex m_mutex;
    std::array<int, max_textures> m_atlasRefCount = {{0}};
    alf::GlyphAtlas m_atlas;

    alf::FontManager m_alfons;
    std::array<std::shared_ptr<alf::Font>, 3> m_font;

    std::vector<GlyphTexture> m_textures;

    // TextShaper to create <LineLayout> for a given text and Font
    alfons::TextShaper m_shaper;

    // TextBatch to 'draw' <LineLayout>s, i.e. creating glyph textures and glyph quads.
    // It is intialized with a TextureCallback implemented by FontContext for adding glyph
    // textures and a MeshCallback implemented by TextStyleBuilder for adding glyph quads.
    alfons::TextBatch m_batch;

};

}
