//Copyright(C) 2025 Lost Empire Entertainment
//This header comes with ABSOLUTELY NO WARRANTY.
//This is free code, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

//This header is a part of the KalaKit KalaHeaders repository: https://github.com/KalaKit/KalaHeaders

// ======================================================================
//  Provides fixed-size, memory-safe primitive types for cross-platform math, logic, and strData layout.
//  Ensures consistent behavior across platforms (Windows, Linux).
//  Includes constexpr min/max bounds and static assertions for type safety.
// ======================================================================

#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>
#include <string>
#include <compare>

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
	using std::is_same_v;
	using std::string;
	using std::basic_string;
	using std::strong_ordering;

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

	// ======================================================================
	// 
	// CHAR
	// 
	// ======================================================================

	using c8 = char;               //roman letters and arabic numbers
	using c16 = char16_t;          //international letters and symbols
	using c32 = char32_t;          //emojis and rare unicode

	// ======================================================================
	// 
	// FIXED LENGTH STRING
	// 
	// Defines compile-time fixed-length strings for safe, bounded text storage.
	// These strings are strongly typed and enforce character encoding and size limits.
	// Overwrites zero-fill unused space to prevent garbage memory.
	// 
	// Types:
	//   s8<N>  - N-length string of c8  (1-byte) characters: ASCII/english text and arabic numbers
	//   s16<N> - N-length string of c16 (2-byte) characters: international alphabets
	//   s32<N> - N-length string of c32 (4-byte) characters: full unicode like emojis
	// 
	// Usage:
	//   read docs/usage/fixed_string.md
	// ======================================================================

	template<typename CharT, usize Length>
	struct FixedString
	{
		static_assert
		(
			is_same_v<CharT, c8>
			|| is_same_v<CharT, c16>
			|| is_same_v<CharT, c32>,
			"FixedString only supports c8, c16, c32"
		);

		CharT strData[Length]{}; //zero-initialized

		//constructs a zero-initialized fixed string (all characters set to 0)
		constexpr FixedString() = default;

		//constructs from a string literal known at runtime
		template<usize N>
		constexpr FixedString(const CharT(&literal)[N])
		{
			static_assert(N - 1 <= Length, "Literal is too long for FixedString");

			for (usize i = 0; i < Length; ++i)
			{
				strData[i] = (i < N - 1) ? literal[i] : 0;
			}
		}

		//constructs from a single character
		constexpr FixedString(CharT ch)
		{
			strData[0] = ch;
			for (usize i = 1; i < Length; ++i) strData[i] = 0;
		}

		//assigns from a string literal at runtime
		template<usize N>
		constexpr FixedString& operator=(const CharT(&literal)[N])
		{
			static_assert(N - 1 <= Length, "Literal too long for FixedString");

			for (usize i = 0; i < Length; ++i)
			{
				strData[i] = (i < N - 1) ? literal[i] : 0;
			}

			return *this;
		}

		//assigns from a basic_string at runtime
		FixedString& operator=(const CharT* str)
		{
			for (usize i = 0; i < Length; ++i)
			{
				strData[i] = str[i];
				if (str[i] == 0)
				{
					//fill remaining with zeros
					for (++i; i < Length; ++i) strData[i] = 0;
					break;
				}
			}
			return *this;
		}

		//constructs from basic_string at runtime
		constexpr FixedString(const basic_string<CharT>& str)
		{
			const usize count = str.size();
			for (usize i = 0; i < Length; ++i)
			{
				strData[i] = (i < count) ? str[i] : 0;
			}
		}

		//assigns from basic_string at runtime
		FixedString& operator=(const basic_string<CharT>& str)
		{
			const usize count = str.size();
			for (usize i = 0; i < Length; ++i)
			{
				strData[i] = (i < count) ? str[i] : 0;
			}
			return *this;
		}

		//converts to basic_string
		basic_string<CharT> to_string() const
		{
			usize count = 0;
			while (count < Length
				&& strData[count] != 0)
			{
				++count;
			}
			return basic_string<CharT>(strData, count);
		}

		constexpr usize size() const { return Length; }
		constexpr usize length() const
		{
			for (usize i = 0; i < Length; ++i)
			{
				if (strData[i] == 0) return i;
			}
			return Length;
		}
		constexpr const CharT* begin() const { return strData; }
		constexpr CharT* begin() { return strData; }
		constexpr const CharT* end() const { return strData + Length; }
		constexpr CharT* end() { return strData + Length; }
		constexpr const CharT* c_str() const { return strData; }
		constexpr const CharT* data() const { return strData; }
		constexpr bool empty() const { return strData[0] == 0; }
		constexpr void clear() { for (usize i = 0; i < Length; ++i) strData[i] = 0; }
		constexpr bool starts_with(const FixedString& prefix) const
		{
			for (usize i = 0; i < prefix.size(); ++i)
			{
				if (strData[i] != prefix.strData[i]) return false;
			}
			return true;
		}
		constexpr bool ends_with(const FixedString& suffix) const
		{
			usize len = length(), slen = suffix.length();
			if (slen > len) return false;
			for (usize i = 0; i < slen; ++i)
			{
				if (strData[len - slen + i] != suffix.strData[i]) return false;
			}
			return true;
		}
		constexpr usize find(CharT ch, usize start = 0) const
		{
			for (usize i = start; i < Length; ++i)
			{
				if (strData[i] == 0) break; //stop at null terminator
				if (strData[i] == ch) return i;
			}
			return Length; //not found
		}
		constexpr bool contains(CharT ch) const
		{
			return find(ch) != Length;
		}

		//returns a substring view from start to start+count
		constexpr FixedString substr(usize start, usize count) const
		{
			FixedString result{};
			for (usize i = 0; i < count && (start + i) < Length; ++i)
			{
				result.strData[i] = strData[start + i];
			}
			return result;
		}

		//truncates the string at a given length
		constexpr void truncate(usize newLength)
		{
			if (newLength < Length) strData[newLength] = 0;
		}

		//resizes the string to a given length, filling with a character if needed
		constexpr void resize(usize newLength, CharT fill = 0)
		{
			usize current = length();
			if (newLength < current) strData[newLength] = 0;
			else
			{
				for (usize i = current; i < newLength && i < Length; ++i)
				{
					strData[i] = fill;
				}
				if (newLength < Length) strData[newLength] = 0;
			}
		}

		//remove a matching prefix
		constexpr void trim_start(const FixedString& pattern)
		{
			usize plen = pattern.length();
			if (plen == 0) return;

			while (starts_with(pattern))
			{
				for (usize i = 0; i + plen < Length; ++i)
				{
					strData[i] = strData[i + plen];
				}
				for (usize i = length(); i < Length; ++i)
				{
					strData[i] = 0;
				}
			}
		}

		//removes a matching suffix
		constexpr void trim_end(const FixedString& pattern)
		{
			usize plen = pattern.length();
			if (plen == 0) return;

			while (ends_with(pattern))
			{
				usize len = length();
				if (len < plen) break;
				strData[len - plen] = 0;
			}
		}

		//removes both matching prefix and suffix
		constexpr void trim(const FixedString& pattern)
		{
			trim_start(pattern);
			trim_end(pattern);
		}

		//equal comparison between fixed strings
		friend constexpr bool operator==(const FixedString& lhs, const FixedString& rhs)
		{
			for (usize i = 0; i < Length; ++i)
			{
				if (lhs.strData[i] != rhs.strData[i]) return false;
			}
			return true;
		}
		//not equal comparison between fixed strings
		friend constexpr bool operator!=(const FixedString& lhs, const FixedString& rhs)
		{
			return !(lhs == rhs);
		}
		//three-way comparison between fixed strings
		friend constexpr auto operator<=>(const FixedString& lhs, const FixedString& rhs)
		{
			for (usize i = 0; i < Length; ++i)
			{
				if (lhs.strData[i] != rhs.strData[i])
				{
					return lhs.strData[i] <=> rhs.strData[i];
				}
				return strong_ordering::equal;
			}
		}

		//equal comparison between fixed string and basic_string
		friend bool operator==(const FixedString& lhs, const basic_string<CharT>& rhs)
		{
			for (usize i = 0; i < Length; ++i)
			{
				if (i >= rhs.size()) return lhs.strData[i] == 0;
				if (lhs.strData[i] != rhs[i]) return false;
			}
			return rhs.size() <= Length;
		}
		//not equal comparison between fixed string and basic_string
		friend bool operator!=(const FixedString& lhs, const basic_string<CharT>& rhs)
		{
			return !(lhs == rhs);
		}
		//three-way comparison between fixed string and basic_string
		friend auto operator<=>(const FixedString& lhs, const basic_string<CharT>& rhs)
		{
			usize i = 0;
			for (; i < Length && i < rhs.size(); ++i)
			{
				if (lhs.strData[i] != rhs[i])
				{
					return lhs.strData[i] <=> rhs[i];
				}
			}
			//shorter string is less
			if (i < Length && rhs.size() <= i) return strong_ordering::greater;
			if (i < rhs.size() && Length <= i) return strong_ordering::less;
			return strong_ordering::equal;
		}

		//equal comparison between basic_string and fixed string
		friend bool operator==(const basic_string<CharT>& lhs, const FixedString& rhs)
		{
			return lhs == rhs;
		}
		//not equal comparison between basic_string and fixed string
		friend bool operator!=(const basic_string<CharT>& lhs, const FixedString& rhs)
		{
			return !(lhs == rhs);
		}

		//three-way comparison between basic_string and fixed string
		friend auto operator<=>(const basic_string<CharT>& lhs, const FixedString& rhs)
		{
			usize i = 0;
			for (; i < lhs.size() && i < Length; ++i)
			{
				if (lhs[i] != rhs.strData[i])
				{
					return lhs[i] <=> rhs.strData[i];
				}
			}
			if (lhs.size() < rhs.length()) return strong_ordering::less;
			if (lhs.size() > rhs.length()) return strong_ordering::greater;
			return strong_ordering::equal;
		}

		//appends a fixed string to this one
		constexpr FixedString& operator+=(const FixedString& other)
		{
			usize i = length();
			usize j = 0;
			while (i < Length
				&& other.strData[j] != 0)
			{
				strData[i++] = other.strData[j];
			}
			if (i < Length) strData[i] = 0;
			return *this;
		}

		constexpr CharT& operator[](usize index) { return strData[index]; }
		constexpr const CharT& operator[](usize index) const { return strData[index]; }
	};

	template<usize N> using s8 = FixedString<c8, N>;   //fixed-length, 1-byte-only string
	template<usize N> using s16 = FixedString<c16, N>; //fixed-length, 2-byte or lesser string
	template<usize N> using s32 = FixedString<c32, N>; //fixed-length, 4-byte or lesser string
}

namespace std
{
	//for fixed string unordered_set and unordered_map
	template<typename CharT, KalaKit::KalaTypes::usize Length>
	struct hash<KalaKit::KalaTypes::FixedString<CharT, Length>>
	{
		constexpr size_t operator()(const KalaKit::KalaTypes::FixedString<CharT, Length>& str) const
		{
			size_t h = 0;
			for (KalaKit::KalaTypes::usize i = 0; i < str.length(); ++i)
			{
				h ^= hash<CharT>{}(str[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
			}
			return h;
		}
	};
}

//
// TODO: fixes
//
// - need to fix invalid size fixed string

//
// TODO: future math and utility types to extend KalaKit base types
//

// contains(FixedString)
// Check if one FixedString contains another substring(not just a single char).

// + = CharT` or `+ = const CharT *
// Append a single character or a C - style string to a FixedString(bounded).

// `compare()` method**
// Lexical comparison returning - 1, 0, or 1 (like `strcmp`).

// `explicit operator std::basic_string<CharT>()
// Enable implicit / explicit conversion to STL string(alternative to `.to_string()`).

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
