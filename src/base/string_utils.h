#pragma once
#include "common.h"

#include <istream>
#include <ostream>
#include <filesystem>


// Define a std::formatter overload for std::format using adl of to_string()
#define DEFINE_TOSTRING_FORMATTER(TYPE)                         \
    template <>                                                 \
    struct std::formatter<TYPE> : std::formatter<std::string> { \
        using Base = std::formatter<std::string>;               \
        using Base::parse;                                      \
        template <class T>                                      \
        auto format(const TYPE& value, T& ctx) const {          \
            return Base::format(to_string(value), ctx);         \
        }                                                       \
    };

// Define a std::formatter overload for std::format using cast to another type
#define DEFINE_CAST_FORMATTER(TYPE, TYPE2)                       \
    template <>                                                  \
    struct std::formatter<TYPE> : std::formatter<TYPE2> {        \
        using Base = std::formatter<TYPE2>;                      \
        using Base::parse;                                       \
        template <class T>                                       \
        auto format(const TYPE& value, T& ctx) const {           \
            return Base::format(static_cast<TYPE2>(value), ctx); \
        }                                                        \
    };

// Define a std::ostream overload using adl of to_string()
#define DEFINE_TOSTRING_OSTREAM(TYPE)                                          \
    inline std::ostream& operator<<(std::ostream& stream, const TYPE& value) { \
        stream << to_string(value);                                            \
        return stream;                                                         \
    }

constexpr uint64_t kStringPoolSize = 8192;

template<typename T>
concept StringData = requires(T a) {
	{ a.data() } -> std::convertible_to<const char*>;
};


template<>
struct std::formatter<std::filesystem::path>: std::formatter<std::string> {
    using Base = std::formatter<std::string>;

public:
    using Base::parse;

	template<class FmtContext>
	auto format(const std::filesystem::path& val, FmtContext& ctx) const {
		return Base::format(val.string(), ctx);
	}
};


/*
 * Helper to create string of serialized data
 */
class StringBuilder {
public:
    constexpr StringBuilder(std::string* inBuffer, uint32_t inIndentSize = 2)
        : buffer_(inBuffer), indent_(0), indentSize_(inIndentSize) {}

    template <typename... ArgTypes>
    constexpr StringBuilder& Line(
        const std::format_string<ArgTypes...> inFormat,
        ArgTypes&&... inArgs) {
        AppendIndent();
        buffer_->append(
            std::format(inFormat, std::forward<ArgTypes>(inArgs)...));
        EndLine();
        return *this;
    }

    constexpr StringBuilder& Line(std::string_view inStr) {
        AppendIndent();
        buffer_->append(inStr);
        EndLine();
        return *this;
    }

    constexpr StringBuilder& Line() {
        EndLine();
        return *this;
    }

    constexpr StringBuilder& SetIndent(uint32_t inIndent = 1) {
        indent_ = inIndent;
        return *this;
    }

    constexpr StringBuilder& PushIndent(uint32_t inIndent = 1) {
        indent_ += inIndent;
        return *this;
    }

    constexpr StringBuilder& PopIndent(uint32_t inIndent = 1) {
        if (indent_)
            indent_ -= inIndent;
        return *this;
    }

    constexpr StringBuilder& EndLine() {
        buffer_->append("\n");
        return *this;
    }

private:
    constexpr void AppendIndent() {
        if (!indent_)
            return;
        for (auto i = indent_ * indentSize_; i; --i) {
            buffer_->append(" ");
        }
    }

private:
    uint32_t indentSize_;
    uint32_t indent_;
    std::string* buffer_;
};

inline std::wstring ToWideString(const std::string& inStr) {
    auto out = std::wstring(inStr.size() + 1, L'\0');
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, out.data(), out.size(), inStr.c_str(),
               _TRUNCATE);
    return out;
}

