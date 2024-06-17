#include "device_dx12.h"
#include "allocator_dx12.h"
#include "descriptor_heap_dx12.h"

#include "thirdparty/doctest/doctest.h"

#include <map>

using namespace dx12;

namespace {

struct DX12Test {

    DX12Test() {
        const int currentAdapter = 0;
        device = Device::Create(currentAdapter, kDebugBuild);
    }

    std::unique_ptr<Device> device;
};

struct DescriptorHeapTest: public DX12Test {

    DescriptorHeapTest() {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 5,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        };
        heap = std::make_unique<DescriptorHeap>(
            device->GetDevice(), desc, "DescriptorHeapTest");
        CHECK(heap->HasProperties(desc));
        CHECK_EQ(heap->GetFreeSize(), 5);
    }

    std::unique_ptr<DescriptorHeap> heap;
};

} // namespace

TEST_CASE_FIXTURE(DescriptorHeapTest, "[DescriptorHeap] Basic") {
    std::optional<DescriptorLocation> alloc0 = heap->TryAllocate(2);
    // x x - - -
    CHECK(alloc0);
    CHECK_EQ(heap->GetFreeSize(), 3);

    std::optional<DescriptorLocation> alloc1 = heap->TryAllocate(2);
    // x x x x -
    CHECK(alloc1);
    CHECK_EQ(heap->GetFreeSize(), 1);

    std::optional<DescriptorLocation> alloc2 = heap->TryAllocate(1);
    // x x x x x
    CHECK(alloc2);
    CHECK_EQ(heap->GetFreeSize(), 0);

    std::optional<DescriptorLocation> alloc3 = heap->TryAllocate(1);
    // x x x x x
    CHECK(!alloc3);
    CHECK_EQ(heap->GetFreeSize(), 0);

    heap->Free(*alloc1);
    // x x - - x
    CHECK_EQ(heap->GetFreeSize(), 2);

    heap->Free(*alloc2);
    // x x - - -
    CHECK_EQ(heap->GetFreeSize(), 3);

    std::optional<DescriptorLocation> alloc4 = heap->TryAllocate(3);
    // x x x x x
    CHECK(alloc4);
    CHECK_EQ(heap->GetFreeSize(), 0);

    heap->Free(*alloc0);
    // - - x x x
    CHECK_EQ(heap->GetFreeSize(), 2);

    heap->Free(*alloc4);
    // - - - - - 
    CHECK_EQ(heap->GetFreeSize(), 5);
}

TEST_CASE_FIXTURE(DescriptorHeapTest, "[DescriptorHeap] Double merge") {
    std::optional<DescriptorLocation> alloc0 = heap->TryAllocate(2);
    std::optional<DescriptorLocation> alloc1 = heap->TryAllocate(2);
    std::optional<DescriptorLocation> alloc2 = heap->TryAllocate(1);

    heap->Free(*alloc0);
    heap->Free(*alloc2);
    // - - x x -
    CHECK_EQ(heap->GetFreeSize(), 3);

    std::optional<DescriptorLocation> alloc3 = heap->TryAllocate(3);
    CHECK(!alloc3);

    heap->Free(*alloc1);
    // - - - - - 
    CHECK_EQ(heap->GetFreeSize(), 5);

    std::optional<DescriptorLocation> alloc4 = heap->TryAllocate(5);
    // x x x x x
    CHECK(alloc4);
    CHECK_EQ(heap->GetFreeSize(), 0);
}



namespace {

template<class T>
concept Formattable = requires (T val) { std::format("{}", val); };

class JsonObjectBuilder {
public:

    constexpr JsonObjectBuilder(const std::string& object, int indent = 0)
        : sb(&out, indent) {
        sb.Line("\"{}\": {}", object, "{");
        sb.PushIndent();
    }

    constexpr JsonObjectBuilder& Field(const std::string& name, const std::string& value) {
        sb.Line("\"{}\": \"{}\",", name, value);
        return *this;
    }

    template<Formattable Arg>
    constexpr JsonObjectBuilder& Field(const std::string& name, Arg arg) {
        sb.Line("\"{}\": \"{}\",", name, std::format("{}", std::forward<Arg>(arg)));
        return *this;
    }

