import builtin
from common import GVMTException

template = r"""
\subsubsection*{%s (%s $\Rightarrow$ %s)} 
%s
%s
"""

def texify(s):
    s = s.replace('_', '\\_').replace('#', '\\#')
    return s.replace('Nth', 'N\\textsuperscript{th}').replace('(0', '(N').replace('#0', '#N').replace('[0', '[N')

def stack(items):
    if items:
        s = ', '.join([to_str(x) for x in items])
        return texify(s)
    else:
        return '---'

def to_str(x):
    if isinstance(x, tuple):
        assert len(x) == 2
        n, t = x
        return '%s %s' % (t,n)
    else:
        return str(x)
        
def write_inst(i):
    if not i.__doc__:
        print 'No __doc__ for %s' % i.name
        import sys
        sys.exit(1)
    if i.operands or i.to_stream:
        if i.operands == 1:
            ops = r'\emph{1 operand byte. '
        elif i.operands > 1:
            ops = r'\emph{%d operand bytes. ' % i.operands
        else:
            ops = r'\emph{No operand bytes. '
        if i.to_stream == 1:
            ops += 'Pushes 1 byte to instruction stream.'
        if i.to_stream > 1:
            ops += 'Pushes %d bytes to instruction stream.' % i.to_stream
        ops += r'} \\'
    else:
        ops = ''
    print template % (texify(i.name), stack(i.inputs), stack(i.outputs), ops, texify(i.__doc__))

if __name__ == '__main__':
    insts = dict(builtin.instructions)
    names = [ n for n in builtin.instructions.keys() if not n.startswith('__') ] 
    for f in builtin.instruction_factories.values():
        try:
            i = f('X')
        except GVMTException:
            i = f('0')
        except ValueError:
            i = f('0')
        except TypeError:
            try:
                i = f('0', '0')
            except GVMTException:
                i = f('0', 'X')
        insts[i.name] = i
        names.append(i.name)
    for i in [ builtin.Immediate('#0'), builtin.ImmediateAdd('#+'),
               builtin.ImmediateAdd('#-'), builtin.PreFetch('#[0]') ]:
        insts[i.name] = i
        names.append(i.name)
    names.sort()
    print '''\section*{Introduction}
    This appendix lists all %d instructions of the GSC instruction set.''' % len(names)
    
    print r''' The GSC instruction set is not as large as it first appears. 
    Many of these are multiple versions of the form OP\_X where X can be any or all of the twelve different types.
    These types are I1, I2, I4, I8, U1, U2, U4, U8, F4, F8, P, R.
    
    IX, UX and FX refer to a signed integer, unsigned integer and floating point real of size (in bytes) X.
    P is a pointer and R is a reference. P pointers cannot point into the GC heap. R references are pointers that can 
    \emph{only} point into the GC heap.
    
    TOS is an abbreviation for top-of-stack and NOS is an abbreviation for next-on-stack.
    
'''
    
    print 'Each instruction is listed below in the form:'
    print template % (texify('Name'), stack(['inputs']), stack(['outputs']), r'\emph{Instruction stream effect} \\', texify('Description of the instruction'))
    print '\section*{The instructions}'
    for name in names:
        i = insts[name]
        write_inst(i)
