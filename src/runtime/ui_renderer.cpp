#include "ui_renderer.h"
#include "common.h"
#include "editor/ui/font.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

#include <random>
#include <map>
#include <stack>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler")
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

#undef DrawText

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_internal.h"





using uint8 = uint8_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

#define check(hResult) Assertf(!FAILED(hResult), "HRESULT failed")
#define checkhr(hResult) Assertf(!FAILED(hResult), "HRESULT failed")

template<typename T>
static inline void SafeRelease(T*& res) {
	if(res)
		res->Release();
	res = NULL;
}

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct FrameContext {
	ID3D12CommandAllocator* CommandAllocator;
	UINT64                  FenceValue;
};

struct ImGui_ImplDX12_RenderBuffers {
	ID3D12Resource*		IndexBuffer;
	ID3D12Resource*		VertexBuffer;
	int                 IndexBufferSize;
	int                 VertexBufferSize;
};

struct VERTEX_CONSTANT_BUFFER {
	float   mvp[4][4];
};

/**
 * Independent draw list extracted from the "Dear ImGui" github.com/ocornut/imgui
 */
class ImGuiDrawListWrapper_Independent final: public DrawList {
public:

	ImGuiDrawListWrapper_Independent(float2 inWindowSize)
		: m_Alpha(1.f)
	{		
		m_ImDrawData = std::make_unique<ImDrawData>();
		m_ImDrawListSharedData = std::make_unique<ImDrawListSharedData>();
		m_ImDrawList = std::make_unique<ImDrawList>(m_ImDrawListSharedData.get());
		m_ImDrawLists.push_back(m_ImDrawList.get());

		// Init draw data
		m_ImDrawData->CmdLists = m_ImDrawLists.data();
		m_ImDrawData->CmdListsCount = 1;

		m_ImDrawData->DisplayPos = {0.f, 0.f};
		m_ImDrawData->DisplaySize = {inWindowSize};

		m_ImDrawData->FramebufferScale = {1.f, 1.f};

		m_ImDrawListSharedData->ClipRectFullscreen = {0.f, 0.f, inWindowSize.x, inWindowSize.y};
		m_ImDrawListSharedData->CurveTessellationTol = 1.25;
		m_ImDrawListSharedData->SetCircleTessellationMaxError(0.3f);

		m_ImDrawListSharedData->InitialFlags = ImDrawListFlags_None;		
		m_ImDrawListSharedData->InitialFlags |= ImDrawListFlags_AntiAliasedLines;
		m_ImDrawListSharedData->InitialFlags |= ImDrawListFlags_AntiAliasedLinesUseTex;
		m_ImDrawListSharedData->InitialFlags |= ImDrawListFlags_AntiAliasedFill;		
		m_ImDrawListSharedData->InitialFlags |= ImDrawListFlags_AllowVtxOffset;

		Reset();
	}

	void DrawLine(const float2& inStart, const float2& inEnd, ColorU32 inColor, float inThickness) final {
		const auto p1 = math::Round(inStart);
		const auto p2 = math::Round(inEnd);
		//const auto color = float4(inColor.x, inColor.y, inColor.z, inColor.w * m_Alpha);
		
		m_ImDrawList->AddLine(p1, p2, inColor.MultiplyAlpha(m_Alpha), inThickness);
	}

	// Draws error indication when size is 0
	void DrawErrorRect(float2 inMin, float2 inMax) {

		constexpr auto ErrorColor1 = ColorU32::FromHex("#FF36B4");
		constexpr auto ErrorColor2 = ColorU32::FromHex("#FFF61F");
		constexpr auto ErrorSize = 4;
		constexpr auto ErrorOffset = float2(4);

		// If the caller tries to draw a box with 0 size, draw a purple rect as an indication
		for(auto axis = 0; axis < 2; axis++) {

			if(inMax[axis] - inMin[axis] <= 0) {
				inMax[axis] = inMin[axis] + ErrorSize;
			}
		}
		m_ImDrawList->AddRectFilled(inMin, inMax, ErrorColor2);
		m_ImDrawList->AddRectFilled(inMin += ErrorOffset, inMax += ErrorOffset, ErrorColor1);
		m_ImDrawList->AddRectFilled(inMin += ErrorOffset, inMax += ErrorOffset, ErrorColor2);
		m_ImDrawList->AddRectFilled(inMin += ErrorOffset, inMax += ErrorOffset, ErrorColor1);
	}

	void DrawRect(const Rect& inRect, ColorU32 inColor, uint8_t inRounding, Corner inCornerMask, float inThickness) final {

		if(inRect.Size().x <= 0 || inRect.Size().y <= 0) {
			return DrawErrorRect(inRect.min, inRect.max);
		}

		const auto min = math::Round(inRect.min);
		const auto max = math::Round(inRect.max);
		//const auto color = float4(inColor.x, inColor.y, inColor.z, inColor.w * m_Alpha);

		ImDrawListFlags cornerMask = 0;

		if (inCornerMask & Corner::TL) {
			cornerMask |= ImDrawFlags_RoundCornersTopLeft;
		}
		if(inCornerMask & Corner::TR) {
			cornerMask |= ImDrawFlags_RoundCornersTopRight;
		}
		if(inCornerMask & Corner::BL) {
			cornerMask |= ImDrawFlags_RoundCornersBottomLeft;
		}
		if(inCornerMask & Corner::BR) {
			cornerMask |= ImDrawFlags_RoundCornersBottomRight;
		}

		m_ImDrawList->AddRect(min, max, inColor.MultiplyAlpha(m_Alpha), (float)inRounding, cornerMask, inThickness);
	}
	
	void DrawRectFilled(const Rect& inRect, ColorU32 inColor, uint8_t inRounding, Corner inCornerMask) final {
		DrawRectFilled(inRect.min, inRect.max, inColor, inRounding, inCornerMask);
	}

	void DrawRectFilled(float2 inMin, float2 inMax, ColorU32 inColor, uint8_t inRounding, Corner inCornerMask) final {
		const float2 size = inMax - inMin;

		if(size.x <= 0 || size.y <= 0) {
			return DrawErrorRect(inMin, inMax);
		}

		const auto min = math::Round(inMin);
		const auto max = math::Round(inMax);
		//const auto color = float4(inColor.x, inColor.y, inColor.z, inColor.w * m_Alpha);

		ImDrawListFlags cornerMask = 0;

		if(inCornerMask & Corner::TL) {
			cornerMask |= ImDrawFlags_RoundCornersTopLeft;
		}
		if(inCornerMask & Corner::TR) {
			cornerMask |= ImDrawFlags_RoundCornersTopRight;
		}
		if(inCornerMask & Corner::BL) {
			cornerMask |= ImDrawFlags_RoundCornersBottomLeft;
		}
		if(inCornerMask & Corner::BR) {
			cornerMask |= ImDrawFlags_RoundCornersBottomRight;
		}

		m_ImDrawList->AddRectFilled(min, max, inColor.MultiplyAlpha(m_Alpha), (float)inRounding, cornerMask);
	}

