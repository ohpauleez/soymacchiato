/*
 * Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package recipes;

import java.dyn.*;
import static java.dyn.MethodHandles.*;
import static java.dyn.MethodType.*;
import static recipes.Utensil.*;

public class FastAndSlow {
    private static final Lookup LOOKUP = lookup();

    // ----

    static Object fastAdd(int x, int y) {
        int z = x+y;
        if ((x ^ y) >= 0 && (x ^ z) < 0) {
            println("oops, it's overflowing");
            return slowAdd(x, y);
        }
        return z;
    }

    // ----

    static Object slowAdd(Object x, Object y) {
        double xd = ((Number)x).doubleValue();
        double yd = ((Number)y).doubleValue();
        println("I'm hungry; is it done yet?");
        return xd + yd;
    }

    // ----

    static boolean bothInts(Object x, Object y) {
        return x instanceof Integer && y instanceof Integer;
    }

    // ---- MH constants (could be loaded from constant pool, also)
    static private final MethodHandle fastAdd, slowAdd, bothInts;
    static { try {
        fastAdd =
            LOOKUP.findStatic(FastAndSlow.class, "fastAdd",
                methodType(Object.class, int.class, int.class));
        slowAdd =
            LOOKUP.findStatic(FastAndSlow.class, "slowAdd",
                methodType(Object.class, Object.class, Object.class));
        bothInts =
            LOOKUP.findStatic(FastAndSlow.class, "bothInts",
                methodType(boolean.class, Object.class, Object.class));
        } catch (NoAccessException ex) { throw new RuntimeException(ex); }
    }

    static MethodHandle makeCheckedFastAdd() {
        MethodHandle fastPath = fastAdd.asType(slowAdd.type());
        return guardWithTest(bothInts, fastPath, slowAdd);
    }

    public static void main(String... av) throws Throwable {
        MethodHandle combo = makeCheckedFastAdd();
        println(combo.invokeWithArguments(2, 3));
        println(combo.invokeWithArguments(2.1, 3.1));
        println(combo.invokeWithArguments(Integer.MAX_VALUE, -1));
        println(combo.invokeWithArguments(Integer.MAX_VALUE, 1));
    }
}
