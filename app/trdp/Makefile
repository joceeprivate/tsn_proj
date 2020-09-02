#//
#// $Id: Makefile 1832 2019-01-28 14:02:32Z bloehr $
#//
#// DESCRIPTION    trdp Makefile
#//
#// AUTHOR         Bernd Loehr, NewTec GmbH
#//
#// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
#// If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
#// Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2013-2018. All rights reserved.
#//

MAKEFLAGS += --quiet

#// Support for POSIX and VXWORKS, set buildsettings and config first!
 .EXPORT_ALL_VARIABLES:
# Check if configuration is present
ifeq (config/config.mk,$(wildcard config/config.mk)) 
# load target specific configuration
include config/config.mk
endif

include rules.mk

MD = mkdir -p
	
CFLAGS += -D$(TARGET_OS)

# Set paths
INCPATH += -I tcnopen/ -I src/

vpath %.c src/ tcnopen/
vpath %.h src/ tcnopen/

INCLUDES = $(INCPATH)

# Set Objects
VOS_OBJS = vos_utils.o \
	   vos_sock.o \
	   vos_mem.o \
	   vos_thread.o \
	   vos_shared_mem.o


TRDP_OBJS = trdp_pdcom.o \
	    trdp_utils.o \
	    trdp_if.o \
	    trdp_stats.o \
		thread_cmd.o \
		port_conf.o \
	    $(VOS_OBJS)

# Optional objects for full blown TRDP usage
TRDP_OPT_OBJS = trdp_xml.o \
		tau_xml.o \
		tau_marshall.o \
		tau_dnr.o \
		tau_tti.o \
		tau_ctrl.o


# Set LDFLAGS
LDFLAGS += -L $(OUTDIR)

# Enable / disable MD Support
# by default MD_SUPPORT is always enabled (in current state)
ifeq ($(MD_SUPPORT),0)
CFLAGS += -DMD_SUPPORT=0
else
TRDP_OBJS += trdp_mdcom.o
CFLAGS += -DMD_SUPPORT=1
endif

ifeq ($(DEBUG), TRUE)
	OUTDIR = build/$(ARCH)-dbg
else
	OUTDIR = build/$(ARCH)-rel
endif

# Enable / disable Debug
ifeq ($(DEBUG),TRUE)
CFLAGS += -g3 -O -DDEBUG
LDFLAGS += -g3
# Display the strip command and do not execute it
STRIP = @$(ECHO) "do NOT strip: "
else
CFLAGS += -Os  -DNO_DEBUG
STRIP = @$(ECHO) "do NOT strip: "
endif

TARGETS = outdir libtrdp app

all:	$(TARGETS)

outdir:
	@$(MD) $(OUTDIR)

libtrdp:	outdir $(OUTDIR)/libtrdp.a

#app:	$(OUTDIR)/sendHello	$(OUTDIR)/receiveHello
app:	$(OUTDIR)/transHello

%_config:
	cp -f config/$@ config/config.mk

$(OUTDIR)/%.o: %.c %.h trdp_if_light.h trdp_types.h vos_types.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OUTDIR)/libtrdp.a:		$(addprefix $(OUTDIR)/,$(notdir $(TRDP_OBJS)))
			@$(ECHO) ' ### Building the lib $(@F)'
			@$(RM) $@
			$(AR) cq $@ $^


###############################################################################
#
# rules for the demos
#
###############################################################################
				  
$(OUTDIR)/receiveHello: receiveHello.c  $(OUTDIR)/libtrdp.a
			@$(ECHO) ' ### Building application $(@F)'
			$(CC) src/receiveHello.c \
			    -ltrdp \
			    $(LDFLAGS) $(CFLAGS) $(INCLUDES) \
			    -o $@
			@$(STRIP) $@
						   
$(OUTDIR)/sendHello:   sendHello.c  $(OUTDIR)/libtrdp.a
			@$(ECHO) ' ### Building application $(@F)'
			$(CC) src/sendHello.c \
			    -ltrdp \
			    $(LDFLAGS) $(CFLAGS) $(INCLUDES) \
			    -o $@
			@$(STRIP) $@

$(OUTDIR)/transHello:   transHello.c  $(OUTDIR)/libtrdp.a
			@$(ECHO) ' ### Building application $(@F)'
			$(CC) src/transHello.c \
			    -ltrdp \
			    $(LDFLAGS) $(CFLAGS) $(INCLUDES) \
			    -o $@
			@$(STRIP) $@

###############################################################################
#
# wipe out everything section - except the previous target configuration
#
###############################################################################
clean:
	rm -f -r build/*

unconfig:
	-$(RM) config/config.mk
	
distclean:	clean unconfig	 		