	void DrawText(const float2& inPos, ColorU32 inColor, const std::string_view& inText, uint8_t inFontSize, bool bBold, bool bItalic)  final {
		DrawTextExt<char>(inPos, inColor, inText, inFontSize, bBold, bItalic);
	}

	void DrawText(const float2& inMin, ColorU32 inColor, const std::wstring_view& inText, uint8_t inFontSize, bool bBold, bool bItalic) final {
		DrawTextExt<wchar_t>(inMin, inColor, inText, inFontSize, bBold, bItalic);
	}

	void DrawBezier(float2 p1, float2 p2, float2 p3, float2 p4, ColorU32 col, float thickness, unsigned segmentNum) final {}

	void DrawTexture(const Rect& inRect, TextureHandle inTextureHandle, ColorU32 inTintColor, float2 inUVMin, float2 inUVMax) final {
		if (!inTextureHandle) {
			return;
		}
		const auto min = math::Round(inRect.min);
		const auto max = math::Round(inRect.max);

		m_ImDrawList->AddImage(inTextureHandle, min, max, inUVMin, inUVMax, ImColor(inTintColor));
	}

	void PushClipRect(const Rect& inRect) final {
		const auto min = math::Round(inRect.min);
		const auto max = math::Round(inRect.max);
		m_ImDrawList->PushClipRect(min, max, true);
	}

	void PopClipRect() final {
		m_ImDrawList->PopClipRect();
	}

	void PushFont(const ui::Font* inFont, uint8_t inDefaultSize, bool bBold = false, bool bItalic = false) final {
		const auto* face = inFont->GetFace(inDefaultSize, bBold, bItalic);
		assert(face && "Pushing a font which is not rasterized");

		ui::Font::FaceRenderParameters params;
		face->GetRenderParameters(&params);

		m_ImDrawListSharedData->TexUvWhitePixel = params.TexUvWhitePixelCoords;
		m_ImDrawListSharedData->TexUvLines = (ImVec4*)params.TexUvLinesArrayPtr;

		m_ImDrawList->PushTextureID((ImTextureID)inFont->GetAtlasTexture());
		m_FontFaceStack.push(face);
	}

	void PopFont() final {
		assert(!m_FontFaceStack.empty() && "Font stack is empty!");
		m_FontFaceStack.pop();
		assert(!m_FontFaceStack.empty() && "Font stack is empty!");

		const auto* slice = m_FontFaceStack.top();

		ui::Font::FaceRenderParameters params;
		slice->GetRenderParameters(&params);

		m_ImDrawListSharedData->TexUvWhitePixel = params.TexUvWhitePixelCoords;
		m_ImDrawListSharedData->TexUvLines = (ImVec4*)params.TexUvLinesArrayPtr;
		//m_ImDrawListSharedData->Font = g.Font;
		//m_ImDrawListSharedData->FontSize = g.FontSize;

		m_ImDrawList->PushTextureID((ImTextureID)slice->GetFont()->GetAtlasTexture());
		m_FontFaceStack.push(slice);
	}

	void SetAlpha(float inAlpha) final { m_Alpha = math::Clamp(inAlpha, 0.f, 1.f); }

	ImDrawData* GetDrawData() const { return m_ImDrawData.get(); }

	void UpdateViewportRect(const Rect& inViewportRect) {
		m_ImDrawData->DisplayPos = inViewportRect.min;
		m_ImDrawData->DisplaySize = inViewportRect.Size();
		m_ImDrawListSharedData->ClipRectFullscreen = Vec4(inViewportRect.min.x, inViewportRect.min.y, inViewportRect.max.x, inViewportRect.max.y);

		if(inViewportRect.Width() > 0 && inViewportRect.Height() > 0) {
			m_ImDrawData->FramebufferScale = ImVec2((float)inViewportRect.Width(), (float)inViewportRect.Height());
		}
	}

	void Reset() final {
		m_ImDrawData->TotalVtxCount = 0;
		m_ImDrawData->TotalIdxCount = 0;
		m_ImDrawList->_ResetForNewFrame();
		m_ImDrawList->PushClipRectFullScreen();
		m_FontFaceStack = std::stack<const ui::Font::Face*>();
	}

private:

