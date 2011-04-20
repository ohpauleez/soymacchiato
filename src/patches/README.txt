OpenJDK Multi-Language VM: The Da Vinci Machine Project

This is a Mercurial patch repository.

  See http://openjdk.java.net/ for more information about the OpenJDK.

  See http://openjdk.java.net/projects/mlvm/ for more information
  about the Da Vinci Machine Project.

  See also the wiki at http://wikis.sun.com/display/mlvm/Home .

== Organization

The OpenJDK mlvm/mlvm repository forest is (at present) only a set of patches, not a full set of JDK sources.  The patches apply to some version of the full OpenJDK forest structure.  The structure of the mlvm forest parallels the structure of the full jdk7 sources, but each repository in mvlm is only the .hg/patches directory (of the Mercurial mq extension).  Thus, repositories under mlvm are called "patch repositories" and those under jdk7 are by contrast called "source repositories".

Commits in the mlvm repositories do not update the full source trees, only the patches.  To make this clear, when a commit occurs in a patch repository, we will refer to it specifically as a "patch commit".

=== Patches

Make sure you have enabled the Mercurial mq extension with the following lines in your ".hgrc" file:
	[extensions] 
	mq =

All patch files must end with the suffix ".patch".

Patches must not have patch headers, since they are easy to lose if patches are regenerated.

All patches must be in "git" format, without file dates.  To ensure this, add the following lines to your ".hgrc" file:
	[diff]
	nodates=1
	git=1

A patch file may be accompanied by a similar file with the suffix ".txt".  This file will contain brief comments about the patch, including:
* references to project documentation, if needed
* draft commit comments
* dependencies on other patches

Patch repositories may also contain scripts and documentation.  All these non-patch files are ignored by the Mercurial patch queue, since they do not appear in the series file.

Patches may be split.  Files that contribute to a split patch must all have the same prefix up to the first dot.

Patches may depend on each other.  (For example, invoke dynamic may depend on method handles, which in turn may depend on anonymous classes.)  Such dependencies should be clearly stated.

The patch sequence is ordered by both stability and by dependency.  If patch B is less stable than patch A, then B should come later in the series.  If B depends on A, it also must come later in the series, and A must not be less stable than B.

Patches of the same name in multiple patch repositories (hotspot and jdk) are to be applied simultaneously to parallel source repositories.  Their series file elements must be kept exactly in sync.  The documenting text file for a patch does not need to be on both sides, and should not be duplicated.

=== Patch Guards

The series file contains guard annotations for each patch.  Patches are guarded with Mercurial revision hashes that they are known apply to.  (They may additionally be guarded with OpenJDK release tags or other tags.)  In this way, as the OpenJDK repositories advance, patches can be rebased independently from each other.

