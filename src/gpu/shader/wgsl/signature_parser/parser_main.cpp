#include "parser.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#define VERIFY(COND, RET, ...)   \
    if (!(COND)) {               \
        PrintError(__VA_ARGS__); \
        return RET;              \
    }

template <class... Args>
void PrintInfo(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << "[info] > " << std::format(fmt, std::forward<Args&&>(args)...)
              << '\n';
}

template <class... Args>
void PrintError(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << "[error] > " << std::format(fmt, std::forward<Args&&>(args)...)
              << '\n';
}

// An empty line
constexpr std::string_view kDataSeparator = "\r\n\r\n";

std::vector<wgsl::BuiltinFuncSignature> signatures;

int main(int argv, char* argc[]) {
    VERIFY(argv == 2, -1, "expected a path to a txt file");
    const auto filename =
        (std::filesystem::path(argc[0]).parent_path() / argc[1]).string();
    VERIFY(std::filesystem::exists(filename), -1,
           "path '{}' is not a valid filename", filename);

    const std::string file = [&]() -> std::string {
        const auto fileSize = std::filesystem::file_size(filename);
        auto file = std::ifstream(filename, std::ios::binary);
        VERIFY(file.is_open(), "", "cannot open file '{}'", filename);
        std::string buf;
        buf.resize(fileSize);
        file.read(buf.data(), fileSize);
        return std::move(buf);
    }();
    VERIFY(!file.empty(), -1, "file '{}' is empty", filename);
    // Parse file
    size_t offset = 0;
    for (;;) {
        const size_t start = offset;
        const size_t end = file.find(kDataSeparator, start);

        if (end == file.npos || offset > file.size()) {
            break;
        }

        const size_t size = end - start;
        const auto block = std::string_view(&file[start], size);
        offset += size + kDataSeparator.size();

        // Parse block
        auto parser = wgsl::SignatureParser();
        const auto res = parser.Parse(block);
        VERIFY(res, -1, "parser error at [Ln {}, Col {}]: {}\n Sig: \n{}",
               res.error().loc.line, res.error().loc.col, res.error().msg,
               block);
        // An empty, probably a comment or empty line
        if(res.value().name.empty()) {
            continue;
        }
        PrintInfo("parsed signature: '{}'", res.value().name);
        signatures.push_back(res.value());
    }
    if(signatures.empty()) {
        PrintError("file {} does not contain valid signatures data", filename);
    } else {
        PrintInfo("successfully parsed {} signatures", signatures.size());
    }
    // std::string buf;
    // auto writer = wgsl::Writer(&buf);
    // wgsl::SignatureWriter::Write(res.value(), writer);
    return 0;
}

// For each param:
//   If param is a concrete type:
//     It should match the arg type exactly
//   Else a template param:
//     Try resolve the template param:
//       If already resolved;
//         If resolved try match the resolved type with the arg type
//           If not matched -> type error
//       Else:
//         Match the outer arg type param then recursively inner types
//         For each nested param in the arg type:
//           Get a template declaration for that name
//           Try match the arg type with the types in the declaration
//           If has a match:
//             Register that template param with that type
//               E.g T -> i32, [fn (T, T)-> T] -> [fn (i32, i32)-> i32]
//           Else: type error