	template<typename CHAR_T>
	void DrawTextExt(const float2& inPos, ColorU32 inColor, const std::basic_string_view<CHAR_T>&inText, uint8 inFontSize, bool bBold, bool bItalic) {
		const auto pos = math::Round(inPos);
		//const auto color = float4(inColor.x, inColor.y, inColor.z, inColor.w * m_Alpha);

		if(inColor.IsTransparent() || inText.empty()) {
			return;
		}

		assert(!m_FontFaceStack.empty() && "No font has been set. PushFont() should be called to set a default font");
		auto* activeFontFace = m_FontFaceStack.top();

		if(inFontSize == 0.0f) {
			inFontSize = (uint8)activeFontFace->GetSize();
		}

		// If passed parameters of the font are different from the active font - check if font face has already been rasterized
		if(activeFontFace->IsBold() xor bBold || activeFontFace->IsItalic() xor bItalic || inFontSize != activeFontFace->GetSize()) {
			auto newFace = activeFontFace->GetFont()->GetFace(inFontSize, bBold, bItalic);

			if(!newFace) {
				activeFontFace->GetFont()->RasterizeFace(inFontSize, bBold, bItalic);
				return;
			}
			activeFontFace = newFace;
		}

		// From Ocornut ImGui::ImFont
		auto	   draw_list = m_ImDrawList.get();
		const auto cpu_fine_clip = false;
		const auto col = ImU32(ImColor(inColor));
		const auto clip_rect = draw_list->_CmdHeader.ClipRect;
		const auto FontSize = activeFontFace->GetSize();
		const auto wrap_width = 0.f;
		const auto text_begin = inText.data();
		auto	   text_end = inText.data() + inText.size();

		// Align to be pixel perfect
		float x = IM_FLOOR(pos.x);
		float y = IM_FLOOR(pos.y);
		if(y > clip_rect.w)
			return;

		const float start_x = x;
		const float scale = 1.f;
		const float line_height = FontSize * scale;
		const bool word_wrap_enabled = (wrap_width > 0.0f);
		const CHAR_T* word_wrap_eol = NULL;

		// Fast-forward to first visible line
		const auto* s = text_begin;
		if(y + line_height < clip_rect.y && !word_wrap_enabled)
			while(y + line_height < clip_rect.y && s < text_end) {
				s = (const CHAR_T*)memchr(s, '\n', text_end - s);
				s = s ? s + 1 : text_end;
				y += line_height;
			}

		// For large text, scan for the last visible line in order to avoid over-reserving in the call to PrimReserve()
		// Note that very large horizontal line will still be affected by the issue (e.g. a one megabyte string buffer without a newline will likely crash atm)
		if(text_end - s > 10000 && !word_wrap_enabled) {
			const auto* s_end = s;
			float y_end = y;
			while(y_end < clip_rect.w && s_end < text_end) {
				s_end = (const CHAR_T*)memchr(s_end, '\n', text_end - s_end);
				s_end = s_end ? s_end + 1 : text_end;
				y_end += line_height;
			}
			text_end = s_end;
		}
		if(s == text_end)
			return;

		// Reserve vertices for remaining worse case (over-reserving is useful and easily amortized)
		const int vtx_count_max = (int)(text_end - s) * 4;
		const int idx_count_max = (int)(text_end - s) * 6;
		const int idx_expected_size = draw_list->IdxBuffer.Size + idx_count_max;
		draw_list->PrimReserve(idx_count_max, vtx_count_max);

		ImDrawVert* vtx_write = draw_list->_VtxWritePtr;
		ImDrawIdx* idx_write = draw_list->_IdxWritePtr;
		unsigned int vtx_current_idx = draw_list->_VtxCurrentIdx;

		const ImU32 col_untinted = col | ~IM_COL32_A_MASK;

		while(s < text_end) {
			if(word_wrap_enabled) {
				// Calculate how far we can render. Requires two passes on the string data but keeps the code simple and not intrusive for what's essentially an uncommon feature.
				if(!word_wrap_eol) {
					word_wrap_eol = activeFontFace->CalculateWordWrapPosition(std::basic_string_view<CHAR_T>(s, text_end), wrap_width - (x - start_x));
					if(word_wrap_eol == s) // Wrap_width is too small to fit anything. Force displaying 1 character to minimize the height discontinuity.
						word_wrap_eol++;    // +1 may not be a character start point in UTF-8 but it's ok because we use s >= word_wrap_eol below
				}

				if(s >= word_wrap_eol) {
					x = start_x;
					y += line_height;
					word_wrap_eol = NULL;

					// Wrapping skips upcoming blanks
					while(s < text_end) {
						const auto c = *s;
						bool bCharIsBlank = false;

						if constexpr(std::is_same_v<CHAR_T, char>) {
							bCharIsBlank = ImCharIsBlankA(c);
						} else {
							bCharIsBlank = ImCharIsBlankW(c);
						}

						if(bCharIsBlank) { 
							s++; 
						} else if(c == '\n') { 
							s++; 
							break;
						} else { 
							break; 
						}
					}
					continue;
				}
			}

			// Decode and advance source
			unsigned int c = (unsigned int)*s;

			if(c < 0x80) {
				s += 1;
			} else {
				if constexpr(std::is_same_v<CHAR_T, char>) {
					s += ImTextCharFromUtf8(&c, s, text_end);
					if(c == 0) // Malformed UTF-8?
						break;
				} else {
					s += 1;
				}
			}

			if(c < 32) {
				if(c == '\n') {
					x = start_x;
					y += line_height;
					if(y > clip_rect.w)
						break; // break out of main loop
					continue;
				}
				if(c == '\r')
					continue;
			}

			auto glyph = activeFontFace->GetGlyph((ImWchar)c);

			float char_width = glyph.AdvanceX * scale;
			if(glyph.Visible) {
				// We don't do a second finer clipping test on the Y axis as we've already skipped anything before clip_rect.y and exit once we pass clip_rect.w
				float x1 = x + glyph.X0 * scale;
				float x2 = x + glyph.X1 * scale;
				float y1 = y + glyph.Y0 * scale;
				float y2 = y + glyph.Y1 * scale;
				if(x1 <= clip_rect.z && x2 >= clip_rect.x) {
					// Render a character
					float u1 = glyph.U0;
					float v1 = glyph.V0;
					float u2 = glyph.U1;
					float v2 = glyph.V1;

					// CPU side clipping used to fit text in their frame when the frame is too small. Only does clipping for axis aligned quads.
					if(cpu_fine_clip) {
						if(x1 < clip_rect.x) {
							u1 = u1 + (1.0f - (x2 - clip_rect.x) / (x2 - x1)) * (u2 - u1);
							x1 = clip_rect.x;
						}
						if(y1 < clip_rect.y) {
							v1 = v1 + (1.0f - (y2 - clip_rect.y) / (y2 - y1)) * (v2 - v1);
							y1 = clip_rect.y;
						}
						if(x2 > clip_rect.z) {
							u2 = u1 + ((clip_rect.z - x1) / (x2 - x1)) * (u2 - u1);
							x2 = clip_rect.z;
						}
						if(y2 > clip_rect.w) {
							v2 = v1 + ((clip_rect.w - y1) / (y2 - y1)) * (v2 - v1);
							y2 = clip_rect.w;
						}
						if(y1 >= y2) {
							x += char_width;
							continue;
						}
					}

					// Support for untinted glyphs
					ImU32 glyph_col = glyph.Colored ? col_untinted : col;

					// We are NOT calling PrimRectUV() here because non-inlined causes too much overhead in a debug builds. Inlined here:
					{
						idx_write[0] = (ImDrawIdx)(vtx_current_idx); idx_write[1] = (ImDrawIdx)(vtx_current_idx + 1); idx_write[2] = (ImDrawIdx)(vtx_current_idx + 2);
						idx_write[3] = (ImDrawIdx)(vtx_current_idx); idx_write[4] = (ImDrawIdx)(vtx_current_idx + 2); idx_write[5] = (ImDrawIdx)(vtx_current_idx + 3);
						vtx_write[0].pos.x = x1; vtx_write[0].pos.y = y1; vtx_write[0].col = glyph_col; vtx_write[0].uv.x = u1; vtx_write[0].uv.y = v1;
						vtx_write[1].pos.x = x2; vtx_write[1].pos.y = y1; vtx_write[1].col = glyph_col; vtx_write[1].uv.x = u2; vtx_write[1].uv.y = v1;
						vtx_write[2].pos.x = x2; vtx_write[2].pos.y = y2; vtx_write[2].col = glyph_col; vtx_write[2].uv.x = u2; vtx_write[2].uv.y = v2;
						vtx_write[3].pos.x = x1; vtx_write[3].pos.y = y2; vtx_write[3].col = glyph_col; vtx_write[3].uv.x = u1; vtx_write[3].uv.y = v2;
						vtx_write += 4;
						vtx_current_idx += 4;
						idx_write += 6;
					}
				}
			}
			x += char_width;
		}

		// Give back unused vertices (clipped ones, blanks) ~ this is essentially a PrimUnreserve() action.
		draw_list->VtxBuffer.Size = (int)(vtx_write - draw_list->VtxBuffer.Data); // Same as calling shrink()
		draw_list->IdxBuffer.Size = (int)(idx_write - draw_list->IdxBuffer.Data);
		draw_list->CmdBuffer[draw_list->CmdBuffer.Size - 1].ElemCount -= (idx_expected_size - draw_list->IdxBuffer.Size);
		draw_list->_VtxWritePtr = vtx_write;
		draw_list->_IdxWritePtr = idx_write;
		draw_list->_VtxCurrentIdx = vtx_current_idx;
	}

private:
	std::unique_ptr<ImDrawData>				m_ImDrawData;
	std::unique_ptr<ImDrawListSharedData>	m_ImDrawListSharedData;
	std::unique_ptr<ImDrawList>				m_ImDrawList;

