#ifndef RA_EMULATORS_H
#define RA_EMULATORS_H

enum EmulatorID
{
    RA_Gens = 0,
    RA_Project64 = 1,
    RA_Snes9x = 2,
    RA_VisualboyAdvance = 3,
    RA_Nester = 4,
    RA_FCEUX = 5,
    RA_PCE = 6,
    RA_Libretro = 7,
    RA_Meka = 8,
    RA_QUASI88 = 9,
    RA_AppleWin = 10,
    RA_Oricutron = 11,

    NumEmulatorIDs,
    UnknownEmulator = NumEmulatorIDs
};

#endif /* !RA_EMULATORS_H */
