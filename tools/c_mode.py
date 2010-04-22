

import common
_exception = common.UnlocatedException
import sys
import itertools, re
import builtin, gtypes, operators

from stacks import Stack, CachingStack

def default_name(index):
    return "gvmt_t%s" % index
    
_uid = 0

_return_type_codes = { 
    gtypes.i1 : 'RETURN_TYPE_I4',
    gtypes.i2 : 'RETURN_TYPE_I4',
    gtypes.i4 : 'RETURN_TYPE_I4',
    gtypes.i8 : 'RETURN_TYPE_I8',
    gtypes.u1 : 'RETURN_TYPE_I4',
    gtypes.u2 : 'RETURN_TYPE_I4',
    gtypes.u4 : 'RETURN_TYPE_I4',
    gtypes.u8 : 'RETURN_TYPE_I8',
    gtypes.f4 : 'RETURN_TYPE_F4',
    gtypes.f8 : 'RETURN_TYPE_F8',
    gtypes.r  : 'RETURN_TYPE_R',
    gtypes.p  : 'RETURN_TYPE_P',
    gtypes.v  : 'RETURN_TYPE_V',
}

_suffices = {
    gtypes.i1 : 'i',
    gtypes.i2 : 'i',
    gtypes.i4 : 'i',
    gtypes.u1 : 'u',
    gtypes.u2 : 'u',
    gtypes.u4 : 'u',
    gtypes.f4 : 'f',
    gtypes.r  : 'o' ,
    gtypes.p  : 'p'
}
            
_temp_index = 0
         
class CStack(object):
    
    def __init__(self, declarations):
        self.offset = 0
        self.declarations = declarations
     
    def pop_double(self, out):
        global _temp_index
        _temp_index += 1
        self.declarations['gvmt_dr%d' % _temp_index] = 'GVMT_DoubleStackItem'
        fmt = ' gvmt_dr%d = *((GVMT_DoubleStackItem*)(gvmt_sp+%d));'
        out << fmt % (_temp_index, -self.offset)
        self.offset -= 2
        return DoubleStackItem('gvmt_dr%d' % _temp_index)
        
    def pop(self, out):  
        global _temp_index
        _temp_index += 1
        self.declarations['gvmt_r%d' % _temp_index] = 'GVMT_StackItem'
        out << ' gvmt_r%d = gvmt_sp[%d];' % (_temp_index, -self.offset)
        self.offset -= 1
        return StackItem('gvmt_r%d' % _temp_index)
            
    def push_double(self, value, out): 
        self.offset += 2
        c_code = '(*((GVMT_DoubleStackItem*)(gvmt_sp+%d)))' % -self.offset
        si = DoubleStackItem(c_code)
        out << ' %s = %s;' % (si.cast(value.tipe), value)
        
    def push(self, value, out): 
        self.offset += 1
        si = StackItem('gvmt_sp[%d]' % (-self.offset))
        out << ' %s = %s;' % (si.cast(value.tipe), value)

    def pick(self, index, out):
        global _temp_index
        _temp_index += 1
        self.declarations['gvmt_r%d' % _temp_index] = 'GVMT_StackItem'
        out << ' gvmt_r%d = gvmt_sp[%s-%d];' % (_temp_index, index, self.offset)
        return StackItem('gvmt_r%d' % _temp_index)
        
    def flush_to_memory(self, out, ignore = 0):
        if self.offset:
            if self.offset < 0:
                out << ' gvmt_sp += %d;' % -self.offset
            else:
                out << ' gvmt_sp -= %d;' % self.offset
            self.offset = 0
            
    def top(self, out, cached = 0):
        global _uid
        _uid += 1
        var = '__sp_top%d' % _uid
        total_offset = cached+self.offset
        if total_offset:
            if total_offset < 0:
                out << ' GVMT_StackItem *%s = gvmt_sp+%d;' % (var, -total_offset)
            else:
                out << ' GVMT_StackItem *%s = gvmt_sp-%d;' % (var, total_offset)
        else:
            out << ' GVMT_StackItem *%s = gvmt_sp;' % var
        return var
        
    def insert(self, offset, size, out):
        #Is offset a build-time constant?
        self.flush_to_memory(out)
        if offset == 0:
            out << ' gvmt_sp -= %s;' % size.cast(gtypes.i4)
            loop_fmt = ' for (int i = 0; i < %s; i++) gvmt_sp[i].i = 0;'
            out << loop_fmt % size.cast(gtypes.i4)
            return '%s' % self.top(out) 
        else:
            out << ' gvmt_sp -= %s;' % size.cast(gtypes.i4)
            loop_fmt = ' for (int i=0; i<%s; i++) gvmt_sp[i]=gvmt_sp[%s+i];'
            out << loop_fmt % (offset.cast(gtypes.i4), size.cast(gtypes.i4))
            loop_fmt = ' for (int i = 0; i < %s; i++) gvmt_sp[%s+i].i = 0;'
            out << loop_fmt % (size.cast(gtypes.i4), offset.cast(gtypes.i4))
            return '(%s+%s)' % (self.top(out) ,'%s' % offset)
              
    def drop(self, offset, size, out):
        self.flush_to_memory(out)
        if offset == 0:
            out << ' gvmt_sp += %s;' % size.cast(gtypes.i4)
        else:
            loop_fmt = ' for (int i=0; i<%s; i++) gvmt_sp[%s+i]=gvmt_sp[i];'
            out << loop_fmt % (offset.cast(gtypes.i4), size.cast(gtypes.i4))
            out << ' gvmt_sp += %s;' % size.cast(gtypes.i4)
                
    def comment(self, out):
        out << 'Offset: %d\n' % self.offset
                
    def copy(self):
        result = CStack(self.declarations)
        result.offset = self.offset 
        return result
        
    def transform(self, other, transformer, out):
        if self.offset != other.offset:
            out << ' gvmt_sp += %d;' % (self.offset - other.offset)
            self.offset = other.offset
            
    def store(self, out):
        pass
            
