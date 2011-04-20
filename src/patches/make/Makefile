# Copyright (c) 2008, 2009, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.

RUN_FROM_DIR = ../..

SH = ksh  # works everywhere

GLOBAL_GUARDS = buildable testable

# RELAX_CHECKS |= FORCE_VERSIONS
RELAX_CHECKS$(RELAX_CHECKS) = $(FORCE_VERSIONS)

# if (RELAX_CHECKS) disable check
MAYBE_CHECK$(RELAX_CHECKS) = check

# if (!FORCE_VERSIONS) disable force
MAYBE_FORCE = force
MAYBE_FORCE$(FORCE_VERSIONS) = #off

all: setup $(MAYBE_FORCE) $(MAYBE_CHECK) patch build

setup: link-patch-dirs mq-select-guards
check: hg-check-versions
force: hg-force-versions
unforce: hg-unforce-versions
patch: mq-push-patches
build:
	cd $(RUN_FROM_DIR); $(SH) patches/make/each-patch-repo.sh \
		"(cd make; $(MAKE))"

link-patch-dirs: $(RUN_FROM_DIR)/sources $(RUN_FROM_DIR)/patches
	cd $(RUN_FROM_DIR); bash patches/make/link-patch-dirs.sh sources patches
	@echo "# Should be identical files:"
	cd $(RUN_FROM_DIR); ls -il patches/hotspot/series sources/hotspot/.hg/patches/series

mq-select-guards:
	cd $(RUN_FROM_DIR); $(SH) patches/make/each-patch-repo.sh \
		"hg qpop -a; hg qselect $(GLOBAL_GUARDS)" \
			"\$$($(SH) `pwd`/patches/make/current-release.sh)"
	# report what happened:
	cd $(RUN_FROM_DIR); $(SH) patches/make/each-patch-repo.sh 'hg qselect; hg qunapplied'

mq-push-patches:
	cd $(RUN_FROM_DIR); $(SH) patches/make/each-patch-repo.sh \
		'(! hg qunapplied | read something) || hg qpush -a'

hg-check-versions:
	@echo "# If this command fails, consider setting RELAX_CHECKS=true or FORCE_VERSIONS=true."
	cd $(RUN_FROM_DIR); $(SH) patches/make/each-patch-repo.sh \
		hg diff --git -I src -r "\$$($(SH) `pwd`/patches/make/current-release.sh)" \
		"> current-diffs.txt;" \
		"if [ ! -s current-diffs.txt ]; then rm current-diffs.txt;" \
		"else grep '^diff' current-diffs.txt; exit 1; fi"

hg-force-versions:
	cd $(RUN_FROM_DIR); $(SH) patches/make/each-patch-repo.sh \
		hg update -r "\$$($(SH) `pwd`/patches/make/current-release.sh)"

hg-unforce-versions:
	cd $(RUN_FROM_DIR); $(SH) patches/make/each-patch-repo.sh "hg update"

.PHONY: \
	setup check force unforce patch build \
	link-patch-dirs mq-select-guards mq-push-patches \
	hg-check-versions hg-force-versions hg-unforce-versions \
	# end

