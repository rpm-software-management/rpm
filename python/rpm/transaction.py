import sys
import rpm
from rpm._rpm import ts as TransactionSetCore

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

    def setVfyFlags(self, flags):
        return self._wrapSetGet('_vfyflags', flags)

    def getVfyFlags(self):
        return self._vfyflags

    def getVfyLevel(self):
        return self._vfylevel

    def setVfyLevel(self, flags):
        return self._wrapSetGet('_vfylevel', flags)

    def setColor(self, color):
        return self._wrapSetGet('_color', color)

    def setPrefColor(self, color):
        return self._wrapSetGet('_prefcolor', color)

    def setFlags(self, flags):
        return self._wrapSetGet('_flags', flags)

    def setProbFilter(self, ignoreSet):
        return self._wrapSetGet('_probFilter', ignoreSet)

    def parseSpec(self, specfile):
        return rpm.spec(specfile)

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
        if isinstance(item, str):
            with open(item) as f:
                header = self.hdrFromFdno(f)
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
            if upgrade:
                raise rpm.error("adding upgrade to transaction failed")
            else:
                raise rpm.error("adding install to transaction failed")

    def addReinstall(self, item, key):
        header = self._f2hdr(item)

        if not TransactionSetCore.addReinstall(self, header, key):
            raise rpm.error("adding reinstall to transaction failed")

    def addErase(self, item):
        hdrs = []
        # match iterators are passed on as-is
        if isinstance(item, rpm.mi):
            hdrs = item
        elif isinstance(item, rpm.hdr):
            hdrs.append(item)
        elif isinstance(item, (int, str)):
            if isinstance(item, int):
                dbi = rpm.RPMDBI_PACKAGES
            else:
                dbi = rpm.RPMDBI_LABEL

            for h in self.dbMatch(dbi, item):
                hdrs.append(h)

            if not hdrs:
                raise rpm.error("package not installed")
        else:
            raise TypeError("invalid type %s" % type(item))

        for h in hdrs:
            if not TransactionSetCore.addErase(self, h):
                raise rpm.error("adding erasure to transaction failed")

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
                if '<' in needop:
                    needflags |= rpm.RPMSENSE_LESS
                if '=' in needop:
                    needflags |= rpm.RPMSENSE_EQUAL
                if '>' in needop:
                    needflags |= rpm.RPMSENSE_GREATER
                needver = needs[2]
            else:
                needver = ""

            res.append(((n, v, r),
                        (needname, needver), needflags, p.key, sense))

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