class Expr(object):
    
    def __init__(self, tipe):
       assert tipe
       self.tipe = tipe
        
    def cast(self, tipe):
        if self.tipe == tipe:
            return self
        else:
            return Cast(tipe, self)
    
    def __int__(self):
        raise ValueError
        
    def indir(self, tipe):
        return Indirection(tipe, self)
        
    def div(self, size):
        return '%s/%d' % (self, size)
                
    def call(self, tipe):
        call_fmt = '(((gvmt_funcptr)%s)(gvmt_sp, (GVMT_Frame)FRAME_POINTER))'
        return Simple(tipe, call_fmt % self)
                
    def n_call(self, tipe, params):
        if params:
            call_fmt = '(((gvmt_native_funcptr_%s)%s)(%s))'
            params = ', '.join([str(x) for x in params])
            return Simple(tipe, call_fmt % (tipe.suffix, self, params))
        else:
            call_fmt = '(((gvmt_native_funcptr0_%s)%s)())'
            return Simple(tipe, call_fmt % (tipe.suffix, self))

    def store(self, decl, out):
        global _temp_index
        _temp_index += 1
        decl['gvmt_r%d' % _temp_index ] = self.tipe.c_name
        out << ' gvmt_r%d = %s;' % (_temp_index, self)
        return Simple(self.tipe, 'gvmt_r%d' % _temp_index)
        
class StackItem(Expr):
    
    def __init__(self, txt):
        Expr.__init__(self, gtypes.x)
        assert isinstance(txt, str)
        self.txt = txt
        
    def cast(self, tipe):
        assert tipe in _suffices or tipe == gtypes.x
        if tipe == gtypes.v:
            return Simple(tipe, '(void)%s' % self.txt)
        elif tipe == gtypes.x:
            return self
        else:
            return Simple(tipe, '%s.%s' % (self.txt, _suffices[tipe]))
            
    def indir(self, tipe):
        return Simple(tipe, '(*(%s*)%s.p)' % (tipe.c_name, self.txt))
            
    def __str__(self):
        return self.txt
        
    def store(self, decl, out):
        return self
        
class DoubleStackItem(Expr):
    
    def __init__(self, txt):
        Expr.__init__(self, gtypes.x2)
        assert isinstance(txt, str)
        self.txt = txt
        
    def cast(self, tipe):
        if tipe is gtypes.x2:
            return self
        if tipe is gtypes.i8:
            return Simple(tipe, '%s.i' % self.txt)
        elif tipe is gtypes.u8:
            return Simple(tipe, '%s.u' % self.txt)
        elif tipe is gtypes.f8:
            return Simple(tipe, '%s.f' % self.txt)
        else:
            raise Exception('Unexpected type: %s' % tipe)
            
    def indir(self, tipe):
        assert False
            
    def __str__(self):
        return self.txt
        
    def store(self, decl, out):
        return self

class Simple(Expr):

    def __init__(self, tipe, txt):
        Expr.__init__(self, tipe)
        assert isinstance(txt, str)
        self.txt = txt
        
    def __str__(self):
        return self.txt
        
    def __int__(self):
        if self.tipe != gtypes.i4 and self.tipe != gtypes.u4:
            raise ValueError
        return int(self.txt)
        
    def store(self, decl, out):
        if self.txt.startswith('gvmt_r'):
            return self
        else:
            return Expr.store(self, decl, out)

class Constant(Simple):
    
    def __init__(self, tipe, val):
        assert val is not None
        assert -(2**31) <= val
        assert 2**32 > val
        if val >= 2**31:
            txt = hex(val)
            if txt[-1] in 'Ll':
                txt = txt[:-1]
        else:
            txt = str(val)
        Simple.__init__(self, tipe, txt)
        
    def div(self, x):
        assert('int' in self.tipe.c_name)
        return str(int(self.txt)/x)
        
    def __int__(self):
        return int(self.txt)
        
    def __str__(self):
        return self.txt
     
class Cast(Expr):

    def __init__(self, tipe, expr):
        Expr.__init__(self, tipe)
        self.expr = expr
        if tipe == gtypes.x2:
            raise Exception("!!")
        
    def __str__(self):
        if (self.tipe == gtypes.f4 or self.expr.tipe == gtypes.f4 or 
            self.tipe == gtypes.f8 or self.expr.tipe == gtypes.f8):
            assert self.tipe.size == self.expr.tipe.size     
            if self.expr.tipe.size > gtypes.p.size:
                dsi = DoubleStackItem('((GVMT_DoubleStackItem)%s)' % self.expr)
                return dsi.cast(self.tipe).__str__()
            else:
                si = StackItem('((GVMT_StackItem)%s)' % self.expr)
                return si.cast(self.tipe).__str__()
        elif self.tipe == gtypes.p and self.expr.tipe.size < gtypes.p.size:
            return '((void*)(intptr_t)(%s))' % self.expr
        else:
            return '((%s)(%s))' % (self.tipe.c_name, self.expr)
        
class Binary(Expr):
    
    def __init__(self, tipe, left, op, right):
        Expr.__init__(self, tipe)
        assert isinstance(left, Expr)
        assert isinstance(right, Expr)
        self.left = left
        self.op = op
        self.right = right
        
    def __str__(self):
        return '(%s%s%s)' % (self.left, self.op.c_name, self.right)
            
    def div(self, x):
        if self.op == operators.add or self.op == operators.sub:
            return '(%s%s%s)' % (self.left.div(x), self.op.c_name, 
                                 self.right.div(x))
        else:
            return '%s/%d' % (self, x)
        
