
# modname

`modname` is an interactive file rename utility based on [GNU readline][readline].

There is already the command `mv` and the command `rename`. Why another
command?

I often want to make slight modifications of a filename _and_ have structured
filenames that require multiple keystrokes and tab auto completions to complete
on the commandline. Doing the same char-tab-char-tab-char-tab-... sequence two
times is annoying. So this utility allows to do it once and then modify the
filename in place with line editing support.

The name `modname` is a short variant of "modify name".

[readline]: https://tiswww.case.edu/php/chet/readline/rltop.html


## Usage:

Example console capture:

    $ touch fileX
    $ modname fileX
    > fileX

The last line is a interactive. You can edit the filename with the line editing
support (like a shell prompt) by GNU readline.

After you made the changes to the filename, you can hit enter to rename the
file. Then the console screen may look like

    $ touch fileX
    $ modname fileX
    > fileY
    $ ls
    fileY


## How to build

Ensure that you have the library and headers for GNU readline installed.
Then just execute

    $ make

The binary `modname` is build directly in the current/root directory of the
project.


## How to install

There is currently no install target. Just copy the binary `modname` into a
folder that is also in your `PATH` environment variable.


## How to test

Execute

    $ make && ./test.py
    $ make modnametest && ./modnametest

## Homepage and repo

There is a [projects homepage](https://stefan.lengfeld.xyz/projects/modname/).

To get the source code of the project, execute

    $ git clone https://github.com/lengfeld/modname


## License

This program is licensed under GPLv2-only.


## Learnings

Bootstrap your test infrastructure as early as possible.

Use a code formatter. I don't want to format/indent my code manually.

Use C variable attribute cleanup to avoid memory leaks and coding mistakes.


## TODOs

Enable sanitziers for test binarys.

Also try out valgrind.

Add manpage

Add install Makefile target

debian/ubuntu package and gentoo ebuild

Support arguments like `-v/--version` and `-h/--help`.


## Links to GNU readline documentation

https://tiswww.case.edu/php/chet/readline/readline.html#IDX325
