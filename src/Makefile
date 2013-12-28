# Makefile for System Garden's habitat project
# Nigel Stuckey, July 2002
#
# Copyright System Garden Ltd 1996-2012. All rights reserved.
#
# Please edit the configuration settings in Make.config, not in this
# Makefile. Make.config should contain everything needed to detect multiple
# platforms and set up the flags, options and variables needed to compile 
# for that platform.

# Definition section ----------------------------------------
# This is a recursive-less makefile and will include all the dependencies
# needs from module definitions
MODULES := pd gnu iiab probe trm cmd myhabitat man #help util ...

# Standard arguments
CFLAGS += -Wall -g -DNMALLOC $(EXTRA_INCLUDE) $(SYS_INCLUDE)
# -Wall -Wcast-qual -Wno-format-y2k # -O3 # -pg -a
LINKPATHARG = -Wl,-rpath,../%
LDFLAGS += 			# see Make.config

# Each module may add to these
SRC =
BIN =
INC =
ETC =
INITBIN =
DOC = ../README ../LICENSE
LDLIBS = 
LOADLIBES = 
HELPFILES =
VPATH = $(MODULES)
TESTSRC =

# File and tool definitions
TAGFILE = TAGS
VERFILE = VERSION
MKTAGS  = /usr/bin/etags
SYMLN   = ln -f -s
MKDIR   = mkdir -p
COPY    = cp
CHMOD   = chmod
GZIP    = gzip -cf
BZIP    = bzip2
MAN2HTML= man2html
HDIUTIL = hdiutil		# mac dmg creation tool

# Definitions
PRODUCT := "System Garden Habitat"
APP     := habitat
VERSION := $(shell cat $(VERFILE))
CFLAGS  += -DVERSION="\"$(VERSION)\""

# -- Install libraries, binaries and other products --
# The standard locations stand-alone build
DSTBIN  := ../bin
DSTLIB  := ../lib
DSTINC  := ../include
DSTETC  := ../etc
DSTINIT := $(DSTETC)/init.d
DSTVAR  := ../var
DSTMAN  := ../man
DSTMAN1 := $(DSTMAN)/man1
DSTMAN5 := $(DSTMAN)/man5
DSTMAN8 := $(DSTMAN)/man8
DSTWEB  := ../web
DSTDIST := ../dist
DSTHTML     := ../html
DSTHTMLMAN1 := ../html/man1
DSTHTMLMAN5 := ../html/man5
DSTHTMLMAN8 := ../html/man8
DSTHELP     := $(DSTLIB)/help
DSTPIXMAPS  := $(DSTLIB)/pixmaps
DSTALL  := $(DSTBIN) $(DSTLIB) $(DSTINC) $(DSTETC) $(DSTINIT) $(DSTVAR) \
		$(DSTMAN1) $(DSTMAN5) $(DSTMAN8) $(DSTHTMLMAN1) $(DSTHTMLMAN5) \
		$(DSTHTMLMAN8) $(DSTWEB) $(DSTDIST) $(DSTHELP) $(DSTPIXMAPS)

# Linux install locations for system friendly build
LINROOT :=
LINDSTBIN    := $(LINROOT)/usr/bin
LINDSTLIB    := $(LINROOT)/usr/lib/$(APP)
LINDSTSOLIB  := $(LINROOT)/usr/lib/$(APP)
# dont install # LINDSTINC  := ../include
LINDSTETC    := $(LINROOT)/etc
LINDSTINIT   := $(LINDSTETC)/init.d
LINDSTVAR    := $(LINROOT)/var/lib/$(APP)
LINDSTMAN    := $(LINROOT)/usr/share/man
LINDSTMAN1   := $(LINDSTMAN)/man1
LINDSTMAN5   := $(LINDSTMAN)/man5
LINDSTMAN8   := $(LINDSTMAN)/man8
LINDSTDOC    := $(LINROOT)/usr/share/doc/$(APP)-$(VERSION)
LINDSTHTML     := $(LINDSTLIB)/html
LINDSTHTMLMAN1 := $(LINDSTHTML)/man1
LINDSTHTMLMAN5 := $(LINDSTHTML)/man5
LINDSTHTMLMAN8 := $(LINDSTHTML)/man8
# dont install # LINDSTWEB  := ../web
LINDSTHELP     := $(LINDSTLIB)/help
LINDSTPIXMAPS  := $(LINDSTLIB)/pixmaps
LINDSTALL    := $(LINDSTBIN) $(LINDSTLIB) $(LINDSTETC) $(LINDSTINIT) \
		$(LINDSTVAR) $(LINDSTMAN1) $(LINDSTMAN5) $(LINDSTMAN8) \
		$(LINDSTHTMLMAN1) $(LINDSTHTMLMAN5) $(LINDSTHTMLMAN8) \
		$(LINDSTDIST) $(LINDSTHELP) $(LINDSTDOC) $(LINDSTPIXMAPS)