class LeftShift(Binary):
    
    def __init__(self, tipe, left, right):
        Binary.__init__(self, tipe, left, operators.lsh, right)
               
    def div(self, size):
        log2 = ( '', '0', '1', '', '2', '', '', '', '3')
        if str(self.right) == log2[size]:
            return str(self.left)
        else:
            return '%s/%d' % (self, size)
            
    def __str__(self):
        try:
            return '(%s<<%s)' % (self.left, self.right) 
        except:
            return Binary.__str__(self)
            
class PointerAdd(Binary):
    
    def __init__(self, tipe, left, op, right):
        Binary.__init__(self, tipe, left, op, right)
              
    def __str__(self):
        if self.left.tipe == gtypes.p or self.left.tipe == gtypes.r:
            return '(((char*)%s)%s%s)' % (self.left, self.op.c_name, 
                                          self.right.cast(gtypes.i4))
        else:
            if self.right.tipe == gtypes.p:
                assert self.op.c_name == '+'
                return '(((char*)%s)+%s)' % (self.right, 
                                             self.left.cast(gtypes.i4))
            else:
                return '((char*)(%s%s%s))' % (self.left.cast(gtypes.i4), 
                             self.op.c_name, self.right.cast(gtypes.i4))
            
class Indirection(Expr):
  
    def __init__(self, tipe, expr):
        Expr.__init__(self, tipe)
        self.expr = expr
        
    def __str__(self):
        return '(((GVMT_memory*)%s)->%s)' % (self.expr, self.tipe.suffix)
        
class Address(Expr):
    
    def __init__(self, txt, externals):
        Expr.__init__(self, gtypes.p)
        assert isinstance(txt, str)
        self.txt = txt
        self.externals = externals
        
    def __str__(self):
        return '&' + self.txt
        
    def indir(self, tipe):
        tname = self.externals[self.txt].split()[0]
        if tname == tipe.c_name:
            return Simple(tipe, self.txt)
        else:
            return Indirection(tipe, '&' + self.txt)
        
    def call(self, tipe):
        return Simple(tipe, '%s(gvmt_sp, (GVMT_Frame)FRAME_POINTER)' % self.txt)
        
    def n_call(self, tipe, params):
        params = ', '.join([str(x) for x in params])
        return Simple(tipe, '%s(%s)' % (self.txt, params))
            
_next_label = 0
        

def _no_amp(x):
    s = str(x)
    if s and s[0] == '&':
        return s[1:]
    else:
        return 'function'
        
