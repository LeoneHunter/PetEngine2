#include <ft2build.h>
#include FT_FREETYPE_H

#include <d3d12.h>
#include <directx/d3dx12.h>

#include <wrl/client.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

// libpng
#include <png.h>

#undef max
#undef min

using Microsoft::WRL::ComPtr;

constexpr int ATLAS_WIDTH = 512;
constexpr int ATLAS_HEIGHT = 512;
constexpr int FONT_SIZE = 48;
constexpr int FIRST_CHAR = 32;
constexpr int LAST_CHAR = 126;

struct GlyphInfo {
  float u1, v1, u2, v2;  // texture coordinates
  int width, height;
  int bearingX, bearingY;
  int advance;
};

std::unordered_map<char, GlyphInfo> glyphMap;

bool CreateFontAtlas(ID3D12Device* device,
                     ID3D12GraphicsCommandList* cmdList,
                     const char* fontPath,
                     ComPtr<ID3D12Resource>& outTexture) {
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    std::cerr << "Failed to init FreeType\n";
    return false;
  }

  FT_Face face;
  if (FT_New_Face(ft, fontPath, 0, &face)) {
    std::cerr << "Failed to load font\n";
    return false;
  }

  FT_Set_Pixel_Sizes(face, 0, FONT_SIZE);

  std::vector<uint8_t> atlasData(ATLAS_WIDTH * ATLAS_HEIGHT, 0);

  int penX = 0, penY = 0;
  int rowHeight = 0;

  for (char c = FIRST_CHAR; c <= LAST_CHAR; ++c) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      std::cerr << "Could not load char " << c << "\n";
      continue;
    }

    FT_Bitmap& bitmap = face->glyph->bitmap;
    if (penX + bitmap.width >= ATLAS_WIDTH) {
      penX = 0;
      penY += rowHeight + 1;
      rowHeight = 0;
    }

    for (unsigned int y = 0; y < bitmap.rows; ++y) {
      for (unsigned int x = 0; x < bitmap.width; ++x) {
        int dstX = penX + x;
        int dstY = penY + y;
        atlasData[dstY * ATLAS_WIDTH + dstX] =
          bitmap.buffer[y * bitmap.pitch + x];
      }
    }

    GlyphInfo info;
    info.u1 = float(penX) / ATLAS_WIDTH;
    info.v1 = float(penY) / ATLAS_HEIGHT;
    info.u2 = float(penX + bitmap.width) / ATLAS_WIDTH;
    info.v2 = float(penY + bitmap.rows) / ATLAS_HEIGHT;
    info.width = bitmap.width;
    info.height = bitmap.rows;
    info.bearingX = face->glyph->bitmap_left;
    info.bearingY = face->glyph->bitmap_top;
    info.advance = face->glyph->advance.x >> 6;

    glyphMap[c] = info;

    penX += bitmap.width + 1;
    rowHeight = std::max(rowHeight, static_cast<int>(bitmap.rows));
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  // Create DirectX 12 texture (1-channel 8-bit grayscale)
  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = ATLAS_WIDTH;
  texDesc.Height = ATLAS_HEIGHT;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
  HRESULT hr = device->CreateCommittedResource(
    &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr, IID_PPV_ARGS(&outTexture));

  if (FAILED(hr)) {
    std::cerr << "Failed to create texture\n";
    return false;
  }

  // Upload heap
  const UINT64 uploadBufferSize =
    GetRequiredIntermediateSize(outTexture.Get(), 0, 1);

  ComPtr<ID3D12Resource> uploadHeap;
  CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC bufferDesc =
    CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

  device->CreateCommittedResource(
    &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadHeap));

  D3D12_SUBRESOURCE_DATA subresourceData = {};
  subresourceData.pData = atlasData.data();
  subresourceData.RowPitch = ATLAS_WIDTH;
  subresourceData.SlicePitch = subresourceData.RowPitch * ATLAS_HEIGHT;

  UpdateSubresources(cmdList, outTexture.Get(), uploadHeap.Get(), 0, 0, 1,
                     &subresourceData);

  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    outTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  cmdList->ResourceBarrier(1, &barrier);

  return true;
}

