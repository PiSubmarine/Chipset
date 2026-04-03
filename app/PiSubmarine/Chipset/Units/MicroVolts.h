#pragma once

#include <cstdint>
#include <iostream>

namespace PiSubmarine::Chipset::Units
{
	class MicroVolts
	{
	public:
		using ValueType = uint32_t;

		explicit constexpr MicroVolts(ValueType value = 0) noexcept
			: Value(value)
		{
		}

		[[nodiscard]] constexpr ValueType Get() const noexcept { return Value; }

		[[nodiscard]] constexpr double ToVolts() const noexcept
		{
			return static_cast<double>(Value) / 1'000'000.0;
		}

		// Arithmetic operators
		constexpr MicroVolts operator+(const MicroVolts& other) const noexcept
		{
			return MicroVolts(Value + other.Value);
		}

		constexpr MicroVolts operator-(const MicroVolts& other) const noexcept
		{
			return MicroVolts(Value - other.Value);
		}

		constexpr MicroVolts operator*(uint64_t scalar) const noexcept
		{
			return MicroVolts(Value * scalar);
		}

		constexpr MicroVolts operator/(uint64_t scalar) const noexcept
		{
			return MicroVolts(Value / scalar);
		}

		constexpr bool operator==(const MicroVolts& other) const noexcept
		{
			return Value == other.Value;
		}

		constexpr bool operator!=(const MicroVolts& other) const noexcept
		{
			return !(*this == other);
		}

	private:
		ValueType Value;
	};
}

constexpr PiSubmarine::Chipset::Units::MicroVolts operator"" _uV(unsigned long long val)
{
	return PiSubmarine::Chipset::Units::MicroVolts(val);
}
