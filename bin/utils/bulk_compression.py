#!/usr/bin/env python3

# PCSX2 - PS2 Emulator for PCs
# Copyright (C) 2024 PCSX2 Dev Team
#
# PCSX2 is free software: you can redistribute it and/or modify it under the terms
# of the GNU General Public License as published by the Free Software Found-
# ation, either version 3 of the License, or (at your option) any later version.
#
# PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with PCSX2.
# If not, see <https://www.gnu.org/licenses/>.

import sys
import os
import re
from subprocess import Popen, PIPE
from os.path import exists

gamecount = [0]

# =================================================================================================

def deletionChoice(source_extension):                               # Choose to delete source files

    yesno = {
        "n"   : 0,
        "no"  : 0,
        "y"   : 1,
        "yes" : 1,
    }

    print("╟-------------------------------------------------------------------------------╢")
    print(f"║ Do you want to delete the original {source_extension.upper()} files as they are converted?")
    choice = input("║ Type Y or N then press ENTER: ").lower()

    if (not choice in yesno):
        exitInvalidOption()

    return (yesno[choice])

# -------------------------------------------------------------------------------------------------

def blockSizeChoice(is_cd, decompressing):                          # Choose block size

    if (decompressing):
        return 0

    sizes = {
        "1" : 16384,
        "2" : 131072,
        "3" : 262144,
    } if not is_cd else {
        "1": 17136,
        "2": 132192,
        "3": 264384,
    }

    print("╟-------------------------------------------------------------------------------╢")
    print("║ Please pick the block size you would like to use:")
    print("║")
    print("║   1 - 16 kB  (bigger files, faster access/less CPU, choose this if unsure)")
    print("║   2 - 128 kB (balanced)")
    print("║   3 - 256 kB (smaller files, slower access/more CPU)")
    print("║")
    blocksize = input("║ Type the number corresponding to your selection then press ENTER: ")

    if (not blocksize in sizes):
        exitInvalidOption()

    return (sizes[blocksize])

# =================================================================================================

def checkSuccess(compressing, fname, extension, error_code): # Ensure file created properly

    target_fname = f"{fname}.{extension}"
    if (error_code):

        print("╠===============================================================================╣")

        if (compressing):
            print(f"║ Compression to {extension.upper()} failed for the following:{(37 - len(extension)) * ' '}║")
        else:
            print(f"║ Extraction to {extension.upper()} failed for the following:{(38 - len(extension)) * ' '}║")

        print(f"║ {target_fname}{(77 - len(target_fname)) * ' '}║")
        print("╚===============================================================================╝")

        sys.exit(1)

    print(f"║ {target_fname} created.{(69 - len(target_fname)) * ' '}║")

# -------------------------------------------------------------------------------------------------

def checkProgramMissing(program):

    if (sys.platform.startswith('win32') and exists(f"./{program}.exe")):
        return                                                      # Windows

    else:                                                           # Linux, macOS
        from shutil import which
        if (which(program) is not None):
            return

    print("╠===============================================================================╗")
    print(f"║                   {program} failed, {program} is missing.{(39 - (len(program) * 2)) * ' '}║")
    print("╚===============================================================================╝")
    sys.exit(1)

# -------------------------------------------------------------------------------------------------

def checkBinCueMismatch(bin_files, cue_files):                      # Ensure all bins and cues match

    if (len(bin_files) != len(cue_files)):                          # Ensure numerical parity
        exitBinCueMismatch()

    for fname in bin_files:                                         # Ensure filename parity
        if (f"{fname[:-4]}.cue" not in cue_files):
            exitBinCueMismatch()

# -------------------------------------------------------------------------------------------------

def checkDuplicates(source_files, target_extensions, crash_protection_type=0):

    dupe_options = {
        "s"          : 0,
        "skip"       : 0,
        "o"          : 1,
        "overwrite"  : 1,
    }

    dupe_files = []
    dupe_names = []
    target_files = []

    for extension in target_extensions:
        target_files[len(target_files):] = returnFilteredPwdContents(extension)

    for fname in source_files:
        for extension in target_extensions:
            target_fname = f"{fname[:-4]}.{extension}"
            if (target_fname in target_files):
                dupe_files.append(target_fname)

    match crash_protection_type:
        case 0:
            pass
        case 1:                                                     # Skip any dupe files no matter what
            [dupe_names.append(fname[:-4]) for fname in dupe_files if fname[:-4] not in dupe_names]
            return dupe_names
        case 2:                                                     # Only skip if intermediate .iso present
            [dupe_names.append(fname[:-4]) for fname in dupe_files if fname[:-4] not in dupe_names and fname[-4:] == ".iso"]
        case _:
            pass

    if (not dupe_files):
        return dupe_names

    print("╟-------------------------------------------------------------------------------╢")
    print("║ The following files were found which would be overwritten:")

    for fname in dupe_files:
        print(f"║  - {fname}")

    print("║")
    print("║ You may choose to OVERWRITE or SKIP all of these.")
    if (crash_protection_type == 2):
        print("║ NOTE: chdman cannot overwrite .cso files. These will be skipped regardless.")
    choice = input("║ Press 'O' to overwrite or 'S' to skip and press ENTER: ").lower()

    if (choice in dupe_options):
        if (not dupe_options[choice]):                              # Skip
            [dupe_names.append(fname[:-4]) for fname in dupe_files if fname[:-4] not in dupe_names]
        return dupe_names
    else:
        exitInvalidOption()

