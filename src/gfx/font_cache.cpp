#include "font_cache.h"

#include <filesystem>
#include <fstream>

// TODO: Handle errors properly
// TODO: Load asynchronously
RefCountedPtr<FontTypeface> FontCache::LoadFromDisk(
    const std::string& filename) {
    // Start loading font
    std::error_code ec;
    auto fileSize = std::filesystem::file_size(filename, ec);
    DASSERT_F(!ec, "Cannot find the file: {}", filename);

    std::vector<char> fileBlob;
    fileBlob.resize(fileSize);

    std::ifstream fontFile;
    fontFile.open(filename, std::ios::binary);
    DASSERT(fontFile.good());

    fontFile.read(fileBlob.data(), fileSize);
    DASSERT(fontFile.good());
    fontFile.close();

    std::expected<RefCountedPtr<FontTypeface>, ErrorCode> face =
        FontTypeface::Create(fileBlob);
    DASSERT(face);
    // TODO: Check for duplicates
    typefaces_.push_back(face.value());
    return face.value();
}


RefCountedPtr<FontTypeface> FontCache::Match(
    std::span<const std::string> familyStack,
    uint32_t weight,
    bool isItalic) {
    //
    DASSERT(!familyStack.empty());
    int64_t bestMatchRank = 0;
    RefCountedPtr<FontTypeface> bestMatchFace = nullptr;
    // Assign rank based on the index of the font
    for (auto& face : typefaces_) {
        int64_t currentRank = 0;
        for (size_t familyIndex = 0; familyIndex < familyStack.size();
             ++familyIndex) {
            if (face->GetFamilyName() == familyStack[familyIndex]) {
                currentRank += familyStack.size() - familyIndex;
                break;
            }
        }
        currentRank -= (int64_t)weight - face->GetWeight();
        currentRank -= isItalic ^ face->IsItalic();
        if (currentRank > bestMatchRank) {
            bestMatchRank = currentRank;
            bestMatchFace = face;
        }
        // TODO: Handle currentRank == bestMatchRank
    }
    if (bestMatchRank != 0) {
        DASSERT(bestMatchFace);
        return bestMatchFace;
    }
    // TODO: Return fallback face
    return {};
}