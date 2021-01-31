# mINI

v0.9.7

## Info

This is a tiny, header only C++ library for manipulating INI files.

It conforms to the following format:
- section and key names are case insensitive
- whitespace around sections, keys and values is ignored
- empty section and key names are allowed
- keys that do not belong to a section are ignored
- comments are lines where the first non-whitespace character is a semicolon (`;`)
- trailing comments are allowed on section lines, but not key/value lines
- every entry exists on a single line and multiline is not supported

Files are read on demand in one go, after which the data is kept in memory and is ready to be manipulated. Files are closed after read or write operations. This utility supports lazy writing, which only writes changes and updates and preserves custom formatting and comments. A lazy write invoked by a `write()` call will read the output file, find which changes have been made, and update the file accordingly. If you only need to generate files, use `generate()` instead.

Section and key order is preserved on read and write operations. Iterating through data will take the same order as the original file or the order in which keys were added to the structure.

This library operates with the `std::string` type to hold values and relies on your host environment for encoding. It should play nicely with UTF-8 but your mileage may vary.

## Installation

This is a header-only library. To install it, just copy everything in `/src/` into your own project's source code folder, or use a custom location and just make sure your compiler sees the additional include directory. Then include the file somewhere in your code:

```C++
#include "mini/ini.h"
```

You're good to go!

## Basic example

Start with an INI file named `myfile.ini`:
```INI
; amounts of fruits
[fruits]
apples=20
oranges=30
```

Our code:
```C++
// first, create a file instance
mINI::INIFile file("myfile.ini");

// next, create a structure that will hold data
mINI::INIStructure ini;

// now we can read the file
file.read(ini);

// read a value
std::string& amountOfApples = ini["fruits"]["apples"];

// update a value
ini["fruits"]["oranges"] = "50";

// add a new entry
ini["fruits"]["bananas"] = "100";

// write updates to file
file.write(ini);
```

After running the code, our INI file now looks like this:
```INI
; amounts of fruits
[fruits]
apples=20
oranges=50
bananas=100
```

## Manipulating files

The `INIFile` class holds the filename and exposes functions for reading, writing and generating INI files. It does not keep the file open but merely provides an abstraction you can use to access physical files.

To create a file instance:
```C++
mINI::INIFile file("myfile.ini");
```

You will also need a structure you can operate on:
```C++
mINI::INIStructure ini;
```

To read from a file:
```C++
bool readSuccess = file.read(ini);
```

To write back to a file while preserving comments and custom formatting:
```C++
bool writeSuccess = file.write(ini);
```

You can set the second parameter to `write()` to `true` if you want the file to be written with pretty-print. Pretty-print adds spaces between key-value pairs and blank lines between sections in the output file:
```C++
bool writeSuccess = file.write(ini, true);
```

A `write()` call will attempt to preserve any custom formatting the original INI file uses and will only use pretty-print for creation of new keys and sections.

To generate a file:
```C++
file.generate(ini);
```

Note that `generate()` will overwrite any custom formatting and comments from the original file!

You can use pretty-print with `generate()` as well:
```C++
file.generate(ini, true);
```

Example output for a generated INI file *without* pretty-print:
```INI
[section1]
key1=value1
key2=value2
[section2]
key1=value1
```

Example output for a generated INI file *with* pretty-print:
```INI
[section1]
key1 = value1
key2 = value2

[section2]
key1 = value1
```

## Manipulating data

### Reading data

There are two ways to read data from the INI structure. You can either use the `[]` operator or the `get()` function:

```C++
// read value - if key or section don't exist, they will be created
// returns reference to real value
std::string& value = ini["section"]["key"];

// read value safely - if key or section don't exist they will NOT be created
// returns a copy
std::string value = ini.get("section").get("key");
```

The difference between `[]` and `get()` operations is that `[]` returns a reference to **real** data that you may modify and creates a new item automatically if it does not yet exist, whereas `get()` returns a **copy** of the data and does not create new items in the structure. Use `has()` before doing any operations with `[]` if you wish to avoid altering the structure.

