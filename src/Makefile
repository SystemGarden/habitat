# Makefile for System Garden's habitat project
# Nigel Stuckey, July 2002
#
# Copyright System Garden Ltd 1996-2002. All rights reserved.
#
# Please edit the configurations setting in Make.config, not in this
# Makefile

# Definition section ----------------------------------------
# This is a recursive-less makefile and will include all the dependencies
# needs from module definitions
MODULES := pd gnu iiab probe trm cmd ghabitat help man #util ...

# Standard arguments
CFLAGS += -g -DNMALLOC $(EXTRA_INCLUDE) $(SYS_INCLUDE)
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

# Definitions
APP     := habitat
VERSION := $(shell cat $(VERFILE))
CFLAGS  += -DVERSION=\"$(VERSION)\"

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
DSTWEB  := ../web
DSTDIST := ../dist
DSTHTML     := ../html
DSTHTMLMAN1 := ../html/man1
DSTHTMLMAN5 := ../html/man5
DSTHELP := $(DSTLIB)/help
DSTALL  := $(DSTBIN) $(DSTLIB) $(DSTINC) $(DSTETC) $(DSTINIT) $(DSTVAR) \
		$(DSTMAN1) $(DSTMAN5) $(DSTHTMLMAN1) $(DSTHTMLMAN5) \
		$(DSTWEB) $(DSTDIST) $(DSTHELP)

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
LINDSTDOC    := $(LINROOT)/usr/share/doc/$(APP)-$(VERSION)
LINDSTHTML     := $(LINDSTLIB)/html
LINDSTHTMLMAN1 := $(LINDSTHTML)/man1
LINDSTHTMLMAN5 := $(LINDSTHTML)/man5
# dont install # LINDSTWEB  := ../web
LINDSTHELP   := $(LINDSTLIB)/help
LINDSTALL    := $(LINDSTBIN) $(LINDSTLIB) $(LINDSTETC) $(LINDSTINIT) \
		$(LINDSTVAR) $(LINDSTMAN1) $(LINDSTMAN5) \
		$(LINDSTHTMLMAN1) $(LINDSTHTMLMAN5) $(LINDSTDIST) \
		$(LINDSTHELP) $(LINDSTDOC)
LININVENTORY := INVENTORY

# Include site and build specific configuration details
include Make.config

# continuation of definitions following configuration
VPATH += $(EXTRA_LIBDIR) $(SYS_LIBDIR)
LDLIBS += $(EXTRA_LIB) $(SYS_LIB)

# Target & dependency section ----------------------------------------
# Special targets used as commands: they do not create files, so we
# use the GNU make .PHONY directive
.PHONY: clean cleanall all really-all tag tags install

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
really-all: $(TAGFILE) $(BIN) $(INITBIN) htmlman help-build

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
dep_nolib   = $(filter-out %.a,$(filter-out %.so,$(1)))
dep_onlylib = $(filter %.so,$(1)) $(filter %.a,$(1))
dep_libdir  = $(patsubst %/,-L%,$(dir $(call dep_onlylib,$(1))))
dep_libname = $(patsubst lib%,-l%,$(basename $(notdir $(call dep_onlylib,$(1)))))

%: %.o
	$(LINK.o) -o $@ $(call dep_nolib,$^) $(LOADLIBES) $(LDLIBS) \
		$(call dep_libdir,$^) $(call dep_libname,$^)

%: %.c
	$(LINK.c) -o $@ $(call dep_nolib,$^) $(LOADLIBES) $(LDLIBS) \
		$(call dep_libdir,$^) $(call dep_libname,$^)

# Manual page rules
HTMLMAN1 :=	$(patsubst %.1, %.1.html, $(MAN1))
HTMLMAN5 :=	$(patsubst %.5, %.5.html, $(MAN5))

htmlman: $(HTMLMAN1) $(HTMLMAN5)

# Rules to compile manpages into html
%.1.html: %.1
	$(MAN2HTML) -p -r $< >$<.html

%.5.html: %.5
	$(MAN2HTML) -p -r $< >$<.html

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
	$(COPY)  $(HELPFILES)    $(DSTHELP); \
	$(COPY)  $(ETC)          $(DSTETC); \
	$(COPY)  $(INITBIN)      $(DSTINIT); \
	$(COPY)  $(MAN1)         $(DSTMAN1); \
	$(COPY)  $(MAN5)         $(DSTMAN5); \
	$(COPY)  $(HTMLMAN1)     $(DSTHTMLMAN1); \
	$(COPY)  $(HTMLMAN5)     $(DSTHTMLMAN5);

