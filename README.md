# Backup-Stackable-File-System
******************************************************************************
* INTRODUCTION:

In a stackable file system, each VFS-based object in the stackable file
system (e.g., in Wrapfs) has a link to one other object in the lower file
system (sometimes called the "hidden" object).  We identify this
symbolically as X->X' where "X" is an object in the upper layer, and X' is
an object in the lower layer.  This form of stacking is a single-layer
linear stacking.

******************************************************************************
* DETAILS (KERNEL CODE):

When you modify a file in traditional Unix file systems, there's no way to
undo or recover a previous version of that file.  There's also no way to get
older versions of the same file.  Being able to access a history of versions
of file changes is very useful, to recover lost or inadvertent changes.
Your task would be to create a file system that automatically creates backup
versions of files, and also allows you to view versions as well as recover
them.  This Backup File System (bkpfs) will have to handle several tasks via
policies that you will have to design, justify your design, implement, and
test: backup, version access, and retention policies.

----------------------------------------------------------------------
1. When to backup?

Here are some choices for you to consider: you can decide to create a backup
when a file is opened for writing, or when it is actually written to.  An
alternative could be to create a backup every N writes to a file, or after B
bytes are modified in a file; or any combination of these and other policies
as you see fit to justify.

----------------------------------------------------------------------
2. What to backup?

For simplicity, back up only regular files.

----------------------------------------------------------------------
3. Where to store backups?

For a given file F, you can imagine having N older versions of the file:
F.1, F.2, F.3, ..., F.N (you can decide whether F.1 is the oldest or latest
backup).  You can come up with any naming scheme for the older file backups:
different policies will make it easier/harder to achieve certain
functionalities.  Examples include, for each file F, the i-th backup can be
called:

(3a) .F.i (aka hidden "dot" files)
(3b) .i.F
(3c) .bkp.F/i (i.e., a hidden per-file dir containing the N backups)
(3d) other naming scheme you deem suitable (explain why)

----------------------------------------------------------------------
4. How to backup?

To create a backup of file X and name it Y, use similar code from the first
homework assignment to copy files in the kernel.  To increase efficiency,
look into "splice" methods to make rapid data copies inside the kernel.

----------------------------------------------------------------------
5. Visibility policy

Backup versions of files should not be visible or accessible by default.  It
would confuse users if /bin/ls shows a lot of "extra" files.  And it defeats
the purpose of being able to control those versions (see below) if a user
can easily manipulate the versions or even delete them too easily.

Therefore you'll have to consider how to change one or more ops like
->lookup, ->readdir, ->filldir, so that users can't just view version files,
and can't easily delete/change them.  You should carefully consider bkpfs's
interaction with the dcache/icache here.

----------------------------------------------------------------------
6. Retention policy

A file should have a maximum of N backups available.  That means if you need
to create another backup, say N+1, you'd have to delete one of the existing
backup files to make room for the new one.  You should keep the last N most
recent backups, and delete the oldest one(s).  You should figure out how to
keep N backups around, what their names should be, and how/if you need to
rename some backup files when you delete the Nth backup to make room

----------------------------------------------------------------------
7. Version management

You need to support several functions:

(7a) List all versions of a file.  This would be a special flavor of
"readdir".  Depending on how/where you write your version files, this can be
easier or harder.  This may be achieved with some modified variant of
->readdir/->filldir, or an ->ioctl.

(7b) Delete newest, oldest, or all versions

Support a way to delete the newest, oldest, or all versions.  This can be
achieved using special ->ioctls, or perhaps some variant of ->unlink or
->rmdir.

(7c) View file version V, newest, or oldest

Support a mechanism to view the contents of any of the version files, in
read-only mode (disallow any changes to the version files).  This is useful
for users to inspect an older version of a file to see if they want to, say,
recover it.

(7d) Restore newest version, oldest, or any version V

Support a way to restore (recover) the latest or oldest version file
created, or any specific one.  Restoring may overwrite the main file F, or
maybe create a special file F' that can be inspected, deleted, or copied
over the main file F.


******************************************************************************
* USER CODE AND TESTING:

You will need to write some user level code to issue ioctls or other
commands to bkpfs: to view versions, recover, etc.  Usage of the program is:

$ ./bkpctl -[ld:v:r:] FILE

FILE: the file's name to operate on
-l: option to "list versions"
-d ARG: option to "delete" versions; ARG can be "newest", "oldest", or "all"
-v ARG: option to "view" contents of versions (ARG: "newest", "oldest", or N)
-r ARG: option to "restore" file (ARG: "newest" or N)
	(where N is a number such as 1, 2, 3, ...)

You will be required to write a series of small shell scripts to exercise
EACH feature of your bkpfs, demonstrating successful as well as error
conditions, as well as any corner cases.  I would expect to see at least
scripts to show how versions are created/retained when files are modified;
scripts to exercise every flag, argument, and combinations to the bkpctl
program; and scripts to demonstrate features such as version file
[in]visibility.  Put all test scripts inside the CSE-506 subdir of your git
repository.


******************************************************************************
