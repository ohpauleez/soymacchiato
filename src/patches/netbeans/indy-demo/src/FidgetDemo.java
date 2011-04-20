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
 *
 */


/*
 * @test
 * @bug 6829144
 * @summary smoke test for invokedynamic
 */

import java.dyn.*;
import static java.dyn.MethodType.*;
import static java.dyn.MethodHandles.*;

class FidgetDemo {
    public static void main(String... av) throws Throwable {
        if (av.length == 0)  av = new String[]{ "fred", "buster", "ricky" };
        System.out.println("Fidgety self-modifying call site...");
        for (int i = 0; i < 6; i++) {
            System.out.println("--- loop #"+i);
            for (String a : av) {
                String result = (String) INDY_fidget().invokeExact(a);
                System.out.println(result);
            }
        }
    }

    private static String itch(String x) { return x+" can't get comfortable"; }
    private static String fuss(String x) { return x+" is feeling moody"; }
    private static String bore(String x) { return x+" needs a change of scenery"; }
    private static final String[] NAMES = { "itch", "fuss", "bore" };
    private static MethodHandle which(int i) throws NoAccessException {
        return lookup().findStatic(FidgetDemo.class, NAMES[i % NAMES.length],
                    methodType(String.class, String.class));
    }

    // It is easy to build hybrid closure-like method handles out of classes.
    private static class Guard {
        MethodHandle target;  // wrapped target
        CSite        site;    // included site
        // constructor folds a sneaky site reference into a direct target
        Guard(CSite site, MethodHandle target) {
            this.target = target;
            this.site = site;
        }

        String invoke(String x) throws Throwable {
            if ((++site.count & 3) == 0)
                site.setTarget(new Guard(site, which(site.count)).asMethodHandle());
            return (String) target.<String>invokeExact(x);
        }
        public MethodHandle asMethodHandle() {
            return INVOKE.bindTo(this);
        }
        public MethodHandle asMethodHandle(MethodType type) {
            return asMethodHandle().asType(type);
        }
        private static final MethodHandle INVOKE;
        static {
            try {
                INVOKE = lookup().findVirtual(Guard.class, "invoke",
                    methodType(String.class, String.class));
            } catch (NoAccessException ex) {
                throw new RuntimeException(ex);
            }
        }
        public String toString() { return "Guard:"+target.toString(); }
    }

    // Use a local call site subclass.  (These are optional but fun.)
    private static class CSite extends MutableCallSite {
        int count;
        public CSite(Class caller, String name, MethodType type) throws NoAccessException {
            super(type);
            System.out.println("[link] new call site: "+this);
            setTarget(new Guard(this, which(0)).asMethodHandle());
        }
        // this is just for the noise value:
        public void setTarget(MethodHandle t) {
            System.out.println("[link] set target to "+t);
            super.setTarget(t);
        }
    }

    // Set up a class-local bootstrap method.
    //static { Linkage.registerBootstrapMethod("linkDynamic"); }
    private static CallSite linkDynamic(Lookup caller, String name, MethodType type) throws NoAccessException {
        assert((Object)name == "fidget" &&
                type == methodType(String.class, String.class));
        return new CSite(caller.lookupClass(), name, type);
    }

    // names recognized by Indify:
    private static MethodType MT_bsm() {
        return methodType(CallSite.class, Lookup.class, String.class, MethodType.class);
    }
    private static MethodHandle INDY_fidget;
    private static MethodHandle INDY_fidget() throws Throwable {
        if (INDY_fidget != null)  return INDY_fidget;
        System.out.println("*** INDY_fidget: running BSM manually");
        CallSite site = (CallSite) lookup().findStatic(FidgetDemo.class, "linkDynamic", MT_bsm())
            .invokeGeneric(lookup(), "fidget", methodType(String.class, String.class));
        return INDY_fidget = site.dynamicInvoker();
    }
}

/* resulting output
--------
Fidgety self-modifying call site...
--- loop #0
[link] new call site: CallSite#14491894[fidget(java.lang.String)java.lang.String => null]
[link] set target to Guard:itch(java.lang.String)java.lang.String
fred can't get comfortable
buster can't get comfortable
ricky can't get comfortable
--- loop #1
[link] set target to Guard:fuss(java.lang.String)java.lang.String
fred can't get comfortable
buster is feeling moody
ricky is feeling moody
--- loop #2
fred is feeling moody
[link] set target to Guard:bore(java.lang.String)java.lang.String
buster is feeling moody
ricky needs a change of scenery
--- loop #3
fred needs a change of scenery
buster needs a change of scenery
[link] set target to Guard:itch(java.lang.String)java.lang.String
ricky needs a change of scenery
--- loop #4
fred can't get comfortable
buster can't get comfortable
ricky can't get comfortable
--- loop #5
[link] set target to Guard:fuss(java.lang.String)java.lang.String
fred can't get comfortable
buster is feeling moody
ricky is feeling moody
--------
 */
