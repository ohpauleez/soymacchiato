# SoyMacchiato - building, tweaking, and using the Java 1.7/jdk7 BSD Version on Mac OS X. 
#    Thsi should be a considered a continuation of the work done by [Landon Fuller](http://landonf.bikemonkey.org/), 
#    namely, [SoyLatte](http://landonf.bikemonkey.org/static/soylatte/).
# 
# These scripts were modified from Stephen Bannasch's 2009 scripts, his header follows:
#==========================================================
# Building the Java 1.7 Da Vinci Machine (mlvm) on Mac OS X
#
# filename: update.sh
# latest version available here: http://gist.github.com/243072
#
# Stephen Bannasch
#==========================================================
#
# This script assumes:
#   Apple's Mac OX X developer tools are installed
#   hg (mercurial) is installed
#   The forest extension to hg is installed
#
# Additional References: 
#   http://wikis.sun.com/display/mlvm
#   http://wikis.sun.com/display/mlvm/Building
#   http://hg.openjdk.java.net/mlvm/mlvm/file/tip/README.txt
#   http://bit.ly/8TKqcS
#   http://landonf.bikemonkey.org/static/soylatte/
#   http://hg.openjdk.java.net/jdk7/build/raw-file/tip/README-builds.html
#   http://sourceforge.net/projects/jibx/files/
#
#   http://hgbook.red-bean.com/read/advanced-uses-of-mercurial-queues.html
#
# Building:
#   source update-mlvm.sh
#
# After building see if the new build can report it's version number:
#   $ ./build/bsd-amd64/j2sdk-image/bin/java -version
#  (Output will be similar):
#   openjdk version "1.7.0-internal-fastdebug"
#   OpenJDK Runtime Environment (build 1.7.0-internal-fastdebug-stephen_2010_08_18_15_10-b00)
#   OpenJDK 64-Bit Server VM (build 19.0-b03-fastdebug, mixed mode)

SOYLATTE=$(pwd -P)/soylatte16-i386-1.0.3

echo '
*** popping previously applied patches to bsdport sources: hotspot, jdk, and langtools ...'

echo '
[hotspot] '
cd hotspot; hg qpop -a; cd -

echo '
[jdk] '
cd jdk; hg qpop -a; cd -

echo '
[langtools] '
cd langtools; hg qpop -a; cd -

echo '
*** updating bsdport sources...
'
hg fpull -u

echo '
*** updating mlvm patchsets ...
'
cd patches
hg fpull -u
cd ..

echo '
*** integrating mlvm patchsets into bsdport sources ...

*** creating symbolic links to the patch directories from the corresponding .hg directories of the full source
'
bash patches/make/link-patch-dirs.sh . patches
ls -il patches/hotspot/series hotspot/.hg/patches/series
export davinci=$(pwd) guards="buildable testable"
# export davinci=$(pwd) guards="buildable testable coro"

echo '
*** running: hg qselect --reapply $guards
'
sh patches/make/each-patch-repo.sh "hg qselect --reapply $guards" '$(sh $davinci/patches/make/current-release.sh)'

echo '
*** running: hg qselect --pop $guards
'
sh patches/make/each-patch-repo.sh "hg qselect --pop $guards" '$(sh $davinci/patches/make/current-release.sh)'

echo '
*** running: hg qselect; hg qunapplied
'
sh patches/make/each-patch-repo.sh "hg qselect; hg qunapplied"

echo '
*** running: hg update -r
'
sh patches/make/each-patch-repo.sh "hg update -r" '$(sh $davinci/patches/make/current-release.sh)'

echo '
*** running: hg qpush -a
'
sh patches/make/each-patch-repo.sh "hg qpush -a"

echo '
*** cleaning previous build products ...
'

mv build build.del; rm -rf build.del &

# Remove file with versioned name of last build and symbolic link to last build.
# Used when referring to a build when copying to /usr/local or creating a tarball.
if [ -e .last_build ]
	then
		echo '
removing references to last build
'
		buildname=`cat .last_build`
		rm -f .last_build
		rm -f $buildname
fi

echo '
*** removing *.gch files ...
'
# Incremental JVM rebuilds have trouble with *.gch files.
# The *.gch file does not get regenerated unless you remove it,
# even if 20 header files have changed.
# This is not a problem for batch builds, of course.
if [ -d build ]; then
  $BUILD_HOTSPOT && {
    ${KEEP_HOTSPOT_HEADERS:-false} ||
    rm -f $(find build -name _precompiled.incl.gch)
  }
fi

# If COMMAND_MODE=legacy then the archive command: 'ar' will not create or update
# catalog entries when creating an archive resulting in errors like this:
#
#   libfdlibm.i586.a, archive has no table of contents
#
# This only appears to be a problem when using iTerm as the terminal program.
# Using the Terminal.app program that comes with Mac OS X sets COMMAND_MODE=unix2003.
# See: http://old.nabble.com/Why-ar-doesn't-call-ranlib-on-Mac--td22319721.html
if [ "$COMMAND_MODE" = "legacy" ]
then
  echo '
*** changing COMMAND_MODE from legacy to unix2003
'
  export COMMAND_MODE=unix2003
fi

echo '
*** running make ...
'
unset CLASSPATH
unset JAVA_HOME
unset LD_LIBRARY_PATH

env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin \
 LANG=C \
 CC=gcc \
 CXX=g++ \
 make \
 ALT_BOOTDIR=$SOYLATTE \
 ALT_JDK_IMPORT_PATH=$SOYLATTE \
 ALT_FREETYPE_HEADERS_PATH=/usr/X11R6/include \
 ALT_FREETYPE_LIB_PATH=/usr/X11R6/lib \
 ALT_CUPS_HEADERS_PATH=/usr/include \
 ALLOW_DOWNLOADS=true \
 ANT_HOME=/usr/share/ant \
 NO_DOCS=true \
 HOTSPOT_BUILD_JOBS=2 \
 BUILD_LANGTOOLS=true \
 BUILD_JAXP=false \
 BUILD_JAXWS=false \
 BUILD_CORBA=false \
 BUILD_HOTSPOT=true \
 BUILD_JDK=true \
 ARCH_DATA_MODEL=64 \
 USER_RELEASE_SUFFIX=soymacchiato-mlvm \
 LD_LIBRARY_PATH=


echo '
testing build: ./build/bsd-amd64/j2sdk-image/bin/java -version
'
./build/bsd-amd64/j2sdk-image/bin/java -version
cp -R ./build/bsd-amd64/j2sdk-image ./build/soymacchiato-mlvm-amd64-1.0.0

#echo '
#running jdk/test/java/lang/invoke tests:
#
#jtreg -XX:+UnlockExperimentalVMOptions -XX:+EnableInvokeDynamic -jdk:build/bsd-amd64/j2sdk-image  -v:summary jdk/test/java/lang/invoke/
#
#'
#jtreg -XX:+UnlockExperimentalVMOptions -XX:+EnableInvokeDynamic -jdk:build/bsd-amd64/j2sdk-image  -v:summary jdk/test/java/lang/invoke/

# Save a name with the date to use when referring to this build
# when copying to /usr/local or creating a tarball later.
buildname=java-1.7.0-internal-mlvm-`date "+%Y_%m_%d"`
ln -Ffs build/bsd-amd64/j2sdk-image/ $buildname
echo $buildname > .last_build
