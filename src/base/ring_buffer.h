#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <base/common.h>

// Standart Ring buffer implementation
// No overalignment, allocators, copy
template <typename T>
class RingBuffer {
public:
  static_assert(alignof(T) <= alignof(std::max_align_t),
                "Type requires over-aligned allocation");

  // Constructor with explicit capacity
  constexpr explicit RingBuffer(size_t capacity)
      : buffer_(static_cast<T*>(::operator new(capacity * sizeof(T))))
      , capacity_(capacity) {
    DASSERT(capacity != 0);
  }

  constexpr ~RingBuffer() { 
    clear(); 
    ::operator delete(buffer_);
  }

  constexpr RingBuffer(const RingBuffer&) = delete;
  constexpr RingBuffer& operator=(const RingBuffer&) = delete;

  constexpr RingBuffer(RingBuffer&& other) noexcept
      : capacity_(0), buffer_(nullptr), head_(0), tail_(0), size_(0) {
    swap(*this, other);
  }

  constexpr RingBuffer& operator=(RingBuffer&& other) noexcept {
    if (this != &other) {
      RingBuffer temp(std::move(other));  
      swap(*this, temp);
      return *this;
    }
  }

  constexpr friend void swap(RingBuffer& first, RingBuffer& second) noexcept {
    using std::swap;
    swap(first.capacity_, second.capacity_);
    swap(first.buffer_, second.buffer_);
    swap(first.head_, second.head_);
    swap(first.tail_, second.tail_);
    swap(first.size_, second.size_);
  }

public:
  constexpr void push(const T& val) { emplace(val); }

  constexpr void push(T&& val) { emplace(std::move(val)); }

  // Push an element to the back of the buffer
  template <class... Args>
    requires std::constructible_from<T, Args...>
  constexpr void emplace(Args&&... args) {
    if (full()) {
      // Overwrite the oldest element if buffer is full
      std::destroy_at(&buffer_[tail_]);
      std::construct_at(&buffer_[tail_], std::forward<Args>(args)...);
      tail_ = (tail_ + 1) % capacity_;
      head_ = tail_;  // Head catches up to tail when overwriting
    } else {
      std::construct_at(&buffer_[head_], std::forward<Args>(args)...);
      head_ = (head_ + 1) % capacity_;
      ++size_;
    }
  }

  // Pop an element from the front of the buffer
  constexpr std::optional<T> pop() {
    if (empty()) {
      return std::nullopt;
    }
    T value = std::move(buffer_[tail_]);
    std::destroy_at(&buffer_[tail_]);
    tail_ = (tail_ + 1) % capacity_;
    --size_;
    return value;
  }

  constexpr T& front() {
    DASSERT(!empty());
    return buffer_[tail_];
  }

  constexpr const T& front() const {
    DASSERT(!empty());
    return buffer_[tail_];
  }

  constexpr T& back() {
    DASSERT(!empty());
    return buffer_[(head_ + capacity_ - 1) % capacity_];
  }

  constexpr const T& back() const {
    DASSERT(!empty());
    return buffer_[(head_ + capacity_ - 1) % capacity_];
  }

  // Capacity queries
  constexpr bool empty() const noexcept { return size_ == 0; }
  constexpr bool full() const noexcept { return size_ == capacity_; }
  constexpr size_t size() const noexcept { return size_; }
  constexpr size_t capacity() const noexcept { return capacity_; }

  // Clear the buffer
  constexpr void clear() noexcept {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      while (!empty()) {
        std::destroy_at(&buffer_[tail_]);
        tail_ = (tail_ + 1) % capacity_;
        --size_;
      }
    } else {
      head_ = 0;
      tail_ = 0;
      size_ = 0;
    }
  }

private:
  T* buffer_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
  size_t capacity_;
};

#endif  // RING_BUFFER_H