#! /usr/bin/env python


import re
import sys

C_ISH_FILENAME = "\.(c|cc|h|cpp|cxx|hxx)$"
C_ISH_FILENAME_RE = re.compile(C_ISH_FILENAME)
C_MODULE_FILENAME_RE = re.compile("\.(c|cc|cpp|cxx)$")
FIRST_INCLUDE = 'config.h'
problems = 0


def Problem(**kwargs):
    global problems
    problems += 1
    msg = kwargs['message']
    if kwargs['line']:
        location = "%(filename)s:%(line)d" % kwargs
    else:
        location = "%(filename)s" % kwargs
    detail = msg % kwargs
    print >>sys.stderr, "error: %s: %s" % (location, detail)


class RegexSniffer(object):
    def __init__(self, source, message):
        super(RegexSniffer, self).__init__()
        self._regex = re.compile(source)
        self._msg = message
    def Sniff(self, text, filename, line):
        m = self._regex.search(text)
        if m:
            if line is None:
                line = 1 + m.string.count('\n', 1, m.start(0))
            args = {
                'filename' : filename,
                'line'     : line,
                'fulltext' : text,
                'matchtext': m.group(0),
                'message'  : self._msg
                }
            Problem(**args)
            

class RegexChecker(object):
    def __init__(self, regex, line_smells, file_smells):
        super(RegexChecker, self).__init__(self)
        self._regex = re.compile(regex)
        self._line_sniffers = [RegexSniffer(s[0],s[1]) for s in line_smells]
        self._file_sniffers = [RegexSniffer(s[0],s[1]) for s in file_smells]
    def Check(self, filename, lines, fulltext):
        if self._regex.search(filename):
            # We recognise this type of file.
            for line_number, line_text in lines:
                for sniffer in self._line_sniffers:
                    sniffer.Sniff(line_text, filename, line_number)
            for sniffer in self._file_sniffers:
                sniffer.Sniff(fulltext, filename, None)
        else:
            # We don't know how to check this file.  Skip it.
            pass


checkers = [
    # Check C-like languages for C code smells.
    RegexChecker(C_ISH_FILENAME_RE,
    # line smells
    [
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
    
    ],
    [
    [r'# *include <assert.h>(?!.*assert \()',
     "If you include <assert.h>, use assert()."],
    [r'# *include "quotearg.h"(?!.*(?<!\w)quotearg(_[^ ]+)? \()',
     "If you include \"quotearg.h\", use one of its functions."],
    [r'# *include "quote.h"(?!.*(?<!\w)quote(_[^ ]+)? \()',
     "If you include \"quote.h\", use one of its functions."],
    ]),
    # Check Makefiles for Makefile code smells.
    RegexChecker('(^|/)[Mm]akefile(.am|.in)?', 
                 [ [r'^ ', "Spaces at start of line"], ],
                 []),
    # Check everything for whitespace problems.
    # RegexChecker('', [], [[r'\s$', "trailing whitespace"],]),
    # Check everything for out of date addresses.
    RegexChecker('', [], [
    [r'675\s*Mass\s*Ave,\s*02139[^a-zA-Z]*USA',
     "out of date FSF address"],
    [r'59 Temple Place.*02111-?1307\s*USA',
     "out of date FSF address"],
    ]),
    ]


# missing check: ChangeLog prefixes
# missing: sc_always_defined_macros from coreutils
# missing: sc_tight_scope


def Warning(filename, desc):
    print >> sys.stderr, "warning: %s: %s" % (filename, desc)


def BuildIncludeList(text):
    """Build a list of included files, with line numbers.
    Args:
      text: the full text of the source file
    Returns:
      [  ('config.h',32), ('assert.h',33), ... ]
    """
    include_re = re.compile(r'# *include +[<"](.*)[>"]')
    includes = []
    last_include_pos = 1
    line = 1
    for m in include_re.finditer(text):
        header = m.group(1)
        # Count only the number of lines between the last include and
        # this one.  Counting them from the beginning would be quadratic.
        line += m.string.count('\n', last_include_pos, m.start(0))
        last_include_pos = m.end()
        includes.append( (header,line) )
    return includes


def CheckStatHeader(filename, lines, fulltext):
    stat_hdr_re = re.compile(r'# *include .*<sys/stat.h>')
    # It's OK to have a pointer though.
    stat_use_re = re.compile(r'struct stat\W *[^*]')
    for line in lines:
        m = stat_use_re.search(line[1])
        if m:
            msg = "If you use struct stat, you must #include <sys/stat.h> first"
            Problem(filename = filename, line = line[0], message = msg)
            # Diagnose only once
            break
        m = stat_hdr_re.search(line[1])
        if m:
            break

def CheckFirstInclude(filename, lines, fulltext):
    includes = BuildIncludeList(fulltext)
    #print "Include map:"
    #for name, line in includes:
    #    print "%s:%d: %s" % (filename, line, name)
    if includes:
        actual_first_include = includes[0][0]
    else:
        actual_first_include = None
    if actual_first_include and actual_first_include  != FIRST_INCLUDE:
        if FIRST_INCLUDE in [inc[0] for inc in includes]:
            msg = ("%(actual_first_include)s is the first included file, "
                   "but %(required_first_include)s should be included first")
            Problem(filename=filename, line=includes[0][1], message=msg,
                    actual_first_include=actual_first_include,
                    required_first_include = FIRST_INCLUDE)
    if FIRST_INCLUDE not in [inc[0] for inc in includes]:
        Warning(filename,
                "%s should be included by most files" % FIRST_INCLUDE)


def SniffSourceFile(filename, lines, fulltext):
    if C_MODULE_FILENAME_RE.search(filename):
        CheckFirstInclude(filename, lines, fulltext)
        CheckStatHeader  (filename, lines, fulltext)
    for checker in checkers:
        checker.Check(filename, lines, fulltext)


def main(args):
    "main program"
    for srcfile in args[1:]:
        f = open(srcfile)
        line_number = 1
        lines = []
        for line in f.readlines():
            lines.append( (line_number, line) )
            line_number += 1
        fulltext = ''.join([line[1] for line in lines])
        SniffSourceFile(srcfile, lines, fulltext)
        f.close()
    if problems:
        return 1
    else:
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
