#! /bin/sh
#
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

qflag=
nflag=
case ${1-} in
-q) qflag=$1; shift;;
-n) nflag=$1; shift;;
esac

sources=.

exit_status=0

for d in $(cd "$sources"; echo *); do
  srepo=$sources/$d
  # Do not mention a repo if there is no 'series' file.
  [ ! -f "$srepo/.hg/patches/series" ] && continue
  case $# in
  0) echo "$srepo"; continue;;
  esac
  case $qflag,$nflag in
  ,)    >&2 echo "+ (cd $srepo; $@)";;
  *,-n) echo "(cd $srepo; $@)"; continue;;
  esac
  (cd "$srepo"; eval "$@") \
  || {
    exit_status=$?
    >&2 echo "*** Exit status $exit_status."
  }
done

exit $exit_status
