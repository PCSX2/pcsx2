# v11.1.0
* add rc_client_get_user_agent_clause to generate substring to include in client User-Agents
* add rc_client_can_pause function to control pause spam
* add achievement type and rarity to rc_api_fetch_game_data_response_t and rc_client_achievement_t
* add RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED for achievements that have been unlocked locally but not synced to the server
* add RC_CONSOLE_NEO_GEO_CD to supported consoles for chd file extension
* add hash logic for RC_CONSOLE_NINTENDO_3DS (note: added new file rhash/aes.c to support this)
* add hash logic for RC_CONSOLE_MS_DOS
* add game_hash and hardcore fields to rc_api_start_session_request_t and rc_api_ping_request_t
* add RC_FORMAT_FIXED1/2/3, RC_FORMAT_TENS, RC_FORMAT_HUNDREDS, RC_FORMAT_THOUSANDS, and RC_FORMAT_UNSIGNED_VALUE
* add RC_CONSOLE_STANDALONE
* add extern "C" and __cdecl attributes to public functions
* add __declspec(dllexport/dllimport) attributes to public functions via #define enablement
* add rc_version and rc_version_string functions for accessing version from external linkage
* add unicode path support to default filereader (Windows builds)
* add rc_mutex support for GEKKO (libogc)
* fix async_handle being returned when rc_client_begin_login is aborted synchronously
* fix logic error hashing CD files smaller than one sector
* fix read across region boundary in rc_libretro_memory_read
* fix RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW event not being raised if achievement is reset in the same frame that it's primed
* moved rc_util.h from src/ to include/
* initial (incomplete) support for rc_client_external_t and rc_client_raintegration_t

# v11.0.0
* add rc_client_t and related functions
* add RC_MEMSIZE_FLOAT_BE
* add Game Pak SRAM to GBA memory map
* add hash method for Super Cassettevision
* add PSP to potential consoles for chd iterator
* add content_type to rc_api_request_t for client to pass to server
* add rc_api_process_X_server_response methods to pass status_code and body_length to response processing functions
* add additional error codes to rc_api_process_login_response: RC_INVALID_CREDENTIALS, RC_EXPIRED_TOKEN, RC_ACCESS_DENIED
* rc_api_start_session now also returns unlocks without having to explicitly call rc_api_fetch_user_unlocks separately
* add validation warning for using hit target of 1 on ResetIf condition
* move compat.c up a directory and rename to rc_compat.c as it's shared by all subfolders
* move rc_libretro.c up a directory as it uses files from all subfolders
* convert loosely sized types to strongly sized types (unsigned -> uint32t, unsigned char -> uint8_t, etc)

# v10.7.1
* add rc_runtime_alloc
* add rc_libretro_memory_find_avail
* extract nginx errors from HTML returned for JSON endpoints
* fix real address for 32X extension RAM
* fix crash attempting to calculate gamecube hash for non-existent file

# v10.7.0
* add hash method and memory map for Gamecube
* add console enum, hash method, and memory map for DSi
* add console enum, hash method, and memory map for TI-83
* add console enum, hash method, and memory map for Uzebox
* add constant for rcheevos version; include in start session server API call
* fix SubSource calculations using float values
* fix game identification for homebrew Jaguar CD games
* fix game identification for CD with many files at root directory
* address _CRT_SECURE_NO_WARNINGS warnings

# v10.6.0
* add RC_RUNTIME_EVENT_ACHIEVEMENT_PROGRESS_UPDATED
* use optimized comparators for most common condition logic
* fix game identification of psx ISOs that have extra slashes in their boot path
* fix game identification of ndd files

