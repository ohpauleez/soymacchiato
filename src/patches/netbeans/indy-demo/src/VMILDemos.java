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

/*
$JAVA7X_HOME/bin/javac -XDinvokedynamic VMILDemos.java
$JAVA7X_HOME/bin/java -XX:+UnlockExperimentalVMOptions -XX:+EnableInvokeDynamic -cp . VMILDemos
 */

import java.dyn.*;
import java.io.*;

public class VMILDemos {
    static MethodHandle println;
    public static void figure_3() throws Throwable {
{;;;}
MethodHandle println = MethodHandles.lookup()
    .findVirtual(PrintStream.class,
      "println", MethodType.methodType(
      void.class, String.class));
println.invokeExact(
    System.out, "hello, world");
//Figure 3. Creating and using a direct method handle.
{;;;}
VMILDemos.println = println;
    }
    public static void figure_4() throws Throwable {
{;;;}
MethodType mt = println.type();
assert mt.parameterType(0)==PrintStream.class;
assert mt.parameterType(1)==String.class;
assert mt.returnType()==void.class;
//Figure 4. Method handle type, with inserted receiver argument.
{;;;}
    }
    public static void figure_6() throws Throwable {
{;;;}
int pos = 0;  // receiver in leading position
MethodHandle println2out = MethodHandles
   .insertArguments(println, pos, System.out);
println2out.invokeExact("hello, world");
//Figure 6. Creating and using a bound method handle.
{;;;}
    }

    public static void figure_7() throws Throwable {
{;;;}
CatBox.test();
//Figure 7. Object-like method handle with a “setter” API.
{;;;}
    }
    public static void figure_8() throws Throwable {
    /*!!!
{;;;} class C {
MethodHandle greet(final String greeting) {
  return new JavaMethodHandle("run") {
      void run(String greetee) {
        String s = greeting+", "+greetee;
        System.out.println(s);
    } };
}
{;;;} }; new C().
greet("hello").invokeExact("world");
// prints "hello, world"
    */
//Figure 8. Anonymous class specifying a method handle.
{;;;}
    }
    public static void figure_9() throws Throwable {
{;;;}
Number x = 123;
int y;
if (!(x instanceof Integer))
  /*S*/ y = x.intValue(); // invokevirtual
else
  /*F*/ y = ((Integer)x).intValue();
//Figure 9. Pseudocode for inline cache logic, of invokevirtual.{;;;}
    }
    public static void figure_10() throws Throwable {
{;;;}
//*//Object x, y;  // y = x+1
//*//static final ICCallSite site = …;
//*//static final MethodHandle logic = …;
//*//if (site.target != logic)
//*//  /*L*/ y = site.target.invokeExact(x,1);
//*//else if (!(x instanceof Integer))
//*//  /*S*/ y = site.cacheAndCall(x,1);
//*//else
//*//  /*F*/ y = Runtime.add((int)x,1);
//*////Figure 10. Pseudocode for inline cache, at a dynamic call site.
{;;;}
    }

    public static void main(String... av) throws Throwable {
        figure_3();
        figure_4();
        figure_6();
        figure_7();
        figure_8();
        figure_9();
        figure_10();
    }
{;;;} static
abstract class AbstractSettableMethodHandle {
  private final MethodHandle getter, setter;
  public MethodHandle getter()
    { return getter; }
  public MethodHandle setter()
    { return setter; }
  public AbstractSettableMethodHandle(
      String get, String set) throws NoAccessException {
    MethodType getType = findGetterType(this.getClass(), get);
    MethodType setType; {{
            int nargs = getType.parameterCount();
            Class ret = getType.returnType();
            setType = getType
                .insertParameterTypes(nargs, ret)
                .changeReturnType(void.class);
        }}
    this.getter = MethodHandles.publicLookup()
       .bind(this, get, getType);
    this.setter = MethodHandles.publicLookup()
       .bind(this, set, setType);
  }
  {;;;} public MethodHandle asMethodHandle() { return getter(); }
        public MethodHandle asMethodHandle(MethodType type) { return asMethodHandle().asType(type); }
    static MethodType findGetterType(Class<?> cls, String get) {
      // Should search the methods of cls for get...
      return MethodType.methodType(int.class);
    }
}
{;;;}
    static public class CatBox extends AbstractSettableMethodHandle {
        int thing1;
        public int getThing1() { return thing1; }
        public void setThing1(int x) { thing1 = x; }
        CatBox(int x) throws NoAccessException {
            super("getThing1", "setThing1");
            thing1 = x;
        }
        static void test() throws Throwable {
            CatBox box = new CatBox(123);
            System.out.println("thing1 = "+(int)box.getter().invokeExact());
            box.setter().invokeExact(456);
            System.out.println("thing2 = "+(int)box.getter().invokeExact());
        }
    }

}