	std::vector<ImDrawList*>				m_ImDrawLists;
	float									m_Alpha;

	std::stack<const ui::Font::Face*>		m_FontFaceStack;
};

/**
 * Handle to the GPU resource descriptor
 * Stores essensial info about a resource in the VRAM
 * Passed to the rendering pipeline when a resource binding is required
 * Basically a handle to the resource used by the rendering pipeline
 * TODO: Basic wrapper, needs further refactoring
 */
class Descriptor {
public:

	Descriptor(): m_CPUHandle{.ptr = 0}, m_GPUHandle{.ptr = 0}, m_HeapIndex(0) {}

	Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE inCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE inGPUHandle, uint64 inHeapIndex)
		: m_CPUHandle(inCPUHandle), m_GPUHandle(inGPUHandle), m_HeapIndex(inHeapIndex) {}

	operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return m_CPUHandle; }
	operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return m_GPUHandle; }

	uint64 GetVptrGPU() const { return m_GPUHandle.ptr; }
	SIZE_T GetVptrCPU() const { return m_CPUHandle.ptr; }

	void Offset(uint64 inOffsetInDescriptors, uint32 inDescriptorIncrementSize) {
		m_CPUHandle.ptr += inOffsetInDescriptors * inDescriptorIncrementSize;
		m_GPUHandle.ptr += inOffsetInDescriptors * inDescriptorIncrementSize;
	}

	bool Empty() const { return !m_CPUHandle.ptr; }
	void Clear() { m_CPUHandle.ptr = 0; m_GPUHandle.ptr = 0; m_HeapIndex = 0; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE	m_CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE	m_GPUHandle;
	uint64						m_HeapIndex;
};

/**
 * Helper for managing descriptor heaps
 * TODO:
 *		maybe should be auto* thisHeap = Device->CreateDescriptorHeap(inProperties);
 *		Descriptor heap should be RAII because it owns a memory in the VRAM
 *		Maybe should store an array of freed descriptors
 */
class DescriptorHeap {
public:

	DescriptorHeap()
		: m_DescriptorIncrementSize(0)
		, m_LastFreeDescriptorIndex(0)
		, m_Device(nullptr) {}

	void init(ID3D12Device2* inDevice, const D3D12_DESCRIPTOR_HEAP_DESC& inProperties) {
		m_Props = inProperties;
		m_Device = inDevice;
		auto hr = inDevice->CreateDescriptorHeap(&inProperties, IID_PPV_ARGS(&m_Heap));
		checkhr(hr);
		m_DescriptorHeapStart = Descriptor(m_Heap->GetCPUDescriptorHandleForHeapStart(), m_Heap->GetGPUDescriptorHandleForHeapStart(), m_LastFreeDescriptorIndex);

		m_DescriptorIncrementSize = inDevice->GetDescriptorHandleIncrementSize(inProperties.Type);
		m_LastFreeDescriptorIndex = 0;
	}

	DescriptorHeap(DescriptorHeap&) = delete;
	DescriptorHeap& operator=(DescriptorHeap&) = delete;

	Descriptor operator[](size_t inDescriptorIndex) const {
		check(inDescriptorIndex < m_Props.NumDescriptors);
		Descriptor descriptor = m_DescriptorHeapStart;
		descriptor.Offset(m_LastFreeDescriptorIndex, m_DescriptorIncrementSize);
		return descriptor;
	}

	Descriptor createDescriptor(ID3D12Resource* inResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* inParams) {
		const auto newDescriptor = operator[](m_LastFreeDescriptorIndex);
		++m_LastFreeDescriptorIndex;
		m_Device->CreateShaderResourceView(inResource, inParams, newDescriptor);
		return newDescriptor;
	}

	Descriptor createDescriptor(ID3D12Resource* inResource, const D3D12_RENDER_TARGET_VIEW_DESC* inParams) {
		const auto newDescriptor = operator[](m_LastFreeDescriptorIndex);
		++m_LastFreeDescriptorIndex;
		m_Device->CreateRenderTargetView(inResource, inParams, newDescriptor);
		return newDescriptor;
	}

	void releaseDescriptor(const Descriptor& inDescriptor) {
		// Possibly add this descriptor's index to free list or smth.
		// std::queue m_FreeDescriptors;
		// m_FreeDescriptors.push(inDescriptor.getHeapIndex);
	}

	bool isShaderVisible() const { return m_Props.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; }

	size_t size() const { return m_Props.NumDescriptors; }

	D3D12_DESCRIPTOR_HEAP_TYPE type() const { return m_Props.Type; }

	ID3D12DescriptorHeap* getNative() { return m_Heap.Get(); }

private:
	D3D12_DESCRIPTOR_HEAP_DESC		m_Props;
	Descriptor						m_DescriptorHeapStart;
	uint32							m_DescriptorIncrementSize;
	size_t							m_LastFreeDescriptorIndex;
	ComPtr<ID3D12DescriptorHeap>	m_Heap;
	ID3D12Device2*					m_Device;
};

