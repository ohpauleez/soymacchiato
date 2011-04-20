#! /bin/ksh
#
# Copyright (c) 2008 Oracle and/or its affiliates. All rights reserved.
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

nflag=false
case ${1-} in
-n) nflag=true; shift;;
esac

sources=${1-sources}
patches=${2-patches}
case $# in
[012]) true;;
*) sources=/no/such/dir;;
esac

[ ! -d "$sources" -o ! -d "$patches" ] && {
  >&2 echo "Usage: $0 sources patches"
  exit 2
}


here=$(pwd -P)
sources=$(cd "$sources"; pwd -P)
patches=$(cd "$patches"; pwd -P)

find "$patches" -name .hg \
 |
while read prepo; do
  prepo=${prepo%"/.hg"}
  # Do not link a repo if there is no 'series' file.
  [ ! -f "$prepo/series" ] && continue
  srepo=$sources/${prepo#"$patches/"}
  $nflag || {
    [ ! -d "$srepo/.hg" ] && {
      >&2 echo "*** Skipping: ${prepo#$here/}"
      continue
    }
    [ -h "$srepo/.hg/patches" ] && {
      # silently remove previous sym-links
      rm -f "$srepo/.hg/patches"
    }
    [ -r "$srepo/.hg/patches" ] && {
      >&2 echo "*** Already exists; please remove: ${srepo#$here/}/.hg/patches"; exit 2
    }
  }
  linkname="$prepo"
  [[ "$prepo" == "$here/"* && "$srepo" == "$here"/* ]] && {
    stepup=${srepo#"$here/"}/.hg
    # turn all path components to '..'
    ##bash-isms: shopt -s extglob; stepup=${stepup//+([^\/])/'..'}
    stepup=$(echo "$stepup" | sed 's:[^/][^/]*:..:g')
    linkname=$stepup/${linkname#"$here/"}
    ( cd "$srepo/.hg"; [ -d "$linkname/." ] ||
        echo "*** Problem linking: $linkname" )
  }
  linkdest=$srepo/.hg
  linkdest=${linkdest#"$here/"}
  $nflag && {
    echo ln -s "$linkname" "$linkdest/patches"
    continue
  }
  [ -d "$linkdest" ] || >&2 echo "*** Problem naming: $linkdest"
  echo + \
  ln -s "$linkname" "$linkdest/patches"
  ln -s "$linkname" "$linkdest/patches"
done