struct Glyph {
  char character;
  int width;
  int height;
};

struct PackedGlyph {
  int x, y;
  int width, height;
};

struct AtlasPackingResult {
  int atlasWidth, atlasHeight;
  std::unordered_map<char, PackedGlyph> placements;
};

// Naive bin-packing into R rows with best heuristic
AtlasPackingResult PackGlyphs(const std::vector<Glyph>& glyphs) {
  const int padding = 1;
  int bestArea = std::numeric_limits<int>::max();
  AtlasPackingResult bestPacking;

  for (int rowCount = 1; rowCount <= (int)glyphs.size(); ++rowCount) {
    std::vector<std::vector<Glyph>> rows(rowCount);

    // Greedy row fill: tallest glyphs first
    auto sorted = glyphs;
    std::sort(sorted.begin(), sorted.end(), [](const Glyph& a, const Glyph& b) {
      return a.height > b.height;
    });

    for (size_t i = 0; i < sorted.size(); ++i) {
      rows[i % rowCount].push_back(sorted[i]);
    }

    int atlasWidth = 0;
    int atlasHeight = 0;
    std::unordered_map<char, PackedGlyph> placements;

    int y = 0;
    for (const auto& row : rows) {
      int rowHeight = 0;
      int x = 0;
      for (const auto& glyph : row) {
        placements[glyph.character] = {x, y, glyph.width, glyph.height};
        x += glyph.width + padding;
        rowHeight = std::max(rowHeight, glyph.height);
      }
      atlasWidth = std::max(atlasWidth, x);
      y += rowHeight + padding;
    }
    atlasHeight = y;

    int area = atlasWidth * atlasHeight;
    if (area < bestArea) {
      bestArea = area;
      bestPacking.atlasWidth = atlasWidth;
      bestPacking.atlasHeight = atlasHeight;
      bestPacking.placements = std::move(placements);
    }
  }

  return bestPacking;
}

bool SaveAtlasAsPNG(const char* filename,
                    const std::vector<uint8_t>& atlasData,
                    int atlasWidth,
                    int atlasHeight) {
  FILE* fp = fopen(filename, "wb");
  if (!fp) {
    perror("fopen");
    return false;
  }

  png_structp png =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    fclose(fp);
    return false;
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, nullptr);
    fclose(fp);
    return false;
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return false;
  }

  png_init_io(png, fp);

  png_set_IHDR(png, info, atlasWidth, atlasHeight, 8, PNG_COLOR_TYPE_GRAY,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);

  png_write_info(png, info);

  std::vector<png_bytep> row_pointers(atlasHeight);
  for (int y = 0; y < atlasHeight; ++y) {
    row_pointers[y] = (png_bytep)&atlasData[y * atlasWidth];
  }

  png_write_image(png, row_pointers.data());
  png_write_end(png, nullptr);

  png_destroy_write_struct(&png, &info);
  fclose(fp);
  return true;
}


// UI tree root objeect
class ComponentRoot {};

class RenderContext {};

class Canvas {
public:
  Canvas();
  void Reset(RenderContext* ctx);
};


// Render the UI tree
class UIRenderer {
public:
  UIRenderer(Canvas* canvas);
  void Draw(ComponentRoot* tree);
};

// Root level project object
class Editor {
public:
  void Init() {
    auto* canvas = new Canvas();
    renderer = new UIRenderer(canvas);
  }

  void Run() {
    canvas->Reset(new RenderContext());
    // Create UI state tree
    auto root = std::make_unique<ComponentRoot>();
    renderer->Draw(root.get());

  }

  Canvas* canvas = nullptr;
  UIRenderer* renderer = nullptr;
};
