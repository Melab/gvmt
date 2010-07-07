#!/usr/bin/python
import common
import sys, gvmtas, getopt, gsc, gtypes, operators
import builtin, ssa, gc_inliner, gc_optimiser
from stacks import Stack, CachingStack
from delta import Unknown
import os
import first_pass
from llvm_mode import LlvmPassMode

# Should this go in generated code somewhere?
LINKAGE = 'extern "C" int %s;\nint* compiler_linkage = &%s;\n' 
# % (cc.linkage(), cc.linkage())

HEADER = '''

using namespace llvm;

class Compiler : public BaseCompiler {
'''

PUBLIC = '''public:
    Compiler(void);
    void init_types(void);
    Function* compile(uint8_t *begin, uint8_t *end, char* name, bool jit);
    static Compiler* get_compiler(void);
'''

COMPILE_FUNCTIONS = '''
Compiler* Compiler::get_compiler(void) {
    static Compiler* compiler = 0;
    if (compiler) 
        return compiler;
    compiler = new Compiler;
    return compiler;
}
 
Function* Compiler::compile(uint8_t *begin, uint8_t *end, char* name, bool jit) {
    if (jit)
        jitting = true;
    else
        jitting = false;
    std::vector<int> starts;
    ip_start = begin;
    int length = end - begin;
//    fprintf(stderr, "First pass\\n");
    first_pass(length, starts);
    current_function = Function::Create(Architecture::FUNCTION_TYPE, 
                                        GlobalValue::ExternalLinkage,
                                        name, module);
    current_function->setCallingConv(%s);
    start_block = makeBB("__preamble");
    block_map.clear();
    for (std::vector<int>::iterator it=starts.begin(); it!=starts.end(); it++) {
        assert(*it >= 0 && *it < length);
        if (block_map.find(*it) == block_map.end()) {
//            fprintf(stderr, "Creating BB\\n");
            block_map[*it] = makeBB(name, *it);
        }
    }
    block_map[length] = 0;
//    fprintf(stderr, "Second pass\\n");
    second_pass(length);
    return current_function;
}
'''

def preamble(bytecodes, out):
    gvmtas.write_header(bytecodes, out)
#    out << '#include "gvmt/internal/core.h"\n'
    out << '#include "gvmt/internal/compiler.hpp"\n'
    for i in bytecodes.instructions:
        ops = i.flow_graph.deltas[0]
        formals = ', '.join([''] + ['int op%d' % j for j in range(ops)])
    out << HEADER
    #private methods
    for i in bytecodes.instructions:
        ops = i.flow_graph.deltas[0]
        formals = ', '.join(['int op%d' % j for j in range(ops)])
        out << '    bool compile_%s(%s);\n' % (i.name, formals)
    out << '  void first_pass(int length, std::vector<int>& starts);\n'
    out << '  void second_pass(int length);\n'
    for t, n in bytecodes.locals:            
        out << '   Value *laddr_%s;\n' %  n
    if bytecodes.func_name:
        name = bytecodes.func_name
    else:
        name = 'gvmt_interpreter'
    out << PUBLIC
    out << '};\n'
    out << COMPILE_FUNCTIONS % common.llvm_cc()

def _compile_body(inst, out, name):
    buf = common.Buffer()
    out << '#define GVMT_CURRENT_OPCODE _gvmt_opcode_%s_%s\n' % (name, inst.name)
    out << ' bool block_terminated = false;\n'
    mode = LlvmPassMode(buf, name)
    inst.top_level(mode)
    try:
        mode.close()
    except common.UnlocatedException, ex:
        raise common.UnlocatedException("%s in compound instruction %s" % (
                                        ex.msg, inst.name))
    mode.declarations(out)
    out << buf
    if mode.block_terminated:
        out << ' return true;\n'
    else:
        out << ' return block_terminated;\n'
    out << '#undef GVMT_CURRENT_OPCODE\n'

def functions(bytecodes, out):
    for i in bytecodes.instructions:
        if 'nocomp' in i.qualifiers:
            continue
        ops = i.flow_graph.deltas[0]
        formals = ', '.join(['int op%d' % j for j in range(ops)])
        out << 'bool Compiler::compile_%s(%s) {\n' % (i.name, formals)
        _compile_body(i, out, bytecodes.func_name)
        out << '}\n' 
        
def constructor(bytecodes, out, gc_name):
    if bytecodes.func_name:
        name = bytecodes.func_name
    else:
        name = 'gvmt_interpreter'
    out << '''Compiler::Compiler() {
    init_types();
    std::vector<const Type*>args;
    args.push_back(POINTER_TYPE_P);
    args.push_back(TYPE_P);
    args.push_back(TYPE_I4);
    FunctionType* ftype = FunctionType::get(TYPE_R, args, false);
    GC_MALLOC_FUNC = Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_%s_malloc", module);
}  
'''% gc_name

SECOND_PASS_INIT = '''
extern uintptr_t gvmt_interpreter_%s_locals; 
extern uintptr_t gvmt_interpreter_%s_locals_offset;
  
void Compiler::second_pass(int length) {
    IP = ip_start;
    uint8_t* ip_end = ip_start + length;
    bool block_terminated = true;
    std::map<int, BasicBlock*>::iterator it = block_map.begin();    current_block = it->second;
    current_block = start_block;
    stack = new CompilerStack(module);
    Function::arg_iterator register_args = current_function->arg_begin();
    Value* incoming_sp = register_args++;
    incoming_sp->setName("sp_in");
    Value* incoming_fp = register_args++;
    incoming_fp->setName("fp_in");
    stack->store_pointer(incoming_sp, current_block);
    initialiseFrame(incoming_fp, gvmt_interpreter_%s_locals_offset+%d,
                    gvmt_interpreter_%s_locals_offset+ %d, current_block);
    assert(it->first == 0);
    Value* _offset;
    unsigned int locals_offset = offsetof(struct gvmt_frame, refs) + 
                                 gvmt_interpreter_%s_locals_offset * sizeof(void*);
    stack->flush(current_block);
'''

