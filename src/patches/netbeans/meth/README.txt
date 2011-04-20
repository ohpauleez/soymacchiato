Standalone NetBeans project of JSR 292 RI

Plea for Help:  The project.properties file is known to work on John's macbook, but not elsewhere.  Please help tidy and port it!

To open:
  open '/Applications/NetBeans/NetBeans 6.5.app'
  Open Project => select .../davinci/patches/netbeans/meth

To build:
  cd .../davinci/patches/netbeans/meth; ant clean test

To run a unit test (requires modified JVM):
  #NetBeansResources="/Applications/NetBeans/NetBeans 6.5.app/Contents/Resources"
  #junit="$NetBeansResources/NetBeans/platform9/modules/ext/junit-4.5.jar"
  export junit=$HOME/env/jars/junit-4.1.jar  #(or wherever you have it stashed)
  export mhproj="$HOME/Projects/meth"  #(pwd -P)
  export cpath="$mhproj/build/classes:$mhproj/build/test/classes:$junit"
  export java7=$JAVA7X_HOME/bin/java
  $java7 -XX:+EnableMethodHandles -Xbootclasspath/p:"$cpath" jdk.java.dyn.MethodHandleBytecodeTest

Simpler MH demo:
  $java7 -XX:+EnableMethodHandles -Xbootclasspath/p:"$cpath" jdk.java.dyn.MethodHandleDemo

Using JUnit 4:
  $java7 -XX:+EnableMethodHandles -Xbootclasspath/p:"$cpath" org.junit.runner.JUnitCore jdk.java.dyn.MethodHandlesTest

N.B.:  For changes not yet merged with the JDK, be sure to use use -Xbootclasspath.

