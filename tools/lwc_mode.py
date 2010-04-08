# This is work in-progress. Don't attempt to use it.

# Mode for generating lightweight compiler
# Analyses bytecodes
# Most of the work is deferred to c_mode for 
# generating instruction bodies

import common, sys_compiler, os
import sys, gtypes, operators
import builtin, ssa, getopt, gsc
from stacks import Stack, CachingStack
from c_mode import IMode
from external import ExternalMode
import lwc_analysis_mode, gvmtas, gc_inliner

class Expr(object):
    
    def load(out):
        return self.materialise(out).load(out)
        
    def store(value, out):
        return self.materialise(out).store(value, out)
        
    def branch(index, t_or_f, out):
        self.materialise(out).branch(t_or_f, out)
        
class Register(object):
    
    def materialise(out):
        return self
        
    def load(out):
        r = get_register()
        out << 'code = emit_reg_offset_load(%s, 0, %s, code);\n' % (self.index, r.index)
        return r
        
    def store(value, out):
        r = value.materialise()
        out << 'code = emit_reg_offset_store(%s, 0, %s, code);\n' % (self.index, r.index)

    def branch(index, t_or_f, out):
        if t_or_f:
            out << 'code = emit_branch_non_zero(%s, code);\n' % self.index
        else:
            out << 'code = emit_branch_zero(%s, code);\n' % self.index
            
class Constoffset(object):
    
    def materialise(out):
        r = get_register()
        assert self.reg.__class__ is Register
        out << 'code = emit_reg_const_add(%s, %s, %s, code);\n' % (self.reg.index, self.offset, r.index)
        return r
        
    def load(out):
        r = get_register()
        out << 'code = emit_reg_offset_load(%s, %s, %s, code);\n' % (self.reg.index, self.offset, r.index)
        return r
        
    def store(value, out):
        r = value.materialise()
        out << 'code = emit_reg_offset_store(%s, %s, %s, code);\n' % (self.reg.index, self.offset, r.index)

class Equal(object):
    
    def __init__(self, way, left, right):
        self.way = way
        self.left = left.materialise()
        self.right = right.materialise()
    
    #This should only be used in a branch.
    def materialise(out):
        dont_compile()
        
    def branch(index, t_or_f, out):
        out << 'add_hop(code, index);\n'
        if self.way ^ t_or_f:
            out << 'code = emit_branch_unequal(%s, %s, code);\n' % (self.left.index, self.right.index)
        else:
            out << 'code = emit_branch_equal(%s, %s, code);\n' % (self.left.index, self.right.index)

class IsZero(object):
    
    def __init__(self, way, val):
        self.way = way
        self.val = val.materialise()
    
    #This should only be used in a branch.
    def materialise(out):
        dont_compile()
        
    def branch(index, t_or_f, out):
        out << 'add_hop(code, index);\n'
        if self.way ^ t_or_f:
            out << 'code = emit_branch_nonzero(%s, %s, code);\n' % (self.left.index, self.right.index)
        else:
            out << 'code = emit_branch_zero(%s, %s, code);\n' % (self.left.index, self.right.index)
           
class LAddr(object):
    
    def __init__(self, name):
        self.offset = 'offsetof(struct gvmt_interpreter_frame, %s)' % name
        
    def materialise(out):
        r = get_register()
        out << 'code = emit_load_frame_address(%s, %s, code);\n' % (self.offset, r.index)
        
    def load(out):
        r = get_register()
        out << 'code = emit_load_from_frame(%s, %s, code);\n' % (self.offset, r.index)
        
    def store(value, out):
        r = value.materialise()
        out << 'code = emit_store_to_frame(%s, %s, code);\n' % (self.offset, r.index)
        
