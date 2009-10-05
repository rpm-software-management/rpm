#!/usr/bin/python

import _rpm

# TODO: migrate relevant documentation from C-side
class TransactionSet(_rpm.ts):
    _probFilter = 0
    # FIXME: kludge for keeping refcounts on transaction element keys
    _keyList = []

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
        import _rpmb
        return _rpmb.spec(specfile)

    def getKeys(self):
        keys = []
        for te in self:
            keys.append(te.Key())
        # Backwards compatibility goo - WTH does this return a *tuple* ?!
        if not keys:
            return None
        else:
            return tuple(keys)

    def addInstall(self, item, key, how="u"):
        if isinstance(item, str):
            f = file(item)
            header = self.hdrFromFdno(f)
            f.close()
        elif isinstance(item, file):
            header = self.hdrFromFdno(item)
        else:
            header = item

        if not how in ['u', 'i']:
            raise ValueError, 'how argument must be "u" or "i"'
        upgrade = (how == "u")

        if not _rpm.ts.addInstall(self, header, key, upgrade):
            raise _rpm.error, "adding package to transaction failed"
        self._keyList.append(key)

    def addErase(self, item):
        hdrs = []
        if isinstance(item, _rpm.hdr):
            hdrs = [item]
        elif isinstance(item, _rpm.mi):
            hdrs = item
        elif isinstance(item, int):
            hdrs = self.dbMatch(_rpm.RPMDBI_PACKAGES, item)
        elif isinstance(item, str):
            hdrs = self.dbMatch(_rpm.RPMDBI_LABEL, item)
        else:
            raise TypeError, "invalid type %s" % type(item)

        for h in hdrs:
            if not _rpm.ts.addErase(self, h):
                raise _rpm.error, "package not installed"

        # garbage collection should take care but just in case...
        if isinstance(hdrs, _rpm.mi):
            del hdrs

    def run(self, callback, data):
        rc = _rpm.ts.run(self, callback, data, self._probFilter)

        # crazy backwards compatibility goo: None for ok, list of problems
        # if transaction didnt complete and empty list if it completed
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
        _rpm.ts.check(self, *args, **kwds)

        probs = self.problems()
        if not probs:
            return None

        # compatibility: munge problem strings into dependency tuples of doom
        res = []
        for p in probs:
            # is it anything we need to care about?
            if p.type == _rpm.RPMPROB_CONFLICT:
                sense = _rpm.RPMDEP_SENSE_CONFLICTS
            elif p.type == _rpm.RPMPROB_REQUIRES:
                sense = _rpm.RPMDEP_SENSE_REQUIRES
            else:
                continue

            # strip arch, split to name, version, release
            nevr = p.pkgNEVR.rsplit('.', 1)[0]
            n, v, r = nevr.rsplit('-', 2)

            # extract the dependency information
            needs = p.altNEVR.split()[1:]
            needname = needs[0]
            needflags = _rpm.RPMSENSE_ANY
            if len(needs) == 3:
                needop = needs[1]
                if needop.find('<') >= 0: needflags |= _rpm.RPMSENSE_LESS
                if needop.find('=') >= 0: needflags |= _rpm.RPMSENSE_EQUAL
                if needop.find('>') >= 0: needflags |= _rpm.RPMSENSE_GREATER
                needver = needs[2]
            else:
                needver = ""

            res.append(((n, v, r),(needname,needver),needflags,sense,p.key))

        return res
