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
    snprintf(installImg, PATH_MAX, "/root/%s.img", id);

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

void showBuildReport(int distroInd, const char *reportPath)
{
    char msgTitle[95];
    snprintf(msgTitle, 95, "%s build report", DISTROS[distroInd].name);
    printHeader(msgTitle);

    char msgBody[4096];
    FILE *stream = fopen(reportPath, "r");
    if (stream)
    {
        size_t bytesRead = fread(msgBody, 1, sizeof(msgBody) - 1, stream);
        fclose(stream);
        msgBody[bytesRead] = '\0';
    }
    else
    {
        size_t extMsgLen = 48 + strlen(reportPath);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: failed to load %s", reportPath);
        exit(1);
    }

    int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen(msgTitle, msgBody, lines, 1);
}

void showDistroActionsMenu(MenuItem distro)
{
    int distroInd = -1;
    for (int i = 0; i < DISTROS_LEN; i++)
    {
        if (strcmp(DISTROS[i].id, distro.id) == 0)
        {
            distroInd = i;
            break;
        }
    }

    if (distroInd < 0)
    {
        EXIT_MSG = "ERROR: could not find selected distro";
        exit(1);
    }

    char reportPath[PATH_MAX];
    snprintf(reportPath, PATH_MAX, "/root/%s.txt", DISTROS[distroInd].id);

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
            printMenu(menu, menuSize, DISTROS[distroInd].desc, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, DISTROS[distroInd].desc, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
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
                    showMinReqs(distroInd);
                else if (strcmp(menu[cursor - 1].id, "report") == 0)
                    showBuildReport(distroInd, reportPath);
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
        { "readme",             "Read first",           "", showReadFirst,  1                                                                         },
        { "shork-486-def",      "SHORK 486 (Default)",  "", NULL,           fileExists("/root/shork-486-def.img") && distroPresent("shork-486-def")   },
        { "shork-486-off",      "SHORK 486 (Offline)",  "", NULL,           fileExists("/root/shork-486-off.img") && distroPresent("shork-486-off")   },
        { "shork-486-min",      "SHORK 486 (Minimal)",  "", NULL,           fileExists("/root/shork-486-min.img") && distroPresent("shork-486-min")   },
        { "shork-486-max",      "SHORK 486 (Maximal)",  "", NULL,           fileExists("/root/shork-486-max.img") && distroPresent("shork-486-max")   },
        { "shork-diskette",     "SHORK DISKETTE",       "", NULL,           fileExists("/root/shork-diskette.img") && distroPresent("shork-diskette") }
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

void showMinReqs(int distroInd)
{
    char msgTitle[95];
    snprintf(msgTitle, 95, "%s minimum requirements", DISTROS[distroInd].name);

    char msgBody[512];
    snprintf(msgBody, 512, " * %s\n * %s\n * %s\n * %s\n * %s\n", DISTROS[distroInd].cpu, DISTROS[distroInd].ram, DISTROS[distroInd].storage, DISTROS[distroInd].gpu, DISTROS[distroInd].display);

    int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen(msgTitle, msgBody, lines, 1);
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