class CMode(object):
    "Output C for immediate execution"
    
    def __init__(self, out, externals, gc_name):
        global _next_label
        self.out = out
        self.stream_offset = 0 
        self.stream_stack = []
        self.label = _next_label
        _next_label += 1
        self.temp_types = {}
        self.stack = CachingStack(CStack({}))
        self.names = {}
        self.type_names = {}
        self.filename = ''
        self.externals = externals
        self.next_edge_set = []
        self.edges = None
        self.n_args = []
        self.gc_name = gc_name
        self.in_regs = set()
        self.ref_base = 0
        self.ref_temps_count = 0
        self.ref_temps_max = 0
        self.mem_temps = []
        self.first_block = False

    def pload(self, tipe, array):
        self._null_check(array)
        return array.indir(tipe)
    
    def pstore(self, tipe, array, value):
        self._null_check(array)
        self.stack.store(self.out)
        self.out << ' %s = %s;' % (array.indir(tipe), value.cast(tipe))
        
    #If debug is on, insert extra checking code.
    def _check_ref_access(self, obj, offset, tipe):
        if common.global_debug:
            if tipe == gtypes.r:
                comp = '<='
                expected = ''
                got = 'non-'
            else:
                comp = '>='
                expected = 'non-'
                got = ''
            shape_fmt = ' if(gvmt_get_shape_at_offset(%s, %s) %s 0)'
            self.out << shape_fmt % (obj, offset, comp)
            self.out << shape_fmt % (obj, offset, '==')
            fatal_fmt = (' __gvmt_fatal("%%s:%%d: Invalid member access'
                         ' (offset %%d), %s \\n", __FILE__, __LINE__, %s);')
            what = 'past end of object'
            self.out << fatal_fmt % (what, offset)
            self.out << ' else'
            what = 'expected %sreference got %sreference' % (expected, got)
            self.out << fatal_fmt % (what, offset)
        
    def _null_check(self, obj):
        if common.global_debug:
            self.out << ' if(%s == NULL) ' % obj 
            self.out << '__gvmt_fatal("%s:%d: Attempted use of NULL'
            self.out << 'reference/pointer\\n", __FILE__, __LINE__);'      
        
    def rload(self, tipe, obj, offset):
        obj = obj.cast(gtypes.r)
        self._null_check(obj)
        self._check_ref_access(obj, offset, tipe)
        return PointerAdd(tipe, obj, operators.add, offset).indir(tipe)
        
    def rstore(self, tipe, obj, offset, value):
        obj = obj.cast(gtypes.r)
        self._null_check(obj)
        self.stack.store(self.out)
        if common.global_debug:
            self.out << ' if (gvmt_object_is_initialised(%s, %s))' % (obj, 
                                                                      offset)
            self._check_ref_access(obj, offset, tipe)
        internal_ptr = PointerAdd(tipe, obj, operators.add, offset)
        self.out << ' %s = %s;'  % (internal_ptr.indir(tipe), value.cast(tipe))

    def binary(self, tipe, left, op, right):
        if op == operators.lsh:
            return LeftShift(tipe, left.cast(tipe), right.cast(gtypes.i4))
        elif tipe == gtypes.p or tipe == gtypes.r:
            return PointerAdd(tipe, left, op, right)
        elif op == operators.rsh:
            return Binary(tipe, left.cast(tipe), op, right.cast(gtypes.i4))
        else:
            return Binary(tipe, left.cast(tipe), op, right.cast(tipe))
        
    def comparison(self, tipe, left, op, right):
        return Binary(gtypes.i4, left.cast(tipe), op, right.cast(tipe))

    def unary(self, tipe, op, arg):
        return Simple(tipe, '(%s%s)' % (op.c_name, arg.cast(tipe)))
        
    def c_call(self, func, tipe, pcount):
        global _uid
        _uid += 1
        top = self.stack.top(self.out)
        if common.global_debug:
            self.out << ' gvmt_last_return_type = 0;'
        self._call(func, tipe)
        new_top = self.stack.top(self.out)
        if common.global_debug:
            fmt = ' if(gvmt_last_return_type && gvmt_last_return_type != %s)'
            self.out << fmt % _return_type_codes[tipe]
            fmt = (' __gvmt_fatal("%%s:%%d:Incorrect return type, '
                   'expected %s got %%s\\n", __FILE__, __LINE__,'
                   'gvmt_return_type_names[gvmt_last_return_type]);')
            self.out << fmt % tipe.suffix
            self.out << ' if(%s-%s > %s)' % (new_top, top, pcount)
            fmt = ' __gvmt_expect_v(__FILE__, __LINE__, "%s", %s, %s-%s);'
            self.out << fmt % (_no_amp(func), pcount, new_top, top)
        if pcount:
            if tipe != gtypes.v:
                fmt = ' %s call_%d = *((%s*)gvmt_sp);'
                self.out << fmt % (tipe.c_name, _uid, tipe.c_name)
                if tipe.size <= gtypes.p.size:
                    self.stack.push(Simple(tipe, 'call_%d' % _uid), self.out)
                else:
                    result = Simple(tipe, 'call_%d' % _uid)
                    self.stack.push_double(result, self.out)
            self.out << ' gvmt_sp = %s+%s;' % (top, pcount)
                
    def call(self, func, tipe):
        if common.global_debug:
            top = self.stack.top(self.out)
            self.out << ' gvmt_last_return_type = 0;'
        self._call(func, tipe)
        new_top = self.stack.top(self.out)
        if tipe == gtypes.v:
            twords = 0
        elif tipe.size <= gtypes.p.size:
            twords = 1
        else:
            twords = 2
        if common.global_debug:
            fmt = ' if(gvmt_last_return_type && gvmt_last_return_type != %s)'
            self.out << fmt % _return_type_codes[tipe]
            fmt = (' __gvmt_fatal("%%s:%%d:Incorrect return type, '
                   'expected %s got %%s\\n", __FILE__, __LINE__,'
                   'gvmt_return_type_names[gvmt_last_return_type]);')
            self.out << fmt % tipe.suffix
            
    def _call(self, func, tipe):
        global _uid
        _uid += 1
        self.in_regs = set()
        self.stack.flush_to_memory(self.out)
        self.out << ' gvmt_sp = %s;' % func.call(tipe)

    def n_call(self, func, tipe, args, gc = True):
        self.in_regs = set()
        self.stack.flush_to_memory(self.out)
        if gc:
            enter = ' gvmt_enter_native(gvmt_sp, (GVMT_Frame)FRAME_POINTER);'
            self.out << enter
        # For now to check that this is OK - To be removed
        if len(self.n_args) < args:
            raise _exception('Insufficient native arguments for N_CALL')
        arguments = self.n_args[-args:]
        a = func.n_call(tipe, arguments)
        self.n_args = self.n_args[:-args]
        if tipe is gtypes.v:
            self.out << ' %s;' % a
            result = None
        elif tipe.size > gtypes.p.size:
            self.stack.push_double(a, self.out)
            self.stack.store(self.out)
            result = self.stack.pop_double(self.out)
        else:
            self.stack.push(a, self.out)
            self.stack.store(self.out)
            result = self.stack.pop(self.out)
        if gc:
            self.out << ' gvmt_sp = gvmt_exit_native();'
        return result
            
    def n_call_no_gc(self, func, tipe, args):
        return self.n_call(func, tipe, args, False)
        
    def alloca(self, tipe, size):
        global _uid
        if self.first_block:
            try:
                #Size is build time constant
                count = int(size)
                if tipe == gtypes.r:
                    ref_fmt = '(FRAME_POINTER->gvmt_frame.refs + %d)'
                    result = Simple(gtypes.p, ref_fmt % self.ref_temps_count)
                    self.ref_temps_count += count
                    if self.ref_temps_count > self.ref_temps_max:
                        self.ref_temps_max = self.ref_temps_count 
                    return result
                else:
                    name =  'gvmt_frame_%d' % _uid
                    if count == 1:
                        self.out << '%s %s;' % (tipe.c_name, name)
                    else:
                        self.out << '%s %s[%s];' % (tipe.c_name, name, count)
                    _uid += 1
                    return Simple(gtypes.p, '&%s' % name)
            except ValueError:
                pass
        if tipe is gtypes.r:
            raise _exception('Illegal use of ALLOCA_R.')
        else:
            bytes = '%s*%s' % (size.cast(gtypes.i4), tipe.size)
            return Simple(gtypes.p, 'alloca(%s)' % bytes)
        
    def gc_malloc(self, size):
        self.stack.flush_to_memory(self.out)
        malloc = 'gvmt_%s_malloc(gvmt_sp, (GVMT_Frame)FRAME_POINTER, %s)'
        obj = Simple(gtypes.p, malloc % (self.gc_name, size.cast(gtypes.i4)))
        self.in_regs = set()
        return obj
        
    def fully_initialised(self, obj):
        if common.global_debug:
           self.out << ' gvmt_fully_initialized_check(%s);' % obj.cast(gtypes.r)
            
    def gc_malloc_fast(self, size):
        global _uid
        _uid += 1
        c = Simple(gtypes.r, 'gvmt_malloc_%d' % _uid)
        malloc_fast = ' GVMT_Object gvmt_malloc_%d = gvmt_fast_allocate(%s);'
        self.out << malloc_fast % (_uid, size)
        return c 

    def convert(self, from_type, to_type, value):
        return Simple(to_type, '((%s)(%s))' % (to_type.c_name, 
                                               value.cast(from_type)))

    def ip(self):
        raise _exception('Cannot use IP outside of intepreter context')

    def opcode(self):
        raise _exception('Cannot use OPCODE outside of intepreter context')

    def next_ip(self):
        raise _exception('Cannot use NEXT_IP outside of intepreter context')
        
    def laddr(self, name):
        raise _exception('Cannot use LADDR outside of intepreter context')
        
    def address(self, name):
        return Address(name, self.externals)
        
    def symbol(self, index):
        return Simple(gtypes.p, '_gvmt_get_symbol(%s)' % index)
     
    def extend(self, tipe, value):
        # Is this right?
        return value.cast(tipe).cast(gtypes.i4)
               
    def gc_safe(self):
        # Uncache all references.
        self.in_regs = set()
        self.out << ' if(gvmt_gc_waiting) gvmt_gc_safe_point'
        self.out << '(gvmt_sp, (GVMT_Frame)FRAME_POINTER);'

    def stack_permute(self, inputs, outputs):
        variables = {}
        for n, t in inputs[::-1]:
            variables[n] = self.stack_pop()
        for x in inputs:
            if x not in outputs: 
                #Evaluate for side effects:
                v = variables[n]
                if '(' in str(v):
                    self.out << ' (void)(%s);' % v
        for n, t in outputs:
            self.stack_push(variables[n])
        
    def compound(self, name, qualifiers, graph):
        global _next_label
        self.stack.store(self.out)
        saved_state = (self.temp_types, self.names, self.type_names, self.label,
                       self.edges, self.mem_temps, self.first_block)
        self.type_names = {}
        old_label = self.label
        self.label = _next_label
        _next_label += 1
        self.names = {}
        self.temp_types = {}
        self.out << ' { '
        out = self.out
        self.out = common.Buffer()
        self.edges = {}
        if graph.may_gc():
            old_in_regs = set()
        else:
            old_in_regs = self.in_regs
        offsets = {}
        last_bb = graph[-1]
        for bb in graph:            
            for i in bb:
                if i.__class__ is builtin.TStore:
                    self.temp_types[i.index] = i.tipe
        for bb in graph:            
            for i in bb:
                if i.__class__ is builtin.TLoad:
                    if i.index not in self.temp_types:
                        unitialised_temp(graph, i.index)
        self.mem_temps = sorted(list(graph.gc_temps()))
        self.out << ' /* Mem temps %s */ ' % self.mem_temps
        old_ref_base = self.ref_base
        self.ref_base = self.ref_temps_count
        self.ref_temps_count = self.ref_base + len(self.mem_temps)
        if self.ref_temps_count > self.ref_temps_max:
            self.ref_temps_max = self.ref_temps_count
        self.first_block = True
        for bb in graph:
            #Set stack here.
            e = bb.parent
            if e:
                if e not in self.edges:
                    c_stack = CStack(self.stack.declarations)
                    self.edges[e] = CachingStack(c_stack)
                if e not in offsets:
                    offsets[e] = self.stream_offset
                self.stack = self.edges[e].copy()
                self.stream_offset = offsets[e]
            else:
                assert bb == graph[0]
