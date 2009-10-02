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