/**
 * Renders ui using DX12
 * Mostly code from "Dear ImGui" github.com/ocornut/imgui
 * TODO: needs refactor into RAII and separation on proper classes: Device, CommandList, CommandQueue etc.
 */
class RendererDX12 final: public Renderer {
public:

	RendererDX12() = default;

	bool Init(INativeWindow* inNativeWindow) override {

		if (inNativeWindow == nullptr) {
			return false;
		}
		m_NativeWindow = inNativeWindow;		

		// Initialize Direct3D
		if(!CreateDeviceD3D((HWND)m_NativeWindow->GetNativeHandle())) {
			CleanupDeviceD3D();
			assert(true);
			return false;
		}

		m_NativeWindow->Show();

		//ImGui::GetMainViewport()->PlatformHandleRaw = m_NativeWindow->GetNativeHandle();

		/*INT64 perf_frequency, perf_counter;
		if(!::QueryPerformanceFrequency((LARGE_INTEGER*)&perf_frequency))
			return false;
		if(!::QueryPerformanceCounter((LARGE_INTEGER*)&perf_counter))
			return false;

		bd->TicksPerSecond = perf_frequency;
		bd->Time = perf_counter;*/

		// Allocate descriptor heap for textures
		{
			D3D12_DESCRIPTOR_HEAP_DESC props{};
			props.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			props.NumDescriptors = 128;
			props.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			props.NodeMask = 0;
			m_DescriptorHeap_SRV.init(g_pd3dDevice, props);
		}
		
		m_RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_pFrameResources = new ImGui_ImplDX12_RenderBuffers[NUM_FRAMES_IN_FLIGHT];
		m_NumFramesInFlight = NUM_FRAMES_IN_FLIGHT;
		m_FrameIndex = UINT_MAX;

		// Create buffers with a default size (they will later be grown as needed)
		for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; i++) {
			ImGui_ImplDX12_RenderBuffers* fr = &m_pFrameResources[i];
			fr->IndexBuffer = NULL;
			fr->VertexBuffer = NULL;
			fr->IndexBufferSize = 10000;
			fr->VertexBufferSize = 5000;
		}

		// Create Objects
		auto result = CreateRootSignature();
		assert(result);

		result = CreatePSO();
		assert(result);
		return true;
	}

	void ResetDrawLists() override {

		// Setup display size (every frame to accommodate for window resizing)
		RECT rect = {0, 0, 0, 0};
		::GetClientRect((HWND)m_NativeWindow->GetNativeHandle(), &rect);
		//io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

		if(m_DrawList == nullptr) {			
			//m_DrawList = new ImGuiDrawListWrapper(ImGui::GetForegroundDrawList());
			m_DrawList = std::make_unique<ImGuiDrawListWrapper_Independent>(m_NativeWindow->GetSize());
		} else {
			//((ImGuiDrawListWrapper*)m_DrawList)->UpdatePointer(ImGui::GetForegroundDrawList());
		}

		m_DrawList->Reset();
	}

	DrawList* GetFrameDrawList() override { return m_DrawList.get(); }

	void RenderFrame(bool bVsync) override {
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		FrameContext* frameCtx = WaitForNextFrameResources();
		UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
		frameCtx->CommandAllocator->Reset();

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		g_pd3dCommandList->Reset(frameCtx->CommandAllocator, NULL);
		g_pd3dCommandList->ResourceBarrier(1, &barrier);

		// Render Dear ImGui graphics
		const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w};
		g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, NULL);
		g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, NULL);

		const auto ptr = m_DescriptorHeap_SRV.getNative();
		g_pd3dCommandList->SetDescriptorHeaps(1, &ptr);

		//const auto drawData = ImGui::GetDrawData();
		auto drawList = (ImGuiDrawListWrapper_Independent*)m_DrawList.get();
		auto drawData = drawList->GetDrawData();

		RenderDrawData(drawData, g_pd3dCommandList);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		g_pd3dCommandList->ResourceBarrier(1, &barrier);
		g_pd3dCommandList->Close();

		g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_pd3dCommandList);

		if (bVsync) {
			g_pSwapChain->Present(1, 0); // Present with vsync
		} else {
			g_pSwapChain->Present(0, 0); // Present without vsync
		}

		UINT64 fenceValue = g_fenceLastSignaledValue + 1;
		g_pd3dCommandQueue->Signal(g_fence, fenceValue);
		g_fenceLastSignaledValue = fenceValue;
		frameCtx->FenceValue = fenceValue;		
	}

	void ResizeFramebuffers(float2 inSize) override {
		WaitForLastSubmittedFrame();
		CleanupRenderTarget();

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		auto hr = g_pSwapChain->GetDesc1(&swapChainDesc);
		checkhr(hr);
		//swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;

		hr = g_pSwapChain->ResizeBuffers(NUM_BACK_BUFFERS, (UINT)inSize.x, (UINT)inSize.y, swapChainDesc.Format, swapChainDesc.Flags);
		checkhr(hr);

		CreateRenderTarget();

		if (m_DrawList) {
			auto drawList = (ImGuiDrawListWrapper_Independent*)m_DrawList.get();
			drawList->UpdateViewportRect(Rect({0, 0}, inSize));
		}		
	}

	// Not so many texture is used in the ui so we can load a texture into the GPU right away. Without batching
	TextureHandle CreateTexture(const Image& inImage) override {
		
		// Allocate a texture resource
		ID3D12Resource* pTexture = nullptr;
		{
			D3D12_HEAP_PROPERTIES props{};
			props.Type = D3D12_HEAP_TYPE_DEFAULT;
			props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Alignment = 0;
			desc.Width = inImage.Width;
			desc.Height = (UINT)inImage.Height;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			g_pd3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
												  D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));
		}

		// Allocate an upload heap
		D3D12_TEXTURE_COPY_LOCATION srcLocation{};
		ID3D12Resource* uploadBuffer = nullptr;
		{
			UINT uploadPitch = (inImage.Width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
			UINT uploadSize = (UINT)inImage.Height * uploadPitch;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.Width = uploadSize;
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			D3D12_HEAP_PROPERTIES props{};
			props.Type = D3D12_HEAP_TYPE_UPLOAD;
			props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			
			HRESULT hr = g_pd3dDevice->CreateCommittedResource(
				&props, 
				D3D12_HEAP_FLAG_NONE, 
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ, 
				NULL, 
				IID_PPV_ARGS(&uploadBuffer)
			);
			IM_ASSERT(SUCCEEDED(hr));

			// Copy texture data into the upload buffer
			{
				void* mapped = NULL;
				D3D12_RANGE range = {0, uploadSize};
				hr = uploadBuffer->Map(0, &range, &mapped);
				IM_ASSERT(SUCCEEDED(hr));

				for(int y = 0; y < inImage.Height; y++) {
					memcpy(
						(void*)((uintptr_t)mapped + y * uploadPitch), 
						inImage.Data.data() + y * inImage.Width * 4, 
						inImage.Width * 4
					);
				}
				uploadBuffer->Unmap(0, &range);
				
				srcLocation.pResource = uploadBuffer;
				srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				srcLocation.PlacedFootprint.Footprint.Width = (UINT)inImage.Width;
				srcLocation.PlacedFootprint.Footprint.Height = (UINT)inImage.Height;
				srcLocation.PlacedFootprint.Footprint.Depth = 1;
				srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;
			}
		}
		
		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = pTexture;
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pTexture;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		ID3D12Fence* fence = NULL;
		auto hr = g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		IM_ASSERT(SUCCEEDED(hr));

		HANDLE event = CreateEvent(0, 0, 0, 0);
		IM_ASSERT(event != NULL);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = NULL;
		hr = g_pd3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12CommandAllocator* cmdAlloc = NULL;
		hr = g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12GraphicsCommandList* cmdList = NULL;
		hr = g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
		IM_ASSERT(SUCCEEDED(hr));

		cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
		cmdList->ResourceBarrier(1, &barrier);

		hr = cmdList->Close();
		IM_ASSERT(SUCCEEDED(hr));

		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
		hr = cmdQueue->Signal(fence, 1);
		IM_ASSERT(SUCCEEDED(hr));

		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);

		cmdList->Release();
		cmdAlloc->Release();
		cmdQueue->Release();
		CloseHandle(event);
		fence->Release();
		uploadBuffer->Release();

		// Create a texture view
		Descriptor textureDescriptor{};
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			textureDescriptor = m_DescriptorHeap_SRV.createDescriptor(pTexture, &srvDesc);
		}	

		const auto handle = (TextureHandle)textureDescriptor.GetVptrGPU();
		m_Textures.emplace(handle, pTexture);
		return handle;
	}

	void DeleteTexture(TextureHandle inTexture) override {
		auto it = m_Textures.find(inTexture);
		if (it != m_Textures.end()) {
			it->second->Release();
			/// TODO delete rtv
		}
	}