#            self.stack.comment(self.out)
            self.next_edge_set.append(bb.child)
            self.in_regs = set()
            for i in bb:
#                self.out << '/* %s */' % i.name
                i.process(self)
            if bb is not last_bb:
                self.block_end()
            self.next_edge_set.pop()
            self.first_block = False
        self.stack.store(self.out)
        for i in self.temp_types:
            if i in self.type_names:
                tname = 'struct gvmt_object_%s*' % self.type_names[i]
            else:
                tname = self.temp_types[i].c_name
            if i in self.names:
                out << ' %s %s;' % (tname, self.names[i])
            else:
                out << ' %s gvmt_t%s;' % (tname, i)
        out << self.out
        (self.temp_types, self.names, self.type_names, self.label,
         self.edges, self.mem_temps, self.first_block) = saved_state
        self.out = out
        self.out << ' }\n'
        self.in_regs = old_in_regs
        self.ref_temps_count = self.ref_base
        self.ref_base = old_ref_base
       
    def transformer(self, s, o):
        if isinstance(o, StackItem):
            self.out << ' %s = %s;' % (o.cast(s.tipe), s)
        else:
            self.out << ' %s = %s;' % (o, s.cast(o.tipe))
        
    def block_end(self):
        next_edge = self.next_edge_set[-1]
        self.stack.flush_to_memory(self.out)
        if next_edge:
            if next_edge in self.edges:
                self.stack.transform(self.edges[next_edge], 
                                     self.transformer, self.out)
            else:
                self.stack.store(self.out)
                self.edges[next_edge] = self.stack.copy()
            self.next_edge_set[-1] = None
