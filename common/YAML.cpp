// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "YAML.h"

#include "Assertions.h"

#if RYML_VERSION_MAJOR > 0 || RYML_VERSION_MINOR >= 11
#include "c4/yml/error.def.hpp" // for ryml::err_basic_format etc
#endif

#include <csetjmp>

struct RapidYAMLContext
{
	std::jmp_buf env;
	Error* error = nullptr;
};

std::optional<ryml::Tree> ParseYAMLFromString(ryml::csubstr yaml, ryml::csubstr file_name, Error* error)
{
	RapidYAMLContext context;
	context.error = error;

	ryml::Callbacks callbacks;

#if RYML_VERSION_MAJOR > 0 || RYML_VERSION_MINOR >= 11
	callbacks.set_user_data(static_cast<void*>(&context));

	callbacks.set_error_basic([](ryml::csubstr msg, const ryml::ErrorDataBasic& errdata, void* user_data) {
		std::string description;
		auto callback = [&description](ryml::csubstr string) {
			description.append(string.str, string.len);
		};
		ryml::err_basic_format(std::move(callback), msg, errdata);

		// We might have already returned, so don't try to recover.
		pxFailRel(description.c_str());
		std::abort();
	});

	callbacks.set_error_parse([](ryml::csubstr msg, const ryml::ErrorDataParse& errdata, void* user_data) {
		RapidYAMLContext* context = static_cast<RapidYAMLContext*>(user_data);

		std::string description;
		auto callback = [&description](ryml::csubstr string) {
			description.append(string.str, string.len);
		};
		ryml::err_parse_format(std::move(callback), msg, errdata);

		Error::SetString(context->error, std::move(description));
		std::longjmp(context->env, 1);
	});

	callbacks.set_error_visit([](ryml::csubstr msg, const ryml::ErrorDataVisit& errdata, void* user_data) {
		std::string description;
		auto callback = [&description](ryml::csubstr string) {
			description.append(string.str, string.len);
		};
		ryml::err_visit_format(std::move(callback), msg, errdata);

		// We've probably already returned, so don't try to recover.
		pxFailRel(description.c_str());
		std::abort();
	});
#else
	callbacks.m_user_data = static_cast<void*>(&context);
	callbacks.m_error = [](const char* msg, size_t msg_len, ryml::Location location, void* user_data) {
		RapidYAMLContext* context = static_cast<RapidYAMLContext*>(user_data);

		Error::SetString(context->error, std::string(msg, msg_len));
		std::longjmp(context->env, 1);
	};
#endif

	ryml::EventHandlerTree event_handler(callbacks);
	ryml::Parser parser(&event_handler);

	ryml::Tree tree;

	// The only options RapidYAML provides for recovering from errors are
	// throwing an exception or using setjmp/longjmp. Since we have exceptions
	// disabled we have to use the latter option.
	if (setjmp(context.env))
		return std::nullopt;

	ryml::parse_in_arena(&parser, file_name, yaml, &tree);

	return tree;
}
