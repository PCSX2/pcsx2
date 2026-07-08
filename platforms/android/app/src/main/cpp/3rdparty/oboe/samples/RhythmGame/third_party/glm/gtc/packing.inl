/// @ref gtc_packing
/// @file glm/gtc/packing.inl

#include "../common.hpp"
#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../detail/type_half.hpp"
#include <cstring>
#include <limits>

namespace glm{
namespace detail
{
	GLM_FUNC_QUALIFIER glm::uint16 float2half(glm::uint32 f)
	{
		// 10 bits    =>                         EE EEEFFFFF
		// 11 bits    =>                        EEE EEFFFFFF
		// Half bits  =>                   SEEEEEFF FFFFFFFF
		// Float bits => SEEEEEEE EFFFFFFF FFFFFFFF FFFFFFFF

		// 0x00007c00 => 00000000 00000000 01111100 00000000
		// 0x000003ff => 00000000 00000000 00000011 11111111
		// 0x38000000 => 00111000 00000000 00000000 00000000
		// 0x7f800000 => 01111111 10000000 00000000 00000000
		// 0x00008000 => 00000000 00000000 10000000 00000000
		return
			((f >> 16) & 0x8000) | // sign
			((((f & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) | // exponential
			((f >> 13) & 0x03ff); // Mantissa
	}

	GLM_FUNC_QUALIFIER glm::uint32 float2packed11(glm::uint32 f)
	{
		// 10 bits    =>                         EE EEEFFFFF
		// 11 bits    =>                        EEE EEFFFFFF
		// Half bits  =>                   SEEEEEFF FFFFFFFF
		// Float bits => SEEEEEEE EFFFFFFF FFFFFFFF FFFFFFFF

		// 0x000007c0 => 00000000 00000000 00000111 11000000
		// 0x00007c00 => 00000000 00000000 01111100 00000000
		// 0x000003ff => 00000000 00000000 00000011 11111111
		// 0x38000000 => 00111000 00000000 00000000 00000000
		// 0x7f800000 => 01111111 10000000 00000000 00000000
		// 0x00008000 => 00000000 00000000 10000000 00000000
		return
			((((f & 0x7f800000) - 0x38000000) >> 17) & 0x07c0) | // exponential
			((f >> 17) & 0x003f); // Mantissa
	}

	GLM_FUNC_QUALIFIER glm::uint32 packed11ToFloat(glm::uint32 p)
	{
		// 10 bits    =>                         EE EEEFFFFF
		// 11 bits    =>                        EEE EEFFFFFF
		// Half bits  =>                   SEEEEEFF FFFFFFFF
		// Float bits => SEEEEEEE EFFFFFFF FFFFFFFF FFFFFFFF

		// 0x000007c0 => 00000000 00000000 00000111 11000000
		// 0x00007c00 => 00000000 00000000 01111100 00000000
		// 0x000003ff => 00000000 00000000 00000011 11111111
		// 0x38000000 => 00111000 00000000 00000000 00000000
		// 0x7f800000 => 01111111 10000000 00000000 00000000
		// 0x00008000 => 00000000 00000000 10000000 00000000
		return
			((((p & 0x07c0) << 17) + 0x38000000) & 0x7f800000) | // exponential
			((p & 0x003f) << 17); // Mantissa
	}

	GLM_FUNC_QUALIFIER glm::uint32 float2packed10(glm::uint32 f)
	{
		// 10 bits    =>                         EE EEEFFFFF
		// 11 bits    =>                        EEE EEFFFFFF
		// Half bits  =>                   SEEEEEFF FFFFFFFF
		// Float bits => SEEEEEEE EFFFFFFF FFFFFFFF FFFFFFFF

		// 0x0000001F => 00000000 00000000 00000000 00011111
		// 0x0000003F => 00000000 00000000 00000000 00111111
		// 0x000003E0 => 00000000 00000000 00000011 11100000
		// 0x000007C0 => 00000000 00000000 00000111 11000000
		// 0x00007C00 => 00000000 00000000 01111100 00000000
		// 0x000003FF => 00000000 00000000 00000011 11111111
		// 0x38000000 => 00111000 00000000 00000000 00000000
		// 0x7f800000 => 01111111 10000000 00000000 00000000
		// 0x00008000 => 00000000 00000000 10000000 00000000
		return
			((((f & 0x7f800000) - 0x38000000) >> 18) & 0x03E0) | // exponential
			((f >> 18) & 0x001f); // Mantissa
	}

	GLM_FUNC_QUALIFIER glm::uint32 packed10ToFloat(glm::uint32 p)
	{
		// 10 bits    =>                         EE EEEFFFFF
		// 11 bits    =>                        EEE EEFFFFFF
		// Half bits  =>                   SEEEEEFF FFFFFFFF
		// Float bits => SEEEEEEE EFFFFFFF FFFFFFFF FFFFFFFF

		// 0x0000001F => 00000000 00000000 00000000 00011111
		// 0x0000003F => 00000000 00000000 00000000 00111111
		// 0x000003E0 => 00000000 00000000 00000011 11100000
		// 0x000007C0 => 00000000 00000000 00000111 11000000
		// 0x00007C00 => 00000000 00000000 01111100 00000000
		// 0x000003FF => 00000000 00000000 00000011 11111111
		// 0x38000000 => 00111000 00000000 00000000 00000000
		// 0x7f800000 => 01111111 10000000 00000000 00000000
		// 0x00008000 => 00000000 00000000 10000000 00000000
		return
			((((p & 0x03E0) << 18) + 0x38000000) & 0x7f800000) | // exponential
			((p & 0x001f) << 18); // Mantissa
	}

	GLM_FUNC_QUALIFIER glm::uint half2float(glm::uint h)
	{
		return ((h & 0x8000) << 16) | ((( h & 0x7c00) + 0x1C000) << 13) | ((h & 0x03FF) << 13);
	}

	GLM_FUNC_QUALIFIER glm::uint floatTo11bit(float x)
	{
		if(x == 0.0f)
			return 0u;
		else if(glm::isnan(x))
			return ~0u;
		else if(glm::isinf(x))
			return 0x1Fu << 6u;

		uint Pack = 0u;
		memcpy(&Pack, &x, sizeof(Pack));
		return float2packed11(Pack);
	}

	GLM_FUNC_QUALIFIER float packed11bitToFloat(glm::uint x)
	{
		if(x == 0)
			return 0.0f;
		else if(x == ((1 << 11) - 1))
			return ~0;//NaN
		else if(x == (0x1f << 6))
			return ~0;//Inf

		uint Result = packed11ToFloat(x);

		float Temp = 0;
		memcpy(&Temp, &Result, sizeof(Temp));
		return Temp;
	}

	GLM_FUNC_QUALIFIER glm::uint floatTo10bit(float x)
	{
		if(x == 0.0f)
			return 0u;
		else if(glm::isnan(x))
			return ~0u;
		else if(glm::isinf(x))
			return 0x1Fu << 5u;

		uint Pack = 0;
		memcpy(&Pack, &x, sizeof(Pack));
		return float2packed10(Pack);
	}

	GLM_FUNC_QUALIFIER float packed10bitToFloat(glm::uint x)
	{
		if(x == 0)
			return 0.0f;
		else if(x == ((1 << 10) - 1))
			return ~0;//NaN
		else if(x == (0x1f << 5))
			return ~0;//Inf

		uint Result = packed10ToFloat(x);

		float Temp = 0;
		memcpy(&Temp, &Result, sizeof(Temp));
		return Temp;
	}

//	GLM_FUNC_QUALIFIER glm::uint f11_f11_f10(float x, float y, float z)
//	{
//		return ((floatTo11bit(x) & ((1 << 11) - 1)) << 0) |  ((floatTo11bit(y) & ((1 << 11) - 1)) << 11) | ((floatTo10bit(z) & ((1 << 10) - 1)) << 22);
//	}

	union u3u3u2
	{
		struct
		{
			uint x : 3;
			uint y : 3;
			uint z : 2;
		} data;
		uint8 pack;
	};

	union u4u4
	{
		struct
		{
			uint x : 4;
			uint y : 4;
		} data;
		uint8 pack;
	};

	union u4u4u4u4
	{
		struct
		{
			uint x : 4;
			uint y : 4;
			uint z : 4;
			uint w : 4;
		} data;
		uint16 pack;
	};

	union u5u6u5
	{
		struct
		{
			uint x : 5;
			uint y : 6;
			uint z : 5;
		} data;
		uint16 pack;
	};

	union u5u5u5u1
	{
		struct
		{
			uint x : 5;
			uint y : 5;
			uint z : 5;
			uint w : 1;
		} data;
		uint16 pack;
	};

	union u10u10u10u2
	{
		struct
		{
			uint x : 10;
			uint y : 10;
			uint z : 10;
			uint w : 2;
		} data;
		uint32 pack;
	};

	union i10i10i10i2
	{
		struct
		{
			int x : 10;
			int y : 10;
			int z : 10;
			int w : 2;
		} data;
		uint32 pack;
	};

	union u9u9u9e5
	{
		struct
		{
			uint x : 9;
			uint y : 9;
			uint z : 9;
			uint w : 5;
		} data;
		uint32 pack;
	};

	template <precision P, template <typename, precision> class vecType>
	struct compute_half
	{};

	template <precision P>
	struct compute_half<P, tvec1>
	{
		GLM_FUNC_QUALIFIER static tvec1<uint16, P> pack(tvec1<float, P> const & v)
		{
			int16 const Unpack(detail::toFloat16(v.x));
			u16vec1 Packed(uninitialize);
			memcpy(&Packed, &Unpack, sizeof(Packed));
			return Packed;
		}

		GLM_FUNC_QUALIFIER static tvec1<float, P> unpack(tvec1<uint16, P> const & v)
		{
			i16vec1 Unpack(uninitialize);
			memcpy(&Unpack, &v, sizeof(Unpack));
			return tvec1<float, P>(detail::toFloat32(v.x));
		}
	};

	template <precision P>
	struct compute_half<P, tvec2>
	{
		GLM_FUNC_QUALIFIER static tvec2<uint16, P> pack(tvec2<float, P> const & v)
		{
			tvec2<int16, P> const Unpack(detail::toFloat16(v.x), detail::toFloat16(v.y));
			u16vec2 Packed(uninitialize);
			memcpy(&Packed, &Unpack, sizeof(Packed));
			return Packed;
		}

		GLM_FUNC_QUALIFIER static tvec2<float, P> unpack(tvec2<uint16, P> const & v)
		{
			i16vec2 Unpack(uninitialize);
			memcpy(&Unpack, &v, sizeof(Unpack));
			return tvec2<float, P>(detail::toFloat32(v.x), detail::toFloat32(v.y));
		}
	};

	template <precision P>
	struct compute_half<P, tvec3>
	{
		GLM_FUNC_QUALIFIER static tvec3<uint16, P> pack(tvec3<float, P> const & v)
		{
			tvec3<int16, P> const Unpack(detail::toFloat16(v.x), detail::toFloat16(v.y), detail::toFloat16(v.z));
			u16vec3 Packed(uninitialize);
			memcpy(&Packed, &Unpack, sizeof(Packed));
			return Packed;
		}

		GLM_FUNC_QUALIFIER static tvec3<float, P> unpack(tvec3<uint16, P> const & v)
		{
			i16vec3 Unpack(uninitialize);
			memcpy(&Unpack, &v, sizeof(Unpack));
			return tvec3<float, P>(detail::toFloat32(v.x), detail::toFloat32(v.y), detail::toFloat32(v.z));
		}
	};

	template <precision P>
	struct compute_half<P, tvec4>
	{
		GLM_FUNC_QUALIFIER static tvec4<uint16, P> pack(tvec4<float, P> const & v)
		{
			tvec4<int16, P> const Unpack(detail::toFloat16(v.x), detail::toFloat16(v.y), detail::toFloat16(v.z), detail::toFloat16(v.w));
			u16vec4 Packed(uninitialize);
			memcpy(&Packed, &Unpack, sizeof(Packed));
			return Packed;
		}

		GLM_FUNC_QUALIFIER static tvec4<float, P> unpack(tvec4<uint16, P> const & v)
		{
			i16vec4 Unpack(uninitialize);
			memcpy(&Unpack, &v, sizeof(Unpack));
			return tvec4<float, P>(detail::toFloat32(v.x), detail::toFloat32(v.y), detail::toFloat32(v.z), detail::toFloat32(v.w));
		}
	};
}//namespace detail

	GLM_FUNC_QUALIFIER uint8 packUnorm1x8(float v)
	{
		return static_cast<uint8>(round(clamp(v, 0.0f, 1.0f) * 255.0f));
	}
	
	GLM_FUNC_QUALIFIER float unpackUnorm1x8(uint8 p)
	{
		float const Unpack(p);
		return Unpack * static_cast<float>(0.0039215686274509803921568627451); // 1 / 255
	}
	
	GLM_FUNC_QUALIFIER uint16 packUnorm2x8(vec2 const & v)
	{
		u8vec2 const Topack(round(clamp(v, 0.0f, 1.0f) * 255.0f));

		uint16 Unpack = 0;
		memcpy(&Unpack, &Topack, sizeof(Unpack));
		return Unpack;
	}
	
	GLM_FUNC_QUALIFIER vec2 unpackUnorm2x8(uint16 p)
	{
		u8vec2 Unpack(uninitialize);
		memcpy(&Unpack, &p, sizeof(Unpack));
		return vec2(Unpack) * float(0.0039215686274509803921568627451); // 1 / 255
	}

	GLM_FUNC_QUALIFIER uint8 packSnorm1x8(float v)
	{
		int8 const Topack(static_cast<int8>(round(clamp(v ,-1.0f, 1.0f) * 127.0f)));
		uint8 Packed = 0;
		memcpy(&Packed, &Topack, sizeof(Packed));
		return Packed;
	}
	
	GLM_FUNC_QUALIFIER float unpackSnorm1x8(uint8 p)
	{
		int8 Unpack = 0;
		memcpy(&Unpack, &p, sizeof(Unpack));
		return clamp(
			static_cast<float>(Unpack) * 0.00787401574803149606299212598425f, // 1.0f / 127.0f
			-1.0f, 1.0f);
	}
	
	GLM_FUNC_QUALIFIER uint16 packSnorm2x8(vec2 const & v)
	{
		i8vec2 const Topack(round(clamp(v, -1.0f, 1.0f) * 127.0f));
		uint16 Packed = 0;
		memcpy(&Packed, &Topack, sizeof(Packed));
		return Packed;
	}
	
	GLM_FUNC_QUALIFIER vec2 unpackSnorm2x8(uint16 p)
	{
		i8vec2 Unpack(uninitialize);
		memcpy(&Unpack, &p, sizeof(Unpack));
		return clamp(
			vec2(Unpack) * 0.00787401574803149606299212598425f, // 1.0f / 127.0f
			-1.0f, 1.0f);
	}

	GLM_FUNC_QUALIFIER uint16 packUnorm1x16(float s)
	{
		return static_cast<uint16>(round(clamp(s, 0.0f, 1.0f) * 65535.0f));
	}

	GLM_FUNC_QUALIFIER float unpackUnorm1x16(uint16 p)
	{
		float const Unpack(p);
		return Unpack * 1.5259021896696421759365224689097e-5f; // 1.0 / 65535.0
	}

	GLM_FUNC_QUALIFIER uint64 packUnorm4x16(vec4 const & v)
	{
		u16vec4 const Topack(round(clamp(v , 0.0f, 1.0f) * 65535.0f));
		uint64 Packed = 0;
		memcpy(&Packed, &Topack, sizeof(Packed));
		return Packed;
	}

	GLM_FUNC_QUALIFIER vec4 unpackUnorm4x16(uint64 p)
	{
		u16vec4 Unpack(uninitialize);
		memcpy(&Unpack, &p, sizeof(Unpack));
		return vec4(Unpack) * 1.5259021896696421759365224689097e-5f; // 1.0 / 65535.0
	}

	GLM_FUNC_QUALIFIER uint16 packSnorm1x16(float v)
	{
		int16 const Topack = static_cast<int16>(round(clamp(v ,-1.0f, 1.0f) * 32767.0f));
		uint16 Packed = 0;
		memcpy(&Packed, &Topack, sizeof(Packed));
		return Packed;
	}

	GLM_FUNC_QUALIFIER float unpackSnorm1x16(uint16 p)
	{
		int16 Unpack = 0;
		memcpy(&Unpack, &p, sizeof(Unpack));
		return clamp(
			static_cast<float>(Unpack) * 3.0518509475997192297128208258309e-5f, //1.0f / 32767.0f, 
			-1.0f, 1.0f);
	}

	GLM_FUNC_QUALIFIER uint64 packSnorm4x16(vec4 const & v)
	{
		i16vec4 const Topack(round(clamp(v ,-1.0f, 1.0f) * 32767.0f));
		uint64 Packed = 0;
		memcpy(&Packed, &Topack, sizeof(Packed));
		return Packed;
	}

	GLM_FUNC_QUALIFIER vec4 unpackSnorm4x16(uint64 p)
	{
		i16vec4 Unpack(uninitialize);
		memcpy(&Unpack, &p, sizeof(Unpack));
		return clamp(
			vec4(Unpack) * 3.0518509475997192297128208258309e-5f, //1.0f / 32767.0f,
			-1.0f, 1.0f);
	}

	GLM_FUNC_QUALIFIER uint16 packHalf1x16(float v)
	{
		int16 const Topack(detail::toFloat16(v));
		uint16 Packed = 0;
		memcpy(&Packed, &Topack, sizeof(Packed));
		return Packed;
	}

	GLM_FUNC_QUALIFIER float unpackHalf1x16(uint16 v)
	{
		int16 Unpack = 0;
		memcpy(&Unpack, &v, sizeof(Unpack));
		return detail::toFloat32(Unpack);
	}

	GLM_FUNC_QUALIFIER uint64 packHalf4x16(glm::vec4 const & v)
	{
		i16vec4 const Unpack(
			detail::toFloat16(v.x),
			detail::toFloat16(v.y),
			detail::toFloat16(v.z),
			detail::toFloat16(v.w));
		uint64 Packed = 0;
		memcpy(&Packed, &Unpack, sizeof(Packed));
		return Packed;
	}

	GLM_FUNC_QUALIFIER glm::vec4 unpackHalf4x16(uint64 v)
	{
		i16vec4 Unpack(uninitialize);
		memcpy(&Unpack, &v, sizeof(Unpack));
		return vec4(
			detail::toFloat32(Unpack.x),
			detail::toFloat32(Unpack.y),
			detail::toFloat32(Unpack.z),
			detail::toFloat32(Unpack.w));
	}

	GLM_FUNC_QUALIFIER uint32 packI3x10_1x2(ivec4 const & v)
	{
		detail::i10i10i10i2 Result;
		Result.data.x = v.x;
		Result.data.y = v.y;
		Result.data.z = v.z;
		Result.data.w = v.w;
		return Result.pack; 
	}

	GLM_FUNC_QUALIFIER ivec4 unpackI3x10_1x2(uint32 v)
	{
		detail::i10i10i10i2 Unpack;
		Unpack.pack = v;
		return ivec4(
			Unpack.data.x,
			Unpack.data.y,
			Unpack.data.z,
			Unpack.data.w);
	}

	GLM_FUNC_QUALIFIER uint32 packU3x10_1x2(uvec4 const & v)
	{
		detail::u10u10u10u2 Result;
		Result.data.x = v.x;
		Result.data.y = v.y;
		Result.data.z = v.z;
		Result.data.w = v.w;
		return Result.pack; 
	}

	GLM_FUNC_QUALIFIER uvec4 unpackU3x10_1x2(uint32 v)
	{
		detail::u10u10u10u2 Unpack;
		Unpack.pack = v;
		return uvec4(
			Unpack.data.x,
			Unpack.data.y,
			Unpack.data.z,
			Unpack.data.w);
	}

	GLM_FUNC_QUALIFIER uint32 packSnorm3x10_1x2(vec4 const & v)
	{
		detail::i10i10i10i2 Result;
		Result.data.x = int(round(clamp(v.x,-1.0f, 1.0f) * 511.f));
		Result.data.y = int(round(clamp(v.y,-1.0f, 1.0f) * 511.f));
		Result.data.z = int(round(clamp(v.z,-1.0f, 1.0f) * 511.f));
		Result.data.w = int(round(clamp(v.w,-1.0f, 1.0f) *   1.f));
		return Result.pack;
	}

	GLM_FUNC_QUALIFIER vec4 unpackSnorm3x10_1x2(uint32 v)
	{
		detail::i10i10i10i2 Unpack;
		Unpack.pack = v;
		vec4 Result;
		Result.x = clamp(float(Unpack.data.x) / 511.f, -1.0f, 1.0f);
		Result.y = clamp(float(Unpack.data.y) / 511.f, -1.0f, 1.0f);
		Result.z = clamp(float(Unpack.data.z) / 511.f, -1.0f, 1.0f);
		Result.w = clamp(float(Unpack.data.w) /   1.f, -1.0f, 1.0f);
		return Result;
	}

	GLM_FUNC_QUALIFIER uint32 packUnorm3x10_1x2(vec4 const & v)
	{
		uvec4 const Unpack(round(clamp(v, 0.0f, 1.0f) * vec4(1023.f, 1023.f, 1023.f, 3.f)));

		detail::u10u10u10u2 Result;
		Result.data.x = Unpack.x;
		Result.data.y = Unpack.y;
		Result.data.z = Unpack.z;
		Result.data.w = Unpack.w;
		return Result.pack;
	}

	GLM_FUNC_QUALIFIER vec4 unpackUnorm3x10_1x2(uint32 v)
	{
		vec4 const ScaleFactors(1.0f / 1023.f, 1.0f / 1023.f, 1.0f / 1023.f, 1.0f / 3.f);

		detail::u10u10u10u2 Unpack;
		Unpack.pack = v;
		return vec4(Unpack.data.x, Unpack.data.y, Unpack.data.z, Unpack.data.w) * ScaleFactors;
	}

	GLM_FUNC_QUALIFIER uint32 packF2x11_1x10(vec3 const & v)
	{
		return
			((detail::floatTo11bit(v.x) & ((1 << 11) - 1)) <<  0) |
			((detail::floatTo11bit(v.y) & ((1 << 11) - 1)) << 11) |
			((detail::floatTo10bit(v.z) & ((1 << 10) - 1)) << 22);
	}

	GLM_FUNC_QUALIFIER vec3 unpackF2x11_1x10(uint32 v)
	{
		return vec3(
			detail::packed11bitToFloat(v >> 0),
			detail::packed11bitToFloat(v >> 11),
			detail::packed10bitToFloat(v >> 22));
	}

	GLM_FUNC_QUALIFIER uint32 packF3x9_E1x5(vec3 const & v)
	{
		float const SharedExpMax = (pow(2.0f, 9.0f - 1.0f) / pow(2.0f, 9.0f)) * pow(2.0f, 31.f - 15.f);
		vec3 const Color = clamp(v, 0.0f, SharedExpMax);
		float const MaxColor = max(Color.x, max(Color.y, Color.z));

		float const ExpSharedP = max(-15.f - 1.f, floor(log2(MaxColor))) + 1.0f + 15.f;
		float const MaxShared = floor(MaxColor / pow(2.0f, (ExpSharedP - 16.f - 9.f)) + 0.5f);
		float const ExpShared = MaxShared == pow(2.0f, 9.0f) ? ExpSharedP + 1.0f : ExpSharedP;

		uvec3 const ColorComp(floor(Color / pow(2.f, (ExpShared - 15.f - 9.f)) + 0.5f));

		detail::u9u9u9e5 Unpack;
		Unpack.data.x = ColorComp.x;
		Unpack.data.y = ColorComp.y;
		Unpack.data.z = ColorComp.z;
		Unpack.data.w = uint(ExpShared);
		return Unpack.pack;
	}

	GLM_FUNC_QUALIFIER vec3 unpackF3x9_E1x5(uint32 v)
	{
		detail::u9u9u9e5 Unpack;
		Unpack.pack = v;

		return vec3(Unpack.data.x, Unpack.data.y, Unpack.data.z) * pow(2.0f, Unpack.data.w - 15.f - 9.f);
	}

	template <precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<uint16, P> packHalf(vecType<float, P> const & v)
	{
		return detail::compute_half<P, vecType>::pack(v);
	}

	template <precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<float, P> unpackHalf(vecType<uint16, P> const & v)
	{
		return detail::compute_half<P, vecType>::unpack(v);
	}

	template <typename uintType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<uintType, P> packUnorm(vecType<floatType, P> const & v)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<uintType>::is_integer, "uintType must be an integer type");
		GLM_STATIC_ASSERT(std::numeric_limits<floatType>::is_iec559, "floatType must be a floating point type");

		return vecType<uintType, P>(round(clamp(v, static_cast<floatType>(0), static_cast<floatType>(1)) * static_cast<floatType>(std::numeric_limits<uintType>::max())));
	}

