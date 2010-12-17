
import common
import sys, gtypes, operators
from stacks import Stack, CachingStack
import delta

_CONST = object()
_COMPARE = object()

def _is_const(o):
    return o is _CONST or isinstance(o, int)

class LwcAnalysisMode(object):
    """LWC Analysis mode. Determines suitability of compiling a bytecode compared
    to calling it."""
   
    def __init__(self):
        self.dont_compile = False
        self.uses_fp = False
        self.uses_ip = False
        self.jumps  = 0
        self.returns = False
        self.farjumps = False
        self.stack = []
        
    def tload(self, tipe, index):
        pass
    
    def tstore(self, tipe, index, value):
        if value == _COMPARE:
            self.dont_compile = True
    
    def ip(self):
        self.uses_ip = True
        return _CONST
        
    def opcode(self):
        return _CONST
        
    def next_ip(self):
        self.uses_ip = True
        return _CONST
        
    def constant(self, value):
        return value
    
    def address(self, name):
        return _CONST
    
    def symbol(self, index):
        self.dont_compile = True
    
    def pload(self, tipe, array):
        if array == _COMPARE:
            self.dont_compile = True
        
    def pstore(self, tipe, array, value):
        if array == _COMPARE or value == _COMPARE:
            self.dont_compile = True
        
    def rload(self, tipe, obj, offset):
        pass
        
    def field_is_null(self, is_null, array, offset):
        pass
    
    def rstore(self, tipe, obj, offset, value):
        if value == _COMPARE:
            self.dont_compile = True      
    
    def binary(self, tipe, left, op, right):
        if left == _COMPARE or left == _COMPARE:
            self.dont_compile = True
        if _is_const(left) and _is_const(right):
            return _CONST
        if op == operators.add:
            if tipe in (gtypes.p, gtypes.r, gtypes.uptr, gtypes.iptr):
                return
        elif op == operators.lsh and right in (0,1,2,3):
            return
        self.dont_compile = True
            
    def comparison(self, tipe, left, op, right):
       if left == _COMPARE or left == _COMPARE:
            self.dont_compile = True
       if _is_const(left) and _is_const(right):
            return _CONST
       if op in (operators.eq, operators.ne):
            if left == 0 or right == 0:
                return _COMPARE
            if tipe in (gtypes.iptr, gtypes.uptr):
                return _COMPARE     
       self.dont_compile = True
        
    def convert(self, from_tipe, to_type, value):
        if value == _COMPARE:
            self.dont_compile = True
        if _is_const(value):
            return _CONST
        self.dont_compile = True
    
    def unary(self, tipe, op, arg):
        if arg == _COMPARE:
            self.dont_compile = True
        if _is_const(arg):
            return _CONST
        self.dont_compile = True
        
    def sign(self, val):
        self.dont_compile = True
        
    def zero(self, val):
        self.dont_compile = True
        
    def n_call(self, func, tipe, args):
        self.dont_compile = True
        self.stack = []
        
    def n_call_no_gc(self, func, tipe, args):
        self.dont_compile = True
        self.stack = []
        
    def call(self, func, tipe):
        self.stack = []
        
    def c_call(self, func, tipe, pcount):
        self.stack = []
        
    def laddr(self, name):
        self.uses_fp = True
        
    def rstack(self):
        pass
    
    def alloca(self, tipe, size):
        self.dont_compile = True
        
    def gc_malloc(self, size):
        self.dont_compile = True
        
    def gc_malloc_fast(self, size):
        self.dont_compile = True
        
    def extend(self, tipe, value):
        if value == _COMPARE:
            self.dont_compile = True
        if _is_const(value):
            return _CONST
        self.dont_compile = True
        
    def gc_safe(self):
        pass
        
    def compound(self, name, qualifiers, graph):
        new_mode = LwcAnalysisMode()
        for bb in graph:
            for i in bb:
                i.process(new_mode)
        self.dont_compile |= new_mode.dont_compile
        self.uses_fp |= new_mode.uses_fp
        self.uses_ip |= new_mode.uses_ip
        self.jumps += new_mode.jumps 
        self.returns |= new_mode.returns
        self.stack = []
        
    def top_level(self, name, qualifiers, graph):
        #Entry point
        for bb in graph:
            for i in bb:
                i.process(self)
        
    def stream_fetch(self, size=1):
        return _CONST
        
    def stream_push(self, value):
        pass
    
    def stream_peek(self, index):
        return _CONST
        
    def immediate_add(self, l, op, r):
        return _CONST
    
    def stack_drop(self, offset, size):
        self.stack = []

    def stack_pop(self, tipe=None):
        if self.stack:
            return self.stack.pop()
    
    def stack_push(self, value):
        self.stack.append(value)
    
    def stack_pick(self, tipe, index):
        pass
        
    def stack_poke(self, index, value):
        pass
    
    def stack_flush(self):
        pass
    
    def stack_insert(self, offset, size):
        self.stack = []
    
    def hop(self, index):
        self.stack = []
    
    def jump(self, offset):
        self.jumps += 1
        self.stack = []
        
    def line(self, number):
        pass
    
    def name(self, index, name):
        pass
    
    def type_name(self, index, name):
        pass
        
    def file(self, name):
        pass
    
    def local_branch(self, index, condition, t_or_f):
        self.stack = []
    
    def target(self, index):
        self.stack = []
    
    def return_(self, type):
        self.returns = True
        self.stack = []
    
    def far_jump(self, addr):
        # Easy in the compiler than in a call...
        self.farjumps = True
        self.stack = []
        
    def push_current_state(self):
        self.stack = []
        
    def discard_state(self):
        self.stack = []
        
    def _raise(self, value):
        self.dont_compile = True
        self.stack = []
    
    def transfer(self, value):
        self.dont_compile = True
        self.stack = []
        
    def n_arg(self, tipe, val):
        pass
    
    def gc_free_pointer_store(self, value):
        self.dont_compile = True
        
    def gc_limit_pointer_store(self, value):
        self.dont_compile = True
        
    def gc_limit_pointer_load(self):
        pass
    
    def gc_free_pointer_load(self):
        pass

    def close(self):
        pass
    
