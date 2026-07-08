# RAInterface

This code is intended to be loaded into another repository as a submodule.

An emulator should include RA_Interface.cpp in its build and link against winhttp.lib. Then, the emulator can be modified to call the hooks provided in RA_Interface.cpp at appropriate times to integrate with the RetroAchievements server via the RA_Integration.dll. See wiki for more details.
