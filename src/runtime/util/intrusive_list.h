#include "runtime/common.h"


template<class T>
struct ListNode {
    T* next = nullptr;
};

template<class T>
struct DListNode {
    T* next = nullptr;
    T* prev = nullptr;
};

template <class T>
concept IsDListNode = requires(T node) {
    { node.next } -> std::convertible_to<T*>;
    { node.prev } -> std::convertible_to<T*>;
};

template<class T>
concept IsListNode = !IsDListNode<T> && requires(T node) {
    { node.next } -> std::convertible_to<T*>;
};

template<class T>
    requires IsListNode<T>
constexpr void ListPush(T*& head, T* node) {
    node->next = head;
    head = node;
}

template<class T>
    requires IsListNode<T>
constexpr T* ListPop(T*& head) {
    if(!head) {
        return nullptr;
    }
    T* pop = head;
    head = head->next;
    pop->next = nullptr;
    return pop;
}

template<class T>
    requires IsListNode<T>
constexpr void ListDelete(T*& head) {
    while(T* node = ListPop(head)) {
        delete node;
    }
    head = nullptr;
}


template<class T>
    requires IsListNode<T> || IsDListNode<T>
constexpr bool ListContains(T* head, T* entry) {
    for(T* node = head; node; node = node->next) {
        if(node == entry) {
            return true;
        }
    }
    return false;
}

template<class T>
    requires IsListNode<T> || IsDListNode<T>
constexpr size_t ListSize(T* head) {
    size_t out = 0;
    for(T* node = head; node; node = node->next) {
        ++out;
    }
    return out;
}

// Remove a node from double linked list
template<class T>
    requires IsDListNode<T>
constexpr void DListRemove(T*& head, T* node) {
    if(node->prev) {
        node->prev->next = node->next;
    }
    if(node->next) {
        node->next->prev = node->prev;
    }
    if(head == node) {
        head = node->next;
    }
    node->next = nullptr;
    node->prev = nullptr;
}

template<class T>
    requires IsDListNode<T>
constexpr void DListPush(T*& head, T* node) {
    node->next = head;
    if(head) {
        head->prev = node;
    }
    head = node;
}

template<class T>
    requires IsDListNode<T>
constexpr T* DListPop(T*& head) {
    if(!head) {
        return nullptr;
    }
    T* oldHead = head;
    head = head->next;
    if(head) {
        head->prev = nullptr;
    }
    oldHead->next = nullptr;
    return oldHead;
}

template<class T>
    requires IsDListNode<T>
constexpr void ListDelete(T*& head) {
    while(T* node = DListPop(head)) {
        delete node;
    }
    head = nullptr;
}

template<class T>
    requires IsListNode<T> || IsDListNode<T>
constexpr auto ListIterate(T* node) {

    struct ListIterator {

        constexpr ListIterator(T* node): node(node) {}

        struct iterator {

            constexpr iterator() = default;
            
            constexpr iterator(T* node): node(node) {}

            constexpr iterator& operator++() { 
                node = node->next;
                return *this;
            }

            constexpr iterator operator++(int) {
                iterator tmp = *this;
                this->operator++();
                return tmp;
            }

            constexpr T* operator*() const {
                return node;
            }

            constexpr T* operator->() const {
                return node;
            }

            constexpr bool operator==(const iterator& right) const {
                return node == right.node;
            }

            T* node = nullptr;
        };

        iterator begin() { return {node}; }
        iterator end() { return {}; }

        T* node = nullptr;
    };

    return ListIterator{node};
}