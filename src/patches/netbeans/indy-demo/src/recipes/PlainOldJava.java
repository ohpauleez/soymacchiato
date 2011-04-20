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

import java.dyn.*;
import static recipes.Utensil.*;

public class PlainOldJava {
    private static final MethodHandles.Lookup LOOKUP = MethodHandles.lookup();

    public static void main(String... av) throws Throwable {
        for (int i = 0; i < 4; i++) {
            test();
        }
    }
    static void test() throws Throwable {
        // ---

        java.io.File file = new java.io.File("muffin.txt");
        println(file.getName());
        MethodHandle getName =
                LOOKUP.findVirtual(file.getClass(), "getName",
                            MethodType.methodType(String.class));
        println(getName.<String>invokeExact(file));

        // ---

        MethodHandle charAt =
                LOOKUP.findVirtual(String.class, "charAt",
                            MethodType.methodType(char.class, int.class));
        println(charAt.<char>invokeExact("foam", 3));

        // ---
        
        // invokedynamic
        println(InvokeDynamic.<String>getName(file));
        println(InvokeDynamic.<String>toString((CharSequence) "soy latte"));
        println(InvokeDynamic.<String>
                    #"static:\=java\|lang\|Integer:toHexString"
                        (0xCAFE));
    }

    // Linkage logic for all invokedynamic call sites in this class.
    private static MethodHandle linkDynamic(Class caller, String name, MethodType type) throws NoAccessException {
        System.out.println("[link] linkDynamic "+caller+" "+name+type);
        MethodHandles.Lookup lookup = MethodHandles.lookup();
        MethodHandle target = null;  // result of call linkage
        Class<?>     recvType = null;  // receiver (1st arg) type if any
        if (type.parameterCount() >= 1) {
            recvType = type.parameterType(0);
            if (recvType.isPrimitive())  recvType = null;
        }
        if (target == null && recvType != null) {
            MethodType dropRecvType = type.dropParameterTypes(0, 1);
            target = lookup.findVirtual(recvType, name, dropRecvType);
        }
        if (target == null && recvType != null && name.startsWith("special:")) {
            MethodType dropRecvType = type.dropParameterTypes(0, 1);
            target = lookup.findSpecial(recvType, name, dropRecvType, caller);
        }
        if (target == null && name.startsWith("static:\\=")) {
            int cbeg = name.indexOf('=')+1;
            int cend = name.indexOf(':', cbeg);
            String cname = name.substring(cbeg, cend).replaceAll("[\\\\][|]", ".");
            String mname = name.substring(cend+1);
            try {
                Class<?> cls = Class.forName(cname);
                target = lookup.findStatic(cls, mname, type);
            } catch (ClassNotFoundException ex) {
            }
        }
        if (target == null) {
            throw new InvokeDynamicBootstrapError("linkage failed for "+name);
        }
        return target;
    }

    // Set up a class-local bootstrap method.
    static { Linkage.registerBootstrapMethod("bootstrapDynamic"); }
    private static CallSite bootstrapDynamic(Class caller, String name, MethodType type) throws NoAccessException {
        MethodHandle target = linkDynamic(caller, name, type);
        println("[link] set site target to "+target);
        if (!target.type().equals(type)) {
            target = MethodHandles.convertArguments(target, type);
            println("[link]   with conversions "+target);
        }
        println("[link] site is linked; this is the last time for "+name);
        return new CallSite(target);
    }
}
