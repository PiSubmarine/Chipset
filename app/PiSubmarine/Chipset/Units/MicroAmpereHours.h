#pragma once

#include <cstdint>
#include <iostream>

namespace PiSubmarine::Chipset::Units
{
	class MicroAmpereHours
	{
	public:
		using ValueType = uint32_t;

		explicit constexpr MicroAmpereHours(ValueType value = 0) noexcept
			: Value(value)
		{
		}

		constexpr ValueType Get() const noexcept { return Value; }

		constexpr double ToVolts() const noexcept
		{
			return static_cast<double>(Value) / 1'000'000.0;
		}

		constexpr MicroAmpereHours operator+(const MicroAmpereHours& other) const noexcept
		{
			return MicroAmpereHours(Value + other.Value);
		}

		constexpr MicroAmpereHours operator-(const MicroAmpereHours& other) const noexcept
		{
			return MicroAmpereHours(Value - other.Value);
		}

		constexpr MicroAmpereHours operator*(uint64_t scalar) const noexcept
		{
			return MicroAmpereHours(Value * scalar);
		}

		constexpr MicroAmpereHours operator/(uint64_t scalar) const noexcept
		{
			return MicroAmpereHours(Value / scalar);
		}

		constexpr bool operator==(const MicroAmpereHours& other) const noexcept
		{
			return Value == other.Value;
		}

		constexpr bool operator!=(const MicroAmpereHours& other) const noexcept
		{
			return !(*this == other);
		}

	private:
		ValueType Value;
	};
}

constexpr PiSubmarine::Chipset::Api::MicroAmpereHours operator"" _uAh(unsigned long long val)
{
	return PiSubmarine::Chipset::Api::MicroAmpereHours(static_cast<PiSubmarine::Chipset::Api::MicroAmpereHours::ValueType>(val));
}
