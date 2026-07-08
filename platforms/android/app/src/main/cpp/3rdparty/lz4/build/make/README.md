# multiconf.make

**multiconf.make** is a self-contained Makefile include that lets you build the **same targets under many different flag sets**—debug vs release, ASan vs UBSan, GCC vs Clang, etc.—without the usual “object-file soup.”
It hashes every combination of `CC/CXX`, `CFLAGS/CXXFLAGS`, `CPPFLAGS`, `LDFLAGS` and `LDLIBS` into a **dedicated cache directory**, so objects compiled with one configuration are never reused by another. Swap flags, rebuild, swap back—previous objects are still there and never collide.

---

## Key Benefits

| Why it matters | What multiconf.make does |
| --- | --- |
| **Isolated configs** | Stores objects into `cachedObjs/<hash>/`, one directory per unique flag set. |
| **Fast switching** | Reusing an old config is instant—link only, no recompilation. |
| **Header deps** | Edits to headers trigger only needed rebuilds. |
| **One-liner targets** | Macros (`c_program`, `cxx_program`, …) hide all rule boilerplate. |
| **Parallel-ready** | Safe with `make -j`, no duplicate compiles of shared sources. |

---

## Quick Start

### 1 · List your sources

```make
C_SRCDIRS   := src src/cdeps    # all .c are in these directories
CXX_SRCDIRS := src src/cxxdeps  # all .cpp are in these directories
```

### 2 · Add and include

```make
# root/Makefile
include multiconf.make
```

### 3 · Declare targets

```make
app:
$(eval $(call c_program,app,app.o obj1.o obj2.o))
test:
$(eval $(call cxx_program,test, test.o objcxx1.o objcxx2.o))
```

### 4 · Build any config you like

```sh
# Release with GCC
make CFLAGS="-O3"

# Debug with Clang + AddressSanitizer (new cache dir)
make CC=clang CFLAGS="-g -O0 -fsanitize=address"

# Switch back to GCC release (objects still valid, relink only)
make CFLAGS="-O3"
```

Objects for each command live in different sub-folders; nothing overlaps.

---

## Additional capabilities

| Command | Description |
| --- | --- |
| `make clean_cache` | Wipe **all** cached objects & deps (full rebuild next time) |
| `V=1` | Show full compile/link commands |

---
