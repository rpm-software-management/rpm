#!/usr/bin/python

import rpm

ps = rpm.ps()
ps.Debug(-1)

#   RPMPROB_BADARCH,    /*!< package ... is for a different architecture */
#   RPMPROB_BADOS,      /*!< package ... is for a different operating system */
#   RPMPROB_PKG_INSTALLED, /*!< package ... is already installed */
#   RPMPROB_BADRELOCATE,/*!< path ... is not relocatable for package ... */
#   RPMPROB_REQUIRES,   /*!< package ... has unsatisfied Requires: ... */
#   RPMPROB_CONFLICT,   /*!< package ... has unsatisfied Conflicts: ... */
#   RPMPROB_NEW_FILE_CONFLICT, /*!< file ... conflicts between attemped installs of ... */
#   RPMPROB_FILE_CONFLICT,/*!< file ... from install of ... conflicts with file from package ... */
#   RPMPROB_OLDPACKAGE, /*!< package ... (which is newer than ...) is already installed */
#   RPMPROB_DISKSPACE,  /*!< installing package ... needs ... on the ... filesystem */
#   RPMPROB_DISKNODES,  /*!< installing package ... needs ... on the ... filesystem */
#   RPMPROB_BADPRETRANS /*!< (unimplemented) */

print "======== setup"
ps[ 0] = ('pkgNEVR-1.2-3', '',		None, 0, 0, 'GoodArch', 0)
ps[ 1] = ('pkgNEVR-1.2-3', '',		None, 1, 0, 'GoodOs', 0)
ps[ 2] = ('pkgNEVR-1.2-3', '',		None, 2, 0, '', 0)
ps[ 3] = ('pkgNEVR-1.2-3', '',		None, 3, 0, '/path/no/relocate', 0)
ps[ 4] = ('pkgNEVR-1.2-3', 'R altNEVR-7.8-9', None, 4, 0, '', 0)
ps[ 5] = ('pkgNEVR-1.2-3', 'C altNEVR-7.8-9', None, 5, 0, '', 0)

ps[ 6] = ('pkgNEVR-1.2-3', 'altNEVR-7.8-9', None, 6, 0, '/new/file/conflict', 0)
ps[ 7] = ('pkgNEVR-1.2-3', 'altNEVR-7.8-9', None, 7, 0, '/file/conflict', 0)

ps[ 8] = ('older-1.2-3', 'newer-7.8-9',	None, 8, 0, '', 0)

ps[ 9] = ('pkgNEVR-1.2-3', '',		None, 9, 0, '/no/blocks', 12345)
ps[10] = ('pkgNEVR-1.2-3', '',		None,10, 0, '/no/inodes', 98765)

ps[11] = ('pkgNEVR-1.2-3', 'X altNEVR-7.8-9', None, 11, 0, 'badpretrans', 1)
ps[12] = ('pkgNEVR-1.2-3', 'X altNEVR-7.8-9', None, 12, 0, 'some string', 1)

print "======== print ps"
print ps

print "======== print ps[4]"
print ps[4]

print "======== del ps[4]"
del ps[4]

print "======== for s in ps:"
for s in ps:
	print s
