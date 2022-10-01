#include "discord_rpc.h"
#include "discord_register.h"
#include <stdio.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool Mkdir(const char* path)
{
    int result = mkdir(path, 0755);
    if (result == 0) {
        return true;
    }
    if (errno == EEXIST) {
        return true;
    }
    return false;
}

// we want to register games so we can run them from Discord client as discord-<appid>://
extern "C" DISCORD_EXPORT void Discord_Register(const char* applicationId, const char* command)
{
    // Add a desktop file and update some mime handlers so that xdg-open does the right thing.

    const char* home = getenv("HOME");
    if (!home) {
        return;
    }

    char exePath[1024];
    if (!command || !command[0]) {
        ssize_t size = readlink("/proc/self/exe", exePath, sizeof(exePath));
        if (size <= 0 || size >= (ssize_t)sizeof(exePath)) {
            return;
        }
        exePath[size] = '\0';
        command = exePath;
    }

    const char* desktopFileFormat = "[Desktop Entry]\n"
                                   "Name=Game %s\n"
                                   "Exec=%s %%u\n" // note: it really wants that %u in there
                                   "Type=Application\n"
                                   "NoDisplay=true\n"
                                   "Categories=Discord;Games;\n"
                                   "MimeType=x-scheme-handler/discord-%s;\n";
    char desktopFile[2048];
    int fileLen = snprintf(
      desktopFile, sizeof(desktopFile), desktopFileFormat, applicationId, command, applicationId);
    if (fileLen <= 0) {
        return;
    }

    char desktopFilename[256];
    snprintf(desktopFilename, sizeof(desktopFilename), "/discord-%s.desktop", applicationId);

    char desktopFilePath[1024];
    snprintf(desktopFilePath, sizeof(desktopFilePath), "%s/.local", home);
    if (!Mkdir(desktopFilePath)) {
        return;
    }
    strcat(desktopFilePath, "/share");
    if (!Mkdir(desktopFilePath)) {
        return;
    }
    strcat(desktopFilePath, "/applications");
    if (!Mkdir(desktopFilePath)) {
        return;
    }
    strcat(desktopFilePath, desktopFilename);

    FILE* fp = fopen(desktopFilePath, "w");
    if (fp) {
        fwrite(desktopFile, 1, fileLen, fp);
        fclose(fp);
    }
    else {
        return;
    }

    char xdgMimeCommand[1024];
    snprintf(xdgMimeCommand,
             sizeof(xdgMimeCommand),
             "xdg-mime default discord-%s.desktop x-scheme-handler/discord-%s",
             applicationId,
             applicationId);
    if (system(xdgMimeCommand) < 0) {
        fprintf(stderr, "Failed to register mime handler\n");
    }
}

extern "C" DISCORD_EXPORT void Discord_RegisterSteamGame(const char* applicationId,
                                                         const char* steamId)
{
    char command[256];
    sprintf(command, "xdg-open steam://rungameid/%s", steamId);
    Discord_Register(applicationId, command);
}
