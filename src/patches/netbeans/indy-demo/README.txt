To build this project, you will need an Da Vinci Machine Project build.
This includes a JSR 292 aware javac from langtools, a libjvm from hotspot, and an rt.jar from jvm.

To run this project, you will need the JVM options -XX:+UnlockExperimentalVMOptions
and -XX:+EnableInvokeDynamic, and a JSR 292 capable JVM that understands them.
