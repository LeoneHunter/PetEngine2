#pragma once
#include "base/common.h"
#include "base/util.h"

// Buddy (binary) allocator with preallocated noded
// Because of preallocated nodes better to be used for small range of sizes
class BuddyAlloc {
public:
    constexpr static uint32_t kOutOfMemoryOffset =
        std::numeric_limits<uint32_t>::max();

public:
    BuddyAlloc(uint32_t maxSize, uint32_t minSize) {
        maxBlockSize_ = std::bit_ceil(maxSize);
        minBlockSize_ = std::bit_ceil(minSize);
        const uint32_t numLevels =
            std::countr_zero(maxBlockSize_) - std::countr_zero(minSize) + 1;
        buckets_.resize(numLevels, kInvalidBlockIndex);
        blocks_.resize(maxBlockSize_ / minBlockSize_);
        // Write root block with max size
        InitBlock(kRootBlockIndex, maxBlockSize_, false);
        PushFreeBlock(kRootBlockIndex, kRootBlockLevel);
    }

    uint32_t Allocate(uint32_t size) {
        DASSERT(size > 0 && size <= maxBlockSize_);
        size = std::max(size, minBlockSize_);
        // Align to pow2
        const uint32_t blockSize = std::bit_ceil(size);
        BlockInfo block = FindBlockCeil(blockSize);
        if (block.index == kInvalidBlockIndex) {
            return kOutOfMemoryOffset;
        }
        if (block.size != blockSize) {
            block = SplitUntil(block, blockSize);
        }
        InitBlock(block, true);
        return GetOffsetFromBlock(block.index);
    }

    void Free(uint32_t offset) {
        BlockInfo block = GetBlockFromOffset(offset);
        DASSERT(blocks_[block.index].used);
        for (;;) {
            if (block.level == kRootBlockLevel) {
                PushFreeBlock(block.index, block.level);
                break;
            }
            const BlockIndex buddyIndex = GetBuddyIndex(block);
            const BlockIndex lhs =
                buddyIndex > block.index ? block.index : buddyIndex;
            const BlockIndex rhs =
                buddyIndex < block.index ? block.index : buddyIndex;
            // Merge if both blocks are of the same size and the buddy is free
            BlockMetadata& buddy = blocks_[buddyIndex];
            if (!buddy.used && buddy.size == block.size) {
                RemoveFreeBlock({buddyIndex, block.level, block.size});
                InitBlock(lhs, block.size * 2, false);
                InitBlock(rhs, 0, false);
                block = {lhs, block.level - 1, block.size * 2};
                continue;
            }
            PushFreeBlock(block);
            break;
        }
    }

private:
    using BlockIndex = uint32_t;
    using Level = uint32_t;

    constexpr static uint32_t kRootBlockIndex = 0;
    constexpr static uint32_t kRootBlockLevel = 0;
    constexpr static BlockIndex kInvalidBlockIndex =
        std::numeric_limits<uint32_t>::max() >> 1;

    struct BlockMetadata {
        uint32_t size = 0;
        uint32_t next : 31 = kInvalidBlockIndex;
        uint32_t used : 1 = false;
    };

    struct BlockInfo {
        BlockIndex index;
        Level level;
        uint32_t size;
    };

private:
    constexpr uint32_t GetOffsetFromBlock(BlockIndex block) {
        return block * minBlockSize_;
    }

    constexpr uint32_t LevelFromBlockSize(uint32_t size) {
        return (buckets_.size() - 1) -
               (std::countr_zero(size) - std::countr_zero(minBlockSize_));
    }

    constexpr void InitBlock(BlockIndex index, uint32_t size, bool used) {
        BlockMetadata& block = blocks_[index];
        block.size = size;
        block.used = used;
    }

    constexpr void InitBlock(BlockInfo info, bool used) {
        BlockMetadata& block = blocks_[info.index];
        block.size = info.size;
        block.used = used;
    }

    constexpr BlockInfo GetBlockFromOffset(uint32_t offset) {
        const BlockIndex index = offset / minBlockSize_;
        DASSERT(index < blocks_.size());
        BlockMetadata& block = blocks_[index];
        return {index, LevelFromBlockSize(block.size), block.size};
    }

    constexpr void RemoveFreeBlock(BlockInfo block) {
        BlockMetadata& blockMetadata = blocks_[block.index];
        BlockIndex& head = buckets_[block.level];
        DASSERT(head != kInvalidBlockIndex);
        // Iterate all nodes
        BlockIndex entry = head;
        BlockIndex prevEntry = kInvalidBlockIndex;
        while (entry != kInvalidBlockIndex) {
            if (entry == block.index) {
                if (entry == head) {
                    head = blocks_[entry].next;
                } else {
                    blocks_[prevEntry].next = blocks_[entry].next;
                }
                blocks_[entry].next = kInvalidBlockIndex;
                return;
            }
            prevEntry = entry;
            entry = blocks_[entry].next;
        }
        UNREACHABLE();
    }

    constexpr BlockIndex TryRemoveBlock(uint32_t level) {
        BlockIndex& head = buckets_[level];
        if (head == kInvalidBlockIndex) {
            return kInvalidBlockIndex;
        }
        BlockIndex block = head;
        head = blocks_[head].next;
        blocks_[block].next = kInvalidBlockIndex;
        blocks_[block].used = true;
        return block;
    }

    constexpr void PushFreeBlock(BlockIndex index, Level level) {
        blocks_[index].next = buckets_[level];
        buckets_[level] = index;
        blocks_[index].used = false;
    }

    constexpr void PushFreeBlock(BlockInfo info) {
        blocks_[info.index].next = buckets_[info.level];
        buckets_[info.level] = info.index;
        blocks_[info.index].used = false;
    }

    constexpr BlockInfo FindBlockCeil(uint32_t size) {
        Level level = std::countr_zero(maxBlockSize_) - std::countr_zero(size);
        for (++level; level-- > 0;) {
            const BlockIndex block = TryRemoveBlock(level);
            if (block != kInvalidBlockIndex) {
                return {block, level, size};
            }
            size *= 2;
        }
        return {kInvalidBlockIndex, 0, 0};
    }

    constexpr BlockInfo SplitUntil(BlockInfo block, uint32_t size) {
        while (block.size != size) {
            const uint32_t newSize = block.size / 2;
            const Level level = block.level + 1;
            const BlockIndex left = block.index;
            const BlockIndex right = left + (newSize / minBlockSize_);
            InitBlock(left, newSize, false);
            InitBlock(right, newSize, false);
            PushFreeBlock(right, level);
            block = BlockInfo{left, level, newSize};
        }
        return block;
    }

    constexpr BlockInfo GetParentBlock(BlockInfo block) {
        const Level parentSize = block.size * 2;
        const BlockIndex parentIndexAlignment = parentSize / minBlockSize_;
        const BlockIndex parentIndex =
            AlignDown(block.index, parentIndexAlignment);
        return {parentIndex, block.level / 2, parentSize};
    }

    constexpr BlockIndex GetBuddyIndex(BlockInfo block) {
        DASSERT(block.level != kRootBlockLevel);
        const BlockInfo parent = GetParentBlock(block);
        if (parent.index == block.index) {
            return block.index + (block.size / minBlockSize_);
        } else {
            return parent.index;
        }
    }

private:
    uint32_t maxBlockSize_;
    uint32_t minBlockSize_;
    // Preallocated blocks of the smallest size
    // Actual block size is stored inside the first metadata
    std::vector<BlockMetadata> blocks_;
    // Freelists by level
    // 0 is root (maxSize_)
    std::vector<BlockIndex> buckets_;
};