    template<Formattable... Args>
    constexpr JsonObjectBuilder& Field(const std::string& name,
                                       const std::format_string<Args...> fmt,
                                       Args&&... args) {
        sb.Line("\"{}\": \"{}\",", name, std::format(fmt, std::forward<Args>(args)...));
        return *this;
    }

    template<Formattable... Args>
    constexpr JsonObjectBuilder& Array(const std::string& name, Args&&... args) {
        // arg1 -> [arg1]
        // arg1 arg2 -> [arg1, arg2]
        std::string fargs;
        (fargs.append(std::format("{}", std::forward<Args>(args))).append(", "), ...);
        fargs.pop_back();
        fargs.pop_back();
        sb.Line("\"{}\": [{}],", name, fargs);
        return *this;
    }

    constexpr std::string Build() {
        // Remove last ','
        if(!out.empty() && out.ends_with(',')) {
            out.pop_back();
        }
        sb.PopIndent();
        sb.Line("}");
        return std::move(out);
    }

private:
    std::string out;
    util::StringBuilder sb;
};


// Formats an integer as a 32 bit hex value (0x00000000)
constexpr auto kHexFormat32 = "{:#010x}";
// Formats an integer as a 64 bit hex value (0x00000000'00000000)
constexpr auto kHexFormat64 = "{:#018x}";

template<class T>
class JsonFormatter {};

template<>
class JsonFormatter<AllocationTracker> {
public:
    static constexpr std::string Format(const AllocationTracker::HeapInfo& info,
                                        unsigned indent = 0) {
        return JsonObjectBuilder("HeapInfo", indent)
            .Field("HeapResourceType", to_string(info.type))
            .Field("Name", info.name)
            .Field("Address", kHexFormat64, info.address)
            .Build();
    }

    static constexpr std::string Format(const AllocationTracker::SpanID& span,
                                        unsigned indent = 0) {
        return JsonObjectBuilder("SpanID", indent)
            .Field("PageIndex", span.pageIndex)
            .Field("HeapAddress", kHexFormat64, span.heapAddress)
            .Build();
    }

    static constexpr std::string Format(const AllocationTracker::SpanInfo& info,
                                        unsigned indent = 0) {
        return JsonObjectBuilder("SpanInfo", indent)
            .Field("SizeClass", info.sizeClass)
            .Field("NumPages", info.numPages)
            .Field("NumSlots", info.numSlots)
            .Field("NumFreeSlots", info.numFreeSlots)
            .Build();
    }
};

class AllocTracker final: public AllocationTracker {
public:
    using AllocationTracker::HeapInfo;
    using AllocationTracker::SpanID;
    using AllocationTracker::SpanInfo;
    using AllocationTracker::SlotAllocInfo;

    using Formatter = JsonFormatter<AllocationTracker>;

    struct HeapStats {
        HeapInfo info;
    };

    AllocTracker(bool printEvents)
        : printEvents(printEvents)
    {}

    void DidCreateHeap(const HeapInfo& info) override {
        if(printEvents) {
            Println("Created heap: \n{}", Formatter::Format(info));
        }
        heaps.emplace(info.address, HeapStats{.info = info});
        maxNumHeaps = std::max(maxNumHeaps, heaps.size());
        reservedBytes += dx12::internal::kHeapSize;
        maxReserved = std::max(maxReserved, reservedBytes);
    }

    void DidDestroyHeap(uintptr_t ptr) override {
        auto it = heaps.find(ptr);
        if(printEvents) {
            Println("Destroyed heap: \n{}", Formatter::Format(it->second.info));
        }
        heaps.erase(it);
        reservedBytes -= dx12::internal::kHeapSize;
    }

    void DidCommitSpan(const SpanID& id, const SpanInfo& info) override {
        if(printEvents) {
            Println("Committed span: \n{}", FormatSpan(id, info));
        }
        spans.emplace(id, info);
        maxNumSpans = std::max(maxNumSpans, spans.size());
        const uint32_t slotSize = dx12::internal::SizeClassToSlotSize(info.sizeClass);
        maxSlotSize = std::max(maxSlotSize, slotSize);
    }

    void DidDecommitSpan(const SpanID& id) override {
        auto it = spans.find(id);
        if(printEvents) {
            Println("Decommitted span: \n{}", FormatSpan(id, it->second));
        }
        spans.erase(it);
    }

