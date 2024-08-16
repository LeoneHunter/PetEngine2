#pragma once
#include "gpu/common.h"
#include "gpu/d3d12/device_dx12.h"
#include "base/util.h"

namespace gpu {

class Shader;

struct ShaderCompileResult {
    explicit operator bool() const { return !hasErrors; }

    RefCountedPtr<ID3DBlob> blob;
    bool hasErrors = false;
    std::string msg;
};

class HLSLShaderCompiler {
public:
    static ShaderCompileResult Compile(const std::string& main,
                                       const std::string& code,
                                       ShaderType type,
                                       bool debugBuild);
};


}  // namespace gpu
