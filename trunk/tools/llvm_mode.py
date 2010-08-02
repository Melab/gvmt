# Mode for generating LLVM-generating code.
# It generates C++ code to generate LLVM-IR at runtime for the 
# LLVM back-end to JIT compile.
# Note that this mode does NOT generate LLVM-IR directly,
# although a mode to do that might be nice.

import common
import gtypes, operators
import builtin, ssa
from stacks import Stack, CachingStack

# Helper functions

def const(val):
    if isinstance(val, int):
        return str(val)
    else:
        return val.const()
        
def no_cast(t1, t2):
    if t1 is t2:
        return True
    elif t1.is_int() and t2.is_int() and t1.size == t2.size:
        return True
    else:
        return False
        
def llvm_stack_save(out):
    out << ''' 
    Value* stack_save_func = module->getOrInsertFunction("llvm.stacksave", BaseCompiler::TYPE_P, NULL);
    Value *params_stack_state[] = { 0, 0 };
    Value *stack_state = CallInst::Create(stack_save_func, 
            &params_stack_state[0], &params_stack_state[0], "", current_block);
'''

def llvm_stack_restore(out):
    out << ''' 
    Value* stack_restore_func = module->getOrInsertFunction("llvm.stackrestore", BaseCompiler::TYPE_V, BaseCompiler::TYPE_P, NULL);
    params_stack_state[0] = stack_state;
    CallInst::Create(stack_restore_func, 
            &params_stack_state[0], &params_stack_state[1], "", current_block);
'''

       
# LLVM operator name
def _op_name(op, tipe):
    if tipe.is_signed():
        return op.llvm_signed
    elif tipe.is_unsigned() or tipe in [ gtypes.p, gtypes.r]:
        return op.llvm_unsigned
    elif tipe in [gtypes.f4, gtypes.f8]:
        return op.llvm_float
    else:
        assert False

class LlvmStack(Stack):
    
    def __init__(self, declarations):
        self.reg_index = 0
        self.dreg_index = 0
        self.declarations = declarations
        
    def registers(self):
        return self.reg_index
        
    def dregisters(self):
        return self.dreg_index
        
    def push(self, value, out):
        assert isinstance(value, Expr)
        out << ' stack->push(%s);\n' % (value)
        
    def pop(self, out):
        self.reg_index += 1
        out << ' Value *gvmt_r%d = %s;\n' % (self.reg_index, Simple(gtypes.x, 
                                             'stack->pop(current_block)'))
        return Simple(gtypes.x, 'gvmt_r%d' % self.reg_index)    
        
    def pick(self, index, out):
        self.reg_index += 1
        out << ' Value *gvmt_r%d = %s;\n' % (self.reg_index, Simple(gtypes.x, 
                            'stack->pick(current_block, %s)' % const(index)))
        return Simple(gtypes.x, 'gvmt_r%d' % self.reg_index)
        
    def poke(self, index, value, out):
        out << ' stack->poke(current_block, %s, %s);\n' % (index, value)
        
    def flush_to_memory(self, out):
        out << ' stack->flush(current_block);\n'
        
    def join(self, out):
        out << ' stack->join(current_block);\n'
            
    def drop(self, offset, size, out):
        drop_fmt = ' stack->drop(%s, %s, current_block);\n'
        out << drop_fmt % (const(offset), size.cast(gtypes.i4))

    def insert(self, offset, size, out):
        self.reg_index += 1
        out << ' Value *gvmt_r%d = stack->insert(%s, %s, current_block);\n' % (
                self.reg_index, const(offset), size.cast(gtypes.i4))
        return Simple(gtypes.p, 'gvmt_r%d' % self.reg_index)
        
    def top(self, out, cached = 0):
        self.reg_index += 1
        out << ' Value *gvmt_r%d = stack->top(current_block, %s);\n' % (
                self.reg_index, cached)
        return Simple(gtypes.p, 'gvmt_r%d' % self.reg_index)
        
    def flush_cache(self, out):
        pass
        
    def comment(self, out):
        pass
    
    def store(self, out):
        pass
    
    def pop_double(self, out):
        self.reg_index += 1
        out << ' Value *gvmt_r%d = %s;\n' % (self.reg_index, Simple(gtypes.x2, 
                                           'stack->pop_double(current_block)'))
        return Simple(gtypes.x2, 'gvmt_r%d' % self.reg_index)
        
    def push_double(self, value, out):
        out << ' stack->push_double(%s);\n' % (value)
        
#Log (base 2) values (up to 8)    
_log2 = (  None, 0, 1, None, 2, None, None, None, 3) 
        
# Expression classes        
        
