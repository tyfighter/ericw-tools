/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/


#include <common/cmdlib.hh>
#include <common/bspfile.hh>

int
main(int argc, char **argv)
{
    bspdata_t bsp;
    char source[1024];
    int i;

    printf("---- bspinfo / ericw-tools " stringify(ERICWTOOLS_VERSION) " ----\n");
    if (argc == 1) {
        printf("usage: bspinfo bspfile [bspfiles]\n");
        exit(1);
    }

    for (i = 1; i < argc; i++) {
        printf("---------------------\n");
        strcpy(source, argv[i]);
        DefaultExtension(source, ".bsp");
        printf("%s\n", source);

        LoadBSPFile(source, &bsp);
        PrintBSPFileSizes(&bsp);

        printf("---------------------\n");
    }

    return 0;
}
