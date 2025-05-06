#include "ring_buffer.h"
#include <doctest/doctest.h>

TEST_CASE("RingBuffer basic operations") {
  RingBuffer<int> rb(3);
  // Assume 3 is the capacity

  CHECK(rb.empty());
  CHECK(rb.size() == 0);

  SUBCASE("Push and Pop") {
    rb.push(1);
    rb.push(2);
    rb.push(3);
    CHECK(rb.size() == 3);
    CHECK(rb.front() == 1);

    rb.pop();
    CHECK(rb.front() == 2);
    CHECK(rb.size() == 2);

    rb.push(4);
    CHECK(rb.size() == 3);
    CHECK(rb.front() == 2);
  }

  SUBCASE("Overwriting behavior") {
    rb.push(1);
    rb.push(2);
    rb.push(3);
    rb.push(4);  // Should overwrite the oldest (1) if it's circular

    CHECK(rb.size() == 3);
    CHECK(rb.front() == 2);
  }

  SUBCASE("Clear operation") {
    rb.push(1);
    rb.push(2);
    rb.clear();
    CHECK(rb.empty());
  }

  SUBCASE("Wraparound behavior") {
    rb.push(10);
    rb.push(20);
    rb.pop();
    rb.push(30);
    rb.push(40);  // Should wrap internally

    CHECK(rb.size() == 3);
    CHECK(rb.front() == 20);
  }
}


struct TestClass {
  TestClass(int val) { ++constr; }
  ~TestClass() { ++destr; }

  static inline uint32_t constr = 0;
  static inline uint32_t destr = 0;
};

TEST_CASE("Constructors & destructors") {
  auto b = RingBuffer<TestClass>(3);
  for (auto i = 0; i < 10; ++i) {
    b.emplace(i);
  }
  b.clear();
  CHECK_EQ(TestClass::constr, TestClass::destr);
}