# v10.5.0
* add RC_MEMSIZE_MBF32_LE
* add RC_OPERATOR_XOR
* add RC_CONSOLE_ATARI_JAGUAR_CD and hash/memory map for Atari Jaguar CD
* add RC_CONSOLE_ARCADIA_2001 and hash/memory map for Arcadia 2001
* add RC_CONSOLE_INTERTON_VC_4000 and hash/memory map for Interton VC 4000
* add RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER and hash/memory map for Elektor TV Games Computer
* split RC_CONSOLE_PC_ENGINE_CD off of RC_CONSOLE_PC_ENGINE
* add hash/memory map for RC_CONSOLE_NEO_GEO_CD
* add additional 256KB of RAM to memory map for RC_CONSOLE_SEGA_32X
* validation: don't report redundancy between trigger and non-trigger conditions
* validation: don't report range validation errors for float comparisons
* change default image host to media.retroachievements.org
* fix decoding of denormalized floats
* fix full line comments in the middle of Display: section causing RC_MISSING_DISPLAY_STRING

# v10.4.0
* add rc_libretro_hash_set_t with support for #SAVEDISK: m3u extension
* add rc_libretro_is_system_allowed for finer-grain control over core support
* fix measured value from hitcount not resetting while paused
* add RC_CONSOLE_WASM and hash/memory map for WASM-4
* add scratchpad memory to RC_CONSOLE_PLAYSTATION_2 memory map
* add hash/memory map for RC_CONSOLE_FAIRCHILD_CHANNEL_F
* add hash/memory map for RC_CONSOLE_COMMODORE_64
* add memory map for RC_CONSOLE_AMIGA

# v10.3.3
* add RC_CONSOLE_ARDUBOY and hash/memory map for Arduboy
* add display_name to rc_api_login_response_t
* detect logical conflicts and redundancies in validator
* fix tab sequences in JSON responses being turned into t
* fix overflow when float value has more than 9 digits after the decimal
* fix libretro memory mapping when disconnect mask breaks a region into multiple blocks
* fix non-virtualized file system call when reading some iso files

# v10.3.2
* fix RC_OPERAND_PRIOR for bit sizes other than RC_MEMSIZE_BIT_0
* add memory map and hash for Amstrad CPC
* fix an issue where fetch_game_data and fetch_user_unlocks could return RC_MISSING_VALUE instead of acknowledging a server error

# v10.3.1
* allow empty description in rc_api_init_update_leaderboard_request
* fix buffered n64 hash when no filereader is registered
* add memory map and hash for Mega Duck

# v10.3.0
* support for floating point memory sizes and logic
* add built-in macros for rich presence: @Number, @Score, @Centisecs, @Seconds, @Minutes, @ASCIIChar, @UnicodeChar
* add rapi functions for fetch_code_notes, update_code_note, upload_achievement, update_leaderboard, fetch_badge_range, and add_game_hash
* add lower_is_better and hidden flags to leaderboards in rc_api_fetch_game_data_response_t
* add achievements_remaining to rc_api_award_achievement_response_t
* add console enums for PC6000, PICO, MEGADUCK and ZEEBO
* add memory map for Dreamcast
* capture leaderboard/rich presence state in rc_runtime_progress data
* support for hashing Dreamcast bin/cues
* support for hashing buffered NDS ROMs
* fix prior for sizes smaller than a byte sometimes returning current value

# v10.2.0

* add RC_MEMSIZE_16_BITS_BE, RC_MEMSIZE_24_BITS_BE, and RC_MEMSIZE_32_BITS_BE
* add secondary flag for RC_CONDITION_MEASURED that tells the UI when to show progress as raw vs. as a percentage
* add rapi calls for fetch_leaderboard_info, fetch_achievement_info and fetch_game_list
* add hash support for RC_CONSOLE_PSP
* add RCHEEVOS_URL_SSL compile flag to use https in rurl functions
* add space to "PC Engine" label
* update RC_CONSOLE_INTELLIVISION memory map to acknowledge non-8-bit addresses
* standardize to z64 format when hashing RC_CONSOLE_N64
* prevent generating hash for PSX disc when requesting RC_CONSOLE_PLAYSTATION2
* fix wrong error message being returned when a leaderboard was only slightly malformed

# v10.1.0

* add RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED
* add rc_runtime_validate_addresses
* add external memory to memory map for Magnavox Odyssey 2
* fix memory map base address for NeoGeo Pocket
* fix bitcount always returning 0 when used in rich presence

# v10.0.0

