#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0-only
# SPDX-FileCopyrightText: 2022 Stefan Lengfeld

import os
import pty
import shutil
import unittest
import tempfile
import contextlib
import subprocess as sub
from os.path import join, isfile, isdir, join, realpath, dirname


class TestCaseTempFolder(unittest.TestCase):
    # These function will be called before and after every
    # testcase.
    @classmethod
    def setUp(cls):
        directory = join(dirname(__file__), "test")
        # cls.__name__ will be the name of the lower class.
        cls.tmpdir = tempfile.mkdtemp(prefix=cls.__name__,
                                      dir=directory)
        cls.old_cwd = os.getcwd()
        os.chdir(cls.tmpdir)

    @classmethod
    def tearDown(cls):
        os.chdir(cls.old_cwd)
        shutil.rmtree(cls.tmpdir)


def touch(filename):
    with open(filename, "bw") as f:
        pass


def mkdir(dirname):
    os.mkdir(dirname)


PROG = join(os.getcwd(), "modname")


def readAll(fd):
    import select

    poller = select.poll()
    poller.register(fd, select.POLLIN)

    buf = bytearray()
    while True:
        # TODO maybe scan for newline character instead of time-based stopping
        # -> No, it should read all available data. Not only a single line.
        events = poller.poll(100)
        if len(events) == 0:
            return buf
        else:
            assert(len(events) == 1)
            assert((events[0][1] & select.POLLIN) == select.POLLIN)
            # TODO actually the fd should have the flag O_NONBLOCK. Otherwise
            # this may block here.
            buf.extend(os.read(fd, 50))


class TTYAndProc():
    def __init__(self, master, proc):
        self.master = master
        self.proc = proc

    def readConsole(self):
        return readAll(self.master)

    def writeConsole(self, data):
        os.write(self.master, data)


@contextlib.contextmanager
def modname(arguments):
    try:
        master, slave = pty.openpty()
        proc = sub.Popen([PROG] + arguments, stdin=slave, stdout=slave, stderr=slave)
        yield TTYAndProc(master, proc)
    finally:
        proc.kill()
        proc.wait()
        os.close(master)
        os.close(slave)


class TestModname(TestCaseTempFolder):
    def testSmokeTest(self):
        touch("test2")

        # Before rename
        self.assertFalse(isfile("test2hello"))
        self.assertTrue(isfile("test2"))

        with modname(["test2"]) as c:
            self.assertEqual(c.readConsole(), b"\x1b[?2004h> test2")

            c.writeConsole(b"hello")
            self.assertEqual(c.readConsole(), b"hello")

            c.writeConsole(b"\n")
            self.assertEqual(c.readConsole(), b"\r\n\x1b[?2004l\r")

            ret = c.proc.wait()
            self.assertEqual(ret, 0)

        # After rename
        self.assertTrue(isfile("test2hello"))
        self.assertFalse(isfile("test2"))

    def testNoArgumentsExitAtOnce(self):
        with modname([]) as c:
            ret = c.proc.wait()
            self.assertEqual(ret, 0)
            self.assertEqual(c.readConsole(), b"")

    def testEmptyArguments(self):
        with modname([""]) as c:
            ret = c.proc.wait()
            self.assertEqual(ret, 0)
            self.assertEqual(c.readConsole(), b"")

    def testEmptyNewFilename(self):
        touch("test21")
        with modname(["test21"]) as c:
            # Hitting the backspace key multiple times
            c.writeConsole(b"\x7F" * 6)
            c.writeConsole(b"\n")
            ret = c.proc.wait()
            # TODO actually this should be an error
            self.assertEqual(ret, 0)

        # file is still there
        self.assertTrue(isfile("test21"))

    def testTrailingSlashes(self):
        touch("test21")
        with modname(["test21//"]) as c:
            # The printed filename should not contain the slashes.
            self.assertIn(b"> test21", c.readConsole())
            c.writeConsole(b"\n")
            ret = c.proc.wait()
            self.assertEqual(ret, 0)
        self.assertTrue(isfile("test21"))

        with modname(["/"]) as c:
            # Slash is removed and therefore the filename is empty. And
            # empty filenames are skipped/ignored.
            ret = c.proc.wait()
            self.assertEqual(ret, 0)

    def testSlashInNewFilename(self):
        touch("test21")
        with modname(["test21"]) as c:
            c.writeConsole(b"/file.txt\n")
            ret = c.proc.wait()
            self.assertIn(b"New filename cannot contain a slash.\r\n", c.readConsole())
            self.assertEqual(ret, 1)
        self.assertTrue(isfile("test21"))

    def testOldFilenameTooLong(self):
        long_filename = "x" * 513
        with modname([long_filename]) as c:
            ret = c.proc.wait()
            self.assertIn(b"Filename too long!\r\n", c.readConsole())
            self.assertEqual(ret, 1)

    def testFileNameIsInNonExistingDirectory(self):
        with modname(["dir/file"]) as c:
            self.assertIn(b"> file", c.readConsole())
            c.writeConsole(b"21\n")
            ret = c.proc.wait()
            self.assertIn(b"Cannot rename file 'dir/file': No such file or directory", c.readConsole())
            self.assertEqual(ret, 1)

    def testNewFilenameTooLong(self):
        short_filename = b"shortfilename.txt"
        touch(short_filename)

        long_filename = b"x" * 256 + b".txt"
        with modname([short_filename]) as c:
            self.assertIn(b"> " + short_filename, c.readConsole())

            c.writeConsole(long_filename)
            self.assertEqual(c.readConsole(), long_filename)

            c.writeConsole(b"\n")
            ret = c.proc.wait()
            self.assertEqual(ret, 1)
            self.assertIn(b"Cannot rename file \'shortfilename.txt\': File name too long\r\n",
                          c.readConsole(),)

    def testFileInDirectory(self):
        mkdir("dir")
        touch("dir/file")
        with modname(["dir/file"]) as c:
            self.assertIn(b"> file", c.readConsole())
            c.writeConsole(b"2\n")  # append "2"
            ret = c.proc.wait()
            self.assertEqual(ret, 0)
        self.assertTrue(isfile("dir/file2"))

    def testRenameNonExistingFile(self):
        with modname(["non-existing"]) as c:
            self.assertIn(b"> non-existing", c.readConsole())
            c.writeConsole(b"2\n")  # append "2"
            ret = c.proc.wait()
            self.assertEqual(ret, 1)
            self.assertIn(b"Cannot rename file 'non-existing': No such file or directory",
                          c.readConsole(),)

    def testNewFilenameAlreadyExists(self):
        touch("file")
        touch("file-existing")
        with modname(["file"]) as c:
            self.assertIn(b"> file", c.readConsole())
            c.writeConsole(b"-existing\n")  # append "-existing"
            ret = c.proc.wait()
            self.assertEqual(ret, 0)
        # The current behavior is that modname will overwrite the existing
        # file.
        self.assertFalse(isfile("file"))
        self.assertTrue(isfile("file-existing"))


if __name__ == '__main__':
    unittest.main()
