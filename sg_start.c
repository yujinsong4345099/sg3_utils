#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "sg_lib.h"
#include "sg_cmds.h"

/*
 *  Copyright (C) 1999-2006 D. Gilbert
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.

    Since this code has been used in the past to form the backbone of
    some Linux apps based on the "sg" device driver, it has been
    strengthened.

    Start/Stop parameter by Kurt Garloff <garloff at suse dot de>, 6/2000
    Sync cache parameter by Kurt Garloff <garloff at suse dot de>, 1/2001
    Guard block device answering sg's ioctls. 
                     <dgilbert at interlog dot com> 12/2002
    Convert to SG_IO ioctl so can use sg or block devices in 2.6.* 3/2003
 
*/

static char * version_str = "0.48 20060125";


void usage ()
{
        fprintf(stderr, "Usage:  sg_start [0|--stop|1|--start] [--eject] "
                "[--fl=<n>] [--imm=0|1]\n"
                "                 [--load] [--loej] [--pc=<n>] [-v] [-V] "
                "<device>\n"
                " where: 0         stop unit (e.g. spin down a disk or a "
                "cd/dvd)\n"
                "        1         start unit (e.g. spin up a disk or a "
                "cd/dvd)\n"
                "        --eject   stop then eject the medium\n"
                "        --fl=<n>  format layer number (mmc5)\n"
                "        --imm=0|1   0->await completion(def), 1->return "
                "immediately\n"
                "        --load    load then start the medium\n"
                "        --loej    load the medium if '-start' option is "
                "also given\n"
                "                  or stop unit and eject\n"
                "        --pc=<n>  power conditions (in hex, default 0 -> no "
                "power condition)\n"
                "                  1 -> active, 2 -> idle, 3 -> standby, "
                "5 -> sleep (MMC)\n"
                "        --start   start unit (same as '1'), default "
                "action\n"
                "        --stop    stop unit (same as '0')\n"
                "        -v        verbose (print out SCSI commands)\n"
                "        -V        print version string then exit\n\n"
                "    Example: 'sg_start --stop /dev/sdb'    stops unit\n"
                "             'sg_start --eject /dev/scd0'  stops unit and "
                "ejects medium\n\n"
                "Performs a START STOP UNIT SCSI command\n"
                );
        exit (1);
}

