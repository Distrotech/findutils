#! /usr/bin/env python


import re
import sys

FIRST_INCLUDE = 'config.h'
problems = 0

LINE_SMELLS_SRC = [
    [r'(?<!\w)free \(\(', "don't cast the argument to free()"],
    [r'\*\) *x(m|c|re)alloc(?!\w)',"don't cast the result of x*alloc"],
    [r'\*\) *alloca(?!\w)',"don't cast the result of alloca"],
    [r'[ ]	',"found SPACE-TAB; remove the space"],
    [r'(?<!\w)([fs]?scanf|ato([filq]|ll))(?!\w)',
     'do not use *scan''f, ato''f, ato''i, ato''l, ato''ll, ato''q, or ss''canf'],
    [r'error \(EXIT_SUCCESS',"passing EXIT_SUCCESS to error is confusing"],
    [r'file[s]ystem', "prefer writing 'file system' to 'filesystem'"],
    [r'HAVE''_CONFIG_H', "Avoid checking HAVE_CONFIG_H"],
#   [r'HAVE_FCNTL_H', "Avoid checking HAVE_FCNTL_H"],
    [r'O_NDELAY', "Avoid using O_NDELAY"],
    [r'the *the', "'the the' is probably not deliberate"],
    [r'(?<!\w)error \([^_"]*[^_]"[^"]*[a-z]{3}', "untranslated error message"],
    [r'^# *if\s+defined *\(', "useless parentheses in '#if defined'"],
     
    ]

FILE_SMELLS_SRC = [
    [r'# *include <assert.h>(?!.*assert \()',
     "If you include <assert.h>, use assert()."],
    [r'# *include "quotearg.h"(?!.*(?<!\w)quotearg(_[^ ]+)? \()',
     "If you include \"quotearg.h\", use one of its functions."],
    [r'# *include "quote.h"(?!.*(?<!\w)quote(_[^ ]+)? \()',
     "If you include \"quote.h\", use one of its functions."],
    [r'\s$', "trailing whitespace"],
    ]

# missing check: ChangeLog prefixes
# missing: sc_always_defined_macros from coreutils
# missing: sc_tight_scope

COMPILED_LINE_SMELLS = [(re.compile(pong[0]), pong[1])
                        for pong in LINE_SMELLS_SRC]
COMPILED_FILE_SMELLS = [(re.compile(pong[0], re.DOTALL), pong[1])
                        for pong in FILE_SMELLS_SRC]

def Problem(filename, desc):
    global problems
    print >> sys.stderr, "error: %s: %s" % (filename, desc)
    problems += 1

def Warning(filename, desc):
    print >> sys.stderr, "warning: %s: %s" % (filename, desc)


def BuildIncludeList(text):
    include_re = re.compile(r'# *include +[<"](.*)[>"]')
    return [m.group(1) for m in include_re.finditer(text)]


def CheckStatHeader(filename, lines, fulltext):
    stat_hdr_re = re.compile(r'# *include .*<sys/stat.h>')
    # It's OK to have a pointer though.
    stat_use_re = re.compile(r'struct stat\W *[^*]')
    for line in lines:
        m = stat_use_re.search(line[1])
        if m:
            msgfmt = "%d: If you use struct stat, you must #include <sys/stat.h> first"
            Problem(filename, msgfmt % line[0])
            # Diagnose only once
            break
        m = stat_hdr_re.search(line[1])
        if m:
            break

def CheckFirstInclude(filename, lines, fulltext):
    includes = BuildIncludeList(fulltext)
    if includes and includes[0] != FIRST_INCLUDE:
        if FIRST_INCLUDE in includes:
            fmt = "%s is first include file, but %s should be included first"
            Problem(filename, fmt % (includes[0], FIRST_INCLUDE))
    if FIRST_INCLUDE not in includes:
        Warning(filename,
                "%s should be included by most files" % FIRST_INCLUDE)

def CheckLineSmells(filename, lines, fulltext):
    for line in lines:
        for smell in COMPILED_LINE_SMELLS:
            match = smell[0].search(line[1])
            if match:
                Problem(filename, '%d: "%s": %s'
                        % (line[0], match.group(0), smell[1]))
            

def CheckFileSmells(filename, lines, fulltext):
    for smell in COMPILED_FILE_SMELLS:
        match = smell[0].search(fulltext)
        if match:
            Problem(filename, ' %s'
                    % (smell[1],))
            



def SniffSourceFile(filename, lines, fulltext):
    for sniffer in [CheckFirstInclude, CheckStatHeader,
                    CheckLineSmells, CheckFileSmells]:
        sniffer(filename, lines, fulltext)


def main(args):
    "main program"
    for srcfile in args[1:]:
        f = open(srcfile)
        line_number = 1
        lines = []
        for line in f.readlines():
            lines.append( (line_number, line) )
            line_number += 1
        fulltext = '\n'.join([line[1] for line in lines])
        SniffSourceFile(srcfile, lines, fulltext)
        f.close()
    if problems:
        return 1
    else:
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
