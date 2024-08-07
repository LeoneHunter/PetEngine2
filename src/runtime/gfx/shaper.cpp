#include "shaper.h"

#include <hb-face.hh>
#include <set>

constexpr int kDefaultFaceIndex = 0;

HBFaceUniquePtr CreateHbFaceFromFace(FontTypeface* face) {
    HBFaceUniquePtr hbFace;
    HBBlobUniquePtr hbBlob;

    std::span<const char> fontData = face->GetData();
    DASSERT(!fontData.empty());

    hbBlob.reset(hb_blob_create(fontData.data(), fontData.size(),
                                HB_MEMORY_MODE_READONLY, nullptr, nullptr));
    DASSERT(hbBlob);
    hbFace.reset(hb_face_create(hbBlob.get(), kDefaultFaceIndex));
    return hbFace;
}

hb_face_t* TextShaper::CacheFindFace(FontTypeface* face) {
    hb_face_t* cachedFace = nullptr;
    // - Find or create a face
    // - Place the face at the start of the cache
    // - If face added and face num > kFaceCacheSize -> pop_back
    if (!faceCache_.empty()) {
        auto it = std::ranges::find_if(
            faceCache_, [&](const HBFaceUniquePtr& cached) -> bool {
                return cached->user_data == face;
            });
        if (it != faceCache_.end()) {
            cachedFace = it->get();
        }
    }
    if (!cachedFace) {
        HBFaceUniquePtr newFace = CreateHbFaceFromFace(face);
        DASSERT(newFace);
        // Use the |face| as a key
        // NOTE: Must not be accessed because it has no ownership
        newFace->user_data = face;
        faceCache_.insert(faceCache_.begin(), std::move(newFace));
    }
    if (faceCache_.size() > kFaceCacheSize) {
        faceCache_.pop_back();
    }
    return cachedFace;
}

GlyphRun TextShaper::Shape(std::string_view text, Font* font) {
    // - Cache the font - hb_font_t
    // - Cache the glyph requests from harfbuzz
    hb_face_t* hbFace = CacheFindFace(font->GetFace());
    HBFontUniquePtr hbFont;
    hbFont.reset(hb_font_create(hbFace));
    DASSERT(hbFont);
    hb_ot_font_set_funcs(hbFont.get());
    // TODO: Should we set variation axes explicitly
    // hb_font_set_variations()
    hb_font_set_scale(hbFont.get(), 1, 1);
    // Set size for hinting
    hb_font_set_ppem(hbFont.get(), font->GetSize(), font->GetSize());

    GlyphRun out;
    // - Check the glyph in the cache
    // - If not in cache rasterize the glyph starting from the font at the
    // top of the stack
    if (text.empty()) {
        return out;
    }
    // hb_ft_font_set_funcs(font);
    hb_buffer_t* buffer = buffer_.get();
    InvokeOnDestruct<hb_buffer_clear_contents, hb_buffer_t*> autoClearBuffer(
        buffer);
    hb_buffer_set_content_type(buffer, HB_BUFFER_CONTENT_TYPE_UNICODE);
    hb_buffer_set_cluster_level(buffer,
                                HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);

    // Add chars into the buffer automatically replacing invalid
    DASSERT(text.size() < std::numeric_limits<unsigned int>::max());
    hb_buffer_add_utf8(buffer, text.data(),
                       static_cast<unsigned int>(text.size()), 0, text.size());

    // Use LTR explicitly
    hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    // TODO: Identify the script and language from the text
    hb_buffer_set_script(buffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
    hb_buffer_guess_segment_properties(buffer);

    // TODO: Implement some OpenType features. Null for now
    hb_shape(hbFont.get(), buffer, nullptr, 0);

    const unsigned bufSize = hb_buffer_get_length(buffer);
    if (!bufSize) {
        return out;
    }
    // Contains GlyphID into the |font| for the corresponding codepoint
    const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, nullptr);
    // Contains glyph parameters: offset and advance
    const hb_glyph_position_t* pos =
        hb_buffer_get_glyph_positions(buffer, nullptr);
    // Allocate run arrays and copy |hb_buffer| data into them
    out.Reserve(bufSize);

    for (size_t glyphIdx = 0; glyphIdx < bufSize; ++glyphIdx) {
        Glyph& glyph = out.EmplaceBack();
        glyph.glyphIndex = info[glyphIdx].codepoint;
        glyph.advanceX = pos[glyphIdx].x_advance;
    }
    return out;
}
