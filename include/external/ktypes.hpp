//Copyright(C) 2025 Lost Empire Entertainment
//This header comes with ABSOLUTELY NO WARRANTY.
//This is free code, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

//This header is a part of the KalaKit KalaHeaders repository: https://github.com/KalaKit/KalaHeaders

// ======================================================================
//  Provides fixed-size, memory-safe primitive types for cross-platform math, logic, and data layout.
//  Ensures consistent behavior across platforms (Windows, Linux).
//  Includes constexpr min/max bounds and static assertions for type safety.
// ======================================================================

#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>

namespace KalaKit::KalaTypes
{
	using std::int8_t;
	using std::int16_t;
	using std::int32_t;
	using std::uint8_t;
	using std::uint16_t;
	using std::uint32_t;
	using std::size_t;
	using std::ptrdiff_t;
	using std::numeric_limits;
	using std::is_unsigned_v;

	// ======================================================================
	// 
	// SIGNED INT
	// 
	// ======================================================================

	using i8 = int8_t;             //-128 to 127
	using i16 = int16_t;           //-32,768 to 32,767
	using i32 = int32_t;           //-2,147,483,648 to 2,147,483,647

	inline constexpr i8  I8_MIN = -128;
	inline constexpr i8  I8_MAX = 127;
	inline constexpr i16 I16_MIN = -32768;
	inline constexpr i16 I16_MAX = 32767;
	inline constexpr i32 I32_MIN = -2147483648;
	inline constexpr i32 I32_MAX = 2147483647;

	// ======================================================================
	// 
	// UNSIGNED INT
	// 
	// ======================================================================

	using u8 = uint8_t;            //0 - 255
	using u16 = uint16_t;          //0 - 65,535
	using u32 = uint32_t;          //0 to 4,294,967,295

	inline constexpr u8  U8_MIN = 0;
	inline constexpr u8  U8_MAX = 255;
	inline constexpr u16 U16_MIN = 0;
	inline constexpr u16 U16_MAX = 65535;
	inline constexpr u32 U32_MIN = 0;
	inline constexpr u32 U32_MAX = 4294967295u;

	// ======================================================================
	// 
	// FLOAT
	// 
	// ======================================================================

	using f32 = float;             //up to 38-digit magnitude, ~7 digits precision

	inline constexpr f32 F32_MIN = -3.402823466e+38f;
	inline constexpr f32 F32_MAX = 3.402823466e+38f;
	inline constexpr f32 F32_EPSILON = 1.192092896e-07f;

	using f64 = double;            //up to 308-digit magnitude, ~16 digits precision

	inline constexpr f64 F64_MIN = -1.7976931348623157e+308;
	inline constexpr f64 F64_MAX = 1.7976931348623157e+308;
	inline constexpr f64 F64_EPSILON = 2.2204460492503131e-16;

	//
	// SIZE
	//

	using usize = size_t;
	using isize = ptrdiff_t;

	inline constexpr isize ISIZE_MIN = static_cast<isize>(~(usize{} >> 1));
	inline constexpr isize ISIZE_MAX = static_cast<isize>(usize{} >> 1);
	inline constexpr usize USIZE_MIN = 0;
	inline constexpr usize USIZE_MAX = static_cast<usize>(~0);

	// ======================================================================
	// 
	// BITFIELD BOOL
	// 
	// Provides a tightly packed array of 1-bit boolean flags using a backing integer.
	// Reduces memory usage and padding compared to individual bools (which are 1 byte).
	// Backing type (T) must be unsigned: u8, u16 or u32.
	// 
	// Types:
	//   b8 - 8 booleans into 1 byte
	//   b16 - 16 booleans into 2 bytes
	//   b32 - 32 booleans into 4 bytes
	// 
	// Usage:
	//   b8 flags{};
	//   flags[0] = true;                   //enable bit 0
	//   if (flags[3]) { /*do something*/ } //check bit 3
	// 
	//   //with named indices:
	//   constexpr u8 visible = 0;
	//   constexpr u8 active = 1;
	//   flags[visible] = true;
	//   if (flags[active]) { /*do something*/ }
	// ======================================================================

	template<typename T, u8 NumBits>
	struct BitField
	{
		static_assert(is_unsigned_v<T>, "Bitfield backing type must be unsigned");
		static_assert(NumBits <= sizeof(T) * 8, "BitField NumBits exceeds capacity of backing type");

		T value = 0;

		struct BitRef
		{
			T& value;
			u8 index;

			operator bool() const { return (value >> index) & 1; }

			BitRef& operator=(bool b)
			{
				if (b) value |= (T(1) << index);
				else   value &= ~(T(1) << index);
				return *this;
			}
		};

		constexpr BitRef operator[](u8 index) { return BitRef{ value, index }; }
		constexpr bool   operator[](u8 index) const { return (value >> index) & 1; }
	};

	using b8 = BitField<u8, 8>;    //array of 8 1-bit bool states
	using b16 = BitField<u16, 16>; //array of 16 1-bit bool states
	using b32 = BitField<u32, 32>; //array of 32 1-bit bool states

//
// TODO: future math and utility types to extend KalaKit base types
//

// Vector types
// using vec2f = ...;   // 2D float vector (x, y)
// using vec3f = ...;   // 3D float vector (x, y, z)
// using vec4f = ...;   // 4D float vector (x, y, z, w)
// using vec2i = ...;   // 2D integer vector
// using vec3i = ...;   // 3D integer vector
// using vec4b8 = ...;  // 4D packed boolean vector (useful for flags)

// Matrix types
// using mat2 = ...;    // 2x2 float matrix
// using mat3 = ...;    // 3x3 float matrix
// using mat4 = ...;    // 4x4 float matrix

// Quaternion and rotation
// using quat = ...;    // quaternion for rotation (x, y, z, w)
// using euler = ...;   // euler angles (pitch, yaw, roll)
