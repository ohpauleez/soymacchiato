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
import java.util.*;
import static java.dyn.MethodHandles.*;
import static java.dyn.MethodType.*;

public class Main {
    static final Class[] CLASSES = {
        // Commented classes have not yet been converted to Indify format.
        Hello.class,
        PrintArgsDemo.class,
        //JavaDocExamples.class,
        GetNameDemo.class,
        FidgetDemo.class,
        //recipes.PlainOldJava.class,
        //recipes.FastAndSlow.class,
        //recipes.InlineCache.class,
        //recipes.Curry.class
    };
    public static void doClasses(Class[] classes, String[] args) throws Throwable {
        for (Class c : classes) {
            MethodHandle main = lookup().findStatic(c, "main",
                    methodType(void.class, String[].class));
            System.out.println("******** "+c.getName()+"."+main);
            main.invokeExact(args);
        }
    }
    public static void main(String[] args) throws Throwable {
        int arg1 = 0;
        if (args.length >= 1) {
            try {
                arg1 = Integer.parseInt(args[0]);
                args = Arrays.copyOfRange(args, 1, args.length);
            } catch (NumberFormatException ex) {
            }
        }
        if (arg1 == 0) {
            doClasses(CLASSES, args);
        } else {
            for (int i = 0; i < arg1; i++) {
                System.out.println("******** ITERATION #"+i);
                doClasses(CLASSES, args);
            }
        }
    }
}
