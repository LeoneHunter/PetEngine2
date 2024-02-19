#pragma once
#include "Runtime/Core/Core.h"

#include <istream>
#include <ostream>

#define NAME_ID_DEBUG

inline constexpr u64 g_string_pool_size = 8192;

template<typename T>
concept StringData = requires(T a) {
	{ a.data() } -> std::convertible_to<const char*>;
};

/**
 * Interned string in a string pool
 * Stores indices into that pool
 * Case insensitive MyString == mystring
 * But case is preserved when transformed back to string
 */
class StringID {
public:	

	using index_type = s32;

	constexpr StringID();
	StringID(const char* inNewName);
	//StringID(const std::string& inNewName): StringID(inNewName.c_str()) {}

	template<StringData T>
	StringID(const T& inStringData): StringID(inStringData.data()) {}

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
	// Index to lower case version of the string.
	// Used for comparisons
	index_type m_CompareIndex;
	// Index to initial case sensitive string
	// Used for string displaying
	index_type m_DisplayIndex;

#ifdef NAME_ID_DEBUG
	const char* m_DebugString;
#endif
};


template<>
struct std::formatter<StringID, char> {

	template<class ParseContext>
	constexpr ParseContext::iterator parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template<class FmtContext>
	FmtContext::iterator format(const StringID& t, FmtContext& ctx) const {
		return std::ranges::copy(std::move(t.String()), ctx.out()).out;
	}
};



/*====================================================================================*/
constexpr inline StringID::StringID()
	: m_CompareIndex(0)
	, m_DisplayIndex(0) 
	, m_DebugString("")
{}

inline StringID::StringID(const char* inNewName)
	: m_CompareIndex(0)
	, m_DisplayIndex(0)
#ifdef NAME_ID_DEBUG
	, m_DebugString()
#endif
{
	std::string_view s(inNewName);
	if (!s.empty()) { _intern(s); }
	m_DebugString = String().c_str();
}

constexpr inline bool StringID::FastLess(const StringID& right) const {
	return this->m_CompareIndex < right.m_CompareIndex;
}

inline bool StringID::LexicalLess(const StringID& right) const {
	return _compareLexically(right) < 0;
}

NODISCARD inline std::string operator+(const std::string& left, const StringID& right) {
	return std::move(left + right.String());
}