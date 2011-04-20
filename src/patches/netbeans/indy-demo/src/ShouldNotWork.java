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

import java.dyn.*;

public class ShouldNotWork {
    public static void main(String... av) throws Throwable {
        test();
    }

    static class Super {
        Super(String x) {
            System.out.println("super "+x+"!");
        }
        void foo(String x) {
            System.out.println("Super.foo "+x);
        }
    }
    static class Sub extends Super {
        static final MethodHandle SPECIAL1, SPECIAL2;
        static {
            try {
                SPECIAL1 = MethodHandles.lookup().findSpecial(Super.class, "foo",
                    MethodType.methodType(void.class, String.class), Sub.class);
                SPECIAL2 = MethodHandles.lookup().findSpecial(Sub.class, "<init>",
                    MethodType.methodType(void.class), Sub.class);
            } catch (NoAccessException ex) { throw new RuntimeException(ex); }
        }
        Sub(String x) throws Throwable {
            super(x);
            System.out.println("invoking "+SPECIAL1);
            SPECIAL1.invokeExact((Super)this, x+" via method handle");
            System.out.println("should fail:  invoking "+SPECIAL2);
            SPECIAL2.invokeExact(this);
        }
        @Override
        void foo(String x) {
            System.out.println("Sub.foo "+x);
            super.foo(x+"+ via invokespecial");
        }
        Sub() {
            super("Sub????");
        }
    }
    static void test() throws Throwable {
        new Sub("legitimate");
    }
}