LOOP_START = '''
    do {
      ++it;
      uint8_t* next_block_start = it->first + ip_start;
      while(IP < next_block_start) {
        switch(*IP) {
'''

LOOP_END = '''        default:
            char buf[32];
            sprintf(buf, "Illegal opcode %d\\n", *IP);
            __gvmt_fatal(buf);
        }
      }
      stack->join_depth = 0;
      stack->flush(current_block);
      if (!block_terminated) {
          if (IP >= ip_end) {
              fprintf(stderr, "Error: Execution may reach end of compiled code\\n");
              abort();
          } else {
              assert(it->second);
              BranchInst::Create(it->second, current_block);
              block_terminated = true;
          }    
      }
      current_block = it->second;
    } while (current_block);
'''

_OFFSET = '    _offset = ConstantInt::get(APInt(32, locals_offset + %d));\n'
L_ADDR = '''    laddr_%s  = GetElementPtrInst::Create(FRAME, _offset,
                                 "laddr_%s", current_block);
'''
    
def second_pass(bytecodes, out):
    ref_locals_count = 0
    for t, n in bytecodes.locals:
        if t == 'object':
            ref_locals_count += 1  
    n = bytecodes.func_name
    out << SECOND_PASS_INIT % (n, n, n, len(bytecodes.locals), n, ref_locals_count, n)
    out << '    stack->max_join_depth(stack_cache_size, current_block);\n'
    out << '    stack->join_depth = 0;\n'
    offset = 0
    for t, n in bytecodes.locals:
        if t == 'object':
            out << _OFFSET % offset
            offset += gtypes.p.size
            out << L_ADDR % (n, n)
    for t, n in bytecodes.locals:
        if t != 'object':
            out << _OFFSET % offset
            offset += gtypes.p.size
            out << L_ADDR % (n, n)
    for i in bytecodes.instructions:
        if i.name == '__preamble':
            out << '    compile___preamble();\n'
    out << '    BranchInst::Create(it->second, current_block);\n'
    out << '    current_block = it->second;\n'
    out << LOOP_START
    for i in bytecodes.instructions:
        if 'private' in i.qualifiers or 'nocomp' in i.qualifiers:
            continue
        out << '  case _gvmt_opcode_%s_%s:\n' % (bytecodes.func_name, i.name)
        consume = i.flow_graph.deltas[1]
        if consume == Unknown:
            out << '        stack->join_depth = 0;\n'
        else:
            out << '        stack->join_depth -= %d;\n' % consume
            out << '        if (stack->join_depth < 0) stack->join_depth = 0;\n' 
        ops = i.flow_graph.deltas[0]
        args = ', '.join(['IP[%d]' % (j+1) for j in range(ops)])
        out << '        block_terminated = compile_%s(%s);\n' % (i.name, args)
        out << '        IP += %d;\n' % (ops+1)
        produce = i.flow_graph.deltas[2]
        out << '        stack->join_depth += %d;\n' % produce
        out << '    break;\n'
    out << LOOP_END
    out << '}\n'
    
EXTERN_C = '''
extern "C" {
    void* gvmt_compile_jit(uint8_t* begin, uint8_t* end, char* name) {
        Compiler* c = Compiler::get_compiler();
        Function* f = c->compile(begin, end, name, true);
        void* result = c->jit_compile(f, name);
        gvmt_last_return_type = RETURN_TYPE_P;
        return result;
    }
    
    void gvmt_compile(uint8_t* begin, uint8_t* end, char* name) {
        Compiler::get_compiler()->compile(begin, end, name, false);
        gvmt_last_return_type = RETURN_TYPE_V;
    }
    
    void gvmt_write_ir(void) {
        Compiler::get_compiler()->write_ir();
    }
}
'''
  
options = {
    'h' : "Print this help and exit",
    'o outfile-name' : 'Specify output file name',
    'm memory_manager' : 'Memory manager (garbage collector) used'
}         
          
if __name__ == '__main__':   
    opts, args = getopt.getopt(sys.argv[1:], 'ho:m:')
    outname = None
    xfile = None
    gc_name = 'none'
    for opt, value in opts:
        if opt == '-h':
            common.print_usage(options, 'interpreter-defn(.gsc)')
            sys.exit(0)
        elif opt == '-o':
            outname = value
        if opt == '-m':
            gc_name = value
    if len(args) != 1:
        common.print_usage(options, 'interpreter-defn(.gsc)')
        sys.exit(1)      
    if not outname:
        directory, filename = os.path.split(args[0])
        outname = os.path.join(directory, filename.split('.')[0] +
                                   '.compiler.cpp')
    src_file = gsc.read(common.In(args))
    src_file.make_unique()
    out = common.Out(outname)
#    Don't inline as llvm doesn't support TLS.
#    Inlined version would be SLOWER than call!
    if gc_name != 'none':
        gc_optimiser.gc_optimise(src_file)
        gc_inliner.gc_inline(src_file, gc_name + "_llvm")
    preamble(src_file.bytecodes, out)
    constructor(src_file.bytecodes, out, gc_name)
    first_pass.first_pass(src_file.bytecodes, out)
    functions(src_file.bytecodes, out)
    second_pass(src_file.bytecodes, out)
    out << EXTERN_C
    out.close()

    
