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

import java.dyn.*;
import static java.dyn.MethodHandles.*;
import static java.dyn.MethodType.*;

class GetNameDemo {
    public static void main(String... av) throws Throwable {
        if (av.length == 0)  av = new String[]{ "fred.txt" };
        java.io.File file = new java.io.File(av[0]);
        for (int i = 0; i < 3; i++) {
            System.out.println("--- loop #"+i);
            String result1 = file.getName();
            System.out.println(result1);
            String result2 = (String) INDY_getName().invokeExact(file);
            System.out.println(result2);
            String result3 = (String) INDY_toString().invokeExact((CharSequence) result1);
            System.out.println(result3);
            String result4 = (String) INDY_toHexString().invokeExact(result1.length());
            System.out.println(result4);
        }
    }

    private static final MethodHandle[] INDY_CACHES = new MethodHandle[3];  // INDY_{getName,toString,toHexString}
    private static MethodHandle INDY_getName() throws Throwable {
        if (INDY_CACHES[0] != null)  return INDY_CACHES[0];
        return INDY_CACHES[0] =
            ((CallSite) MH_bsm().invokeGeneric(lookup(),
                                                "getName", methodType(String.class, java.io.File.class)
                                                )).dynamicInvoker();
    }
    private static MethodHandle INDY_toString() throws Throwable {
        if (INDY_CACHES[1] != null)  return INDY_CACHES[1];
        return INDY_CACHES[1] =
            ((CallSite) MH_bsm().invokeGeneric(lookup(),
                                               "toString", methodType(String.class, CharSequence.class)
                                               )).dynamicInvoker();
    }
    private static MethodHandle INDY_toHexString() throws Throwable {
        if (INDY_CACHES[2] != null)  return INDY_CACHES[2];
        return INDY_CACHES[2] =
            ((CallSite) MH_bsm().invokeGeneric(lookup(),
                                               "static:\\=java\\|lang\\|Integer:toHexString", methodType(String.class, int.class)
                                               )).dynamicInvoker();
    }
    private static MethodType MT_bsm() {
        return methodType(CallSite.class, Lookup.class, String.class, MethodType.class);
    }
    private static MethodHandle MH_bsm() throws ReflectiveOperationException {
        return lookup().findStatic(lookup().lookupClass(), "bootstrapDynamic", MT_bsm());
    }

