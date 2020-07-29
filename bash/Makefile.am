# nbd client library in userspace
# Copyright (C) 2013-2020 Red Hat Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

include $(top_srcdir)/subdir-rules.mk

EXTRA_DIST = \
	README \
	nbdsh \
	$(NULL)

if HAVE_BASH_COMPLETION

dist_bashcomp_DATA = nbdcopy nbdfuse nbdinfo nbdsh

nbdcopy: nbdsh
	rm -f $@
	$(LN_S) nbdsh $@

nbdfuse: nbdsh
	rm -f $@
	$(LN_S) nbdsh $@

nbdinfo: nbdsh
	rm -f $@
	$(LN_S) nbdsh $@

CLEANFILES += nbdcopy nbdfuse nbdinfo

endif