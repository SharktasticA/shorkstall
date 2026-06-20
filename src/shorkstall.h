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
    char *name;
    char *desc;
    char *cpu;
    char *ram;
    char *storage;
    char *gpu;
    char *display;
} Distro;



static const Distro DISTROS[] = {
    {
        "shork-486-min",
        "SHORK 486 (Minimal)",
        "SHORK 486 proper is the main version of the SHORK 486 Operating System, designed to be installed on a hard or solid-state disk. The \"minimal\" variant is built with a minimalist configuration for extremely low memory requirements and excludes all optional software.",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 8MiB minimum/10MiB recommended",
        "Disk: ~8MiB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    },
    {
        "shork-486-off",
        "SHORK 486 (Offline)",
        "SHORK 486 proper is the main version of the SHORK 486 Operating System, designed to be installed on a hard or solid-state disk. The \"offline\" variant is the same as the \"default\" variant but lacks any networking support and software.",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 12MiB minimum/20MiB recommended",
        "Disk: ~60MiB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    },
    {
        "shork-486-def",
        "SHORK 486 (Default)",
        "SHORK 486 proper is the main version of the SHORK 486 Operating System, designed to be installed on a hard or solid-state disk. This is the author's recommended version, designed to balance features and bundled software variety with system requirements.",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 16MiB minimum/24MiB recommended",
        "Disk: ~80MiB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    },
    {
        "shork-486-max",
        "SHORK 486 (Maximal)",
        "SHORK 486 proper is the main version of the SHORK 486 Operating System, designed to be installed on a hard or solid-state disk. The \"maximal\" variant includes all optional software and drivers, so it can be used on systems newer than 486/586.",
        "Processor: Intel 486SX or compatible (no FPU required)",
        "Memory: 24MiB minimum/32MiB recommended",
        "Disk: ~440MiB",
        "Graphics: IBM VGA or compatible",
        "Monitor: VGA (640x480) or higher"
    },
    {
        "shork-diskette",
        "SHORK DISKETE",
        "SHORK DISKETTE is a version of the SHORK 486 Operating System, designed to be written to a 1.44MB floppy diskette.",
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
void showBuildReport(int, const char*);
void showDistroActionsMenu(MenuItem);
void showHelp(void);
void showMainMenu(void);
void showMinReqs(int);
void showReadFirst(void);
int showSelectFDXMenu(void);
char showSelectSDXMenu(void);

#endif
