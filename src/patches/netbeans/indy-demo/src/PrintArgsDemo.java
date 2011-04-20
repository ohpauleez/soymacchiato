/*
 * Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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
import static java.dyn.MethodHandles.*;
import static java.dyn.MethodType.*;

class PrintArgsDemo {
    public static void main(String... av) throws Throwable {
        System.out.println("Printing some argument lists, starting with a empty one:");
        INDY_nothing().invokeExact();
        INDY_foo().invokeExact("foo arg");
        INDY_bar().invokeExact("bar arg", 1);
        test(); //INDY_baz.invokeExact("baz arg", 2, 3.14);
        System.out.println("Done printing argument lists.");
    }

    //// BEGIN JAVADOC EXAMPLE
static void test() throws Throwable {
    INDY_baz().invokeExact("baz arg", 2, 3.14);
}
private static void printArgs(Object... args) {
  System.out.println(java.util.Arrays.deepToString(args));
}
private static final MethodHandle printArgs;
static {
  MethodHandles.Lookup lookup = MethodHandles.lookup();
  Class thisClass = lookup.lookupClass();  // (who am I?)
  try {
      printArgs = lookup.findStatic(thisClass,
          "printArgs", MethodType.methodType(void.class, Object[].class));
  } catch (NoAccessException ex) { throw new RuntimeException(ex); }
}
private static CallSite bootstrapDynamic(Lookup caller, String name, MethodType type) {
  // ignore caller and name, but match the type:
  return new ConstantCallSite(MethodHandles.collectArguments(printArgs, type));
}
    //// END JAVADOC EXAMPLE

    private static MethodHandle INDY_nothing() throws Throwable {
        return ((CallSite) MH_bsm().invokeGeneric(lookup(),
                                                  "nothing", methodType(void.class)
                                                  )).dynamicInvoker();
    }
    private static MethodHandle INDY_foo() throws Throwable {
        return ((CallSite) MH_bsm().invokeGeneric(lookup(),
                                                  "foo", methodType(void.class, String.class)
                                                  )).dynamicInvoker();
    }
    private static MethodHandle INDY_bar() throws Throwable {
        return ((CallSite) MH_bsm().invokeGeneric(lookup(),
                                                  "bar", methodType(void.class, String.class, int.class)
                                                  )).dynamicInvoker();
    }
    private static MethodHandle INDY_baz() throws Throwable {
        return ((CallSite) MH_bsm().invokeGeneric(lookup(),
                                                  "baz", methodType(void.class, String.class, int.class, double.class)
                                                  )).dynamicInvoker();
    }
    private static MethodType MT_bsm() {
        return methodType(CallSite.class, Lookup.class, String.class, MethodType.class);
    }
    private static MethodHandle MH_bsm() throws ReflectiveOperationException {
        return lookup().findStatic(lookup().lookupClass(), "bootstrapDynamic", MT_bsm());
    }
}
