#include "Vector2.h"

Vector2 Vector2::operator+(const Vector2& other)
{
	return { x + other.x, y + other.y };
}

Vector2 Vector2::operator-(const Vector2& other)
{
	return { x - other.x, y - other.y };
}

Vector2 Vector2::operator*(const Vector2& other)
{
	return { x * other.x, y * other.y };
}

Vector2 Vector2::operator/(const Vector2& other)
{
	return { x / other.x, y / other.y };
}

Vector2 Vector2::operator+=(const Vector2& v)
{
	x += v.x;
	y += v.y;
	return *this;
}

Vector2 Vector2::operator-=(const Vector2& v)
{
	x -= v.x;
	y -= v.y;
	return *this;
}

Vector2 Vector2::operator*=(float scalar)
{
	x *= scalar;
	y *= scalar;
	return *this;
}