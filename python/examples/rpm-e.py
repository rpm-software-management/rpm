#!/usr/bin/python3

# Uninstall a package from the system.
# A Python equivalent for `rpm -e hello`

import sys
import rpm


class Callback:
    def callback(self, what, amount, total, mydata, wibble):
        # Transactions that only remove packages don't strictly require the
        # the callback to do anything. Though, most operations require at least
        # `rpm.RPMCALLBACK_INST_OPEN_FILE` and `rpm.RPMCALLBACK_INST_CLOSE_FILE`
        # to be handled. See `rpm-i.py` example for more information.
        pass


ts = rpm.TransactionSet()
for name in sys.argv[1:]:
    ts.addErase(name)

callback = Callback()
ts.run(callback.callback, "")