    void DidAllocateSlot(const SpanID& id, const SlotAllocInfo& info) override {
        ++numAllocs;
        maxNumAllocs = std::max(maxNumAllocs, numAllocs);

        auto it = spans.find(id);
        DASSERT(it != spans.end());
        SpanInfo& span = it->second;
        const uint32_t slotSize = span.sizeClass * dx12::internal::kMinSlotSize;

        allocatedBytes += slotSize;
        utilization = (float)allocatedBytes / reservedBytes;
        maxUtilization = std::max(maxUtilization, utilization);
        maxUsed = std::max(maxUsed, allocatedBytes);
    }

    void DidFreeSlot(const SpanID& id, const SlotAllocInfo& info) override {
        --numAllocs;

        auto it = spans.find(id);
        DASSERT(it != spans.end());
        SpanInfo& span = it->second;
        const uint32_t slotSize = span.sizeClass * dx12::internal::kMinSlotSize;

        allocatedBytes -= slotSize;
        utilization = (float)allocatedBytes / reservedBytes;
    }

public:

    static std::string FormatSpan(const SpanID& id, const SpanInfo& info) {
        return JsonObjectBuilder("Span")
            .Field("Heap", kHexFormat64, id.heapAddress)
            .Array("RangePages", id.pageIndex, id.pageIndex + info.numPages - 1)
            .Build();
    }

public:
    std::map<uintptr_t, HeapStats> heaps;
    std::map<SpanID, SpanInfo> spans;
    bool printEvents;

    size_t numAllocs = 0;
    size_t reservedBytes = 0;
    size_t allocatedBytes = 0;
    float  utilization = 0;

    size_t maxNumAllocs = 0;
    size_t maxNumSpans = 0;
    size_t maxNumHeaps = 0;
    size_t maxUsed = 0;
    size_t maxReserved = 0;
    // 0.0 - 1.0
    float  maxUtilization = 0;

    uint32_t maxSlotSize = 0;
};

} // namespace


TEST_CASE("[dx12::ResourceSlabAllocator] Basic smoke test") {
    using namespace dx12::internal;
    auto tracker = std::make_unique<AllocTracker>(false);
    {
        const int currentAdapter = 0;
        auto device = Device::Create(currentAdapter, kDebugBuild);
        ResourceSlabAllocator alloc(device.get(), tracker.get());

        auto desc = CD3DX12_RESOURCE_DESC::Buffer(kMaxSlotSize);
        auto obj1 = alloc.Allocate(desc);
        auto obj2 = alloc.Allocate(desc);
        CHECK_EQ(tracker->spans.size(), 1);
        CHECK_EQ(tracker->numAllocs, 2);

        auto desc1 = CD3DX12_RESOURCE_DESC::Buffer(kMinSlotSize * 3);
        auto obj3 = alloc.Allocate(desc1);
        auto obj4 = alloc.Allocate(desc1);
        CHECK_EQ(tracker->spans.size(), 2);
        CHECK_EQ(tracker->numAllocs, 4);

        alloc.Free(*obj1);
        alloc.Free(*obj2);
        CHECK_EQ(tracker->spans.size(), 1);
        CHECK_EQ(tracker->numAllocs, 2);

        alloc.Free(*obj3);
        alloc.Free(*obj4);
        CHECK_EQ(tracker->heaps.size(), 1);
        CHECK_EQ(tracker->spans.size(), 0);
        CHECK_EQ(tracker->numAllocs, 0);
    }
    CHECK_EQ(tracker->heaps.size(), 0);
    CHECK_EQ(tracker->spans.size(), 0);
}

#include "runtime/util.h"