    // Linkage logic for all invokedynamic call sites in this class.
    private static MethodHandle linkDynamic(Lookup lookup, String name, MethodType type) throws NoAccessException {
        Class<?> caller = lookup.lookupClass();
        System.out.println("[link] linkDynamic "+caller+" "+name+type);
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
    //static { Linkage.registerBootstrapMethod("bootstrapDynamic"); }
    private static CallSite bootstrapDynamic(Lookup caller, String name, MethodType type) throws NoAccessException {
        MethodHandle target = linkDynamic(caller, name, type);
        System.out.println("[link] set site target to "+target);
        if (!target.type().equals(type)) {
            target = MethodHandles.convertArguments(target, type);
            System.out.println("[link]   with conversions "+target);
        }
        System.out.println("[link] site is linked; this is the last time for "+name);
        return new ConstantCallSite(target);
    }
}

/* resulting code
--------
        MH=$DAVINCI/patches/netbeans/meth/dist/meth.jar
--------
        LT=$DAVINCI/sources/langtools
--------
        cd $DAVINCI/patches/netbeans/indy-demo/
--------
        $JAVA7_HOME/bin/javac -target 7 -d build/classes -classpath $MH src/GetNameDemo.java
--------
        $JAVA7_HOME/bin/javac -d build/classes src/indify/Indify.java
--------
        $JAVA7_HOME/bin/java -cp build/classes indify.Indify --overwrite build/classes/GetNameDemo.class
--------
        $LT/dist/bin/javap -c -classpath build/classes GetNameDemo
Compiled from "GetNameDemo.java"
class GetNameDemo extends java.lang.Object {
  GetNameDemo();
    Code:
       0: aload_0       
       1: invokespecial #1                  // Method java/lang/Object."<init>":()V
       4: return        

  public static void main(java.lang.String...) throws java.lang.Throwable;
    Code:
       0: aload_0       
       1: arraylength   
       2: ifne          15
       5: iconst_1      
       6: anewarray     #2                  // class java/lang/String
       9: dup           
      10: iconst_0      
      11: ldc           #3                  // String fred.txt
      13: aastore       
      14: astore_0      
      15: new           #4                  // class java/io/File
      18: dup           
      19: aload_0       
      20: iconst_0      
      21: aaload        
      22: invokespecial #5                  // Method java/io/File."<init>":(Ljava/lang/String;)V
      25: astore_1      
      26: iconst_0      
      27: istore_2      
      28: iload_2       
      29: iconst_3      
      30: if_icmpge     130
      33: getstatic     #6                  // Field java/lang/System.out:Ljava/io/PrintStream;
      36: new           #7                  // class java/lang/StringBuilder
      39: dup           
      40: invokespecial #8                  // Method java/lang/StringBuilder."<init>":()V
      43: ldc           #9                  // String --- loop #
      45: invokevirtual #10                 // Method java/lang/StringBuilder.append:(Ljava/lang/String;)Ljava/lang/StringBuilder;
      48: iload_2       
      49: invokevirtual #11                 // Method java/lang/StringBuilder.append:(I)Ljava/lang/StringBuilder;
      52: invokevirtual #12                 // Method java/lang/StringBuilder.toString:()Ljava/lang/String;
      55: invokevirtual #13                 // Method java/io/PrintStream.println:(Ljava/lang/String;)V
      58: aload_1       
      59: invokevirtual #14                 // Method java/io/File.getName:()Ljava/lang/String;
      62: astore_3      
      63: getstatic     #6                  // Field java/lang/System.out:Ljava/io/PrintStream;
      66: aload_3       
      67: invokevirtual #13                 // Method java/io/PrintStream.println:(Ljava/lang/String;)V
      70: aload_1       
      71: invokedynamic #260,  0            // InvokeDynamic getName:(Ljava/io/File;)Ljava/lang/String; {bootstrap=invokeStatic GetNameDemo.bootstrapDynamic:(Ljava/dyn/MethodHandles$Lookup;Ljava/lang/String;Ljava/dyn/MethodType;)Ljava/dyn/CallSite;}
      76: nop           
      77: astore        4
      79: getstatic     #6                  // Field java/lang/System.out:Ljava/io/PrintStream;
      82: aload         4
      84: invokevirtual #13                 // Method java/io/PrintStream.println:(Ljava/lang/String;)V
      87: aload_3       
      88: invokedynamic #264,  0            // InvokeDynamic toString:(Ljava/lang/CharSequence;)Ljava/lang/String; {bootstrap=invokeStatic GetNameDemo.bootstrapDynamic:(Ljava/dyn/MethodHandles$Lookup;Ljava/lang/String;Ljava/dyn/MethodType;)Ljava/dyn/CallSite;}
      93: nop           
      94: astore        5
      96: getstatic     #6                  // Field java/lang/System.out:Ljava/io/PrintStream;
      99: aload         5
     101: invokevirtual #13                 // Method java/io/PrintStream.println:(Ljava/lang/String;)V
     104: aload_3       
     105: invokevirtual #20                 // Method java/lang/String.length:()I
     108: invokedynamic #268,  0            // InvokeDynamic "static:\\=java\\|lang\\|Integer:toHexString":(I)Ljava/lang/String; {bootstrap=invokeStatic GetNameDemo.bootstrapDynamic:(Ljava/dyn/MethodHandles$Lookup;Ljava/lang/String;Ljava/dyn/MethodType;)Ljava/dyn/CallSite;}
     113: nop           
     114: astore        6
     116: getstatic     #6                  // Field java/lang/System.out:Ljava/io/PrintStream;
     119: aload         6
     121: invokevirtual #13                 // Method java/io/PrintStream.println:(Ljava/lang/String;)V
     124: iinc          2, 1
     127: goto          28
     130: return        

  static {};
    Code:
       0: iconst_3      
       1: anewarray     #74                 // class java/dyn/MethodHandle
       4: putstatic     #22                 // Field INDY_CACHES:[Ljava/dyn/MethodHandle;
       7: return        
}
--------
 */
