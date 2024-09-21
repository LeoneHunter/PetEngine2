#pragma once
#include "ast_attribute.h"
#include "ast_node.h"
#include "ast_variable.h"
#include "program_alloc.h"

namespace wgsl {

class FunctionScope;

namespace ast {

class Parameter;
class Statement;
class ScopedStatement;

using ParameterList = ProgramList<const Parameter*>;

// A Function with a scope
class Function : public Symbol {
public:
    const std::string_view name;
    const AttributeList attributes;
    const ParameterList parameters;
    const Type* retType;
    const AttributeList retAttributes;
    ScopedStatement* body;

    constexpr static inline auto kStaticType = NodeType::Function;

    Function(SourceLoc loc,
             ScopedStatement* scope,
             std::string_view name,
             AttributeList&& attributes,
             ParameterList&& params,
             const ast::Type* retType,
             AttributeList&& retAttributes)
        : Symbol(loc, kStaticType)
        , name(name)
        , body(scope)
        , attributes(attributes)
        , parameters(params)
        , retType(retType)
        , retAttributes(retAttributes) {}
};

// Function parameters
class Parameter final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::Parameter;

    Parameter(SourceLoc loc,
              std::string_view ident,
              const Type* type,
              AttributeList&& attributes)
        : Variable(loc,
                   kStaticType,
                   ident,
                   type,
                   std::move(attributes),
                   nullptr) {}
};



#define BUILTIN_CONSTRUCTOR_LIST(V) \
    V(bool)                         \
    V(i32)                          \
    V(u32)                          \
    V(f32)                          \
    V(f16)                          \
    V(vec2)                         \
    V(vec3)                         \
    V(vec4)                         \
    V(mat2x2)                       \
    V(mat2x3)                       \
    V(mat2x4)                       \
    V(mat3x2)                       \
    V(mat3x3)                       \
    V(mat3x4)                       \
    V(mat4x2)                       \
    V(mat4x3)                       \
    V(mat4x4)                       \
    V(array)



#define BUILTIN_FUNC_LIST(V)        \
    V(abs)                          \
    V(acos)                         \
    V(acosh)                        \
    V(all)                          \
    V(any)                          \
    V(arrayLength)                  \
    V(asin)                         \
    V(asinh)                        \
    V(atan)                         \
    V(atan2)                        \
    V(atanh)                        \
    V(ceil)                         \
    V(clamp)                        \
    V(cos)                          \
    V(cosh)                         \
    V(countLeadingZeros)            \
    V(countOneBits)                 \
    V(countTrailingZeros)           \
    V(cross)                        \
    V(degrees)                      \
    V(determinant)                  \
    V(distance)                     \
    V(dot)                          \
    V(dot4I8Packed)                 \
    V(dot4U8Packed)                 \
    V(dpdx)                         \
    V(dpdxCoarse)                   \
    V(dpdxFine)                     \
    V(dpdy)                         \
    V(dpdyCoarse)                   \
    V(dpdyFine)                     \
    V(exp)                          \
    V(exp2)                         \
    V(extractBits)                  \
    V(faceForward)                  \
    V(firstLeadingBit)              \
    V(firstTrailingBit)             \
    V(floor)                        \
    V(fma)                          \
    V(fract)                        \
    V(frexp)                        \
    V(fwidth)                       \
    V(fwidthCoarse)                 \
    V(fwidthFine)                   \
    V(insertBits)                   \
    V(inverseSqrt)                  \
    V(ldexp)                        \
    V(length)                       \
    V(log)                          \
    V(log2)                         \
    V(max)                          \
    V(min)                          \
    V(mix)                          \
    V(modf)                         \
    V(normalize)                    \
    V(pack2x16float)                \
    V(pack2x16snorm)                \
    V(pack2x16unorm)                \
    V(pack4x8snorm)                 \
    V(pack4x8unorm)                 \
    V(pack4xI8)                     \
    V(pack4xU8)                     \
    V(pack4xI8Clamp)                \
    V(pack4xU8Clamp)                \
    V(pow)                          \
    V(quantizeToF16)                \
    V(radians)                      \
    V(reflect)                      \
    V(refract)                      \
    V(reverseBits)                  \
    V(round)                        \
    V(saturate)                     \
    V(select)                       \
    V(sign)                         \
    V(sin)                          \
    V(sinh)                         \
    V(smoothstep)                   \
    V(sqrt)                         \
    V(step)                         \
    V(storageBarrier)               \
    V(tan)                          \
    V(tanh)                         \
    V(transpose)                    \
    V(trunc)                        \
    V(unpack2x16float)              \
    V(unpack2x16snorm)              \
    V(unpack2x16unorm)              \
    V(unpack4x8snorm)               \
    V(unpack4x8unorm)               \
    V(unpack4xI8)                   \
    V(unpack4xU8)                   \
    V(workgroupBarrier)             \
    V(textureBarrier)               \
    V(textureDimensions)            \
    V(textureGather)                \
    V(textureGatherCompare)         \
    V(textureNumLayers)             \
    V(textureNumLevels)             \
    V(textureNumSamples)            \
    V(textureSample)                \
    V(textureSampleBias)            \
    V(textureSampleCompare)         \
    V(textureSampleCompareLevel)    \
    V(textureSampleGrad)            \
    V(textureSampleLevel)           \
    V(textureSampleBaseClampToEdge) \
    V(textureStore)                 \
    V(textureLoad)                  \
    V(atomicLoad)                   \
    V(atomicStore)                  \
    V(atomicAdd)                    \
    V(atomicSub)                    \
    V(atomicMax)                    \
    V(atomicMin)                    \
    V(atomicAnd)                    \
    V(atomicOr)                     \
    V(atomicXor)                    \
    V(atomicExchange)               \
    V(atomicCompareExchangeWeak)    \
    V(subgroupBallot)               \
    V(subgroupBroadcast)

enum class BuiltinFn : uint8_t {
#define ENUM(NAME) __##NAME,
    BUILTIN_FUNC_LIST(ENUM) BUILTIN_CONSTRUCTOR_LIST(ENUM)
#undef ENUM
};

constexpr std::string BuiltinFnNameFromString(std::string_view name) {
    return std::string("__").append(name);
}

constexpr std::optional<BuiltinFn> BuiltinFnFromString(std::string_view str) {
#define STR(S) #S
#define IF_ELSE(NAME)               \
    if (str == STR(__##NAME))       \
        return BuiltinFn::__##NAME; \
    else
    BUILTIN_FUNC_LIST(IF_ELSE)
    // BUILTIN_CONSTRUCTOR_LIST(IF_ELSE)
    return std::nullopt;
#undef IF_ELSE
#undef STR
}

constexpr std::string_view to_string(BuiltinFn fn) {
#define STR(S) #S
#define CASE(NAME) \
    case BuiltinFn::__##NAME: return STR(__##NAME);
    switch (fn) {
        BUILTIN_FUNC_LIST(CASE)
        // BUILTIN_CONSTRUCTOR_LIST(CASE)
        default: return "";
    }
#undef CASE
#undef STR
}


// Builtint functions: interpolate, log, etc.
class BuiltinFunction final : public Symbol {
public:
    constexpr static inline auto kStaticType = NodeType::BuiltinFunction;
    const BuiltinFn type;

    BuiltinFunction(std::string_view ident, BuiltinFn type)
        : Symbol(kStaticType), type(type) {}
};



}  // namespace ast
}  // namespace wgsl