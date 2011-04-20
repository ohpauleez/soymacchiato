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

# Report the last tag in sources/.../.hgtags.
# Hopefully, that will be the most recent one also.
# Also report any hashcode in the first line of source/.../series.

command=${0##*/}

hflag=
tflag=

while true; do
  case $# in
  0) break;;
  esac
  case ${1-} in
    -h) hflag=$1;;
    -t) tflag=$1;;
    *)  >&2 echo "Usage: sh $command [ -h | -t ]"; exit 2;;
  esac
  shift
done

print_hash() {
  d='[0-9a-fA-F]'; not_d='[^'${d#'['}
  d12=$d$d$d$d$d$d$d$d$d$d$d$d
  sed -n < .hg/patches/series "
    s/^.*${not_d}\(${d12}\)/\1/
    s/^\(${d12}\)${not_d}.*/\1/
    /^${d12}$/p
    q
  "
}

print_tag() {
  sed -n < .hgtags "
    s/#.*//; s/^  *//; s/  *$//
    s/.* //p
  " | tail -1
}

case $hflag,$tflag in
'','') print_hash;;
-h,'') print_hash;;
-h,-t) print_hash; print_tag;;
'',-t) print_tag;;
esac
