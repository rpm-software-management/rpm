#!/usr/bin/env python
#
# Interface for generating TET like report journals
#
# This intended to be a simple way of creating journals which
# can be analysed using standard TET journal tools without
# having to compile or link against the TET libraries.
#
# (C) Copyright 2002 The Free Standards Group, Inc.
#
# Python module converted from C version (tetj.c 1.3)
# 2002/09/20 Mats Wichmann, Intel
#
# This is Revision: 1.1 
#
# Log: tetj.py,v 
# Revision 1.1  2002/09/20 15:12:24  mwichmann
# Initial converted version.  Includes a self-test harness.
#
#
import sys, os, pwd, time

TETJ_PASS = 0
TETJ_FAIL = 1
TETJ_UNRESOLVED = 2
TETJ_NOTINUSE = 3
TETJ_UNSUPPORTED = 4
TETJ_UNTESTED = 5
TETJ_UNINITIATED = 6
TETJ_UNREPORTED = 7
TETJ_WARNING = 101
TETJ_FIP = 102
TETJ_NOTIMP = 103
TETJ_UNAPPROVE = 104

result_codes = {
    TETJ_PASS: "PASS",
    TETJ_FAIL: "FAIL",
    TETJ_UNRESOLVED: "UNRESOLVED",
    TETJ_NOTINUSE: "NOTINUSE",
    TETJ_UNSUPPORTED: "UNSUPPORTED",
    TETJ_UNTESTED: "UNTESTED",
    TETJ_UNINITIATED: "UNITIATED",
    TETJ_UNREPORTED: "UNREPORTED",
    TETJ_WARNING: "WARNING",
    TETJ_FIP: "FIP",
    TETJ_NOTIMP: "NOTIMP",
    TETJ_UNAPPROVE: "UNAPPROVE",
}

class tetj_handle:
    def __init__(self, journal):
        self.journal = journal

activity_count = 0
tp_count = 0

def get_current_time_string():
    return time.strftime("%H:%M:%S")

def start_journal(pathname, command_run):
    try:
        f = open(pathname, 'w')
    except IOerror:
        return None
    handle = tetj_handle(f)

    (sysname, nodename, release, version, machine) = os.uname()
    datetime = time.strftime("%H:%M:%S %Y%m%d")
    uid = os.getuid();
    try:
        pwent = pwd.getpwuid(uid);
    except KeyError:
        pwent = ""
  
    handle.journal.write("0|lsb-0.1 %s|User: %s (%i) TCC Start, Command line: %s\n" %
          (datetime, pwent[0], uid, command_run))

    handle.journal.write("5|%s %s %s %s %s|System Information\n" %
          (sysname, nodename, release, version, machine))
          
    return handle

def close_journal(handle):
    if handle:
        handle.journal.write("900|%s|TCC End\n" % get_current_time_string())
        return handle.journal.close()
    else:
        return 0

def add_config(handle, message):
    if handle:
        handle.journal.write("30||%s\n" % message)

def add_controller_error(handle, message):
    if handle:
        handle.journal.write("50||%s\n" % message)

def testcase_start(handle, activity, testcase, message):
    if handle:
        handle.journal.write("10|%u %s %s|TC Start%s\n" %
            (activity, testcase, get_current_time_string(), message))

def testcase_end(handle, activity, testcase, message):
    if handle:
        handle.journal.write("80|%u %s %s|TC End%s\n" %
            (activity, testcase, get_current_time_string(), message))

def purpose_start(handle, activity, tpnumber, message):
    if (handle):
        handle.journal.write("400|%u %u %s|IC Start\n" %
            (activity, tpnumber, get_current_time_string()))
        handle.journal.write("200|%u %u %s|%s\n" %
            (activity, tpnumber, get_current_time_string(), message))

def purpose_end(handle, activity, tpnumber):
    if handle:
        handle.journal.write("410|%u %u %s|IC End\n" %
            (activity, tpnumber, get_current_time_string()))

def result(handle, activity, tpnumber, result):
    if handle:
        code = result_codes.get(result, "UNKNOWN")
        handle.journal.write("220|%u %u %i %s|%s\n" %
            (activity, tpnumber, result, get_current_time_string(), code))

def testcase_info(handle,activity,tpnumber,context,block,sequence,message):
    if handle:
        handle.journal.write("520|%u %u %u %u %u|%s\n" %
            (activity, tpnumber, context, block, sequence, message))


def _test():
    # self-test code: exercise a bunch of the stuff,
    # for now there's no feedback as to whether it came out right -
    # just manually scan the journal.tetjtest file
    teststuff = {
        "red": TETJ_PASS,
        "green": TETJ_FAIL,
        "blue": TETJ_UNRESOLVED,
        "white": TETJ_NOTINUSE,
        "black": TETJ_UNSUPPORTED,
        "purple": TETJ_UNTESTED,
        "teal": TETJ_UNINITIATED,
        "yellow": TETJ_UNREPORTED,
        "orange": TETJ_WARNING,
        "plum": TETJ_FIP,
        "foxglove": TETJ_NOTIMP,
        "alabaster": TETJ_UNAPPROVE
    }
    print "tetj.py: writing journal to journal.tetjtest"
    journal = start_journal("journal.tetjtest", "tetjtest")
    add_config(journal, "VSX_NAME=tetjtest unofficial")
    activity_count = 1
    testcase_start(journal, activity_count, "foo", "")
    tp_count = 0
    for (purpose, tpresult) in teststuff.items():
        tp_count += 1
        purpose_start(journal, activity_count, tp_count, purpose)
        result(journal, activity_count, tp_count, tpresult)
        purpose_end(journal, activity_count, tp_count)
    testcase_end(journal, activity_count, "foo", "")
    activity_count += 1
    testcase_start(journal, activity_count, "bar", "")
    for (purpose, tpresult) in teststuff.items():
        tp_count += 1
        purpose_start(journal, activity_count, tp_count, purpose)
        result(journal, activity_count, tp_count, tpresult)
        purpose_end(journal, activity_count, tp_count)
    testcase_end(journal, activity_count, "bar", "")
    close_journal(journal)

if __name__ == "__main__":
    _test()