* add rapi sublibrary for communicating with server (eliminates need for client-side JSON parsing; client must still
  provide HTTP functionality). rurl is now deprecated
* renamed 'rhash.h' to 'rc_hash.h' to eliminate conflict with system headers, renamed 'rconsoles.h' and 'rurl.h' for 
  consistency
* split non-runtime functions out of 'rcheevos.h' as they're not needed by most clients
* allow ranges in rich presence lookups
* add rc_richpresence_size_lines function to fetch line associated to error when processing rich presence script
* add rc_runtime_invalidate_address function to disable achievements when an unknown address is queried
* add RC_CONDITION_RESET_NEXT_IF
* add RC_CONDITION_SUB_HITS
* support MAXOF operator ($) for leaderboard values using trigger syntax
* allow RC_CONDITION_PAUSE_IF and RC_CONDITION_RESET_IF in leaderboard value expression
* changed track parameter of rc_hash_cdreader_open_track_handler to support three virtual tracks:
  RC_HASH_CDTRACK_FIRST_DATA, RC_HASH_CDTRACK_LAST and RC_HASH_CDTRACK_LARGEST.
* changed offset parameter of rc_hash_filereader_seek_handler and return value of rc_hash_filereader_tell_handler
  from size_t to int64_t to support files larger than 2GB when compiling in 32-bit mode.
* reset to default cd reader if NULL is passed to rc_hash_init_custom_cdreader
* add hash support for RC_CONSOLE_DREAMCAST, RC_CONSOLE_PLAYSTATION_2, RC_CONSOLE_SUPERVISION, and RC_CONSOLE_TIC80
* ignore headers when generating hashs for RC_CONSOLE_PC_ENGINE and RC_CONSOLE_ATARI_7800
* require unique identifier when hashing RC_CONSOLE_SEGA_CD and RC_CONSOLE_SATURN discs
* add expansion memory to RC_CONSOLE_SG1000 memory map
* rename RC_CONSOLE_MAGNAVOX_ODYSSEY -> RC_CONSOLE_MAGNAVOX_ODYSSEY2
* rename RC_CONSOLE_AMIGA_ST -> RC_CONSOLE_ATARI_ST
* add RC_CONSOLE_SUPERVISION, RC_CONSOLE_SHARPX1, RC_CONSOLE_TIC80, RC_CONSOLE_THOMSONTO8
* fix error identifying largest track when track has multiple bins
* fix memory corruption error when cue track has more than 6 INDEXs
* several improvements to data storage for conditions (rc_memref_t and rc_memref_value_t structures have been modified)

# v9.2.0

* fix issue identifying some PC-FX titles where the boot code is not in the first data track
* add enums and labels for RC_CONSOLE_MAGNAVOX_ODYSSEY, RC_CONSOLE_SUPER_CASSETTEVISION, RC_CONSOLE_NEO_GEO_CD,
  RC_CONSOLE_FAIRCHILD_CHANNEL_F, RC_CONSOLE_FM_TOWNS, RC_CONSOLE_ZX_SPECTRUM, RC_CONSOLE_GAME_AND_WATCH,
  RC_CONSOLE_NOKIA_NGAGE, RC_CONSOLE_NINTENDO_3DS

# v9.1.0

* add hash support and memory map for RC_CONSOLE_MSX
* add hash support and memory map for RC_CONSOLE_PCFX
* include parent directory when hashing non-arcade titles in arcade mode
* support absolute paths in m3u
* make cue scanning case-insensitive
* expand SRAM mapping for RC_CONSOLE_WONDERSWAN
* fix display of measured value when another group has an unmeasured hit count
* fix memory read error when hashing file with no extension
* fix possible divide by zero when using RC_CONDITION_ADD_SOURCE/RC_CONDITION_SUB_SOURCE
* fix classification of secondary RC_CONSOLE_SATURN memory region

# v9.0.0

