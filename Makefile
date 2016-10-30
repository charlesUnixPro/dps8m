# Copyright 2016 by Charles Anthony
#
#  All rights reserved.
#
# This software is made available under the terms of the
# ICU License -- ICU 1.8.1 and later.
# See the LICENSE file at the top-level directory of this distribution and
# at http://example.org/project/LICENSE.

all:
	cd src/dps8 && $(MAKE)

install:
	cd src/dps8 && $(MAKE) install

clean:
	cd src/dps8 && $(MAKE) clean

