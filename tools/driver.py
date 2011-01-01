#Driver code for gvmt-lcc

import re
import sys
import os
from common import In, Out, GVMTException
import tempfile
import getopt

LCC_DIR = '/usr/local/lib/gvmt/lcc'

def run_lcc(target, src, flags, dst, lcc_dir):
    if lcc_dir is None:
         lcc_dir = LCC_DIR
    lcc_path = lcc_dir + '/lcc' 
    args = [ lcc_path, '-Wo-lccdir=%s' % lcc_dir, '-Wf-target=%s' % target, 
             '-g', '-S', '-A' ]
    args += flags
    args += [ '-I/usr/local/lib/gvmt/include', '-DGVMT_THREAD_LOCAL= ',
              '-DGVMT_LINKAGE', '-UNATIVE_LINKAGE', '-o', dst, src ]
    if '-v' in args:
        print lcc_path, ' '.join(args)
    code = os.spawnv(os.P_WAIT, lcc_path, args)
    if code:
#        print args
        raise GVMTException("lcc failed with exit code %d" % code)
 
if __name__ == '__main__':
    import sys
    run_lcc('gvmt', sys.argv[-2], sys.argv[-2:], sys.argv[-1])
        
