# SoyMacchiato - building, tweaking, and using the Java 1.7/jdk7 BSD Version on Mac OS X. 
#    Thsi should be a considered a continuation of the work done by [Landon Fuller](http://landonf.bikemonkey.org/), 
#    namely, [SoyLatte](http://landonf.bikemonkey.org/static/soylatte/).
# 
# These scripts were modified from Stephen Bannasch's 2009 scripts, his header follows:
#==========================================================
# Building the Java 1.7 BSD version of OpenJDK on Mac OS X
#
# filename: update.sh
# latest version available here: http://gist.github.com/617451
#
# Stephen Bannasch, 2009 11 29, tested on Mac OS X 10.5.8
#==========================================================
#
# This script assumes:
#   Apple's Mac OX X developer tools are installed
#   hg (mercurial) is installed
#   The forest extension to hg is installed
#   git is installed
#
# Additonal References: 
#   http://wikis.sun.com/display/mlvm
#   http://wikis.sun.com/display/mlvm/Building
#   http://hg.openjdk.java.net/mlvm/mlvm/file/tip/README.txt
#   http://bit.ly/8TKqcS
#   http://landonf.bikemonkey.org/static/soylatte/
#   http://hg.openjdk.java.net/jdk7/build/raw-file/tip/README-builds.html
#
#   http://hgbook.red-bean.com/read/advanced-uses-of-mercurial-queues.html
#
# Building:
#   source update-amd64.sh
#
# After building see if the new build can report it's version number.
#  (Output will be similar):
#    $ ./build/soymacchiato-amd64-1.0.0/bin/java -version
#    openjdk version "1.7.0-internal"
#    OpenJDK Runtime Environment (build 1.7.0-internal-soymacchiato_2011_04_19_11_33-b00)
#    OpenJDK 64-Bit Server VM (build 19.0-b05, mixed mode)

SOYLATTE=$(pwd -P)/soylatte16-i386-1.0.3/
OPENJDK17=/usr/local/java-1.7.0
MLVM=/usr/local/java-1.7.0-mlvm

echo '
*** updating bsd-port sources...
'
hg fpull -u

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
*** building bsd-port: java 1.7.0 ...
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
 ARCH_DATA_MODEL=64 \
 USER_RELEASE_SUFFIX=soymacchiato \
 LD_LIBRARY_PATH=

echo '
testing build: ./build/bsd-amd64/j2sdk-image/bin/java -version
'
./build/bsd-amd64/j2sdk-image/bin/java -version
cp -R ./build/bsd-amd64/j2sdk-image ./build/soymacchiato-amd64-1.0.0

# Save a name with the date to use when referring to this build
# when copying to /usr/local or creating a tarball later.
buildname=java-1.7.0-internal-`date "+%Y_%m_%d"`
ln -Ffs build/bsd-amd64/j2sdk-image/ $buildname
echo $buildname > .last_build
