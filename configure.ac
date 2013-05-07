# Copyright (C) 2013 Eric P. Hutchins

# This file is part of libcasheph.

# libcasheph is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# libcasheph is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with libcasheph.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([libcasheph],[0.1.0],[hutchiep190@potsdam.edu])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AM_PROG_AR

AC_PROG_CC
LT_INIT

AC_CHECK_LIB([z],[gzopen],[],[AC_MSG_ERROR([zlib library not found])])

AC_CHECK_LIB([pthread],[pthread_getspecific],[LIBS="$LIBS -lpthread"],
             [AC_MSG_ERROR([libmxml library not found])])

AC_CHECK_LIB([mxml],[mxmlLoadFile],[LIBS="$LIBS -lmxml"],
             [AC_MSG_ERROR([libmxml library not found])])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
])

AC_OUTPUT
