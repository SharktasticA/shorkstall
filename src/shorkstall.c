/*
    ######################################################
    ##            SHORK UTILITY - SHORKSTALL            ##
    ######################################################
    ## Main program logic                               ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



#include "colours.h"
#include "general.h"
#include "shorkmenu.h"
#include "shorkstall.h"

#include <errno.h>
#include <linux/fd.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>



/**
 * Checks if the target floppy drive has a diskette inserted, if its writable,
 * or if there is an error.
 * @param dev Floppy drive block device
 * @return 1 if present and writable; 0 if not; -1 if error
 */
int checkFloppyDrive(char* dev)
{
    // Silence possible kernel message spam
    system("dmesg -n 1");

    int fd = open(dev, O_RDONLY);
    if (fd < 0)
    {
        system("dmesg -n 4");
        if (errno == ENXIO)
        {
            char msgBody[72];
            snprintf(msgBody, sizeof(msgBody), "No floppy diskette is inserted. Please insert one and try again.", dev);
            int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
            printTextScreen(dev, msgBody, lines, 0);
            return 0;
        }
        else
        {
            char msgBody[86];
            snprintf(msgBody, sizeof(msgBody), "There was an unidentified issue trying to access %s.", dev);
            int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
            printTextScreen(dev, msgBody, lines, 0);
            return -1;
        }
    }

    // Force media check
    char buffer[1];
    read(fd, buffer, 1);

    struct floppy_drive_struct ds;
    if (ioctl(fd, FDGETDRVSTAT, &ds) < 0)
    {
        system("dmesg -n 4");
        char msgBody[86];
        snprintf(msgBody, sizeof(msgBody), "There was an unidentified issue trying to access %s.", dev);
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(dev, msgBody, lines, 0);
        close(fd);
        return -1;
    }

    close(fd);
    // Restore kernel messaging
    system("dmesg -n 4");

    if (!(ds.flags & FD_DISK_WRITABLE))
    {
        char msgBody[128];
        snprintf(msgBody, sizeof(msgBody), "The inserted floppy diskette does not appear to be writable. Please check its write-protect switch or try another diskette.", dev);
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(dev, msgBody, lines, 0);
        return 0;
    }
    
    return 1;
}

/**
 * Checks if a given SHORK distro ID is present in the DISTROS global array.
 * @returns 1 if distro present; 0 if not or error
 */
int distroPresent(const char *id)
{
    int found = 0;
    for (int i = 0; i < DISTROS_LEN; i++)
    {
        if (strcmp(DISTROS[i].id, id) == 0)
        {
            found = 1;
            break;
        }
    }
    return found;
}

/**
 * @param imgSize Image size in bytes
 * @param dev Target block device
 * @return 1 if image fits; 0 if not; -1 if error
 */
int imageFitsOnBD(const char *name, long imgSize, char *dev)
{
    // Sanity check against image size
    if (imgSize <= 0)
    {
        char msgBody[256];
        snprintf(msgBody, sizeof(msgBody), "The %s image may be corrupted.", name);
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(dev, msgBody, lines, 0);
        return -1;
    }

    int fd = open(dev, O_RDONLY);
    if (fd == -1)
    {
        char msgBody[86];
        snprintf(msgBody, sizeof(msgBody), "There was an unidentified issue trying to access %s.", dev);
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(dev, msgBody, lines, 0);
        return -1;
    }

    long long devSize = 0;
    if (ioctl(fd, BLKGETSIZE64, &devSize) == -1)
    {
        close(fd);
        char msgBody[86];
        snprintf(msgBody, sizeof(msgBody), "Unable to determine the size of %s.", dev);
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(dev, msgBody, lines, 0);
        return -1;
    }
    close(fd);

    if (imgSize > devSize)
    {
        const char *devType = (strstr(dev, "/dev/fd") != NULL) ? "diskette" : "disk";
        long devSizeMiB = devSize / (1024 * 1024);
        long imgSizeMiB = imgSize / (1024 * 1024);

        char msgBody[512];
        snprintf(msgBody, sizeof(msgBody), "The target %s (%ldMiB) is too small for %s (%ldMiB).\n", devType, devSizeMiB, name, imgSizeMiB);
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(dev, msgBody, lines, 0);
        return 0;
    }

    return 1;
}

/**
 * Prompts the user which block device they want to install SHORK to, confirms
 * if they want to proceed, then writes the an image to the said device dd
 * whilst tracking its progress.
 * @param id Selected distro's ID
 * @param name Selected distro's name
 * @return 1 if successful; 0 if not
 */