inline std::string ToLower(const std::string& inString) {
    std::string str(inString);
    std::transform(std::begin(str), std::end(str), std::begin(str),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

inline bool WStringToString(std::wstring_view wstr, std::string& str) {
    size_t strLen;
    errno_t err;
    if ((err = wcstombs_s(&strLen, nullptr, 0, wstr.data(), _TRUNCATE)) != 0) {
        return false;
    }
    DASSERT(strLen > 0);
    // Remove \0
    str.clear();
    str.resize(strLen);
    // Remove \0
    str.pop_back();

    if ((err = wcstombs_s(nullptr, &str.front(), strLen, wstr.data(),
                          _TRUNCATE)) != 0) {
        return false;
    }
    return true;
}


/**
 * Interned string in a string pool
 * Stores indices into that pool
 * Case insensitive MyString == mystring
 * But case is preserved when transformed back to string
 */
class StringID {
public:	

	using index_type = int32_t;

	constexpr StringID();
	StringID(const char* inNewName);

	template<StringData T>
	StringID(const T& inStringData): StringID(inStringData.data()) {}

	constexpr StringID(const StringID& other) = default;
	constexpr StringID(StringID&& other) = default;

	constexpr StringID& operator=(const StringID& other) = default;
	constexpr StringID& operator=(StringID&& other) = default;

	const std::string&		String() const;

	bool					Empty() const { return m_CompareIndex == 0; }
	explicit				operator bool() const { return m_CompareIndex != 0; }

	void					Clear() { m_CompareIndex = 0; m_DisplayIndex = 0; }


	NODISCARD friend constexpr bool operator==(const StringID& left, const StringID& right) {
		return left._compareFast(right) == 0;
	}

	NODISCARD friend bool operator==(const StringID& left, const char* charString) {
		return left.String() == charString;
	}

	NODISCARD friend bool operator==(const StringID& left, const std::string& right) {
		return *left == right;
	}

	NODISCARD friend bool operator==(const std::string& left, const StringID& right) {
		return left == *right;
	}

	NODISCARD friend bool operator==(const char* left, const StringID& right) {
		return std::string_view(left) == *right;
	}

#if _HAS_CXX20

	NODISCARD friend auto operator<=>(const StringID& left, const StringID& right) {
		return left._compareFast(right) <=> 0;
	}

#else 	

	NODISCARD friend constexpr bool operator!=(const StringID& left, const StringID& right) {
		return left.m_CompareIndex != right.m_CompareIndex;
	}

	NODISCARD friend constexpr bool operator<(const StringID& left, const StringID& right) {
		return left.fastLess(right);
	}

	NODISCARD friend constexpr bool operator>(const StringID& left, const StringID& right) {
		return !left.fastLess(right);
	}

#endif

	/** Fast non-alphabetical order that is only stable during this process' lifetime. */
	constexpr bool			FastLess(const StringID& right) const;

	/** Slow alphabetical order that is stable / deterministic over process runs. */
	bool					LexicalLess(const StringID& right) const;

	// Returns c_str()
	const char*				operator*() const;

	// Serialization operator for std streams
	template <class _Elem, class _Traits>
	friend std::basic_ostream<_Elem, _Traits>& operator<<(std::basic_ostream<_Elem, _Traits>& _Ostr, const StringID& _NameID) {
		return _Ostr << std::quoted(*_NameID);
	}

	static void				test();

	// Fast less predicate
	struct FastLessPred {
		constexpr bool	operator()(const StringID& left, const StringID& right) const {
			return left.FastLess(right);
		}
	};

	// Lexical less predicate
	struct LexicalLessPred {
		constexpr bool operator()(const StringID& left, const StringID& right) const {
			return left.FastLess(right);
		}
	};

public:

	friend void operator<<(std::string& inString, const StringID& inNameID) {
		inString = inNameID.String();
	}

	friend void operator>>(const std::string& inString, StringID& inNameID) {
		inNameID = StringID(inString);
	}

private:

	// Interns this string array to the string pool
	// and return indices to the pool array
	void _intern(std::string_view inName);

	// Compares string values
	// Stable across runs
	// Slow
	//  1: this > right
	//  0: this == right
	// -1: this < right
	int _compareLexically(const StringID& right) const;

	constexpr int _compareFast(const StringID& right) const {
		return m_CompareIndex - right.m_CompareIndex;	
	}

private:
#ifndef NDEBUG
	const char* m_DebugString;
#endif
	// Index to lower case version of the string.
	// Used for comparisons
	index_type m_CompareIndex;
	// Index to initial case sensitive string
	// Used for string displaying
	index_type m_DisplayIndex;
};


template<>
struct std::formatter<StringID>: std::formatter<std::string_view> {
    using Super = formatter<std::string_view>;

public:
    using Super::parse;

	template<class FmtContext>
	auto format(const StringID& t, FmtContext& ctx) const {
		return Super::format(t.String(), ctx);
	}
};

/*====================================================================================*/
constexpr StringID::StringID()
	: m_CompareIndex(0)
	, m_DisplayIndex(0) 
#ifndef NDEBUG
	, m_DebugString("")
#endif
{}

inline StringID::StringID(const char* inNewName)
	: m_CompareIndex(0)
	, m_DisplayIndex(0) {
	std::string_view s(inNewName);
	if (!s.empty()) { _intern(s); }
#ifndef NDEBUG
	m_DebugString = String().c_str();
#endif
}

constexpr bool StringID::FastLess(const StringID& right) const {
	return this->m_CompareIndex < right.m_CompareIndex;
}

inline bool StringID::LexicalLess(const StringID& right) const {
	return _compareLexically(right) < 0;
}

NODISCARD inline std::string operator+(const std::string& left, const StringID& right) {
	return std::move(left + right.String());
}





