
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
*/  



#include <tsload/defs.h>

#include <tsload/getopt.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


LIBEXPORT int opterr = 0;
LIBEXPORT int optind = 1;
LIBEXPORT int optopt = 0;
LIBEXPORT char *optarg = NULL;

#define OPT_CHAR	'-'
#define BAD_CHAR 	'?'
#define ARG_CHAR	':'

#define OPT_OK		0
#define OPT_MULT	1
#define OPT_FAIL 	2

int opt_state = OPT_OK;

/* TODO: Support for -- */

PLATAPI int plat_getopt(int argc, char* const argv[], const char* options) {
    static const char* p_opt = NULL;

    if(opt_state == OPT_FAIL)
        return (EOF);

    if(p_opt == NULL || opt_state == OPT_OK) {
        if(optind >= argc) {
            return (EOF);
        }

        p_opt = argv[optind];

        /* -abcdef
         * ^       */
        if(*p_opt++ == OPT_CHAR) {
            optopt = (int) *p_opt++;

            if(optopt == 0) {
                opt_state = OPT_FAIL;
                return BAD_CHAR;
            }

            /* -a
			 *  ^ */
			if(*p_opt == 0) {
				opt_state = OPT_OK;
				++optind;
			}
        }
        else {
            /* Last option parsed */
            return (EOF);
        }
    }
    else if(opt_state == OPT_MULT) {
        optopt = (int) *p_opt++;

        /* -abcdef
         *       ^ */
        if(*p_opt == 0) {
            opt_state = OPT_OK;
            ++optind;
        }
    }

    if(optopt != OPT_CHAR) {
        char* oli = strchr(options, optopt);
        optarg = NULL;

        if(oli == NULL) {
            /* Unknown option */
            opt_state = OPT_FAIL;
            return BAD_CHAR;
        }

        if(*(++oli) == ARG_CHAR) {
            if(*p_opt) {
                /* -aARGUMENT
                 *   ^       */
                optarg = p_opt;
                p_opt = NULL;
                ++optind;
                return optopt;
            }
            else {
                /* -a ARGUMENT
                 *    ^       */
                if(optind >= argc) {
                    /* No argument provided */
                    return BAD_CHAR;
                }

                optarg = argv[optind++];
                p_opt = NULL;
                return optopt;
            }
        }
        else if(*p_opt) {
            opt_state = OPT_MULT;
            return optopt;
        }

        /*Single option*/
        return optopt;
    }

    return (EOF);
}

