// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS.h"
#include "GS/Renderers/HW/GSTextureReplacements.h"
#include "Host.h"
#include "LayeredSettingsInterface.h"
#include "VMManager.h"
#include "svnrev.h"

#include "common/Assertions.h"
#include "common/CrashHandler.h"
#include "common/FileSystem.h"
#include "common/HeterogeneousContainers.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

#include <cstdarg>
#include <shared_mutex>

namespace Host
{
	static std::pair<const char*, u32> LookupTranslationString(
		const std::string_view& context, const std::string_view& msg);

	static std::mutex s_settings_mutex;
	static LayeredSettingsInterface s_layered_settings_interface;

	static constexpr u32 TRANSLATION_STRING_CACHE_SIZE = 4 * 1024 * 1024;
	using TranslationStringMap = UnorderedStringMap<std::pair<u32, u32>>;
	using TranslationStringContextMap = UnorderedStringMap<TranslationStringMap>;
	static std::shared_mutex s_translation_string_mutex;
	static TranslationStringContextMap s_translation_string_map;
	static std::vector<char> s_translation_string_cache;
	static u32 s_translation_string_cache_pos;
} // namespace Host

std::pair<const char*, u32> Host::LookupTranslationString(const std::string_view& context, const std::string_view& msg)
{
	// TODO: TranslatableString, compile-time hashing.

	TranslationStringContextMap::iterator ctx_it;
	TranslationStringMap::iterator msg_it;
	std::pair<const char*, u32> ret;
	s32 len;

	// Shouldn't happen, but just in case someone tries to translate an empty string.
	if (msg.empty()) [[unlikely]]
	{
		ret.first = &s_translation_string_cache[0];
		ret.second = 0;
		return ret;
	}

	s_translation_string_mutex.lock_shared();
	ctx_it = s_translation_string_map.find(context);

	if (ctx_it == s_translation_string_map.end()) [[unlikely]]
		goto add_string;

	msg_it = ctx_it->second.find(msg);
	if (msg_it == ctx_it->second.end()) [[unlikely]]
		goto add_string;

	ret.first = &s_translation_string_cache[msg_it->second.first];
	ret.second = msg_it->second.second;
	s_translation_string_mutex.unlock_shared();
	return ret;

add_string:
	s_translation_string_mutex.unlock_shared();
	s_translation_string_mutex.lock();

	if (s_translation_string_cache.empty()) [[unlikely]]
	{
		// First element is always an empty string.
		s_translation_string_cache.resize(TRANSLATION_STRING_CACHE_SIZE);
		s_translation_string_cache[0] = '\0';
		s_translation_string_cache_pos = 0;
	}

	if ((len = Internal::GetTranslatedStringImpl(context, msg,
			 &s_translation_string_cache[s_translation_string_cache_pos],
			 TRANSLATION_STRING_CACHE_SIZE - 1 - s_translation_string_cache_pos)) < 0)
	{
		Console.Error("WARNING: Clearing translation string cache, it might need to be larger.");
		s_translation_string_cache_pos = 0;
		if ((len = Internal::GetTranslatedStringImpl(context, msg,
				 &s_translation_string_cache[s_translation_string_cache_pos],
				 TRANSLATION_STRING_CACHE_SIZE - 1 - s_translation_string_cache_pos)) < 0)
		{
			pxFailRel("Failed to get translated string after clearing cache.");
			len = 0;
		}
	}

	// New context?
	if (ctx_it == s_translation_string_map.end())
		ctx_it = s_translation_string_map.emplace(context, TranslationStringMap()).first;

	// Impl doesn't null terminate, we need that for C strings.
	// TODO: do we want to consider aligning the buffer?
	const u32 insert_pos = s_translation_string_cache_pos;
	s_translation_string_cache[insert_pos + static_cast<u32>(len)] = 0;

	ctx_it->second.emplace(msg, std::pair<u32, u32>(insert_pos, static_cast<u32>(len)));
	s_translation_string_cache_pos = insert_pos + static_cast<u32>(len) + 1;

	ret.first = &s_translation_string_cache[insert_pos];
	ret.second = static_cast<u32>(len);
	s_translation_string_mutex.unlock();
	return ret;
}

const char* Host::TranslateToCString(const std::string_view& context, const std::string_view& msg)
{
	return LookupTranslationString(context, msg).first;
}

std::string_view Host::TranslateToStringView(const std::string_view& context, const std::string_view& msg)
{
	const auto mp = LookupTranslationString(context, msg);
	return std::string_view(mp.first, mp.second);
}

std::string Host::TranslateToString(const std::string_view& context, const std::string_view& msg)
{
	return std::string(TranslateToStringView(context, msg));
}

void Host::ClearTranslationCache()
{
	s_translation_string_mutex.lock();
	s_translation_string_map.clear();
	s_translation_string_cache_pos = 0;
	s_translation_string_mutex.unlock();
}

void Host::ReportFormattedErrorAsync(const std::string_view& title, const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);
	std::string message(StringUtil::StdStringFromFormatV(format, ap));
	va_end(ap);
	ReportErrorAsync(title, message);
}

