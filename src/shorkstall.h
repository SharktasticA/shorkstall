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



#ifndef SHORKSTALL
#define SHORKSTALL

#include "shorkmenu.h"



typedef struct 
{
    char *id;
    char *cpu;
    char *ram;
    char *storage;
    char *gpu;
    char *display;
} Distro;



static const Distro DISTROS[] = {
    {
        "shork-486-min",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 8MiB minimum/10MiB recommended",
        "Disk: ~8MiB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    },
    {
        "shork-486-def",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 16MiB minimum/24MiB recommended",
        "Disk: ~80MiB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    },
    {
        "shork-486-max",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 24MiB minimum/32MiB recommended",
        "Disk: ~440MiB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    },
    {
        "shork-diskette",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 16MiB minimum/24MiB recommended",
        "Diskette: 1.44MB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    }
};
static const int DISTROS_LEN = sizeof(DISTROS) / sizeof(DISTROS[0]);



int checkFloppyDrive(char*);
int distroPresent(const char*);
int imageFitsOnBD(const char*, long, char*);
int install(const char*, const char*);
void showBuildReport(const char*, const char*);
void showDistroActionsMenu(MenuItem);
void showHelp(void);
void showMainMenu(void);
void showMinReqs(const char*, const char*);
void showReadFirst(void);
int showSelectFDXMenu(void);
char showSelectSDXMenu(void);

#endif
