#!/usr/bin/python

import _rpm

# TODO: migrate relevant documentation from C-side
class TransactionSet(_rpm.ts):
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

    def addInstall(self, header, key, how="u"):
        if not how in ['u', 'i']:
            raise ValueError, 'how argument must be "u" or "i"'
        upgrade = (how == "u")

        if not _rpm.ts.addInstall(self, header, key, upgrade):
            raise _rpm.error, "adding package to transaction failed"
        self._keyList.append(key)