TEST_CASE("[dx12::ResourceSlabAllocator] Several spans") {
    using namespace dx12::internal;
    auto tracker = std::make_unique<AllocTracker>(false);
    {
        const int currentAdapter = 0;
        auto device = Device::Create(currentAdapter, kDebugBuild);
        ResourceSlabAllocator alloc(device.get(), tracker.get());

        // Span with 16 pages
        // 1 page is 2MiB
        // 1 slot is 4MiB 2 pages
        // 8 slots per span
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(kMaxSlotSize);
        // Allocate 3 spans
        constexpr auto kNumObjects = kMinSpanSize * 3;
        ResourceSlabAllocator::Allocation allocs[kNumObjects];

        for(auto i = 0; i < std::size(allocs); ++i) {
            auto result = alloc.Allocate(desc);
            CHECK(result);
            allocs[i] = *result;
        }
        CHECK_EQ(tracker->numAllocs, kNumObjects);
        CHECK_EQ(tracker->spans.size(), 3);

        // Free in random order
        std::random_device rd;
        std::mt19937 gen {rd()};
        std::ranges::shuffle(allocs, gen);

        for(auto i = 0; i < std::size(allocs); ++i) {
            alloc.Free(allocs[i]);
        }
        CHECK_EQ(tracker->numAllocs, 0);
        CHECK_EQ(tracker->spans.size(), 0);
    }
    CHECK_EQ(tracker->heaps.size(), 0);
    CHECK_EQ(tracker->spans.size(), 0);
}

TEST_CASE("[dx12::ResourceSlabAllocator] Random Test") {
    using namespace dx12::internal;
    constexpr bool kPrintEvents = false;
    auto tracker = std::make_unique<AllocTracker>(kPrintEvents);

    const int currentAdapter = 0;
    auto device = Device::Create(currentAdapter, kDebugBuild);
    ResourceSlabAllocator alloc(device.get(), tracker.get());
    alloc.PreallocateHeapsForTesting(HeapResourceType::Unknown, 4);

    constexpr auto kNumObjects = 512;
    constexpr auto kObjectsPerBucket = kNumObjects / kNumBuckets;

    struct AllocData {
        ResourceSlabAllocator::Allocation allocation;
        uint32_t bucket;
    };
    AllocData pool[kNumObjects];
    int poolIndex = 0;
    std::vector<int> allocs;

    for(int i = 0; i < std::size(pool); ++i) {
        AllocData& object = pool[i];
        object.bucket = (i / kObjectsPerBucket) + 1;
    }
    // Shuffle
    std::random_device rd;
    std::mt19937 gen {rd()};
    std::ranges::shuffle(pool, gen);

    constexpr auto kNumOps = kNumObjects * 2;
    const auto startTp = TimePoint::Now();

    for(auto i = 0; i < kNumOps; ++i) {
        const bool random = gen() % 2;
        const bool shouldAllocate =
            (random || allocs.empty() || i < kNumObjects / 4) &&
            poolIndex < kNumObjects;

        if (shouldAllocate) {
            AllocData& object = pool[poolIndex];
            const uint32_t size = object.bucket * kMinSlotSize;
            auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
            auto result = alloc.Allocate(desc);
            object.allocation = *result;
            allocs.push_back(poolIndex);
            ++poolIndex;
            continue;
        }
        alloc.Free(pool[allocs.back()].allocation);
        allocs.pop_back();
    }
    const auto endTp = TimePoint::Now();

    Println("[dx12::Allocator Random Test] Test statistics:");
    Println("[dx12::Allocator Random Test]   Time: {:.2f}ms", (endTp - startTp).ToMillisFloat());
    Println("[dx12::Allocator Random Test]   NumObjects: {}", kNumObjects);
    Println("[dx12::Allocator Random Test]   NumObjectsPerBucket: {}",
            kObjectsPerBucket);
    Println("[dx12::Allocator Random Test]   MaxHeaps: {}",
            tracker->maxNumHeaps);
    Println("[dx12::Allocator Random Test]   MaxSpans: {}",
            tracker->maxNumSpans);
    Println("[dx12::Allocator Random Test]   MaxAllocs: {}",
            tracker->maxNumAllocs);
    Println("[dx12::Allocator Random Test]   MaxSlotSize: {}KiB",
            tracker->maxSlotSize / 1024);
    Println("[dx12::Allocator Random Test]   MaxMemoryReserved: {}MiB",
            tracker->maxReserved / (1024 * 1024));
    Println("[dx12::Allocator Random Test]   MaxMemoryUsed: {}MiB",
            tracker->maxUsed / (1024 * 1024));
    Println("[dx12::Allocator Random Test]   MaxUtilization: {:.2}",
            tracker->maxUtilization);
}