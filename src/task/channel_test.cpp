#include "channel.h"
#include <atomic>
#include <future>
#include <random>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

// Basic functionality tests
TEST_CASE("Single threaded Send and Receive") {
  auto [send_end, recv_end] = MakeChannel<int>(2);

  SUBCASE("Send and Receive single value") {
    CHECK(send_end.Send(42));
    auto received = recv_end.Receive();
    CHECK(received.has_value());
    CHECK(received.value() == 42);
  }
}

TEST_CASE("Channel closing behavior") {
  auto [send_end, recv_end] = MakeChannel<std::string_view>(2);

  SUBCASE("Receive after Close returns nullopt") {
    send_end.Close();
    auto received = recv_end.Receive();
    CHECK_FALSE(received.has_value());
  }

  SUBCASE("Send after Close fails") {
    send_end.Close();
    CHECK_FALSE(send_end.Send("test"));
  }

  SUBCASE("Pending receives complete after Close") {
    std::thread receiver([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      auto received = recv_end.Receive();
      CHECK(received.has_value());
      CHECK(received.value() == "hello");

      received = recv_end.Receive();
      CHECK_FALSE(received.has_value());  // Channel closed
    });

    send_end.Send("hello");
    send_end.Close();
    receiver.join();
  }
}

// Concurrent tests
TEST_CASE("Multiple producers single consumer (fan-in)") {
  constexpr size_t num_producers = 5;
  constexpr size_t messages_per_producer = 1000;
  auto [send_end, recv_end] = MakeChannel<int>(10);

  std::vector<std::thread> producers;
  std::atomic<int> counter{0};

  for (int i = 0; i < num_producers; ++i) {
    producers.emplace_back([&, i]() {
      for (int j = 0; j < messages_per_producer; ++j) {
        if (send_end.Send(i * 1000 + j)) {
          counter++;
        }
      }
    });
  }

  std::thread consumer([&]() {
    int received = 0;
    while (received < num_producers * messages_per_producer) {
      if (auto val = recv_end.Receive()) {
        received++;
      }
    }
  });

  for (auto& p : producers)
    p.join();
  send_end.Close();
  consumer.join();

  CHECK(counter == num_producers * messages_per_producer);
}

TEST_CASE("Single producer multiple consumers (fan-out)") {
  constexpr size_t num_consumers = 5;
  constexpr size_t total_messages = 10000;
  auto [send_end, recv_end] = MakeChannel<int>(10);

  std::vector<std::thread> consumers;
  std::atomic<int> received_count{0};
  std::vector<int> received_values(total_messages, 0);

  for (size_t i = 0; i < num_consumers; ++i) {
    consumers.emplace_back([&]() {
      while (true) {
        auto val = recv_end.Receive();
        if (!val)
          break;
        int v = val.value();
        if (v >= 0 && v < total_messages) {
          received_values[v]++;
          received_count++;
        }
      }
    });
  }

  std::thread producer([&]() {
    for (int i = 0; i < total_messages; ++i) {
      while (!send_end.Send(i)) {
        std::this_thread::yield();
      }
    }
    send_end.Close();
  });

  producer.join();
  for (auto& c : consumers)
    c.join();

  CHECK(received_count == total_messages);
  bool passed = true;
  for (size_t i = 0; i < total_messages; ++i) {
    if (received_values[i] != 1) {
      passed = false;
      break;
    }
  }
  CHECK(passed);
}

TEST_CASE("Stress test with random operations") {
  constexpr size_t num_threads = 8;
  constexpr size_t operations_per_thread = 5000;

  auto [send_end, recv_end] = MakeChannel<int>(10);

  std::vector<std::thread> threads;
  std::atomic<int> total_sent{0};
  std::atomic<int> total_received{0};
  std::atomic<bool> done{false};

  // Random number generator
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> op_dist(0, 3);

  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {
      for (size_t j = 0; j < operations_per_thread; ++j) {
        int op = op_dist(gen);
        switch (op) {
          case 0:  // Send
            if (send_end.Send(i)) {
              total_sent++;
            }
            break;
          case 1:  // Receive
            if (recv_end.Receive()) {
              total_received++;
            }
            break;
          case 2:  // try Send
          {
            int value = i * 1000 + j;
            if (send_end.Send(std::move(value))) {
              total_sent++;
            }
            break;
          }
          case 3:  // try Receive
            if (recv_end.Receive()) {
              total_received++;
            }
            break;
        }
        std::this_thread::yield();
      }
    });
  }

  // Let threads run for a while
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Close channel and join threads
  send_end.Close();
  for (auto& t : threads)
    t.join();

  // Drain remaining messages
  while (true) {
    if (recv_end.Receive()) {
      total_received++;
    } else {
      break;
    }
  }

  CHECK(total_received == total_sent);
}

TEST_CASE("Channel with move-only type") {
  struct MoveOnly {
    int value;
    MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
  };
  auto [send_end, recv_end] = MakeChannel<MoveOnly>(2);

  SUBCASE("Move-only Send and Receive") {
    CHECK(send_end.Send(MoveOnly(42)));
    auto received = recv_end.Receive();
    CHECK(received.has_value());
    CHECK(received->value == 42);
  }

  SUBCASE("Move-only stress test") {
    constexpr int count = 1000;
    std::thread producer([&]() {
      for (int i = 0; i < count; ++i) {
        while (!send_end.Send(MoveOnly(i))) {
          std::this_thread::yield();
        }
      }
      send_end.Close();
    });

    std::thread consumer([&]() {
      for (int i = 0; i < count; ++i) {
        auto val = recv_end.Receive();
        CHECK(val.has_value());
        CHECK(val->value == i);
      }
      auto val = recv_end.Receive();
      CHECK_FALSE(val.has_value());
    });

    producer.join();
    consumer.join();
  }
}