private:

	bool CreateDeviceD3D(HWND hWnd) {
	// Setup swap chain
		DXGI_SWAP_CHAIN_DESC1 sd{};
		{
			sd.BufferCount = NUM_BACK_BUFFERS;
			sd.Width = 0;
			sd.Height = 0;
			sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			sd.Scaling = DXGI_SCALING_STRETCH;
			sd.Stereo = FALSE;
		}

		// [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
		ID3D12Debug* pdx12Debug = NULL;
		if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
			pdx12Debug->EnableDebugLayer();
#endif

	// Create device
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
		if(D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
			return false;

		// [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
		if(pdx12Debug != NULL) {
			ID3D12InfoQueue* pInfoQueue = NULL;
			g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			pInfoQueue->Release();
			pdx12Debug->Release();
		}
#endif

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = NUM_BACK_BUFFERS;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 1;
			if(g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
				return false;

			SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
			for(UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
				g_mainRenderTargetDescriptor[i] = rtvHandle;
				rtvHandle.ptr += rtvDescriptorSize;
			}
		}

		{
			D3D12_COMMAND_QUEUE_DESC desc = {};
			desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			desc.NodeMask = 1;
			if(g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
				return false;
		}

		for(UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
			if(g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
				return false;

		if(g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
		   g_pd3dCommandList->Close() != S_OK)
			return false;

		if(g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
			return false;

		g_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if(g_fenceEvent == NULL)
			return false;

		{
			IDXGIFactory4* dxgiFactory = NULL;
			IDXGISwapChain1* swapChain1 = NULL;
			if(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
				return false;
			if(dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK)
				return false;
			if(swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
				return false;
			swapChain1->Release();
			dxgiFactory->Release();
			g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
			g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
		}

		CreateRenderTarget();
		return true;
	}

	void CleanupDeviceD3D() {
		CleanupRenderTarget();
		if(g_pSwapChain) { g_pSwapChain->SetFullscreenState(false, NULL); g_pSwapChain->Release(); g_pSwapChain = NULL; }
		if(g_hSwapChainWaitableObject != NULL) { CloseHandle(g_hSwapChainWaitableObject); }
		for(UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
			if(g_frameContext[i].CommandAllocator) { g_frameContext[i].CommandAllocator->Release(); g_frameContext[i].CommandAllocator = NULL; }
		if(g_pd3dCommandQueue) { g_pd3dCommandQueue->Release(); g_pd3dCommandQueue = NULL; }
		if(g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = NULL; }
		if(g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = NULL; }
		if(g_fence) { g_fence->Release(); g_fence = NULL; }
		if(g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = NULL; }
		if(g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }

#ifdef DX12_ENABLE_DEBUG_LAYER
		IDXGIDebug1* pDebug = NULL;
		if(SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)))) {
			pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
			pDebug->Release();
		}
#endif
	}

	void CreateRenderTarget() {
		for(UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
			ID3D12Resource* pBackBuffer = NULL;
			g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
			g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, g_mainRenderTargetDescriptor[i]);
			g_mainRenderTargetResource[i] = pBackBuffer;
		}
	}

	void CleanupRenderTarget() {
		WaitForLastSubmittedFrame();

		for(UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
			if(g_mainRenderTargetResource[i]) { 
				g_mainRenderTargetResource[i]->Release(); 
				g_mainRenderTargetResource[i] = nullptr; 
			}
		}
	}

	bool CreateRootSignature() {
		
		// Describes descriptor table
		D3D12_DESCRIPTOR_RANGE descRange = {};
		descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descRange.NumDescriptors = 1;
		descRange.BaseShaderRegister = 0;
		descRange.RegisterSpace = 0;
		descRange.OffsetInDescriptorsFromTableStart = 0;

		// D3D12_ROOT_PARAMETER param[2] = {};
		std::array<D3D12_ROOT_PARAMETER, 2> param{};

		param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param[0].Constants.ShaderRegister = 0;
		param[0].Constants.RegisterSpace = 0;
		param[0].Constants.Num32BitValues = 16;
		param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param[1].DescriptorTable.NumDescriptorRanges = 1;
		param[1].DescriptorTable.pDescriptorRanges = &descRange;
		param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.
		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.MipLODBias = 0.f;
		staticSampler.MaxAnisotropy = 0;
		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		staticSampler.MinLOD = 0.f;
		staticSampler.MaxLOD = 0.f;
		staticSampler.ShaderRegister = 0;
		staticSampler.RegisterSpace = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = static_cast<UINT>(param.size());
		desc.pParameters = param.data();
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &staticSampler;
		desc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		// Load d3d12.dll and D3D12SerializeRootSignature() function address dynamically to facilitate using with D3D12On7.
		// See if any version of d3d12.dll is already loaded in the process. If so, give preference to that.
		static HINSTANCE d3d12_dll = ::GetModuleHandleA("d3d12.dll");
		if(d3d12_dll == NULL) {
			// Attempt to load d3d12.dll from local directories. This will only succeed if
			// (1) the current OS is Windows 7, and
			// (2) there exists a version of d3d12.dll for Windows 7 (D3D12On7) in one of the following directories.
			// See https://github.com/ocornut/imgui/pull/3696 for details.
			const char* localD3d12Paths[] = {".\\d3d12.dll", ".\\d3d12on7\\d3d12.dll", ".\\12on7\\d3d12.dll"}; // A. current directory, B. used by some games, C. used in Microsoft D3D12On7 sample
			for(int i = 0; i < IM_ARRAYSIZE(localD3d12Paths); i++)
				if((d3d12_dll = ::LoadLibraryA(localD3d12Paths[i])) != NULL)
					break;

			// If failed, we are on Windows >= 10.
			if(d3d12_dll == NULL)
				d3d12_dll = ::LoadLibraryA("d3d12.dll");

			if(d3d12_dll == NULL) {
				return false;
			}
		}

		PFN_D3D12_SERIALIZE_ROOT_SIGNATURE D3D12SerializeRootSignatureFn = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)::GetProcAddress(d3d12_dll, "D3D12SerializeRootSignature");
		if(D3D12SerializeRootSignatureFn == NULL) {}

		ID3DBlob* blob = NULL;
		if(D3D12SerializeRootSignatureFn(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL) != S_OK) {
			return false;
		}

		auto result = g_pd3dDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature));
		blob->Release();

		return result == S_OK;
	}

	bool CreatePSO() {
		// By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
		// If you would like to use this DX12 sample code but remove this dependency you can:
		//  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
		//  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
		// See https://github.com/ocornut/imgui/pull/638 for sources and details.

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.NodeMask = 1;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.pRootSignature = m_pRootSignature;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = m_RTVFormat;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ID3DBlob* vertexShaderBlob;
		ID3DBlob* pixelShaderBlob;

		// Create the vertex shader
		{
			static const char* vertexShader =
				"cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

			if(FAILED(D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &vertexShaderBlob, NULL))) {
				return false;
			}
			psoDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};

			// Create the input layout
			static D3D12_INPUT_ELEMENT_DESC local_layout[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};
			psoDesc.InputLayout = {local_layout, 3};
		}

		// Create the pixel shader
		{
			static const char* pixelShader =
				"struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            SamplerState sampler0 : register(s0);\
            Texture2D texture0 : register(t0);\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
              float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
              return out_col; \
            }";

			if(FAILED(D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &pixelShaderBlob, NULL))) {
				vertexShaderBlob->Release();
				return false; // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
			}
			psoDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
		}

		// Create the blending setup
		{
			D3D12_BLEND_DESC& desc = psoDesc.BlendState;
			desc.AlphaToCoverageEnable = false;
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}

		// Create the rasterizer state
		{
			D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
			desc.FillMode = D3D12_FILL_MODE_SOLID;
			desc.CullMode = D3D12_CULL_MODE_NONE;
			desc.FrontCounterClockwise = FALSE;
			desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
			desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
			desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			desc.DepthClipEnable = true;
			desc.MultisampleEnable = FALSE;
			desc.AntialiasedLineEnable = FALSE;
			desc.ForcedSampleCount = 0;
			desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		}

		// Create depth-stencil State
		{
			D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
			desc.DepthEnable = false;
			desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.StencilEnable = false;
			desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace = desc.FrontFace;
		}

		HRESULT result_pipeline_state = g_pd3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState));
		vertexShaderBlob->Release();
		pixelShaderBlob->Release();
		if(result_pipeline_state != S_OK)
			return false;

		return true;
	}	

	void WaitForLastSubmittedFrame() {
		FrameContext* frameCtx = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

		UINT64 fenceValue = frameCtx->FenceValue;
		if(fenceValue == 0)
			return; // No fence was signaled

		frameCtx->FenceValue = 0;
		if(g_fence->GetCompletedValue() >= fenceValue)
			return;

		g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
		WaitForSingleObject(g_fenceEvent, INFINITE);
	}

	FrameContext* WaitForNextFrameResources() {
		UINT nextFrameIndex = g_frameIndex + 1;
		g_frameIndex = nextFrameIndex;

		HANDLE waitableObjects[] = {g_hSwapChainWaitableObject, NULL};
		DWORD numWaitableObjects = 1;

		FrameContext* frameCtx = &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
		UINT64 fenceValue = frameCtx->FenceValue;
		if(fenceValue != 0) // means no fence was signaled
		{
			frameCtx->FenceValue = 0;
			g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
			waitableObjects[1] = g_fenceEvent;
			numWaitableObjects = 2;
		}

		WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

		return frameCtx;
	}

	void ImGui_ImplDX12_SetupRenderState(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx, ImGui_ImplDX12_RenderBuffers* fr) {

		// Setup orthographic projection matrix into our constant buffer
		// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
		VERTEX_CONSTANT_BUFFER vertex_constant_buffer{};
		{
			float L = draw_data->DisplayPos.x;
			float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
			float T = draw_data->DisplayPos.y;
			float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
			float mvp[4][4] =
			{
				{ 2.0f / (R - L),  		0.0f,           	0.0f,       0.0f },
				{ 0.0f,         		2.0f / (T - B),     0.0f,       0.0f },
				{ 0.0f,         		0.0f,           	0.5f,       0.0f },
				{ (R + L) / (L - R),  	(T + B) / (B - T),  0.5f,       1.0f },
			};
			memcpy(&vertex_constant_buffer.mvp, mvp, sizeof(mvp));
		}

		// Setup viewport
		D3D12_VIEWPORT vp{};
		vp.Width = draw_data->DisplaySize.x;
		vp.Height = draw_data->DisplaySize.y;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = vp.TopLeftY = 0.0f;
		ctx->RSSetViewports(1, &vp);

		// State
		ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->SetPipelineState(m_pPipelineState);
		ctx->SetGraphicsRootSignature(m_pRootSignature);
		// Setup blend factor
		const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
		ctx->OMSetBlendFactor(blend_factor);

		// Arguments
		// Bind shader and vertex buffers
		unsigned int stride = sizeof(ImDrawVert);
		unsigned int offset = 0;

		D3D12_VERTEX_BUFFER_VIEW vbv{};
		vbv.BufferLocation = fr->VertexBuffer->GetGPUVirtualAddress() + offset;
		vbv.SizeInBytes = fr->VertexBufferSize * stride;
		vbv.StrideInBytes = stride;
		ctx->IASetVertexBuffers(0, 1, &vbv);

		D3D12_INDEX_BUFFER_VIEW ibv{};
		ibv.BufferLocation = fr->IndexBuffer->GetGPUVirtualAddress();
		ibv.SizeInBytes = fr->IndexBufferSize * sizeof(ImDrawIdx);
		ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		ctx->IASetIndexBuffer(&ibv);

		ctx->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);
	}

	void RenderDrawData(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx) {
		// Avoid rendering when minimized
		if(draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
			return;

		// FIXME: I'm assuming that this only gets called once per frame!
		// If not, we can't just re-allocate the IB or VB, we'll have to do a proper allocator.
		g_frameIndex = g_frameIndex + 1;
		ImGui_ImplDX12_RenderBuffers* fr = &m_pFrameResources[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

		for(int n = 0; n < draw_data->CmdListsCount; n++) {
			ImDrawList* draw_list = draw_data->CmdLists[n];
			draw_list->_PopUnusedDrawCmd();
			draw_data->TotalVtxCount += draw_list->VtxBuffer.Size;
			draw_data->TotalIdxCount += draw_list->IdxBuffer.Size;
		}

		// Create and grow vertex/index buffers if needed
		if(fr->VertexBuffer == NULL || fr->VertexBufferSize < draw_data->TotalVtxCount) {
			SafeRelease(fr->VertexBuffer);
			fr->VertexBufferSize = draw_data->TotalVtxCount + 5000;

			D3D12_HEAP_PROPERTIES props{};
			props.Type = D3D12_HEAP_TYPE_UPLOAD;
			props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Width = fr->VertexBufferSize * sizeof(ImDrawVert);
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			if(g_pd3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&fr->VertexBuffer)) < 0)
				return;
		}
		if(fr->IndexBuffer == NULL || fr->IndexBufferSize < draw_data->TotalIdxCount) {
			SafeRelease(fr->IndexBuffer);
			fr->IndexBufferSize = draw_data->TotalIdxCount + 10000;

			D3D12_HEAP_PROPERTIES props{};
			props.Type = D3D12_HEAP_TYPE_UPLOAD;
			props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Width = fr->IndexBufferSize * sizeof(ImDrawIdx);
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			auto result = g_pd3dDevice->CreateCommittedResource(
				&props, D3D12_HEAP_FLAG_NONE, 
				&desc, 
				D3D12_RESOURCE_STATE_GENERIC_READ, 
				NULL, 
				IID_PPV_ARGS(&fr->IndexBuffer)
			);
			Assert(SUCCEEDED(result));		
		}

		// Upload vertex/index data into a single contiguous GPU buffer
		void* vtx_resource, * idx_resource;
		D3D12_RANGE range{};
		if(fr->VertexBuffer->Map(0, &range, &vtx_resource) != S_OK)
			return;
		if(fr->IndexBuffer->Map(0, &range, &idx_resource) != S_OK)
			return;
		ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
		ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;

		for(int n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtx_dst += cmd_list->VtxBuffer.Size;
			idx_dst += cmd_list->IdxBuffer.Size;
		}
		fr->VertexBuffer->Unmap(0, &range);
		fr->IndexBuffer->Unmap(0, &range);

		// Setup desired DX state
		ImGui_ImplDX12_SetupRenderState(draw_data, ctx, fr);

		// Render command lists
		// (Because we merged all buffers into a single one, we maintain our own offset into them)
		int global_vtx_offset = 0;
		int global_idx_offset = 0;
		ImVec2 clip_off = draw_data->DisplayPos;

		for(int n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* cmd_list = draw_data->CmdLists[n];

			for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];				
				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
				ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
				if(clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
					continue;

				// Apply Scissor/clipping rectangle, Bind texture, Draw
				const D3D12_RECT r = {(LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y};
				D3D12_GPU_DESCRIPTOR_HANDLE texture_handle = {};
				texture_handle.ptr = (UINT64)pcmd->GetTexID();
				ctx->SetGraphicsRootDescriptorTable(1, texture_handle);
				ctx->RSSetScissorRects(1, &r);
				ctx->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
				
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}
	}

private:

	INativeWindow*				m_NativeWindow = nullptr;

	// Data
	constexpr static int const  NUM_FRAMES_IN_FLIGHT = 2;
	FrameContext                g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
	UINT                        g_frameIndex = 0;

	constexpr static int const	NUM_BACK_BUFFERS = 2;
	ID3D12Device2*				g_pd3dDevice = nullptr;
	ID3D12DescriptorHeap*		g_pd3dRtvDescHeap = nullptr;
	ID3D12CommandQueue*			g_pd3dCommandQueue = nullptr;
	ID3D12GraphicsCommandList*	g_pd3dCommandList = nullptr;
	ID3D12Fence*				g_fence = nullptr;
	HANDLE                      g_fenceEvent = nullptr;
	UINT64                      g_fenceLastSignaledValue = 0;
	IDXGISwapChain3*			g_pSwapChain = nullptr;
	HANDLE                      g_hSwapChainWaitableObject = nullptr;
	ID3D12Resource*				g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

	ID3D12RootSignature*		m_pRootSignature;
	ID3D12PipelineState*		m_pPipelineState;
	DXGI_FORMAT                 m_RTVFormat;

	ImGui_ImplDX12_RenderBuffers* 
								m_pFrameResources;
	UINT                        m_NumFramesInFlight;
	UINT                        m_FrameIndex;

	DescriptorHeap				m_DescriptorHeap_SRV;

	std::unique_ptr<DrawList>	m_DrawList;

	std::map<TextureHandle, ID3D12Resource*> m_Textures;

};

Renderer* CreateRendererDX12() { return new RendererDX12(); }