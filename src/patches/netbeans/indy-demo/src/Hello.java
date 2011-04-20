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

import java.dyn.*;
import java.dyn.MethodHandles.Lookup;

public class Hello {
    public static void main(String... av) throws Throwable {
        if (av.length == 0)  av = new String[] { "world" };
        greeter(av[0] + " (from a statically linked call site)");
        for (String whom : av) {
            greeter.invokeExact(whom);  // strongly typed direct call
            // previous line generates invokevirtual MethodHandle.invokeExact(String)void
            Object x = whom;
            x = INDY_hail().invokeExact(x);      // weakly typed invokedynamic
            // previous line generates invokedynamic hail(Object)Object
        }
    }

    static void greeter(String x) { System.out.println("Hello, "+x); }
    // intentionally pun between the method and its reified handle:
    static final MethodHandle greeter;
    static { try { greeter
            = MethodHandles.lookup().findStatic(Hello.class, "greeter",
                           MethodType.methodType(void.class, String.class));
        } catch (NoAccessException ex) {
            throw new RuntimeException(ex);
        }
    }

    // Set up a class-local bootstrap method.
    private static CallSite bootstrapDynamic(Lookup caller, String name, MethodType type) {
        assert(type.parameterCount() == 1 && name.equals("hail"));  // in lieu of MOP
        System.out.println("set target to adapt "+greeter);
        return new VolatileCallSite(greeter.asType(type));
    }

    private static MethodType MT_bsm() {
        return MethodType.methodType(CallSite.class, Lookup.class, String.class, MethodType.class);
    }
    private static MethodHandle MH_bsm() throws ReflectiveOperationException {
        return MethodHandles.lookup().findStatic(MethodHandles.lookup().lookupClass(), "bootstrapDynamic", MT_bsm());
    }

    private static MethodHandle INDY_hail;
    private static MethodHandle INDY_hail() throws Throwable {
        if (INDY_hail != null)  return INDY_hail;
        System.out.println("*** INDY_hail: running BSM manually");
        CallSite site = (CallSite) MH_bsm()
            .invokeGeneric(MethodHandles.lookup(), "hail", MethodType.methodType(Object.class, Object.class));
        return INDY_hail = site.dynamicInvoker();
    }
}
