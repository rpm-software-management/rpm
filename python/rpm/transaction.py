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