int install(const char *id, const char *name)
{
    // Title for yes/no prompt
    char title[128];
    snprintf(title, sizeof(title), "Install %s", name);

    // Target block device path
    char targetBD[10];

    // Get and confirm /dev/fdX 
    if (strstr(id, "diskette") != NULL)
    {
        int fd = showSelectFDXMenu();
        if (fd == -1)
            return 0;
        snprintf(targetBD, 10, "/dev/fd%d", fd);

        char prompt[512];
        snprintf(prompt, sizeof(prompt), "Are you sure you want to install %s to %s? DOING SO WILL ERASE EVERYTHING ON THE INSERTED DISKETTE, so please double-check you have inserted the correct diskette you wish to install to, and backup any potential data on it before proceeding. Choosing \"Yes\" will begin the install process immediately.", name, targetBD);
        if (printYesNoScreen(title, prompt) != 1)
            return 0;

        // Check we have a diskette and it is writable
        if (checkFloppyDrive(targetBD) != 1)
            return 0;
    }
    // Get and confirm /dev/sdX 
    else if (strstr(id, "486") != NULL)
    {
        char sd = showSelectSDXMenu();
        if (sd == '\0')
            return 0;
        snprintf(targetBD, 10, "/dev/sd%c", sd);

        char prompt[512];
        snprintf(prompt, sizeof(prompt), "Are you sure you want to install %s to %s? DOING SO WILL ERASE EVERYTHING ON THIS DRIVE, so please double-check you selected the correct disk you wish to install to, and backup any potential data on it before proceeding. Choosing \"Yes\" will begin the install process immediately.", name, targetBD);
        if (printYesNoScreen(title, prompt) != 1)
            return 0;
    }

    // Install image path
    char installImg[PATH_MAX];
    snprintf(installImg, PATH_MAX, "/root/shork-%s.img", id);

    // Open install image to get its size
    FILE *img = fopen(installImg, "rb");
    if (!img)
    {
        size_t extMsgLen = 29 + strlen(installImg);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: failed to open image %s", installImg);
        exit(1);
    }
    fseek(img, 0, SEEK_END);
    long imgSize = ftell(img);
    rewind(img);
    fclose(img);

    // Check image will fit on target
    if (imageFitsOnBD(name, imgSize, targetBD) != 1)
        return 0;

    // Temporary file for tracking progress (status=progress doesn't exist with
    // BusyBox's dd)
    char tmpProgress[16] = "/tmp/shorkstall";

    // Build dd command
    char ddCmd[PATH_MAX + 256];
    snprintf(ddCmd, sizeof(ddCmd), "dd if=%s of=%s bs=512 2>%s & echo $!", installImg, targetBD, tmpProgress);

    snprintf(title, sizeof(title), "Installing %s to %s...", name, targetBD);

    // Start dd
    FILE *pidStream = popen(ddCmd, "r");
    if (!pidStream)
    {
        EXIT_MSG = "ERROR: failed to start dd";
        exit(1);
    }
    pid_t ddPid = 0;
    fscanf(pidStream, "%d", &ddPid);
    pclose(pidStream);

    if (ddPid <= 0)
    {
        EXIT_MSG = "ERROR: failed to get dd's PID";
        exit(1);
    }

    clearScreen();
    printHeader(title);
    printFooter("Please wait - do not turn off your computer");

    // Poll progress until dd finishes
    while (kill(ddPid, 0) == 0)
    {
        kill(ddPid, SIGUSR1);
        sleep(1);

        FILE *prog = fopen(tmpProgress, "r");
        if (prog)
        {
            char line[80] = {0};
            char lastLine[80] = {0};
            while (fgets(line, sizeof(line), prog))
                strncpy(lastLine, line, sizeof(lastLine) - 1);
            fclose(prog);

            if (lastLine[0])
            {
                lastLine[strcspn(lastLine, "\n")] = '\0';

                if (COL_ENABLED)
                    printf("\033[2;1H%s\033[K", lastLine);
                else 
                    printf("\033[3;1H%s\033[K", lastLine);
                fflush(stdout);
            }
        }
    }

    remove(tmpProgress);
    
    clearScreen();
    char msgBody[512];
    snprintf(msgBody, sizeof(msgBody), "%s has successfully been installed to %s! You may now exit SHORKSTALL, eject this installation media, and restart your computer to start using %s.", name, targetBD, name);
    int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen("Installation complete", msgBody, lines, 0);

    return 1;
}

