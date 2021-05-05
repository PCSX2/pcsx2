import yaml

# Assumes this is ran from the root of the repository
file_path = "./bin/GameIndex.yaml"

# These settings have to be manually kept in sync with the emulator code unfortunately.
# up to date validation should ALWAYS be provided via the application!
allowed_game_options = [
    "name",
    "region",
    "compat",
    "roundModes",
    "clampModes",
    "gameFixes",
    "speedHacks",
    "memcardFilters",
    "patches",
]
allowed_round_modes = ["eeRoundMode", "vuRoundMode"]
allowed_clamp_modes = ["eeClampMode", "vuClampMode"]
allowed_game_fixes = [
    "VuAddSubHack",
    "FpuCompareHack",
    "FpuMulHack",
    "FpuNegDivHack",
    "XGKickHack",
    "IPUWaitHack",
    "EETimingHack",
    "SkipMPEGHack",
    "OPHFlagHack",
    "DMABusyHack",
    "VIFFIFOHack",
    "VIF1StallHack",
    "GIFFIFOHack",
    "GoemonTlbHack",
    "ScarfaceIbitHack",
    "CrashTagTeamRacingIbitHack",
    "VU0KickstartHack",
]
allowed_speed_hacks = ["mvuFlagSpeedHack", "InstantVU1SpeedHack"]
# Patches are allowed to have a 'default' key or a crc-32 key, followed by
allowed_patch_options = ["author", "content"]

issue_list = []


def is_hex_number(string):
    try:
        int(string, 16)
        return True
    except:
        return False


def validate_string_option(serial, key, value):
    if type(value) != str:
        issue_list.append("[{}]: '{}' must be a string".format(serial, key))


def validate_int_option(serial, key, value, low, high):
    if type(value) != int or (value < low or value > high):
        issue_list.append(
            "[{}]: '{}' must be an int and between {}-{} (inclusive)".format(
                serial, key, low, high
            )
        )


def validate_valid_options(serial, key, value, allowed_values):
    if value not in allowed_values:
        issue_list.append("[{}]: Invalid '{}' option [{}]".format(serial, key, value))


print("Opening {}...".format(file_path))
with open(file_path) as f:
    try:
        print("Attempting to parse GameDB file...")
        gamedb = yaml.load(f, Loader=yaml.FullLoader)
    except Exception as err:
        print(
            "Unable to parse GameDB. Exiting, verify that the file indeed is valid YAML."
        )
        exit(1)

    print("File loaded successfully, validating schema...")
    progress_counter = 0
    for serial, game_options in gamedb.items():
        progress_counter = progress_counter + 1
        if progress_counter % 500 == 0 or progress_counter >= len(gamedb.items()):
            print(
                "[{}/{}] Processing GameDB Entries...".format(
                    progress_counter, len(gamedb.items())
                )
            )

        # Check for required values
        if not "name" in game_options.keys():
            issue_list.append("[{}]: 'name' is a required value".format(serial))
        if not "region" in game_options.keys():
            issue_list.append("[{}]: 'region' is a required value".format(serial))

        for key, value in game_options.items():
            if key not in allowed_game_options:
                issue_list.append("[{}]: Invalid option [{}]".format(serial, key))

            # SIMPLE METADATA VALIDATION
            if key in ["name", "region"]:
                validate_string_option(serial, key, value)

            if key == "compat":
                validate_int_option(serial, key, value, 0, 6)

            # ROUND MODE VALIDATION
            if key == "roundModes" and type(value) == dict:
                for roundMode, roundValue in game_options["roundModes"].items():
                    validate_valid_options(serial, key, roundMode, allowed_round_modes)
                    validate_int_option(serial, key, roundValue, 0, 3)
            elif key == "roundModes":
                issue_list.append(
                    "[{}]: 'roundModes' must be a valid object".format(serial)
                )

            # CLAMP MODE VALIDATION
            if key == "clampModes" and type(value) == dict:
                for clampMode, clampValue in game_options["clampModes"].items():
                    validate_valid_options(serial, key, clampMode, allowed_clamp_modes)
                    validate_int_option(serial, key, clampValue, 0, 3)
            elif key == "clampModes":
                issue_list.append(
                    "[{}]: 'clampModes' must be a valid object".format(serial)
                )

            # GAME FIX VALIDATION
            if key == "gameFixes" and type(value) == list:
                for gamefix in game_options["gameFixes"]:
                    validate_valid_options(serial, key, gamefix, allowed_game_fixes)
            elif key == "gameFixes":
                issue_list.append(
                    "[{}]: 'gameFixes' must be a list of valid gameFixes".format(serial)
                )

            # SPEEDHACKS VALIDATION
            if key == "speedHacks" and type(value) == dict:
                for speedhack, speedhackValue in game_options["speedHacks"].items():
                    validate_valid_options(serial, key, speedhack, allowed_speed_hacks)
                    validate_int_option(serial, speedhack, speedhackValue, 0, 1)
            elif key == "speedHacks":
                issue_list.append(
                    "[{}]: 'speedHacks' must be a valid object".format(serial)
                )

            # MEM CARD VALIDAITON
            if key == "memcardFilters" and not all(
                isinstance(memcardFilter, str) for memcardFilter in value
            ):
                issue_list.append(
                    "[{}]: 'memcardFilters' must be a list of strings".format(serial)
                )

            # PATCH VALIDATION
            if key == "patches" and type(value) == dict:
                for crc, patch in game_options["patches"].items():
                    if crc != "default" and not is_hex_number(str(crc)):
                        issue_list.append(
                            "[{}]: Patches must either be key'd with 'default' or a valid CRC-32 Hex String".format(
                                serial
                            )
                        )
                        continue
                    for patchOption, optionValue in patch.items():
                        validate_valid_options(
                            serial, key, patchOption, allowed_patch_options
                        )
                        if key == "author":
                            validate_string_option(serial, patchOption, optionValue)
                        if key == "content" and not all(
                            isinstance(patchLine, str) for patchLine in optionValue
                        ):
                            issue_list.append(
                                "[{}]: Patch 'content' must be a list of strings".format(
                                    serial
                                )
                            )
            elif key == "patches":
                issue_list.append(
                    "[{}]: 'patches' must be valid mapping of CRC32 -> Patch Objects".format(
                        serial
                    )
                )

if len(issue_list) > 0:
    print("Issues found during validation:")
    print(*issue_list, sep="\n")
    exit(1)
else:
    print("GameDB Validated Successfully!")
    exit(0)
