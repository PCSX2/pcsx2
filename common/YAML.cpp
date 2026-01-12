// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "YAML.h"

#include <csetjmp>
#include <cstdlib>

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
	callbacks.m_user_data = static_cast<void*>(&context);
	callbacks.m_error = [](const char* msg, size_t msg_len, ryml::Location location, void* user_data) {
		RapidYAMLContext* context = static_cast<RapidYAMLContext*>(user_data);

		Error::SetString(context->error, std::string(msg, msg_len));
		std::longjmp(context->env, 1);
	};

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