void showBuildReport(const char *name, const char *report)
{
    char msgTitle[95];
    snprintf(msgTitle, 95, "%s build report", name);
    printHeader(msgTitle);

    char msgBody[4096];
    FILE *stream = fopen(report, "r");
    if (stream)
    {
        size_t bytesRead = fread(msgBody, 1, sizeof(msgBody) - 1, stream);
        fclose(stream);
        msgBody[bytesRead] = '\0';
    }
    else
    {
        size_t extMsgLen = 48 + strlen(report);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: failed to load %s", report);
        exit(1);
    }

    int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen(msgTitle, msgBody, lines, 1);
}

void showDistroActionsMenu(MenuItem distro)
{
    char reportPath[PATH_MAX];
    snprintf(reportPath, PATH_MAX, "/root/shork-%s.txt", distro.id);

    MenuItem rawMenu[] = {
        { "install",    "Install",              "", NULL,   1                      },
        { "min-reqs",   "Minimum requirements", "", NULL,   1                      },
        { "report",     "Build report",         "", NULL,   fileExists(reportPath) },
    };
    int rawMenuSize = sizeof(rawMenu) / sizeof(rawMenu[0]);

    // Filter menu to just what should actually be visible
    MenuItem menu[rawMenuSize];
    int menuSize = 0;
    for (int i = 0; i < rawMenuSize; i++)
        if (rawMenu[i].visible)
            menu[menuSize++] = rawMenu[i];

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader(distro.name);
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                if (strcmp(menu[cursor - 1].id, "install") == 0)
                {
                    if (install(distro.id, distro.name) == 1)
                        return;
                }
                else if (strcmp(menu[cursor - 1].id, "min-reqs") == 0)
                    showMinReqs(distro.id, distro.name);
                else if (strcmp(menu[cursor - 1].id, "report") == 0)
                    showBuildReport(distro.name, reportPath);
                fullRedraw = 1;
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
}

void showHelp(void)
{
    TERM_SIZE = getTerminalSize();

    char desc[140] = "A SHORK Operating System image installer for SHORK DISC.\n";
    formatNewLines(desc, TERM_SIZE.ws_col, NULL, 0);
    printf("%s\n", desc);

    char usage[50] = "Usage: shorkstall [OPTIONS]\n\n";
    formatNewLines(usage, TERM_SIZE.ws_col, NULL, 0);
    printf("%s", usage);

    char help[70] = "-h, --help     Displays help information and exits\n";
    formatNewLines(help, TERM_SIZE.ws_col, "               ", 0);
    printf("%s", help);

    char version[100] = "-v, --version  Displays version number and exits\n";
    formatNewLines(version, TERM_SIZE.ws_col, "               ", 0);
    printf("%s", version);
}

void showMainMenu(void)
{
    setupMenuSys();

    if (TERM_SIZE.ws_col < 40 || TERM_SIZE.ws_row < 10)
    {
        EXIT_MSG = "ERROR: terminal size too small (must be 40x10 or larger)\n";
        exit(1);
    }

    MenuItem rawMenu[] = {
        { "readme",     "Read first",           "", showReadFirst,  1                                                                         },
        { "486-def",    "SHORK 486 (Default)",  "", NULL,           fileExists("/root/shork-486-def.img") && distroPresent("shork-486-def")   },
        { "486-off",    "SHORK 486 (Offline)",  "", NULL,           fileExists("/root/shork-486-off.img") && distroPresent("shork-486-off")   },
        { "486-min",    "SHORK 486 (Minimal)",  "", NULL,           fileExists("/root/shork-486-min.img") && distroPresent("shork-486-min")   },
        { "486-max",    "SHORK 486 (Maximal)",  "", NULL,           fileExists("/root/shork-486-max.img") && distroPresent("shork-486-max")   },
        { "diskette",   "SHORK DISKETTE",       "", NULL,           fileExists("/root/shork-diskette.img") && distroPresent("shork-diskette") }
    };
    int rawMenuSize = sizeof(rawMenu) / sizeof(rawMenu[0]);

    // Filter menu to just what should actually be visible
    MenuItem menu[rawMenuSize];
    int menuSize = 0;
    for (int i = 0; i < rawMenuSize; i++)
        if (rawMenu[i].visible)
            menu[menuSize++] = rawMenu[i];

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("SHORKSTALL");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Quit");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                if (menu[cursor - 1].action != NULL)
                    menu[cursor - 1].action();
                else
                    showDistroActionsMenu(menu[cursor - 1]);
                fullRedraw = 1;
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
}