class Expr(object):
    
    def __init__(self, tipe):
        assert tipe
        self.tipe = tipe
    
    def __int__(self):
        raise ValueError
        
    def __div__(self, other):
        if other == 1:
            return self
        elif other in (2,4,8) and self.tipe.is_int():
            log = _log2[other]
            return Binary(self.tipe, self, operators.rsh, 
                          Constant(gtypes.i4, log).cast(self.tipe))
        else:
            return Binary(self.tipe, self, operators.div, 
                          Constant(gtypes.i4, other).cast(self.tipe))
        
    def pcast(self, tipe):
        return 'new BitCastInst(%s, POINTER_TYPE_%s, "", current_block)' % (
                self.cast(gtypes.p), tipe.suffix)
        
    def pstore(self, tipe, value):
        return ' new StoreInst(%s, %s, current_block);\n' % (value.cast(tipe), 
                                                             self.pcast(tipe))
        
    def load(self, tipe):
        return Simple(tipe, 'new LoadInst(%s, "", current_block)' % 
                      self.pcast(tipe))
      
    def const(self):
        "Returns a compile-time constant expression (string)"
        raise Exception("Not a compile-time constant")
      
    def cast(self, tipe):
        if no_cast(self.tipe, tipe):
            return self
        else:
            return self._cast(tipe)
            
    def _cast(self, tipe):
        return Cast(tipe, self)
        
    def call(self, params, tipe, pcount):
        # Type must be a pointer type.
        fp = self.cast(gtypes.p)
        func = ('new BitCastInst(%s, Architecture::POINTER_FUNCTION_TYPE,'
                ' "", current_block)') % fp
        call_fmt = 'CallInst::Create(%s, &%s[0], &%s[%s], "", current_block)'
        return Simple(gtypes.p, call_fmt % (func, params, params, pcount))
        
    def n_call(self, tipe, ftypes, params, pcount):
        # Type must be a pointer type.
        fp = self.cast(gtypes.p)
        cast_fmt = 'new BitCastInst(%s, PTR_FUNC_TYPE_%s%s, "", current_block)'
        func = cast_fmt % (fp, ''.join(ftypes), tipe.suffix)
        call_fmt = 'CallInst::Create(%s, &%s[0], &%s[%s], "", current_block)'
        return Simple(tipe, call_fmt % (func, params, params, pcount))
        
    def store(self, decl, out):
        return self
        
    def binary(self, tipe, op, right):
        assert tipe != gtypes.r 
        if op == operators.add and self.tipe in [gtypes.p, gtypes.r]:
            return PointerAdd(self, op, right)
        elif (op == operators.sub and self.tipe in [gtypes.p, gtypes.r] 
                and right.tipe.is_int()):
            #Negate right, then treat as pointer addition.
            right = Binary(right.tipe, Constant(right.tipe, 0), op, right)
            return PointerAdd(self, op, right)
        elif op == operators.add and right.tipe in [gtypes.p, gtypes.r]:
            return PointerAdd(right, op, self)
        elif tipe == gtypes.p:
            assert (self.tipe in [gtypes.iptr, gtypes.uptr] and 
                    right.tipe in [gtypes.iptr, gtypes.uptr])
            return Binary(gtypes.iptr, self, op, right).cast(gtypes.p)
        else:
            return Binary(tipe, self.cast(tipe), op, right.cast(tipe))
        
class Cast(Expr):
    
    def __init__(self, tipe, expr):
        Expr.__init__(self, tipe)
        self.expr = expr   
        assert expr.tipe.same_kind(tipe) or expr.tipe.size == tipe.size or (
           expr.tipe in [gtypes.b, gtypes.i1, gtypes.i2, gtypes.u1, gtypes.u2] 
           and tipe.is_int()) or tipe is gtypes.b
            
    def cast(self, tipe):
        if tipe == self.tipe:
            return self
        elif tipe == self.expr.tipe:
            return self.expr
        else:
            return Cast(tipe, self.expr)
        
    # This is a bit inelegant  -- Use tables to tidy up.
    def __str__(self):
        frm = self.expr.tipe
        to = self.tipe
        if no_cast(to, frm):
            return str(self.expr)
        if frm == gtypes.x:
            assert to.size == frm.size or to is gtypes.b
            if to == gtypes.u4:
                to = gtypes.i4
            return 'cast_to_%s(%s, current_block)' % (to.suffix, self.expr)
        elif frm == gtypes.x2:
            assert False
        if to in [gtypes.p, gtypes.r] and frm in [gtypes.p, gtypes.r]:
            return str(self.expr)
        elif to == gtypes.b:
            if frm in [gtypes.p, gtypes.r]:
                null = 'ConstantPointerNull::get(TYPE_%s)' % frm.suffix
                return 'new ICmpInst(ICmpInst::ICMP_NE, %s, %s, "", current_block)' % (null, self.expr)
            elif frm.is_int():
                return 'new ICmpInst(ICmpInst::ICMP_NE, %s, ZERO, "", current_block)' % self.expr
            else:
                assert frm.is_float()
                f0 = 'ConstantFP::get(APFloat(0.000000e+00f))'
                return 'new FCmpInst(FCmpInst::FCMP_UNE, %s, %s, "", current_block)' % (self.expr, f0)
        elif frm == gtypes.b:
            assert to.is_int()
            zext_fmt = 'new ZExtInst(%s, TYPE_%s, "", current_block)'
            return zext_fmt % (self.expr, to.suffix)
        elif to.size == frm.size:
            # Casting between different types of same size.
            if to in [gtypes.p, gtypes.r]:               
                if frm.is_float():
                    #Casting from float to pointer???
                    tmp_int = 'new BitCastInst(%s, TYPE_I%d, "", current_block)' % (self.expr, to.size)
                    return 'new IntToPtrInst(%s, TYPE_%s, "", current_block)' % (tmp_int, to.suffix)
                else:
                    if to == gtypes.p:
                        return 'cast_to_P(%s,current_block)' % self.expr
                    else:
                        return 'cast_to_R(%s,current_block)' % self.expr
            elif frm in [gtypes.p, gtypes.r]:
                if to.is_float():
                    #Casting from pointer to float???
                    tmp_int = 'new PtrToIntInst(%s, TYPE_I%d, "", current_block)' % (self.expr, to.size)
                    return 'new BitCastInst(%s, TYPE_%s, "", current_block)' % (tmp_int, to.suffix)
                else:
                    return 'new PtrToIntInst(%s, TYPE_%s, "", current_block)' % (self.expr, to.suffix)
            else:
                return 'new BitCastInst(%s, TYPE_%s, "", current_block)' % (self.expr, to.suffix)
        else:
            if to.size > frm.size:
                if frm.is_signed():
                    return 'new SExtInst(%s, TYPE_%s, "", current_block)' % (self.expr, to.suffix)
                elif frm.is_unsigned():
                    return 'new ZExtInst(%s, TYPE_%s, "", current_block)' % (self.expr, to.suffix)
                else:
                    assert to is gtypes.f8 and frm is gtypes.f4
                    return 'new FPExtInst(%s, Type::DoubleTy, "", current_block)' % self.expr
            else:
                if to.is_float():
                    return 'new FPTruncInst(%s, Type::FloatTy, "", current_block)' % self.expr
                else:
                    assert to.is_int()
                    return 'new TruncInst(%s, TYPE_%s, "", current_block)' % (self.expr, to.suffix)
 
