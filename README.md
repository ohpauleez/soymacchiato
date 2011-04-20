SoyMacchiato
============
This project is for building, tweaking, and using jdk7 on OS X.  
It should be a considered a continuation of the work done by [Landon Fuller](http://landonf.bikemonkey.org/), 
namely, [SoyLatte](http://landonf.bikemonkey.org/static/soylatte/).


Downloading
-----------
The SoyMacchiato binaries can also be downloaded from the following sources:  

*  64-bit JDK for Mac OS X 10.5 (Latest Build): [Latest Build](http://www.pauldee.org/soymacchiato/soymacchiato-amd64-latest.tar.bz2)
*  64-bit JDK for Mac OS X 10.5: [soymacchiato-amd64-1.0.0](http://www.pauldee.org/soymacchiato/soymacchiato-amd64-1.0.0.tar.bz2)
*  64-bit MLVM(DaVinci,) JDK for Mac OS X: [soymacchiato-mlvm-amd64-1.0.0](http://www.pauldee.org/soymacchiato/soymacchiato-mlvm-amd64-1.0.0.tar.bz2) - Most of this work (InvokeDynamic) was later rolled into the JDK (JSR-292)


Building
--------
Building depends on the following tools, libraries, and plugins:  

*  Apple's Mac OX X developer tools
*  hg (mercurial)
*  The forest extension to hg (for the DaVinci builds, you'll also need the mq module enabled)

### Standard 64-bit JDK (10.5 and below)

`cd src`  
`source update-amd64.sh`  
*Optionally*, you can build an i586 version using: `source update-i586.sh`

### Standard 64-bit JDK (10.6 and Up)

TODO - There is an osx-port of the JDK, which builds a standard Java Framework drop/bundle, OSX Style.

### DaVinci Machine enhanced 64-bit JDK

`cd src`  
`source update-mlvm.sh`  


Installation and Usage
----------------------

1. Untar your desired build, ex: `tar xvjf soymacchiato-amd64-1.0.0.tar.bz2`
2. Place the directory somewhere on your file system, ex: `mv soymacchiato-amd64-1.0.0 /usr/local/soymacchiato17-amd64`  
 * *Optionally*, symlink this dir, ex: `ln -s /usr/local/soymacciato17-amd64 /usr/local/soymacchiato`
3. Update your `JAVA_HOME` environment variable in your shell config, then `source` the config.
4. Update your java related links, ex: `sudo rm /usr/bin/java; sudo ln /usr/local/soymacchiato/bin/java /usr/bin/java`
 * *Optionally*, you can just put your new java7 directory on your path, ex: `export PATH=/usr/local/soymacchiato17-amd64/bin:$PATH`


Contributing
------------
To contribute builds, patches, or resources please file an issue, or fork and send a pull request.


License Information
-------------------
    Both OpenJDK and SoyMacchiato are released under the GNU Public License, Version 2.
    See LICENSE for more information.

