#pragma once
#include "base/common.h"
#include "string_utils.h"
#include <span>

class CommandLine {
public:

    static void Set(int argc, char* argv[]) {
        DASSERT(line_.empty());
        line_ = {argv, argv + argc};
        if(line_.size() > 1) {
            args_ = std::span<std::string>(line_.begin() + 1, line_.end());
        }
    }

    static void Set(int argc, wchar_t* argv[]) {
        DASSERT(line_.empty());
        for(int i = 0; i < argc; ++i) {
            std::wstring_view arg(argv[i]);
            std::string& elem = line_.emplace_back();
            WStringToString(arg, elem);
        }
        if(line_.size() > 1) {
            args_ = std::span<std::string>(line_.begin() + 1, line_.end());
        }
    }

    static void Set(std::convertible_to<std::string> auto ...args) {
        DASSERT(line_.empty());
        (line_.push_back(args), ...);
        if(line_.size() > 1) {
            args_ = std::span<std::string>(line_.begin() + 1, line_.end());
        }
    }

    static std::filesystem::path GetWorkingDir() {
        DASSERT(!line_.empty());
        return std::filesystem::path(line_.front()).parent_path();
    }

    static size_t ArgSize() { return args_.size(); }

    // Try to parse the arguments array
    // E.g. pares the line: program -arg_name arg_value
    template <class T>
        requires std::integral<T> || std::floating_point<T> ||
                 std::same_as<T, std::string>
    static std::optional<T> ParseArg(std::string_view argName) {
        std::string value;
        if(args_.empty() || args_.size() == 1) {
            return {};
        }
        for(auto it = args_.begin(); it != args_.end(); ++it) {
            if(*it != argName || (it + 1) == args_.end()) {
                continue;
            }
            value = *(it + 1);
            break;
        }
        if(value.empty()) {
            return {};
        }
        if constexpr(std::same_as<T, std::string>) {
            return value;
        } else {
            T parsedValue;
            const std::from_chars_result result = std::from_chars(
                value.data(), value.data() + value.size(), parsedValue, 10);
            if(result.ec != std::errc()) {
                return {};
            }
            return parsedValue;
        }
    }

    template <class T>
        requires std::integral<T> || std::floating_point<T> ||
                 std::same_as<T, std::string>
    static T ParseArgOr(std::string_view argName, const T& defaultValue) {
        std::optional<T> result = ParseArg<T>(argName);
        if(!result) {
            return defaultValue;
        }
        return result.value();
    }

    static std::string GetLine() {
        std::string out;
        for(const std::string& elem: line_) {
            out += elem + ' ';
        }
        return out;
    }

private:
    static inline std::vector<std::string> line_;
    static inline std::span<std::string> args_;
};