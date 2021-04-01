from __future__ import annotations
from collections.abc import Iterable
from typing import Any, cast, List, Literal, Optional, Tuple, Union
import sys
import rpm
from rpm._rpm import ts as TransactionSetCore

HdrOrFD = Union[rpm.hdr, rpm.AnyFD]
RunRes = List[Tuple[str, Tuple[rpm.rpmProblemType, Optional[str], int]]]
CheckRes = List[Tuple[Tuple[str, str, str], Tuple[str, str], int, int, Any]]

# TODO: migrate relevant documentation from C-side
class TransactionSet(TransactionSetCore):
    _probFilter: int = 0

    def _wrapSetGet(self, attr: str, val: int) -> int:
        oval = getattr(self, attr)
        setattr(self, attr, val)
        return cast(int, oval)

    def setVSFlags(self, flags: int) -> int:
        return self._wrapSetGet('_vsflags', flags)

    def getVSFlags(self) -> int:
        return self._vsflags

    def setVfyFlags(self, flags: int) -> int:
        return self._wrapSetGet('_vfyflags', flags)

    def getVfyFlags(self) -> int:
        return self._vfyflags

    def getVfyLevel(self) -> int:
        return self._vfylevel

    def setVfyLevel(self, flags: int) -> int:
        return self._wrapSetGet('_vfylevel', flags)

    def setColor(self, color: int) -> int:
        return self._wrapSetGet('_color', color)

    def setPrefColor(self, color: int) -> int:
        return self._wrapSetGet('_prefcolor', color)

    def setFlags(self, flags: int) -> int:
        return self._wrapSetGet('_flags', flags)

    def setProbFilter(self, ignoreSet: int) -> int:
        return self._wrapSetGet('_probFilter', ignoreSet)

    def parseSpec(self, specfile: str) -> rpm.spec:
        return rpm.spec(specfile)

    def getKeys(self) -> Optional[Tuple[Any, ...]]:
        keys: List[Any] = []
        for te in self:
            keys.append(te.Key())
        # Backwards compatibility goo - WTH does this return a *tuple* ?!
        if not keys:
            return None
        else:
            return tuple(keys)

    def _f2hdr(self, item: HdrOrFD) -> rpm.hdr:
        if isinstance(item, str):
            with open(item) as f:
                header = self.hdrFromFdno(f)
        elif isinstance(item, rpm.hdr):
            header = item
        else:
            header = self.hdrFromFdno(item)
        return header

    def addInstall(self, item: HdrOrFD, key: Any, how: Literal['u', 'i'] = "u") -> None:
        header = self._f2hdr(item)

        if how not in ['u', 'i']:
            raise ValueError('how argument must be "u" or "i"')
        upgrade = (how == "u")

        if not TransactionSetCore.addInstall(self, header, key, upgrade):
            if upgrade:
                raise rpm.error("adding upgrade to transaction failed")
            else:
                raise rpm.error("adding install to transaction failed")

    def addReinstall(self, item: HdrOrFD, key: Any) -> None:
        header = self._f2hdr(item)

        if not TransactionSetCore.addReinstall(self, header, key):
            raise rpm.error("adding reinstall to transaction failed")

    def addErase(self, item: Union[rpm.mi, rpm.hdr, int, str]) -> None:
        hdrs: Iterable[rpm.hdr]

        # match iterators are passed on as-is
        if isinstance(item, rpm.mi):
            hdrs = item
        elif isinstance(item, rpm.hdr):
            hdrs = [item]
        elif isinstance(item, (int, str)):
            hdrlist: List[rpm.hdr] = []
            if isinstance(item, int):
                dbi = rpm.RPMDBI_PACKAGES
            else:
                dbi = rpm.RPMDBI_LABEL

            for h in self.dbMatch(dbi, item):
                hdrlist.append(h)

            if not hdrlist:
                raise rpm.error("package not installed")
            hdrs = hdrlist
        else:
            raise TypeError("invalid type %s" % type(item))

        for h in hdrs:
            if not TransactionSetCore.addErase(self, h):
                raise rpm.error("adding erasure to transaction failed")

    def run(self, callback: rpm.tsRunCb, data: Any) -> Optional[RunRes]:
        rc = TransactionSetCore.run(self, callback, data, self._probFilter)

        # crazy backwards compatibility goo: None for ok, list of problems
        # if transaction didn't complete and empty list if it completed
        # with errors
        if rc == 0:
            return None

        res: RunRes = []
        if rc > 0:
            for prob in self.problems():
                item = ("%s" % prob, (prob.type, prob._str, prob._num))
                res.append(item)
        return res

    def check(self, *args: rpm.tsCheckCb, **kwds: rpm.tsCheckCb) -> CheckRes:
        TransactionSetCore.check(self, *args, **kwds)

        # compatibility: munge problem strings into dependency tuples of doom
        res: CheckRes = []
        for p in self.problems():
            # is it anything we need to care about?
            if p.type == rpm.RPMPROB_CONFLICT:
                sense = rpm.RPMDEP_SENSE_CONFLICTS
            elif p.type == rpm.RPMPROB_REQUIRES:
                sense = rpm.RPMDEP_SENSE_REQUIRES
            else:
                continue

            # strip arch, split to name, version, release
            altnevr: Optional[str] = p.altNEVR
            if not altnevr:
                continue
            nevr = altnevr.rsplit('.', 1)[0]
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
                        (needname, needver), needflags, sense, p.key))

        return res

    def hdrCheck(self, blob: bytes) -> None:
        res, msg = TransactionSetCore.hdrCheck(self, blob)
        # generate backwards compatibly broken exceptions
        if res == rpm.RPMRC_NOKEY:
            raise rpm.error("public key not available")
        elif res == rpm.RPMRC_NOTTRUSTED:
            raise rpm.error("public key not trusted")
        elif res != rpm.RPMRC_OK:
            raise rpm.error(msg)

    def hdrFromFdno(self, fd: rpm.AnyFD) -> rpm.hdr:
        res, h = TransactionSetCore.hdrFromFdno(self, fd)
        # generate backwards compatibly broken exceptions
        if res == rpm.RPMRC_NOKEY:
            raise rpm.error("public key not available")
        elif res == rpm.RPMRC_NOTTRUSTED:
            raise rpm.error("public key not trusted")
        elif res != rpm.RPMRC_OK:
            raise rpm.error("error reading package header")

        return h
