#pragma once

#include "labels/label.h"
#include "labels/labelProperty.h"
#include "labels/labelSet.h"

#include <glm/glm.hpp>

namespace Tangram {

class TextLabels;
class TextStyle;

struct GlyphQuad {
    size_t atlas;
    struct {
        glm::i16vec2 pos;
        glm::u16vec2 uv;
    } quad[4];
};

struct TextVertex {
    glm::i16vec2 pos;
    glm::u16vec2 uv;
    uint32_t color;
    uint32_t stroke;
    struct State {
        glm::i16vec2 screenPos;
        uint8_t alpha;
        uint8_t scale;
        int16_t rotation;
    } state;

    const static float position_scale;
    const static float rotation_scale;
    const static float alpha_scale;
};

class TextLabel : public Label {

public:

    struct FontVertexAttributes {
        uint32_t fill;
        uint32_t stroke;
        uint8_t fontScale;
    };

    TextLabel(Label::Transform transform, Type type, Label::Options options,
              LabelProperty::Anchor anchor, TextLabel::FontVertexAttributes attrib,
              glm::vec2 dim, TextLabels& labels, Range vertexRange);

    void updateBBoxes(float zoomFract) override;

protected:
    void align(glm::vec2& screenPosition, const glm::vec2& ap1, const glm::vec2& ap2) override;
    glm::vec2 m_anchor;

    void pushTransform() override;

private:
    // Back-pointer to owning container
    const TextLabels& m_textLabels;
    // first vertex and count in m_textLabels quads
    const Range m_vertexRange;

    FontVertexAttributes m_fontAttrib;
};

}
