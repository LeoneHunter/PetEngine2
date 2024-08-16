#include "util.h"
#include "string_utils.h"

#include <doctest.h>

namespace {

bool isDestructed = false;

struct RefCounted {

    void AddRef() { ++refCount; }

    void Release() { 
        --refCount; 
        if(!refCount) { 
            delete this; 
        } 
    }

    ~RefCounted() { isDestructed = true; }

    size_t refCount = 0;
};

struct Derived: public RefCounted {
    int data = 42;
};

} // namespace

TEST_CASE("[RefCountedPtr]") {
    RefCountedPtr<RefCounted> rc;
    {
        CHECK(!rc);
        rc = new RefCounted();
        CHECK_EQ(rc->refCount, 1);

        RefCountedPtr<RefCounted> rc2 = rc;
        CHECK_EQ(rc->refCount, 2);
        CHECK(rc2 == rc);

        RefCountedPtr<RefCounted> rc3 = std::move(rc2);
        CHECK_EQ(rc->refCount, 2);
        CHECK(rc3);
        CHECK(!rc2);
    }
    CHECK_EQ(rc->refCount, 1);
    rc = nullptr;
    CHECK(!rc);
    CHECK(isDestructed);

    RefCountedPtr<RefCounted> baseRc;
    {
        RefCountedPtr<Derived> derivedRc;
        derivedRc = new Derived();
        baseRc = derivedRc;
        CHECK_EQ(derivedRc->refCount, 2);
        CHECK_EQ(baseRc->refCount, 2);
    }
    CHECK_EQ(baseRc->refCount, 1);
}

TEST_CASE("[TrivialPoolAllocator]") {
    struct MyClass {
        size_t index = 42;
        int data = 13;
    };
    using Allocator = PooledAllocator<sizeof(MyClass), 4>;
    Allocator alloc;

    MyClass* obj1 = new (alloc.Allocate()) MyClass();
    obj1->data = 1;

    MyClass* obj2 = new (alloc.Allocate()) MyClass();
    obj2->data = 2;

    MyClass* obj3 = new (alloc.Allocate()) MyClass();
    obj3->data = 3;

    MyClass* obj4 = new (alloc.Allocate()) MyClass();
    obj4->data = 4;

    alloc.Free(obj2);
    alloc.Free(obj4);
    alloc.Free(obj3);
    alloc.Free(obj1);

    // Last freed should be the first allocated
    MyClass* obj11 = reinterpret_cast<MyClass*>(alloc.Allocate());
    CHECK_EQ(obj11->data, 1);

    MyClass* obj23 = reinterpret_cast<MyClass*>(alloc.Allocate());
    CHECK_EQ(obj23->data, 3);

    MyClass* obj24 = reinterpret_cast<MyClass*>(alloc.Allocate());
    CHECK_EQ(obj24->data, 4);

    MyClass* obj22 = reinterpret_cast<MyClass*>(alloc.Allocate());
    CHECK_EQ(obj22->data, 2);
}

TEST_CASE("[StringID]") {
	StringID sid1("New Material");
	StringID sid2("New Material");
	StringID sid3("new material");
	StringID sid4("NeW MaTerial");

	CHECK(sid1 == sid1);
	CHECK(sid2 == sid1);
	CHECK(sid3 == sid1);
	CHECK(sid4 == sid1);

	CHECK(sid1.String() == "New Material");
	CHECK(sid2.String() == "New Material");
	CHECK(sid3.String() == "new material");
	CHECK(sid4.String() == "NeW MaTerial");
}

TEST_CASE("[DList]") {
    struct Node {
        Node* next = nullptr;
        Node* prev = nullptr;
    };

    Node* head = nullptr;
    Node* _0 = new Node();
    Node* _1 = new Node();
    Node* _2 = new Node();
    DListPush(head, _0);
    DListPush(head, _1);
    DListPush(head, _2);
    // head -> [2] <-> [1] <-> [0] -> null
    CHECK(_0->prev == _1);
    CHECK(_0->next == nullptr);
    CHECK(_1->next == _0);
    CHECK(_1->prev == _2);
    CHECK(_2->next == _1);
    CHECK(_2->prev == nullptr);
    CHECK(head == _2);
    DListRemove(head, _1);
    // head -> [2] <-> [0] -> null
    CHECK(_0->prev == _2);
    CHECK(_2->next == _0);
    DListPop(head);
    // head -> [0] -> null
    CHECK(head == _0);
    DListPop(head);
    CHECK(head == nullptr);
    CHECK(_0->prev == nullptr);
    CHECK(_0->next == nullptr);
    CHECK(_1->next == nullptr);
    CHECK(_1->prev == nullptr);
    CHECK(_2->next == nullptr);
    CHECK(_2->prev == nullptr);
    delete _0;
    delete _1;
    delete _2;
}

TEST_CASE("[Color4f] From hex") {
    auto _1 = Color4f::FromHex("#abcdef00");
    CHECK_EQ(_1.r, 0xab / 255.f);
    CHECK_EQ(_1.g, 0xcd / 255.f);
    CHECK_EQ(_1.b, 0xef / 255.f);
    CHECK_EQ(_1.a, 0x00 / 255.f);
}
