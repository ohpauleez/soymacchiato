/*
 * Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
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

package sun.invoke.util;

import org.junit.Test;
import sun.invoke.util.Wrapper;
import static sun.invoke.util.Wrapper.*;

/**
 *
 * @author jrose
 */
public class WrapperTest {
    static void assertLocal(boolean z) {
        if (!z)  throw new InternalError();
    }

    @Test public void test() { test(true); }

    static boolean test(boolean verbose) {
        Class<Integer> PTYPE = int.class, WTYPE = Integer.class;
        assertLocal(PTYPE != WTYPE);
        assertLocal(forPrimitiveType(PTYPE) == INT);
        assertLocal(forWrapperType(WTYPE) == INT);
        assertLocal(asPrimitiveType(PTYPE) == PTYPE);
        assertLocal(asPrimitiveType(WTYPE) == PTYPE);
        assertLocal(  asWrapperType(PTYPE) == WTYPE);
        assertLocal(  asWrapperType(WTYPE) == WTYPE);

        assertLocal(forWrapperType(Object.class) == OBJECT);
        assertLocal(forWrapperType(Iterable.class) == OBJECT);
        assertLocal(forWrapperType(Void.class) == VOID);
        assertLocal(forWrapperType(Integer.class) == INT);
        assertLocal(forPrimitiveType(void.class) == VOID);
        assertLocal(forPrimitiveType(int.class).bitWidth() == 32);
        assertLocal(forPrimitiveType(char.class) == CHAR);
        assertLocal(forPrimitiveType(char.class).bitWidth() == 16);
        assertLocal(forPrimitiveType(short.class).bitWidth() == 16);
        assertLocal(forPrimitiveType(byte.class).bitWidth() == 8);
        assertLocal(forPrimitiveType(boolean.class).bitWidth() == 1);
        assertLocal(forPrimitiveType(double.class).bitWidth() == 64);
        assertLocal(forPrimitiveType(float.class).bitWidth() == 32);

        Wrapper[] wrappers = { BYTE, SHORT, INT, LONG, BOOLEAN, CHAR, FLOAT, DOUBLE, VOID, OBJECT };
        assertLocal(values().length == wrappers.length);
        String chars = "BSIJ"          + "ZCFD"          + "VL";
        String bitws = "\10\20\40\100" + "\01\20\40\100" + "\0\0";
        String kinds = "\1\1\1\1"      + "\2\2\4\4"      + "\0\0";
        String slots = "\1\1\1\2"      + "\1\1\1\2"      + "\0\1";
        for (int i = 0; i < wrappers.length; i++) {
            Wrapper w = wrappers[i];
            if (verbose)  System.out.println("testing "+w);
            assertLocal(forPrimitiveType(w.primitiveType()) == w);
            assertLocal(forWrapperType(w.wrapperType()) == w);
            assertLocal(w.basicTypeChar() == chars.charAt(i));
            assertLocal(w.bitWidth() == bitws.charAt(i));
            int wkind = (w.isSigned() ? 1 : 0) | (w.isUnsigned() ? 2 : 0) | (w.isFloating() ? 4 : 0);
            assertLocal(wkind == kinds.charAt(i));
            assertLocal(w.stackSlots() == slots.charAt(i));
            assertLocal(w.isNumeric()  == ((wkind & 7) != 0));
            assertLocal(w.isIntegral() == ((wkind & 3) != 0));
            assertLocal(w.isSubwordOrInt() == (w.isIntegral() && w.bitWidth() <= 32));
        }
        return true;
    }
    public static void main(String... av) {
        test(true);
    }
}