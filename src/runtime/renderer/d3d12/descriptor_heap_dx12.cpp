#include "descriptor_heap_dx12.h"


namespace dx12 {

DescriptorHeap::DescriptorHeap(ID3D12Device* device,
                               D3D12_DESCRIPTOR_HEAP_DESC desc,
                               const std::string& debugName) 
    : debugName_(debugName)
    , desc_(desc) {
    DASSERT(device);
    auto hr = device->CreateDescriptorHeap(&desc_, IID_PPV_ARGS(&d3dHeap_));
    DASSERT_HR(hr);
    segments_.push_back(SegmentMetadata{
        .startSlotIndex = 0,
        .size = desc_.NumDescriptors,
        .isFree = true,
    });
    freeSize_ = desc_.NumDescriptors;
    baseCpu_ = d3dHeap_->GetCPUDescriptorHandleForHeapStart();
    baseGpu_ = d3dHeap_->GetGPUDescriptorHandleForHeapStart();
    descriptorSize_ = device->GetDescriptorHandleIncrementSize(desc.Type);
}

// Return null if there're no enough space
std::optional<DescriptorLocation> DescriptorHeap::TryAllocate(size_t size) {
    if(size > desc_.NumDescriptors ||
        size > freeSize_) {
        return {};
    }
    const size_t requestedSize = size;
    // Index of the descriptor slot at the start of the segment
    size_t segmentStartSlotIndex = 0;
    
    for(size_t segmentIndex = 0; 
        segmentIndex < segments_.size(); 
        ++segmentIndex) {

        SegmentMetadata& segment = segments_[segmentIndex];

        if(!segment.isFree || segment.size < requestedSize) {
            segmentStartSlotIndex += segment.size;
            continue;
        }
        // TODO: Deal with fragmentation or just ignore if modifications are rare
        freeSize_ -= requestedSize;
        segment.isFree = false;
        // Split the segment
        if(segment.size > requestedSize) {
            const SegmentMetadata newFreeSegment{
                .startSlotIndex = segmentStartSlotIndex + requestedSize,
                .size = segment.size - requestedSize,
                .isFree = true
            };
            segment.size = requestedSize;
            // Insert before iterator so + 1
            const auto where = segments_.begin() + segmentIndex + 1;
            segments_.insert(where, newFreeSegment);
        }
        return GetLocationForIndex(segmentStartSlotIndex);
    }
    return {};
}

void DescriptorHeap::Free(DescriptorLocation location) {
    const size_t startSlotIndex = 
            (location.ptrCpu.ptr - baseCpu_.ptr) / descriptorSize_;
    // DASSERT in range
    DASSERT((location.ptrCpu.ptr % descriptorSize_) == 0);
    DASSERT(ContainsLocation(location));

    // Find the segment with binary search
    size_t pivot = 0;
    auto& segment = [&]()->SegmentMetadata& {
        size_t lowerBound = 0;
        size_t higherBound = segments_.size();

        for(;;) {
            pivot = lowerBound + ((higherBound - lowerBound) / 2);
            SegmentMetadata& segment = segments_[pivot];
            if(startSlotIndex == segment.startSlotIndex) {
                return segment;
            }
            if(startSlotIndex > segment.startSlotIndex) {
                DASSERT_F(
                    [&] {
                        const bool isLastSegment = pivot + 1 >= segments_.size();
                        const size_t nextSegmentStartSlot = isLastSegment 
                            ? GetCapacity() 
                            : segments_[pivot + 1].startSlotIndex;
                        return startSlotIndex >= nextSegmentStartSlot;
                    }(),
                    "Descriptor location points to the center of the allocated segment"
                );
                lowerBound = pivot;
            } else {
                higherBound = pivot;
            }
        }
    }();
    const size_t segmentIndex = pivot;
    segment.isFree = true;
    freeSize_ += segment.size;

    // Try to merge the segment with neighbors from right to left
    const bool mergeLeft = segmentIndex 
        ? segments_[segmentIndex - 1].isFree : false;

    const size_t lastSegmentIndex = segments_.size() - 1;
    const bool mergeRight = segmentIndex < lastSegmentIndex
        ? segments_[segmentIndex + 1].isFree : false;

    // [xx used xx] [xx current xx] [-- free --]
    if(mergeRight) {
        SegmentMetadata& right = segments_[segmentIndex + 1];
        segment.size += right.size;
        segments_.erase(segments_.begin() + segmentIndex + 1);
    }
    // [-- free --] [--------- current --------]
    // OR
    // [-- free --] [xx current xx] [xx used xx]
    if(mergeLeft) {
        SegmentMetadata& left = segments_[segmentIndex - 1];
        segment.size += left.size;
        segment.startSlotIndex = left.startSlotIndex;
        // Erase left
        segments_.erase(segments_.begin() + segmentIndex - 1);
    }
}

bool DescriptorHeap::ContainsLocation(DescriptorLocation location) const {
    const DescriptorLocation descriptorLocationEnd = 
            GetLocationForIndex(desc_.NumDescriptors);
    return location.ptrCpu.ptr >= baseCpu_.ptr && 
            location.ptrCpu.ptr < descriptorLocationEnd.ptrCpu.ptr;
}




// static 
std::unique_ptr<PooledDescriptorAllocator> PooledDescriptorAllocator::Create(
        Device* parent,
        const std::string& debugName) {
   std::unique_ptr<PooledDescriptorAllocator> out(new PooledDescriptorAllocator());
   out->debugName_ = debugName;
   out->device_ = parent;
   return out;
}

DescriptorHeap& PooledDescriptorAllocator::CreateHeap(
        D3D12_DESCRIPTOR_HEAP_DESC desc, const std::string& debugName) {
    heaps_.emplace_back(new DescriptorHeap(device_->GetDevice(), desc, debugName));
    return *heaps_.back();
}

DescriptorLocation PooledDescriptorAllocator::Allocate(
        const D3D12_DESCRIPTOR_HEAP_DESC& desc) {

    for(auto& ptr: heaps_) {
        DescriptorHeap& heap = *ptr;
        if(!heap.HasProperties(desc)) {
            continue;
        }
        // Check if has a free segment with enough size
        std::optional<DescriptorLocation> out = heap.TryAllocate(desc.NumDescriptors);
        if(out) {
            return *out;
        }
    }
    // Create a new heap with specified properties
    D3D12_DESCRIPTOR_HEAP_DESC newHeapDesc = desc;
    newHeapDesc.NumDescriptors = desc.NumDescriptors * 128;
    DescriptorHeap& newHeap = CreateHeap(desc);

    std::optional<DescriptorLocation> out = newHeap.TryAllocate(desc.NumDescriptors);
    DASSERT(out);
    return *out;
}

void PooledDescriptorAllocator::Free(DescriptorLocation location) {
    for(auto& ptr: heaps_) {
        DescriptorHeap& heap = *ptr;
        if(heap.ContainsLocation(location)) {
            heap.Free(location);
        }
    }
}

} // namespace dx12
