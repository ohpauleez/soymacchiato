/*
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
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

import java.util.*;
import java.dyn.*;
import static java.dyn.MethodHandles.*;

class Utensil {
    public static void println(Object x) {
        if (x instanceof MethodHandle)
            x = withType((MethodHandle)x);
        System.out.println(x);
    }
    public static String withType(MethodHandle x) {
        return x.toString() + x.type();
    }

    private static final Lookup LOOKUP = lookup();

    /** Return the identity function from Object to Object. */
    public static MethodHandle identity() {
        try {
            return LOOKUP.findStatic(Utensil.class, "identity", MethodType.genericMethodType(1));
        } catch (NoAccessException ex) {
            // kitchen accident; call 911
            throw new RuntimeException(ex);
        }
    }

    private static Object identity(Object x) { return x; }

    private static final Object[] NO_ARGS_ARRAY = {};
    private static Object[] array() { return NO_ARGS_ARRAY; }
    private static Object[] makeArray(Object... args) { return args; }
    private static Object[] array(Object a0)
                { return makeArray(a0); }
    private static Object[] array(Object a0, Object a1)
                { return makeArray(a0, a1); }
    private static Object[] array(Object a0, Object a1, Object a2)
                { return makeArray(a0, a1, a2); }
    private static Object[] array(Object a0, Object a1, Object a2, Object a3)
                { return makeArray(a0, a1, a2, a3); }
    private static Object[] array(Object a0, Object a1, Object a2, Object a3,
                                  Object a4)
                { return makeArray(a0, a1, a2, a3, a4); }

    /** Return a method handle that takes the indicated number of Object
     *  arguments and returns an Object array of them, as if for varargs.
     */
    public static MethodHandle array(int nargs) {
        try {
            MethodType type = MethodType.genericMethodType(nargs).changeReturnType(Object[].class);
            return LOOKUP.findStatic(Utensil.class, "array", type);
        } catch (NoAccessException ex) {
            // kitchen accident; call 911
            throw new RuntimeException(ex);
        }
    }

    private static final List<Object> NO_ARGS_LIST = Arrays.asList(NO_ARGS_ARRAY);
    private static List<Object> list() { return NO_ARGS_LIST; }
    private static List<Object> makeList(Object... args) { return Arrays.asList(args); }
    private static List<Object> list(Object a0)
                { return makeList(a0); }
    private static List<Object> list(Object a0, Object a1)
                { return makeList(a0, a1); }
    private static List<Object> list(Object a0, Object a1, Object a2)
                { return makeList(a0, a1, a2); }
    private static List<Object> list(Object a0, Object a1, Object a2, Object a3)
                { return makeList(a0, a1, a2, a3); }
    private static List<Object> list(Object a0, Object a1, Object a2, Object a3,
                                     Object a4)
                { return makeList(a0, a1, a2, a3, a4); }

    /** Return a method handle that takes the indicated number of Object
     *  arguments and returns an Object array of them, as if for varargs.
     */
    public static MethodHandle list(int nargs) {
        try {
            MethodType type = MethodType.genericMethodType(nargs).changeReturnType(List.class);
            return LOOKUP.findStatic(Utensil.class, "list", type);
        } catch (NoAccessException ex) {
            // kitchen accident; call 911
            throw new RuntimeException(ex);
        }
    }
}