# Need to tie this to architecture
machine_register_parameters = 2
    
def get_best_cc(inst):
    mode = LwcAnalysisMode()
    inst.top_level(mode)
    params = []
    deltas = inst.flow_graph.deltas
    if mode.uses_ip:
        params = [ 'IP' ]
    if deltas[1] is delta.Unknown:
        if deltas[0] + len(params) > machine_register_parameters:
            params.append('operands')
        else:
            for i in range(deltas[0]):
                params.append('operand%d' % i)
    else:
        if deltas[0] + deltas[1] + len(params) > machine_register_parameters:
            params.append('operands')
            for i in range(deltas[1]):
                if len(params) == machine_register_parameters:
                    break
                params.append('stack%d' % i)
        else:
            for i in range(deltas[0]):
                params.append('operand%d' % i)
            for i in range(deltas[1]):
                params.append('stack%d' % i)
    if mode.returns or mode.farjumps or mode.jumps:
        return (tuple(params), 'flow')
    elif deltas[2]:
        return (tuple(params), 'tos')
    else:
        return (tuple(params), 'None')
  
if __name__ == '__main__':
    import getopt
    opts, args = getopt.getopt(sys.argv[1:], 'p')
    if opts:
        show_private = True
    else:
        show_private = False
    if len(args) != 1:
        print ("Usage: %s gsc_file")
    infile = common.In(args[0])
    import gsc
    gfile = gsc.read(infile)
    if not gfile.bytecodes:
        print "Not an interpreter file!"
        sys.exit(1)
    inst = gfile.bytecodes.instructions
    for i in inst:
        if 'private' in i.qualifiers and not show_private:
            continue
        print '%s:' % i.name
        mode = LwcAnalysisMode()
        i.top_level(mode)
        if mode.dont_compile:
            print '    Compile as call'
            if mode.uses_fp:
                print '    Uses FP'
            if mode.uses_ip:
                print '    Uses IP'
            print '    Deltas: %s %s %s' % i.flow_graph.deltas
        else:
            print '    Compile directly'       
        if mode.jumps:
            print '    Includes %d jump(s)' % mode.jumps
        if mode.farjumps:
            print '    Includes farjump(s)'
        if mode.returns:
            print '    Includes return(s)'
        #From analysis determine CC.
        if mode.dont_compile:
            print get_best_cc(i)
        print
        print