Current output:
-------- -------- -------- --------
JUnit version 4.1
.findStatic
OpenJDK Server VM warning: JSR 292 invokedynamic is disabled in this JVM.  Use -XX:+EnableMethodHandles to enable.
findStatic class jdk.java.dyn.MethodHandlesTest$Example.s0/()void => s0()void
calling [s0, []] on s0()void
:findStatic class jdk.java.dyn.MethodHandlesTest$Example.pkg_s0/()void => pkg_s0()void
calling [pkg_s0, []] on pkg_s0()void
:findStatic class jdk.java.dyn.MethodHandlesTest$Example.pri_s0/()void => pri_s0()void
calling [pri_s0, []] on pri_s0()void
:findStatic class jdk.java.dyn.MethodHandlesTest$Example.s1/(java.lang.Object)java.lang.Object => s1(java.lang.Object)java.lang.Object
calling [s1, [#1000000]] on s1(java.lang.Object)java.lang.Object
:findStatic class jdk.java.dyn.MethodHandlesTest$Example.s2/(int)java.lang.Object => s2(int)java.lang.Object
calling [s2, [1000001]] on s2(int)java.lang.Object
:findStatic class jdk.java.dyn.MethodHandlesTest$Example.bogus/()void => null !! java.dyn.NoAccessException: cannot access: jdk.java.dyn.MethodHandlesTest$Example.bogus()void, from jdk.java.dyn.MethodHandlesTest$Example
.findVirtual
findVirtual class jdk.java.dyn.MethodHandlesTest$Example.v0/()void => v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [v0, [Example#1000002]] on v0(jdk.java.dyn.MethodHandlesTest$Example)void
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.pkg_v0/()void => pkg_v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [pkg_v0, [Example#1000003]] on pkg_v0(jdk.java.dyn.MethodHandlesTest$Example)void
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.pri_v0/()void => pri_v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [pri_v0, [Example#1000004]] on pri_v0(jdk.java.dyn.MethodHandlesTest$Example)void
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.v1/(java.lang.Object)java.lang.Object => v1(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object)java.lang.Object
calling [v1, [Example#1000005, #1000006]] on v1(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object)java.lang.Object
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.v2/(java.lang.Object,java.lang.Object)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,java.lang.Object)java.lang.Object
calling [v2, [Example#1000007, #1000008, #1000009]] on v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,java.lang.Object)java.lang.Object
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.v2/(java.lang.Object,int)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,int)java.lang.Object
calling [v2, [Example#1000010, #1000011, 1000012]] on v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,int)java.lang.Object
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.v2/(int,java.lang.Object)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,int,java.lang.Object)java.lang.Object
calling [v2, [Example#1000013, 1000014, #1000015]] on v2(jdk.java.dyn.MethodHandlesTest$Example,int,java.lang.Object)java.lang.Object
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.v2/(int,int)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,int,int)java.lang.Object
calling [v2, [Example#1000016, 1000017, 1000018]] on v2(jdk.java.dyn.MethodHandlesTest$Example,int,int)java.lang.Object
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.bogus/()void => null !! java.dyn.NoAccessException: cannot access: jdk.java.dyn.MethodHandlesTest$Example.bogus()void, from jdk.java.dyn.MethodHandlesTest
findVirtual class jdk.java.dyn.MethodHandlesTest$SubExample.Sub/v0/()void => v0(jdk.java.dyn.MethodHandlesTest$SubExample)void
calling [Sub/v0, [SubExample#1000019]] on v0(jdk.java.dyn.MethodHandlesTest$SubExample)void
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.Sub/v0/()void => v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [Sub/v0, [SubExample#1000021]] on v0(jdk.java.dyn.MethodHandlesTest$Example)void
:findVirtual interface jdk.java.dyn.MethodHandlesTest$IntExample.Sub/v0/()void => v0(jdk.java.dyn.MethodHandlesTest$IntExample)void
calling [Sub/v0, [SubExample#1000023]] on v0(jdk.java.dyn.MethodHandlesTest$IntExample)void
:findVirtual class jdk.java.dyn.MethodHandlesTest$SubExample.Sub/pkg_v0/()void => pkg_v0(jdk.java.dyn.MethodHandlesTest$SubExample)void
calling [Sub/pkg_v0, [SubExample#1000024]] on pkg_v0(jdk.java.dyn.MethodHandlesTest$SubExample)void
:findVirtual class jdk.java.dyn.MethodHandlesTest$Example.Sub/pkg_v0/()void => pkg_v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [Sub/pkg_v0, [SubExample#1000026]] on pkg_v0(jdk.java.dyn.MethodHandlesTest$Example)void
:findVirtual interface jdk.java.dyn.MethodHandlesTest$IntExample.v0/()void => v0(jdk.java.dyn.MethodHandlesTest$IntExample)void
calling [v0, [Example#1000028]] on v0(jdk.java.dyn.MethodHandlesTest$IntExample)void
:findVirtual interface jdk.java.dyn.MethodHandlesTest$IntExample.Int/v0/()void => v0(jdk.java.dyn.MethodHandlesTest$IntExample)void
calling [Int/v0, [jdk.java.dyn.MethodHandlesTest$IntExample$Impl@29428e]] on v0(jdk.java.dyn.MethodHandlesTest$IntExample)void
:I.bind
bind Example#1000031.v0/()void => Bound[v0()void]
calling [v0, []] on Bound[v0()void]
:bind Example#1000032.pkg_v0/()void => Bound[pkg_v0()void]
calling [pkg_v0, []] on Bound[pkg_v0()void]
:bind Example#1000033.pri_v0/()void => Bound[pri_v0()void]
calling [pri_v0, []] on Bound[pri_v0()void]
:bind Example#1000034.v1/(java.lang.Object)java.lang.Object => Bound[v1(java.lang.Object)java.lang.Object]
calling [v1, [#1000035]] on Bound[v1(java.lang.Object)java.lang.Object]
:bind Example#1000036.v2/(java.lang.Object,java.lang.Object)java.lang.Object => Bound[v2(java.lang.Object,java.lang.Object)java.lang.Object]
calling [v2, [#1000037, #1000038]] on Bound[v2(java.lang.Object,java.lang.Object)java.lang.Object]
:bind Example#1000039.v2/(java.lang.Object,int)java.lang.Object => Bound[v2(java.lang.Object,int)java.lang.Object]
calling [v2, [#1000040, 1000041]] on Bound[v2(java.lang.Object,int)java.lang.Object]
:bind Example#1000042.v2/(int,java.lang.Object)java.lang.Object => Bound[v2(int,java.lang.Object)java.lang.Object]
calling [v2, [1000043, #1000044]] on Bound[v2(int,java.lang.Object)java.lang.Object]
:bind Example#1000045.v2/(int,int)java.lang.Object => Bound[v2(int,int)java.lang.Object]
calling [v2, [1000046, 1000047]] on Bound[v2(int,int)java.lang.Object]
:bind Example#1000048.bogus/()void => null !! java.dyn.NoAccessException: cannot access: jdk.java.dyn.MethodHandlesTest$Example.bogus()void, from jdk.java.dyn.MethodHandlesTest
bind SubExample#1000049.Sub/v0/()void => Bound[v0()void]
calling [Sub/v0, []] on Bound[v0()void]
:bind SubExample#1000050.Sub/pkg_v0/()void => Bound[pkg_v0()void]
calling [Sub/pkg_v0, []] on Bound[pkg_v0()void]
:bind jdk.java.dyn.MethodHandlesTest$IntExample$Impl@b1b4c3.Int/v0/()void => Bound[v0()void]
calling [Int/v0, []] on Bound[v0()void]
:.unreflect
unreflect class jdk.java.dyn.MethodHandlesTest$Example.s0/()void => s0()void
calling [s0, []] on s0()void
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.pkg_s0/()void => pkg_s0()void
calling [pkg_s0, []] on pkg_s0()void
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.pri_s0/()void => pri_s0()void
calling [pri_s0, []] on pri_s0()void
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.s1/(java.lang.Object)java.lang.Object => s1(java.lang.Object)java.lang.Object
calling [s1, [#1000052]] on s1(java.lang.Object)java.lang.Object
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.s2/(int)java.lang.Object => s2(int)java.lang.Object
calling [s2, [1000053]] on s2(int)java.lang.Object
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.v0/()void => v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [v0, [Example#1000054]] on v0(jdk.java.dyn.MethodHandlesTest$Example)void
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.pkg_v0/()void => pkg_v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [pkg_v0, [Example#1000055]] on pkg_v0(jdk.java.dyn.MethodHandlesTest$Example)void
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.pri_v0/()void => pri_v0(jdk.java.dyn.MethodHandlesTest$Example)void
calling [pri_v0, [Example#1000056]] on pri_v0(jdk.java.dyn.MethodHandlesTest$Example)void
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.v1/(java.lang.Object)java.lang.Object => v1(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object)java.lang.Object
calling [v1, [Example#1000057, #1000058]] on v1(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object)java.lang.Object
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.v2/(java.lang.Object,java.lang.Object)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,java.lang.Object)java.lang.Object
calling [v2, [Example#1000059, #1000060, #1000061]] on v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,java.lang.Object)java.lang.Object
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.v2/(java.lang.Object,int)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,int)java.lang.Object
calling [v2, [Example#1000062, #1000063, 1000064]] on v2(jdk.java.dyn.MethodHandlesTest$Example,java.lang.Object,int)java.lang.Object
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.v2/(int,java.lang.Object)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,int,java.lang.Object)java.lang.Object
calling [v2, [Example#1000065, 1000066, #1000067]] on v2(jdk.java.dyn.MethodHandlesTest$Example,int,java.lang.Object)java.lang.Object
:unreflect class jdk.java.dyn.MethodHandlesTest$Example.v2/(int,int)java.lang.Object => v2(jdk.java.dyn.MethodHandlesTest$Example,int,int)java.lang.Object
calling [v2, [Example#1000068, 1000069, 1000070]] on v2(jdk.java.dyn.MethodHandlesTest$Example,int,int)java.lang.Object
:IIIII.convertArguments/pairwise
convert id(java.lang.Object)java.lang.Object to (java.lang.String)java.lang.Object => Adapted[id](java.lang.String)java.lang.Object
calling [id(java.lang.Object)java.lang.Object, [#1000071]] on Adapted[id](java.lang.String)java.lang.Object
:convert id(java.lang.Object)java.lang.Object to (java.lang.Integer)java.lang.Object => Adapted[id](java.lang.Integer)java.lang.Object
calling [id(java.lang.Object)java.lang.Object, [1000072]] on Adapted[id](java.lang.Integer)java.lang.Object
:convert id(java.lang.Object)java.lang.Object to (int)java.lang.Object => Bound[<unknown>(int)java.lang.Object]
calling [id(java.lang.Object)java.lang.Object, [1000073]] on Bound[<unknown>(int)java.lang.Object]
:convert id(java.lang.Object)java.lang.Object to (short)java.lang.Object => Bound[<unknown>(short)java.lang.Object]
calling [id(java.lang.Object)java.lang.Object, [17034]] on Bound[<unknown>(short)java.lang.Object]
:IIIIIIII
Time: 0.753

OK (5 tests)

-------- -------- -------- --------