class LWC_CompilerMode(object):
    "LWC Compiler generator"
    
    def __init__(self, out, i_name):
        self.out = out
        self.i_name = i_name

    def declarations(self, out):
        pass
        
    def tload(self, tipe, index):
        global _uid
        _uid += 1
        self.out << 'LWC_Register r%s = load_temp_as_register();\n' % _uid
        #Return the value in tempN
        return Value('r%s' % _uid);
        
    def tstore(self, tipe, index, value):
        self.out << 'save_to_temp(%s, %s);\n' % (index, value)
        
    def ip(self):
        return Constant('IP');
        
    def ip(self):
        return Constant('GVMT_CURRENT_OPCODE');
        
    def next_ip(self):
        return Constant('(IP+%s)' % self.i_length)
        
    def constant(self, value):
        return Constant(str(value))
        
    def address(self, name):
        return Constant('((intptr_t)name)')
        
    def symbol(self, index):
        cant_compile()
        
    def pload(self, tipe, array):
        #Need to emit code here
        return array.load(self.out)
        
    def pstore(self, tipe, array, value):
        array.store(value, self.out)
        
    def rload(self, tipe, obj, offset):
        return ConstOffset(obj, offset).load(value, self.out)
        
    def rstore(self, tipe, obj, offset, value):
        ConstOffset(obj, offset).store(value, self.out)
       
    def binary(self, tipe, left, op, right):
        if _is_const(left) and _is_const(right):
            return Constant('%s%s%s' % (left, op.c_name, right))
        if op == operators.add:
            if tipe in (gtypes.p, gtypes.r, gtypes.uptr, gtypes.iptr):
                if _is_const(left):
                    return ConstOffset(right, left)
                elif _is_const(right):
                    return ConstOffset(left, right)
        elif op == operators.lsh and right in (0,1,2,3):
            l = left.materialise(self.out)
            r = self.get_register(self.out)
            self.out << 'code = emit_left_shift(%s, %s, %s, code);\n' % (l, right, r)
            return r
        cant_compile()
        
    def comparison(self, tipe, left, op, right):
        if _is_const(left) and _is_const(right):
            return Constant('(%s%s%s)' % (left, op.c_name, right))
        if op in (operators.eq, operators.ne):
            if left == 0
                return IsZero(op == operators.eq, right)
            elif right == 0:
                return IsZero(op == operators.eq, left)
            if tipe in (gtypes.iptr, gtypes.uptr):
                return Equal(op == operators.eq, left, right)
        cant_compile()
        
    def convert(self, from_tipe, to_type, value):
        if _is_const(value):
            return Constant('((%s)%s)' % (to_type, value)
        cant_compile()
        
    def unary(self, tipe, op, arg):
        if _is_const(arg):
            return Constant('(%s%s)' % (op.c_name, value)
        cant_compile()
        
    def sign(self, val):
        cant_compile()
        
    def zero(self, val):
        cant_compile()
        
    def n_call(self, func, tipe, args):
        cant_compile()
        
    def n_call_no_gc(self, func, tipe, args):
        cant_compile()
        
    def call(self, func, tipe):
        return func.call(pcount, self.out)
        
    def c_call(self, func, tipe, pcount):
        return func.c_call(pcount, self.out)
        
    def laddr(self, name):
        return LAddr(name)
        
    def rstack(self):
        self.stack.flush()
        r = self.get_register()
        self.out << 'emit_load_stack_pointer(%s);\n' % r
        return r
     
    def frame(self):
        cant_compile()
        
    def alloca(self, tipe, size):
        cant_compile()
        
    def gc_malloc(self, size):
        cant_compile()
        
    def extend(self, tipe, value):
        if _is_const(value):
            return Constant('((intptr_t)(%s)%s)' % (tipe.c_name, value))
        cant_compile()
        
    def gc_safe(self):
        #Need to emit test for gc_waiting
        # pload constant gc_waiting
        # branch if zero
        # call gc_safe_point()
        # end
        assert False, "To do"
    
    def compound(self, name, qualifiers, graph):
        for bb in graph:
            for i in bb:
                i.process(self)
        
    def top_level(self, name, qualifiers, graph):   
        for bb in graph:
            for i in bb:
                i.process(self)
        self.i_name = name
        
    def __get(self):
        if self.stream_stack:
            return self.stream_stack.pop()
        else:
            val = 'IP[%s]' % self.stream_offset
            self.stream_offset += 1
            return val
        
    def stream_fetch(self, size=1):
        s = self.__get()
        for i in range(1, size):
            value = self.__get()
            s = '(%s << 8) | %s' % (s, value)
        return Constant(s)
        
    def stream_peek(self, index):
        index += self.stream_offset
        return Constant('IP[%s]' % index)
        
    def stream_push(self, value):
        self.stream_stack.push(value)
        
    def immediate_add(self, l, op, r):
        return Constant('(%s%s%s)' % (l, op.c_name, r))
        
    def stack_drop(self, offset, size):
        self.stack.drop(offset, size, self.out)
        
    def stack_pop(self):
        return self.stack.pop(self.out)
        
    def stack_push(self, value):
        self.stack.push(value, self.out)
        
    def stack_pop_double(self):
        return self.stack.pop_double(self.out)
        
    def stack_push_double(self, value):
        assert(val.tipe.size == 8)
        return self.stack.push_double(val, self.out)
        
    def stack_pick(self, index):
        return self.stack.pick(int(index), self.out)
 
    def stack_flush(self):
        self.stack.flush_to_memory(self.out)
 
    def stack_insert(self, offset, size):
        return self.stack.insert(offset, size, self.out)
 
    def hop(self, index):
        self.stack.flush_to_memory(self.out)
        self.out << 'add_hop(code, %s);\n' % index
        self.out << 'code = emit_jump(code);\n'    
        
    def jump(self, offset):
        self.stack.flush_to_memory(self.out)
        self.out << 'add_jump(code, IP+%s);\n' % offset
        self.out << 'code = emit_jump(code);\n'
        
    def line(self, number):
        pass
    
    def name(self, index, name):
        pass
    
    def type_name(self, index, name):
        pass
        
    def file(self, name):
        pass
        
    def local_branch(self, index, condition, t_or_f):
        self.stack.flush_to_memory(self.out)
        self.out << condition.branch(index, t_or_f)
        
    def target(self, index):
        self.stack.flush_to_memory(self.out)
        self.out << 'add_target(code, %s);\n' % index
        
    def return_(self, type):
        self.out << 'code = emit_return(code);\n'
        
    def far_jump(self, addr):
        self.stack.flush_to_memory(self.out)
        self.out << 'code = emit_dealloca(sizeof(struct gvmt_frame) + sizeof(void*)*locals, code);\n'  
        self.out << 'code = emit_restore_registers(code);\n'
        self.out << 'add_far_jump(code, %s);\n' % addr
        self.out << 'code = emit_jump(code);\n'
        
    def protect(self):
        self.out << 'add_call(code, gvmt_protect);\n'
        self.out << 'free_register(gvmt_lwc_return_register);\n'
        self.out << 'code = emit_call_direct(code);\n'
        return Register('gvmt_lwc_return_register')
        
    def unprotect(self):
        self.out << 'add_call(code, gvmt_unprotect);\n'
        self.out << 'code = emit_call_direct(code);\n'

    def _raise(self, value):      
        cant_compile()
        
    def n_arg(self, tipe, val):
        cant_compile()
        
    def gc_free_pointer_store(self, value):
        cant_compile()
        
    def gc_limit_pointer_store(self, value):
        cant_compile()
        
    def gc_limit_pointer_load(self):
        cant_compile()
    
    def gc_free_pointer_load(self):
        cant_compile()

    def close(self):
        pass #???
    
        
class LWC_FunctionMode(IMode):
    
    def __init__(self, temp, externals, gc_name, name):
        IMode.__init__(self, temp, externals, gc_name, name)
        self.jump_index = 0
    
    def return_(self, type):
        self.stack.flush_to_memory(self.out)
        self.out << ' return -1;\n'
        self.stream_offset = 0
        
    def jump(self, offset):
        self.jump_index += 1
        self.stack.flush_to_memory(self.out)
        self.out << ' return %s;\n' % self.jump_index
        
    def _stream_item(self, index):
        return  self.operand_string % index
        
    def close(self):
        if self.stream_stack:
            raise _exception(
                  "Value(s) pushed back to stream at end of instruction")
        if self.return_tos:
            tos = self.stack_pop()
            self.stack.flush_to_memory(self.out)
            self.out << ' return %s;' % tos
        else:
            self.stack.flush_to_memory(self.out)
    
def generate_function(inst, out, externals, gc_name):
    analysis = lwc_analysis_mode.LwcAnalysisMode()
    inst.top_level(analysis)
    if not analysis.dont_compile:
        return 0
    cc = lwc_analysis_mode.get_best_cc(inst)
    buf = common.Buffer()
    mode = LWC_FunctionMode(buf, externals, gc_name, bytecodes.name)
    mode.return_tos = False
    #preamble:
    out << 'GVMT_CALL '
    if cc[1] == 'tos':
        out << 'GVMT_StackItem '
        mode.return_tos = True
    elif cc[1] == 'flow':
        out << 'int '
    else:
        out << 'void '
    out << 'gvmt_lwc_compiler_function_%s (' % inst.name
    params = cc[0]
    plist = []
    for p in params:
        if p.startswith('operand'):
            if p[-1] == 's':
                plist.append('uint8_t* operands')
                mode.operand_string = 'operands[%s]'
            else:
                plist.append('intptr_t %s' % p)
                mode.operand_string = 'operand%s'
        elif p == 'IP':
            plist.append('uint8_t* _gvmt_ip')
        else:
            assert p.startswith('stack')
            plist.append('void* %s' % p)
    out << ','.join(plist)
    out << ') {\n'
    out.no_line()
    buf.no_line()
    inst.top_level(mode)
    mode.stack_flush()
    buf.close()
    mode.close()
    mode.declarations(out);
    assert mode.jump_index == analysis.jumps
    out << buf
    if cc[1] == 'flow':
        out << 'return 0;'
    out << '}\n'
    return mode.ref_temps_max
    
if __name__ == '__main__':    
    opts, args = getopt.getopt(sys.argv[1:], '')
    # TO DO -- add options
    gc_name = 'copy_threads'
    if not args:
        common.print_usage({})
        sys.exit(1)
    src_file = gsc.read(common.In(args))
    if gc_name != 'none':
        gc_inliner.gc_inline(src_file, gc_name)
    bytecodes = src_file.bytecodes
    c_file = os.path.join(sys_compiler.TEMPDIR, '%s_lwc_funcs.c' % bytecodes.name)
    out = common.Out(c_file)
    out << '#include <alloca.h>\n'
    out << '#include "gvmt/internal/core.h"\n'
    out << 'register GVMT_StackItem* gvmt_sp asm ("esi");\n'
    out << 'register struct gvmt_interpreter_frame* gvmt_lwc_fp asm ("edi");\n'
    out << 'extern GVMT_Object gvmt_%s_malloc(GVMT_StackItem* sp, GVMT_Frame fp, uintptr_t size);\n' % gc_name
    out << '#define FRAME_POINTER gvmt_lwc_fp\n'
    mode = ExternalMode()
    for i in bytecodes.instructions:
        i.process(mode)
    externals = mode.externals   
    mode.declarations(out)
    tmp = common.Buffer()
    max_refs = 0
    for i in bytecodes.instructions:
        if 'private' in i.qualifiers and i.name not in ('__preamble', '__postamble'):
            continue
        mr = generate_function(i, tmp, externals, gc_name)
        if mr > max_refs:
            max_refs = 0
    out << '   struct gvmt_interpreter_frame { struct gvmt_frame frame;\n'
    out << '   GVMT_Object refs[%s];\n' % max_refs
    ref_locals = 0
    for t, n in bytecodes.locals:
        if t == 'object':
            ref_locals += 1
            out << '   GVMT_Object %s;\n' % n
    for t, n in bytecodes.locals:
        if t != 'object':
            if t in gvmtas.c_types:
                out << '   %s %s;\n' % (gvmtas.c_types[t], n)
            else:
                raise UnlocatedException("Unrecognised type '%s'" % t)
    out << '   };\n'             
    out << tmp