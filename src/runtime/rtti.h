#pragma once
#include <array>
#include <string>

#include "error.h"

#define DEFINE_CLASS_META(className, superClassName)               \
public:                                                            \
    static inline const ClassMeta* _StaticClass =                  \
        StaticClassRegistrator{#className, #superClassName}.Class; \
    virtual const ClassMeta* GetClass() const override {           \
        return _StaticClass;                                       \
    }                                                              \
    static const ClassMeta* GetStaticClass() {                     \
        return _StaticClass;                                       \
    }                                                              \
    virtual std::string_view GetClassName() const override {       \
        return #className;                                         \
    }                                                              \
    static std::string_view GetStaticClassName() {                 \
        return #className;                                         \
    }

#define DEFINE_ROOT_CLASS_META(className)                                    \
public:                                                                      \
    static inline const ClassMeta* _StaticClass =                            \
        StaticClassRegistrator{#className, ""}.Class;                        \
    virtual const ClassMeta* GetClass() const {                              \
        return _StaticClass;                                                 \
    }                                                                        \
    static const ClassMeta* GetStaticClass() {                               \
        return _StaticClass;                                                 \
    }                                                                        \
    virtual std::string_view GetClassName() const {                          \
        return #className;                                                   \
    }                                                                        \
    static std::string_view GetStaticClassName() {                           \
        return #className;                                                   \
    }                                                                        \
                                                                             \
public:                                                                      \
    template <class ClassType>                                               \
    bool IsA() const {                                                       \
        if (ClassType::GetStaticClass() == GetClass()) {                     \
            return true;                                                     \
        }                                                                    \
        auto* super = GetClass()->Super;                                     \
        for (auto* super = GetClass()->Super; super; super = super->Super) { \
            if (super == ClassType::GetStaticClass()) {                      \
                return true;                                                 \
            }                                                                \
        }                                                                    \
        return false;                                                        \
    }                                                                        \
                                                                             \
    template <class ClassType>                                               \
    ClassType* As() {                                                        \
        if (IsA<ClassType>()) {                                              \
            return static_cast<ClassType*>(this);                            \
        }                                                                    \
        return nullptr;                                                      \
    }                                                                        \
                                                                             \
    template <class ClassType>                                               \
    const ClassType* As() const {                                            \
        if (IsA<ClassType>()) {                                              \
            return static_cast<const ClassType*>(this);                      \
        }                                                                    \
        return nullptr;                                                      \
    }


struct ClassMeta {
    ClassMeta* Super = nullptr;
    std::string ClassName;
};

/*
 * Stores C++ inheritance class tree
 * Used for safe object casting
 */
class ClassTree {
public:
    constexpr static inline auto BufferSize = 100;

    using container_type = std::array<ClassMeta, BufferSize>;
    using iterator = typename container_type::iterator;

public:
    ClassTree() : classList_{}, tail_(0) {}

    // Called statically before main
    // Which means call order is unspecified
    const ClassMeta* RegisterClass(std::string_view inClassName,
                                   std::string_view inSuperClassName) {
        DASSERT(tail_ < BufferSize);
        ClassMeta* superClassPtr = nullptr;

        if (!inSuperClassName.empty()) {
            auto superClass =
                std::ranges::find_if(classList_, [&](const ClassMeta& inClass) {
                    return inClass.ClassName == inSuperClassName;
                });

            if (superClass == classList_.end()) {
                // Precreate superclass for later, when the actual class gets to
                // register, update its super
                classList_[tail_].ClassName = std::string(inSuperClassName);
                superClass = classList_.begin() + tail_;
                ++tail_;
            }
            superClassPtr = &*superClass;
        }
        // Check if already added as a super previously
        auto classObject =
            std::ranges::find_if(classList_, [&](const ClassMeta& inClass) {
                return inClass.ClassName == inClassName;
            });

        if (classObject == classList_.end()) {
            classList_[tail_].Super = superClassPtr;
            classList_[tail_].ClassName = std::string(inClassName);
            classObject = classList_.begin() + tail_;
            ++tail_;

        } else {
            classObject->Super = superClassPtr;
        }
        return &*classObject;
    }

public:
    static ClassTree* Instance() {
        static ClassTree* instance = nullptr;
        if (!instance) {
            instance = new ClassTree();
        }
        return instance;
    }

private:
    // Points to sentinel
    size_t tail_;
    container_type classList_;
};

/*
 * Used to refiste C++ classes for simple RTTI
 * RTTI used for safe object casting
 */
struct StaticClassRegistrator {
    StaticClassRegistrator(std::string_view inClassName,
                           std::string_view inSuperClassName) {
        Class =
            ClassTree::Instance()->RegisterClass(inClassName, inSuperClassName);
    }
    const ClassMeta* Class;
};