LININVENTORY := INVENTORY

# Include site and build specific configuration details
include Make.config

# continuation of definitions following configuration
VPATH += $(EXTRA_LIBDIR) $(SYS_LIBDIR)
LDLIBS += $(EXTRA_LIB) $(SYS_LIB)

# Target & dependency section ----------------------------------------
# Special targets used as commands: they do not create files, so we
# use the GNU make .PHONY directive
.PHONY: clean cleanall all really-all test test-mkdir test-all tag tags \
		install linuxinstall linuxinventory srctar gnubintar \
		gnubintar bintar rpm ctest cleaninstall cleaninstonly testrun \
		windows

# Major target
all: really-all

# Test tagret
test: test-mkdir test-all

# Include the description of each module
include $(patsubst %, %/Make.sub, $(MODULES))

# Determine the object files
OBJ :=	$(patsubst %.c, %.o, $(filter %.c, $(SRC)))

# Determine the the dependency files
DEPFILES := $(OBJ:.o=.P)

 # Include the c include dependencies
-include $(DEPFILES)

# Continuation of major target
really-all: $(TAGFILE) $(SOLIB) $(BIN) $(INITBIN) htmlman #help-build

# Calculate the c include dependencies (-MD flag) into .d file, then process
# into .P file, making every dependency a 'no-action' target so make doesn't
# barf. Then, compile straight away.
%.o: %.c
	$(COMPILE.c) -MD -o $@ $<
	$(COPY) $*.d $*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	    -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P ; \
	rm -f $*.d
	$(COMPILE.c) -o $@ $<

# Rule to install shell scripts without the '.sh' extension and make 
# them executable
%: %.sh
	$(COPY) $< $@; \
	$(CHMOD) a+x $@

# link .o and .c files 'correctly'. The libraries will be filtered so
# that only the library name is stored in the binary, without directory.
# This enables dynamic linking to continue with the normal behaviour.
dep_nolib   = $(filter-out %.a,$(filter-out %.so,$(filter-out %.dylib,$(filter-out %.lib,$(filter-out %.dll,$(1))))))
dep_onlylib = $(filter %.so,$(1)) $(filter %.a,$(1)) $(filter %.dylib,$(1)) $(filter %.lib,$(1)) $(filter %.dll,$(1))
dep_libdir  = $(patsubst %/,-L%,$(sort $(dir $(call dep_onlylib,$(1)))))
dep_libname = $(patsubst lib%,-l%,$(sort $(basename $(notdir $(call dep_onlylib,$(1))))))

%: %.o
	$(LINK.o) -o $@ $(call dep_nolib,$^) $(LOADLIBES) $(LDLIBS) \
		$(call dep_libdir,$^) $(call dep_libname,$^)

%: %.c
	$(LINK.c) -o $@ $(call dep_nolib,$^) $(LOADLIBES) $(LDLIBS) \
		$(call dep_libdir,$^) $(call dep_libname,$^)

# Manual page rules
HTMLMAN1 :=	$(patsubst %.1, %.1.html, $(MAN1))
HTMLMAN5 :=	$(patsubst %.5, %.5.html, $(MAN5))
HTMLMAN8 :=	$(patsubst %.8, %.8.html, $(MAN8))

htmlman: $(HTMLMAN1) $(HTMLMAN5) $(HTMLMAN8)

# Rules to compile manpages into html
%.1.html: %.1 $(MANHTMLCSS)
	$(MAN2HTML) -p -r $< | tail -n +2 - | cat $(MANHTMLCSS) - >$<.html