class Simple(Expr):

    def __init__(self, tipe, txt):
        Expr.__init__(self, tipe)
        assert isinstance(txt, str)
        self.txt = txt
        
    def __str__(self):
        return self.txt
        
    def __int__(self):
        raise ValueError
 
class Constant(Simple):
    
    def __init__(self, tipe, val):
        if tipe.is_signed():
            Simple.__init__(self, tipe, 'ConstantInt::get(APInt(%d, %s, true))'
                            % (tipe.size*8, val))
        else:
            assert tipe.is_unsigned()
            Simple.__init__(self, tipe, 'ConstantInt::get(APInt(%d, %s))'
                            % (tipe.size*8, val))
        self.val = val
        
    def const(self):
        return str(self.val)
        
    def __int__(self):
        return int(self.val)
        
    def __div__(self, other):
        assert isinstance(other, int)
        try:
            return Constant(self.tipe, '%d' % (int(self.val) / other))
        except ValueError:
            return Constant(self.tipe, '%s/%d' % (self.val, other))
        
    def __str__(self):
        return self.txt
        
    def binary(self, tipe, op, right):
        if right.__class__ == self.__class__:
            return Constant(tipe, '(%s%s%s)' % (self.val, op.c_name, right.val))
        else:
            return Expr.binary(self, tipe, op, right)
            
    def _cast(self, tipe):
        if tipe.is_int():
            return Constant(tipe, '((%s)%s)' % (tipe.c_name, self.val))
        else:
            return Cast(tipe, self)

class Binary(Simple):
    
    def __init__(self, tipe, left, op, right):
        Expr.__init__(self, tipe)
        assert isinstance(left, Expr)
        assert isinstance(right, Expr)
        self.left = left
        self.right = right
        self.op = op
        
    def __div__(self, other):
        try :
            if self.op is operators.lsh and int(self.right) == _log2[other]:
                return self.left.cast(self.tipe)
        except:
            pass
        return Expr.__div__(self, other)
        
    def __str__(self):
        return 'BinaryOperator::Create(%s, %s, %s, "", current_block)' % (
                    _op_name(self.op, self.tipe), self.left, self.right)
        
class CompareExpr(Binary):
    
    def __init__(self, left, op, right):
        Binary.__init__(self, gtypes.b, left, op, right)
        
    def __str__(self):
        return 'new ICmpInst(%s, %s, %s, "", current_block)' % (
                _op_name(self.op, self.left.tipe), self.left, self.right)

class PointerAdd(Binary):
    
    def __init__(self, left, op, right):
        Binary.__init__(self, gtypes.p, left, op, right)
        
    def __str__(self):
        return self._gep(gtypes.i1)
        
    def _gep(self, tipe):
        if self.left.tipe in [gtypes.p, gtypes.r]:
            ptr = self.left
            offset = self.right
        else:
            assert self.right.tipe in [gtypes.p, gtypes.r]
            ptr = self.right
            offset = self.left
        assert ptr.tipe in [gtypes.p, gtypes.r]
        if tipe is gtypes.i1:
            return 'GetElementPtrInst::Create(%s, %s, "", current_block)' % (
                    ptr, offset/tipe.size)
        else:
            return 'GetElementPtrInst::Create(%s, %s, "", current_block)' % (
                    ptr.pcast(tipe), offset/tipe.size) 
        
    def load(self, tipe):
        gep = self._gep(tipe)
        return Simple(tipe, 'new LoadInst(%s, "", current_block)' % gep)
              
    def pstore(self, tipe, value):
        gep = self._gep(tipe)
        return ' new StoreInst(%s, %s, current_block);\n' % (value.cast(tipe), 
                                                             gep)
            
