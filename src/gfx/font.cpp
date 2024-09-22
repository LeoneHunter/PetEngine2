#include "font.h"
#include "base/util.h"

#include <ft2build.h>
#include <freetype/ftadvanc.h>
#include <freetype/ftimage.h>
#include <freetype/ftbitmap.h>
#include <freetype/freetype.h>
#include <freetype/ftlcdfil.h>
#include <freetype/ftmodapi.h>
#include <freetype/ftmm.h>
#include <freetype/ftoutln.h>
#include <freetype/ftsizes.h>
#include <freetype/ftsystem.h>
#include <freetype/tttables.h>
#include <freetype/t1tables.h>
#include <freetype/ftfntfmt.h>

namespace {

// FT uses 16.16 fixed point types for some parameters
constexpr float FTFixedToFloat(FT_Fixed val) {
    return val * 1.52587890625e-5f;
}

constexpr uint16_t FTFixedToInt(FT_Fixed val) {
    return uint16_t(val >> 16);
}

// Byte tags used in variation axes and other font parameters as short
// identifiers
constexpr uint32_t MakeByteTag(char a, char b, char c, char d) {
    return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 |
           (uint32_t)d << 0;
}

constexpr std::string ParseByteTag(uint32_t tag) {
    return {(char)((tag & 0xFF000000) >> 24), (char)((tag & 0x00FF0000) >> 16),
            (char)((tag & 0x0000FF00) >> 8), (char)((tag & 0x000000FF) >> 0)};
}

constexpr uint32_t kWeightByteTag = MakeByteTag('w', 'g', 'h', 't');
constexpr uint32_t kWidthByteTag = MakeByteTag('w', 'd', 't', 'h');
constexpr uint32_t kSlantByteTag = MakeByteTag('s', 'l', 'n', 't');

// Static, leaks
FT_Library library = nullptr;
// Index of a default face in a file
constexpr auto kFaceIndex = 0;

} // namespace


std::expected<RefCountedPtr<FontTypeface>, ErrorCode> FontTypeface::Create(
    std::span<char> data) {

    if(!library) {
        FT_Init_FreeType(&library);
    }
    AutoFreePtr<FT_FaceRec_, Deleter<FT_Done_Face>> face;
    FT_Open_Args open_args = {
        FT_OPEN_MEMORY,
        reinterpret_cast<FT_Byte*>(data.data()),
        static_cast<FT_Long>(data.size())
    };

    if(FT_Open_Face(library, &open_args, kFaceIndex, &face)) {
        return std::unexpected(ErrorCode::CannotOpenFile);
    }
    
    // Retreive face parameters: weight, width, slant
    // Or variation axes parameters
    FontTypeface::Metrics metrics{};
    metrics.isMonospace = face->face_flags & FT_FACE_FLAG_FIXED_WIDTH;
    metrics.isBold = face->style_flags & FT_STYLE_FLAG_BOLD;
    metrics.isItalic = face->style_flags & FT_STYLE_FLAG_ITALIC;
    metrics.familyName = face->family_name;
    metrics.styleName = face->style_name;
    metrics.hasAxes = face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS;

    // Os/2 table with metrics data
    const TT_OS2* ttOs2 =
        static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
    const bool hasOs2 = ttOs2 && ttOs2->version != 0xffff;

    if(hasOs2) {
        metrics.weight = ttOs2->usWeightClass;
        metrics.width = ttOs2->usWidthClass;
    }

    // Parse variations of variable fonts
    AutoFreePtr<FT_MM_Var> variations;
    // Default axes values
    std::vector<FT_Fixed> designCoordinates;

    if(FT_Get_MM_Var(face, &variations) == FT_Err_Ok) {
        metrics.axes.reserve(variations->num_axis);
        designCoordinates.resize(variations->num_axis);
        const FT_Error result = FT_Get_Var_Design_Coordinates(
            face, variations->num_axis, designCoordinates.data());
        const bool hasCoords = result == FT_Err_Ok;

        for (FT_UInt i = 0; i < variations->num_axis; ++i) {
            const FT_Var_Axis& axis = variations->axis[i];
            VariationAxis& out = metrics.axes.emplace_back();
            out.name = ParseByteTag(axis.tag);
            out.min = FTFixedToFloat(axis.minimum);
            out.def = FTFixedToFloat(axis.def);
            out.max = FTFixedToFloat(axis.maximum);

            // Get weight, width and slant paramters from axes values
            switch(axis.tag) {
                case kWeightByteTag: {
                    metrics.weight = hasCoords
                                         ? FTFixedToInt(designCoordinates[i])
                                         : metrics.weight;
                }
                break;
                case kWidthByteTag: {
                    metrics.width = hasCoords
                                        ? FTFixedToInt(designCoordinates[i])
                                        : metrics.width;
                }
                break;
            }
        }
    }
    DASSERT_F(metrics.weight > 0, "Font {} weight cannot be determined",
              metrics.familyName);

    auto out = RefCountedPtr(new FontTypeface());
    out->face_ = face.release();
    out->metrics_ = metrics;
    return out;
}

FontTypeface::~FontTypeface() {
    FT_Done_Face(face_);
}