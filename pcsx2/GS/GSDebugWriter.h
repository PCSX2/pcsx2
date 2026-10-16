#include "common/Pcsx2Defs.h"
#include "GS/GSVector.h"

#include "fmt/format.h"

#include <type_traits>

struct GSDebugWriter
{
	fmt::memory_buffer buffer;
	u32 indent = 0;
	bool beginning_of_line = true;

	/// Uses RAII to add 1 to indent on construction and remove on destruction
	struct RAIIIndent
	{
		GSDebugWriter& writer;
		RAIIIndent(RAIIIndent&&) = delete;
		explicit RAIIIndent(GSDebugWriter& writer) : writer(writer) { writer.PushIndent(); }
		~RAIIIndent() { writer.PopIndent(); }
		operator GSDebugWriter&() { return writer; }
	};

	void PushIndent() { indent++; }
	void PopIndent() { indent--; }
	RAIIIndent WithIndent() { return RAIIIndent(*this); }

	template <typename... T>
	FMT_INLINE void WriteLn(fmt::format_string<T...> fmt, const T&... args)
	{
		fmt::vargs<T...> va = {{args...}};
		if (beginning_of_line)
			WriteIndent();
		beginning_of_line = true;
		fmt::detail::vformat_to(buffer, fmt.str, va);
		buffer.push_back('\n');
	}

	template <typename... T>
	FMT_INLINE void Write(fmt::format_string<T...> fmt, const T&... args)
	{
		fmt::vargs<T...> va = {{args...}};
		if (beginning_of_line)
			WriteIndent();
		beginning_of_line = false;
		fmt::detail::vformat_to(buffer, fmt.str, va);
	}

	template<typename T>
		requires (std::is_same_v<std::remove_cvref_t<T>, GSVector4> ||
			std::is_same_v<std::remove_cvref_t<T>, GSVector4i>)
	void WriteVector4(const char* name, const T& val)
	{
		WriteLn("{}: [{}, {}, {}, {}]", name, val.x, val.y, val.z, val.w);
	};

	template<typename T>
		requires (std::is_same_v<std::remove_cvref_t<T>, GSVector2> ||
			std::is_same_v<std::remove_cvref_t<T>, GSVector2i>)
	void WriteVector2(const char* name, const T& val)
	{
		WriteLn("{}: [{}, {}]", name, val.x, val.y);
	};

private:
	void WriteIndent()
	{
		size_t sz = buffer.size();
		buffer.resize(sz + indent);
		for (u32 i = 0; i < indent; i++)
			buffer[sz + i] = '\t';
	}
};