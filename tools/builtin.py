'Predefined GSC instructions.'

from common import GVMTException, UnlocatedException

import gtypes, operators, re

class Instruction(object):
    'Base class for all GSC instructions.'
    
    exits = False
    fallthru = True
    operands = 0
    to_stream = 0
    
    def process(self, mode):
        ( 'Called by tools, with appropriate mode. '
        'All sub classes should override this method' )
        raise Exception("%s must implement process()" % self.__class__.__name__)
        
    def may_gc(self):
        'Returns True if this instruction may yield control to the GC'
        return False
        
    def __hash__(self):
        return id(self)
            
def _push(mode, tipe, val):
    'Pushes val of type tipe to stack'
    if tipe.size > gtypes.p.size:
        mode.stack_push_double(val)
    else:
        mode.stack_push(val)
        
def _pop(mode, tipe):
    'Pops value fo type tipe from stack'
    if tipe.size > gtypes.p.size:
        return mode.stack_pop_double()
    else:
        return mode.stack_pop()

class Comment(Instruction):
    'For debugging purposes, may be discarded by tools.'
    
    def __init__(self, text):
        self.name = '//%s\n' % text
        self.__doc__ = 'Comment: %s'  % text
        self.inputs = [ ]
        self.outputs = [ ]
        
    def process(self, mode):
        'Does nothing'
        pass
        
class NativeArg(Instruction):

    def __init__(self, tipe):
        self.tipe = tipe
        self.name = 'NARG_%s' % tipe.suffix
        self.inputs = [ 'val' ]
        self.outputs = [ ]
        self.__doc__ = ( 'Native argument of type %s.'
        'TOS is pushed to the native argument stack.' ) % tipe.doc
                
    def process(self, mode):
        val = _pop(mode, self.tipe)
        mode.n_arg(self.tipe, val)
        
_letters = { gtypes.i4 : 'I',
            gtypes.f4 : 'F',
            gtypes.i8 : 'L',
            gtypes.f8 : 'D' }

class Convert(Instruction):

    def __init__(self, from_type, to_type):
        self.name = '%s2%s' % (_letters[from_type], _letters[to_type])
        self.from_type = from_type
        self.to_type = to_type
        self.inputs = [ 'val' ]
        self.outputs = [ 'result' ]
        self.__doc__ = 'Converts %s to %s.' % (from_type.doc, to_type.doc)
        
    def process(self, mode):
        val = _pop(mode, self.from_type)
        result = mode.convert(self.from_type, self.to_type, val)
        _push(mode, self.to_type, result)
            
class BinaryOp(Instruction):
    
    def __init__(self, op, tipe):
        self.name = '%s_%s' % (op.name, tipe.suffix)
        self.inputs = [ 'op1', 'op2' ]
        self.outputs = [ 'value' ]
        self.op = op
        self.tipe = tipe
        self.__doc__ = "Binary operation: %s %s." % (tipe.doc, op.description)
        
    def process(self, mode):
        if (self.tipe.size > gtypes.p.size and self.op is not operators.lsh
            and self.op is not operators.rsh):
            right = mode.stack_pop_double()
        else:
            right = mode.stack_pop()
        left = _pop(mode, self.tipe)
        result = mode.binary(self.tipe, left, self.op, right)
        _push(mode, self.tipe, result)
            
class ComparisonOp(Instruction):
    
    def __init__(self, op, tipe):
        self.name = '%s_%s' % (op.name, tipe.suffix)
        self.inputs = [ 'op1', 'op2' ]
        self.outputs = [ 'value' ]
        self.op = op
        self.tipe = tipe
        self.__doc__ = ("Comparison operation: %s %s." 
                        % (tipe.doc, op.description))
        
    def process(self, mode):
        right = _pop(mode, self.tipe)
        left = _pop(mode, self.tipe)
        result = mode.comparison(self.tipe, left, self.op, right)
        mode.stack_push(result)
        
    def invert(self):
        return ComparisonOp(self.op._invert, self.tipe)
        
class UnaryOp(Instruction):
    
    def __init__(self, op, tipe):
        self.name = '%s_%s' % (op.name, tipe.suffix)
        self.inputs = [ 'op1' ]
        self.outputs = [ 'value' ]
        self.op = op
        self.tipe = tipe
        self.__doc__ = "Unary operation: %s %s." % (tipe.doc, op.description)
        
    def process(self, mode):
        arg = _pop(mode, self.tipe)   
        result = mode.unary(self.tipe, self.op, arg)
        _push(mode, self.tipe, result)
 
            
class TLoad(Instruction):
  
    def __init__(self, tipe, index):
        try:
            self.index = int(index)
        except ValueError:
            raise GVMTException('TLOAD_%s parameter must be an integer' 
                                % tipe.suffix)
        self.inputs = []
        self.outputs = [ 'value' ]
        self.tipe = tipe
        self.name = "TLOAD_%s(%d)" % (tipe.suffix, self.index)
        self.__doc__ = r"Push the contents of the Nth temporary variable as a %s." % self.tipe.doc
        
    def process(self, mode):
        val = mode.tload(self.tipe, self.index)
        _push(mode, self.tipe, val)
        