# Linux install for its system friendly build
linuxinstall: all
	$(MKDIR) $(LINDSTALL); \
	$(COPY)  $(BIN)       $(LINDSTBIN); \
	$(COPY)  $(LIB)       $(LINDSTLIB); \
	$(COPY)  $(SOLIB)     $(LINDSTSOLIB); \
	$(COPY)  $(HELPFILES) $(LINDSTHELP); \
	$(COPY)  $(ETC)       $(LINDSTETC);
	$(COPY)  $(INITBIN)   $(LINDSTINIT); \
	for f in $(MAN1); \
	do                \
		$(GZIP) <$$f >$(LINDSTMAN1)/`basename $$f`.gz; \
	done; \
	for f in $(MAN5); \
	do                \
		$(GZIP) <$$f >$(LINDSTMAN5)/`basename $$f`.gz; \
	done; \
	$(COPY)  $(HTMLMAN1)  $(LINDSTHTMLMAN1); \
	$(COPY)  $(HTMLMAN5)  $(LINDSTHTMLMAN5);
	$(COPY)  $(DOC) $(LININVENTORY) $(LINDSTDOC);

linuxinventory:
	echo $(addprefix $(LINDSTBIN)/,$(notdir $(BIN))) \
	  $(addprefix $(LINDSTLIB)/,$(notdir $(LIB))) \
	  $(addprefix $(LINDSTSOLIB)/,$(notdir $(SOLIB))) \
	  $(addprefix $(LINDSTHELP)/,$(notdir $(HELPFILES))) \
	  $(addprefix $(LINDSTETC)/,$(notdir $(ETC))) \
	  $(addprefix $(LINDSTINIT)/,$(notdir $(INITBIN))) \
	  $(addsuffix .*,$(addprefix $(LINDSTMAN1)/,$(notdir $(MAN1)))) \
	  $(addsuffix .*,$(addprefix $(LINDSTMAN5)/,$(notdir $(MAN5)))) \
	  $(addprefix $(LINDSTHTMLMAN1)/,$(notdir $(HTMLMAN1))) \
	  $(addprefix $(LINDSTHTMLMAN5)/,$(notdir $(HTMLMAN5))) \
	  $(addprefix $(LINDSTDOC)/,$(notdir $(DOC) $(LININVENTORY))) \
	  | tr ' ' '\n' >$(LININVENTORY);

# Source distributions
SRCTOP    := $(APP)-$(VERSION)-src
GNUSRCTOP := $(APP)-$(VERSION)-gnusrc
GNUBINTOP := $(APP)-$(VERSION)-$(shell uname -s)-$(shell uname -m)-gnubin
BINTOP    := $(APP)-$(VERSION)-$(shell uname -s)-$(shell uname -m)
BINTARF   := $(subst ..,$(BINTOP),../README ../LICENSE $(DSTBIN) $(DSTLIB) \
		$(DSTINC) $(DSTETC) $(DSTMAN) $(DSTHTML) $(DSTHELP) $(DSTVAR))
RPMDIR    := $(APP)-$(VERSION)-rpm
RPMSPEC   := util/lib/$(APP).spec

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
cleanall_files= $(wildcard $d/*.so) $(wildcard $d/*.a) \
		$(wildcard $d/*.lib) $(wildcard $d/*.dll)
cleaninst_files=$(DSTBIN)/wrapper.bin \
		$(addprefix $(DSTBIN)/,$(notdir $(BIN))) \
		$(addsuffix .bin,$(addprefix $(DSTBIN)/,$(notdir $(BIN)))) \
		$(addprefix $(DSTLIB)/,$(notdir $(LIB))) \
		$(addprefix $(DSTSOLIB)/,$(notdir $(SOLIB))) \
		$(addprefix $(DSTINC)/,$(notdir $(INC))) \
		$(addprefix $(DSTETC)/,$(notdir $(ETC))) \
		$(addprefix $(DSTINIT)/,$(notdir $(INITBIN))) \
		$(addprefix $(DSTHELP)/,$(notdir $(HELPFILES)))
ctest:
	echo $(foreach d,$(MODULES) .,$(wildcard $d/.[A-Za-rt-z0-9]*) $d/core)

clean:
	$(RM) $(OBJ) $(DEPFILES) $(foreach d,$(MODULES) .,$(clean_files)) \
		$(HTMLMAN1) $(HTMLMAN5) $(TAGFILE)

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

