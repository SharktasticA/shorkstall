/*
    ######################################################
    ##            SHORK UTILITY - SHORKSTALL            ##
    ######################################################
    ## A SHORK Operating System image installer for     ##
    ## SHORK DISC                                       ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



static const char *VERSION = "1.0-pt1";



#include "shorkstall.h"

#include <stdio.h>
#include <string.h>



int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            showHelp();
            return 0;
        }
        else if ((strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--version") == 0))
        {
            printf("SHORKSTALL %s\n", VERSION);
            return 0;
        }
        else
        {
            showHelp();
            return 0;
        }
    }

    showMainMenu();
    return 0;
}
