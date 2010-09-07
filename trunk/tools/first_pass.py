# Part of the llvm-based JIT tools.
# Generates the first pass in the compiler which creates the flow-graph.
# Used solely by gvmtcc.
import sys, common, gtypes, external
from delta import Unknown

class FirstPassMode(object):
    "LLVM First pass"

    def __init__(self, out, ftypes, glbls):
        self.out = out
        self.stream_offset = 1
        self.stream_stack = []
        self.n_args = []
        self.f_types = ftypes
        self.glbls = glbls
       
    def tload(self, tipe, index):
        pass
    
    def tstore(self, tipe, index, value):
        pass
    
    def constant(self, value):
        return value
    
    def address(self, name):
        self.glbls.add(name)
        
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
        
    def opcode(self):
        pass
        
    def ip(self):
        pass
        
    def next_ip(self):
        pass
    
    def comparison(self, tipe, left, op, right):
        pass

    def convert(self, from_tipe, to_type, value):
        pass
        
    def unary(self, tipe, op, arg):
        pass
        
    def sign(self, val):
        pass
                              
    def n_call(self, func, tipe, args):
        self.n_args.append(tipe.suffix)
        self.f_types.add(tuple(self.n_args))
        self.n_args = []
        
    def n_call_no_gc(self, func, tipe, args):
        self.n_call(func, tipe, args)
        
    def call(self, func, tipe):
        pass
        
    def c_call(self, func, tipe, pcount):
        pass
        
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
        offsets = {}
        for bb in graph: 
            e = bb.parent
            if e:
                if e not in offsets:
                    offsets[e] = self.stream_offset
                self.stream_offset = offsets[e]
            for i in bb:
                i.process(self)

            
    def get(self):
        if self.stream_stack:
            return self.stream_stack.pop()
        else:
            val = 'IP[%d]' % self.stream_offset
            self.stream_offset += 1
            return val
        
    def stream_fetch(self, size = 1):
        s = self.get()
        for i in range(1, size):
            value = self.get()
            s = '(%s << 8) | %s' % (s, value)
        return s
            
    def stream_peek(self, index):
        if index < len(self.stream_stack):
            return self.stream_stack[-index-1]
        else:
            return 'IP[%d]' % (self.stream_offset + index-len(self.stream_stack))
    
    def immediate_add(self, l, op, r):
        return '(%s%s%s)' % (l,op,r)
            
    def stream_push(self, value):
        assert value is not None
        self.stream_stack.append(value)
    
    def stack_drop(self, offset, size):
        pass
    
    def stack_pop(self, tipe=None):
        pass
        
    def stack_pick(self, tipe, index):
        pass
        
    def stack_poke(self, index, value):
        pass
    
    def stack_push(self, value):
        pass
              
    def stack_flush(self):
        pass
 
    def stack_insert(self, size, offset):
        pass
    
    def hop(self, index):
        pass
        
    def jump(self, offset):
        self.out << '  starts.push_back(IP - ip_start + ((int16_t)%s));\n' % offset
        self.out << '  end_of_block = true;\n'
       
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
        self.out << '  end_of_block = true;\n'
    
    def push_current_state(self):
        pass
    
    def pop_state(self):
        pass
    
    def push_state(self, value):
        pass
    
    def discard_state(self):
        pass
    
    def _raise(self, value):
        pass
    
    def transfer(self, value):
        pass
    
    def n_arg(self, tipe, val):
        self.n_args.append(tipe.suffix)

    def far_jump(self, addr):
        self.out << '  end_of_block = true;\n'
        
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
    
    def close(self):
       self.out << ' IP += %d;\n' % self.stream_offset
       self.stream_offset = 0
       
_START = '''
void Compiler::first_pass(int length, std::vector<int>& starts) {
    IP = ip_start;
    uint8_t* ip_end = ip_start + length;
    bool end_of_block = true;
    int join_depth = 0;
    stack_cache_size = join_depth;
    while(IP < ip_end) {
    if (end_of_block) {
        starts.push_back(IP - ip_start);
        end_of_block = false;   
    }    
    switch(*IP) {
'''