void showMinReqs(const char *id, const char *name)
{
    char distroID[strlen(id) + 7];
    snprintf(distroID, sizeof(distroID), "shork-%s", id);

    int distroInd = -1;
    for (int i = 0; i < DISTROS_LEN; i++)
    {
        if (strcmp(DISTROS[i].id, distroID) == 0)
        {
            distroInd = i;
            break;
        }
    }

    if (distroInd >= 0)
    {
        char msgTitle[95];
        snprintf(msgTitle, 95, "%s minimum requirements", name);

        char msgBody[512];
        snprintf(msgBody, 512, "* %s\n* %s\n* %s\n* %s\n* %s\n", DISTROS[distroInd].cpu, DISTROS[distroInd].ram, DISTROS[distroInd].storage, DISTROS[distroInd].gpu, DISTROS[distroInd].display);

        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(msgTitle, msgBody, lines, 1);
    }
}

void showReadFirst(void)
{
    const int strSize = 1024;
    char pt1Str[strSize];
    int pos = 0;

    pos += snprintf(pt1Str + pos, strSize - pos, "SHORK INSTALLER (SHORKSTALL)...");

    int lines = formatNewLines(pt1Str, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen("Read first", pt1Str, lines, 1);
}

/**
 * @return Floppy drive device number; -1 if no device fount/error
 */
int showSelectFDXMenu(void)
{
    MenuItem menu[8];
    int menuSize = 0;

    for (int i = 0; i < 8; i++)
    {
        char fdXPath[PATH_MAX];
        snprintf(fdXPath, PATH_MAX, "/dev/fd%d", i);
        if (access(fdXPath, F_OK) == 0)
        {
            snprintf(menu[menuSize].id, sizeof(menu[menuSize].id), "%d", i);
            snprintf(menu[menuSize].name, sizeof(menu[menuSize].name), "/dev/fd%d", i);
            snprintf(menu[menuSize].payload, sizeof(menu[menuSize].payload), "%s", fdXPath);
            menu[menuSize].action  = NULL;
            menu[menuSize].visible = 1;
            menuSize++;
        }
    }

    if (menuSize == 0)
    {
        char msgBody[256] = "No floppy drive device (/dev/fdX) was found. When it is safe to do so, please ensure a floppy drive connected to your computer and try again.";
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen("No floppy devices found", msgBody, lines, 0);
        return -1;
    }

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select floppy drive device");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                return atoi(menu[cursor - 1].id);
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
    return -1;
}

/**
 * @return Fixed drive device letter; '\0' if no device fount/error
 */
char showSelectSDXMenu(void)
{
    MenuItem menu[26];
    int menuSize = 0;

    for (int i = 0; i < 26; i++)
    {
        char sdXPath[PATH_MAX];
        snprintf(sdXPath, PATH_MAX, "/dev/sd%c", 'a' + i);
        if (access(sdXPath, F_OK) == 0)
        {
            snprintf(menu[menuSize].id, sizeof(menu[menuSize].id), "%d", i);
            snprintf(menu[menuSize].name, sizeof(menu[menuSize].name), "/dev/sd%c", 'a' + i);
            snprintf(menu[menuSize].payload, sizeof(menu[menuSize].payload), "%s", sdXPath);
            menu[menuSize].action  = NULL;
            menu[menuSize].visible = 1;
            menuSize++;
        }
    }

    if (menuSize == 0)
    {
        char msgBody[256] = "No fixed drive device (/dev/sdX) was found. When it is safe to do so, please ensure a hard or solid-state disk is connected to your computer and try again.";
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen("No fixed drive devices found", msgBody, lines, 0);
        return '\0';
    }

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select fixed drive device");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                return 'a' + atoi(menu[cursor - 1].id);
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
    return '\0';
}



/*
void applyFontColFiles(char *ansi)
{
    char cmd[256];

    // Write to /etc/profile
    snprintf(cmd, 256, "sed -i 's|\\\\033\\[[0-9;]*m|\\\\033[%sm|g' /etc/profile", ansi);
    system(cmd);

    // Write to etc/init.d/rc
    snprintf(cmd, 256, "sed -i 's|\\\\033\\[[0-9;]*m|\\\\033[%sm|g' /etc/init.d/rc", ansi);
    system(cmd);

    // Write to terminfo.src
    //snprintf(cmd, 256, "sed -i 's|\\\\E\\[[0-9;]*m|\\\\E[%sm|g' /usr/share/terminfo/src/terminfo.src", ascii);
    //system(cmd);

    // Rebuild terminfo
    //system("tic -x -1 -o /usr/share/terminfo /usr/share/terminfo/src/terminfo.src");
}

void applyFontColTtys(char *ansi)
{
    // TODO: dynamically get number of TTYs
    // SHORK 486 always has 3 at least...
    for (int t = 1; t <= 3; t++)
    {
        char ttyPath[32];
        snprintf(ttyPath, sizeof(ttyPath), "/dev/tty%d", t);

        int tty = open(ttyPath, O_WRONLY | O_NOCTTY);
        if (tty < 0)
            continue;

        dprintf(tty, "\033[%sm", ansi);
        close(tty);
    }
}

void getCurrRes(void)
{
    CONFIG.dispRes = -1;

    // Find and select a bootloader cfg file
    const char *cfg = NULL;
    for (int i = 0; i < CFG_PATHS_LEN; i++)
    {
        if (access(CFG_PATHS[i], F_OK) == 0)
        {
            cfg = CFG_PATHS[i];
            break;
        }
    }

    // If no cfg found, time to exit...
    if (!cfg)
    {
        EXIT_MSG = strdup("ERROR: no valid bootloader configuration file was found\n");
        exit(1);
    }

    // Open stream to cfg
    FILE *stream = fopen(cfg, "r");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to open bootloader configuration file\n");
        exit(1);
    }

    // Find and extract the "vga=" parameter's values
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, stream) != -1)
    {
        char *needle = strstr(line, "vga=");
        if (!needle)
            continue;

        char *val = needle + 4;
        if (strncmp(val, "ask", 3) == 0 || strncmp(val, "normal", 6) == 0)
            CONFIG.dispRes = 3840;
        else
            CONFIG.dispRes = (int)strtol(val, NULL, 10);
        break;
    }
    free(line);
    fclose(stream);

    // Catch if unsuccessful
    if (CONFIG.dispRes == -1)
    {
        EXIT_MSG = strdup("ERROR: unable to determine the current resolution from bootloader configuration file\n");
        exit(1);
    }
}

void loadConf(void)
{
    FILE *stream = fopen(DOT_CONF, "r");
    if (stream)
    {
        // Buffer size has to be PATH_MAX for FONT_PSF and KEYMAP, and +15 to
        // take into account the variable and null terminator
        char buffer[PATH_MAX + 15];
        while (fgets(buffer, sizeof(buffer), stream))
        {
            buffer[strcspn(buffer, "\r\n")] = '\0';

            char *value = strchr(buffer, '=');
            if (!value)
                continue;

            value++;
            if (*value == '"')
            {
                value++;
                char *end = strrchr(value, '"');
                if (end) *end = '\0';
            }
   
            if (strncmp(buffer, "DISP_RES=", 9) == 0)
                CONFIG.dispRes = atoi(value);
            else if (strncmp(buffer, "FONT_COL_NAME=", 14) == 0)
                snprintf(CONFIG.fontColName, sizeof(CONFIG.fontColName), "%s", value);
            else if (strncmp(buffer, "FONT_COL_ANSI=", 14) == 0)
                snprintf(CONFIG.fontColANSI, sizeof(CONFIG.fontColANSI), "%s", value);
            else if (strncmp(buffer, "FONT_PSF=", 9) == 0)
                snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "%s", value);
            else if (strncmp(buffer, "KEYMAP=", 7) == 0)
                snprintf(CONFIG.keymap, sizeof(CONFIG.keymap), "%s", value);
        }
        fclose(stream);
    }
    else
    {
        size_t extMsgLen = 48 + strlen(DOT_CONF);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: failed to load %s", DOT_CONF);
        exit(1);
    }
}

int loadConFonts(void)
{
    struct stat st;
    if (stat(CONFONTS_DIR, &st) != 0 || !S_ISDIR(st.st_mode))
        return 0;

    DIR *dir = opendir(CONFONTS_DIR);
    if (!dir)
    {
        size_t extMsgLen = 48 + strlen(CONFONTS_DIR);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: could not access %s", CONFONTS_DIR);
        exit(1);
    }

    CONFONTS_COUNT = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        if (CONFONTS_COUNT >= MAX_FONTS)
            break;
            
        snprintf(CONFONTS[CONFONTS_COUNT], PATH_MAX, "%s/%s", CONFONTS_DIR, entry->d_name);
        CONFONTS_COUNT++;
    }
    closedir(dir);

    qsort(CONFONTS, CONFONTS_COUNT, PATH_MAX, natCmp);
    return CONFONTS_COUNT > 0;
}

int loadKeymaps(void)
{
    struct stat st;
    if (stat(KEYMAPS_DIR, &st) != 0 || !S_ISDIR(st.st_mode))
        return 0;

    DIR *dir = opendir(KEYMAPS_DIR);
    if (!dir)
    {
        size_t extMsgLen = 48 + strlen(KEYMAPS_DIR);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: could not access %s", KEYMAPS_DIR);
        exit(1);
    }

    KEYMAPS_COUNT = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        if (KEYMAPS_COUNT >= MAX_FONTS)
            break;
            
        snprintf(KEYMAPS[KEYMAPS_COUNT], PATH_MAX, "%s/%s", KEYMAPS_DIR, entry->d_name);
        KEYMAPS_COUNT++;
    }
    closedir(dir);

    qsort(KEYMAPS, KEYMAPS_COUNT, PATH_MAX, natCmp);
    return KEYMAPS_COUNT > 0;
}

void saveDispRes(MenuItem itm, int skipMsg)
{
    // Find and select a bootloader cfg file
    const char *cfg = NULL;
    for (int i = 0; i < CFG_PATHS_LEN; i++)
    {
        if (access(CFG_PATHS[i], F_OK) == 0)
        {
            cfg = CFG_PATHS[i];
            break;
        }
    }

    // If no cfg found, time to exit...
    if (!cfg)
    {
        EXIT_MSG = strdup("ERROR: no valid bootloader configuration file was found\n");
        exit(1);
    }

    // Open stream to cfg
    FILE *stream = fopen(cfg, "r");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to open bootloader configuration file\n");
        exit(1);
    }

    // Get cfg's contents length
    fseek(stream, 0, SEEK_END);
    size_t size = ftell(stream);
    rewind(stream);

    // Read cfg into buffer
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, stream);
    buffer[size] = '\0';
    fclose(stream);

    // Ensure vga param is in the buffer
    char *vgaNeedle = strstr(buffer, "vga=");
    if (!vgaNeedle)
    {
        EXIT_MSG = strdup("ERROR: bootloader configuration missing the \"vga\" kernel parameter\n");
        free(buffer);
        exit(1);
    }

    // Get end of where we need to patch
    char *end = vgaNeedle + 4;
    while (*end && *end != ' ' && *end != '\n')
        end++;

    // Assemble the patch and result string
    char mode[5];
    int modeLen = snprintf(mode, 5, "%s", itm.id);
    size_t patchedSize = size - (end - (vgaNeedle + 4)) + modeLen;
    char *result = malloc(patchedSize + 1);
    size_t prefix = vgaNeedle + 4 - buffer;

    // Make the patch
    memcpy(result, buffer, prefix);
    memcpy(result + prefix, mode, modeLen);
    strcpy(result + prefix + modeLen, end);
    free(buffer);

    // Save the new cfg
    stream = fopen(cfg, "w");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to write bootloader configuration file\n");
        free(result);
        exit(1);
    }
    fwrite(result, 1, patchedSize, stream);

    fclose(stream);
    free(result);
    CONFIG.dispRes = atoi(itm.id);
    // If a VGA resolution was selected, we also have to reset the PSF font to
    // "default" since they normally dictate their own VGA resolution
    if (!skipMsg && CONFIG.dispRes >= 3840)
        snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "default");
    writeConf();

    if (!skipMsg)
    {
        char msgTitle[80];
        snprintf(msgTitle, 80, "%s", itm.name);
        char msgBody[320] = "The selected display resolution has been saved. If you selected a VGA resolution and had selected a PSF font before, the latter setting will now be discarded as PSF fonts dictate their own VGA resolution. A system restart is required before the changes will take effect.";
        formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printGenericScreen(msgTitle, msgBody);
    }
}

void saveFontCol(MenuItem itm)
{
    applyFontColFiles(itm.id);
    applyFontColTtys(itm.id);

    snprintf(CONFIG.fontColName, sizeof(CONFIG.fontColName), "%s", itm.payload);
    snprintf(CONFIG.fontColANSI, sizeof(CONFIG.fontColANSI), "%s", itm.id);
    writeConf();

    char msgTitle[80];
    snprintf(msgTitle, 80, "%s", itm.payload);
    char msgBody[320] = "The selected font colour has been saved and will be applied once you exit SHORKSET. If there are any other active virtual terminals (ttyX), you may need to enter \"exit\" when convenient, or restart your computer before this change will take complete effect.";
    formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printGenericScreen(msgTitle, msgBody);
}

void saveFontPSF(MenuItem itm)
{   
    if (strcmp(itm.id, "default") == 0)
    {
        snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "default");
        writeConf();
        
        char msgTitle[80] = "default";
        char msgBody[320] = "The PSF font will be reset to default. If a PSF font other than \"default\" was previously selected, you must restart your computer before this change will take effect.";
        formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printGenericScreen(msgTitle, msgBody);
    }
    else
    {
        char cmd[PATH_MAX + 9];
        snprintf(cmd, sizeof(cmd), "setfont %s", itm.payload);
        system(cmd);
        setupViewport();

        snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "%s", itm.payload);
        // If VGA font, we need to reset to default since PSF fonts override VGA
        // resolutions
        if (CONFIG.dispRes >= 3840)
            saveDispRes((MenuItem){ "3840", "", "", NULL, 1 }, 1);
        // saveDispRes will call writeConf anyway, only needed if not resetting
        // display resolution
        else
            writeConf();

        char msgTitle[80];
        snprintf(msgTitle, 80, "%s", itm.name);
        char msgBody[320] = "The selected PSF font has been saved and will be applied once you exit SHORKSET. If you had selected a VGA display resolution before, that setting will now be discarded as the PSF font will dictate its own VGA resolution. VBE display resolutions are unaffected.";
        formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printGenericScreen(msgTitle, msgBody);
    }
}

void saveKeymap(MenuItem itm)
{   
    char cmd[PATH_MAX + 12];
    snprintf(cmd, sizeof(cmd), "loadkmap < %s", itm.payload);
    system(cmd);

    snprintf(CONFIG.keymap, sizeof(CONFIG.keymap), "%s", itm.id);
    writeConf();

    char msgTitle[80];
    snprintf(msgTitle, 80, "%s", itm.name);
    char msgBody[320] = "The selected keyboard layout has been applied.";
    formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printGenericScreen(msgTitle, msgBody);
}

void showDispResMenu(void)
{
    MenuItem menu[] = {
        { "3840",   "VGA: 80x25",               "", NULL,   1 },
        { "3843",   "VGA: 80x28",               "", NULL,   1 },
        { "3846",   "VGA: 80x34",               "", NULL,   1 },
        { "3842",   "VGA: 80x43",               "", NULL,   1 },
        { "3841",   "VGA: 80x50",               "", NULL,   1 },
        { "3847",   "VGA: 80x60",               "", NULL,   1 },
#ifdef FB
        { "782",    "VBE: 320x200 (CGA)",       "", NULL,   1 },
        { "785",    "VBE: 640x480 (VGA)",       "", NULL,   1 },
        { "788",    "VBE: 800x600 (SVGA)",      "", NULL,   1 },
        { "791",    "VBE: 1024x768 (XGA)",      "", NULL,   1 },
        { "794",    "VBE: 1280x1024 (SXGA)",    "", NULL,   1 },
        { "837",    "VBE: 1400x1050 (SXGA+)",   "", NULL,   1 },
        { "838",    "VBE: 1600x1200 (UXGA)",    "", NULL,   1 },
#endif
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    // Mark the current resolution
    for (int i = 0; i < menuSize; i++)
    {
        if (atoi(menu[i].id) == CONFIG.dispRes)
        {
            strcat(menu[i].name, "*");
            cursor = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select display resolution");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                saveDispRes(menu[cursor - 1], 0);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (atoi(menu[i].id) == CONFIG.dispRes)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursor = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
}

void showFontColMenu(void)
{
    MenuItem menu[] = {
        { "0;34",   "\033[0;34mBlue\033[0m",                "blue",             NULL,   1 },
        { "0;1;34", "\033[0;1;34mBright blue\033[0m",       "blue_bright",      NULL,   1 },
        { "0;36",   "\033[0;36mCyan\033[0m",                "cyan",             NULL,   1 },
        { "0;1;36", "\033[0;1;36mBright cyan\033[0m",       "cyan_bright",      NULL,   1 },
        { "0;32",   "\033[0;32mGreen\033[0m",               "green",            NULL,   1 },
        { "0;1;32", "\033[0;1;32mBright green\033[0m",      "green_bright",     NULL,   1 },
        { "0;1;30", "\033[0;1;30mGrey\033[0m",              "grey",             NULL,   1 },
        { "0;35",   "\033[0;35mMagenta\033[0m",             "magenta",          NULL,   1 },
        { "0;1;35", "\033[0;1;35mBright magenta\033[0m",    "magneta_bright",   NULL,   1 },
        { "0;31",   "\033[0;31mRed\033[0m",                 "red",              NULL,   1 },
        { "0;1;31", "\033[0;1;31mBright red\033[0m",        "red_bright",       NULL,   1 },
        { "0;37",   "\033[0;37mWhite\033[0m",               "white",            NULL,   1 },
        { "0;1;37", "\033[0;1;37mBright white\033[0m",      "white_bright",     NULL,   1 },
        { "0;33",   "\033[0;33mYellow\033[0m",              "yellow",           NULL,   1 },
        { "0;1;33", "\033[0;1;33mBright yellow\033[0m",     "yellow_bright",    NULL,   1 },
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    // Mark the current colour
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].payload, CONFIG.fontColName) == 0)
        {
            strcat(menu[i].name, "*");
            cursor = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select font colour");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                saveFontCol(menu[cursor - 1]);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (strcmp(menu[i].payload, CONFIG.fontColName) == 0)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursor = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
}

void showFontPSFMenu(void)
{
    MenuItem menu[MAX_FONTS + 1];
    menu[0] = (MenuItem){ "default", "Default", "default", NULL, 1 };
    int menuSize = 1;
    
    for(int i = 0; i < CONFONTS_COUNT; i++)
    {
        char *base = basename(CONFONTS[i]);

        // Strip extension
        char nameStr[129];
        strncpy(nameStr, base, 128);
        nameStr[128] = '\0';
        char *dot = strrchr(nameStr, '.');
        if (dot)
            *dot = '\0';

        // Trim to 100 chars and add "..." if needed
        if (strlen(nameStr) > 100)
        {
            nameStr[100] = '\0';
            strcat(nameStr, "...");
        }

        // Add font
        snprintf(menu[menuSize].id, sizeof(menu[menuSize].id), "%s", nameStr);
        snprintf(menu[menuSize].name, sizeof(menu[menuSize].name), "%s", nameStr);
        snprintf(menu[menuSize].payload, sizeof(menu[menuSize].payload), "%s", CONFONTS[i]);
        menu[menuSize].action = NULL;
        menu[menuSize].visible = 1;
        menuSize++;
    }

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    // Mark the current colour
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].payload, CONFIG.fontPSF) == 0)
        {
            strcat(menu[i].name, "*");
            cursor = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select font (PSF)");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                saveFontPSF(menu[cursor - 1]);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (strcmp(menu[i].payload, CONFIG.fontPSF) == 0)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursor = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
}

void showKeymapMenu(void)
{
    MenuItem menu[MAX_KEYMAPS + 1];
    int menuSize = 0;
    for(int i = 0; i < KEYMAPS_COUNT; i++)
    {
        char *base = basename(KEYMAPS[i]);

        // Strip extension
        char nameStr[129];
        strncpy(nameStr, base, 128);
        nameStr[128] = '\0';
        char *dot = strrchr(nameStr, '.');
        if (dot)
        {
            *dot = '\0';
            dot = strrchr(nameStr, '.');
            if (dot && (strcmp(dot, ".kmap") == 0 || strcmp(dot, ".bin") == 0))
                *dot = '\0';
        }

        // Trim to 100 chars and add "..." if needed
        if (strlen(nameStr) > 100)
        {
            nameStr[100] = '\0';
            strcat(nameStr, "...");
        }

        // Add font
        snprintf(menu[menuSize].id, sizeof(menu[menuSize].id), "%s", nameStr);
        snprintf(menu[menuSize].name, sizeof(menu[menuSize].name), "%s", nameStr);
        snprintf(menu[menuSize].payload, sizeof(menu[menuSize].payload), "%s", KEYMAPS[i]);
        menu[menuSize].action = NULL;
        menu[menuSize].visible = 1;
        menuSize++;
    }

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    // Mark the current colour
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].id, CONFIG.keymap) == 0)
        {
            strcat(menu[i].name, "*");
            cursor = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select keyboard layout");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                saveKeymap(menu[cursor - 1]);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (strcmp(menu[i].id, CONFIG.keymap) == 0)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursor = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
}

void writeConf(void)
{
    FILE *stream = fopen(DOT_CONF, "w");
    if (!stream)
    {
        size_t extMsgLen = 48 + strlen(DOT_CONF);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: failed to write %s", DOT_CONF);
        exit(1);
    }

    fprintf(stream, "DISP_RES=%d\n", CONFIG.dispRes);
    fprintf(stream, "FONT_COL_NAME=\"%s\"\n", CONFIG.fontColName);
    fprintf(stream, "FONT_COL_ANSI=\"%s\"\n", CONFIG.fontColANSI);
    fprintf(stream, "FONT_PSF=\"%s\"\n", CONFIG.fontPSF);
    fprintf(stream, "KEYMAP=\"%s\"\n", CONFIG.keymap);
    fclose(stream);
}
*/
