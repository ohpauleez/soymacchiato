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
import static java.dyn.MethodHandles.*;
import static java.dyn.MethodType.*;
import static recipes.Utensil.*;

public class Curry {
    public static void main(String... av) throws Throwable {
        // ----

        MethodHandle list2 = Utensil.list(2);
        println(list2);  // list2 = {(x,y) => Arrays.asList(x,y)}
        println(list2.invokeGeneric("chicken", "rice"));  // [chicken, rice]

        // curry with chicken or rice:
        MethodHandle partialApp = insertArguments(list2, 0, "curry");
        println(partialApp); // partialApp = {x => list2("curry", x)}
        println(partialApp.invokeGeneric("chicken"));  // [curry, chicken]
        println(partialApp.invokeGeneric("rice"));     // [curry, rice]

        // ----

        // curry with everything:
        MethodHandle list3 = Utensil.list(3);
        println(list3);  // list3 = {(x,y,z) => Arrays.asList(x,y,z)}
        MethodHandle partialApp2 = insertArguments(list3, 0, "curry");
        // partialApp2 = {(x, y) => list3("curry", x, y)}
        println(partialApp2);
        println(partialApp2.invokeGeneric("chicken", "rice"));
                                            // [curry, chicken, rice]

        // ----

        // double curry:
        MethodHandle pa3 = insertArguments(list3, 0, "curry", "chutney");
        // pa3 = {x => list3("curry", "chutney", x)}
        println(pa3);
        println(pa3.invokeGeneric("tofu")); //[curry, chutney, tofu]

        // triple curry:
        MethodHandle pa4 = insertArguments(pa3, 0, "yak");
        // pa4 = { => list3("curry", "chutney", "yak")}
        println(pa4);
        println(pa4.invokeGeneric());  // [curry, chutney, yak]
    }

    static void stronglyTyped() throws Throwable {
        MethodHandle list2 = Utensil.list(2);
        println(list2);  // list2 = {(x,y) => Arrays.asList(x,y)}
        MethodType s2t = methodType(Object.class, String.class, String.class);
        //list2 = convertArguments(list2, s2t);
        println(list2.invokeExact("chicken", "rice"));

        // curry with chicken or rice:
        MethodHandle partialApp
                = MethodHandles.insertArguments(list2, 0, "curry");
        println(partialApp); // partialApp = {x => list2("curry", x)}
        println(partialApp.invokeExact("chicken"));  // [curry, chicken]
        println(partialApp.invokeExact("rice"));     // [curry, rice]

        // curry with everything:
        MethodHandle list3 = Utensil.list(3);
        println(list3);  // list3 = {(x,y,z) => Arrays.asList(x,y,z)}
        MethodType s3t = methodType(Object.class, String.class, String.class, String.class);
        list3 = convertArguments(list3, s3t);
        MethodHandle partialApp2
                = MethodHandles.insertArguments(list3, 0, "curry");
        // partialApp2 = {(x, y) => list3("curry", x, y)}
        println(partialApp2);
        println(partialApp2.invokeExact("chicken", "rice"));  // [curry, chicken, rice]

        // double curry:
        MethodHandle partialApp3
                = MethodHandles.insertArguments(list3, 0, "curry", "chutney");
        // partialApp3 = {x => list3("curry", "chutney", x)}
        println(partialApp3);
        println(partialApp3.invokeExact("tofu"));  // [curry, chutney, tofu]

        // triple curry:
        MethodHandle partialApp4
                = MethodHandles.insertArguments(partialApp3, 0, "yak");
        // partialApp4 = { => list3("curry", "chutney", "yak")}
        println(partialApp4);
        println(partialApp4.invokeExact());  // [curry, chutney, yak]
    }
}
