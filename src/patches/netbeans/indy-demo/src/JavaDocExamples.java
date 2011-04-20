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
import static java.dyn.MethodHandles.*;
import static java.dyn.MethodType.*;

public class JavaDocExamples {
    public static void main(String... av) throws Throwable {
        forMethodHandle();
        forJavaMethodHandle();
        forInvokeDynamic();
    }

    static boolean successful(String who) {
        System.out.println(who + ": all assertions passed");
        return true;
    }

    /** Code examples from the javadoc in java.dyn.MethodHandle. */
    static void forMethodHandle() throws Throwable {
{;;;}
Object x, y; String s; int i;
MethodType mt; MethodHandle mh;
MethodHandles.Lookup lookup = MethodHandles.lookup();
// mt is {(char,char) => String}
mt = MethodType.methodType(String.class, char.class, char.class);
mh = lookup.findVirtual(String.class, "replace", mt);
// (Ljava/lang/String;CC)Ljava/lang/String;
s = (String) mh.<String>invokeExact("daddy",'d','n');
assert(s.equals("nanny"));
// weakly typed invocation (using MHs.invoke)
s = (String) mh.<String>invokeGeneric("sappy", 'p', 'v');
assert(s.equals("savvy"));
// mt is {Object[] => List}
mt = MethodType.methodType(java.util.List.class, Object[].class);
mh = lookup.findStatic(java.util.Arrays.class, "asList", mt);
// mt is {(Object,Object,Object) => Object}
mt = MethodType.genericMethodType(3);
mh = MethodHandles.collectArguments(mh, mt);
// mt is {(Object,Object,Object) => Object}
// (Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
x = mh.invokeExact((Object)1, (Object)2, (Object)3);
assert(x.equals(java.util.Arrays.asList(1,2,3)));
// mt is { => int}
mt = MethodType.methodType(int.class);
mh = lookup.findVirtual(java.util.List.class, "size", mt);
// (Ljava/util/List;)I
i = (int) mh.<int>invokeExact(java.util.Arrays.asList(1,2,3));
assert(i == 3);
{;;;}
        assert(successful("forMethodHandle"));
        oldStuffForMethodHandle();
    }

    static void oldStuffForMethodHandle() throws Throwable {
        Object x, y; String s; int i;
        MethodType mt; MethodHandle mh;
        // for type imports:
        mh = lookup().findVirtual(String.class, "hashCode", methodType(int.class));
        i = (int) mh.<int>invokeExact("foo");
        assert(i == "foo".hashCode());
        mh = lookup().findVirtual(String.class, "concat", methodType(String.class, String.class));
        s = (String) mh.<String>invokeExact("foo", "tball");
        assert(s.equals("football"));
        mh = lookup().findVirtual(String.class, "replace", methodType(String.class, char.class, char.class));
        s = (String) mh.<String>invokeExact("blubber", 'b', 'f');
        assert(s.equals("fluffer"));
        assert(successful("forMethodHandle #2"));
    }

    /** Code examples from the javadoc in java.dyn.InvokeDynamic. */
    static void forJavaMethodHandle() throws Throwable {
        Greeter greeter = new Greeter("world");
        greeter.run();  // prints "hello, world"
        // Statically typed method handle invocation (most direct):
        MethodHandle mh = greeter.asMethodHandle();
        mh.<void>invokeExact();  // also prints "hello, world"
        // Dynamically typed method handle invocation:
        greeter.asMethodHandle().invokeWithArguments(new Object[0]);  // also prints "hello, world"
    /*!!!
        // We can also do this with symbolic names and/or inner classes:
        new JavaMethodHandle("yow") {
            void yow() { System.out.println("yow, world"); }
        }.invokeWithArguments(new Object[0]);
    */
        assert(successful("forJavaMethodHandle"));
    }

    static
    class Greeter {
        public void run() { System.out.println("hello, "+greetee); }
        private final String greetee;
        Greeter(String greetee) {
            this.greetee = greetee;
        }
        public MethodHandle asMethodHandle() {
            return RUN.bindTo(this);
        }
        public MethodHandle asMethodHandle(MethodType type) {
            return asMethodHandle().asType(type);
        }
        // the entry point function is computed once:
        private static final MethodHandle RUN;
        static {
            try {
                RUN = MethodHandles.lookup().findVirtual(Greeter.class, "run",
                          methodType(void.class));
            } catch (NoAccessException ex) {
                throw new RuntimeException(ex);
            }
        }
    }

    /** Code examples from the javadoc in java.dyn.InvokeDynamic. */
    @BootstrapMethod(value=JavaDocExamples.class, name="bootstrapDynamic")
    static void forInvokeDynamic() throws Throwable {
        exampleForInvokeDynamic();
    }
{}  //// BEGIN JAVADOC EXAMPLE
@BootstrapMethod(value=JavaDocExamples.class, name="bootstrapDynamic")
static void exampleForInvokeDynamic() throws Throwable {
    Object x; String s; int i;
    x = InvokeDynamic.greet("world"); // greet(Ljava/lang/String;)Ljava/lang/Object;
    s = (String) InvokeDynamic.<String>hail(x); // hail(Ljava/lang/Object;)Ljava/lang/String;
    InvokeDynamic.<void>cogito(); // cogito()V
    i = (int) InvokeDynamic.<int>#"op:+"(2, 3); // op:+(II)I
{}
        {}
        assert(i == 2+3);
        assert(successful("forInvokeDynamic"));
    }

    // Set up a class-local bootstrap method.
    private static MethodHandle bootstrapDynamic(Class caller, String name, MethodType type)
        throws NoAccessException
    {
        if (name.contains("op:"))  name = "op";
        return lookup().findStatic(caller, name+"Handler", type);
    }
    private static String hailHandler(Object x) {
        System.out.println("howdy, "+x);
        return "galaxy";
    }
    private static Object greetHandler(String x) {
        return hailHandler((Object)x);
    }
    private static void cogitoHandler() {
        System.out.println("if I only had a brain...");
    }
    private static int opHandler(int x, int y) {
        return x + y;
    }
}
