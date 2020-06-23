#//
#// $Id: LINUX_config 1832 2019-01-28 14:02:32Z bloehr $
#//
#// DESCRIPTION    Config file to make TRDP for native Linux
#//
#// AUTHOR         B. Loehr
#//
#// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0 
#// If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/
#// Copyright NewTec GmbH, 2015. All rights reserved.
#//

ARCH = linux
TARGET_VOS = posix
TARGET_OS = LINUX
TCPREFIX = 
TCPOSTFIX = 
DOXYPATH = /usr/local/bin/

# the _GNU_SOURCE is needed to get the extended poll feature for the POSIX socket

DEBUG = TRUE

CFLAGS += -Wall -fstrength-reduce -fno-builtin -fsigned-char -pthread -fPIC -D_GNU_SOURCE -DPOSIX -DCLOCK_PTP
#CFLAGS += -Wno-unknown-pragmas -Wno-format -Wno-unused-label -Wno-unused-function -Wno-int-to-void-pointer-cast -Wno-self-assign

LDFLAGS += -lrt

LINT_SYSINCLUDE_DIRECTIVES = -i ./src/vos/posix -wlib 0 -DL_ENDIAN
