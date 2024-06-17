#include "string_utils.h"
#include "common.h"
#include "util.h"
#include "threading.h"

#include <map>

using int32 = int32_t;

#define check(a) DASSERT(a)

class StringPool {
public:

	// Reserve 0 index
	StringPool() { 
		m_StringPool.reserve(kStringPoolSize); 
		m_StringPool.emplace_back(); 
	}

	static StringPool& instance() {
		static StringPool* _instance; 
		if (!_instance) {
			_instance = new StringPool;
		}
		return *_instance; 
	}

	/**
	 * Adds new string to pool or returns existing one
	 */
	void intern(std::string_view inString, int32& outCompareIndex, int32& outDisplayIndex) {
		check(!inString.empty());

		std::string compareStr(inString);

		std::transform(
			std::begin(compareStr),
			std::end(compareStr),
			std::begin(compareStr),
			[](unsigned char c) {
				return std::tolower(c);
			}
		);
		_intern(compareStr, outCompareIndex);

		if (inString == compareStr) {
			outDisplayIndex = outCompareIndex;			
		} else {
			_intern(std::string(inString), outDisplayIndex);
		}
	}

	/**
	 * Returns intern or NULL for specified index
	 */
	const std::string& string(int32 inIndex) {
		DASSERT(inIndex < m_StringPool.size());
		return m_StringPool[inIndex];
	}

private:

	void _intern(const std::string& inString, int32& outIndex) {

		auto& str = inString;
		Spinlock::ScopedLock lock(m_StringPoolMutex);
		auto it = m_InternedStrings.find(str);

		if (it != m_InternedStrings.end()) {
			outIndex = it->second;

		} else {			
			auto newIndex = (int32)m_StringPool.size();
			m_StringPool.emplace_back(str);
			m_InternedStrings.emplace(str, newIndex);
			outIndex = newIndex;
		}
	}

private:
	Spinlock						m_StringPoolMutex;
	std::vector<std::string>		m_StringPool;

	// TODO try using hash map
	std::map<std::string, int32>	m_InternedStrings;
};

void StringID::_intern(std::string_view inName) {
	StringPool::instance().intern(inName, m_CompareIndex, m_DisplayIndex);
}

int StringID::_compareLexically(const StringID& right) const {
	if (m_CompareIndex == right.m_CompareIndex) { return 0; }
	const char* rightStr = StringPool::instance().string(m_CompareIndex).c_str();
	const char* thisStr = StringPool::instance().string(m_DisplayIndex).c_str();
	auto len = std::min(std::strlen(thisStr), std::strlen(rightStr));
	if (int32 diff = std::strncmp(thisStr, rightStr, len)) {
		return diff;
	}
	return (int)std::strlen(thisStr) - (int)std::strlen(rightStr);
}

const std::string& StringID::String() const {
	return StringPool::instance().string(m_DisplayIndex);
}

const char* StringID::operator*() const {
	return StringPool::instance().string(m_DisplayIndex).c_str();
}

void StringID::test() {

	StringID sid1("New Material");
	StringID sid2("New Material");
	StringID sid3("new material");
	StringID sid4("NeW MaTerial");

	check(sid1 == sid2);
	check(sid1 == sid3);
	check(sid2 == sid4);

	check(sid1.String() == "New Material");
	check(sid2.String() == "New Material");
	check(sid3.String() == "new material");
	check(sid4.String() == "NeW MaTerial");

	StringID fullObjectPath = "Material [/Resources/wall_material_1.gasset]wall_material_1";
	StringID objectPath = "[/Resources/wall_material_1.gasset]wall_material_1";	

	std::map<StringID, int, StringID::FastLessPred> testMap;
	testMap.emplace(sid1, 4);
	testMap.emplace(fullObjectPath, 4);
	testMap.emplace(objectPath, 1);

	check(1);
}
