#!/usr/bin/python
# gvmtcc
# Front end compiler.
# Translates C code to GSC.
import re, sys, os
import common
import tempfile
import getopt
import gsc, objects
import sys_compiler
import driver
   
options = {
    'h' : "Print this help and exit",
    'I file' : "Add file to list of #include file for lcc sub-process, can be used multiple times",
    'o outfile-name' : 'Specify output file name',
    'v' : 'Echo sub-processes invoked',
    'a' : 'Warn about non-ANSI C.',
    'D symbol' : 'Define symbol in preprocessor',
    'z' : 'Do not put gc-safe-points on backward edges', 
    'L' : 'Directory to find lcc executables',
    'x' : 'Output dot file of IR (For debugging compiler)'
} 

class Fgen(object):

    def __init__(self, opts):
        flags = [ '-I/usr/local/include' ]
        includes = []
        outfile = None
        target = 'gvmt'
        verbose = False
        gc_safe = True
        ext = '.gsc'
        self.dot = False
        self.lcc_dir = None
        for opt, value in opts:
            if opt == '-h':
                common.print_usage(options)
                sys.exit(0)
            elif opt == '-I':
                flags.append('-I%s' % value)
            elif opt == '-o':
                outfile = value
            elif opt == '-v':
                flags.append('-v')
                verbose = True
            elif opt == '-x':
                flags.append('-Wf-xdot')
                ext = '.dot'
                self.dot = True
            elif opt == '-a':
                flags.append('-A')
            elif opt == '-D':
                flags.append('-D%s' % value)
            elif opt == '-D':
                flags.append('-D%s' % value)
            elif opt == '-L':
                self.lcc_dir = value
        if gc_safe:
            flags.append('-Wf-xgcsafe')
        self.flags = flags
        if outfile:
            self.outfile = outfile
        else:
            dot = args[0].rfind('.')
            self.outfile = args[0][:dot] + ext
        self.target = target
        self.verbose = verbose
        self.tempdir = os.path.join(sys_compiler.TEMPDIR, 'gvmtc')
        if not os.path.exists(self.tempdir):
            os.makedirs(self.tempdir)
        
    def process(self, args):
        if isinstance(args, str):
            args = [ args ]
        h = os.path.join(self.tempdir, 'functions.h')
        for a in args:
            lcc_in = os.path.join(self.tempdir, 'functions.c')
            out = open(lcc_in, 'w')
            print >> out, '#line 1 "%s"' % a
            in_ = open(a, 'r')
            out.writelines(in_.readlines())
            in_.close()
            out.close()
            lcc_out = os.path.join(self.tempdir, 'functions.s')
            if self.dot:
                driver.run_lcc(self.target, lcc_in, self.flags, self.outfile, self.lcc_dir)
            else:
                driver.run_lcc(self.target, lcc_in, self.flags, lcc_out, self.lcc_dir)
                gsc_file = gsc.read(common.In(lcc_out))
#            os.rmdir(self.tempdir) 
                gsc_file.code.lcc_post()
                gsc_file.write(common.Out(self.outfile))
            
    def close(self):
        pass
   
if __name__ == '__main__':
    opts, args = getopt.getopt(sys.argv[1:], 'hI:o:vnzxaD:L:')
    if not args:
        common.print_usage(options)
        sys.exit(1)
    try:
        fgen = Fgen(opts)
        fgen.process(args)
        fgen.close()
    except common.GVMTException, ex:
        print ex
        sys.exit(1)
        
