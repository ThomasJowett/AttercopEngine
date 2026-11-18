#include "UUID.h"

#include <limits>
#include <random>

namespace
{
	// Separate from Random, so seeding gameplay randomness never repeats IDs
	std::mt19937_64& Generator()
	{
		thread_local std::mt19937_64 s_Generator = []
			{
				std::random_device device;
				std::seed_seq seed{ device(), device(), device(), device(), device(), device(), device(), device() };
				return std::mt19937_64(seed);
			}();
		return s_Generator;
	}

	std::optional<uint64_t> ParseNumber(std::string_view text)
	{
		constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
		if (text.empty())
			return std::nullopt;

		uint64_t value = 0;
		for (char c : text)
		{
			if (c < '0' || c > '9')
				return std::nullopt;
			const uint64_t digit = uint64_t(c - '0');
			if (value > max / 10 || (value == max / 10 && digit > max % 10))
				return std::nullopt;
			value = value * 10 + digit;
		}
		return value;
	}
}

Uuid::Uuid()
{
	do
	{
		m_Lo = Generator()();
		m_Hi = Generator()();
	} while (IsNil());
}

std::optional<Uuid> Uuid::Parse(std::string_view text)
{
	const size_t dash = text.find('-');
	if (dash == std::string_view::npos)
		return std::nullopt;

	std::optional<uint64_t> lo = ParseNumber(text.substr(0, dash));
	std::optional<uint64_t> hi = ParseNumber(text.substr(dash + 1));
	if (!lo || !hi)
		return std::nullopt;
	return Uuid(*lo, *hi);
}