class TStore(Instruction):
    
    def __init__(self, tipe, index):
        try:
            self.index = int(index)
        except ValueError:
            raise GVMTException('TSTORE_%s parameter must be an integer' 
                                % tipe.suffix)
        self.name = "TSTORE_%s(%d)" % (tipe.suffix, self.index)
        self.inputs = [ 'value' ]
        self.outputs = []
        self.tipe = tipe
        self.__doc__ = r"Pop a %s from the stack and store in the Nth temporary variable." %  self.tipe.doc
    
    def process(self, mode):
        val = _pop(mode, self.tipe)
        mode.tstore(self.tipe, self.index, val)
        
# Set of all legal chars, instersection of ASCII and C identifier characters.
legal_chars = set(
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_')

def legal_name(name):
    tname = name.strip('"')
    if tname[0] in '0123456789':
        raise UnlocatedException("'%s' is not a legal C identifier" % tname)
    for c in tname:
        if c not in legal_chars:
            raise UnlocatedException("'%s' is not a legal C identifier" % tname)
    return tname
     
class Name(Instruction):
    
    def __init__(self, index, name):
        self.name = 'NAME(%s,%s)' % (index, name)
        try:
            self.index = int(index)
        except ValueError:
            raise UnlocatedException(
                'First parameter for %s must be an integer' % self.name)
        self.tname = legal_name(name)
        self.inputs = [ ]
        self.outputs = []
        self.__doc__ = ('Name the Nth temporary variable,'
        ' for debugging purposes.')

    def process(self, mode):
        mode.name(self.index, self.tname)
    
class TypeName(Instruction):
    
    __doc__ = ( 'Name the (reference) type of the Nth temporary variable,' 
                ' for debugging purposes.')
    
    def __init__(self, index, name):
        self.name = 'TYPE_NAME(%s,%s)' % (index, name)
        try:
            self.index = int(index)
        except ValueError:
            raise UnlocatedException(
                'First parameter for %s must be an integer' % self.name)
        self.tname = legal_name(name)
        self.inputs = [ ]
        self.outputs = []    

    def process(self, mode):
        mode.type_name(self.index, self.tname)
        
class PLoad(Instruction):
  
    def __init__(self, tipe):
        self.name = "PLOAD_" + tipe.suffix
        self.inputs = [ 'addr' ]
        self.outputs = [ 'value' ]
        self.tipe = tipe
        self.__doc__ = ('Load from memory. '
                        'Push %s value loaded from address in TOS.') % tipe.doc
  
    def process(self, mode):
        val = mode.pload(self.tipe, mode.stack_pop())
        _push(mode, self.tipe, val)
        
class PStore(Instruction):
    
    def __init__(self, tipe):
        self.name = "PSTORE_" + tipe.suffix
        self.inputs = [ 'value', 'array' ]
        self.outputs = []
        self.tipe = tipe
        self.__doc__ = ('Store to memory. '
                        'Store %s value in NOS to address in TOS.') % tipe.doc 
        
    def process(self, mode):
        array = mode.stack_pop()
        val = _pop(mode, self.tipe)
        mode.pstore(self.tipe, array, val)

class RLoad(Instruction):
  
    def __init__(self, tipe):
        self.name = "RLOAD_" + tipe.suffix
        self.inputs = [ 'object', 'offset' ]
        self.outputs = [ 'value' ]
        self.tipe = tipe
        self.__doc__ = ('Load from object. Load %s '
                        'value from object NOS at offset TOS.') % tipe.doc
        if tipe == gtypes.r:
            self.__doc__ += ('Any read-barriers required by the garbage ' 
                             'collector are performed.')
            
  
    def process(self, mode):
        offset = mode.stack_pop()
        obj = mode.stack_pop()
        val = mode.rload(self.tipe, obj, offset)
        _push(mode, self.tipe, val)
        
class RStore(Instruction):
    
    def __init__(self, tipe):
        self.name = "RSTORE_" + tipe.suffix
        self.inputs = [  'value', 'object', 'offset']
        self.outputs = []
        self.tipe = tipe
        self.__doc__ = ('Store into object.Store %s ' 
                        'value at 3OS into object NOS, offset TOS.') % tipe.doc 
        if tipe == gtypes.r:
            self.__doc__ += ('Any write-barriers required by the garbage' 
                             ' collector are performed.')
        
    def process(self, mode):
        offset = mode.stack_pop()
        obj = mode.stack_pop()
        val = _pop(mode, self.tipe)
        mode.rstore(self.tipe, obj, offset, val)
        
class RStoreSimple(Instruction):
    
    def __init__(self, tipe):
        self.name = "__RSTORE_" + tipe.suffix
        self.inputs = [  'value', 'object', 'offset']
        self.outputs = []
        self.tipe = tipe
        self.__doc__ = ('Store into object, no write barrier. '
                        ' Toolkit internal use only.')
        
    def process(self, mode):
        offset = mode.stack_pop()
        obj = mode.stack_pop()
        val = _pop(mode, self.tipe)
        mode.rstore(self.tipe, obj, offset, val)

class FullyInitialized(Instruction):
    
    def __init__(self):
        self.name = 'FULLY_INITIALIZED'
        self.inputs = [ 'object' ]
        self.outputs = []
        self.__doc__ = ('Declare TOS object to be fully-initialised. '
                        'This allows optimisations to be made by the toolkit. '
                        'Drops TOS as a side effect.')
            
    def process(self, mode):
        obj = mode.stack_pop()
        mode.fully_initialised(obj)

class Branch(Instruction):
      
    def __init__(self, index, way = True):
        if way:
            self.name = "BRANCH_T(%s)" % index
        else:
            self.name = "BRANCH_F(%s)" % index 
        self.inputs = [ 'cond' ]
        self.outputs = []
        self.index = int(index)
        self.way = way
        self.__doc__ = ("Branch if TOS is %s to Target(%s)." % 
            (['False', 'True'][way == True], index))
        
    def process(self, mode):
        cond = mode.stack_pop()
        mode.local_branch(self.index, cond, self.way)
        
class Hop(Instruction):
      
    name = "HOP(n)"
    doc = "Jump (unconditionally) to TARGET(n)"
    
    def __init__(self, index):
        self.name = "HOP(%s)" % index
        self.inputs = []
        self.outputs = []
        self.index = int(index)
        self.__doc__ = "Jump (unconditionally) to TARGET(%s)." % index
        
    def process(self, mode):
        mode.hop(self.index)
        
class Target(Instruction):
      
    def __init__(self, index):
        self.name = "TARGET(%s)" % index
        self.inputs = [  ]
        self.outputs = [  ]
        self.index = int(index)
        self.__doc__  = "Target for Jump and Branch."
  
    def process(self, mode):
        mode.target(self.index)

class Extend(Instruction):
      
    def __init__(self, tipe):
        self.name = "EXT_%s" % tipe.suffix
        self.inputs = [ 'value' ]
        self.outputs = [ 'extended' ]
        self.tipe = tipe
        self.__doc__ = 'Extends %s value to full word.' % tipe.suffix
            
    def process(self, mode):
        mode.stack_push(mode.extend(self.tipe, mode.stack_pop()))

class Symbol(Instruction):
      
    def __init__(self):
        self.name = "SYMBOL"
        self.inputs = [ ]
        self.outputs = [ 'address' ]
        self.operands = 2
        self.__doc__ = 'Push address of symbol to TOS'
            
    def process(self, mode):
        index =  mode.stream_fetch(2)
        mode.stack_push(mode.symbol(index))
        
class Call(Instruction):
      
    def __init__(self, tipe):
        self.name = 'CALL_%s' % tipe.suffix
        self.tipe = tipe
        self.inputs = []
        self.outputs = [ 'value' ]
        self.annotations = [ 'private' ]
        self.__doc__ = ('Calls the function whose address is TOS. Pushes the ' 
                        'return value which must be of type %s.') % tipe
        
    def process(self, mode):
        func = mode.stack_pop()
        mode.call(func, self.tipe)
        
    def may_gc(self):
        return True
            
class V_Call(Instruction):
      
    def __init__(self, tipe):
        self.name = 'V_CALL_%s' % tipe.suffix
        self.tipe = tipe
        self.inputs = []
        self.outputs = [ 'value' ]
        self.operands = 1
        self.annotations = [ 'private' ]
        self.__doc__ = ('Variadic call. '
                'Calls the function whose address is TOS. Ensures '
                'that N parameters are removed from the evaluation stack '
                'and pushes the return value which must be of type %s. '
                'N is fetched from the instruction stream.') % tipe
        
    def process(self, mode):
        func = mode.stack_pop()
        args = mode.stream_fetch()
        mode.c_call(func, self.tipe, args)
        
    def may_gc(self):
        return True

class N_Call(Instruction):
      
    def __init__(self, tipe, args):
        self.name = 'N_CALL_%s(%s)' % (tipe.suffix, args)
        self.inputs = []
        self.outputs = [ 'value' ]
        try:
            self.args = int(args)
        except ValueError:
            raise GVMTException('%s parameter must be an integer' 
                                % self.name)
        self.annotations = [ 'private' ]
        self.tipe = tipe
        self.__doc__ = ('Calls the function whose address is TOS. Uses '
        'the native calling convention for this platform with %s '
        'parameters which are popped from the native argument stack. '
        'Pushes the return value which must be of type %s.') % (args, tipe)
        
    def process(self, mode):
        func = mode.stack_pop()
        val = mode.n_call(func, self.tipe, self.args)
        if self.tipe is not gtypes.v:
            _push(mode, self.tipe, val)
        
    def may_gc(self):
        return True

class N_Call_No_GC(Instruction):
      
    def __init__(self, tipe, args):
        self.name = 'N_CALL_NO_GC_%s(%s)' % (tipe.suffix, args)
        self.inputs = []
        self.outputs = [ 'value' ]
        try:
            self.args = int(args)
        except ValueError:
            raise GVMTException('%s parameter must be an integer' 
                                % self.name)
        self.annotations = [ 'private' ]
        self.tipe = tipe
        self.__doc__ = ('As N_CALL_%s(%s). Garbage collection is suspended '
        'during this call. Only use the NO_GC variant for calls which cannot '
        'block. If unsure use N_CALL.') % (tipe.suffix, args)
        
    def process(self, mode):
        func = mode.stack_pop()
        val = mode.n_call_no_gc(func, self.tipe, self.args)
        if self.tipe is not gtypes.v:
            _push(mode, self.tipe, val)
        
    def may_gc(self):
        return False

class Frame(Instruction):
    
    def __init__(self):
        self.name = 'FRAME'
        self.inputs = [ ]
        self.outputs = [ 'frame' ]
        self.__doc__ = ('Pushes an opaque pointer representing the current '
        'interpreter frame, if in the interpreter, or the most recently ' 
        'executed interpreter frame, if not in the interpreter, to TOS.')
        
    def process(self, mode):
        mode.stack_push(mode.frame())

class Alloca(Instruction):
    
    def __init__(self, tipe):
        self.name = 'ALLOCA_%s' % tipe.suffix
        self.inputs = [ 'N' ]
        self.outputs = [ 'ptr' ]
        self.tipe = tipe
        self.__doc__ = ('Allocates space for N %ss in the current control '
        'stack frame, leaving pointer to allocated space in TOS. All memory '
        'allocated after a PROTECT is invalidated immediately by a RAISE, '
        'but not necessarily immediately reclaimed. All memory allocated is '
        'invalidated and reclaimed by a RETURN instruction.') % tipe.doc
         
    def process(self, mode):
        mode.stack_push(mode.alloca(self.tipe, mode.stack_pop()))
         
class GC_Malloc(Instruction):
    
    def __init__(self):
        self.name = 'GC_MALLOC'
        self.inputs = [ 'N' ]
        self.outputs = [ 'ptr' ]
        self.__doc__ = ('Allocates N bytes in the heap leaving pointer to '
                        'allocated space in TOS. GC pass may replace with a '
                        'faster inline version. Defaults to GC_MALLOC_CALL.')
         
    def process(self, mode):
        mode.stack_push(mode.gc_malloc(mode.stack_pop()))
        
    def may_gc(self):
        return True
         
class GC_Allocate_Only(Instruction):
    
    def __init__(self):
        self.name = '__GC_ALLOC_ONLY'
        self.inputs = [ 'N' ]
        self.outputs = [ 'ptr' ]
        self.__doc__ = ('Allocates N bytes in the heap leaving pointer to '
                        'allocated space in TOS. Does not initialise memory. '
                        'For internal toolkit use only.')
         
    def process(self, mode):
        mode.stack_push(mode.gc_malloc(mode.stack_pop()))
        
    def may_gc(self):
        return True
         
class GC_Malloc_Call(Instruction):
    
    def __init__(self):
        self.name = 'GC_MALLOC_CALL'
        self.inputs = [ 'N' ]
        self.outputs = [ 'ptr' ]
        self.__doc__ = ('Allocates N bytes, via a call to the GC collector. '
                        'Generally users should use GC_MALLOC and allow the '
                        'toolkit to substitute appropriate inline code.')
        
    def process(self, mode):
        mode.stack_push(mode.gc_malloc(mode.stack_pop()))
        
    def may_gc(self):
        return True
         
class GC_Malloc_Fast(Instruction):
    
    def __init__(self):
        self.name = 'GC_MALLOC_FAST'
        self.inputs = [ 'N' ]
        self.outputs = [ 'ptr' ]
        self.__doc__ = ('Fast allocates N bytes, ptr is 0 if cannot allocate '
                        'fast. Generally users should use GC_MALLOC and allow '
                        'the toolkit to substitute appropriate inline code.')
         
    def process(self, mode):
        mode.stack_push(mode.gc_malloc_fast(mode.stack_pop()))
       
        
class LAddr(Instruction):

    def __init__(self, name):
        self.name = 'LADDR(%s)' % (name)
        self.tname = name.strip('"')
        if self.tname[0] in '0123456789':
            raise UnlocatedException(
                "'%s' is not a legal C identifier" % self.tname)
        for c in self.tname:
            if c not in legal_chars:
                raise UnlocatedException(
                    "'%s' is not a legal C identifier" % self.tname)
        self.inputs = []
        self.outputs = [ '%s' % self.tname ]
        self.__doc__ = ("Pushes the address of the local variable " 
                        "'%s' to TOS.") % self.tname

    def process(self, mode):
        mode.stack_push(mode.laddr(self.tname))
        
class Pick(Instruction):
    def __init__(self):
        self.name = 'PICK'
        self.inputs = []
        self.outputs = [ r'N\textsuperscript{th}' ]
        self.operands = 1
        self.__doc__ = ('Picks the Nth item from the stack(TOS is index 0)'
                        'and pushes it onto the evaluation stack.')

    def process(self, mode):
        mode.stack_push(mode.stack_pick(mode.stream_fetch()))         
        
class Stack(Instruction):

    def __init__(self):
        self.name = 'STACK'
        self.inputs = []
        self.outputs = [ 'sp' ]
        self.__doc__ = ('Pushes the Evaluation-stack stack-pointer to TOS. '
        'The evaluation stack grows downwards, so stack items will be at '
        'non-negative offsets from sp. Values pushed on to the stack are not '
        'visible, do \emph{not} access values at negative offsets. '
        'As soon as a net positive number of values are popped from the stack, '
        'sp becomes invalid and should \emph{not} be used.')
    def process(self, mode):
        mode.stack_push(mode.rstack())
        
class IP(Instruction):
    
    def __init__(self):
        self.name = 'IP'
        self.inputs = []
        self.outputs = [ 'instruction_pointer' ]
        self.__doc__ = 'Pushes the (interpreter) instruction pointer to TOS.'
        
    def process(self, mode):
        mode.stack_push(mode.ip())
        
class Opcode(Instruction):
    
    def __init__(self):
        self.name = 'OPCODE'
        self.inputs = []
        self.outputs = [ 'opcode' ]
        self.__doc__ = 'Pushes the current opcode to TOS.'
        
    def process(self, mode):
        mode.stack_push(mode.opcode())
        
class Next_IP(Instruction):
    
    def __init__(self):
        self.name = 'NEXT_IP'
        self.inputs = []
        self.outputs = [ 'instruction_pointer' ]
        self.__doc__ = ('Pushes the (interpreter) instruction pointer '
                        'for the \emph{next} instruction to TOS.')
        
    def process(self, mode):
        mode.stack_push(mode.next_ip())
 
class IP_Fetch(Instruction):
    def __init__(self, operands):
        self.name = '#@' if operands == 1 else '#%d@' % operands
        self.inputs = []
        self.outputs = [ 'operand' ]
        self.operands = operands
        self.__doc__ = ('Fetches the next %d byte(s) from the instruction stream '
                        'and pushes it onto the evaluation stack.' % operands)

    def process(self, mode):
        mode.stack_push(mode.stream_fetch(self.operands))
         
class Sign(Instruction):
    
    def __init__(self):
        self.name = 'SIGN'
        self.inputs = [ 'val' ]
        self.outputs = [ 'extended' ]
        self.__doc__ = ('Sign extend a single word to 64 bit. Leaves a '
                        'double word in TOS and NOS for 32bit machines. '
                        'This is a no-op for 64bit machines.')
        
    def process(self, mode):
        if gtypes.p.size == 4:
            tos = mode.stack_pop()
            mode.stack_push_double(mode.sign(tos))
         
class Zero(Instruction):
    
    def __init__(self):
        self.name = 'ZERO'
        self.inputs = [ 'val' ]
        self.outputs = [ 'extended' ]
        self.__doc__ = ('Zero extend a single word to 64 bit. Leaves a '
                        'double word in TOS and NOS for 32bit machines. '
                        'This is a no-op for 64bit machines.')
        
    def process(self, mode):
        if gtypes.p.size == 4:
            tos = mode.stack_pop()
            mode.stack_push_double(mode.zero(tos))
         
class Protect(Instruction):
    
    def __init__(self):
        self.name = 'PROTECT'
        self.inputs = [  ]
        self.outputs = [ 'value' ]
        self.__doc__ = ('Pushes a new protect-object to the protection stack and '
        'pushes 0 to TOS, when initially executed. '
        'When execution resumes after a RAISE, the value that was TOS when the ' 
        'RAISE instruction was exceuted is pushed to TOS.')
        
    def process(self, mode):
        mode.stack_push(mode.protect())
        
class Protect_Push(Instruction):
    
    def __init__(self):
        self.name = 'PROTECT_PUSH'
        self.inputs = [ 'protect' ]
        self.outputs = [ ]
        self.__doc__ = ('Pushes a pre-exisiting protect-object '
                         'to the protection stack.')
        
    def process(self, mode):
        mode.protect_push(mode.stack_pop())
        
class Protect_Pop(Instruction):
    
    def __init__(self):
        self.name = 'PROTECT_PUSH'
        self.inputs = [ ]
        self.outputs = [ 'protect' ]
        self.__doc__ = ('Pops the protect-object '
                         'from the protection stack.')
        
    def process(self, mode):
        mode.stack_push(mode.protect_pop())
         
class Unprotect(Instruction):
    
    def __init__(self):
        self.name = 'UNPROTECT'
        self.inputs = [  ]
        self.outputs = [ 'value' ]
        self.__doc__ = 'Pops and destroys the protect-object on top of the exception stack.'
        
    def process(self, mode):
        mode.unprotect()
         
class Raise(Instruction):
    
    def __init__(self):
        self.name = 'RAISE'
        self.inputs = [ 'value' ]
        self.outputs = [ ]
        self.__doc__ = ('Pops the value from the stack. Unwinds the stack, '
        'if necessary, and resumes execution at the PROTECT '
        'instruction associated with the protect-object on top of the '
        'protection stack, pushing the value back to the restored stack.')
        
    def process(self, mode):
        tos = mode.stack_pop()
        mode._raise(tos)
        
class Return(Instruction):
    
    def __init__(self, tipe):
        self.name = "RETURN_" + tipe.suffix
        self.inputs = ['value']
        self.outputs = []
        if tipe is gtypes.v:
            self.__doc__ = "Returns"
        else:
            self.__doc__ = "Returns the %s value (on TOS)." % tipe.doc
        self.tipe = tipe
        
    def process(self, mode):
        mode.return_(self.tipe)
   
class Jump(Instruction):
      
    def __init__(self):
        self.name = "JUMP"
        self.inputs = []
        self.outputs = []
        self.operands = 2
        self.__doc__ = ('Only valid in bytecode context. Performs VM jump. '
        'Jumps by N bytes, where N is the next '
        'two-byte value in the instruction stream.')
       
    def process(self, mode):
        offset =  mode.stream_fetch(2)
        mode.jump(offset)
   
    exits = True
    fallthru = False

class Flush(Instruction):
      
    def __init__(self):
        self.name = 'FLUSH'
        self.inputs = []
        self.outputs = []
        self.annotations = [ 'stack' ]
        self.__doc__ = 'Flushes evaluation stack to memory.'
        
    def process(self, mode):
        mode.stack_flush()
              
class Address(Instruction):
      
    def __init__(self, val):
        self.name = 'ADDR(%s)' % val
        self.symbol = val
        self.inputs = []
        self.outputs = [ 'address' ]
        self.__doc__ = "Address of the global variable %s" % val
        
    def process(self, mode):
        mode.stack_push(mode.address(self.symbol))
        
    def rename(self, rename_dict):
        if self.symbol in rename_dict:
            self.symbol = rename_dict[self.symbol]
        self.name = 'ADDR(%s)' % self.symbol
              
class Number(Instruction):
      
    def __init__(self, val):
        self.name = val
        self.value = int(val)
        self.inputs = []
        self.outputs = [ 'value' ]
        self.__doc__ = "Integral constant"
        
    def process(self, mode):
        mode.stack_push(mode.constant(self.value))
        
class PreFetch(Instruction):
    
    def __init__(self, txt):
        self.name = txt
        self.inputs = []
        self.outputs = []
        assert txt[1] == '['
        assert txt[-1] == ']'
        self.value = int(txt[2:-1])
        self.to_stream = 1
        self.__doc__ = ('Only valid in bytecode context. Peeks into the '
                        'instruction stream and pushes the Nth byte in the '
                        'stream to the front of the instruction stream.')
    
    def process(self, mode):
        val = mode.stream_peek(self.value)
        mode.stream_push(val)
        
class GC_Safe(Instruction):
    
    def __init__(self):
        self.name = 'GC_SAFE'
        self.inputs = []
        self.outputs = []
        self.__doc__ = ('Declares this point to be a safe point for Garbage '
                        'Collection to occur at. GC pass should replace with '
                        'a custom version. Defaults to GC_SAFE_CALL.')
        
    def process(self, mode):
        mode.gc_safe()
        
    def may_gc(self):
        return True 

class GC_Safe_Call(Instruction):
    
    def __init__(self):
        self.name = 'GC_SAFE_CALL'
        self.inputs = []
        self.outputs = []
        self.__doc__ = ('Calls GC to inform it that calling thread is safe '
                        'for Garbage Collection. Generally users should use '
                        'GC_SAFE and allow the toolkit to substitute '
                        'appropriate inline code.')
        
    def process(self, mode):
        mode.gc_safe()
        
    def may_gc(self):
        return True 
     
class Insert(Instruction):
    
    def __init__(self):
        self.name = 'INSERT'
        self.inputs = [ 'count' ]
        self.outputs = [ 'address' ]
        self.operands = 1
        self.__doc__ = ('Pops count off the stack. Inserts count NULLs into '
                        'the stack at offset fetched from the instruction '
                        'stream. Ensures that all inserted values are flushed '
                        'to memory. Pushes the address of first inserted slot '
                        'to the stack.')

    def process(self, mode):
        size = mode.stack_pop()
        offset = mode.stream_fetch()
        mode.stack_push(mode.stack_insert(offset, size))
 
class FarJump(Instruction):
    
    def __init__(self):
        self.name = 'FAR_JUMP'
        self.inputs = [ 'ip' ]
        self.outputs = [ ]
        self.__doc__ = ('Continue interpretation, with the current abstract '
                        'machine state, at the IP popped from the stack. '
                        'If this instruction is executed in compiled code, '
                        'execution leaves the compiler and continues in the '
                        '\emph{interpreter}. In order to ensure jumps are '
                        'compiled use JUMP instead. However, FAR_JUMP can be '
                        'used as a means of exiting compiled code and resuming '
                        'interpretation.')
        
    def process(self, mode):
        ip = mode.stack_pop()
        mode.far_jump(ip)
 
class DropN(Instruction):
    
    def __init__(self):
        self.name = 'DROP_N'
        self.inputs = [ 'count' ]
        self.outputs = [ ]
        self.operands = 1
        self.__doc__ = ('Pops count off the stack. Drops count values '
        'from the stack at offset fetched from stream.')

    def process(self, mode):
        size = mode.stack_pop()
        offset = mode.stream_fetch()
        mode.stack_drop(offset, size)
        
class Drop(Instruction):
    
    def __init__(self):
        self.name = 'DROP'
        self.inputs = [ 'top' ]
        self.outputs = [ ]
        self.__doc__ = 'Drops the top value from the stack.'
    def process(self, mode):
        mode.stack_pop()
        
class Immediate(Instruction):
    
    def __init__(self, txt):
        self.name = txt
        self.inputs = []
        self.outputs = []
        try:
            self.value = int(txt[1:])
        except ValueError:
            raise UnlocatedException(
                "Immediate value must be an integer, found '%s'" % txt[1:])
        self.operands = 0
        self.to_stream = 1
        self.__doc__ = ('Push 1 byte value to the front '
                        'of the instruction stream.')
          
    def process(self, mode):
        mode.stream_push(mode.constant(self.value))
                
class ImmediateAdd(Instruction):
    
    def __init__(self, txt):
        self.name = txt
        self.inputs = []
        self.outputs = []
        self.op = txt[1]
        self.operands = 2
        self.to_stream = 1
        iaddop = { '+' : 'adds' , '-' : 'subtracts' }
        self.__doc__ = ('Fetches the first two values in the instruction '
                        'stream, %s them and pushes the result back to the '
                        'stream.') % iaddop[self.op]
 
    def process(self, mode):
        mode.stream_push(mode.immediate_add(mode.stream_fetch(), self.op, 
                         mode.stream_fetch()))
 
class Line(Instruction):
    
    def __init__(self, line):
        try :
            self.name = 'LINE(%d)' % int(line)
        except ValueError:
            raise UnlocatedException(
                "LINE parameter must be an integer, found '%s'" % line)
        self.line = int(line)
        self.inputs = []
        self.outputs = []
        self.__doc__ = ('Set the source code line number of the source code. '  
                        'Like #LINE in C.')
        
    def process(self, mode):
        mode.line(self.line)
        
        
class File(Instruction):
    
    def __init__(self, name):
        self.name = 'FILE(%s)' % name
        self.filename = name.strip('"')
        self.inputs = []
        self.outputs = []
        self.__doc__ = ('Declares the source file for this code. '
                        'Like #FILE in C.')
        
    def process(self, mode):
        mode.file(self.filename)
        
#These 5 instructions are for use by GC code only.

class ZeroMemory(Instruction):
    
    def __init__(self):
        self.name = '__ZERO_MEMORY'
        self.inputs = [ 'object', 'size' ]
        self.outputs = [ ]
        self.__doc__ = ('Zero the memory for newly allocated object.' 
                        'For GC use only')
        
    def process(self, mode):
        size = mode.stack_pop()
        obj = mode.stack_pop()
        mode.zero_memory(obj, size)


class GC_FreePointerLoad(Instruction):

    def __init__(self):
        self.name = '__GC_FREE_POINTER_LOAD'
        self.inputs = []
        self.outputs = [ 'free_pointer' ]
        self.__doc__ = 'Load the free pointer. For GC use only'
        
    def process(self, mode):
        mode.stack_push(mode.gc_free_pointer_load())

class GC_FreePointerStore(Instruction):
    
    def __init__(self):
        self.name = '__GC_FREE_POINTER_STORE'
        self.inputs = [ 'free_pointer' ]
        self.outputs = []
        self.__doc__ = 'Store to the free pointer. For GC use only'
        
    def process(self, mode):
        mode.gc_free_pointer_store(mode.stack_pop())

class GC_LimitPointerLoad(Instruction):
    
    def __init__(self):
        self.name = '__GC_LIMIT_POINTER_LOAD'
        self.inputs = []
        self.outputs = [ 'limit_pointer' ]
        self.__doc__ = 'Load the limit pointer. For GC use only'
        
    def process(self, mode):
        mode.stack_push(mode.gc_limit_pointer_load())

class GC_LimitPointerStore(Instruction):
    
    def __init__(self):
        self.name = '__GC_LIMIT_POINTER_STORE'
        self.inputs = [ 'limit_pointer' ]
        self.outputs = []
        self.__doc__ = 'Store to the limit pointer. For GC use only'
        
    def process(self, mode):
        mode.gc_limit_pointer_store(mode.stack_pop())
        
class Lock(Instruction):
    
    def __init__(self):
        self.name = 'LOCK'
        self.inputs = [ 'lock' ]
        self.outputs = []
        self.__doc__ = 'Locks the fast-lock popped from TOS'
        
    def process(self, mode):
        mode.lock(mode.stack_pop())
        
class Unlock(Instruction):
    
    def __init__(self):
        self.name = 'UNLOCK'
        self.inputs = [ 'lock' ]
        self.outputs = []
        self.__doc__ = 'Unlocks the fast-lock popped from TOS'
        
    def process(self, mode):
        mode.unlock(mode.stack_pop())
        
instructions = {}
        
def _br_t(x):
    return Branch(x, True)

def _br_f(x):
    return Branch(x, False)

instruction_factories = {
    'BRANCH_T' : _br_t, 'BRANCH_F' : _br_f, 'HOP' : Hop, 'LADDR' : LAddr,
    'TARGET' : Target, 'LINE' : Line, 'FILE' : File, 'NAME': Name,
    'TYPE_NAME' : TypeName, "ADDR" : Address
}

def factory(cls, kind):
    def f(index):
        return cls(kind, index)
    return f

def _init():
        
    reg_types = [ gtypes.i4, gtypes.i8, gtypes.u4, gtypes.u8, gtypes.f4, 
                  gtypes.f8, gtypes.r, gtypes.p ]
    mem_types = [ gtypes.i1, gtypes.i2, gtypes.i4, gtypes.i8, gtypes.u1,
                  gtypes.u2,  gtypes.u4, gtypes.u8, gtypes.f4, gtypes.f8, 
                  gtypes.r, gtypes.p ]
    int_types = [ gtypes.i4, gtypes.i8, gtypes.u4, gtypes.u8 ]
    float_types = [ gtypes.f4, gtypes.f8 ]
    
    for cls in [ Jump, Sign, GC_Malloc, GC_Malloc_Call, Protect, Raise, Drop, 
               GC_Safe, GC_Safe_Call, Flush, Stack, Insert,
               Protect_Push, Protect_Pop, Opcode, 
               Unprotect, IP, Pick, Zero, DropN, Symbol, FarJump, ZeroMemory, 
               GC_FreePointerStore, GC_FreePointerLoad, Frame, GC_Malloc_Fast,
               GC_LimitPointerStore, GC_LimitPointerLoad, Next_IP, 
               GC_Allocate_Only, FullyInitialized, Lock, Unlock ]:
        i = cls()
        instructions[i.name] = i
    for x in (1,2,4):
        i = IP_Fetch(x)
        instructions[i.name] = i
    for tipe in reg_types:
        for cls in [Call, V_Call, Return]:
            i = cls(tipe)
            instructions[i.name] = i
        for cls in [TLoad, TStore, N_Call, N_Call_No_GC]:
            fact = factory(cls, tipe)     
            name = cls.__name__.upper() + '_' + tipe.suffix
            instruction_factories[name] = fact
    
    for cls in [N_Call, N_Call_No_GC]:
        fact = factory(cls, gtypes.v)     
        name = cls.__name__.upper() + '_' + gtypes.v.suffix
        instruction_factories[name] = fact

    for cls in [Call, V_Call, Return]:
        i = cls(gtypes.v)
        instructions[i.name] = i
                
    for tipe in mem_types:
        for cls in [PLoad, PStore, RStore, RLoad, Alloca, RStoreSimple]:
            i = cls(tipe)
            instructions[i.name] = i
            
    for t1 in [ gtypes.i4, gtypes.i8 ]:
        for t2 in [gtypes.f4, gtypes.f8 ]:
            i = Convert(t1, t2)
            instructions[i.name] = i
            i = Convert(t2, t1)
            instructions[i.name] = i
    i = Convert(gtypes.f4, gtypes.f8)
    instructions[i.name] = i
    i = Convert(gtypes.f8, gtypes.f4)
    instructions[i.name] = i
    i = Convert(gtypes.i8, gtypes.i4)
    instructions[i.name] = i
        
    for tipe in [ gtypes.i1, gtypes.i2, gtypes.i4, 
                  gtypes.u1, gtypes.u2, gtypes.u4]:
        i = Extend(tipe)
        instructions[i.name] = i
            
    for op in [ operators.and_, operators.or_, operators.xor, 
                operators.lsh, operators.rsh, operators.mod]:
        for tipe in int_types:
            bin = BinaryOp(op, tipe)
            instructions[bin.name] =  bin
            
    for tipe in int_types:
        unary = UnaryOp(operators.inv, tipe)
        instructions[unary.name] =  unary
            
    for tipe in float_types + [ gtypes.i4, gtypes.i8]:
        unary = UnaryOp(operators.neg, tipe)
        instructions[unary.name] =  unary
        
    for tipe in float_types + int_types + [ gtypes.p]:
        i = NativeArg(tipe)
        instructions[i.name] = i
        
    for op in [ operators.add, operators.sub]:
        bin = BinaryOp(op, gtypes.p)
        instructions[bin.name] =  bin
        
    for op in [ operators.eq, operators.ne, operators.lt, 
                operators.gt, operators.le, operators.ge ]:
        for tipe in int_types + float_types + [ gtypes.p ]:
            bin = ComparisonOp(op, tipe)
            instructions[bin.name] =  bin
    bin = ComparisonOp(operators.eq, gtypes.r)
    instructions[bin.name] =  bin
    bin = ComparisonOp(operators.ne, gtypes.r)
    instructions[bin.name] =  bin
    
              
    for op in [ operators.add, operators.sub, operators.mul, operators.div ]:
        for tipe in int_types + float_types:
            bin = BinaryOp(op, tipe)
            instructions[bin.name] =  bin
    
    for i in instructions.itervalues():
        if not hasattr(i, 'annotations'):
            i.annotations = []
        i.annotations.append('private')

_NUMBER = re.compile(r'-?[0-9]+$')

_if_index = 256

def parse(simples, name, idict = None):
    global _if_index
    if idict is None:
        idict = instructions
    if_stack = []
    ilist = []
    filename = None
    line_no = 0
    it = iter(simples)
    for s in it:
        s = s.strip()
        if s[0] == '#':
            if s[1] == '[':
                ilist.append(PreFetch(s))
            elif (s[1] == '+' or s[1] == '-') and len(s) == 2:
                ilist.append(ImmediateAdd(s))
            elif s[-1] == '@'and s in instructions:
                ilist.append(instructions[s])
            else:
                ilist.append(Immediate(s))
        elif _NUMBER.match(s):
            ilist.append(Number(s))
        elif s == 'IF':
            if_stack.append(_if_index)
            ilist.append(Branch(_if_index, False))
            _if_index += 1
        elif s == 'ELSE':
            end_index = _if_index
            _if_index += 1
            ilist.append(Hop(end_index))
            ilist.append(Target(if_stack.pop()))
            if_stack.append(end_index)
        elif s == 'ENDIF':
            ilist.append(Target(if_stack.pop()))
        else:
            try:
                if '(' in s:
                    cn = s[:s.find('(')]
                    try:
                        while ')' not in s:
                            s += it.next().strip()
                    except StopIteration:
                        msg = ("No closing bracket for '%s', " 
                               "in compound instruction '%s'") %  (s, name)
                        if filename and line_no:
                            raise GVMTException("%s:%d: %s" % 
                                                (filename, line_no, msg))
                        else:
                            raise UnlocatedException(msg)
                    params = s[s.find('(') + 1: -1].split(',')
                    cls = instruction_factories[cn]
                    try :
                        i = cls(*params)
                    except TypeError:
                        fmt = "Incorrect number of parameters for %s: %s"
                        msg = fmt % (cn, s)
                        if filename and line_no:
                            raise GVMTException("%s:%d: %s" % 
                                                (filename, line_no, msg))
                        else:
                            raise UnlocatedException(msg)
                else:
                    i = idict[s]
                if i.__class__ == File:
                    filename = i.filename
                if i.__class__ == Line:
                    line_no = i.line
            except KeyError:
                fmt = "No instruction named '%s', in compound instruction '%s'"
                msg = fmt %  (s, name)
                if filename and line_no:
                    raise GVMTException("%s:%d: %s" % (filename, line_no, msg))
                else:
                    raise UnlocatedException(msg)
            except :
                fmt = "Unable to parse '%s', in compound instruction '%s'" 
                msg = fmt %  (s, name)
                if filename and line_no:
                    raise GVMTException("%s:%d: %s" % (filename, line_no, msg))
                else:
                    raise UnlocatedException(msg)
            ilist.append(i)
    return ilist
    
_init()   
