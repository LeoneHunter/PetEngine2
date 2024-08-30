#pragma once
#include "gpu/common.h"
#include "gpu/d3d12/device.h"
#include "base/util.h"

namespace gpu {

class Shader;

class HLSLShaderCompiler {
public:
    static ShaderCompileResult Compile(const std::string& main,
                                       const std::string& code,
                                       ShaderType type,
                                       bool debugBuild);
};


}  // namespace gpu