# =================================================================================================

def printInitialStatus(decompressing, target_fname):

    if (gamecount[0] != 0):
        print("╟-------------------------------------------------------------------------------╢")
    gamecount[0] += 1

    if (decompressing):
        print(f"║ Extracting to {target_fname}... ({gamecount[0]}){(58 - len(target_fname) - len(str(gamecount[0]))) * ' '}║")
    else:
        print(f"║ Compressing to {target_fname}... ({gamecount[0]}){(57 - len(target_fname) - len(str(gamecount[0]))) * ' '}║")

# -------------------------------------------------------------------------------------------------

def printSkip(target_fname):

    if (gamecount[0] != 0):
        print("╟-------------------------------------------------------------------------------╢")
    gamecount[0] += 1
    print(f"║ Skipping creation of {target_fname}{(57 - len(target_fname)) * ' '}║")

# =================================================================================================

def createCommandList(mode, source_fname, target_fname, blocksize=0):

    match mode:
        case 1:
            return [["maxcso", f"--block={blocksize}", source_fname]]
        case 2:
            return [["chdman", "createraw", "-us", "2048", "-hs", f"{blocksize}", "-f", "-i", source_fname, "-o", target_fname]]
        case 3:
            return [["chdman", "createcd", "-hs", f"{blocksize}", "-i", source_fname, "-o", f"{source_fname[:-4]}.chd"]]
        case 4:
            return [["maxcso", "--decompress", source_fname],
                    ["chdman", "createraw", "-us", "2048", "-hs", f"{blocksize}", "-f", "-i", f"{source_fname[:-4]}.iso", "-o", f"{source_fname[:-4]}.chd"]]
        case 5:
            return [["chdman", "extractraw", "-i", source_fname, "-o", f"{source_fname[:-4]}.iso"],
                    ["maxcso", f"--block={blocksize}", f"{source_fname[:-4]}.iso"]]
        case 6:
            return [["chdman", "extractraw", "-i", source_fname, "-o", target_fname]]
        case 7:
            return [["chdman", "extractcd", "-i", source_fname, "-o", target_fname]]
        case 8:
            return [["maxcso", "--decompress", source_fname]]
        case _:
            print("You have somehow chosen an invalid mode, and this was not correctly caught by the program.\nPlease report this as a bug.")
            sys.exit(1)

# =================================================================================================

def returnFilteredPwdContents(file_extension):                      # Get files in pwd with extension

    extension_pattern = r".*\." + file_extension.lower()
    extension_reg = re.compile(extension_pattern)
    return [fname for fname in os.listdir('.') if extension_reg.match(fname)]

# -------------------------------------------------------------------------------------------------

def deleteFile(fname):                                              # Delete a file in pwd

    print(f"║ Deleting {fname}...{(66 - len(fname)) * ' '}║")
    os.remove(f"./{fname}")

# =================================================================================================

def exitInvalidOption():

    print("╠===============================================================================╗")
    print("║                              Invalid option.                                  ║")
    print("╚===============================================================================╝")
    sys.exit(1)

# -------------------------------------------------------------------------------------------------

def exitBinCueMismatch():

    print("╠===============================================================================╗")
    print("║                   All BIN files must have a matching CUE.                     ║")
    print("╚===============================================================================╝")
    sys.exit(1)

# =================================================================================================
# /////////////////////////////////////////////////////////////////////////////////////////////////
# =================================================================================================

options = {                                                         # Options listings
    1 : "Convert ISO to CSO",
    2 : "Convert ISO to CHD",
    3 : "Convert CUE/BIN to CHD",
    4 : "Convert CSO to CHD",
    5 : "Convert DVD CHD to CSO",
    6 : "Extract DVD CHD to ISO",
    7 : "Extract CD CHD to CUE/BIN",
    8 : "Extract CSO to ISO",
    9 : "Exit script",
}

