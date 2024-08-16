#pragma once
#include "base/common.h"
#include "base/util.h"

#include <iostream>
#include <span>


namespace prototyping {

class Error {
public:

    enum class Code {
        Ok, 
        Error
    };

    std::string_view Message() { return ""; }

	constexpr static Error Ok() { return {Code::Ok}; }

    constexpr operator bool() const { return code != Code::Ok; }

    Code code;
};













using namespace std::literals;
using Color = ColorFloat4;

// Style flags
enum class FontStyleFlags {
	Normal        = 0x0,
	Italic        = 0x1,
	StrikeThrough = 0x2,
	Underline     = 0x4,
};
DEFINE_ENUM_FLAGS_OPERATORS(FontStyleFlags)

// From OpenType
enum class FontWeight {
	Thin       = 100,
	ExtraLight = 200,
	Light      = 300,
	Normal     = 400,
	Medium     = 500,
	Semibold   = 600,
	Bold       = 700,
	ExtraBold  = 800,
	Black      = 900,
};

enum class TextOrientation {
	Horizontal,
	VerticalRight,
	VerticalLeft,
};



/*
* An entry into a ui theme
* Can be referenced by TextStyle entries
* Will be loaded on initialization
* Usage:
* 	auto* theme = Application::GetTheme();
*	auto& font = theme->Add<FontFace>();
*	font.SetFamily("Arial")
*		.SetFilename("fonts/arial-normal.ttf")
*		.SetWeight(FontWeight::Normal)
*		.SetItalics(true);
*/
struct FontFace {
	std::string	filename; 
	std::string family;
	int  		weight;
	bool		italic;	
};





/* 
* Defines a font parameters to be used to draw a text
* Highest-level font description
* Used in a TextBox widget
*/
struct FontStyleParams {
	std::string    family = "arial";
	uint32_t            size   = 13;
	Color          color  = 0xffffff00;
	FontWeight     weight = FontWeight::Normal;
	FontStyleFlags style  = FontStyleFlags::Normal;
};




struct GlyphMetrics {
	uint32_t advanceX;
	uint32_t advanceY;
};

/*
* Freetype wrapper
* TODO: handle colored glyphs
*/
class FreetypeReader {
public:

	static std::unique_ptr<FreetypeReader> New();

	NODISCARD bool InitFromMemory(std::span<uint8_t> data, uint32_t size);

	bool LoadGlyphForChar(char32_t ch);

	GlyphMetrics GetGlyphMetrics();

	NODISCARD std::span<uint8_t> RasterizeGlyph();
};




using Point = float2;

// Low level rendering API
// Simple thread-safe rendering front-end
// when we dont care about threads and command buffers
namespace rendering {

// TODO: decide which ownership model to use
struct TextureHandle {
	int t;
	// std::shared_ptr<int> texture;
};

struct CommandList {
	// Actual buffer
	// std::shared_ptr<int> cmdList;
};

// Creates a new texture syncronously possibly blocking this thread
inline TextureHandle CreateTextureSync(std::span<uint8_t> bitmap, int format) { return {}; }

// Creates a new command list used to record rendering commands
// TODO: maybe use more high level abstraction like a render pass or render graph node
// Maybe use more declarative approach, like declare a render pass, input and output resources, submit the graph
inline CommandList NewCommandList() { return {}; }

} // namespace rendering

