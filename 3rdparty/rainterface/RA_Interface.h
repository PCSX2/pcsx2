#ifndef RA_INTERFACE_H
#define RA_INTERFACE_H

#include <wtypes.h> /* HWND */

#ifdef __cplusplus
extern "C" {
#endif

/******************************
 * Initialization             *
 ******************************/

/**
 * Loads and initializes the DLL.
 *
 * Must be called before using any of the other functions. Will automatically download the DLL
 * if not found, or prompt the user to upgrade if a newer version is available
 *
 * @param hMainHWND       the handle of the main window
 * @param nEmulatorID     the unique idenfier of the emulator
 * @param sClientVersion  the current version of the emulator (will be validated against the minimum version for the specified emulator ID)
 */
extern void RA_Init(HWND hMainHWND, int nEmulatorID, const char* sClientVersion);

/**
 * Loads and initializes the DLL.
 *
 * Must be called before using any of the other functions. Will automatically download the DLL
 * if not found, or prompt the user to upgrade if a newer version is available
 *
 * @param hMainHWND       the handle of the main window
 * @param sClientName     the name of the client, displayed in the title bar and included in the User-Agent for API calls
 * @param sClientVersion  the current version of the client
 */
extern void RA_InitClient(HWND hMainHWND, const char* sClientName, const char* sClientVersion);

/**
 * Defines callbacks that the DLL can use to interact with the client.
 *
 * @param fpUnusedIsActive  [no longer used] returns non-zero if a game is running
 * @param fpCauseUnpause    unpauses the emulator
 * @param fpCausePause      pauses the emulator
 * @param fpRebuildMenu     notifies the client that the popup menu has changed (@see RA_CreatePopupMenu)
 * @param fpEstimateTitle   gets a short description for the game being loaded (parameter is a 256-byte buffer to fill)
 * @param fpResetEmulator   resets the emulator
 * @param fpLoadROM         [currently unused] tells the emulator to load a specific game
 */
extern void RA_InstallSharedFunctions(int (*fpUnusedIsActive)(void),
    void (*fpCauseUnpause)(void), void (*fpCausePause)(void), void (*fpRebuildMenu)(void),
    void (*fpEstimateTitle)(char*), void (*fpResetEmulator)(void), void (*fpLoadROM)(const char*));

/**
 * Tells the DLL to use UpdateWindow instead of InvalidateRect when the UI needs to be repainted. This is primarily
 * necessary when integrating with an emulator using the SDL library as it keeps the message queue flooded so the
 * InvalidateRect messages never get turned into WM_PAINT messages.
 *
 * @param bEnable         non-zero if InvalidateRect calls should be replaced with UpdateWindow
 */
extern void RA_SetForceRepaint(int bEnable);

/**
 * Creates a popup menu that can be appended to the main menu of the emulator.
 *
 * @return                handle to the menu. if not attached to the program menu, caller must destroy it themselves.
 */
extern HMENU RA_CreatePopupMenu(void);

/* Resource values for menu items - needed by MFC ON_COMMAND_RANGE macros or WM_COMMAND WndProc handlers
 * they're not all currently used, allowing additional items without forcing recompilation of the emulators
 */
#define IDM_RA_MENUSTART 1700
#define IDM_RA_MENUEND 1739

typedef struct RA_MenuItem
{
    LPCWSTR sLabel;
    LPARAM nID;
    int bChecked;
} RA_MenuItem;

/**
 * Gets items for building a popup menu.
 *
 * @param pItems          Pre-allocated array to populate [should contain space for at least 32 items]
 * @return                Number of items populated in the items array
 */
extern int RA_GetPopupMenuItems(RA_MenuItem *pItems);

/**
 * Called when a menu item in the popup menu is selected.
 *
 * @param nID             the ID of the menu item (will be between IDM_RA_MENUSTART and IDM_RA_MENUEND)
 */
extern void RA_InvokeDialog(LPARAM nID);

/**
 * Provides additional information to include in the User-Agent string for API calls.
 *
 * This is primarily used to identify dependencies or configurations (such as libretro core versions)
 *
 * @param sDetail         the additional information to include
 */
extern void RA_SetUserAgentDetail(const char* sDetail);

/**
 * Attempts to log in to the retroachievements.org site.
 *
 * Prompts the user for their login credentials and performs the login. If they've previously logged in and have
 * chosen to store their credentials, the login occurs without prompting.
 *
 * @param bBlocking       if zero, control is returned to the calling process while the login request is processed by the server.
 */
extern void RA_AttemptLogin(int bBlocking);

/**
 * Specifies the console associated to the emulator.
 *
 * May be called just before loading a game if the emulator supports multiple consoles.
 *
 * @param nConsoleID      the unique identifier of the console associated to the game being loaded.
 */
extern void RA_SetConsoleID(unsigned int nConsoleID);

/**
 * Resets memory references created by previous calls to RA_InstallMemoryBank.
 */
extern void RA_ClearMemoryBanks(void);

typedef unsigned char (RA_ReadMemoryFunc)(unsigned int nAddress);
typedef void (RA_WriteMemoryFunc)(unsigned int nAddress, unsigned char nValue);
/**
 * Exposes a block of memory to the DLL.
 *
 * The blocks of memory expected by the DLL are unique per console ID. To identify the correct map for a console,
 * view the consoleinfo.c file in the rcheevos repository.
 *
 * @param nBankID         the index of the bank to update. will replace any existing bank at that index.
 * @param pReader         a function to read from the bank. parameter is the offset within the bank to read from.
 * @param pWriter         a function to write to the bank. parameters are the offset within the bank to write to and an 8-bit value to write.
 * @param nBankSize       the size of the bank.
 */
extern void RA_InstallMemoryBank(int nBankID, RA_ReadMemoryFunc pReader, RA_WriteMemoryFunc pWriter, int nBankSize);

typedef unsigned int (RA_ReadMemoryBlockFunc)(unsigned int nAddress, unsigned char* pBuffer, unsigned int nBytes);
extern void RA_InstallMemoryBankBlockReader(int nBankID, RA_ReadMemoryBlockFunc pReader);

/**
 * Deinitializes and unloads the DLL.
 */
extern void RA_Shutdown(void);



/******************************
 * Overlay                    *
 ******************************/

/**
 * Determines if the overlay is fully visible.
 *
 * Precursor check before calling RA_NavigateOverlay
 *
 * @return                non-zero if the overlay is fully visible, zero if it is not.
 */
extern int RA_IsOverlayFullyVisible(void);

/**
 * Called to show or hide the overlay.
 *
 * @param bIsPaused       true to show the overlay, false to hide it.
 */
extern void RA_SetPaused(bool bIsPaused);

struct ControllerInput
{
    int m_bUpPressed;
    int m_bDownPressed;
    int m_bLeftPressed;
    int m_bRightPressed;
    int m_bConfirmPressed; /* Usually C or A */
    int m_bCancelPressed;  /* Usually B */
    int m_bQuitPressed;    /* Usually Start */
};

/**
 * Passes controller input to the overlay.
 *
 * Does nothing if the overlay is not fully visible.
 *
 * @param pInput          pointer to a ControllerInput structure indicating which inputs are active.
 */
extern void RA_NavigateOverlay(struct ControllerInput* pInput);

/**
 * [deprecated] Updates the overlay for a single frame.
 *
 * This function just calls RA_NavigateOverlay. Updating and rendering is now handled internally to the DLL.
 */
extern void RA_UpdateRenderOverlay(HDC, struct ControllerInput* pInput, float, RECT*, bool, bool);

/**
 * Updates the handle to the main window.
 *
 * The main window handle is used to anchor the overlay. If the client recreates the handle as the result of switching
 * from windowed mode to full screen, or for any other reason, it should call this to reattach the overlay.
 *
 * @param hMainHWND       the new handle of the main window
 */
extern void RA_UpdateHWnd(HWND hMainHWND);



/******************************
 * Game Management            *
 ******************************/

/**
 * Identifies the game associated to a block of memory.
 *
 * The block of memory is the fully buffered game file. If more complicated identification is required, the caller
 * needs to link against rcheevos/rhash directly to generate the hash, and call RA_IdentifyHash with the result.
 * Can be called when switching discs to ensure the additional discs are still associated to the loaded game.
 *
 * @param pROMData        the contents of the game file
 * @param nROMSize        the size of the game file
 * @return                the unique identifier of the game, 0 if no association available.
 */
extern unsigned int RA_IdentifyRom(BYTE* pROMData, unsigned int nROMSize);

/**
 * Identifies the game associated to a pre-generated hash.
 *
 * Used when the hash algorithm is something other than full file.
 * Can be called when switching discs to ensure the additional discs are still associated to the loaded game.
 *
 * @param sHash           the hash generated by rcheevos/rhash
 * @return                the unique identifier of the game, 0 if no association available.
 */
extern unsigned int RA_IdentifyHash(const char* sHash);

/**
 * Fetches all retroachievements related data for the specified game.
 *
 * @param nGameId         the unique identifier of the game to activate
 */
extern void RA_ActivateGame(unsigned int nGameId);

/**
 * Identifies and activates the game associated to a block of memory.
 *
 * Functions as a call to RA_IdentifyRom followed by a call to RA_ActivateGame.
 *
 * @param pROMData        the contents of the game file
 * @param nROMSize        the size of the game file
 */
extern void RA_OnLoadNewRom(BYTE* pROMData, unsigned int nROMSize);

/**
 * Called before unloading the game to allow the user to save any changes they might have.
 *
 * @param bIsQuitting     non-zero to change the messaging to indicate the emulator is closing.
 * @return                zero to abort the unload. non-zero to continue.
 */
extern int RA_ConfirmLoadNewRom(int bIsQuitting);



/******************************
 * Runtime Functionality      *
 ******************************/

/**
 * Does all achievement-related processing for a single frame.
 */
extern void RA_DoAchievementsFrame(void);

/**
 * Temporarily disables forced updating of tool windows.
 *
 * Primarily used while fast-forwarding.
 */
extern void RA_SuspendRepaint(void);

/**
 * Resumes forced updating of tool windows.
 */
extern void RA_ResumeRepaint(void);

/**
 * [deprecated] Used to be used to ensure the asynchronous server calls are processed on the UI thread.
 * That's all managed within the DLL now. Calling this function does nothing.
 */
extern void RA_HandleHTTPResults(void);

/**
 * Adds flavor text to the application title bar.
 *
 * Application title bar is managed by the DLL. Value will be "ClientName - Version - Flavor Text - Username"
 *
 * @param sCustomMessage  the flavor text to include in the title bar.
 */
extern void RA_UpdateAppTitle(const char* sCustomMessage);

/**
 * Get the user name of the currently logged in user.
 *
 * @return                user name of the currently logged in user, empty if no user is logged in. 
 */
const char* RA_UserName(void);

/**
 * Determines if the user is currently playing with hardcore enabled.
 *
 * The client should disable any features that would give the player an unfair advantage if this returns non-zero.
 * Things like loading states, using cheats, modifying RAM, disabling rendering layers, viewing decoded tilemaps, etc.
 *
 * @return                non-zero if hardcore mode is currently active.
 */
extern int RA_HardcoreModeIsActive(void);

/**
 * Warns the user they're about to do something that will disable hardcore mode.
 *
 * @param sActivity       what the user is about to do (i.e. "load a state").
 * @return                non-zero if the user disabled hardcore and the activity is allowed.
 *                        zero if the user declined to disable hardcore and the activity should be aborted.
 */
extern int RA_WarnDisableHardcore(const char* sActivity);

/**
 * Disables hardcore mode without prompting or notifying the user.
 *
 * Should only be called if the client does its own prompting/notification.
 * Typically used when an activity cannot be aborted.
 */
extern void RA_DisableHardcore(void);

/**
 * Notifies the DLL that the game has been reset.
 *
 * Disables active leaderboards and resets hit counts on all active achievements.
 */
extern void RA_OnReset(void);

/**
 * Notifies the DLL that a save state has been created.
 *
 * Creates a .rap file next to the state file that contains achievement-related information for the save state.
 *
 * @param sFilename       full path to the save state file.
 */
extern void RA_OnSaveState(const char* sFilename);

/**
 * Notifies the DLL that a save state has been loaded.
 *
 * Loads the .rap file next to the state file that contains achievement-related information for the save state being loaded.
 *
 * @param sFilename       full path to the save state file.
 */
extern void RA_OnLoadState(const char* sFilename);

/**
 * Captures the current state of the achievement runtime for inclusion in a save state.
 *
 * @param pBuffer         buffer to write achievement state information to
 * @param nBufferSize     the size of the buffer
 * @return                the number of bytes needed to capture the achievement state. if less than nBufferSize, pBuffer
 *                        will not be populated. the function should be called again with a larger buffer.
 */
extern int RA_CaptureState(char* pBuffer, int nBufferSize);

/**
 * Restores the state of the achievement runtime from a previously captured state.
 *
 * @param pBuffer         buffer containing previously serialized achievement state information
 */
extern void RA_RestoreState(const char* pBuffer);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // !RA_INTERFACE_H
