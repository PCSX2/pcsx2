## libgamemode

**libgamemode** is an internal library used to dispatch requests to the daemon. Note: `libgamemode` should never be linked with directly.

**libgamemodeauto** is a simple dynamic library that automatically requests game mode when loaded. Useful to `LD_PRELOAD` into any game as needed.

**gamemode\_client.h** is as single header lib that lets a game request game mode and handle errors.

### Integration
Developers can integrate the request directly into an app. Note that none of these client methods force your users to have the daemon installed or running - they will safely no-op if the host is missing.

```C
// Manually with error checking
#include "gamemode_client.h"

	if( gamemode_request_start() < 0 ) {
		fprintf( stderr, "gamemode request failed: %s\n", gamemode_error_string() );
	}

	/* run game... */

	gamemode_request_end(); // Not required, gamemoded can clean up after game exits
```

```C
// Automatically on program start and finish
#define GAMEMODE_AUTO
#include "gamemode_client.h"
```

Or, distribute `libgamemodeauto.so` and either add `-lgamemodeauto` to your linker arguments, or add it to an LD\_PRELOAD in a launch script.

### Supervisor support
Developers can also create apps that manage GameMode on the system, for other processes:

```C
#include "gamemode_client.h"

	gamemode_request_start_for(gamePID);
	gamemode_request_end_for(gamePID);
```

This functionality can also be controlled in the config file in the `supervisor` section.