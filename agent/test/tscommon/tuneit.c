
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#include <tuneit.h>

#include <string.h>
#include <assert.h>

int test_main() {
	short so1;
	short so2;
	long ix1;
	long z = -1;
	char c;

	boolean_t plus;
	boolean_t minus;
	boolean_t bopt;

	char longstr[10] = "default";
	char str[24];

	tuneit_add_option("so1=0o777");
	tuneit_add_option("so2=0555");
	tuneit_add_option("ix1=0xFeffAab1");
	tuneit_add_option("c=-127");
	tuneit_add_option("z=kadsjfkljfkl");

	tuneit_add_option("+plus");
	tuneit_add_option("-minus");
	tuneit_add_option("bopt=true");

	tuneit_add_option("longstr=aaaaaaaaaaaaaaaaaaa");
	tuneit_add_option("str=test");

	/* Integer tests */
	assert(tuneit_set_int(short, so1)  == TUNEIT_OK);
	assert(so1 == 0777);

	assert(tuneit_set_int(short, so2)  == TUNEIT_OK);
	assert(so2 == 0555);

	assert(tuneit_set_int(long, ix1)  == TUNEIT_OK);
	assert(ix1 == 0xFEFFAAB1);

	assert(tuneit_set_int(char, c)  == TUNEIT_OK);
	assert(c == -127);

	assert(tuneit_set_int(long, z)  == TUNEIT_INVALID);
	assert(z == -1);

	/* Boolean tests */
	assert(tuneit_set_bool(plus)  == TUNEIT_OK);
	assert(plus == B_TRUE);

	assert(tuneit_set_bool(minus)  == TUNEIT_OK);
	assert(minus == B_FALSE);

	assert(tuneit_set_bool(bopt)  == TUNEIT_OK);
	assert(bopt == B_TRUE);

	/* String tests */
	assert(tuneit_set_string(longstr, 10)  == TUNEIT_INVALID);
	assert(strcmp(longstr, "default") == 0);

	assert(tuneit_set_string(str, 24)  == TUNEIT_OK);
	assert(strcmp(str, "test") == 0);

	assert(tuneit_finalize() == TUNEIT_OK);

	return 0;
}


