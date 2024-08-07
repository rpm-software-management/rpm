#!/usr/bin/python3

# Uninstall a package from the system.
# A Python equivalent for `rpm -e hello`

import sys
import rpm


class Callback:
    def callback(self, what, amount, total, mydata, wibble):
        pass


ts = rpm.TransactionSet()
for name in sys.argv[1:]:
    ts.addErase(name)

callback = Callback()
ts.run(callback.callback, "")