You may combine usage of `[]` and `get()`:
```C++
// will get a copy of the section and create or retreive a key from that copy
// technically a better way to read data safely than .get().get() since it only
// copies data once; does not create new keys in actual data
ini.get("section")["key"];

// if we're sure section exists and we want a copy of key if one exists
// without creating an empty value when the key doesn't exist
ini["section"].get("key");

// you may chain other functions in a similar way
// the following code gets a copy of section and checks if a key exists
ini.get("section").has("key");
```

Section and key names are case insensitive and are stripped of leading and trailing whitespace. `ini["section"]` is the same as `ini["SECTION"]` is the same as `ini["   sEcTiOn   "]` and so on, and same for keys. Generated files always use lower case for section and key names. Writing to an existing file will preserve letter cases of the original file whenever those keys or sections already exists.

### Updating data

To set or update a value:
```C++
ini["section"]["key"] = "value";
```

Note that when writing to a file, values will be stripped of leading and trailing whitespace . For example, the following value will be converted to just `"c"` when reading back from a file: `ini["a"]["b"] = "   c   ";`

You can set multiple values at once by using `set()`:
```C++
ini["section"].set({
	{"key1", "value1"},
	{"key2", "value2"}
});
```

To create an empty section, simply do:
```C++
ini["section"];
```

Similarly, to create an empty key:
```C++
ini["section"]["key"];
```

To remove a single key from a section:
```C++
bool removeSuccess = ini["section"].remove("key");
```

To remove a section:
```C++
bool removeSuccess = ini.remove("section");
```

To remove all keys from a section:
```C++
ini["section"].clear();
```

To remove all data in structure:
```C++
ini.clear();
```

### Other functions

To check if a section is present:
```C++
bool hasSection = ini.has("section");
```

To check if a key within a section is present:
```C++
bool hasKey = ini["section"].has("key");
```

To get the number of keys in a section:
```C++
size_t n_keys = ini["section"].size();
```

To get the number of sections in the structure:
```C++
size_t n_sections = ini.size();
```

### Nitty-gritty

Keep in mind that `[]` will always create a new item if the item does not already exist! You can use `has()` to check if an item exists before performing further operations. Remember that `get()` will return a copy of data, so you should **not** be doing removes or updates to data with it!

Usage of the `[]` operator shouldn't be a problem in most real-world cases where you're doing lookups on known keys and you may not care if empty keys or sections get created. However - if you have a situation where you do not want new items to be added to the structure, either use `get()` to retreive items, or if you don't want to be working with copies of data, use `has()` before using the `[]` operator if you want to be on the safe side.

Short example that demonstrates safe manipulation of data:
```C++
if (ini.has("section"))
{
	// we have section, we can access it safely without creating a new one
	auto& collection = ini["section"];
	if (collection.has("key"))
	{
		// we have key, we can access it safely without creating a new one
		auto& value = collection["key"];
	}
}
```

## Iteration

You can traverse the structure in order of insertion. The following example loops through the structure and displays results in a familiar format:
```C++
for (auto const& it : ini)
{
	auto const& section = it.first;
	auto const& collection = it.second;
	std::cout << "[" << section << "]" << std::endl;
	for (auto const& it2 : collection)
	{
		auto const& key = it2.first;
		auto const& value = it2.second;
		std::cout << key << "=" << value << std::endl;
	}
}
```

`it.first` is always `std::string` type.

`it.second` is an object which is either a `mINI::INIMap` type on the first level or `std::string` type on the second level.

The API only exposes a `const_iterator`, so you can't use iterators to manipulate data directly. You can however access the structure as normal while iterating:

```C++
// change all values in the structure to "banana"
for (auto const& it : ini)
{
	auto const& section = it.first;
	auto const& collection = it.second;
	for (auto const& it2 : collection)
	{
		auto const& key = it2.first;
		ini[section][key] = "banana";
	}
}
```

## Case sensitivity

If you wish to make the library not ignore letter case, add the directive `#define MINI_CASE_SENSITIVE` **before** including the library:
```C++
#define MINI_CASE_SENSITIVE
#include "mini/ini.h"
```

This will affect reading and writing from files and access to the structure.

## Thanks

- [lest](https://github.com/martinmoene/lest) - testing framework

## License

Copyright (c) 2018 Danijel Durakovic

MIT License
