#pragma once

#include <cstdint>
#include <iostream>

namespace PiSubmarine::Chipset::Units
{
	class MicroKelvins
	{
	public:
		using ValueType = uint32_t;

		explicit constexpr MicroKelvins(ValueType value = 0) noexcept
			: Value(value)
		{
		}

		constexpr ValueType Get() const noexcept { return Value; }

		constexpr double ToKelvin() const noexcept
		{
			return static_cast<double>(Value) / 1'000'000.0;
		}

		constexpr double ToCelcius() const noexcept
		{
			return ToKelvin() - 273.15;
		}

		// Arithmetic operators
		constexpr MicroKelvins operator+(const MicroKelvins& other) const noexcept
		{
			return MicroKelvins(Value + other.Value);
		}

		constexpr MicroKelvins operator-(const MicroKelvins& other) const noexcept
		{
			return MicroKelvins(Value - other.Value);
		}

		constexpr MicroKelvins operator*(uint64_t scalar) const noexcept
		{
			return MicroKelvins(Value * scalar);
		}

		constexpr MicroKelvins operator/(uint64_t scalar) const noexcept
		{
			return MicroKelvins(Value / scalar);
		}

		constexpr bool operator==(const MicroKelvins& other) const noexcept
		{
			return Value == other.Value;
		}

		constexpr bool operator!=(const MicroKelvins& other) const noexcept
		{
			return !(*this == other);
		}

	private:
		ValueType Value;
	};
}

constexpr PiSubmarine::Chipset::Units::MicroKelvins operator"" _uK(unsigned long long val)
{
	return PiSubmarine::Chipset::Units::MicroKelvins(static_cast<PiSubmarine::Chipset::Units::MicroKelvins::ValueType>(val));
}
