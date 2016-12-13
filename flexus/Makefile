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
include makefile.defs

.PHONY: core stat-manager

stat-manager:
	@cd stat-manager ; $(MAKE) $(SILENT_MAKE) -f Makefile all

ifndef FLEXUS_ROOT

#determine what FLEXUS_ROOT should be
.DEFAULT core:
	$(MAKE) $(SILENT_MAKE) FLEXUS_ROOT=`pwd` $@

list_targets:
	$(MAKE) $(SILENT_MAKE) FLEXUS_ROOT=`pwd` $@

else
# We know what Flexus root is.

ifndef SETUP_OK
# Check the Flexus setup to make sure all paths are correct and Boost
# are installed correctly.

.DEFAULT core:
	$(MAKE) $(SILENT_MAKE) -f makefile.checksetup
	$(MAKE) $(SILENT_MAKE) SETUP_OK=true $@

else
# We have checked the Flexus setup and it is ok to proceed with the build

ifndef TARGET_PARSED
# We need to parse the target name
# We break the target into a TARGET and a TARGET_SPEC, breaking up on the first dash.  All dashes are stripped out of both

.DEFAULT core:
	$(MAKE) $(SILENT_MAKE) TARGET_PARSED=true $(word 1,$(subst -, ,$@)) "TARGET_OPTIONS=$(strip $(filter-out $(word 1,$(subst -, ,$@)),$(subst -, ,$@)))"

else
# We have split the target name and spec.  Invoke the appropriate makefile
# to build whatever the target is

# Pick the make target apart to see what it is.
# It could be:
#     - a simulator, in which case:
#		$@ names a directory under FLEXUS_ROOT/simulators
#     - a special make target, in which case:
#	      Makefile.$@ exists and supports that target
# Otherwise, it is an error.  In the case of an error, we print out all legal targets

.DEFAULT core:
	if [ -e makefile.$@ ] ; then \
		$(MAKE) $(SILENT_MAKE) -f makefile.$@ $@ ; \
	elif [ -d $(SIMULATORS_DIR)/$@ ] ; then \
		$(MAKE) $(SILENT_MAKE) -f makefile.simulators $@ ; \
	elif [ -d $(COMPONENTS_DIR)/$@ ] ; then \
		$(MAKE) $(SILENT_MAKE) -f makefile.components $@ ; \
	else \
		if [ "$@" != "list_targets" ] ; then \
			echo "$@ is not a valid simulator or component." ; \
		fi ; \
		echo "Supported simulators:" ; \
		ls -ICVS $(SIMULATORS_DIR) ; \
		echo "Supported components:" ; \
		ls -ICVS $(COMPONENTS_DIR) ; \
	fi

endif
endif
endif