int main(int argc, char * argv[])
{
        int startstop = -1;
        const char * file_name = 0;
        const char * cp;
        int k, fd, num, res, plen, jmp_out;
        unsigned int u;
        int ambigu = 0;
        int immed = 0;
        int loej = 0;
        int fl_num = -1;
        int power_conds = 0;
        int verbose = 0;
        
        for (k = 1; k < argc; ++k) {
                cp = argv[k];
                plen = strlen(cp);
                if (plen <= 0)
                        continue;
                if ('-' == *cp) {
                        for (--plen, ++cp, jmp_out = 0; plen > 0;
                             --plen, ++cp) {
                                switch (*cp) {
                                case 'v':
                                        ++verbose;
                                        break;
                                case 'V':
                                        fprintf(stderr, "Version string: "
                                                "%s\n", version_str);
                                        exit(0);
                                case '?':
                                        usage();
                                        return 1;
                                case '-':
                                        ++cp;
                                        --plen;
                                        jmp_out = 1;
                                        break;
                                default:
                                        jmp_out = 1;
                                        break;
                                }
                                if (jmp_out)
                                        break;
                        }
                        if (plen <= 0)
                                continue;

                        if (0 == strncmp(cp, "eject", 5)) {
                                loej = 1;
                                if (startstop == 1)
                                        ambigu = 1;
                                else
                                        startstop = 0;
                        } else if (0 == strncmp("fl=", cp, 3)) {
                                num = sscanf(cp + 3, "%x", &u);
                                if (1 != num) {
                                        fprintf(stderr, "Bad value after "
                                                "'fl=' option\n");
                                        usage();
                                        return 1;
                                }
                                fl_num = u;
                        } else if (0 == strncmp("imm=", cp, 4)) {
                                num = sscanf(cp + 4, "%x", &u);
                                if ((1 != num) || (u > 1)) {
                                        fprintf(stderr, "Bad value after "
                                                "'imm=' option\n");
                                        usage();
                                        return 1;
                                }
                                immed = u;
                        } else if (0 == strncmp(cp, "load", 4)) {
                                loej = 1;
                                if (startstop == 0)
                                        ambigu = 1;
                                else
                                        startstop = 1;
                        } else if (0 == strncmp(cp, "loej", 4))
                                loej = 1;
                        else if (0 == strncmp("pc=", cp, 3)) {
                                num = sscanf(cp + 3, "%x", &u);
                                if ((1 != num) || (u > 15)) {
                                        fprintf(stderr, "Bad value after "
                                                "after 'pc=' option\n");
                                        usage();
                                        return 1;
                                }
                                power_conds = u;
                        } else if (0 == strncmp(cp, "start", 5)) {
                                if (startstop == 0)
                                        ambigu = 1;
                                else
                                        startstop = 1;
                        } else if (0 == strncmp(cp, "stop", 4)) {
                                if (startstop == 1)
                                        ambigu = 1;
                                else
                                        startstop = 0;
                        } else if (jmp_out) {
                                fprintf(stderr, "Unrecognized option: %s\n",
                                        cp);
                                usage();
                                return 1;
                        }
                } else if (0 == strcmp("0", cp)) {
                        if (startstop >= 0)
                                ambigu = 1;
                        else
                                startstop = 0;
                } else if (0 == strcmp("1", cp)) {
                        if (startstop >= 0)
                                ambigu = 1;
                        else
                                startstop = 1;
                } else if (0 == file_name)
                        file_name = cp;
                else {
                        fprintf(stderr, "too many arguments, got: %s, not "
                                "expecting: %s\n", file_name, cp);
                        usage();
                        return 1;
                }
                if (ambigu) {
                        fprintf(stderr, "please, only one of 0, 1, --eject, "
                                "--load, --start or --stop\n");
                        usage();
                        return 1;
                }
        }
    
        if (0 == file_name) {
                fprintf(stderr, "No <scsi_device> argument given\n");
                usage();
                return 1;
        }

        if (fl_num >= 0) {
                if (startstop == 0) {
                        fprintf(stderr, "Giving '--fl=<n>' and '--stop' (or "
                                "'--eject') is invalid\n");
                        usage();
                        return 1;
                }
                if (power_conds > 0) {
                        fprintf(stderr, "Giving '--fl=<n>' and '--pc=<n>' "
                                "when <n> is non-zero is invalid\n");
                        usage();
                        return 1;
                }
        } else {
                if ((startstop == -1) && loej)
                        startstop = 0;
                if ((startstop == -1) && (0 == power_conds))
                        startstop = 1;
        }
                
        fd = sg_cmds_open_device(file_name, 0 /* rw */, verbose);
        if (fd < 0) {
                fprintf(stderr, "Error trying to open %s: %s\n",
                        file_name, safe_strerror(-fd));
                return 2;
        }

        res = 0;
        if (fl_num >= 0)
                res = sg_ll_start_stop_unit(fd, immed, fl_num, power_conds,
                                            1 /* fl */, 1 /* loej */,
                                            1 /*start */, 1 /* noisy */,
                                            verbose);
        else if (power_conds > 0)
                res = sg_ll_start_stop_unit(fd, immed, 0, power_conds, 0, 0,
                                            0, 1, verbose);
        else if (startstop != -1)
                res = sg_ll_start_stop_unit(fd, immed, 0, 0, 0, loej,
                                            startstop, 1, verbose);
        if (res) {
                if (verbose < 2) {
                        if (SG_LIB_CAT_INVALID_OP == res)
                                fprintf(stderr, "command not supported\n");
                        else if (SG_LIB_CAT_ILLEGAL_REQ == res)
                                fprintf(stderr, "command malformed\n");
                }
                fprintf(stderr, "START STOP UNIT command failed\n");
        }
        sg_cmds_close_device(fd);
        return res ? 1 : 0;
}
