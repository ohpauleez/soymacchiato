/* Declarations for System V style searching functions.
   Copyright (C) 1995-1999, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _HSEARCH_R_H_
#define _HSEARCH_R_H_

#include <search.h>

struct _ENTRY;

/* Data type for reentrant functions.  */
struct hsearch_data
  {
    struct _ENTRY *table;
    unsigned int size;
    unsigned int filled;
  };

/* Reentrant versions which can handle multiple hashing tables at the
   same time.  */
extern int hsearch_r (ENTRY __item, ACTION __action, ENTRY **__retval,
                      struct hsearch_data *__htab);
extern int hcreate_r (size_t __nel, struct hsearch_data *__htab);
extern void hdestroy_r (struct hsearch_data *__htab);

#endif /* _HSEARCH_R_H_ */
