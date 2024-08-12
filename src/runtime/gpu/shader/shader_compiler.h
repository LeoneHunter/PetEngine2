#pragma once
#include "device.h"
#include "runtime/util.h"

#include <variant>
#include <map>
#include <deque>

namespace gpu {

class Shader;

struct ShaderCompileResult {
    explicit operator bool() const { return !hasErrors; }

    RefCountedPtr<Shader> shader;
    bool hasErrors = false;
    std::string msg;
};

class ShaderCompiler {
public:
    ShaderCompileResult CompileD3D12(const std::string& vertexCode,
                                     const std::string& pixelCode,
                                     const std::string& mainName,
                                     const std::string& version);
};


}  // namespace gpu
