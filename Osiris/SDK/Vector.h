#pragma once

#include <cmath>

#include "../Helpers.h"

class matrix3x4;

struct Vector {
	constexpr bool notNull() const noexcept
	{
		return x || y || z;
	}

	constexpr bool null() const noexcept
	{
		return !notNull();
	}

	constexpr float operator[](int i) const noexcept
	{
		return ((float*)this)[i];
	}

	constexpr float& operator[](int i) noexcept
	{
		return ((float*)this)[i];
	}

	constexpr bool operator==(const Vector& v) const noexcept
	{
		return x == v.x && y == v.y && z == v.z;
	}

	constexpr bool operator!=(const Vector& v) const noexcept
	{
		return !(*this == v);
	}

	constexpr Vector& operator=(const float array[3]) noexcept
	{
		x = array[0];
		y = array[1];
		z = array[2];
		return *this;
	}

	constexpr Vector& operator+=(const Vector& v) noexcept
	{
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	constexpr Vector& operator+=(float f) noexcept
	{
		x += f;
		y += f;
		z += f;
		return *this;
	}

	constexpr Vector& operator-=(const Vector& v) noexcept
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	constexpr Vector& operator-=(float f) noexcept
	{
		x -= f;
		y -= f;
		z -= f;
		return *this;
	}

	constexpr Vector& operator*=(const Vector& v) noexcept
	{
		x *= v.x;
		y *= v.y;
		z *= v.z;
		return *this;
	}

	constexpr Vector& operator*=(float f) noexcept
	{
		x *= f;
		y *= f;
		z *= f;
		return *this;
	}

	constexpr Vector& operator/=(const Vector& v) noexcept
	{
		x /= v.x;
		y /= v.y;
		z /= v.z;
		return *this;
	}

	constexpr Vector& operator/=(float f) noexcept
	{
		x /= f;
		y /= f;
		z /= f;
		return *this;
	}

	constexpr Vector operator+(const Vector& v) const noexcept
	{
		return Vector{ x + v.x, y + v.y, z + v.z };
	}

	constexpr Vector operator+(float f) const noexcept
	{
		return Vector{ x + f, y + f, z + f };
	}

	constexpr Vector operator-(const Vector& v) const noexcept
	{
		return Vector{ x - v.x, y - v.y, z - v.z };
	}

	constexpr Vector operator-(float f) const noexcept
	{
		return Vector{ x - f, y - f, z - f };
	}

	constexpr Vector operator*(const Vector& v) const noexcept
	{
		return Vector{ x * v.x, y * v.y, z * v.z };
	}

	constexpr Vector operator*(float f) const noexcept
	{
		return Vector{ x * f, y * f, z * f };
	}

	constexpr Vector operator/(const Vector& v) const noexcept
	{
		return Vector{ x / v.x, y / v.y, z / v.z };
	}

	constexpr Vector operator/(float f) const noexcept
	{
		return Vector{ x / f, y / f, z / f };
	}

	constexpr Vector& normalize() noexcept
	{
		x = std::isfinite(x) ? std::remainder(x, 360.0f) : 0.0f;
		y = std::isfinite(y) ? std::remainder(y, 360.0f) : 0.0f;
		z = 0.0f;
		return *this;
	}

	float length() const noexcept
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	float length2D() const noexcept
	{
		return std::sqrt(x * x + y * y);
	}

	constexpr float squareLength() const noexcept
	{
		return x * x + y * y + z * z;
	}

	constexpr float dotProduct(const Vector& v) const noexcept
	{
		return x * v.x + y * v.y + z * v.z;
	}

	constexpr float dotProduct2D(const Vector& v) const noexcept
	{
		return x * v.x + y * v.y;
	}

	constexpr Vector crossProduct(const Vector& v) const noexcept
	{
		return Vector{ y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
	}

	constexpr auto transform(const matrix3x4& mat) const noexcept;

	float distTo(const Vector& v) const noexcept
	{
		return (*this - v).length();
	}

	Vector toAngle() const noexcept
	{
		return Vector{ Helpers::rad2deg(std::atan2(-z, std::hypot(x, y))),
					   Helpers::rad2deg(std::atan2(y, x)),
					   0.0f };
	}

	Vector snapTo4() const noexcept
	{
		const float l = length2D();
		bool xp = x >= 0.0f;
		bool yp = y >= 0.0f;
		bool xy = std::abs(x) >= std::abs(y);
		if (xp && xy)
			return Vector{ l, 0.0f, 0.0f };
		if (!xp && xy)
			return Vector{ -l, 0.0f, 0.0f };
		if (yp && !xy)
			return Vector{ 0.0f, l, 0.0f };
		if (!yp && !xy)
			return Vector{ 0.0f, -l, 0.0f };

		return Vector{};
	}

	void fromAngle(std::optional<std::reference_wrapper<Vector>> forward, std::optional<std::reference_wrapper<Vector>> right, std::optional<std::reference_wrapper<Vector>> up) const noexcept
	{
		const float sr = std::sin(Helpers::deg2rad(z))
				  , sp = std::sin(Helpers::deg2rad(x))
				  , sy = std::sin(Helpers::deg2rad(y))
				  , cr = std::cos(Helpers::deg2rad(z))
				  , cp = std::cos(Helpers::deg2rad(x))
				  , cy = std::cos(Helpers::deg2rad(y));

		if (forward) forward->get() = Vector{cp * cy, cp * sy, -sp};
		if (right) right->get() = Vector{(-1 * sr * sp * cy + -1 * cr * -sy), (-1 * sr * sp * sy + -1 * cr * cy), -1 * sr * cp};
		if (up) up->get() = Vector{(cr * sp * cy + -sr * -sy), (cr * sp * sy + -sr * cy), cr * cp};
	}

	static Vector fromAngle(const Vector& angle) noexcept
	{
		Vector forward; angle.fromAngle(forward, {}, {});
		return forward;
	}

	static Vector fromAngle2D(float angle) noexcept
	{
		return Vector{ std::cos(Helpers::deg2rad(angle)),
					   std::sin(Helpers::deg2rad(angle)),
					   0.0f };
	}

	Vector normalized() const noexcept
	{
		return *this * (1.f / (length() + std::numeric_limits<float>::epsilon()));
	}

	float x, y, z;
};

#include "matrix3x4.h"

constexpr auto Vector::transform(const matrix3x4& mat) const noexcept
{
	return Vector{ dotProduct({ mat[0][0], mat[0][1], mat[0][2] }) + mat[0][3],
				   dotProduct({ mat[1][0], mat[1][1], mat[1][2] }) + mat[1][3],
				   dotProduct({ mat[2][0], mat[2][1], mat[2][2] }) + mat[2][3] };
}