* new size: RC_MEMSIZE_BITCOUNT
* new flag: RC_CONDITION_OR_NEXT
* new flag: RC_CONDITION_TRIGGER
* new flag: RC_CONDITION_MEASURED_IF
* new operators: RC_OPERATOR_MULT / RC_OPERATOR_DIV
* is_bcd removed from memref - now part of RC_MEMSIZE
* add rc_runtime_t and associated functions
* add rc_hash_ functions
* add rc_error_str function
* add game_hash parameter to rc_url_award_cheevo
* remove hash parameter from rc_url_submit_lboard
* add rc_url_ping function
* add rc_console_ functions

# v8.1.0

* new flag: RC_CONDITION_MEASURED
* new flag: RC_CONDITION_ADD_ADDRESS
* add rc_evaluate_trigger - extended version of rc_test_trigger with more granular return codes
* make rc_evaluate_value return a signed int (was unsigned int)
* new formats: RC_FORMAT_MINUTES and RC_FORMAT_SECONDS_AS_MINUTES
* removed " Points" text from RC_FORMAT_SCORE format
* removed RC_FORMAT_OTHER format. "OTHER" format now parses to RC_FORMAT_SCORE
* bugfix: AddHits will now honor AndNext on previous condition

# v8.0.1

* bugfix: prevent null reference exception if rich presence contains condition without display string
* bugfix: 24-bit read from memory should only read 24-bits

# v8.0.0

* support for prior operand type
* support for AndNext condition flag
* support for rich presence
* bugfix: update delta/prior memory values while group is paused
* bugfix: allow floating point number without leading 0
* bugfix: support empty alt groups

# v7.1.1

* Address signed/unsigned mismatch warnings

# v7.1.0

* Added the RC_DISABLE_LUA macro to compile rcheevos without Lua support

# v7.0.2

* Make sure the code is C89-compliant
* Use 32-bit types in Lua
* Only evaluate Lua operands when the Lua state is not `NULL`

# v7.0.1

* Fix the alignment of memory allocations

# v7.0.0

* Removed **rjson**

# v6.5.0

* Added a schema for errors returned by the server

# v6.4.0

* Added an enumeration with the console identifiers used in RetroAchievements

# v6.3.1

* Pass the peek function and the user data to the Lua functions used in operands.

# v6.3.0

* Added **rurl**, an API to build URLs to access RetroAchievements web services.

# v6.2.0

* Added **rjson**, an API to easily decode RetroAchievements JSON files into C structures.

# v6.1.0

* Added support for 24-bit operands with the `'W'` prefix (`RC_OPERAND_24_BITS`)

# v6.0.2

* Only define RC_ALIGNMENT if it has not been already defined

# v6.0.1

* Use `sizeof(void*)` as a better default for `RC_ALIGNMENT`

# v6.0.0

* Simplified API: separate functions to get the buffer size and to parse `memaddr` into the provided buffer
* Fixed crash trying to call `rc_update_condition_pause` during a dry-run
* The callers are now responsible to pass down a scratch buffer to avoid accesses to out-of-scope memory

# v5.0.0

* Pre-compute if a condition has a pause condition in its group
* Added a pre-computed flag that tells if the condition set has at least one pause condition
* Removed the link to the previous condition in a condition set chain

# v4.0.0

* Fixed `ret` not being properly initialized in `rc_parse_trigger`
* Build the unit tests with optimizations and `-Wall` to help catch more issues
* Added `extern "C"` around the inclusion of the Lua headers so that **rcheevos** can be compiled cleanly as C++
* Exposed `rc_parse_value` and `rc_evaluate_value` to be used with rich presence
* Removed the `reset` and `dirty` flags from the external API

# v3.2.0

* Added the ability to reset triggers and leaderboards
* Add a function to parse a format string and return the format enum, and some unit tests for it

# v3.1.0

* Added `rc_format_value` to the API

# v3.0.1

* Fixed wrong 32-bit value on 64-bit platforms

# v3.0.0

* Removed function rc_evaluate_value from the API

# v2.0.0

* Removed leaderboard callbacks in favor of a simpler scheme

# v1.1.2

* Fixed NULL pointer deference when there's an error during the parse

# v1.1.1

* Removed unwanted garbage
* Should be v1.0.1 :/

# v1.0.0

* First version
