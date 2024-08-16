#include "device_dx12.h"
#include "common_dx12.h"
#include "slab_allocator_dx12_v2.h"

#include "thirdparty/doctest/doctest.h"
#include "runtime/util/bench.h"

#include <map>

using namespace d3d12;

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
    StringBuilder sb;
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
        reservedBytes += info.size;
        maxReserved = std::max(maxReserved, reservedBytes);
    }

    void DidDestroyHeap(uintptr_t ptr) override {
        auto it = heaps.find(ptr);
        if(printEvents) {
            Println("Destroyed heap: \n{}", Formatter::Format(it->second.info));
        }
        reservedBytes -= it->second.info.size;
        heaps.erase(it);
    }

    void DidCommitSpan(const SpanID& id, const SpanInfo& info) override {
        if(printEvents) {
            Println("Committed span: \n{}", FormatSpan(id, info));
        }
        spans.emplace(id, info);
        maxNumSpans = std::max(maxNumSpans, spans.size());
        const uint32_t slotSize = internal::SizeClassToSlotSize(info.sizeClass);
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
        const uint32_t slotSize = internal::SizeClassToSlotSize(span.sizeClass);

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
        const uint32_t slotSize = span.sizeClass * internal::kMinSlotSize;

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


TEST_CASE("[d3d12::SlabAllocator_v2] Random Test") {
    constexpr bool kPrintEvents = false;
    auto tracker = std::make_unique<AllocTracker>(kPrintEvents);

    const int currentAdapter = 0;
    auto device = Device::Create(currentAdapter, kDebugBuild);
    HeapAllocator heapAlloc(device.get(), D3D12_HEAP_TYPE_DEFAULT);
    SlabAllocator alloc(&heapAlloc, tracker.get());
    // 1 GiB
    alloc.PreallocateHeap(4);

    constexpr auto kNumObjects = 512;
    constexpr auto kObjectsPerBucket = kNumObjects / internal::kNumBuckets;

    struct AllocData {
        SlabAllocator::Allocation allocation;
        uint32_t bucket;
    };
    std::vector<AllocData> pool;
    pool.resize(kNumObjects);
    // Generate buckets
    for(int i = 0; i < std::size(pool); ++i) {
        AllocData& object = pool[i];
        object.bucket = (i / kObjectsPerBucket) + 1;
    }
    // Shuffle
    std::random_device rd;
    std::mt19937 gen {rd()};
    std::ranges::shuffle(pool, gen);

    constexpr auto kNumOps = kNumObjects * 2;
    Benchmark bench;

    bench.SetMain([&]{
        int poolIndex = 0;
        std::vector<int> allocs;

        for(auto i = 0; i < kNumOps; ++i) {
            const bool random = gen() % 2;
            const bool shouldAllocate =
                (random || allocs.empty() || i < kNumObjects / 4) &&
                poolIndex < kNumObjects;

            if(shouldAllocate) {
                AllocData& object = pool[poolIndex];
                const uint32_t size = object.bucket * internal::kMinSlotSize;
                auto result = alloc.Allocate(size);
                object.allocation = *result;
                allocs.push_back(poolIndex);
                ++poolIndex;
                continue;
            }
            alloc.Free(pool[allocs.back()].allocation);
            allocs.pop_back();
        }
    });
    bench.Run(20);
    Benchmark::Stats stats = bench.GetStats();

    Println("[d3d12::Allocator_v2 Random Test] Test statistics:");
    Println("[d3d12::Allocator_v2 Random Test]   Num iterations: {}",
            stats.numIters);
    Println("[d3d12::Allocator_v2 Random Test]   Average wall time: {:.3f} ms",
            stats.wallTime.average * 1000.);
    Println("[d3d12::Allocator_v2 Random Test]   Average CPU time: {:.3f} ms",
            stats.cpuTime.average * 1000.);
    Println("[d3d12::Allocator_v2 Random Test]   Min wall time: {:.3f} ms",
            stats.wallTime.min * 1000.);
    Println("[d3d12::Allocator_v2 Random Test]   Min CPU time: {:.3f} ms",
            stats.cpuTime.min * 1000.);
    Println("[d3d12::Allocator_v2 Random Test]   Num objects: {}", 
            kNumObjects);
    Println("[d3d12::Allocator_v2 Random Test]   Num objects per bucket: {}",
            kObjectsPerBucket);
    Println("[d3d12::Allocator_v2 Random Test]   Max heaps: {}",
            tracker->maxNumHeaps);
    Println("[d3d12::Allocator_v2 Random Test]   Max spans: {}",
            tracker->maxNumSpans);
    Println("[d3d12::Allocator_v2 Random Test]   Max allocs: {}",
            tracker->maxNumAllocs);
    Println("[d3d12::Allocator_v2 Random Test]   Max slot size: {} KiB",
            tracker->maxSlotSize / 1024);
    Println("[d3d12::Allocator_v2 Random Test]   Max memory reserved: {} MiB",
            tracker->maxReserved / (1024 * 1024));
    Println("[d3d12::Allocator_v2 Random Test]   Max memory used: {} MiB",
            tracker->maxUsed / (1024 * 1024));
    Println("[d3d12::Allocator_v2 Random Test]   Max utilization: {:.2f}",
            tracker->maxUtilization);
}