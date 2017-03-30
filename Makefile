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

kit:
	cd src/dps8 && $(MAKE) dps8.sha1.txt
	cp src/dps8/dps8.sha1.txt src/dps8/dps8.sha1.txt~
	tar cfz source.tgz  `git ls-files | grep -v .gitignore | grep -v .metadata` src/dps8/dps8.sha1.txt~