Each patch must have one or more positive guards.  At least one guard must be a twelve-digit hexadecimal mercurial revision hash, such as "7836be3e92d0".  If a patch is guarded by such a revision, it is guaranteed to apply, without rejects, to that particular OpenJDK build.  It will also build successfully (unless it is marked non-buildable, via a #-buildable guard).

Other tags may be present, especially tags of OpenJDK builds, such as "jdk7-b25".  As it happens, "7836be3e92d0" and "jdk7-b25" refer to the same revision in the hotspot repository.  These symbolic tags are informational and approximate, and (being less accurate than hashes) do not guarantee clean application of patches.

Each patch must have a negative guard which names that patch with a "slash" prefix.  This allows developers to control individual entries in the patch queue without editing the series file.  Editing the series file is risky, since it is under version control and shared by all developers.  By contrast, the guards file is not under version control.

The following guards are also significant as negative guards on patches which do not yet have the relevant quality level:
* #-buildable: the patch does not build, or iterferes with the operation of the JVM
* #-testable: the patch fails to have a working test suite

For normal development, the guards 'buildable' and 'testable' should be present in the guard file, as well as the OpenJDK release in use.

Example series file contents:
	# base = 05f8c84c5daa in http://hg.openjdk.java.net/bsd-port/bsd-port/hotspot
	anonk.patch #-/anonk #+05f8c84c5daa
	meth.patch  #-/meth  #+05f8c84c5daa

The 'qselect' command can be used to control these patches:
	hg qselect 05f8c84c5daa  # select both patches
	hg qselect 05f8c84c5daa /meth  # select anonk but not meth

The first line of each series file must contain a twelve-digit hexadecimal number, which declares the base Mercurial revision for the patch series as a whole.  The script patches/make/current-release.sh will scan this revision automatically.  By convention, the first line of the series file can be a handy human-readable description of that base.  If it were missing, the hexadecimal number would be extracted from the tag on the first patch in the series.

When any patch is rebased, the commit which updates the patch must also update patch's guard in the series file also.  This should be done a special "rebasing patch commit" which is distinct from actual development.  Actual development occur in patch commits that are *not* rebasing.

References:
* more on guards: http://hgbook.red-bean.com/hgbookch13.html
* patch rebasing procedures: http://www.selenic.com/mercurial/wiki/index.cgi/MqMerge
* a good summary on rebasing: http://www.selenic.com/pipermail/mercurial/2008-February/017367.html

=== Multi-based Patches

(This technique is untested; maybe we don't need it.)

If a patch must be split so as to apply to several OpenJDK builds, the latest patch in the series must be a complete patch for the most recent build, and for each previous build, a temporary patches must simply track the relevant changes up to the most recent build, so as to make the newest patch apply correctly in all cases.

For example, to support builds 25 and 28 behind build 30 two fixup patches are needed:
	anonk.jdk7-b28.patch #-/anonk #+7836be3e92d0 #+jdk7-b25
	anonk.jdk7-b30.patch #-/anonk #+c14dab40ed9b #+jdk7-b28
	anonk.patch          #-/anonk #+d1605aabd0a1 #+jdk7-b30

If you have build jdk7-b28, you must set the guards for all three revisions, so that all three patches are enabled.

Normally this will not be necessary, unless the patch provides functionality which several other patches depend on, and those patches are in different stages of rebasing.

== Setting Up Your Workspace

Make a directory which will contain both sets of repositories (patches and full sources), and pull everything there.  Then create symbolic links to the patch directories from the corresponding ".hg" directories of the full sources.

	$ mkdir davinci
	$ cd davinci
        $ hg fclone http://hg.openjdk.java.net/bsd-port/bsd-port sources
	$ hg fclone http://hg.openjdk.java.net/mlvm/mlvm patches
	$ (cd patches/make; gnumake setup)  # will probably fail
	$ (cd patches/make; gnumake force)  # force "hg update" to fix
	$ (cd patches/make; gnumake)
	$ (cd patches/make; gnumake FORCE_VERSIONS=1) # include the force by default

The "gnumake setup" command is likely to fail, if the source version of each sub-repository is not exactly the same as the version advertised in the first line of its series file.  If it fails, the "force" target cleans up by running "hg update" (a.k.a. "hg checkout") to force the repository back to the required revision.  See the section above on "Patch Guards" to determine the required base revision.

Instead of using the makefile, the following shell commands will work as well:
	$ bash patches/make/link-patch-dirs.sh sources patches
	+ ln -s ../../../patches/hotspot sources/hotspot/.hg/patches
	+ ln -s ../../../patches/jdk sources/jdk/.hg/patches
	$ ls -il patches/hotspot/series sources/hotspot/.hg/patches/series
	(should be identical files)
	$ export davinci=$(pwd) guards="buildable testable"
	$ sh patches/make/each-patch-repo.sh "hg qselect --pop $guards" \
		'$(sh $davinci/patches/make/current-release.sh)'
	$ sh patches/make/each-patch-repo.sh "hg qselect; hg qunapplied"
	$ sh patches/make/each-patch-repo.sh "hg update -r" \
		'$(sh $davinci/patches/make/current-release.sh)'
	$ sh patches/make/each-patch-repo.sh hg qpush -a

The last command may produce output about patches being skipped.  This is correct, because the setting of $guards will ensure that only buildable and testable patches will be applied.  It may also produce messages about the patches being already applied, in which case a non-zero exit status is normal.

If you have not forced the repository to the expected base revision (via "hg update"), the last command may also produce output about failed patch application.  Such failures must be fixed in an ad hoc manner, as you merge the upstream changes with the patch.

If you have forced the repository, the last command may produce a warning about "working directory not at tip".  This is normal.  It reminds you that you are working with an older version of the software.

(To apply other sets of patches, adjust the guard settings and/or use qpush to refer to specific desired patches.  Do not make permanent edits to the series file, unless they reflect true changes of development status.)

=== Updating Your Workspace

If you wish to refresh your source code from the mlvm project, you will have to take the following steps, in order:
* properly dispose of any private changes you may have made
* pop the old mq patches
* update your sources from the bsd-port
* pull new mq patches
* adjust 'qselect' guards to match the new required source revisions
* adjust source revisions (if needed) to match the new patches
* push the new patches

Example command sequence:
        $ sh patches/make/each-patch-repo.sh  hg diff  # display sources
        $ sh patches/make/each-patch-repo.sh  hg qpop -a  # remove patches
        $ sh patches/make/each-patch-repo.sh  hg pull -u  # pull new sources
        $ sh patches/make/each-patch-repo.sh  hg pull -u -R .hg/patches  # pull new patches
        # from this point, the guard and patch commands are the same as with a fresh repo
	$ export davinci=$(pwd) guards="buildable testable"
	$ sh patches/make/each-patch-repo.sh "hg qselect --pop $guards" \
		'$(sh $davinci/patches/make/current-release.sh)'  # set new guards
	$ sh patches/make/each-patch-repo.sh "hg qselect; hg qunapplied"
	$ sh patches/make/each-patch-repo.sh "hg update -r" \
		'$(sh $davinci/patches/make/current-release.sh)'  # ensure correct source version
       $ sh patches/make/each-patch-repo.sh  hg qpush -a  # push new patches

== Trouble Shooting

To verify that the current release is properly checked out, you can use Mercurial commands like these in the source directory:
	cd davinci/sources/hotspot
	hg qpop -a  # following commands assume no patches active
	hg parent  # output should include the 12-digit hash of the working version
	R_CUR=$(hg parent --template '{node|short}\n')
	head -1 < .hg/patches/series # header comment describes main base revision
	R_REQ=$(head -1 < .hg/patches/series | awk '{print $4}')  # should be main base revision
	[ $R_REQ = $R_CUR ] || echo "*** WRONG PATCH BASE"

The 'checkout' command can be used to reset a clean source repository to the base revision required for patches:
	hg qpop -a  # following commands assume no patches active
	hg checkout $R_REQ
	R_CUR=$(hg parent --template '{node|short}\n')
	[ $R_REQ = $R_CUR ] || echo "*** CHECKOUT FAILED"
	hg qselect buildable testable $R_CUR
	hg qpush -a
	R_QPAR=$(hg log -r qparent --template '{node|short}\n')
	[ $R_REQ = $R_QPAR ] || echo "*** QUEUE PUSH FAILED"

