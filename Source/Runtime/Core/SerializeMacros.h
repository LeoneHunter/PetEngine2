#pragma once
#include "BaseTypes.h"

#define DEFINE_ENUMFLAGS_ENTRY(type, entry) if(inEnum & type::entry) { out.empty() ? out.append(#entry) : out.append(" | "#entry); }

#define DEFINE_ENUMFLAGS_TOSTRING_2(type, deflt, enum1)\
	constexpr std::string ToString(type inEnum) {\
		std::string out;\
		DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
		if(out.empty()) out = #deflt;\
		return out;\
	}

#define DEFINE_ENUMFLAGS_TOSTRING_3(type, deflt, enum1, enum2)\
	constexpr std::string ToString(type inEnum) {\
		std::string out;\
		DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
		if(out.empty()) out = #deflt;\
		return out;\
	}

#define DEFINE_ENUMFLAGS_TOSTRING_4(type, deflt, enum1, enum2, enum3)\
	constexpr std::string ToString(type inEnum) {\
		std::string out;\
		DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum3)\
		if(out.empty()) out = #deflt;\
		return out;\
	}

#define DEFINE_ENUMFLAGS_TOSTRING_5(type, deflt, enum1, enum2, enum3, enum4)\
	constexpr std::string ToString(type inEnum) {\
		std::string out;\
		DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum3)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum4)\
		if(out.empty()) out = #deflt;\
		return out;\
	}

#define DEFINE_ENUMFLAGS_TOSTRING_6(type, deflt, enum1, enum2, enum3, enum4, enum5)\
	constexpr std::string ToString(type inEnum) {\
		std::string out;\
		DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum3)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum4)\
		DEFINE_ENUMFLAGS_ENTRY(type, enum5)\
		if(out.empty()) out = #deflt;\
		return out;\
	}










#define DEFINE_ENUM_TOSTRING_1(type, e1)\
	constexpr std::string ToString(type inEnum) {\
		if(inEnum == e1) { return #e1; }\
		return {};\
	}

#define DEFINE_ENUM_TOSTRING_2(type, e1, e2)\
	constexpr std::string ToString(type inEnum) {\
		if(inEnum == type::e1) { return #e1; }\
		else if(inEnum == type::e2) { return #e2; }\
		return {};\
	}

#define DEFINE_ENUM_TOSTRING_3(type, e1, e2, e3)\
	constexpr std::string ToString(type inEnum) {\
		if(inEnum == type::e1) { return #e1; }\
		else if(inEnum == type::e2) { return #e2; }\
		else if(inEnum == type::e3) { return #e3; }\
		return {};\
	}

#define DEFINE_ENUM_TOSTRING_4(type, e1, e2, e3, e4)\
	constexpr std::string ToString(type inEnum) {\
		if(inEnum == type::e1) { return #e1; }\
		else if(inEnum == type::e2) { return #e2; }\
		else if(inEnum == type::e3) { return #e3; }\
		else if(inEnum == type::e4) { return #e4; }\
		return {};\
	}

#define DEFINE_ENUM_TOSTRING_5(type, e1, e2, e3, e4, e5)\
	constexpr std::string ToString(type inEnum) {\
		if(inEnum == type::e1) { return #e1; }\
		else if(inEnum == type::e2) { return #e2; }\
		else if(inEnum == type::e3) { return #e3; }\
		else if(inEnum == type::e4) { return #e4; }\
		else if(inEnum == type::e5) { return #e5; }\
		return {};\
	}