_next_label = 0
_uid = 0
        

class LlvmPassMode(object):
    "LLVM pass"
    
    def __init__(self, out, i_name):
        global _next_label
        self.label = _next_label
        _next_label += 1
        self.out = out
        self.stream_offset = 0
        self.min_offset = 0
        self.decls = {}
        self.stack = CachingStack(LlvmStack(self.decls))
        self.n_args = []
        self.n_types = []
        self.targets = {}
        self.filename = ''
        self.block_terminated = False
        self.successor_block = None
        self.i_name = i_name

    def declarations(self, out):
        for k,v in self.decls.items():
            out << 'Value* %s = %s;\n' % (k, v)

    def tload(self, tipe, index):
        tmp = 'tmp%d_%d' % (index, self.block.index)
        if tipe == gtypes.r:
            assert index in self.in_mem or index in self.in_regs
            if index in self.in_mem and index not in self.in_regs:
                fmt = ' %s = new LoadInst(ref_temp(%d, current_block), "", current_block);\n'
                self.out << fmt % (tmp, self.mem_temps.index(index))
                self.in_regs.add(index)
        return Simple(tipe, tmp)
   
    def tstore(self, tipe, index, value):
        self.stack.store(self.out)
        tmp = 'tmp%d_%d' % (index, self.block.index)
        self.out << ' %s = %s;\n'% (tmp, value.cast(tipe))
        if tipe == gtypes.r:
            self.in_regs.add(index)
            if index in self.mem_temps:
                self.in_mem.discard(index)
        
    def ip(self):
        i = 'ConstantInt::get(APInt(%d, (intptr_t)IP))' % (gtypes.p.size*8)
        i_to_ptr = 'ConstantExpr::getIntToPtr(%s, BaseCompiler::TYPE_P)'
        return Simple(gtypes.p, i_to_ptr % i)
        
    def opcode(self):
        i = 'ConstantInt::get(APInt(%d, GVMT_CURRENT_OPCODE))' % (gtypes.p.size*8)
        return Simple(gtypes.p, i)
        
    def next_ip(self):
        i = 'ConstantInt::get(APInt(%d, (intptr_t)(IP+%d)))' % (gtypes.p.size*8,
                                                                self.i_length)
        i_to_ptr = 'ConstantExpr::getIntToPtr(%s, BaseCompiler::TYPE_P)'
        return Simple(gtypes.p, i_to_ptr % i)
        
    def constant(self, value):
        assert isinstance(value, int) or isinstance(value, long)
        return Constant(gtypes.i4, value)
    
    def address(self, name):
        return Simple(gtypes.p, 'globals->%s' % name)
        
    def symbol(self, index):
        glob = 'module->getOrInsertGlobal(_gvmt_get_symbol_name(%s), BaseCompiler::TYPE_P)'
        return Simple(gtypes.p, glob % index)
        
    def pload(self, tipe, array):
        return array.load(tipe)
    
    def pstore(self, tipe, array, value):
        self.stack.store(self.out)
        self.out << array.pstore(tipe, value)
        
    def rload(self, tipe, obj, offset):
        if tipe == gtypes.r:
            return Simple(tipe, 'gc_read(%s, %s, current_block)' % (obj, offset))
        else:
            return PointerAdd(obj, operators.add, offset).load(tipe)
    
    def rstore(self, tipe, obj, offset, value):
        self.stack.store(self.out)
        if tipe == gtypes.r:
            self.out << ' gc_write(%s, %s, %s, current_block);\n' % (obj, offset, value.cast(tipe))
        else:
            self.out << PointerAdd(obj, operators.add, offset).pstore(tipe, value)
        
    def binary(self, tipe, left, op, right):
        return left.binary(tipe, op, right)
        
    def comparison(self, tipe, left, op, right):
        if left.tipe == right.tipe and tipe.is_int() and left.tipe in (gtypes.p, gtypes.r):
            return CompareExpr(left, op, right)
        else:
            return CompareExpr(left.cast(tipe), op, right.cast(tipe))

    def convert(self, from_type, to_type, value):
        # This is a conversion, not just a cast.
        # Need to use a conversion instructionCast
        if from_type.is_int():
            assert to_type.is_float()
            if from_type.is_signed():
                convert = 'new SIToFPInst(%s, TYPE_%s, "", current_block)'
            else:                                                         
                convert = 'new UIToFPInst(%s, TYPE_%s, "", current_block)'
        else:                                                             
            assert from_type.is_float() and to_type.is_int()              
            if to_type.is_signed():                                       
                convert = 'new FPToSIInst(%s, TYPE_%s, "", current_block)'
            else:                                                         
                convert = 'new FPToUIInst(%s, TYPE_%s, "", current_block)'
        return Simple(to_type, convert % (value, to_type.suffix))
        
    def unary(self, tipe, op, arg):
        if op == operators.neg:
            return Binary(tipe, Constant(tipe, 0), operators.sub, arg.cast(tipe))
        elif op == operators.inv:
            return Binary(tipe, Constant(tipe, -1), operators.xor, arg.cast(tipe))
        else:
            raise Exception("Unexpected unary operator: %s" % op)
        
    def sign(self, val):
        global _uid
        _uid += 1
        s_ext = ' Value* t%d = new SExtInst(%s, IntegerType::get(%d), "", current_block)'
        self.out << s_ext % (_uid, val, gtypes.iptr.size*8)
        return Simple(gtypes.iptr, 't%d' % _uid)
        
    def zero(self, val):
        global _uid
        _uid += 1
        z_ext = ' Value* t%d = new ZExtInst(%s, IntegerType::get(%d), "", current_block)'
        self.out << z_ext % (_uid, val, gtypes.iptr.size*8)
        return Simple(gtypes.iptr, 't%d' % _uid)
            
    def _save_all(self):
        'Force all in-reg temps into memory, so that GC can find them'
        for i in self.in_regs:
            if i not in self.in_mem and i in self.mem_temps:
                tmp = 'tmp%d_%d' % (i, self.block.index)
                fmt = ' new StoreInst(%s, ref_temp(%d, current_block), current_block);\n'
                self.out << fmt % (tmp, self.mem_temps.index(i))
                self.in_mem.add(i)
        self.in_regs = set()
  
    def n_call(self, func, tipe, args, gc = True):
        global _uid
        _uid += 1
        self._save_all()
        self.out << ' Value* nargs_%d[] = {' % _uid
        for a in self.n_args:
            self.out << '%s, ' % a
        self.out << ' 0};\n '
        a = func.n_call(tipe, self.n_types, 'nargs_%d' % _uid, args)
        self.n_types = []
        self.n_args = []
        self.stack.flush_to_memory(self.out)
        if gc:
            self.out << ' CallInst::Create(Architecture::ENTER_NATIVE, &NO_ARGS[0], &NO_ARGS[0], "", current_block);\n'
        result = Simple(tipe, 'freturn_%d' % _uid)
        self.out << ' Value* freturn_%d = %s;' % (_uid, a)
        if gc:
            self.out << ' CallInst::Create(Architecture::EXIT_NATIVE, &NO_ARGS[0], &NO_ARGS[0], "", current_block);\n'
        return result
        
    def n_call_no_gc(self, func, tipe, args):
        return self.n_call(func, tipe, args, False)
        
    def call(self, func, tipe):
        c = self._call(func, tipe)
        self.out << ' stack->store_pointer(%s, current_block);\n' % c

        
    def c_call(self, func, tipe, pcount):
        top = self.stack.top(self.out)
        a = self._call(func, tipe)
        new_top = 'GetElementPtrInst::Create(%s, %s, "", current_block)' % (top, pcount)
        self.out << ' stack->store_pointer(%s, current_block);\n' % new_top
        return a
            
    def _call(self, func, tipe):
        global _uid
        _uid += 1
        c = Simple(tipe, '__freturn%d' % _uid)
        self._save_all()
        self.stack.flush_to_memory(self.out)
        self.out << ' Value *params_%d[] = { stack->get_pointer(current_block), FRAME };\n' % _uid, 
        params = 'params_%d' % _uid
        call = func.call(params, tipe, 2)
        self.out << ' CallInst *%s = %s;\n' % (c, call)
        self.out << ' %s->setCallingConv(%s);\n' % (c, common.llvm_cc())
        return c 
        
    def laddr(self, name):
        return Simple(gtypes.p, 'laddr_%s' % name)
        
    def rstack(self):
        self.stack.flush_to_memory(self.out)
        sp = 'stack->top(current_block, 0)'
        return Simple(gtypes.p, 'new BitCastInst(%s, TYPE_P, "", current_block)' % sp)
                  
    def alloca(self, tipe, size):
        global _uid
        _uid += 1
        assert tipe is not gtypes.r
        self.out << ' Value* t%d = new AllocaInst(TYPE_%s, %s, "", current_block);' % (_uid, tipe.suffix, size)
        return Simple(gtypes.iptr, 't%d' % _uid)
       
    def gc_malloc(self, size):
        self.stack.flush_to_memory(self.out)
        self._save_all()
        return Simple(gtypes.r, 'gc_malloc(%s, current_block)' % size)
       
    def gc_malloc_fast(self, size):
        return Simple(gtypes.r, 'gc_malloc_fast(%s, current_block)' % size)
        
    def extend(self, tipe, value):
        if tipe.size < gtypes.iptr.size:
            value = 'new TruncInst(%s, TYPE_%s, "", current_block)' % (value, tipe.suffix)
        if tipe in (gtypes.i1, gtypes.i2, gtypes.i4):
            return Simple(gtypes.iptr, 'new SExtInst(%s, IntegerType::get(%s), "", current_block)' % (value, gtypes.iptr.size * 8))
        elif tipe in (gtypes.u1, gtypes.u2, gtypes.u4):
            return Simple(gtypes.uptr, 'new ZExtInst(%s, IntegerType::get(%s), "", current_block)' % (value, gtypes.uptr.size * 8))
        else:
            assert False
               
    def gc_safe(self):
        self._save_all()
        self.stack.flush_to_memory(self.out)
        self.out << ' CallInst::Create(Architecture::GC_SAFE_POINT, &NO_ARGS[0], &NO_ARGS[0], "", current_block);\n'
                            
    def compound(self, name, qualifiers, graph):
        self.stack.flush_cache(self.out)
        if graph.may_gc():
            self._save_all()
        else:
            self.out << ' /* No yield */\n'
        ops = graph.deltas[0]
        args = []
        for i in range(ops):
            args.append(self.__get())
        self.out << ' ref_temps_base += %d;\n' % len(self.mem_temps)
        self.out << ' block_terminated = compile_%s(%s);\n' % (name, ', '.join(args))
        self.out << ' ref_temps_base -= %d;\n' % len(self.mem_temps)
        self.out << ' bb%s_%d = current_block;\n' % (self.label, self.block.index)
                 
                 
    def top_level(self, name, qualifiers, graph):
        global _next_label
        self.i_length = graph.deltas[0] + 1
        self.label = _next_label
        _next_label += 1
        temps = set()
        temp_types = {}
        uses_alloca = False
        for bb in graph: 
            for i in bb:
                if i.__class__ is builtin.TStore:
                    temp_types[i.index] = i.tipe
                    temps.add(i.index)
                elif i.__class__ is builtin.TLoad:
                    temps.add(i.index)
                    temp_types[i.index] = i.tipe
                elif i.__class__ is builtin.Alloca:
                    uses_alloca = True
        forwards = set()
        self.targets = {}
        self.mem_temps = sorted(list(graph.gc_temps()))
        for bb in graph: 
            for i in bb:
                if i.__class__ is builtin.Target:
                    self.targets[i.index] = bb.index
        phis = ssa.phi_nodes(graph)
        self.out << '/*  %s */\n' % name 
        offsets = {}
        bb_count = 0
        self.out << ' BasicBlock *bb%s_0 = current_block;\n' % self.label
        for bb in graph:
            bb_count += 1
            if bb.predecessors:
                self.out << ' BasicBlock *bb%s_%d = makeBB("%s");\n' % (self.label, bb.index, name)
        self.out << ' {\n'
        if uses_alloca:
            llvm_stack_save(self.out)
        last_bb = graph[-1]
        for i in range(bb_count):
            self.first_block = i == 0
            bb = graph[i]
            if bb is not last_bb:
                self.successor_block = 'bb%s_%d' % (self.label, graph[i+1].index)
            self.out << ' /***** %s *****/\n' % bb
            e = bb.parent
            if e:
                if e not in offsets:
                    offsets[e] = self.stream_offset
                self.stream_offset = offsets[e]
            if i:
                self.out << ' current_block = bb%s_%d;' % (self.label, bb.index)
            if temps:
                temp_decls = ['*tmp%s_%d' % (t, bb.index) for t in temps]
                self.out << ' Value ' << ', '.join(temp_decls) << ';\n'
            for t in temps:
                if t in bb.live_in:
                    if t in phis[bb]:
                        self.out << ' /* t%d = PHI(%s) */\n ' % (t, bb.predecessors)
                        self.out << ' PHINode* phi%s_%d = PHINode::Create(TYPE_%s, "", current_block);\n ' % (
                            t, bb.index, temp_types[t].suffix)
                        self.out << ' tmp%s_%d = phi%s_%d;\n' % (t, bb.index, t, bb.index)
                    elif bb.predecessors:
                        j = min([p.index for p in bb.predecessors])
                        if j > bb.index:
                            if '%s_%d' % (t, j) not in forwards:
                                forwards.add('%s_%d' % (t, j))
                                self.out << ' Value* fr%s_%d = new Argument(TYPE_%s);\n' % (t, j, temp_types[t].suffix)
                            var = 'fr%s_%d' % (t, j)
                        else:
                            var = 'tmp%s_%d' % (t, j)
                        self.out << ' tmp%s_%d = %s;\n' % (t, bb.index, var)
            self.block_terminated = False
            self.out << ' block_terminated = false;\n'
            self.in_regs = set()
            self.in_mem = set()
            if bb.parent:
                pref = bb.parent.ref_preferences
                for j in bb.ref_live_in:
                    if not j in pref or pref[j] < 0:
                        self.in_regs.add(j)
                    else:
                        self.in_mem.add(j)
            self.block = bb
            if bb.child:
                self.end_prefs = bb.child.ref_preferences
            else:
                self.end_prefs = {}
            if i:
                self.out << ' stack->start_block(current_block);\n'
            comment = False
            for ins in bb:
                if comment:
                    self.stack.comment(self.out)
                else:
                    comment = True
                ins.process(self)
            if not self.block_terminated:
                if len(bb.successors) == 1:
                    to = iter(bb.successors).next()
                    self.block_end()
                    self.out << ' if(!block_terminated) BranchInst::Create(bb%s_%d, current_block);\n' % (self.label, to.index)
                else:
                    # Cannot have multiple successors in block without branch
                    assert not bb.successors
            for t in temps:
                if t in bb.live_in and t in phis[bb]:
                    for p in bb.predecessors:
                        if p.index > bb.index:
                            if '%s_%d' % (t, p.index) not in forwards:
                                forwards.add('%s_%d' % (t, p.index))
                                self.out << ' Value* fr%s_%d = new Argument(TYPE_%s);\n' % (t, p.index, temp_types[t].suffix)
                            var = 'fr%s_%d' % (t, p.index)
                        else:
                            var = 'tmp%s_%d' % (t, p.index)
                        self.out << ' phi%s_%d->addIncoming(%s, bb%s_%d);\n' % (
                            t, bb.index, var, self.label, p.index)
        self.stack.store(self.out)
        if uses_alloca:
            llvm_stack_restore(self.out)
        for f in forwards:
            self.out << ' fr%s->replaceAllUsesWith(tmp%s); delete fr%s;\n' % (f, f, f)
        self.out << ' }\n'
            
    def __stream_name(self, offset):
        if offset < 0:
            result = 'opm%d' % -offset
        else:
            result = 'op%d' % offset
        return result
            
    def __stream_val(self, offset):
        global _uid
        _uid += 1
        result = self.__stream_name(offset)
        self.out << ' uint8_t opval%d = %s;\n' % (_uid, result)
        return 'opval%d' % _uid
            
    def __get(self):
        result = self.__stream_val(self.stream_offset)
        self.stream_offset += 1
        return result        
        
    def stream_fetch(self, size = 1):
        s = self.__get()
        for i in range(1, size):
            value = self.__get()
            s = '((%s << 8) | %s)' % (s, value)
        assert size <= 4
        return Constant(gtypes.i4, s)
            
    def stream_peek(self, index):
        index += self.stream_offset
        return Constant(gtypes.i4, self.__stream_val(index))
            
    def stream_push(self, value):
        self.stream_offset -= 1
        if self.min_offset > self.stream_offset:
            self.min_offset = self.stream_offset
            self.out << 'uint8_t '
        self.out << '%s = %s;\n' % (self.__stream_name(self.stream_offset), value.const())
        
    def immediate_add(self, l, op, r):
        return Constant(gtypes.i4, '(%s%s%s)' % (l.const() ,op, r.const()))
    
    def stack_drop(self, offset, size):
        self.stack.drop(offset, size, self.out)
        
    def stack_permute(self, inputs, outputs):
        variables = {}
        for n, t in inputs[::-1]:
            variables[n] = self.stack_pop()
        for x in inputs:
            if x not in outputs: 
                #Evaluate for side effects:
                v = variables[n]
                self.out << ' %s;\n' % v
        for n, t in outputs:
            self.stack_push(variables[n])
    
    def stack_pop(self):
        return self.stack.pop(self.out)
    
    def stack_push(self, value):
        if value.__class__ is not Constant:
            global _uid
            _uid += 1
            self.out << ' sv_%d = %s;\n' % (_uid, value)
            self.decls['sv_%d' % _uid] = '0'
            value = Simple(value.tipe, 'sv_%d' % _uid)
        self.stack.push(value, self.out)

    def stack_pop_double(self):
        return self.stack.pop_double(self.out)
        
    def stack_push_double(self, value):
        assert(value.tipe.size == 8)
        return self.stack.push_double(value, self.out)
        
    def stack_pick(self, index):
        return self.stack.pick(index, self.out)
        
    def stack_poke(self, index, value):
        self.stack.poke(index, value, self.out)
 
    def stack_flush(self):
        self.stack.flush_to_memory(self.out)
 
    def stack_insert(self, offset, size):
        return self.stack.insert(offset, size, self.out)
    
    def hop(self, index):
        self.block_end()
        self.out << ' BranchInst::Create(bb%s_%d, current_block);\n' % (
                    self.label, self.targets[index])
        
    def jump(self, offset):
        self.stack.flush_to_memory(self.out)
        self.block_terminated = True
        self.out << ' BranchInst::Create(block_map[IP - ip_start + ((int16_t)%s)], current_block);\n' % offset.const()
        
    def line(self, number):
        self.out << ' /* %s:%s */\n' % (self.filename, number)
    
    def name(self, index, name):
        pass
    
    def type_name(self, index, name):
        pass
        
    def file(self, name):
        self.filename = name
    
    def block_end(self):
        if not self.block.ref_live_out.issubset(self.in_regs.union(self.in_mem)):
            error_set = self.block.ref_live_out - self.in_regs - self.in_mem
