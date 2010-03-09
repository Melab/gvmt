import os
import sys
    
CC_PATH = ''
AR_PATH = '/usr/bin/ar'
RANLIB = 'ranlib'
LD_PATH = ''

TEMPDIR = '/tmp/gvmt'
if 'linux' in sys.platform:
    for f in ['/usr/bin/gcc', '/usr/sbin/gcc', '/usr/local/bin/gcc', '/usr/local/sbin/gcc' ]:
        if os.path.exists(f):
            CC_PATH = f
            break
    if not CC_PATH:
        raise Exception("Cannot find GCC")
    CC_ARGS = [ CC_PATH, '-std=c99', '-ffreestanding', '-fomit-frame-pointer' ]
    for f in ['/usr/bin/ld', '/usr/sbin/ld', '/usr/local/bin/ld', '/usr/local/sbin/ld' ]:
        if os.path.exists(f):
            LD_PATH = f
            break
    if not LD_PATH:
        raise Exception("Cannot find ld")
    del f
else:
    raise Exception( "FIX ME!")

ASM_EXT = '.s'
OBJ_EXT = '.o'
LIB_EXT = '.so'

DEBUG = '-g'
OBJECT = '-c'
ASSEMBLER = '-S'
OUTPUT_PATTERN = '-o%s'
LIB = '-shared'
MOVABLE = '-fPIC'

def section_label(name):
    return '''.globl gvmt_section_%s
    .data
    .align 32
gvmt_section_%s:''' % (name, name)
           
def c_to_object(base_name, optimise, library, debug, sys_headers):
    c_to_asm(base_name, optimise, library, debug, sys_headers)
    return asm_to_object(base_name, debug)
#    out_name = os.path.join(TEMPDIR, '%s%s' % (base_name, OBJ_EXT))
#    c_name = os.path.join(TEMPDIR, '%s.c' % base_name)
#    args = CC_ARGS + [OBJECT, optimise, OUTPUT_PATTERN % out_name]
#    if sys_headers:
#        args.append('-I%s' % sys_headers)
#    if debug:
#        args.append(DEBUG)
#    if library:
#        args.append(MOVABLE)
#    return os.spawnv(os.P_WAIT, CC_PATH, args + [c_name] )

def c_to_asm(base_name, optimise = '', library = False, debug = False, sys_headers = None):
    out_name = os.path.join(TEMPDIR, '%s%s' % (base_name, ASM_EXT))
    c_name = os.path.join(TEMPDIR, '%s.c' % base_name)
    args = CC_ARGS + [ASSEMBLER, optimise, OUTPUT_PATTERN % out_name, c_name]
    if sys_headers:
        args.append('-I%s' % sys_headers)
    if debug:
        args.append(DEBUG)
    if library:
        args.append(MOVABLE)
    return os.spawnv(os.P_WAIT, CC_PATH, args )
                    
def asm_to_object(base_name, debug = False):
    out_name = os.path.join(TEMPDIR, '%s%s' % (base_name, OBJ_EXT))
    asm_name = os.path.join(TEMPDIR, '%s%s' % (base_name, ASM_EXT))
    if debug:
        args =  CC_ARGS + [ DEBUG, OBJECT, OUTPUT_PATTERN % out_name , asm_name]
    else:
        args =  CC_ARGS + [ OBJECT, OUTPUT_PATTERN % out_name , asm_name]
    return os.spawnv(os.P_WAIT, CC_PATH, args )
                    
def link(name_list, out_name, library, verbose = False):
    obj_names = [ '%s%s' % (name, OBJ_EXT) for name in name_list ]
    if library:
        args =  CC_ARGS + [ LIB, DEBUG, OUTPUT_PATTERN % out_name] + obj_names
        if verbose:
            print >> sys.stderr, ' '.join(args)
        code = os.spawnv(os.P_WAIT, CC_PATH, args )
    else:
        args = [ LD_PATH, '-r', OUTPUT_PATTERN % out_name] +  obj_names
        if verbose:
            print >> sys.stderr, ' '.join(args)
        code = os.spawnv(os.P_WAIT, LD_PATH, args )
    return code
    
if not os.path.exists(TEMPDIR):
    os.makedirs(TEMPDIR)
