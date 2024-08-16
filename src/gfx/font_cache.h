#include "common.h"
#include "font.h"
#include "base/util.h"

#include <map>

// Stores loaded fonts
class FontCache {
public:

    static std::unique_ptr<FontCache> Create(size_t maxCapacity);

    void LoadAllFromDirectory(const std::string& dir);
    RefCountedPtr<FontTypeface> LoadFromDisk(const std::string& filename);

    void Insert(RefCountedPtr<FontTypeface> face);
    void Remove(FontTypeface& face);

    RefCountedPtr<FontTypeface> FindByFilename(const std::string& filename);
    RefCountedPtr<FontTypeface> FindByName(const std::string& name);

    // Try to find a font that matches parameters the best
    RefCountedPtr<FontTypeface> Match(std::span<const std::string> familyStack,
                                      uint32_t weight,
                                      bool isItalic);

    // Set the fallback font which will match any parameters
    // Used as a last resort if no other font has been matched
    void SetFallback(RefCountedPtr<FontTypeface> face);

private:
    uint32_t capacity_;
    std::vector<RefCountedPtr<FontTypeface>> typefaces_;
};