#            self.stack.comment(self.out)
        
    def constant(self, val):
        assert isinstance(val, int) or isinstance(val, long)
        # Should validate that value fits in native int.
        return Constant(gtypes.iptr, val)
        
    def stream_fetch(self, size = 1):
        assert size in (1,2,4)
        s = '0'
        while size and self.stream_stack:
            value = self.get()
            s = Simple(gtypes.iptr, '((%s << 8) | %s)' % (s, value))
            size -= 1
        if size == 4:
            ip = '(_gvmt_ip + %d)' % self.stream_offset
            self.stream_offset += 4
            return Simple(gtypes.iptr, '_gvmt_fetch_4(%s)' % ip)
        while size:
            value = self.get()
            s = Simple(gtypes.iptr, '((%s << 8) | %s)' % (s, value))
            size -= 1
        return s
        
    def _stream_item(self, index):
        return  '_gvmt_ip[%d]' % index
        
    def get(self):
        if self.stream_stack:
            return self.stream_stack.pop()
        else:
            s = self._stream_item(self.stream_offset)
            self.stream_offset += 1
            return Simple(gtypes.iptr, s)
            
    def stream_peek(self, index):
        if index < len(self.stream_stack):
            return self.stream_stack[-index-1]
        else:
            index = self.stream_offset + index-len(self.stream_stack)
            return Simple(gtypes.iptr, self._stream_item(index))
    
    def immediate_add(self, l, op, r):
        return Simple(gtypes.iptr, '%s%s%s' % (l, op, r))
            
    def stream_push(self, value):
        assert isinstance(value, Expr)
        self.stream_stack.append(value)
    
    def stack_pop(self):
        return self.stack.pop(self.out)
        
    def top_level(self, name, qualifiers, graph):
        out = self.out
        self.out = common.Buffer()
        self.compound(name, qualifiers, graph)
        if self.ref_temps_max:
            out << '\n#define FRAME_POINTER (&gvmt_frame)\n'
            out << ' struct { struct gvmt_frame gvmt_frame; '
            out << 'GVMT_Object refs[%d]; } gvmt_frame;' % self.ref_temps_max
            out << ' gvmt_frame.gvmt_frame.previous = _gvmt_caller_frame;'
            out << ' gvmt_frame.gvmt_frame.count = %d;' % self.ref_temps_max
            out << ' gvmt_frame.gvmt_frame.interpreter = 0;'
            for i in range(self.ref_temps_max):
                out << ' gvmt_frame.gvmt_frame.refs[%d] = 0;' % i
        else:
            out << '\n#define FRAME_POINTER _gvmt_caller_frame\n'
        out << self.out
        self.out = out
        out.no_line()
        self.out << '#undef FRAME_POINTER\n'
    
    def declarations(self, out):
        for name in self.stack.declarations:
            out << ' %s %s;' % (self.stack.declarations[name], name)

    def stack_push(self, value):
        assert isinstance(value, Expr)
        self.stack.push(value, self.out)
        
    def stack_pick(self, index):
        return self.stack.pick(index, self.out)
        
    def sign(self, val):
        return Simple(gtypes.i8, '((int64_t)%s)' % val.cast(gtypes.i4))
        
    def zero(self, val):
        return Simple(gtypes.u8, '((uint64_t)%s)' % val.cast(gtypes.u4))
        
    def name(self, index, name):
        if index in self.names and name != self.names[index]:
            if self.names[index] == default_name(index):
                raise _exception("Temp %d named '%s' after use" % (index, name))
            else:
                raise _exception("Renaming of temp %d from '%s' to '%s'" % 
                                         (index, self.names[index], name))   
        # name may have already been used for another variable:
        if name not in self.names.values():
            self.names[index] = name
        
    def type_name(self, index, name):
        if index in self.type_names and name != self.type_names[index]:
            raise _exception("Renaming of type of temp %d from '%s' to '%s'" % 
                                     (index, self.type_names[index], name))   
        self.type_names[index] = name
        
    def rstack(self):
        self.stack.flush_to_memory(self.out)
        return Simple(gtypes.p, self.stack.top(self.out))
    
    def stack_flush(self):
        self.stack.flush_to_memory(self.out)
    
    def stack_insert(self, offset, size):   
        return Simple(gtypes.p, self.stack.insert(offset, size, self.out))
    
    def frame(self):
        iframe = 'gvmt_interpreter_frame((GVMT_Frame)FRAME_POINTER)'
        return Simple(gtypes.p, iframe)
    
    def hop(self, index):
        self.block_end()
        self.out << ' goto target_%s_%d;' % (self.label, index)
        
    def jump(self, offset):
        raise _exception('Cannot use JUMP outside of intepreter context')
        
    def line(self, number):
