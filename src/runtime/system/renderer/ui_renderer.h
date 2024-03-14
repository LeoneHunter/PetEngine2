#pragma once
#include "runtime/core/core.h"
#include "runtime/core/util.h"
#include "runtime/platform/native_window.h"

class Image;
namespace UI {
	class Font;
}

struct __TextureHandle {};
// Opaque handle to the texture stored in the VRAM managed by the Renderer
using TextureHandle = __TextureHandle*;


/**
 * Used for image rendering
 */
struct ImageInfo {
	TextureHandle	TextureHandle = 0;
	float2			TextureSize;
};

/**
 * Interface to the Draw lists
 * For now we will use draw list from Ocornut::ImGui
 * Draw list to which views append draw commands
 * Positions in the Screen Coordinates
 */
class DrawList {
public:

	enum class Corner {
		None = 0, 
		TL = 0x1, 
		TR = 0x2, 
		BL = 0x4, 
		BR = 0x8,
		All = TL | TR | BL | BR,
	};
	DEFINE_ENUM_FLAGS_OPERATORS_FRIEND(Corner)

	virtual void DrawLine(const float2& inStart, const float2& inEnd, ColorU32 inColor, float inThickness = 1.0f) = 0;
		
	virtual void DrawRect(const Rect& inRect, ColorU32 inColor, u8 inRounding = 0, Corner inCornerMask = Corner::None, float inThickness = 1.0f) = 0;
	virtual void DrawRectFilled(const Rect& inRect, ColorU32 inColor, u8 inRounding = 0, Corner inCornerMask = Corner::None) = 0;
	virtual void DrawRectFilled(float2 inMin, float2 inMax, ColorU32 inColor, u8 inRounding = 0, Corner inCornerMask = Corner::None) = 0;

	virtual void DrawText(const float2& inMin, ColorU32 inColor, const std::string_view& inText, u8 inFontSize = 0, bool bBold = false, bool bItalic = false) = 0;
	virtual void DrawText(const float2& inMin, ColorU32 inColor, const std::wstring_view& inText, u8 inFontSize = 0, bool bBold = false, bool bItalic = false) = 0;
	virtual void DrawBezier(float2 p1, float2 p2, float2 p3, float2 p4, ColorU32 col, float thickness, unsigned segmentNum = 30) = 0;
	virtual void DrawTexture(const Rect& inRect, TextureHandle inTextureHandle, ColorU32 inTintColor, float2 inUVMin = {0.f, 0.f}, float2 inUVMax = {1.f, 1.f}) = 0;

	virtual void PushClipRect(const Rect& inRect) = 0;
	virtual void PopClipRect() = 0;

	/**
	 * Set font for subsequent DrawText() commands
	 */
	virtual void PushFont(const UI::Font* inFont, u8 inDefaultSize, bool bBold = false, bool bItalic = false) = 0;
	virtual void PopFont() = 0;

	/**
	 * Sets alpha applied to all following commands
	 */
	virtual void SetAlpha(float inAlpha) = 0;

	/**
	 * Deletes all commands and prepares for a new frame
	 */
	virtual void Reset() = 0;

	virtual ~DrawList() = default;
};

/**
 * Rendering backend wich renders our UI on the window
 */
class Renderer {
public:

	/**
	 * Initialize renderer objects
	 * Initialize imgui
	 */
	virtual bool Init(INativeWindow* inNativeWindow) = 0;
	virtual void ResetDrawLists() = 0;
	virtual DrawList* GetFrameDrawList() = 0;
	virtual void RenderFrame(bool bVsync) = 0;
	virtual void ResizeFramebuffers(float2 inSize) = 0;

	virtual TextureHandle CreateTexture(const Image& inImage) = 0;
	virtual void DeleteTexture(TextureHandle inTexture) = 0;

	virtual ~Renderer() = default;
};

/**
 * Factory method for a DX12 renderer
 */
Renderer* CreateRendererDX12();