# -------------------------------------------------------------------------------------------------

sources = {                                                         # Source file extensions
    1 : "iso",
    2 : "iso",
    3 : "cue/bin",
    4 : "cso",
    5 : "chd",
    6 : "chd",
    7 : "chd",
    8 : "cso",
}

# -------------------------------------------------------------------------------------------------

targets = {                                                         # Target file extensions
    1 : ["cso"],
    2 : ["chd"],
    3 : ["chd"],
    4 : ["iso", "chd"],
    5 : ["iso", "cso"],
    6 : ["iso"],
    7 : ["cue", "bin"],
    8 : ["iso"],
}

# # -------------------------------------------------------------------------------------------------

reqs = {                                                            # Selection dependencies
    1 : ["maxcso"],
    2 : ["chdman"],
    3 : ["chdman"],
    4 : ["maxcso", "chdman"],
    5 : ["maxcso", "chdman"],
    6 : ["chdman"],
    7 : ["chdman"],
    8 : ["maxcso"],
}

# -------------------------------------------------------------------------------------------------

print("╔===============================================================================╗")
print("║  CSO/CHD/ISO/CUEBIN Conversion by Refraction, RedDevilus and TheTechnician27  ║")
print("║                           (Version Jul 16 2024)                               ║")
print("╠===============================================================================╣")
print("║                                                                               ║")
print("║ PLEASE NOTE: This will affect all files in this folder!                       ║")
print("║ Be sure to run this from the same directory as the files you wish to convert. ║")
print("║                                                                               ║")

for number, message in options.items():
    print("║ ", number, " - ", message, f"{(70 - len(message)) * ' '}║")

print("║                                                                               ║")
print("╠===============================================================================╝")
#print("║")
mode = input("║ Type the number corresponding to your selection then press ENTER: ")

# -------------------------------------------------------------------------------------------------

try:
    mode = int(mode)

except ValueError:
    exitInvalidOption()

# -------------------------------------------------------------------------------------------------

if (mode < 9 and mode > 0):

    for program in reqs[mode]:                                  # Check for dependencies
        checkProgramMissing(program)

    delete = deletionChoice(sources[mode])                      # Choose to delete source files
    blocksize = blockSizeChoice(mode == 3, mode > 5)            # Choose block size if compressing

    match mode:
        case 3:
            bin_files = returnFilteredPwdContents("bin")        # Get all BIN files in pwd
            source_files = returnFilteredPwdContents("cue")     # Get all CUE files in pwd
            checkBinCueMismatch(bin_files, source_files)
            dupe_list = checkDuplicates(source_files, targets[mode], 1)
        case 5:
            source_files = returnFilteredPwdContents(sources[mode]) # Get source files in pwd
            dupe_list = checkDuplicates(source_files, targets[mode], 2)
        case 6:
            source_files = returnFilteredPwdContents(sources[mode]) # Get source files in pwd
            dupe_list = checkDuplicates(source_files, targets[mode], 1)
        case _:
            source_files = returnFilteredPwdContents(sources[mode]) # Get source files in pwd
            dupe_list = checkDuplicates(source_files, targets[mode])


    print("╠===============================================================================╗")

    # ---------------------------------------------------------------------------------------------

    for fname in source_files:

        target_fname = f"{fname[:-4]}.{targets[mode][0]}"
        commands = createCommandList(mode, fname, target_fname, blocksize)
        if (fname[:-4] in dupe_list):
            printSkip(target_fname)
            continue

        printInitialStatus(mode > 5, f"{fname[:-4]}.{targets[mode][-1]}")

        for step, command in enumerate (commands):

            process = Popen(commands[step], stdout=PIPE, stderr=PIPE)   # Execute process
            stdout, stderr = process.communicate()                      # Suppress output
            checkSuccess(mode < 6, fname[:-4],                          # Ensure target creation
                         targets[mode][step], process.returncode)

            if (step == 1):                                             # Delete intermediate file
                deleteFile(f"{fname[:-4]}.iso")

        if (delete):                                                    # Delete source requested
            deleteFile(fname)
            if (mode == 3):
                deleteFile(f"{fname[:-4]}.bin")

# ===== EXIT SCRIPT ===============================================================================

elif (mode == 9):
    print("╠===============================================================================╗")
    print("║                                Goodbye! :)                                    ║")
    print("╚===============================================================================╝")
    sys.exit(0)

# ===== EXIT SCRIPT WITH ERROR ====================================================================

else:
    exitInvalidOption()

# -------------------------------------------------------------------------------------------------

print("╠===============================================================================╣")
print("║                               Process complete!                               ║")
print("╚===============================================================================╝")
sys.exit(0)
