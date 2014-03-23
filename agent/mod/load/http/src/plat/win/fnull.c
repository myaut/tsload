/*
 * fnull.c
 *
 *  Created on: Mar 23, 2014
 *      Author: myaut
 */

#include <http.h>

PLATAPI FILE* plat_open_null(void) {
	return fopen("NUL", "w");
}
