# **rcheevos**

**rcheevos** is a set of C code, or a library if you will, that tries to make it easier for emulators to process [RetroAchievements](https://retroachievements.org) data, providing support for achievements and leaderboards for their players.

Keep in mind that **rcheevos** does *not* provide HTTP network connections. Clients must get data from RetroAchievements, and pass the response down to **rcheevos** for processing.

Not all structures defined by **rcheevos** can be created via the public API, but are exposed to allow interactions beyond just creation, destruction, and testing, such as the ones required by UI code that helps to create them.

**NOTE**: development occurs on the _develop_ branch, which is set as the default branch in GitHub so newly opened PRs will request to be merged into the _develop_ branch. When integrating **rcheevos** into your project, we recommend using the _master_ branch, which corresponds to the last official release, and minimizes the risk of encountering a bug that has been introduced since the last official release.

## API

An understanding about how achievements are developed may be useful, you can read more about it [here](https://docs.retroachievements.org/developer-docs/).

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

A set of functions for managing an active game is provided by the library. If you are considering adding achievement support to your emulator, you should look at the `rc_client_t` functions which will prepare the API calls and other implement other common functionality (like managing the user information, identifying/loading a game, and building the active/inactive achievements list for the UI). It has several callback functions which allow the client to implement dependent functionality (UI and HTTP calls). Please see [the wiki](https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration) for details on using the `rc_client_t` functions.

## Server Communication

**rapi** builds URLs to access many RetroAchievements web services. Its purpose it to just to free the developer from having to URL-encode parameters and build correct URLs that are valid for the server.

**rapi** does *not* make HTTP requests.

NOTE: `rc_client` is the preferred way to have a client interact with the server.

**rapi** headers are `rc_api_user.h`, `rc_api_runtime.h` and `rc_api_common.h`.

The basic process of making an **rapi** call is to initialize a params object, call a function to convert it to a URL, send that to the server, then pass the response to a function to convert it into a response object, and handle the response values.

An example can be found on the [rc_api_init_login_request](https://github.com/RetroAchievements/rcheevos/wiki/rc_api_init_login_request#example) page.

### Functions

Please see the [wiki](https://github.com/RetroAchievements/rcheevos/wiki) for details on the functions exposed for **rapi**.

## Game Identification

**rhash** provides logic for generating a RetroAchievements hash for a given game. There are two ways to use the API - you can pass the filename and let rhash open and process the file, or you can pass the buffered copy of the file to rhash if you've already loaded it into memory.

These are in `rc_hash.h`.

```c
  void rc_hash_initialize_iterator(rc_hash_iterator_t* iterator, const char* path, const uint8_t* buffer, size_t buffer_size);
  int rc_hash_generate(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator);
  int rc_hash_iterate(char hash[33], rc_hash_iterator_t* iterator);
  void rc_hash_destroy_iterator(rc_hash_iterator_t* iterator);
```

### Custom file handling

**rhash** (and by extension **rc_client**) support custom handlers for opening/reading files. This allows the client to redirect file reads to support custom file formats (like ZIP or CHD).
