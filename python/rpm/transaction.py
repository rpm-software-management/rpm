#!/usr/bin/python

import _rpm

# TODO: migrate relevant documentation from C-side
class TransactionSet(_rpm.ts):
    def _wrapSetGet(self, attr, val):
        oval = getattr(self, attr)
        setattr(self, attr, val)
        return oval
        
    def setVSFlags(self, flags):
        return _wrapSetGet('_vsflags', flags)

    def getVSFlags(self):
        return self._vsflags

    def setColor(self, color):
        return _wrapSetGet('_color', color)

    def setPrefColor(self, color):
        return _wrapSetGet('_prefcolor', color)

    def setFlags(self, flags):
        return _wrapSetGet('_flags', flags)

    def setProbFilter(self, ignoreSet):
        return _wrapSetGet('_probFilter', ignoreSet)

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