%.5.html: %.5 $(MANHTMLCSS)
	$(MAN2HTML) -p -r $< | tail -n +2 - | cat $(MANHTMLCSS) - >$<.html

%.8.html: %.8 $(MANHTMLCSS)
	$(MAN2HTML) -p -r $< | tail -n +2 - | cat $(MANHTMLCSS) - >$<.html

# Make tags and sym links to the modules
tags: $(TAGFILE)
tag:  $(TAGFILE)
$(TAGFILE): $(SRC) $(OBJ)
	if [ -f $(MKTAGS) ]; then $(MKTAGS) $(SRC); fi

# Standard install for stand-alone build
install: all
	$(MKDIR) $(DSTALL); \
	$(COPY)  $(BIN)          $(DSTBIN); \
	$(COPY)  $(LIB) $(SOLIB) $(DSTLIB); \
	$(COPY)  $(INC)          $(DSTINC); \
	$(COPY)  $(ETC)          $(DSTETC); \
	$(COPY)  $(INITBIN)      $(DSTINIT); \
	$(COPY)  $(MAN1)         $(DSTMAN1); \
	$(COPY)  $(MAN5)         $(DSTMAN5); \
	$(COPY)  $(MAN8)         $(DSTMAN8); \
	$(COPY)  $(HTMLMAN1)     $(DSTHTMLMAN1); \
	$(COPY)  $(HTMLMAN5)     $(DSTHTMLMAN5); \
	$(COPY)  $(HTMLMAN8)     $(DSTHTMLMAN8); \
	$(COPY)  $(PIXMAPS)      $(DSTPIXMAPS);
#	$(COPY)  $(HELPFILES)    $(DSTHELP);

# Linux install into its various system areas, rather than in a single tree
linuxinstall: all
	$(MKDIR) $(LINDSTALL); \
	$(COPY)  $(BIN)       $(LINDSTBIN); \
	$(COPY)  $(LIB)       $(LINDSTLIB); \
	$(COPY)  $(SOLIB)     $(LINDSTSOLIB); \
	$(COPY)  $(ETC)       $(LINDSTETC); \
	$(COPY)  $(INITBIN)   $(LINDSTINIT); \
	for f in $(MAN1); \
	do                \
		$(GZIP) <$$f >$(LINDSTMAN1)/`basename $$f`.gz; \
	done; \
	for f in $(MAN5); \
	do                \
		$(GZIP) <$$f >$(LINDSTMAN5)/`basename $$f`.gz; \
	done; \
	for f in $(MAN8); \
	do                \
		$(GZIP) <$$f >$(LINDSTMAN8)/`basename $$f`.gz; \
	done; \
	$(COPY)  $(HTMLMAN1)  $(LINDSTHTMLMAN1); \
	$(COPY)  $(HTMLMAN5)  $(LINDSTHTMLMAN5); \
	$(COPY)  $(HTMLMAN8)  $(LINDSTHTMLMAN8); \
	$(COPY)  $(PIXMAPS)   $(LINDSTPIXMAPS); \
	$(COPY)  $(DOC) $(LININVENTORY) $(LINDSTDOC);
#	$(COPY)  $(HELPFILES) $(LINDSTHELP);

# produce a INVENTORY file reflecting the locations of a linux install
linuxinventory:
	echo $(addprefix $(LINDSTBIN)/,$(notdir $(BIN))) \
	  $(addprefix $(LINDSTLIB)/,$(notdir $(LIB))) \
	  $(addprefix $(LINDSTSOLIB)/,$(notdir $(SOLIB))) \
	  $(addprefix $(LINDSTHELP)/,$(notdir $(HELPFILES))) \
	  $(addprefix $(LINDSTETC)/,$(notdir $(ETC))) \
	  $(addprefix $(LINDSTINIT)/,$(notdir $(INITBIN))) \
	  $(addsuffix .*,$(addprefix $(LINDSTMAN1)/,$(notdir $(MAN1)))) \
	  $(addsuffix .*,$(addprefix $(LINDSTMAN5)/,$(notdir $(MAN5)))) \
	  $(addsuffix .*,$(addprefix $(LINDSTMAN8)/,$(notdir $(MAN8)))) \
	  $(addprefix $(LINDSTHTMLMAN1)/,$(notdir $(HTMLMAN1))) \
	  $(addprefix $(LINDSTHTMLMAN5)/,$(notdir $(HTMLMAN5))) \
	  $(addprefix $(LINDSTHTMLMAN8)/,$(notdir $(HTMLMAN8))) \
	  $(addprefix $(LINDSTPIXMAPS)/,$(notdir $(PIXMAPS))) \
	  $(addprefix $(LINDSTDOC)/,$(notdir $(DOC) $(LININVENTORY))) \
	  | tr ' ' '\n' >$(LININVENTORY);