bool Host::ConfirmFormattedMessage(const std::string_view& title, const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);
	std::string message = StringUtil::StdStringFromFormatV(format, ap);
	va_end(ap);

	return ConfirmMessage(title, message);
}

std::string Host::GetHTTPUserAgent()
{
	return fmt::format("PCSX2 " GIT_REV " ({})", GetOSVersionString());
}

std::unique_lock<std::mutex> Host::GetSettingsLock()
{
	return std::unique_lock<std::mutex>(s_settings_mutex);
}

SettingsInterface* Host::GetSettingsInterface()
{
	return &s_layered_settings_interface;
}

SettingsInterface* Host::GetSettingsInterfaceForBindings()
{
	SettingsInterface* input_layer = s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_INPUT);
	return input_layer ? input_layer : &s_layered_settings_interface;
}

std::string Host::GetBaseStringSettingValue(const char* section, const char* key, const char* default_value /*= ""*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->GetStringValue(section, key, default_value);
}

bool Host::GetBaseBoolSettingValue(const char* section, const char* key, bool default_value /*= false*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->GetBoolValue(section, key, default_value);
}

int Host::GetBaseIntSettingValue(const char* section, const char* key, int default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->GetIntValue(section, key, default_value);
}

uint Host::GetBaseUIntSettingValue(const char* section, const char* key, uint default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->GetUIntValue(section, key, default_value);
}

float Host::GetBaseFloatSettingValue(const char* section, const char* key, float default_value /*= 0.0f*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->GetFloatValue(section, key, default_value);
}

double Host::GetBaseDoubleSettingValue(const char* section, const char* key, double default_value /* = 0.0f */)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->GetDoubleValue(section, key, default_value);
}

std::vector<std::string> Host::GetBaseStringListSetting(const char* section, const char* key)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetStringList(section, key);
}

void Host::SetBaseBoolSettingValue(const char* section, const char* key, bool value)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->SetBoolValue(section, key, value);
}

void Host::SetBaseIntSettingValue(const char* section, const char* key, int value)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->SetIntValue(section, key, value);
}

void Host::SetBaseUIntSettingValue(const char* section, const char* key, uint value)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->SetUIntValue(section, key, value);
}

void Host::SetBaseFloatSettingValue(const char* section, const char* key, float value)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->SetFloatValue(section, key, value);
}

void Host::SetBaseStringSettingValue(const char* section, const char* key, const char* value)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->SetStringValue(section, key, value);
}

void Host::SetBaseStringListSettingValue(const char* section, const char* key, const std::vector<std::string>& values)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->SetStringList(section, key, values);
}

bool Host::AddBaseValueToStringList(const char* section, const char* key, const char* value)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->AddToStringList(section, key, value);
}

bool Host::RemoveBaseValueFromStringList(const char* section, const char* key, const char* value)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)
		->RemoveFromStringList(section, key, value);
}

bool Host::ContainsBaseSettingValue(const char* section, const char* key)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->ContainsValue(section, key);
}

void Host::RemoveBaseSettingValue(const char* section, const char* key)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->DeleteValue(section, key);
}

std::string Host::GetStringSettingValue(const char* section, const char* key, const char* default_value /*= ""*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetStringValue(section, key, default_value);
}

bool Host::GetBoolSettingValue(const char* section, const char* key, bool default_value /*= false*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetBoolValue(section, key, default_value);
}

int Host::GetIntSettingValue(const char* section, const char* key, int default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetIntValue(section, key, default_value);
}

uint Host::GetUIntSettingValue(const char* section, const char* key, uint default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetUIntValue(section, key, default_value);
}

float Host::GetFloatSettingValue(const char* section, const char* key, float default_value /*= 0.0f*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetFloatValue(section, key, default_value);
}

double Host::GetDoubleSettingValue(const char* section, const char* key, double default_value /*= 0.0f*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetDoubleValue(section, key, default_value);
}

std::vector<std::string> Host::GetStringListSetting(const char* section, const char* key)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetStringList(section, key);
}

SettingsInterface* Host::Internal::GetBaseSettingsLayer()
{
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE);
}

SettingsInterface* Host::Internal::GetGameSettingsLayer()
{
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_GAME);
}

SettingsInterface* Host::Internal::GetInputSettingsLayer()
{
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_INPUT);
}

void Host::Internal::SetBaseSettingsLayer(SettingsInterface* sif)
{
	pxAssertRel(s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE) == nullptr,
		"Base layer has not been set");
	s_layered_settings_interface.SetLayer(LayeredSettingsInterface::LAYER_BASE, sif);
}

void Host::Internal::SetGameSettingsLayer(SettingsInterface* sif)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.SetLayer(LayeredSettingsInterface::LAYER_GAME, sif);
}

void Host::Internal::SetInputSettingsLayer(SettingsInterface* sif)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.SetLayer(LayeredSettingsInterface::LAYER_INPUT, sif);
}
