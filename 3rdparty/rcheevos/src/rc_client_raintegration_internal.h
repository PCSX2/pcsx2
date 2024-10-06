#ifndef RC_CLIENT_RAINTEGRATION_INTERNAL_H
#define RC_CLIENT_RAINTEGRATION_INTERNAL_H

#include "rc_client_raintegration.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

#include "rc_client_external.h"
#include "rc_compat.h"

RC_BEGIN_C_DECLS

/* RAIntegration follows the same calling convention as rcheevos */

typedef void (RC_CCONV* rc_client_raintegration_action_func_t)(void);
typedef const char* (RC_CCONV* rc_client_raintegration_get_string_func_t)(void);
typedef int (RC_CCONV* rc_client_raintegration_init_client_func_t)(HWND hMainWnd, const char* sClientName, const char* sClientVersion);
typedef int (RC_CCONV* rc_client_raintegration_get_external_client_func_t)(rc_client_external_t* pClient, int nVersion);
typedef void (RC_CCONV* rc_client_raintegration_hwnd_action_func_t)(HWND hWnd);
typedef int (RC_CCONV* rc_client_raintegration_get_achievement_state_func_t)(uint32_t nMenuItemId);
typedef const rc_client_raintegration_menu_t* (RC_CCONV* rc_client_raintegration_get_menu_func_t)(void);
typedef int (RC_CCONV* rc_client_raintegration_activate_menuitem_func_t)(uint32_t nMenuItemId);
typedef void (RC_CCONV* rc_client_raintegration_set_write_memory_func_t)(rc_client_t* pClient, rc_client_raintegration_write_memory_func_t handler);
typedef void (RC_CCONV* rc_client_raintegration_set_get_game_name_func_t)(rc_client_t* pClient, rc_client_raintegration_get_game_name_func_t handler);
typedef void (RC_CCONV* rc_client_raintegration_set_event_handler_func_t)(rc_client_t* pClient, rc_client_raintegration_event_handler_t handler);
typedef void (RC_CCONV* rc_client_raintegration_set_int_func_t)(int);
typedef int (RC_CCONV* rc_client_raintegration_get_int_func_t)(void);

typedef struct rc_client_raintegration_t
{
  HINSTANCE hDLL;
  HWND hMainWindow;
  HMENU hPopupMenu;
  uint8_t bIsInited;

  rc_client_raintegration_get_string_func_t get_version;
  rc_client_raintegration_get_string_func_t get_host_url;
  rc_client_raintegration_init_client_func_t init_client;
  rc_client_raintegration_init_client_func_t init_client_offline;
  rc_client_raintegration_set_int_func_t set_console_id;
  rc_client_raintegration_action_func_t shutdown;

  rc_client_raintegration_hwnd_action_func_t update_main_window_handle;

  rc_client_raintegration_set_write_memory_func_t set_write_memory_function;
  rc_client_raintegration_set_get_game_name_func_t set_get_game_name_function;
  rc_client_raintegration_set_event_handler_func_t set_event_handler;
  rc_client_raintegration_get_menu_func_t get_menu;
  rc_client_raintegration_activate_menuitem_func_t activate_menu_item;
  rc_client_raintegration_get_int_func_t has_modifications;
  rc_client_raintegration_get_achievement_state_func_t get_achievement_state;

  rc_client_raintegration_get_external_client_func_t get_external_client;

} rc_client_raintegration_t;

RC_END_C_DECLS

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */

#endif /* RC_CLIENT_RAINTEGRATION_INTERNAL_H */
