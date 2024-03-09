add_library(rcheevos
	include/rcheevos.h
	include/rc_api_editor.h
	include/rc_api_info.h
	include/rc_api_request.h
	include/rc_api_runtime.h
	include/rc_api_user.h
	include/rc_client.h
	include/rc_consoles.h
	include/rc_error.h
	include/rc_hash.h
	include/rc_runtime.h
	include/rc_runtime_types.h
	include/rc_url.h
	include/rc_util.h
	src/rapi/rc_api_common.c
	src/rapi/rc_api_common.h
	src/rapi/rc_api_editor.c
	src/rapi/rc_api_info.c
	src/rapi/rc_api_runtime.c
	src/rapi/rc_api_user.c
	src/rcheevos/alloc.c
	src/rcheevos/condition.c
	src/rcheevos/condset.c
	src/rcheevos/consoleinfo.c
	src/rcheevos/format.c
	src/rcheevos/lboard.c
	src/rcheevos/memref.c
	src/rcheevos/operand.c
	src/rcheevos/rc_internal.h
	src/rcheevos/rc_validate.c
	src/rcheevos/rc_validate.h
	src/rcheevos/richpresence.c
	src/rcheevos/runtime.c
	src/rcheevos/runtime_progress.c
	src/rcheevos/trigger.c
	src/rcheevos/value.c
	src/rc_client.c
	src/rc_client_internal.h
	src/rc_compat.c
	src/rc_compat.h
	src/rc_util.c
	src/rc_version.h
	src/rhash/cdreader.c
	src/rhash/hash.c
	src/rhash/md5.c
	src/rhash/md5.h
	src/rurl/url.c
)

target_include_directories(rcheevos PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(rcheevos INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(rcheevos PRIVATE "RC_DISABLE_LUA=1" "RCHEEVOS_URL_SSL")