_NO_COMP = '''  case _gvmt_opcode_%s_%s: {
    char buf[100];
    sprintf(buf, "Illegal instuction \'%s\' in compiled code\\n");
    __gvmt_fatal(buf);
  }
'''

_END = '''
  default:
    char buf[100];
    sprintf(buf, "Illegal opcode %d\\n", *IP);
    __gvmt_fatal(buf);
  }
  }
}
    
'''
           
def first_pass(bytecodes, out):
    f_types = set()
    glbls = set()
    # Temporary globals for malloc
    glbls.add('gvmt_gc_free_pointer')
    glbls.add('gvmt_gc_limit_pointer')
    #end
    out << _START 
    for i in bytecodes.instructions:
        if i.name != '__preamble':
            continue
        out << '  {\n'
        mode = FirstPassMode(out, f_types, glbls)
        i.process(mode)
        mode.stack_flush()
        out << '}\n'
    for i in bytecodes.instructions:
        if 'nocomp' in i.qualifiers:
            if 'private' not in i.qualifiers:
                out << _NO_COMP % (bytecodes.func_name, i.name, i.name)
                continue
        elif 'private' in i.qualifiers:
            # Just want globals and f_types, discard output.
            mode = FirstPassMode(common.Buffer(), f_types, glbls)
        else:
            out << '  case _gvmt_opcode_%s_%s: {\n' % (bytecodes.func_name, i.name)
            mode = FirstPassMode(out, f_types, glbls)
            consume = i.flow_graph.deltas[1]
            if consume == Unknown:
                out << '        join_depth = 0;\n'
            else:
                out << '        join_depth -= %d;\n' % consume
                out << '        if (join_depth < 0) join_depth = 0;\n'   
        i.process(mode)
        produce = i.flow_graph.deltas[2]
        if 'private' not in i.qualifiers:
            out << '        join_depth += %d;\n' % produce
            out << '        if (stack_cache_size < join_depth) stack_cache_size = join_depth;\n'
            try:
                if mode.stream_offset != i.flow_graph.deltas[0] + 1:
                    print "%s %d %d\n" % (i.name, mode.stream_offset, 
                                          i.flow_graph.deltas[0])
                assert(mode.stream_offset == i.flow_graph.deltas[0] + 1)
                mode.close()
            except common.UnlocatedException, ex:
                raise common.UnlocatedException("%s in compound instruction %s"
                                                % (ex.msg, i.name))
            out << '} break;\n'
    out << _END
    for f in f_types:
        out << 'PointerType *PTR_FUNC_TYPE_%s;\n' % ''.join(f)
    out << 'class Globals {\n'
    out << ' public:\n'
    out << '    Globals(void);\n'
    for g in glbls:
        out << ' Value* %s;\n' % g
    out << '};\n\n'
    out << '    Globals::Globals(void) { }\n\n'
    out << '\n'
    out << 'Globals* make_globals(Module* module) {\n'
    out << '    Globals* g = new Globals();\n'
    for g in glbls:
        out << '    g->%s = new GlobalVariable(BaseCompiler::TYPE_I1, "", GlobalValue::ExternalWeakLinkage, 0, "%s", module);\n' % (g, g)
    out << '    return g;\n}\n\n'
    out << 'void Compiler::init_types(void) {\n'
    out << '    std::vector<const Type*> args;\n'
    for f in f_types:
        for a in f[:-1]:
            out << '    args.push_back(TYPE_%s);\n' % a
        out << '    PTR_FUNC_TYPE_%s = PointerType::get(FunctionType::get(TYPE_%s, args, false), 0);\n' % (''.join(f), f[-1])
        out << '    args.clear();\n'
    out << '    globals = make_globals(module);\n'
    out << '}\n\n'
    