	template <typename uintType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<floatType, P> unpackUnorm(vecType<uintType, P> const & v)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<uintType>::is_integer, "uintType must be an integer type");
		GLM_STATIC_ASSERT(std::numeric_limits<floatType>::is_iec559, "floatType must be a floating point type");

		return vecType<float, P>(v) * (static_cast<floatType>(1) / static_cast<floatType>(std::numeric_limits<uintType>::max()));
	}

	template <typename intType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<intType, P> packSnorm(vecType<floatType, P> const & v)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<intType>::is_integer, "uintType must be an integer type");
		GLM_STATIC_ASSERT(std::numeric_limits<floatType>::is_iec559, "floatType must be a floating point type");

		return vecType<intType, P>(round(clamp(v , static_cast<floatType>(-1), static_cast<floatType>(1)) * static_cast<floatType>(std::numeric_limits<intType>::max())));
	}

	template <typename intType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<floatType, P> unpackSnorm(vecType<intType, P> const & v)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<intType>::is_integer, "uintType must be an integer type");
		GLM_STATIC_ASSERT(std::numeric_limits<floatType>::is_iec559, "floatType must be a floating point type");

		return clamp(vecType<floatType, P>(v) * (static_cast<floatType>(1) / static_cast<floatType>(std::numeric_limits<intType>::max())), static_cast<floatType>(-1), static_cast<floatType>(1));
	}

	GLM_FUNC_QUALIFIER uint8 packUnorm2x4(vec2 const & v)
	{
		u32vec2 const Unpack(round(clamp(v, 0.0f, 1.0f) * 15.0f));
		detail::u4u4 Result;
		Result.data.x = Unpack.x;
		Result.data.y = Unpack.y;
		return Result.pack;
	}

	GLM_FUNC_QUALIFIER vec2 unpackUnorm2x4(uint8 v)
	{
		float const ScaleFactor(1.f / 15.f);
		detail::u4u4 Unpack;
		Unpack.pack = v;
		return vec2(Unpack.data.x, Unpack.data.y) * ScaleFactor;
	}

	GLM_FUNC_QUALIFIER uint16 packUnorm4x4(vec4 const & v)
	{
		u32vec4 const Unpack(round(clamp(v, 0.0f, 1.0f) * 15.0f));
		detail::u4u4u4u4 Result;
		Result.data.x = Unpack.x;
		Result.data.y = Unpack.y;
		Result.data.z = Unpack.z;
		Result.data.w = Unpack.w;
		return Result.pack;
	}

	GLM_FUNC_QUALIFIER vec4 unpackUnorm4x4(uint16 v)
	{
		float const ScaleFactor(1.f / 15.f);
		detail::u4u4u4u4 Unpack;
		Unpack.pack = v;
		return vec4(Unpack.data.x, Unpack.data.y, Unpack.data.z, Unpack.data.w) * ScaleFactor;
	}

	GLM_FUNC_QUALIFIER uint16 packUnorm1x5_1x6_1x5(vec3 const & v)
	{
		u32vec3 const Unpack(round(clamp(v, 0.0f, 1.0f) * vec3(31.f, 63.f, 31.f)));
		detail::u5u6u5 Result;
		Result.data.x = Unpack.x;
		Result.data.y = Unpack.y;
		Result.data.z = Unpack.z;
		return Result.pack;
	}

	GLM_FUNC_QUALIFIER vec3 unpackUnorm1x5_1x6_1x5(uint16 v)
	{
		vec3 const ScaleFactor(1.f / 31.f, 1.f / 63.f, 1.f / 31.f);
		detail::u5u6u5 Unpack;
		Unpack.pack = v;
		return vec3(Unpack.data.x, Unpack.data.y, Unpack.data.z) * ScaleFactor;
	}

	GLM_FUNC_QUALIFIER uint16 packUnorm3x5_1x1(vec4 const & v)
	{
		u32vec4 const Unpack(round(clamp(v, 0.0f, 1.0f) * vec4(31.f, 31.f, 31.f, 1.f)));
		detail::u5u5u5u1 Result;
		Result.data.x = Unpack.x;
		Result.data.y = Unpack.y;
		Result.data.z = Unpack.z;
		Result.data.w = Unpack.w;
		return Result.pack;
	}

	GLM_FUNC_QUALIFIER vec4 unpackUnorm3x5_1x1(uint16 v)
	{
		vec4 const ScaleFactor(1.f / 31.f, 1.f / 31.f, 1.f / 31.f, 1.f);
		detail::u5u5u5u1 Unpack;
		Unpack.pack = v;
		return vec4(Unpack.data.x, Unpack.data.y, Unpack.data.z, Unpack.data.w) * ScaleFactor;
	}

	GLM_FUNC_QUALIFIER uint8 packUnorm2x3_1x2(vec3 const & v)
	{
		u32vec3 const Unpack(round(clamp(v, 0.0f, 1.0f) * vec3(7.f, 7.f, 3.f)));
		detail::u3u3u2 Result;
		Result.data.x = Unpack.x;
		Result.data.y = Unpack.y;
		Result.data.z = Unpack.z;
		return Result.pack;
	}

	GLM_FUNC_QUALIFIER vec3 unpackUnorm2x3_1x2(uint8 v)
	{
		vec3 const ScaleFactor(1.f / 7.f, 1.f / 7.f, 1.f / 3.f);
		detail::u3u3u2 Unpack;
		Unpack.pack = v;
		return vec3(Unpack.data.x, Unpack.data.y, Unpack.data.z) * ScaleFactor;
	}
}//namespace glm

