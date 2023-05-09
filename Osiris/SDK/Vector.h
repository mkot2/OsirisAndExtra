#pragma once

#include <cmath>

#include "../Helpers.h"
#include "Utils.h"

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

	Vector normalized() noexcept
	{
		auto vectorNormalize = [](Vector& vector) {
			float radius = static_cast<float>(std::sqrt(std::pow(vector.x, 2) + std::pow(vector.y, 2) + std::pow(vector.z, 2)));
			radius = 1.f / (radius + std::numeric_limits<float>::epsilon());

			vector *= radius;

			return radius;
		};
		Vector v = *this;
		vectorNormalize(v);
		return v;
	}

	constexpr Vector& clamp() noexcept
	{
		x = std::clamp(x, -89.f, 89.f);
		y = std::clamp(y, -180.f, 180.f);
		z = std::clamp(z, -50.f, 50.f);
		return *this;
	}

	constexpr Vector& normalize() noexcept
	{
		x = std::isfinite(x) ? std::remainder(x, 360.0f) : 0.0f;
		y = std::isfinite(y) ? std::remainder(y, 360.0f) : 0.0f;
		z = 0.0f;
		return *this;
	}

	auto length() const noexcept
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	auto length2D() const noexcept
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

	constexpr auto dotProduct(const float* other) const noexcept
	{
		return x * other[0] + y * other[1] + z * other[2];
	}

	constexpr auto transform(const matrix3x4& mat) const noexcept;

	auto distTo(const Vector& v) const noexcept
	{
		return (*this - v).length();
	}

	auto toAngle() const noexcept
	{
		return Vector{ Helpers::rad2deg(std::atan2(-z, std::hypot(x, y))),
					   Helpers::rad2deg(std::atan2(y, x)),
					   0.0f };
	}

	auto snapTo4() const noexcept
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

	static auto fromAngle(const Vector& angle) noexcept
	{
		return Vector{ std::cos(Helpers::deg2rad(angle.x)) * std::cos(Helpers::deg2rad(angle.y)),
					   std::cos(Helpers::deg2rad(angle.x)) * std::sin(Helpers::deg2rad(angle.y)),
					  -std::sin(Helpers::deg2rad(angle.x)) };
	}

	static auto fromAngle2D(float angle) noexcept
	{
		return Vector{ std::cos(Helpers::deg2rad(angle)),
					  std::sin(Helpers::deg2rad(angle)),
					  0.0f };
	}

	static auto fromAngle(const Vector& angle, Vector* out) noexcept
	{
		if (out) {
			out->x = std::cos(Helpers::deg2rad(angle.x)) * std::cos(Helpers::deg2rad(angle.y));
			out->y = std::cos(Helpers::deg2rad(angle.x)) * std::sin(Helpers::deg2rad(angle.y));
			out->z = -std::sin(Helpers::deg2rad(angle.x));
		}
	}

	auto fromAngle(Vector& forward, Vector& right, Vector& up) const noexcept
	{
		float sr = std::sin(Helpers::deg2rad(z))
			, sp = std::sin(Helpers::deg2rad(x))
			, sy = std::sin(Helpers::deg2rad(y))
			, cr = std::cos(Helpers::deg2rad(z))
			, cp = std::cos(Helpers::deg2rad(x))
			, cy = std::cos(Helpers::deg2rad(y));

		forward = Vector{ cp * cy, cp * sy, -sp };
		right = Vector{ (-1 * sr * sp * cy + -1 * cr * -sy), (-1 * sr * sp * sy + -1 * cr * cy), -1 * sr * cp };
		up = Vector{ (cr * sp * cy + -sr * -sy), (cr * sp * sy + -sr * cy), cr * cp };
	}

	constexpr auto dotProduct2D(const Vector& v) const noexcept
	{
		return x * v.x + y * v.y;
	}

	constexpr auto crossProduct(const Vector& v) const noexcept
	{
		return Vector{ y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
	}

	constexpr static auto up() noexcept
	{
		return Vector{ 0.0f, 0.0f, 1.0f };
	}

	constexpr static auto down() noexcept
	{
		return Vector{ 0.0f, 0.0f, -1.0f };
	}

	constexpr static auto forward() noexcept
	{
		return Vector{ 1.0f, 0.0f, 0.0f };
	}

	constexpr static auto back() noexcept
	{
		return Vector{ -1.0f, 0.0f, 0.0f };
	}

	constexpr static auto left() noexcept
	{
		return Vector{ 0.0f, 1.0f, 0.0f };
	}

	constexpr static auto right() noexcept
	{
		return Vector{ 0.0f, -1.0f, 0.0f };
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
