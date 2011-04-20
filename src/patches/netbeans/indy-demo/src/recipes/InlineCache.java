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

package recipes;

/* Run like this:
   java -XX:+UnlockExperimentalVMOptions -XX:+EnableInvokeDynamic -cp dist/InvokeDynamicDemo.jar recipes.InlineCache 100000
   To examine the code, use: -XX:CompileCommand=print,*.doOneCall
   Make sure a copy of the 'hsdis' plugin is on your dynamic library search path.
   This plugin can be built from sources in the OpenJDK.
   For x86, it can also be obtained from: http://kenai.com/projects/base-hsdis
 */

import java.dyn.*;
import java.dyn.MethodHandles.*;

public class InlineCache {
    public static void main(String... av) throws Throwable {
        int limit = 100000;
        if (av.length > 0)
            limit = Integer.parseInt(av[0]);
        for (int i = 0; i < limit; i++) {
            int x = i%97, y = (i*11)%91;
            int z = (Integer) doOneCall((Integer)x, (Integer)y);
            if (z != x + y)  throw new RuntimeException("fail");
        }
    }

    static Object doOneCall(Object x, Object y) throws Throwable {
        return InvokeDynamic.facciaDerPrammaKudasai(x, y); // do the thing, please
    }

    // ---- invokedynamic infrastructure

    static {
        Linkage.registerBootstrapMethod("bootstrapDynamic");
    }
    private static CallSite bootstrapDynamic(Class caller, String name, MethodType type) {
        // ignore caller and name, but get the type right:
        return new CallSite(FastAndSlow.makeCheckedFastAdd().asType(type));
    }
}
