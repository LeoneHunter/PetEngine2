#ifndef CHANNEL_H
#define CHANNEL_H

#include <base/common.h>
#include <base/ring_buffer.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

template <class T>
class Channel;

template <class T>
class ChannelReceiveEnd;

template <class T>
class ChannelSendEnd;

// Channel control block
template <class T>
class Channel {
public:
  friend class ChannelReceiveEnd<T>;
  friend class ChannelSendEnd<T>;

  explicit Channel(size_t capacity) : buffer_(capacity) {}

  // No point in copying or moving a shared state
  Channel(Channel&&) = delete;
  Channel& operator=(Channel&&) = delete;

public:
  std::optional<T> Receive() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !buffer_.empty() || closed_; });

    if (buffer_.empty() && closed_) {
      return std::nullopt;
    }

    auto value = buffer_.pop();
    cv_.notify_one();
    return value;
  }

  template <class... Args>
  bool Send(Args&&... value) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return !buffer_.full() || closed_; });

    if (closed_) {
      return false;
    }

    buffer_.emplace(std::forward<Args>(value)...);
    cv_.notify_one();
    return true;
  }

  void Close() {
    std::lock_guard lock(mutex_);
    closed_ = true;
    cv_.notify_all();
  }

  bool IsClosed() const {
    std::lock_guard lock(mutex_);
    return closed_;
  }

private:
  RingBuffer<T> buffer_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool closed_ = false;
};

template <class T>
class ChannelReceiveEnd {
public:
  explicit ChannelReceiveEnd(std::shared_ptr<Channel<T>> channel)
      : channel_(std::move(channel)) {}

  std::optional<T> Receive() { return channel_->Receive(); }

  void Close() { channel_->Close(); }

private:
  std::shared_ptr<Channel<T>> channel_;
};

template <class T>
class ChannelSendEnd {
public:
  ChannelSendEnd() = default;

  explicit ChannelSendEnd(std::shared_ptr<Channel<T>> channel)
      : channel_(std::move(channel)) {}

  bool Send(T&& val) {
    DASSERT(channel_);
    return channel_->Send(std::move(val));
  }

  bool Send(const T& val) {
    DASSERT(channel_);
    return channel_->Send(val);
  }

  void Close() {
    DASSERT(channel_);
    channel_->Close();
  }

  bool IsClosed() const {
    DASSERT(channel_);
    return channel_->IsClosed();
  }

private:
  std::shared_ptr<Channel<T>> channel_;
};

// Factory function to create channel ends
template <class T>
std::pair<ChannelSendEnd<T>, ChannelReceiveEnd<T>> MakeChannel(
  size_t capacity = 0) {
  auto channel = std::make_shared<Channel<T>>(capacity);
  return {ChannelSendEnd<T>(channel), ChannelReceiveEnd<T>(channel)};
}

#endif  // CHANNEL_H