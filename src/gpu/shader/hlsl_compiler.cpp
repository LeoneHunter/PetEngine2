#include "hlsl_compiler.h"
#include <d3dcompiler.h>

namespace gpu {

// static
ShaderCompileResult HLSLShaderCompiler::Compile(const std::string& main,
                                                const std::string& code,
                                                ShaderType type,
                                                bool debugBuild) {
    std::string targetString;
    switch(type) {
        case ShaderType::Vertex: targetString = "vs_5_0"; break;
        case ShaderType::Pixel: targetString = "ps_5_0"; break;
        default: DASSERT_M(false, "Unknown shader type");
    }
    ShaderCompileResult result;

    UINT compileFlags = 0;
    if (debugBuild) {
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }
    RefCountedPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(code.c_str(), code.size(), "", nullptr, nullptr,
                            main.c_str(), targetString.c_str(), compileFlags, 0,
                            &result.blob, &errors);

    if(FAILED(hr)) {
        result.msg = std::string((char*)errors->GetBufferPointer(),
                                 errors->GetBufferSize());
        result.hasErrors = true;
    }
    return result;
}

} // namespace gpu