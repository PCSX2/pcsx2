# **rcheevos**

**rcheevos** is a set of C code, or a library if you will, that tries to make it easier for emulators to process [RetroAchievements](https://retroachievements.org) data, providing support for achievements and leaderboards for their players.

Keep in mind that **rcheevos** does *not* provide HTTP network connections. Clients must get data from RetroAchievements, and pass the response down to **rcheevos** for processing.

Not all structures defined by **rcheevos** can be created via the public API, but are exposed to allow interactions beyond just creation, destruction, and testing, such as the ones required by UI code that helps to create them.

## Lua

RetroAchievements previously considered the use of the [Lua](https://www.lua.org) language to expand the syntax supported for creating achievements.

To enable Lua support, you must compile with an additional compilation flag: `HAVE_LUA`, as neither the backend nor the UI for editing achievements are currently Lua-enabled. We do not foresee enabling it any time soon, but the code has not yet been completely eliminated as many of the low-level API fuctions have parameters for LUA data.

> **rcheevos** does *not* create or maintain a Lua state, you have to create your own state and provide it to **rcheevos** to be used when Lua-coded achievements are found. Calls to **rcheevos** may allocate and/or free additional memory as part of the Lua runtime.

Lua functions used in trigger operands receive two parameters: `peek`, which is used to read from the emulated system's memory, and `userdata`, which must be passed to `peek`. `peek`'s signature is the same as its C counterpart:

```lua
function peek(address, num_bytes, userdata)
```

## API

An understanding about how achievements are developed may be useful, you can read more about it [here](http://docs.retroachievements.org/Developer-docs/).

Most of the exposed APIs are documented [here](https://github.com/RetroAchievements/rcheevos/wiki)

### Return values

Any function in the rcheevos library that returns a success indicator will return `RC_OK` or one of the constants defined in `rc_error.h`.

To convert the return code into something human-readable, pass it to:
```c
const char* rc_error_str(int ret);
```

### Console identifiers

Platforms supported by RetroAchievements are enumerated in `rc_consoles.h`. Note that some consoles in the enum are not yet fully supported (may require a memory map or some way to uniquely identify games).

## Runtime support

Provides a set of functions for managing an active game - initializing and processing achievements, leaderboards, and rich presence. When important things occur, events are raised for the caller via a callback.

These are in `rc_runtime.h`.

Note: `rc_runtime_t` still requires the client implement all of the logic that calls the APIs to retrieve the data and perform the unlocks.

The `rc_client_t` functions wrap a `rc_runtime_t` and manage the API calls and other common functionality (like managing the user information, identifying/loading a game, and building the active/inactive achievements list for the UI). Please see [the wiki](https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration) for details on using the `rc_client_t` functions.

## Server Communication

**rapi** builds URLs to access many RetroAchievements web services. Its purpose it to just to free the developer from having to URL-encode parameters and build correct URLs that are valid for the server.

**rapi** does *not* make HTTP requests.

NOTE: **rapi** is a replacement for **rurl**. **rurl** has been deprecated.

NOTE: `rc_client` is the preferred way to have a client interact with the server.

These are in `rc_api_user.h`, `rc_api_runtime.h` and `rc_api_common.h`.

The basic process of making an **rapi** call is to initialize a params object, call a function to convert it to a URL, send that to the server, then pass the response to a function to convert it into a response object, and handle the response values.

An example can be found on the [rc_api_init_login_request](https://github.com/RetroAchievements/rcheevos/wiki/rc_api_init_login_request#example) page.

### Functions

Please see the [wiki](https://github.com/RetroAchievements/rcheevos/wiki) for details on the functions exposed for **rapi**.

## Game Identification

**rhash** provides logic for generating a RetroAchievements hash for a given game. There are two ways to use the API - you can pass the filename and let rhash open and process the file, or you can pass the buffered copy of the file to rhash if you've already loaded it into memory.

These are in `rc_hash.h`.

```c
  int rc_hash_generate_from_buffer(char hash[33], int console_id, uint8_t* buffer, size_t buffer_size);
  int rc_hash_generate_from_file(char hash[33], int console_id, const char* path);
```
