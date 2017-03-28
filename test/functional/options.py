import os

tmpdir = "/tmp/user/1000/123"
coveragedir = None
loglevel = "DEBUG"
nocleanup = False
noshutdown = False
srcdir = os.path.normpath(os.path.dirname(os.path.realpath(__file__)) + "/../../../src")
cachedir = os.path.normpath(os.path.dirname(os.path.realpath(__file__)) + "/../../cache")
trace_rpc = False
portseed = os.getpid()
coveragedir = None