# Distribution packages: files and directories
SRCTOP    := $(APP)-$(VERSION)-src
GNUSRCTOP := $(APP)-$(VERSION)-gnusrc
GNUBINTOP := $(APP)-$(VERSION)-$(shell uname -s)-$(shell uname -m)-gnubin
BINTOP    := $(APP)-$(VERSION)-$(shell uname -s)-$(shell uname -m)
BINTARF   := $(subst ..,$(BINTOP),../README ../LICENSE $(DSTBIN) $(DSTLIB) \
		$(DSTINC) $(DSTETC) $(DSTMAN) $(DSTHTML) $(DSTHELP) $(DSTVAR))
RPMDIR    := $(APP)-$(VERSION)-rpm
RPMSPEC   := util/lib/$(APP).spec
DMGDIR    := $(APP)-$(VERSION)-dmg
DMGFILE   := $(APP)-$(VERSION).dmg

srctar:	cleanall
	$(MKDIR) $(DSTDIST); cd $(DSTDIST); \
	if [ -L $(SRCTOP) ]; then rm $(SRCTOP); fi; \
	ln -s .. $(SRCTOP); \
	tar cvhzf $(SRCTOP).tar.gz \
		--exclude=CVS --exclude=.svn --exclude='gnu/[a-z]*' \
		--exclude=test --exclude='*.bak' --exclude='*~' \
		--exclude='.*' \
		$(SRCTOP)/README $(SRCTOP)/LICENSE $(SRCTOP)/src; \
	rm $(SRCTOP);

gnubintar:	
	$(MKDIR) $(DSTDIST); cd $(DSTDIST); \
	if [ -L $(GNUBINTOP) ]; then rm $(GNUBINTOP); fi; \
	ln -s .. $(GNUBINTOP); \
	tar cvhzf $(GNUBINTOP).tar.gz --exclude=CVS --exclude=.svn \
		$(GNUBINTOP)/src/gnu/bin  $(GNUBINTOP)/src/gnu/include \
		$(GNUBINTOP)/src/gnu/info $(GNUBINTOP)/src/gnu/lib \
		$(GNUBINTOP)/src/gnu/man; \
	rm $(GNUBINTOP);

gnusrctar:	
	$(MKDIR) $(DSTDIST); cd $(DSTDIST); \
	if [ -L $(GNUSRCTOP) ]; then rm $(GNUSRCTOP); fi; \
	ln -s .. $(GNUSRCTOP); \
	tar cvhzf $(GNUSRCTOP).tar.gz --exclude=CVS --exclude=.svn \
		$(GNUSRCTOP)/src/gnu/gdbm  $(GNUSRCTOP)/src/gnu/readline \
		$(GNUSRCTOP)/src/gnu/curl; \
	rm $(GNUSRCTOP);

bintar:	install
	$(MKDIR) $(DSTDIST); cd $(DSTDIST); \
	if [ -L $(BINTOP) ]; then rm $(BINTOP); fi; \
	ln -s .. $(BINTOP); \
	tar cvhzf $(BINTOP).tar.gz --exclude='$(BINTOP)/var/*' \
		--exclude='*.bak' --exclude='*~' --exclude='.*' $(BINTARF); \
	rm $(BINTOP);

