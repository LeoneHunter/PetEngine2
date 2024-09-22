#pragma once
#include "shader_dsl.h"

// Shader builder DSL language api
// Each function call and object creation uses thread_local ShaderBuilder to
//   record that operation into a syntax tree
namespace gpu::shader_dsl {

inline ShaderDSLContext& Context() {
    return ShaderDSLContext::Current();
}

// Custom user definen identifier for functions and variables
class Identifier {
public:
    Identifier(std::string_view name) : str(name) {}
    std::string str;
};


/*============================================================================*/
// Types
/*============================================================================*/

class Float : public internal::TypeBase {
public:
    explicit Float(Address addr) : internal::TypeBase(addr) {}

    // lvalue
    Float(std::string_view name = "") {
        SetAddress(Context().DeclareVar(GetType()));
        Context().SetIdentifier(GetAddress(), name);
    }

    // rvalue
    Float(float literal) {
        SetAddress(Context().DeclareLiteral(DataType::Float, literal));
    }

    static DataType GetType() { return DataType::Float; }
};

class Float2 : public internal::TypeBase {
public:
    explicit Float2(Address addr) : internal::TypeBase(addr) {}

    Float2(std::string_view name = "") {
        SetAddress(Context().DeclareVar(GetType()));
        Context().SetIdentifier(GetAddress(), name);
    }

    Float2(Float x, Float y) {
        const Address addr = Context().DefineVar(
            DataType::Float2, x.GetAddress(), y.GetAddress());
        SetAddress(addr);
    }

    Float X() { return Float(Context().OpFieldAccess(GetAddress(), 0)); }

    Float Y() { return Float(Context().OpFieldAccess(GetAddress(), 1)); }

    static DataType GetType() { return DataType::Float2; }
};

class Float3 : public internal::TypeBase {
public:
    explicit Float3(Address addr) : internal::TypeBase(addr) {}

    Float3(std::string_view name = "") {
        SetAddress(Context().DeclareVar(GetType()));
        Context().SetIdentifier(GetAddress(), name);
    }

    Float3(Float x, Float y, Float z) {
        const Address addr = Context().DefineVar(
            DataType::Float3, x.GetAddress(), y.GetAddress(), z.GetAddress());
        SetAddress(addr);
    }

    Float3(Float2 xy, Float z) {
        const Address addr = Context().DefineVar(
            DataType::Float3, xy.GetAddress(), z.GetAddress());
        SetAddress(addr);
    }

    Float X() { return Float(Context().OpFieldAccess(GetAddress(), 0)); }

    Float Y() { return Float(Context().OpFieldAccess(GetAddress(), 1)); }

    Float Z() { return Float(Context().OpFieldAccess(GetAddress(), 2)); }

    static DataType GetType() { return DataType::Float3; }
};

class Float4 : public internal::TypeBase {
public:
    explicit Float4(Address addr) : internal::TypeBase(addr) {}

    Float4(std::string_view name = "") {
        SetAddress(Context().DeclareVar(GetType()));
        Context().SetIdentifier(GetAddress(), name);
    }

    Float4(Float x, Float y, Float z, Float w) {
        const Address addr =
            Context().DefineVar(DataType::Float4, x.GetAddress(),
                                y.GetAddress(), z.GetAddress(), w.GetAddress());
        SetAddress(addr);
    }

    Float4(Float2 xy, Float z, Float w) {
        const Address addr = Context().DefineVar(
            DataType::Float4, xy.GetAddress(), z.GetAddress(), w.GetAddress());
        SetAddress(addr);
    }

    Float4(Float3 xyz, Float w) {
        const Address addr = Context().DefineVar(
            DataType::Float4, xyz.GetAddress(), w.GetAddress());
        SetAddress(addr);
    }

    Float X() { return Float(Context().OpFieldAccess(GetAddress(), 0)); }

    Float Y() { return Float(Context().OpFieldAccess(GetAddress(), 1)); }

    Float Z() { return Float(Context().OpFieldAccess(GetAddress(), 2)); }

    Float W() { return Float(Context().OpFieldAccess(GetAddress(), 3)); }