#        self.stack.comment(self.out)
        if self.filename:
            self.out << '\n#line %s "%s"\n  ' % (number, self.filename)
        
    def file(self, name):
        self.filename = name
        
    def local_branch(self, index, condition, t_or_f):
        self.block_end()
        if t_or_f:
            self.out << ' if(%s) ' % condition
        else:
            self.out << ' if(!(%s)) ' % condition.cast(gtypes.i4)
        self.out << 'goto target_%s_%d;' % (self.label, index)
        
    def target(self, index):
        self.stack.store(self.out)
        self.out << '   target_%s_%d: ((void)0);'  % (self.label, index)
        
    def tload(self, tipe, index):
        if index in self.names:
            name = self.names[index]
        else:
            name = default_name(index)
            self.names[index] = name
        if index in self.type_names:
            cast = '(struct gvmt_object_%s*)' % self.type_names[index]
        else:
            cast = ''
        if index in self.mem_temps and index not in self.in_regs:
            self.out << ' %s = %sFRAME_POINTER->gvmt_frame.refs[%d];' % (name, 
                            cast, self.ref_base + self.mem_temps.index(index))
            self.in_regs.add(index)
        if cast:
            name = '((GVMT_Object)%s)' % name
        if index not in self.temp_types:
            raise _exception("Undefined temp %d" % index)
        if tipe != self.temp_types[index]:
            if not tipe.compatible(self.temp_types[index]):
                raise _exception("Reuse of temp %d type %s with type %s" % (
                                 index, self.temp_types[index], tipe))
            else:
                return Simple(self.temp_types[index], name).cast(tipe)
        else:
            return Simple(tipe, name)

    def stack_pop_double(self):
        return self.stack.pop_double(self.out)
        
    def stack_push_double(self, val):
        assert(val.tipe.size == 8)
        return self.stack.push_double(val, self.out)
        
    def tstore(self, tipe, index, value):
        self.stack.store(self.out)
        if tipe != self.temp_types[index]:
            if not tipe.compatible(self.temp_types[index]):
                fmt = "Reuse of temp %d type %s with type %s"
                raise _exception(fmt % (index, self.temp_types[index], tipe))
        if index in self.type_names:
            cast = '(struct gvmt_object_%s*)' % self.type_names[index]
        else:
            cast = ''
        if index in self.names:
            name = self.names[index]
        else:
            name = default_name(index)
            self.names[index] = name
        self.out << ' %s = %s%s;' % (name, cast, value.cast(tipe))
        if index in self.mem_temps:
            fmt = ' FRAME_POINTER->gvmt_frame.refs[%d] = (GVMT_Object)%s;'
            self.out << fmt % (self.ref_base+self.mem_temps.index(index), name)
            self.in_regs.discard(index)
            
    def return_(self, type):
        self.stack.flush_to_memory(self.out)
        self.stream_offset = 0
        if common.global_debug:
            fmt = ' gvmt_last_return_type = %s;'
            self.out << fmt % _return_type_codes[type]
        self.out << ' return gvmt_sp;'

    def far_jump(self, addr):
        raise _exception('Cannot use FAR_JUMP outside of intepreter context')
        
    def stack_drop(self, offset, size):
        self.stack.drop(offset, size, self.out)
        
    def close(self):
        if self.stream_offset:
            self.out << ' _gvmt_ip += %d;' % self.stream_offset
        if self.stream_stack:
            msg = 'Value(s) pushed back to stream at end of instruction'
            raise _exception(msg)
        self.stream_offset = 0
#        self.stack.comment(self.out)
        self.stack.flush_to_memory(self.out)
        
    def create_and_push_handler(self):
        self.stack.flush_to_memory(self.out)
        fmt = ' GvmtExceptionHandler __handler_%d = gvmt_create_and_push_handler();'
        self.out << fmt % _uid
        self.out << ' __handler_%d->sp = gvmt_sp;' % _uid
        
    def n_arg(self, tipe, val):
        global _uid
        _uid += 1
        name = '_gvmt_narg_%s' % _uid
        self.out << '%s %s = %s;' % (tipe.c_name, name, val.cast(tipe))
        self.n_args.append(Simple(tipe, name))
                 
    def setjump(self):
        c = Simple(gtypes.r, '__state_%d' % _uid)
        self.out << 'gvmt_double_return_t val%d; ' % _uid
        'val%d.ret =  ' % _uid
        fmt = 'val%d.ret = gvmt_setjump(&__handler_%d->registers, gvmt_sp); '
        self.out << fmt % (_uid, _uid)
        self.out << 'GVMT_Object %s = val%d.regs.ex; ' % (c, _uid)
        self.out << 'gvmt_sp = val%d.regs.sp; ' % _uid
        return c 
        
    def push_state(self, value):
        self.out << '((GvmtExceptionHandler)%s)->link = gvmt_exception_stack; ' % value.cast(gtypes.p)
        self.out << 'gvmt_exception_stack = ((GvmtExceptionHandler)%s); ' % value.cast(gtypes.p)
        
    def pop_state(self):
        global _uid
        _uid += 1
        self.out << 'GvmtExceptionHandler h_%d = gvmt_exception_stack; ' % _uid
        self.out << 'gvmt_exception_stack = h_%d->link; ' % _uid
        return Simple(gtype.p, '((void*)h_%d' %_uid)
        
    def push_current_state(self):
        global _uid
        _uid += 1
        self.create_and_push_handler()
        return self.setjump()

    def discard_state(self):
        self.out << ' gvmt_pop_and_free_handler();'
    
    def _raise(self, value):
        self.stack.flush_to_memory(self.out)
        self.out << ' gvmt_raise_exception(%s);'% value.cast(gtypes.r)
    
    def transfer(self, value):
        self.stack.flush_to_memory(self.out)
        self.out << ' gvmt_transfer(%s, gvmt_sp);'% value.cast(gtypes.r)

    def gc_free_pointer_store(self, value):
        self.out << ' gvmt_gc_free_pointer = (GVMT_StackItem*)%s;' % value

    def gc_limit_pointer_store(self, value):
        self.out << ' gvmt_gc_limit_pointer = (GVMT_StackItem*)%s;' % value

    def gc_limit_pointer_load(self):
        return Simple(gtypes.p, 'gvmt_gc_limit_pointer')

    def gc_free_pointer_load(self):
        return Simple(gtypes.p, 'gvmt_gc_free_pointer')
    
    def zero_memory(self, obj, size):
        try:
            size = int(size)
            if size <= gtypes.p.size * 6:
                size = (size + gtypes.p.size - 1) // gtypes.p.size
                for i in range(size):
                    self.out << ' ((intptr*)%s)[%d] = 0;' % (obj, i)
        except ValueError:
            self.out << ' GVMT_SET_MEMORY(%s, %s, 0);' % (
                            obj, size.cast(gtypes.iptr))
            
    def lock(self, lock):
        #Inline uncontended case
        global _uid
        _uid += 1
        self.out << ' intptr_t* lock_%d = (intptr_t*)%s; ' % (_uid, lock)
        self.out << ' if(!COMPARE_AND_SWAP(lock_%d, ' % _uid
        self.out << 'GVMT_LOCKING_UNLOCKED, '
        self.out << 'gvmt_thread_id | GVMT_LOCKING_LOCKED))'
        self.out << ' gvmt_fast_lock(lock_%d);' % _uid
        
    def unlock(self, lock):
        #Inline uncontended case:
        global _uid
        _uid += 1
        self.out << ' intptr_t* lock_%d = (intptr_t*)%s; ' % (_uid, lock)
        self.out << ' if(!COMPARE_AND_SWAP(lock_%d, ' % _uid
        self.out << 'gvmt_thread_id | GVMT_LOCKING_LOCKED, '
        self.out << 'GVMT_LOCKING_UNLOCKED)) '
        self.out << ' gvmt_fast_unlock(lock_%d);' % _uid
            
    def lock_internal(self, obj, offset):
        #Inline uncontended case
        global _uid
        _uid += 1
        if common.global_debug:
            self._check_ref_access(obj, offset, gtypes.iptr)
        self.out << ' intptr_t* lock_%d = (intptr_t*)(((char*)%s)+%s); ' % (_uid, obj, offset)
        self.out << ' if(!COMPARE_AND_SWAP(lock_%d, ' % _uid
        self.out << 'GVMT_LOCKING_UNLOCKED, '
        self.out << 'gvmt_thread_id | GVMT_LOCKING_LOCKED))'
        self.out << ' gvmt_fast_lock(lock_%d);' % _uid
        
    def unlock_internal(self, obj, offset):
        #Inline uncontended case:
        global _uid
        _uid += 1
        if common.global_debug:
            self._check_ref_access(obj, offset, gtypes.iptr)
        self.out << ' intptr_t* lock_%d = (intptr_t*)(((char*)%s)+%s); ' % (_uid, obj, offset)
        self.out << ' if(!COMPARE_AND_SWAP(lock_%d, ' % _uid
        self.out << 'gvmt_thread_id | GVMT_LOCKING_LOCKED, '
        self.out << 'GVMT_LOCKING_UNLOCKED)) '
        self.out << ' gvmt_fast_unlock(lock_%d);' % _uid
            
