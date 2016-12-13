# DO-NOT-REMOVE begin-copyright-block 
#
# Redistributions of any form whatsoever must retain and/or include the
# following acknowledgment, notices and disclaimer:
#
# This product includes software developed by Carnegie Mellon University.
#
# Copyright 2012 by Mohammad Alisafaee, Eric Chung, Michael Ferdman, Brian 
# Gold, Jangwoo Kim, Pejman Lotfi-Kamran, Onur Kocberber, Djordje Jevdjic, 
# Jared Smolens, Stephen Somogyi, Evangelos Vlachos, Stavros Volos, Jason 
# Zebchuk, Babak Falsafi, Nikos Hardavellas and Tom Wenisch for the SimFlex 
# Project, Computer Architecture Lab at Carnegie Mellon, Carnegie Mellon University.
#
# For more information, see the SimFlex project website at:
#   http://www.ece.cmu.edu/~simflex
#
# You may not use the name "Carnegie Mellon University" or derivations
# thereof to endorse or promote products derived from this software.
#
# If you modify the software you must place a notice on or within any
# modified version provided or made available to any third party stating
# that you have modified the software.  The notice shall include at least
# your name, address, phone number, email address and the date and purpose
# of the modification.
#
# THE SOFTWARE IS PROVIDED "AS-IS" WITHOUT ANY WARRANTY OF ANY KIND, EITHER
# EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO ANY WARRANTY
# THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS OR BE ERROR-FREE AND ANY
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
# TITLE, OR NON-INFRINGEMENT.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
# BE LIABLE FOR ANY DAMAGES, INCLUDING BUT NOT LIMITED TO DIRECT, INDIRECT,
# SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN
# ANY WAY CONNECTED WITH THIS SOFTWARE (WHETHER OR NOT BASED UPON WARRANTY,
# CONTRACT, TORT OR OTHERWISE).
#
# DO-NOT-REMOVE end-copyright-block   
# Get variables from Makefile.defs
include $(FLEXUS_ROOT)/makefile.defs

QEMU_SOURCES = $(subst $(CORE_DIR)/,,$(shell ls $(CORE_DIR)/qemu/*.cpp $(CORE_DIR)/qemu/aux_/*.cpp))
TEST_SOURCES = $(subst $(CORE_DIR)/,,$(shell ls $(CORE_DIR)/test/*.cpp))
DOC_SOURCES = core_documentation.cpp
PREFILTER_CORE_LIB_SOURCES = $(subst $(CORE_DIR)/,,$(shell ls $(CORE_DIR)/*/*.cpp)) $(subst $(CORE_DIR)/,,$(wildcard $(CORE_DIR)/*.cpp))
CORE_LIB_SOURCES = $(filter-out $(QEMU_SOURCES) $(TEST_SOURCES) $(DOC_SOURCES),$(PREFILTER_CORE_LIB_SOURCES))

include $(FLEXUS_ROOT)/makefile.variant

.SUFFIXES:
.SUFFIXES: .cpp .$(OEXT) .$(GCC_DEP_EXT) .$(ICC_DEP_EXT)

.PHONY: default clean clobber qemu test doc core

default: qemu

qemu: depend core
	$(MAKE) -f makefile.$(SELECTED_CC) libqemu_$(VARIANT).a $(BUILD_OPTIONS) SOURCES="$(QEMU_SOURCES)"

test: depend core
	$(MAKE) -f makefile.$(SELECTED_CC) test $(BUILD_OPTIONS) SOURCES="$(TEST_SOURCES)"

core:
	$(MAKE) -f makefile.$(SELECTED_CC) libcore_$(VARIANT).a $(BUILD_OPTIONS) SOURCES="$(CORE_LIB_SOURCES)"

clean:
	echo "Cleaning core"
	find . -name "*.*_o" | xargs rm -f
	find . -name "*.*_dep" | xargs rm -f
	find . -name "Makefile.depend.*cc" | xargs rm -f
	find . -name "core.*.ipo_out.o" | xargs rm -f
	find . -name "qemu.*.ipo_out.o" | xargs rm -f
	find . -name "*.il" | xargs rm -f
	find . -name "core-test.*.ipo_out.o" | xargs rm -f
	find . -name "core-test.*.?cc" | xargs rm -f
	find . -name "*_v9_*_gcc.a" | xargs rm -f

clobber: clean
	echo "Clobbering core"
	find . -name "*cc.a" | xargs rm -f
	find . -name "libboost_*.a" | xargs rm -f
	find . -name "*gcc.a.fresh" | xargs rm -f
	rm -rf ../doc/core

doc:
	echo "Generating core documentation"
	$(DOXYGEN) doxygen.config

# Set up a depend target
###############

depend: Makefile.depend.$(VARIANT)

.cpp.$(GCC_DEP_EXT):
	$(GCC) $(INCLUDES) $(VARIANT_DEFINES) $(GCC_DEPFLAGS) -MF $@ -MT $*.$(GCC_OEXT) $<

Makefile.depend.$(GCC_VARIANT): $(patsubst %.cpp,%.$(GCC_DEP_EXT),$(CORE_LIB_SOURCES) $(TEST_SOURCES) $(QEMU_SOURCES))
	cat $^ > Makefile.depend.$(GCC_VARIANT)


$(patsubst %.cpp,%.$(ICC_DEP_EXT),$(CORE_LIB_SOURCES)): %.$(ICC_DEP_EXT): %.cpp
	$(GCC) $(INCLUDES) $(VARIANT_DEFINES) $(GCC_DEPFLAGS) -MF $@ -MT core.$(VARIANT_EXT).ipo_out.o $<

$(patsubst %.cpp,%.$(ICC_DEP_EXT),$(QEMU_SOURCES)): %.$(ICC_DEP_EXT): %.cpp
	$(GCC) $(INCLUDES) $(VARIANT_DEFINES) $(GCC_DEPFLAGS) -MF $@ -MT qemu.$(VARIANT_EXT).ipo_out.o $<

Makefile.depend.$(ICC_VARIANT): $(patsubst %.cpp,%.$(ICC_DEP_EXT),$(CORE_LIB_SOURCES) $(QEMU_SOURCES))
	cat $^ > Makefile.depend.$(ICC_VARIANT)
