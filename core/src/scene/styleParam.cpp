#include "styleParam.h"

#include "csscolorparser.hpp"
#include "platform.h"
#include "util/builders.h" // for cap, join
#include "util/extrude.h"
#include "util/geom.h" // for CLAMP
#include <algorithm>
#include <map>
#include <cstring>

namespace Tangram {

using Color = CSSColorParser::Color;

const std::map<std::string, StyleParamKey> s_StyleParamMap = {
    {"align", StyleParamKey::align},
    {"anchor", StyleParamKey::anchor},
    {"cap", StyleParamKey::cap},
    {"centroid", StyleParamKey::centroid},
    {"collide", StyleParamKey::collide},
    {"color", StyleParamKey::color},
    {"extrude", StyleParamKey::extrude},
    {"font:family", StyleParamKey::font_family},
    {"font:fill", StyleParamKey::font_fill},
    {"font:size", StyleParamKey::font_size},
    {"font:stroke:color", StyleParamKey::font_stroke_color},
    {"font:stroke:width", StyleParamKey::font_stroke_width},
    {"font:style", StyleParamKey::font_style},
    {"font:transform", StyleParamKey::transform},
    {"font:weight", StyleParamKey::font_weight},
    {"interactive", StyleParamKey::interactive},
    {"join", StyleParamKey::join},
    {"miter_limit", StyleParamKey::miter_limit},
    {"none", StyleParamKey::none},
    {"offset", StyleParamKey::offset},
    {"order", StyleParamKey::order},
    {"outline:cap", StyleParamKey::outline_cap},
    {"outline:color", StyleParamKey::outline_color},
    {"outline:join", StyleParamKey::outline_join},
    {"outline:miter_limit", StyleParamKey::outline_miter_limit},
    {"outline:order", StyleParamKey::outline_order},
    {"outline:width", StyleParamKey::outline_width},
    {"outline:style", StyleParamKey::outline_style},
    {"priority", StyleParamKey::priority},
    {"repeat_distance", StyleParamKey::repeat_distance},
    {"repeat_group", StyleParamKey::repeat_group},
    {"size", StyleParamKey::size},
    {"sprite", StyleParamKey::sprite},
    {"sprite_default", StyleParamKey::sprite_default},
    {"style", StyleParamKey::style},
    {"text_source", StyleParamKey::text_source},
    {"text_wrap", StyleParamKey::text_wrap},
    {"tile_edges", StyleParamKey::tile_edges},
    {"transition:hide:time", StyleParamKey::transition_hide_time},
    {"transition:selected:time", StyleParamKey::transition_selected_time},
    {"transition:show:time", StyleParamKey::transition_show_time},
    {"visible", StyleParamKey::visible},
    {"width", StyleParamKey::width},
};

const std::map<StyleParamKey, std::vector<Unit>> s_StyleParamUnits = {
    {StyleParamKey::offset, {Unit::pixel}},
    {StyleParamKey::size, {Unit::pixel}},
    {StyleParamKey::font_stroke_width, {Unit::pixel}},
    {StyleParamKey::width, {Unit::meter, Unit::pixel}},
    {StyleParamKey::outline_width, {Unit::meter, Unit::pixel}}
};

static int parseInt(const std::string& str, int& value) {
    try {
        size_t index;
        value = std::stoi(str, &index);
        return index;
    } catch (std::invalid_argument) {
    } catch (std::out_of_range) {}
    LOGW("Not an Integer '%s'", str.c_str());

    return -1;
}

static int parseFloat(const std::string& str, double& value) {
    try {
        size_t index;
        value = std::stof(str, &index);
        return index;
    } catch (std::invalid_argument) {
    } catch (std::out_of_range) {}
    LOGW("Not a Float '%s'", str.c_str());

    return -1;
}

const std::string& StyleParam::keyName(StyleParamKey key) {
    static std::string fallback = "bug";
    for (const auto& entry : s_StyleParamMap) {
        if (entry.second == key) { return entry.first; }
    }
    return fallback;
}

StyleParamKey StyleParam::getKey(const std::string& key) {
    auto it = s_StyleParamMap.find(key);
    if (it == s_StyleParamMap.end()) {
        return StyleParamKey::none;
    }
    return it->second;
}

StyleParam::StyleParam(const std::string& keyStr, const std::string& valueStr) {
    key = getKey(keyStr);
    value = none_type{};

    if (key == StyleParamKey::none) {
        LOGW("Unknown StyleParam %s:%s", keyStr.c_str(), valueStr.c_str());
        return;
    }
    if (!valueStr.empty()) {
        value = parseString(key, valueStr);
    }
}

StyleParam::Value StyleParam::parseString(StyleParamKey key, const std::string& valueStr) {

    switch (key) {
    case StyleParamKey::extrude: {
        return parseExtrudeString(valueStr);
    }
    case StyleParamKey::text_wrap: {
        int textWrap;
        if (valueStr == "true") return textWrap;
        if (valueStr == "false") return std::numeric_limits<uint32_t>::max();
        if (parseInt(valueStr, textWrap) > 0) {
             return static_cast<uint32_t>(textWrap);
        }
    }
    case StyleParamKey::offset: {
        auto vec2 = glm::vec2(0.f, 0.f);
        if (!parseVec2(valueStr, { Unit::pixel }, vec2) || std::isnan(vec2.y)) {
            LOGW("Invalid offset parameter '%s'.", valueStr.c_str());
        }
        return vec2;
    }
    case StyleParamKey::size: {
        auto vec2 = glm::vec2(0.f, 0.f);
        if (!parseVec2(valueStr, { Unit::pixel }, vec2)) {
            LOGW("Invalid size parameter '%s'.", valueStr.c_str());
        }
        return vec2;
    }
    case StyleParamKey::transition_hide_time:
    case StyleParamKey::transition_show_time:
    case StyleParamKey::transition_selected_time: {
        float time = 0.0f;
        if (!parseTime(valueStr, time)) {
            LOGW("Invalid time param '%s'", valueStr.c_str());
        }
        return time;
    }
    case StyleParamKey::font_family:
    case StyleParamKey::font_weight:
    case StyleParamKey::font_style: {
        std::string normalized = valueStr;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        return normalized;
    }
    case StyleParamKey::align:
    case StyleParamKey::anchor:
    case StyleParamKey::text_source:
    case StyleParamKey::transform:
    case StyleParamKey::sprite:
    case StyleParamKey::sprite_default:
    case StyleParamKey::style:
    case StyleParamKey::outline_style:
    case StyleParamKey::repeat_group:
        return valueStr;
    case StyleParamKey::font_size: {
        float fontSize = 0.f;
        if (!parseFontSize(valueStr, fontSize)) {
            LOGW("Invalid font-size '%s'.", valueStr.c_str());
        }
        return fontSize;
    }
    case StyleParamKey::centroid:
    case StyleParamKey::interactive:
    case StyleParamKey::tile_edges:
    case StyleParamKey::visible:
    case StyleParamKey::collide:
        if (valueStr == "true") { return true; }
        if (valueStr == "false") { return false; }
        LOGW("Bool value required for capitalized/visible. Using Default.");
        break;
    case StyleParamKey::order:
    case StyleParamKey::outline_order:
    case StyleParamKey::priority: {
        int num;
        if (parseInt(valueStr, num) > 0) {
             return static_cast<uint32_t>(num);
        }
        break;
    }
    case StyleParamKey::repeat_distance: {
        ValueUnitPair repeatDistance;
        repeatDistance.unit = Unit::pixel;

        int pos = parseValueUnitPair(valueStr, 0, repeatDistance);
        if (pos < 0) {
            LOGW("Invalid repeat distance value '%s'", valueStr.c_str());
            repeatDistance.value = 256.0f;
            repeatDistance.unit = Unit::pixel;
        } else {
            if (repeatDistance.unit != Unit::pixel) {
                LOGW("Invalid unit provided for repeat distance");
            }
        }

        return Width(repeatDistance);
    }
    case StyleParamKey::width:
    case StyleParamKey::outline_width: {
        ValueUnitPair width;
        width.unit = Unit::meter;

        int pos = parseValueUnitPair(valueStr, 0, width);
        if (pos < 0) {
            LOGW("Invalid width value '%s'", valueStr.c_str());
            width.value =  2.0f;
            width.unit = Unit::pixel;
        }

        return Width(width);
    }
    case StyleParamKey::miter_limit:
    case StyleParamKey::outline_miter_limit:
    case StyleParamKey::font_stroke_width: {
        double num;
        if (parseFloat(valueStr, num) > 0) {
             return static_cast<float>(num);
        }
        break;
    }

    case StyleParamKey::color:
    case StyleParamKey::outline_color:
    case StyleParamKey::font_fill:
    case StyleParamKey::font_stroke_color:
        return parseColor(valueStr);

    case StyleParamKey::cap:
    case StyleParamKey::outline_cap:
        return static_cast<uint32_t>(CapTypeFromString(valueStr));

    case StyleParamKey::join:
    case StyleParamKey::outline_join:
        return static_cast<uint32_t>(JoinTypeFromString(valueStr));

    default:
        break;
    }

    return none_type{};
}

std::string StyleParam::toString() const {

    std::string k(keyName(key));
    k += " : ";

    // TODO: cap, join and color toString()
    if (value.is<none_type>()) {
        return k + "none";
    }

    switch (key) {
    case StyleParamKey::extrude: {
        if (!value.is<glm::vec2>()) break;
        auto p = value.get<glm::vec2>();
        return k + "(" + std::to_string(p[0]) + ", " + std::to_string(p[1]) + ")";
    }
    case StyleParamKey::size:
    case StyleParamKey::offset: {
        if (!value.is<glm::vec2>()) break;
        auto p = value.get<glm::vec2>();
        return k + "(" + std::to_string(p.x) + "px, " + std::to_string(p.y) + "px)";
    }
    case StyleParamKey::transition_hide_time:
    case StyleParamKey::transition_show_time:
    case StyleParamKey::transition_selected_time:
    case StyleParamKey::font_family:
    case StyleParamKey::font_weight:
    case StyleParamKey::font_style:
    case StyleParamKey::text_source:
    case StyleParamKey::transform:
    case StyleParamKey::text_wrap:
    case StyleParamKey::repeat_group:
    case StyleParamKey::sprite:
    case StyleParamKey::sprite_default:
    case StyleParamKey::style:
    case StyleParamKey::align:
    case StyleParamKey::anchor:
        if (!value.is<std::string>()) break;
        return k + value.get<std::string>();
    case StyleParamKey::interactive:
    case StyleParamKey::tile_edges:
    case StyleParamKey::visible:
    case StyleParamKey::centroid:
    case StyleParamKey::collide:
        if (!value.is<bool>()) break;
        return k + std::to_string(value.get<bool>());
    case StyleParamKey::width:
    case StyleParamKey::outline_width:
    case StyleParamKey::font_stroke_width:
    case StyleParamKey::font_size:
        if (!value.is<Width>()) break;
        return k + std::to_string(value.get<Width>().value);
    case StyleParamKey::order:
    case StyleParamKey::outline_order:
    case StyleParamKey::priority:
    case StyleParamKey::color:
    case StyleParamKey::outline_color:
    case StyleParamKey::font_fill:
    case StyleParamKey::font_stroke_color:
    case StyleParamKey::repeat_distance:
    case StyleParamKey::cap:
    case StyleParamKey::outline_cap:
    case StyleParamKey::join:
    case StyleParamKey::outline_join:
        if (!value.is<uint32_t>()) break;
        return k + std::to_string(value.get<uint32_t>());
    case StyleParamKey::miter_limit:
    case StyleParamKey::outline_miter_limit:
        if (!value.is<float>()) break;
        return k + std::to_string(value.get<float>());
    default:
        break;
    }

    if (value.is<std::string>()) {
        return k + "wrong type: " + value.get<std::string>();

    }

    return k + "undefined " + std::to_string(static_cast<uint8_t>(key));
}

int StyleParam::parseValueUnitPair(const std::string& value, size_t start,
                                   StyleParam::ValueUnitPair& result) {

    static const std::vector<std::string> units = { "px", "ms", "m", "s" };

    if (start >= value.length()) { return -1; }

    float num;
    int end;

    int ok = std::sscanf(value.c_str() + start, "%f%n", &num, &end);

    if (!ok) { return -1; }

    result.value = static_cast<float>(num);

    start += end;

    if (start >= value.length()) { return start; }

    for (size_t i = 0; i < units.size(); ++i) {
        const auto& unit = units[i];
        std::string valueUnit;
        if (unit == value.substr(start, std::min<int>(value.length(), unit.length()))) {
            result.unit = static_cast<Unit>(i);
            start += unit.length();
            break;
        }
    }

    // TODO skip whitespace , whitespace
    return std::min(value.length(), start + 1);
}

bool StyleParam::parseTime(const std::string &value, float &time) {
    ValueUnitPair p;

    if (!parseValueUnitPair(value, 0, p)) {
        return false;
    }

    switch (p.unit) {
        case Unit::milliseconds:
            time = p.value / 1000.f;
            break;
        case Unit::seconds:
            time = p.value;
            break;
        default:
            LOGW("Invalid unit provided for time %s", value.c_str());
            return false;
            break;
    }

    return true;
}

bool StyleParam::parseVec2(const std::string& value, const std::vector<Unit> units, glm::vec2& vec) {
    ValueUnitPair v1, v2;

    // initialize with defaults
    v1.unit = v2.unit = units[0];

    int pos = parseValueUnitPair(value, 0, v1);
    if (pos < 0) {
        return false;
    }

    if (std::find(units.begin(), units.end(), v1.unit) == units.end()) {
        return false;
    }

    pos = parseValueUnitPair(value, pos, v2);
    if (pos < 0) {
        vec = { v1.value, NAN };
        return true;
    }

    if (std::find(units.begin(), units.end(), v1.unit) == units.end()) {
        return false;
    }

    vec = { v1.value, v2.value };
    return true;
}

uint32_t StyleParam::parseColor(const std::string& colorStr) {
    Color color;

    if (isdigit(colorStr.front())) {
        // try to parse as comma-separated rgba components
        float r, g, b, a = 1.;
        if (sscanf(colorStr.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) >= 3) {
            color = Color {
                static_cast<uint8_t>(CLAMP((r * 255.), 0, 255)),
                static_cast<uint8_t>(CLAMP((g * 255.), 0, 255)),
                static_cast<uint8_t>(CLAMP((b * 255.), 0, 255)),
                CLAMP(a, 0, 1)
            };
        }
    } else {
        // parse as css color or #hex-num
        color = CSSColorParser::parse(colorStr);
    }
    return color.getInt();
}

bool StyleParam::parseFontSize(const std::string& str, float& pxSize) {
    if (str.empty()) {
        return false;
    }

    double num;
    int index = parseFloat(str, num);
    if (index < 0) { return false; }

    pxSize = static_cast<float>(num);

    if (size_t(index) == str.length() && (str.find('.') == std::string::npos)) {
        return true;
    }

    size_t end = str.length() - 1;

    if (str.compare(index, end, "px") == 0) {
        return true;
    } else if (str.compare(index, end, "em") == 0) {
        pxSize *= 16.f;
    } else if (str.compare(index, end, "pt") == 0) {
        pxSize /= 0.75f;
    } else if (str.compare(index, end, "%") == 0) {
        pxSize /= 6.25f;
    } else {
        return false;
    }

    return true;
}

bool StyleParam::isColor(StyleParamKey key) {
    switch (key) {
        case StyleParamKey::color:
        case StyleParamKey::outline_color:
        case StyleParamKey::font_fill:
        case StyleParamKey::font_stroke_color:
            return true;
        default:
            return false;
    }
}

bool StyleParam::isWidth(StyleParamKey key) {
    switch (key) {
        case StyleParamKey::width:
        case StyleParamKey::outline_width:
        case StyleParamKey::size:
        case StyleParamKey::font_stroke_width:
            return true;
        default:
            return false;
    }
}

bool StyleParam::isOffsets(StyleParamKey key) {
    switch (key) {
        case StyleParamKey::offset:
            return true;
        default:
            return false;
    }
}

bool StyleParam::isFontSize(StyleParamKey key) {
    switch (key) {
        case StyleParamKey::font_size:
            return true;
        default:
            return false;
    }
}

bool StyleParam::isRequired(StyleParamKey key) {
    static const std::vector<StyleParamKey> requiredKeys =
        { StyleParamKey::color, StyleParamKey::order, StyleParamKey::width };

    return std::find(requiredKeys.begin(), requiredKeys.end(), key) != requiredKeys.end();
}

bool StyleParam::unitsForStyleParam(StyleParamKey key, std::vector<Unit>& unit) {
    auto it = s_StyleParamUnits.find(key);
    if (it != s_StyleParamUnits.end()) {
        unit = it->second;
        return true;
    }
    return false;
}

}
