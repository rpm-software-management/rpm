"""
TestCases for using the DB.join and DBCursor.join_item methods.
"""

import sys, os, string
import tempfile
from pprint import pprint
import unittest

from rpmdb import db

from test_all import verbose