rpm:	srctar
	$(MKDIR) $(DSTDIST)/$(RPMDIR); \
	util/bin/mkrpm $(DSTDIST)/$(RPMDIR) $(DSTDIST)/$(SRCTOP).tar.gz \
		$(APP) $(VERSION); \
	mv $(DSTDIST)/$(RPMDIR)/* $(DSTDIST); \
	rmdir $(DSTDIST)/$(RPMDIR);

# Create Mac .dmg file, a disk image container
dmg:	installmac
	$(HDIUTIL) create $(DMGFILE) -volname "$(PRODUCT)" -fs HFS+ -srcfolder $(DMGDIR) -ov

# Create windows binaries using cross compile config
windows: all

# Clean the source and build areas
# There are four levels: 
# (1) clean: to remove intermedeate and test files
# (2) cleanall: to remove (1) and libraries and binaries after an install
# (3) cleaninstall: remove (1), (2) & all products from the install locations
# (4) cleaninstonly: remove all products from the install locations only
clean_files =	$(wildcard $d/*[~%]) $(wildcard $d/*.bak) \
		$(wildcard $d/*.out) $(wildcard $d/*.dat) \
		$(wildcard $d/t.*) $d/core \
		$(wildcard $d/.[A-Za-rt-z0-9]*) \
		$(wildcard $d/.[^s.][^v][^n]*) 
cleanall_files= $(wildcard $d/*.so) $(wildcard $d/*.a) $(wildcard $d/*.dylib) \
		$(wildcard $d/*.lib) $(wildcard $d/*.dll)
cleaninst_files=$(addprefix $(DSTBIN)/,$(notdir $(BIN))) \
		$(addprefix $(DSTLIB)/,$(notdir $(LIB))) \
		$(addprefix $(DSTSOLIB)/,$(notdir $(SOLIB))) \
		$(addprefix $(DSTINC)/,$(notdir $(INC))) \
		$(addprefix $(DSTETC)/,$(notdir $(ETC))) \
		$(addprefix $(DSTINIT)/,$(notdir $(INITBIN))) \
		$(addprefix $(DSTHELP)/,$(notdir $(HELPFILES))) \
		$(addprefix $(DSTHTMLMAN1)/,$(notdir $(HTMLMAN1))) \
		$(addprefix $(DSTHTMLMAN5)/,$(notdir $(HTMLMAN5))) \
		$(addprefix $(DSTHTMLMAN8)/,$(notdir $(HTMLMAN8))) \
		$(addprefix $(DSTMAN1)/,$(notdir $(MAN1))) \
		$(addprefix $(DSTMAN5)/,$(notdir $(MAN5))) \
		$(addprefix $(DSTMAN8)/,$(notdir $(MAN8))) \
		$(addprefix $(DSTPIXMAPS)/,$(notdir $(PIXMAPS)))
ctest:
	echo $(foreach d,$(MODULES) .,$(wildcard $d/.[A-Za-rt-z0-9]*) $d/core)

clean:
	$(RM) $(OBJ) $(DEPFILES) $(foreach d,$(MODULES) .,$(clean_files)) \
		$(HTMLMAN1) $(HTMLMAN5) $(HTMLMAN8) $(TAGFILE)

cleanall:	clean
	$(RM) $(BIN) $(INITBIN) $(HELPFILES) \
		$(foreach d,$(MODULES) .,$(cleanall_files))

cleaninstall:	cleanall
	$(RM) $(cleaninst_files)

cleaninstonly:
	$(RM) $(cleaninst_files)

# Unit tests
# The test binaries are all place in the test subdirectory, dynamically
# created and prefixed with 't.'. When anything in $(TESTBIN) is called,
# the VPATH is checked for the corresponding '.c' files.
test-mkdir:
	if [ ! -d test ]; then mkdir test; fi

TESTBIN := $(addprefix test/t., $(notdir $(basename $(TESTSRC))))
test-all: $(TESTBIN)

testrun: test-all
	@cd test; \
	for f in $(notdir $(TESTBIN)); \
	do \
		echo "---------- testing $$f ----------"; \
		if ! $$f; then exit 1; fi; \
	done

test/t.%: %.c
	$(LINK.c) -DTEST -o $@ $<\
		$(LOADLIBES) $(LDLIBS) \
		$(patsubst %/,-L%,$(dir $(filter %.so,$^) $(filter %.a,$^))) \
		$(patsubst lib%,-l%,$(basename $(notdir $(filter %.so,$^) $(filter %.a,$^)))) -Liiab -liiab -Lgnu/lib -lgdbm -lcurl -ldl