    static DataType GetType() { return DataType::Float4; }
};

class Float4x4 : public internal::TypeBase {
public:
    explicit Float4x4(Address addr) : internal::TypeBase(addr) {}

    Float4x4(std::string_view name = "") {
        SetAddress(Context().DeclareVar(GetType()));
        Context().SetIdentifier(GetAddress(), name);
    }

    static DataType GetType() { return DataType::Float4x4; }
};

class Sampler : public internal::TypeBase {
public:
    explicit Sampler(Address addr) : internal::TypeBase(addr) {}

    Sampler(std::string_view name = "") {
        SetAddress(Context().DeclareVar(GetType()));
        Context().SetIdentifier(GetAddress(), name);
    }

    static DataType GetType() { return DataType::Sampler; }
};

class Texture2D : public internal::TypeBase {
public:
    explicit Texture2D(Address addr) : internal::TypeBase(addr) {}

    Texture2D(std::string_view name = "") {
        SetAddress(Context().DeclareVar(GetType()));
        Context().SetIdentifier(GetAddress(), name);
    }

    static DataType GetType() { return DataType::Texture2D; }
};


// Declare a function
class Function : public internal::TypeBase {
public:
    Function(std::string_view identifier = "") {
        SetAddress(Context().DeclareFunction());
        Context().SetIdentifier(GetAddress(), identifier);
    }
};

// End current function scope
inline void EndFunction() {
    Context().DeclareFunctionEnd();
}


/*============================================================================*/
// Operators
/*============================================================================*/

// Sets a custom identifier for a function, struct or variable
template <std::derived_from<internal::TypeBase> T>
T operator|(T var, const Identifier& name) {
    Context().SetIdentifier(var.GetAddress(), name.str);
    return var;
}

template <std::derived_from<internal::TypeBase> T>
T operator|(T var, BindIndex bindIdx) {
    Context().SetBindIndex(var.GetAddress(), bindIdx);
    return var;
}

template <std::derived_from<internal::TypeBase> T>
T operator|(T var, Semantic semantic) {
    Context().SetSemantic(var.GetAddress(), semantic);
    return var;
}

template <std::derived_from<internal::TypeBase> T>
T operator|(T var, Attribute attr) {
    Context().SetAttribute(var.GetAddress(), attr);
    return var;
}

template <std::derived_from<internal::TypeBase> T>
T operator*(T lhs, T rhs) {
    return T{Context().Expression(gpu::internal::OpCode::Mul,
                                  lhs.GetAddress(), rhs.GetAddress())};
}

template <std::derived_from<internal::TypeBase> T>
T operator*(T lhs, float scalar) {
    Address rhs = Context().DeclareLiteral(DataType::Float, scalar);
    return {Context().Expression(gpu::internal::OpCode::Mul,
                                 lhs.GetAddress(), rhs)};
}

inline Float4 operator*(Float4x4 mat, Float4 vec) {
    return Float4{Context().Expression(gpu::internal::OpCode::Mul,
                                       mat.GetAddress(), vec.GetAddress())};
}



/*============================================================================*/
// Internal Functions
/*============================================================================*/

inline Float4 SampleTexture(Texture2D texture, Sampler sampler, Float2 uv) {
    return Float4{Context().Expression(gpu::internal::OpCode::SampleTexture,
                                       texture.GetAddress(),
                                       sampler.GetAddress(), uv.GetAddress())};
}

// A shader parameter if defined in global scope
// A function parameter if defined in a function
template <std::derived_from<internal::TypeBase> T>
T Input(std::string_view identifier = "") {
    auto var = T(identifier) | Attribute::Input;
    return var;
}

// A shader return parameter if defined in global scope
// A function return parameter if defined in a function
template <std::derived_from<internal::TypeBase> T>
T Output(std::string_view identifier = "") {
    auto var = T(identifier) | Attribute::Output;
    return var;
}

// Define an input constant
template <std::derived_from<internal::TypeBase> T>
T Uniform(std::string_view identifier = "") {
    auto var = T(identifier) | Attribute::Uniform;
    return var;
}


}  // namespace gpu::shader_dsl