#Mode for calculating stack and stream usage of instruction sequences.
#Used by flow_graph, all other code should get delta information
# directly from the compound instruction
import sys, common

class StackOffset(object):
    '''Represents stack offset, either relative or absolute'''
       
    def __init__(self, absolute, value):
        self.absolute = absolute
        assert value.__class__ is int
        self.value = value
                    
    def __add__(self, i):
        assert i.__class__ is int
        val = self.value + i
        if self.absolute and val < 0:
            return StackOffset(True, 0)
        else:
            return StackOffset(self.absolute, val)
            
    def merge(self, other):
        if self.absolute and not other.absolute:
            return other
        elif other.absolute and not self.absolute:
            return self
        else:
            if self.value < other.value:
                return self
            else:
                return other
                
    def __str__(self):
        if not self.absolute:
            if self.value >= 0:
                return '+%d' % self.value
            else:
                return '%d' % self.value
        else:
            assert self.value >= 0
            return str(self.value)
                
class _UnknownType(object):
    
    def __add__(self, other):
        return self
    
    def __radd__(self, other):
        return self
    
    def __sub__(self, other):
        return self
    
    def __rsub__(self, other):
        return self
    
    def __or__(self, other):
        return self
    
    def __ror__(self, other):
        return self
       
    def __lshift__(self, other):
        return self
        
    def __str__(self):
        return 'unknown'
                      
Unknown = _UnknownType()

def delta_merge(d1, d2):
    stream1, consume1, produce1 = d1
    stream2, consume2, produce2 = d2
    if stream1 != stream2:
        raise common.UnlocatedException('Differing stream offsets at join point: %d and %d' % (stream1, stream2))
    if consume1 is Unknown:
        return d2
    if consume1 is Unknown:
        return d1
    return stream1, min(consume1, consume2), min(produce1, produce2)
    
def delta_concat(d1, d2):
    stream1, consume1, produce1 = d1
    stream2, consume2, produce2 = d2
    if consume1 == Unknown or consume2 == Unknown:
        return stream1 + stream2, Unknown, produce2
    consume = max(consume2 - produce1, consume1)
    produce = produce1 + produce2 - consume1 - consume2 + consume
    return stream1 + stream2, consume, produce

class DeltaMode(object):
    "Calculate stream and stack deltas"
    
    def set_consume(self):
        if -self.stack_offset.value > self.consume:
            self.consume = -self.stack_offset.value
                
    def __init__(self):
        self.offset = 0
        self.stream_offset = 0
        self.stack_offset = StackOffset(False, 0)
        self.consume = 0
        self.stream_stack = []
        
    def deltas(self):
        if self.stack_offset.absolute:
            return self.stream_offset, Unknown, self.stack_offset.value
        else:
            return self.stream_offset, self.consume, self.stack_offset.value+self.consume
        
    def tload(self, tipe, index):
        pass
    
    def tstore(self, tipe, index, value):
        pass
        
    def ip(self):
        pass
        
    def next_ip(self):
        pass
    
    def constant(self, value):
        return value
    
    def address(self, name):
        pass
    
    def symbol(self, index):
        pass
        
    def pload(self, tipe, array):
        pass
    
    def pstore(self, tipe, array, value):
        pass
        
    def rload(self, tipe, array, offset):
        pass
    
    def rstore(self, tipe, array, offset, value):
        pass
        
    def binary(self, tipe, left, op, right):
        pass
        
    def comparison(self, tipe, left, op, right):
        pass

    def convert(self, from_tipe, to_type, value):
        pass
        
    def unary(self, tipe, op, arg):
        pass
        
    def sign(self, val):
        pass
        
    def zero(self, val):
        pass
    
    def frame(self):
        pass
                       
    def n_call(self, func, tipe, args):
        self.stack_offset = StackOffset(True, 0)
                       
    def n_call_no_gc(self, func, tipe, args):
        self.stack_offset = StackOffset(True, 0)
        
    def call(self, func, tipe):
        self.stack_offset = StackOffset(True, 0)
        
    def c_call(self, func, tipe, pcount):
        self.stack_offset = StackOffset(True, 0)
        
    def laddr(self, name):
        pass
        
    def rstack(self):
        pass
        
    def alloca(self, tipe, count):
        pass
        
    def gc_malloc(self, size):
        pass
        
    def gc_malloc_fast(self, size):
        pass
        
    def extend(self, tipe, value):
        pass
               
    def gc_safe(self):
        pass
        
    def compound(self, name, qualifiers, graph):
        stream, consume, produce = graph.deltas
        self.stream_offset += stream
        if consume == Unknown:
            self.stack_offset = StackOffset(True, produce)
        else:
            assert produce.__class__ is int
            assert consume.__class__ is int
            self.stack_offset = StackOffset(self.stack_offset.absolute, 
                                produce-consume + self.stack_offset.value)
        self.set_consume()
        
    def get(self):
        if self.stream_stack:
            return self.stream_stack.pop()
        else:
            return Unknown
        
    def stream_fetch(self, size = 1):
        self.stream_offset += size
        s = self.get()
        for i in range(1, size):
            value = self.get()
            s = (s << 8) | value
        return s
            
    def stream_peek(self, index):
        if index < len(self.stream_stack):
            return self.stream_stack[-index-1]
        else:
            return Unknown
    
    def immediate_add(self, l, op, r):
        if op == '+':
            return l + r
        else:
            return l - r
            
    def stream_push(self, value):
        assert value is not None
        self.stream_offset -= 1
        self.stream_stack.append(value)
    
    def stack_drop(self, offset, size):
        self.stack_offset = StackOffset(True, 0)
        
#    def stack_permute(self, in_, out):
#        self.stack_offset += len(out) - len(in_)
    
    def stack_pop(self):
        self.stack_offset += -1
        self.set_consume()
        
    def stack_push(self, value):
        self.stack_offset += 1
    
    def stack_pick(self, value):
        pass

    def stack_pop_double(self):
        self.stack_offset += -2
        self.set_consume()
        
    def stack_push_double(self, value):
        self.stack_offset += 2

    def stack_flush(self):
        self.stack_offset = StackOffset(True, 0)
 
    def stack_insert(self, size, offset):
        'Note: size is never a build-time constant, offset maybe'
        try: 
            self.stack_offset = StackOffset(True, int(offset))
            assert int(offset) >= 0
        except TypeError:
            self.stack_offset = StackOffset(True, 0)
    
    def hop(self, index):
        pass
        
    def jump(self, offset):
        pass
        
    def line(self, number):
        pass
        
    def name(self, index, name):
        pass
    
    def type_name(self, index, name):
        pass        
        
    def file(self, name):
        pass
        
    def local_branch(self, index, condition, t_or_f):
        pass
        
    def target(self, index):
        pass
        
    def return_(self, type):
        pass
    
    def far_jump(self, value):
        pass
    
    def protect(self):
        pass
    
    def protect_pop(self):
        pass
    
    def protect_push(self, value):
        pass
    
    def unprotect(self):
        pass
    
    def _raise(self, value):
        pass
    
    def n_arg(self, tipe, val):
        pass
    
    def gc_free_pointer_store(self, value):
        pass
    
    def gc_limit_pointer_store(self, value):
        pass

    def gc_limit_pointer_load(self):
        pass
    
    def gc_free_pointer_load(self):
        pass
    
    def zero_memory(self, obj, size):
        pass
    
    def fully_initialised(self, obj):
        pass
    
    def lock(self, lock):
        pass
    
    def unlock(self, lock):
        pass

