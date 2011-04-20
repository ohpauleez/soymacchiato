#! /bin/ksh

# Config setup:
#NetBeansResources="/Applications/NetBeans/NetBeans 6.5.app/Contents/Resources"
#export JUNIT4_JAR="$NetBeansResources/NetBeans/platform9/modules/ext/junit-4.5.jar"
#export JUNIT4_JAR=$HOME/env/jars/junit-4.1.jar  #(or wherever you have it stashed)
: ${JAVA7X_HOME?'point this at a recent build of JDK7, with JSR 292 extensions added'}
: ${JUNIT4_JAR?'point this at a download of JUnit 4.1 (or 4.5, etc.)'}
: ${DAVINCI?'point this at the parent directory of the patches and sources of your mlvm download'}
export mhproj="$DAVINCI/patches/netbeans/meth"
export cpath="$mhproj/build/classes:$mhproj/build/test/classes:${JUNIT4_JAR}"
export java7=${JAVA7X_HOME}/bin/java
[ -d "$mhproj" ] || { echo "*** Not a directory: $mhproj"; exit 2; }

set -x
$java7 -XX:+EnableMethodHandles -Xbootclasspath/p:"$cpath" jdk.java.dyn.MethodHandleDemo
$java7 -XX:+EnableMethodHandles -Xbootclasspath/p:"$cpath" org.junit.runner.JUnitCore jdk.java.dyn.MethodHandlesTest

# From ant:
# ant -emacs -q -D{javac,test}.includes='**/MethodHandlesTest.java' test-single
