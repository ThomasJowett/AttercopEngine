#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class Uuid
{
public:
	// A new random ID, never nil
	Uuid();
	Uuid(uint64_t lo, uint64_t hi) :m_Lo(lo), m_Hi(hi) {}
	Uuid(const Uuid& rhs) : m_Lo(rhs.m_Lo), m_Hi(rhs.m_Hi) {}

	// The all-zero ID, meaning "none"
	static Uuid Nil() { return Uuid(0, 0); }
	bool IsNil() const { return m_Lo == 0 && m_Hi == 0; }

	// Reads "low-high" in decimal, as written by to_string; nothing for any other text
	static std::optional<Uuid> Parse(std::string_view text);

	std::string to_string() const
	{
		return std::to_string(m_Lo) + "-" + std::to_string(m_Hi);
	}

	operator std::string() const { return to_string(); }
	bool operator==(const Uuid& rhs) const { return (m_Lo == rhs.m_Lo) && (m_Hi == rhs.m_Hi); }
	bool operator!=(const Uuid& rhs) const { return (m_Lo != rhs.m_Lo) || (m_Hi != rhs.m_Hi); }
	bool operator<(const Uuid& rhs) const
	{
		if (m_Hi == rhs.m_Hi)
			return m_Lo < rhs.m_Lo;
		else
			return m_Hi < rhs.m_Hi;
	}
private:
	uint64_t m_Lo;
	uint64_t m_Hi;
};

namespace std 
{
	template<>
	struct hash<Uuid>
	{
		std::size_t operator()(const Uuid& uuid) const
		{
			// A perfect hash is not required
			const int round = sizeof(Uuid) / sizeof(size_t);
			const size_t* ptr = (const size_t*)&uuid;
			size_t result = ptr[0];
			for (int n = 1; n < round; ++n)
				result ^= ptr[n];
			return result;
		}
	};
}
