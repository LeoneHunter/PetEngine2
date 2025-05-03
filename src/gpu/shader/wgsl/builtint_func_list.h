
// List of supported builtin function signatures
// Grammar:
// - 'concept' declares a concept
// - '--' a comment
// - '@' an attribute
// - '[]' a template list
// - 'fn' declares a function (optional)
// - '->' declares a return (optional)
// - 'T : ...' declares a templated type with constraints
// Only one line declarations is supported
// NOTE: Unsupported features:
// - Abstract types
// - Const functions
constexpr auto kBuiltinFuncList = R"(
-- Concepts (defined in code, here for reference)
    concept float = f32
    concept signed = f32 | i32 
    concept unsigned = u32
    concept integer = i32 | u32
    concept numeric = float | integer
    concept scalar = numeric | bool

    concept constructible
    concept concrete_constructible

-- Primitive constructors
    [T: concrete_constructible] @const @must_use fn T() -> T
    [T: concrete_constructible] @const @must_use fn array<T, N>(e1 : T, ..., eN : T) -> array<T, N>
    [T: constructible] @const @must_use fn array(e1 : T, ..., eN : T) -> array<T, N>

    [T: scalar] @const @must_use fn bool(e : T) -> bool
    [T: scalar] @const @must_use fn f32(e : T) -> f32
    [T: scalar] @const @must_use fn i32(e : T) -> i32

-- 2x2 matrices
    [T: f32, S: float] @const @must_use fn mat2x2<T>(e : mat2x2<S>) -> mat2x2<T>

    [T: float] @const @must_use fn mat2x2<T>(v1 : vec2<T>, v2 : vec2<T>) -> mat2x2<T>
    [T: float] @const @must_use fn mat2x2(v1 : vec2<T>, v2 : vec2<T>) -> mat2x2<T>

    [T: float] @const @must_use fn mat2x2<T>(e1 : T, e2 : T, e3 : T, e4 : T) -> mat2x2<T>
    [T: float] @const @must_use fn mat2x2(e1 : T, e2 : T, e3 : T, e4 : T) -> mat2x2<T>

-- 2x3 matrices
    [T: f32, S: float] @const @must_use fn mat2x3<T>(e : mat2x3<S>) -> mat2x3<T>
    [T: f32, S: float] @const @must_use fn mat2x3(e : mat2x3<S>) -> mat2x3<S>

    [T: float] @const @must_use fn mat2x3<T>(v1 : vec3<T>, v2 : vec3<T>) -> mat2x3<T>
    [T: float] @const @must_use fn mat2x3(v1 : vec3<T>, v2 : vec3<T>) -> mat2x3<T>

    [T: float] @const @must_use fn mat2x3<T>(e1 : T, ..., e6 : T) -> mat2x3<T>
    [T: float] @const @must_use fn mat2x3(e1 : T, ..., e6 : T) -> mat2x3<T>

-- 2x4 matrices
    [T: f32, S: float] @const @must_use fn mat2x4<T>(e : mat2x4<S>) -> mat2x4<T>
    [T: f32, S: float] @const @must_use fn mat2x4(e : mat2x4<S>) -> mat2x4<S>

    [T: float] @const @must_use fn mat2x4<T>(v1 : vec4<T>, v2 : vec4<T>) -> mat2x4<T>
    [T: float] @const @must_use fn mat2x4(v1 : vec4<T>, v2 : vec4<T>) -> mat2x4<T>

    [T: float] @const @must_use fn mat2x4<T>(e1 : T, ..., e8 : T) -> mat2x4<T>
    [T: float] @const @must_use fn mat2x4(e1 : T, ..., e8 : T) -> mat2x4<T>

-- 3x2 matrices
    [T: f32, S: float] @const @must_use fn mat3x2<T>(e : mat3x2<S>) -> mat3x2<T>
    [T: f32, S: float] @const @must_use fn mat3x2(e : mat3x2<S>) -> mat3x2<S>

    [T: float] @const @must_use fn mat3x2<T>(v1 : vec2<T>, v2 : vec2<T>, v3 : vec2<T>) -> mat3x2<T>
    [T: float] @const @must_use fn mat3x2(v1 : vec2<T>, v2 : vec2<T>, v3 : vec2<T>) -> mat3x2<T>

    [T: float] @const @must_use fn mat3x2<T>(e1 : T, ..., e6 : T) -> mat3x2<T>
    [T: float] @const @must_use fn mat3x2(e1 : T, ..., e6 : T) -> mat3x2<T>

-- Numeric built-in
    [T: numeric | vec<numeric>] @const @must_use fn abs(e: T ) -> T

    [T: float | vec<float>] @const @must_use fn acos(e: T ) -> T
    [T: float | vec<float>] @const @must_use fn acosh(e: T ) -> T
    [T: float | vec<float>] @const @must_use fn asin(e: T ) -> T
    [T: float | vec<float>] @const @must_use fn asinh(e: T ) -> T
    [T: float | vec<float>] @const @must_use fn atan(e: T ) -> T
    [T: float | vec<float>] @const @must_use fn atanh(e: T ) -> T
    [T: float | vec<float>] @const @must_use fn atan2(y: T, x: T) -> T

    [T: float | vec<float>] @const @must_use fn ceil(e: T) -> T
    [T: numeric | vec<numeric>] @const @must_use fn clamp(e: T, low: T, high: T) -> T

    [T: float | vec<float>] @const @must_use fn cos(e: T) -> T
    [T: float | vec<float>] @const @must_use fn cosh(a: T) -> T
    
    [T: i32 | u32 | vec<i32> | vec<u32>] @const @must_use fn countLeadingZeros(e: T) -> T
    [T: i32 | u32 | vec<i32> | vec<u32>] @const @must_use fn countOneBits(e: T) -> T
    [T: i32 | u32 | vec<i32> | vec<u32>] @const @must_use fn countTrailingZeros(e: T) -> T

    [T: float] @const @must_use fn cross(a: vec3<T>, b: vec3<T>) -> vec3<T>
    [T: float | vec<float>] @const @must_use fn degrees(e1: T) -> T

    [T: float]@const @must_use fn determinant(e: matCxC<T>) -> T
    [S: float, T: S | vec<S>] @const @must_use fn distance(e1: T, e2: T) -> S
    [S: float, T: S | vec<S>] @const @must_use fn length(e: T) -> S

    [T: numeric] @const @must_use fn dot(e1: vecN<T>, e2: vecN<T>) -> T

-- Texture access
    @must_use fn textureSample(t: texture_1d<f32>, s: sampler, coords: f32) -> vec4<f32>
    @must_use fn textureSample(t: texture_2d<f32>, s: sampler, coords: vec2<f32>) -> vec4<f32>
)";