class IMode(CMode):

    def __init__(self, temp, externals, gc_name, name):
        CMode.__init__(self, temp, externals, gc_name)
        self.i_name = name

    def top_level(self, name, qualifiers, graph):
        self.i_length = graph.deltas[0] + 1
        self.compound(name, qualifiers, graph)

    def push_current_state(self):
        global _uid
        _uid += 1
        self.create_and_push_handler()
        self.out << ' __handler_%d->ip = _gvmt_ip;' % _uid
        c = self.setjump()
        self.out << ' __handler_%d = gvmt_exception_stack;' % _uid
        self.out << ' _gvmt_ip = __handler_%d->ip;' % _uid
        return c

    def ip(self):
        return Simple(gtypes.p, '_gvmt_ip')
        
    def opcode(self):
        return Simple(gtypes.ip, '(GVMT_CURRENT_OPCODE)')

    def next_ip(self):
        return Simple(gtypes.p, '(_gvmt_ip+%d)' % self.i_length)

    def laddr(self, name):
        return Simple(gtypes.p, '(&FRAME_POINTER->%s)' % name) 

    def far_jump(self, addr):
        self.out << ' _gvmt_ip = %s;' % addr
        self.stack.flush_to_memory(self.out)
        if common.token_threading:
            self.out << ' goto *gvmt_operand_table[*_gvmt_ip];'
        else:
            self.out << ' break;'
       
    def close(self):
        if self.stream_offset:
            self.out << ' _gvmt_ip += %d;' % self.stream_offset
        if self.stream_stack:
            raise _exception(
                  "Value(s) pushed back to stream at end of instruction")
        self.stream_offset = 0
        self.stack.flush_to_memory(self.out)

    def jump(self, offset):
        self.out << ' _gvmt_ip += (int16_t)(%s);' % offset 
        self.stack.flush_to_memory(self.out)
        if common.token_threading:
            self.out << ' goto *gvmt_operand_table[*_gvmt_ip];'
        else:
            self.out << ' break;'
        
    def alloca(self, tipe, size):
        global _uid
        if tipe is gtypes.r:
            raise _exception('Illegal use of ALLOCA_R.')
        if self.first_block:
            try:
                #Size is build time constant
                count = int(size)
                name =  'gvmt_frame_%d' % _uid
                if count == 1:
                    self.out << '%s %s;' % (tipe.c_name, name)
                else:
                    self.out << '%s %s[%s];' % (tipe.c_name, name, count)
                _uid += 1
                return Simple(gtypes.p, '&%s' % name)
            except ValueError:
                msg = "Cannot use variable sized ALLOCA_X in interpreter code"
                raise _exception(msg)
        else:
            msg = "Use of ALLOCA_X in interpreter code must be in first block"
            raise _exception(msg)

def unitialised_temp(graph, index):
    filename = None
    line = None
    for bb in graph:
        for i in bb:
            if i.__class__ == builtin.File:
                filename = i.filename
            if i.__class__ == builtin.Line:
                line = i.line            
            if i.__class__ is builtin.Name and i.index == index:
                if filename and line:
                    raise common.GVMTException(
                               "%s:%s:Uninitialised variable '%s'" % 
                               (filename, line, i.tname))
                else:
                    raise _exception("Uninitialised temp '%s'" % i.tname)
     