#            print self.in_regs
#            print self.in_mem
#            print self.block.ref_live_out
#            print self.block
#            print self.mem_temps
            raise common.UnlocatedException("Use of undefined variable(s): " + 
                                            str(list(error_set)))
        self.stack.join(self.out)
        for i in self.block.ref_live_out:
            if i not in self.end_prefs:
                # Do nothing
                assert i in self.in_regs
            elif self.end_prefs[i] < 0:
                # In reg.
                if i not in self.in_regs:
                    assert i in self.in_mem
                    tmp = 'tmp%d_%d' % (i, self.block.index)
                    self.out << ' %s = new LoadInst(ref_temp(%d, current_block), "", current_block);\n' % (tmp, self.mem_temps.index(i))
            else:
                assert self.end_prefs[i] > 0
                if i not in self.in_mem:
                    assert i in self.in_regs
                    tmp = 'tmp%d_%d' % (i, self.block.index)
                    self.out << ' new StoreInst(%s, ref_temp(%d, current_block), current_block);\n' % (tmp, self.mem_temps.index(i))
        self.block_terminated = True
    
    def local_branch(self, index, condition, t_or_f):
        self.block_end()
        if t_or_f:
            labels = 'bb%s_%s, %s' % (self.label, self.targets[index], self.successor_block)
        else:
            labels = '%s, bb%s_%s' % (self.successor_block, self.label, self.targets[index])
        self.out << ' BranchInst::Create(%s, %s, current_block);\n' % (labels, condition.cast(gtypes.b))
        
    def target(self, index):
        self.stack.store(self.out)
        
    def return_(self, type):
        self.stack.flush_to_memory(self.out)
        self.block_terminated = True
        self.out << ' ReturnInst::Create(stack->get_pointer(current_block), current_block);\n'
        
    def far_jump(self, addr):
        raise common.UnlocatedException("FAR_JUMP is not allowed in compiled code")
    
    def push_state(self, value):
        fmt = ' push_state(%s, current_block);\n'
        self.out << fmt % value.cast(gtypes.p)
        
    def pop_state(self):
        global _uid
        _uid += 1
        self.out << ' Value* popped_%d = pop_state(current_block);\n' % _uid
        return Simple(gtypes.p, 'popped_%d' % _uid)
        
    def push_current_state(self):
        global _uid
        _uid += 1
        self.stack.flush_to_memory(self.out)
        self.out << ' Value* state_%d = push_current_state(current_block);\n' % _uid
        return Simple(gtypes.r, 'state_%d' % _uid)
    
    def discard_state(self):
        self.out << (' CallInst::Create(Architecture::POP_AND_FREE_HANDLER,'
                     ' &NO_ARGS[0], &NO_ARGS[0], "", current_block);\n')
    
    def _raise(self, value):
        global _uid
        _uid += 1        
        self.stack.flush_to_memory(self.out)
        args_fmt = ' Value *args_%d[] = { %s, 0 };\n'
        self.out << args_fmt % (_uid, value.cast(gtypes.r))
        raise_fmt = (' CallInst::Create(Architecture::RAISE_EXCEPTION, '
                     ' &args_%d[0], &args_%d[1], "", current_block);\n')
        self.out << raise_fmt % (_uid, _uid)
        
    def transfer(self, value):
        global _uid
        _uid += 1        
        self.stack.flush_to_memory(self.out)

        args_fmt = ' Value *args_%d[] = { %s, 0 };\n'
        self.out << args_fmt % (_uid, value.cast(gtypes.r))
        self.out << ' args_%d[1] = stack->get_pointer(current_block);\n' % _uid 
        raise_fmt = (' CallInst::Create(Architecture::TRANSFER, '
                     ' &args_%d[0], &args_%d[2], "", current_block);\n')
        self.out << raise_fmt % (_uid, _uid)
        
    def n_arg(self, tipe, val):
        self.n_args.append(val.cast(tipe))
        self.n_types.append(tipe.suffix)
        
    def gc_free_pointer_store(self, value):
        assert(False and "Not implemented until llvm JIT supports TLS")
        store_free_fmt = ' GC_pointers::store_free(%s, current_block);'
        self.out << store_free_fmt % value.cast(gtypes.p)
    
    def gc_limit_pointer_store(self, value):
        assert(False and "Not implemented until llvm JIT supports TLS")
        store_limit_fmt = ' GC_pointers::store_limit(%s, current_block);'
        self.out << store_limit_fmt % value.cast(gtypes.p)

    def gc_limit_pointer_load(self):
        assert(False and "Not implemented until llvm JIT supports TLS")
        return Simple(gtypes.p, 'GC_pointers::load_limit(current_block)')
    
    def gc_free_pointer_load(self):
        assert(False and "Not implemented until llvm JIT supports TLS")
        return Simple(gtypes.p, 'GC_pointers::load_free(current_block)')
          
    def zero_memory(self, obj, size):
        zero_fmt = ' zero_memory(%s, %s, current_block);'
        self.out << zero_fmt % (obj.cast(gtypes.p), size.cast(gtypes.i4))
          
    def close(self):
        self.stack.flush_cache(self.out)
        self.stream_offset = 0
        
    def fully_initialised(self, obj):
        pass
       
    def lock(self, lock):
        self.out << ' Value *params_%d[] = { stack->get_pointer(current_block), FRAME };\n' % _uid, 
        params = 'params_%d' % _uid
        self.out << ' Value *func = module->getOrInsertFunction("gvmt_save_pointers", TYPE_P, TYPE_P, NULL);'
        self.out << ' CallInst::Create(func, &params_%d[0], &params_%d[2], "", current_block)->setCallingConv(CallingConv::X86_FastCall);\n' % (_uid, _uid)
        self.out << ' lock(%s, current_block);' % lock.cast(gtypes.p)
    
    def unlock(self, lock):
        self.out << ' unlock(%s, current_block);' % lock.cast(gtypes.p)
       
    def lock_internal(self, obj, offset):
        self.out << ' lock_internal(%s, %s, current_block);' % (
            obj.cast(gtypes.r), offset. cast(gtypes.i4))
    
    def unlock_internal(self, obj, offset):
        self.out << ' unlock_internal(%s, %s, current_block);' % (
            obj.cast(gtypes.r), offset. cast(gtypes.i4))


