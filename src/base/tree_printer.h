#pragma once
#include "common.h"
#include "util.h"

#include <functional>

class JsonObjectBuilder;

template <class T>
concept Formattable = requires(T val) { std::format("{}", val); };

// Formats an integer as a 32 bit hex value (0x00000000)
constexpr auto kHexFormat32 = "{:#010x}";
// Formats an integer as a 64 bit hex value (0x00000000'00000000)
constexpr auto kHexFormat64 = "{:#018x}";

// Printer/Serializer interface for trees
// I.e. can be used to serialize or print a ui widget tree, or an ast tree
// Can be used to print to Json or XML
class TreePrinter {
public:
    virtual ~TreePrinter() = default;

    // Open object node '{'
    virtual TreePrinter& OpenObject(const std::string& name) = 0;
    // Close previously opened object node '}'
    virtual TreePrinter& CloseObject() = 0;

    // Open array node '['
    virtual TreePrinter& OpenArray(const std::string& name) = 0;
    // Close previously opened array node ']'
    virtual TreePrinter& CloseArray() = 0;

    // Push regular {name : value} node
    template <Formattable T>
    TreePrinter& Entry(const std::string& name, T&& value) {
        PushImpl(name, std::format("{}", value));
        return *this;
    }

    // Push array element [ value, ... ] node
    template <Formattable T>
    TreePrinter& Element(T&& value) {
        PushElementImpl(std::format("{}", value));
        return *this;
    }

    // RAII helper to close object node
    struct AutoObject {
        AutoObject(TreePrinter* p, const std::string& name) {
            p_ = p;
            p_->OpenObject(name);
        }
        ~AutoObject() {
            p_->CloseObject();
        }
        TreePrinter* p_ = nullptr;
    };
    
    // RAII helper to close array node
    struct AutoArray {
        AutoArray(TreePrinter* p, const std::string& name) {
            p_ = p;
            p_->OpenArray(name);
        }
        ~AutoArray () {
            p_->CloseArray();
        }
        TreePrinter* p_ = nullptr;
    };

protected:
    virtual TreePrinter& PushImpl(const std::string& name,
                                  const std::string& value) = 0;

    virtual TreePrinter& PushElementImpl(const std::string& value) = 0;
};


// Writes a json formatter tree
// TODO: Add folding when under some column length
class JsonTreePrinter final : public TreePrinter {
public:
    JsonTreePrinter();
    void ReserveBuffer(uint32_t size);

    TreePrinter& OpenObject(const std::string& name) override;
    TreePrinter& CloseObject() override;

    TreePrinter& OpenArray(const std::string& name) override;
    TreePrinter& CloseArray() override;

    TreePrinter& PushImpl(const std::string& name,
                          const std::string& value) override;

    TreePrinter& PushElementImpl(const std::string& value) override;

    std::string Finalize();

private:
    JsonTreePrinter& WriteIndent();

    template <class... T>
    JsonTreePrinter& Write(T&&... str) {
        (buffer_.append(str), ...);
        return *this;
    }

    JsonTreePrinter& NewLine();
    JsonTreePrinter& PushDepth();
    JsonTreePrinter& PopDepth();
    JsonTreePrinter& PopTrailComma();

private:
    std::string buffer_;
    uint32_t indentSize_ = 2;
    uint32_t depth_ = 0;
    uint32_t arrayDepth_ = 0;
};


/*
 * Similar to TreePrinter but stores object internally
 * Simple write only archive class
 * Used for building a tree of objects with string properties
 */
class PropertyArchive {
public:
    struct Object;

    using ObjectID = uintptr_t;

    using Visitor = std::function<bool(Object&)>;

    struct Property {

        Property(std::string_view name, const std::string& value)
            : Name(name), Value(value) {}

        std::string_view Name;
        std::string Value;
    };

    struct Object {

        Object(const std::string& className, ObjectID objectID, Object* parent)
            : debugName_(className), objectID_(objectID), parent_(parent) {}

        void PushProperty(std::string_view name, const std::string& value) {
            properties_.emplace_back(name, value);
        }

        Object* EmplaceChild(const std::string& className, ObjectID objectID) {
            return &*children_.emplace_back(
                new Object(className, objectID, this));
        }

        bool Visit(const Visitor& visitor) {
            auto shouldContinue = visitor(*this);
            if (!shouldContinue)
                return false;

            for (auto& child : children_) {
                shouldContinue = child->Visit(visitor);
                if (!shouldContinue)
                    return false;
            }
            return true;
        }

        ObjectID objectID_ = 0;
        std::string debugName_;
        std::list<Property> properties_;
        Object* parent_ = nullptr;
        std::list<std::unique_ptr<Object>> children_;
    };

public:
    // Visit objects recursively in depth first
    // Stops iteration if visitor returns false
    void VisitRecursively(const Visitor& visitor) {
        if (rootObjects_.empty())
            return;

        for (auto& child : rootObjects_) {
            const auto shouldContinue = child->Visit(visitor);
            if (!shouldContinue)
                return;
        }
    }

    template <class T>
        requires(std::is_pointer_v<T> || std::is_integral_v<T>)
    void PushObject(const std::string& debugName, T object, T parent) {
        const auto objectID = (ObjectID)object;
        const auto parentID = (ObjectID)parent;
        if (!parent || !cursorObject_) {
            cursorObject_ = &*rootObjects_.emplace_back(
                new Object(debugName, objectID, nullptr));
            return;
        }
        if (cursorObject_->objectID_ != parentID) {
            for (Object* ancestor = cursorObject_->parent_; ancestor;
                 ancestor = ancestor->parent_) {
                if (ancestor->objectID_ == parentID) {
                    cursorObject_ = ancestor;
                }
            }
        }
        DASSERT(cursorObject_);
        cursorObject_ = cursorObject_->EmplaceChild(debugName, objectID);
    }

    // Push formatter property string
    template <class Vec2>
        requires(requires(Vec2 v) {
            v.x;
            v.y;
        })
    void PushProperty(std::string_view name, const Vec2& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(
            name, std::format("{:.0f}:{:.0f}", property.x, property.y));
    }

    void PushProperty(std::string_view name, const std::string& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, std::format("{}", property));
    }

    void PushStringProperty(std::string_view name,
                            const std::string& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, std::format("\"{}\"", property));
    }

    template <typename Enum>
        requires std::is_enum_v<Enum>
    void PushProperty(std::string_view name, Enum property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, ToString(property));
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    void PushProperty(std::string_view name, T property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());

        if constexpr (std::is_integral_v<T>) {
            cursorObject_->PushProperty(name, std::format("{}", property));
        } else if constexpr (std::is_same_v<T, bool>) {
            cursorObject_->PushProperty(name, property ? "True" : "False");
        } else {
            cursorObject_->PushProperty(name, std::format("{:.2f}", property));
        }
    }

public:
    std::list<std::unique_ptr<Object>> rootObjects_;
    Object* cursorObject_ = nullptr;
};