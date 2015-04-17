#!/usr/bin/python

import sys
import rpm
from rpm._rpm import ts as TransactionSetCore

if sys.version_info[0] == 3:
    _string_types = str,
else:
    _string_types = basestring,


# TODO: migrate relevant documentation from C-side
class TransactionSet(TransactionSetCore):
    _probFilter = 0

    def _wrapSetGet(self, attr, val):
        oval = getattr(self, attr)
        setattr(self, attr, val)
        return oval

    def setVSFlags(self, flags):
        return self._wrapSetGet('_vsflags', flags)

    def getVSFlags(self):
        return self._vsflags

    def setColor(self, color):
        return self._wrapSetGet('_color', color)

    def setPrefColor(self, color):
        return self._wrapSetGet('_prefcolor', color)

    def setFlags(self, flags):
        return self._wrapSetGet('_flags', flags)

    def setProbFilter(self, ignoreSet):
        return self._wrapSetGet('_probFilter', ignoreSet)

    def parseSpec(self, specfile):
        import rpm._rpmb
        return rpm._rpmb.spec(specfile)

    def getKeys(self):
        keys = []
        for te in self:
            keys.append(te.Key())
        # Backwards compatibility goo - WTH does this return a *tuple* ?!
        if not keys:
            return None
        else:
            return tuple(keys)

    def _f2hdr(self, item):
        if isinstance(item, _string_types):
            f = open(item)
            header = self.hdrFromFdno(f)
            f.close()
        elif isinstance(item, rpm.hdr):
            header = item
        else:
            header = self.hdrFromFdno(item)
        return header

    def addInstall(self, item, key, how="u"):
        header = self._f2hdr(item)

        if how not in ['u', 'i']:
            raise ValueError('how argument must be "u" or "i"')
        upgrade = (how == "u")

        if not TransactionSetCore.addInstall(self, header, key, upgrade):
            raise rpm.error("adding package to transaction failed")

    def addReinstall(self, item, key):
        header = self._f2hdr(item)

        if not TransactionSetCore.addReinstall(self, header, key):
            raise rpm.error("adding package to transaction failed")

    def addErase(self, item):
        hdrs = []
        if isinstance(item, rpm.hdr):
            hdrs = [item]
        elif isinstance(item, rpm.mi):
            hdrs = item
        elif isinstance(item, int):
            hdrs = self.dbMatch(rpm.RPMDBI_PACKAGES, item)
        elif isinstance(item, _string_types):
            hdrs = self.dbMatch(rpm.RPMDBI_LABEL, item)
        else:
            raise TypeError("invalid type %s" % type(item))

        for h in hdrs:
            if not TransactionSetCore.addErase(self, h):
                raise rpm.error("package not installed")

        # garbage collection should take care but just in case...
        if isinstance(hdrs, rpm.mi):
            del hdrs

    def run(self, callback, data):
        rc = TransactionSetCore.run(self, callback, data, self._probFilter)

        # crazy backwards compatibility goo: None for ok, list of problems
        # if transaction didn't complete and empty list if it completed
        # with errors
        if rc == 0:
            return None

        res = []
        if rc > 0:
            for prob in self.problems():
                item = ("%s" % prob, (prob.type, prob._str, prob._num))
                res.append(item)
        return res

    def check(self, *args, **kwds):
        TransactionSetCore.check(self, *args, **kwds)

        # compatibility: munge problem strings into dependency tuples of doom
        res = []
        for p in self.problems():
            # is it anything we need to care about?
            if p.type == rpm.RPMPROB_CONFLICT:
                sense = rpm.RPMDEP_SENSE_CONFLICTS
            elif p.type == rpm.RPMPROB_REQUIRES:
                sense = rpm.RPMDEP_SENSE_REQUIRES
            else:
                continue

            # strip arch, split to name, version, release
            nevr = p.altNEVR.rsplit('.', 1)[0]
            n, v, r = nevr.rsplit('-', 2)

            # extract the dependency information
            needs = p._str.split()
            needname = needs[0]
            needflags = rpm.RPMSENSE_ANY
            if len(needs) == 3:
                needop = needs[1]
                if needop.find('<') >= 0:
                    needflags |= rpm.RPMSENSE_LESS
                if needop.find('=') >= 0:
                    needflags |= rpm.RPMSENSE_EQUAL
                if needop.find('>') >= 0:
                    needflags |= rpm.RPMSENSE_GREATER
                needver = needs[2]
            else:
                needver = ""

            res.append(((n, v, r),
                        (needname, needver), needflags, sense, p.key))

        return res

    def hdrCheck(self, blob):
        res, msg = TransactionSetCore.hdrCheck(self, blob)
        # generate backwards compatibly broken exceptions
        if res == rpm.RPMRC_NOKEY:
            raise rpm.error("public key not available")
        elif res == rpm.RPMRC_NOTTRUSTED:
            raise rpm.error("public key not trusted")
        elif res != rpm.RPMRC_OK:
            raise rpm.error(msg)

    def hdrFromFdno(self, fd):
        res, h = TransactionSetCore.hdrFromFdno(self, fd)
        # generate backwards compatibly broken exceptions
        if res == rpm.RPMRC_NOKEY:
            raise rpm.error("public key not available")
        elif res == rpm.RPMRC_NOTTRUSTED:
            raise rpm.error("public key not trusted")
        elif res != rpm.RPMRC_OK:
            raise rpm.error("error reading package header")

        return h
