#Used by C generating backends to handle symbols,
#avoiding duplicate and conflicting entries.
# Poor choice of name comes from the declarations it produces...
# extern xxx;

import sys, common

standard_symbols = set(('malloc', 'strcmp', 'gvmt_opcode_names')) 

class ExternalMode(object):
    "External declarations for C code."
    
    def __init__(self):
        self.externals = { }
        self.funcs = {}
        self.required = set()
        self.stack = []
        self.return_type = None
        self.n_args = []
        
    def deltas(self):
        pass
        
    def tload(self, tipe, index):
        pass
    
    def tstore(self, tipe, index, value):
        pass
    
    def constant(self, value):
        pass
    
    def address(self, name):
        # Need to ensure we have a declaration for this...
        self.required.add(name)
        return name
     
    def symbol(self, index):
        pass
        
    def pload(self, tipe, array):
        if array:
            if array not in self.externals:
                self.externals[array] = '%s %s' % (tipe.c_name, array)
    
    def pstore(self, tipe, array, value):
        if array:
            if array not in self.externals:
                self.externals[array] = '%s %s' % (tipe.c_name, array)
        
    def rload(self, tipe, array, offset):
        pass

    def field_is_null(self, is_null, array, offset):
        pass    
    
    def rstore(self, tipe, array, offset, value):
        pass
        
    def binary(self, tipe, left, op, right):
        pass
        
    def comparison(self, tipe, left, op, right):
        pass

    def convert(self, from_tipe, to_type, value):
        pass
        
    def ip(self):
        pass
        
    def pin(self, val):
        pass
    
    def pinned_object(self, val):
        pass
        
    def opcode(self):
        pass
        
    def next_ip(self):
        pass
        
    def unary(self, tipe, op, arg):
        pass
        
    def sign(self, val):
        pass
        
    def zero(self, val):
        pass

    def n_call(self, func, tipe, args):
        if func:
            if self.n_args:
                self.externals[func] = '%s %s(%s, ...)' % (tipe.c_name, func, self.n_args[0])
            else:
                self.externals[func] = '%s %s(void)' % (tipe.c_name, func)  
        self.n_args = []
                       
    def n_call_no_gc(self, func, tipe, args):                 
        self.n_call(func, tipe, args)
        
    def call(self, func, tipe):
        if func:
            self.externals[func] = 'GVMT_CALL GVMT_StackItem* %s(GVMT_StackItem* gvmt_sp, GVMT_Frame _gvmt_caller_frame)' % func

    def function(self, name, is_priv, type):
        self.externals[name] = 'GVMT_CALL GVMT_StackItem* %s(GVMT_StackItem* gvmt_sp, GVMT_Frame _gvmt_caller_frame)' % name
        self.funcs[name] = is_priv;
                       
    def c_call(self, func, tipe, pcount):
        self.call(func, tipe)
         
    def laddr(self, name):
        pass
        
    def rstack(self):
        pass
        
    def alloca(self, tipe, count):
        pass
        
    def gc_malloc_fast(self, size):
        pass
        
    def gc_malloc(self, size):
        pass
        
    def extend(self, tipe, value):
        pass
             
    def gc_safe(self):
        pass
        
    def compound(self, name, qualifiers, graph):
        for block in graph:
            for i in block:
                i.process(self)
        
    def stream_fetch(self, size = 1):
        pass
            
    def stream_peek(self, index):
        pass
    
    def immediate_add(self, l, op, r):
        pass
            
    def stream_push(self, value):
        pass
    
    def stack_pop(self, tipe=None):
        if self.stack:
            return self.stack.pop()
    
    def stack_pick(self, tipe, index):
        pass
        
    def stack_poke(self, index, value):
        pass
            
    def stack_push(self, value):
        self.stack.append(value)
          
    def stack_flush(self):
        self.stack = []
 
    def stack_insert(self, size, offset):
        self.stack = []
 
    def stack_drop(self, offset, size):
        self.stack = []
    
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
            
    def far_jump(self, value):
        pass
        
    def target(self, index):
        pass
        
    def return_(self, type):
        if self.return_type:
            if type != self.return_type:
                raise common.UnlocatedException("Conflicting return types, was '%s'," % self.return_type)
        else:
            self.return_type = type
        
    def declarations(self, out):
        for name in self.externals:
            self.required.discard(name)
        for name in self.required:
            self.externals[name] = 'void* %s' % name
        for name in self.externals:
            if name not in standard_symbols:
                private = name in self.funcs and self.funcs[name]
                if private:
                    out << 'static %s;\n' % self.externals[name]
                else:
                    out << 'extern %s;\n' % self.externals[name]
        
    def push_current_state(self):
        pass
    
    def discard_state(self):
        pass
    
    def _raise(self, value):
        pass
    
    def transfer(self, value):
        pass
    
    def n_arg(self, tipe, val):
        self.n_args.append(tipe.c_name)
    
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
       
    def lock_internal(self, obj, offset):
        pass
    
    def unlock_internal(self, obj, offset):
        pass

