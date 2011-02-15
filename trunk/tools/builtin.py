'Predefined abstract machine instructions.'

from common import GVMTException, UnlocatedException

import gtypes, operators, re

def _indefinite_article(s) :
    if s == 'void':
        return s    
    elif s[0] in 'aeiou':
        return 'an ' + s
    else:
        return 'a ' + s

class Instruction(object):
    'Base class for all abstract machine instructions.'
    
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
        self.__doc__ = ( 'Native argument of type %s. '
        'TOS is pushed to the native argument stack.' ) % tipe.doc
                
    def process(self, mode):
        val = mode.stack_pop(self.tipe)
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
        self.__doc__ = '''Converts %s to %s. This is a convertion, not a cast. 
        It is the value that remains the same, not the bit-pattern.
        ''' % (from_type.doc, to_type.doc)
        
    def process(self, mode):
        val = mode.stack_pop(self.from_type)
        result = mode.convert(self.from_type, self.to_type, val)
        mode.stack_push(result)
            
class BinaryOp(Instruction):
    
    def __init__(self, op, tipe):
        self.name = '%s_%s' % (op.name, tipe.suffix)
        self.inputs = [ 'op1', 'op2' ]
        self.outputs = [ 'result' ]
        self.op = op
        self.tipe = tipe
        if op == operators.rsh:
            if tipe.is_signed():
                des = 'arithmetic'
            else:
                des = 'logical'
            self.__doc__ = ("Binary operation: %s %s right shift.\n\n"
                'result := op1 >> op2.') % (tipe.doc, des)
        else:
            self.__doc__ = ("Binary operation: %s %s.\n\n" 
                'result := op1 %s op2.') % (tipe.doc, op.description, op.c_name)
        if op == operators.div:
            self.__doc__ += " Rounds towards zero."
        
    def process(self, mode):
        right = mode.stack_pop(self.tipe)
        left = mode.stack_pop(self.tipe)
        result = mode.binary(self.tipe, left, self.op, right)
        mode.stack_push(result)
            
class ComparisonOp(Instruction):
    
    def __init__(self, op, tipe):
        self.name = '%s_%s' % (op.name, tipe.suffix)
        self.inputs = [ 'op1', 'op2' ]
        self.outputs = [ 'comp' ]
        self.op = op
        self.tipe = tipe
        self.__doc__ = ("Comparison operation: %s %s.\n\n"
            'comp := op1 %s op2.' % (tipe.doc, op.description, op.c_name))
        
    def process(self, mode):
        right = mode.stack_pop(self.tipe)
        left = mode.stack_pop(self.tipe)
        result = mode.comparison(self.tipe, left, self.op, right)
        mode.stack_push(result)
        
    def invert(self):
        return ComparisonOp(self.op._invert, self.tipe)
        
class FieldIsNull(Instruction):
    '''Tests whether an object field is null.
Equivalent to RLOAD_X 0 EQ_X where X is a R, P or a pointer sized integer.
'''
    
    def __init__(self, is_null):
        self.is_null = is_null
        if is_null:
            self.name = 'FIELD_IS_NULL'
            not_ = ''
        else:
            self.name = 'FIELD_IS_NOT_NULL'
            not_ = 'not '
        self.inputs = [ 'object', 'offset' ]
        self.outputs = [ 'value' ]
    
    def process(self, mode):
        offset = mode.stack_pop(gtypes.iptr)
        obj = mode.stack_pop(gtypes.r)
        val = mode.field_is_null(self.is_null, obj, offset)
        mode.stack_push(val)
      
class UnaryOp(Instruction):
    
    def __init__(self, op, tipe):
        self.name = '%s_%s' % (op.name, tipe.suffix)
        self.inputs = [ 'op1' ]
        self.outputs = [ 'value' ]
        self.op = op
        self.tipe = tipe
        self.__doc__ = "Unary operation: %s %s." % (tipe.doc, op.description)
        
    def process(self, mode):
        arg = mode.stack_pop(self.tipe)   
        result = mode.unary(self.tipe, self.op, arg)
        mode.stack_push(result)
 
            
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
        self.__doc__ = r'''Push the contents of the nth 
            temporary variable as %s''' % _indefinite_article(self.tipe.doc)
        
    def process(self, mode):
        val = mode.tload(self.tipe, self.index)
        mode.stack_push(val)
        
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
        self.__doc__ = r'''Pop %s from the stack and store in the nth 
            temporary variable.''' % _indefinite_article(self.tipe.doc)
    
    def process(self, mode):
        val = mode.stack_pop(self.tipe)
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
    'Name the nth temporary variable, for debugging purposes.'
    
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

    def process(self, mode):
        mode.name(self.index, self.tname)
    
class TypeName(Instruction):
    '''Name the (reference) type of the nth temporary variable,
    for debugging purposes.'''
    
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
        self.__doc__ = '''Load from memory. 
                          Push %s value loaded from address in TOS 
                          (which must be a pointer).''' % tipe.doc
  
    def process(self, mode):
        val = mode.pload(self.tipe, mode.stack_pop(gtypes.p))
        mode.stack_push(val)
        
class PStore(Instruction):
    
    def __init__(self, tipe):
        self.name = "PSTORE_" + tipe.suffix
        self.inputs = [ 'value', 'array' ]
        self.outputs = []
        self.tipe = tipe
        self.__doc__ = '''Store to memory. 
                          Store %s value in NOS to address in TOS.
                          (TOS must be a pointer)''' % tipe.doc 
        
    def process(self, mode):
        array = mode.stack_pop(gtypes.p)
        val = mode.stack_pop(self.tipe)
        mode.pstore(self.tipe, array, val)

class RLoad(Instruction):
  
    def __init__(self, tipe):
        self.name = "RLOAD_" + tipe.suffix
        self.inputs = [ 'object', 'offset' ]
        self.outputs = [ 'value' ]
        self.tipe = tipe
        self.__doc__ = '''Load from object. 
            Load %s value from object NOS at offset TOS.
            (NOS must be a reference and TOS must be an integer)''' % tipe.doc
        if tipe == gtypes.r:
            self.__doc__ += ('Any read-barriers required by the garbage ' 
                             'collector are performed.')
            
  
    def process(self, mode):
        offset = mode.stack_pop(gtypes.iptr)
        obj = mode.stack_pop(gtypes.r)
        val = mode.rload(self.tipe, obj, offset)
        mode.stack_push(val)
        
class RStore(Instruction):
    
    def __init__(self, tipe):
        self.name = "RSTORE_" + tipe.suffix
        self.inputs = [  'value', 'object', 'offset']
        self.outputs = []
        self.tipe = tipe
        self.__doc__ = '''Store into object.
            Store %s value at 3OS into object NOS, offset TOS.
            (NOS must be a reference and TOS must be an integer)''' % tipe.doc 
        if tipe == gtypes.r:
            self.__doc__ += ('Any write-barriers required by the garbage' 
                             ' collector are performed.')
        
    def process(self, mode):
        offset = mode.stack_pop(gtypes.iptr)
        obj = mode.stack_pop(gtypes.r)
        val = mode.stack_pop(self.tipe)
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
        offset = mode.stack_pop(gtypes.iptr)
        obj = mode.stack_pop(gtypes.r)
        val = mode.stack_pop(self.tipe)
        mode.rstore(self.tipe, obj, offset, val)

class FullyInitialized(Instruction):
    
    def __init__(self):
        self.name = 'FULLY_INITIALIZED'
        self.inputs = [ 'object' ]
        self.outputs = []
        self.__doc__ = ('Declare TOS object to be fully-initialised.'
                        'This allows optimisations to be made by the toolkit.'
                        'Drops TOS as a side effect. TOS must be a reference,'
                        'it is a (serious) error if TOS object'
                        'has \emph{any} uninitialised reference fields')

    def process(self, mode):
        obj = mode.stack_pop(gtypes.r)
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
        self.__doc__ = (
            'Branch if TOS is %s to Target(%s). TOS must be an integer.'
            % (['zero', 'non-zero'][way == True], index))
        
    def process(self, mode):
        cond = mode.stack_pop(gtypes.b)
        mode.local_branch(self.index, cond, self.way)
        
class Hop(Instruction):
      
    __doc__ = "Jump (unconditionally) to TARGET(n)"
    
    def __init__(self, index):
        self.name = "HOP(%s)" % index
        self.inputs = []
        self.outputs = []
        self.index = int(index)
        
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
        if tipe.is_signed():
            ext = 'Sign'
        else:
            ext = 'Zero'        
        self.__doc__ = '''
            %s extends TOS from to a %s to a pointer-sized integer.
            ''' % (ext, tipe.suffix)
            
    def process(self, mode):
        mode.stack_push(mode.extend(self.tipe, mode.stack_pop(self.tipe)))

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
        self.__doc__ = '''Calls the function whose address is TOS. 
            TOS must be a pointer. 
            Removal parameters from the stack is the callee's responsibility.
            The function called must return %s.''' % _indefinite_article(tipe.doc)
        
    def process(self, mode):
        func = mode.stack_pop(gtypes.p)
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
            'The number of parameters, n, '
            'is the next byte in the instruction stream (which is consumed). '
            'Calls the function whose address is TOS. '
            'Upon return removes the n parameters are from the data stack. '
            'The function called must return %s.') % _indefinite_article(tipe.doc)
        
    def process(self, mode):
        func = mode.stack_pop(gtypes.p)
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
        'Pushes the return value which must be a %s.') % (args, tipe.doc)
        
    def process(self, mode):
        func = mode.stack_pop(gtypes.p)
        val = mode.n_call(func, self.tipe, self.args)
        if self.tipe is not gtypes.v:
            mode.stack_push(val)
        
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
        func = mode.stack_pop(gtypes.p)
        val = mode.n_call_no_gc(func, self.tipe, self.args)
        if self.tipe is not gtypes.v:
            mode.stack_push(val)
        
    def may_gc(self):
        return False

class Alloca(Instruction):
    
    def __init__(self, tipe):
        self.name = 'ALLOCA_%s' % tipe.suffix
        self.inputs = [ 'n' ]
        self.outputs = [ 'ptr' ]
        self.tipe = tipe
        self.__doc__ = ('Allocates space for n %ss in the current control '
        'stack frame, leaving pointer to allocated space in TOS. All memory '
        'allocated after a PUSH_CURRENT_STATE is invalidated immediately by a RAISE, '
        'but not necessarily immediately reclaimed. All memory allocated is '
        'invalidated and reclaimed by a RETURN instruction.') % tipe.doc
        if tipe == gtypes.r:
            self.__doc__ += ''' ALLOCA_R cannot be used after the first HOP,
            BRANCH, TARGET, JUMP or FAR_JUMP instruction.'''
         
    def process(self, mode):
        mode.stack_push(mode.alloca(self.tipe, mode.stack_pop(gtypes.uptr)))
         
class GC_Malloc(Instruction):
    
    def __init__(self):
        self.name = 'GC_MALLOC'
        self.inputs = [ 'size' ]
        self.outputs = [ 'ref' ]
        self.__doc__ = ('Allocates size bytes in the heap leaving reference to '
                        'allocated space in TOS. GC pass may replace with a '
                        'faster inline version. Defaults to GC_MALLOC_CALL.')
         
    def process(self, mode):
        mode.stack_push(mode.gc_malloc(mode.stack_pop(gtypes.uptr)))
        
    def may_gc(self):
        return True
         
class GC_Allocate_Only(Instruction):
    
    def __init__(self):
        self.name = '__GC_ALLOC_ONLY'
        self.inputs = [ 'size' ]
        self.outputs = [ 'ref' ]
        self.__doc__ = ('Allocates size bytes in the heap leaving reference to '
                        'allocated space in TOS. Does not initialise memory. '
                        'For internal toolkit use only.')
         
    def process(self, mode):
        mode.stack_push(mode.gc_malloc(mode.stack_pop(gtypes.uptr)))
        
    def may_gc(self):
        return True
         
class GC_Malloc_Call(Instruction):
    
    def __init__(self):
        self.name = 'GC_MALLOC_CALL'
        self.inputs = [ 'size' ]
        self.outputs = [ 'ref' ]
        self.__doc__ = ('Allocates size bytes, via a call to the GC collector. '
            'Generally users should use GC_MALLOC and allow the '
            'toolkit to substitute appropriate inline code.'
            'Safe to use, but front-ends should use GC_MALLOC instead.')
        
    def process(self, mode):
        mode.stack_push(mode.gc_malloc(mode.stack_pop(gtypes.uptr)))
        
    def may_gc(self):
        return True
         
class GC_Malloc_Fast(Instruction):
    
    def __init__(self):
        self.name = 'GC_MALLOC_FAST'
        self.inputs = [ 'size' ]
        self.outputs = [ 'ref' ]
        self.__doc__ = ('Fast allocates size bytes, ref is 0 if cannot allocate '
                        'fast. Generally users should use GC_MALLOC and allow '
                        'the toolkit to substitute appropriate inline code.'
                        'For internal toolkit use only.')
         
    def process(self, mode):
        mode.stack_push(mode.gc_malloc_fast(mode.stack_pop(gtypes.uptr)))
       
        
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
        self.outputs = [ 'addr' ]
        self.__doc__ = ("Pushes the address of the local variable " 
                        "'%s' to TOS.") % self.tname

    def process(self, mode):
        mode.stack_push(mode.laddr(self.tname))
        
class Pick(Instruction):
    def __init__(self, tipe):
        self.name = 'PICK_%s' % tipe.suffix
        self.inputs = []
        self.outputs = [ r'n\textsuperscript{th}' ]
        self.operands = 1
        self.tipe = tipe
        self.__doc__ = ('Picks the nth item from the data stack(TOS is index 0)'
                        'and pushes it to TOS.')

    def process(self, mode):
        mode.stack_push(mode.stack_pick(self.tipe, mode.stream_fetch()))      
        
class Poke(Instruction):
    def __init__(self):
        self.name = 'PICK'
        self.inputs = [ 'value' ]
        self.outputs = [ ]
        self.operands = 1
        self.__doc__ = ('Pops TOS. Then overwrites the nth item with TOS.'
                        'PICK x POKE x has no net effect.')

    def process(self, mode):
        tos = mode.stack_pop(gtypes.uptr)
     
class Stack(Instruction):

    def __init__(self):
        self.name = 'STACK'
        self.inputs = []
        self.outputs = [ 'sp' ]
        self.__doc__ = ('Pushes the data-stack stack-pointer to TOS. '
        'The data stack grows downwards, so stack items will be at '
        'non-negative offsets from sp. '
        'Values subsequently pushed on to the stack are not '
        'visible. Attempting to access values at negative offsets is an error. '
        'As soon as a net positive number of values are popped from the stack, '
        'sp becomes invalid and should \emph{not} be used.')
    def process(self, mode):
        mode.stack_push(mode.rstack())
        
class IP(Instruction):
    
    def __init__(self):
        self.name = 'IP'
        self.inputs = []
        self.outputs = [ 'instruction_pointer' ]
        self.__doc__ = 'Pushes the current (interpreter) instruction pointer to TOS.'
        
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
            'for the \emph{next} instruction to TOS.'
            ' This is equal to IP plus the length of the current bytecode')
        
    def process(self, mode):
        mode.stack_push(mode.next_ip())
 
class IP_Fetch(Instruction):
    def __init__(self, operands):
        self.name = '#@' if operands == 1 else '#%d@' % operands
        self.inputs = []
        self.outputs = [ 'operand' ]
        self.operands = operands
        bytes = 'byte' if operands == 1 else '%d bytes' % operands
        self.__doc__ = ('Fetches the next %s from the instruction stream. ' 
                        % bytes)
        if operands > 1:
            s = 'Combine into an integer, first byte is most significant.'
            self.__doc__ += s
        self.__doc__ += 'Push onto the data stack.'

    def process(self, mode):
        mode.stack_push(mode.stream_fetch(self.operands))
         
class Sign(Instruction):
    
    def __init__(self):
        self.name = 'SIGN'
        self.inputs = [ 'val' ]
        self.outputs = [ 'extended' ]
        self.__doc__ = ('On a 32 bit machine, sign extend TOS'
            ' from a 32 bit value to a 64 bit value. '
            'This is a no-op for 64bit machines.')
        
    def process(self, mode):
        if gtypes.p.size == 4:
            tos = mode.stack_pop(gtypes.i4)
            mode.stack_push(mode.sign(tos))
         
class Zero(Instruction):
    
    def __init__(self):
        self.name = 'ZERO'
        self.inputs = [ 'val' ]
        self.outputs = [ 'extended' ]
        self.__doc__ = ('On a 32 bit machine, zero extend TOS'
            ' from a 32 bit value to a 64 bit value. '
            'This is a no-op for 64bit machines.')
        
    def process(self, mode):
        if gtypes.p.size == 4:
            tos = mode.stack_pop(gtypes.u4)
            mode.stack_push(mode.zero(tos))
         
class PushCurrentState(Instruction):
    '''Pushes a new state-object to the state stack and pushes 0 to TOS, 
    when initially executed. When execution resumes after a RAISE or TRANSFER, 
    then the value in the transfer register is  pushed to TOS.'''
    
    def __init__(self):
        self.name = 'PUSH_CURRENT_STATE'
        self.inputs = [  ]
        self.outputs = [ 'value' ]
        
    def process(self, mode):
        mode.stack_push(mode.push_current_state())

class PopState(Instruction):
    'Pops and discards the state-object on top of the state stack.'
    
    def __init__(self):
        self.name = 'POP_STATE'
        self.inputs = [  ]
        self.outputs = [ 'value' ]
        
    def process(self, mode):
        mode.discard_state()
         
class Raise(Instruction):
    '''Pop TOS, which must be a reference, and place in the transfer register.
    Examine the state object on top of state stack.
    Pop values from the data-stack to the depth recorded.
    Resume execution from the PUSH_CURRENT_STATE instruction that stored
    the state object on the state stack.'''
    
    def __init__(self):
        self.name = 'RAISE'
        self.inputs = [ 'value' ]
        self.outputs = [ ]
        
    def process(self, mode):
        tos = mode.stack_pop(gtypes.r)
        mode._raise(tos)
         
class Transfer(Instruction):
    '''Pop TOS, which must be a reference, and place in the transfer register. 
    Resume execution from the PUSH_CURRENT_STATE instruction that stored
    the state object on the state stack.
    Unlike RAISE, TRANSFER does not modify the data stack.'''
    
    def __init__(self):
        self.name = 'TRANSFER'
        self.inputs = [ ]
        self.outputs = [ ]
        
    def process(self, mode):
        tos = mode.stack_pop(gtypes.r)
        mode.transfer(tos)
        
class Return(Instruction):
    
    def __init__(self, tipe):
        self.name = "RETURN_" + tipe.suffix
        self.inputs = ['value']
        self.outputs = []
        self.__doc__ = '''Returns from the current function.
            Type must match that of CALL instruction.'''
        self.tipe = tipe
        
    def process(self, mode):
        mode.return_(self.tipe)
        
class Pin(Instruction):
    "Pins the object on TOS. Changes type of TOS from a reference to a pointer."
    
    def __init__(self):
        self.name = 'PIN'
        self.inputs = ['object']
        self.outputs = ['pinned']
    
    def process(self, mode):
        pinned = mode.pin(mode.stack_pop(gtypes.r))
        mode.stack_push(pinned)
        
class PinnedObject(Instruction):
    '''Declares that pointer is in fact a reference to a pinned object.
     Changes type of TOS from a pointer to a reference.
     It is an error if the pointer is not a reference to a pinned object.
     Incorrect use of this instruction can be difficult to detect. 
     Use with care.'''

    def __init__(self):
        self.name = 'PINNED_OBJECT'
        self.inputs = ['pointer']
        self.outputs = ['object']
    
    def process(self, mode):
        pinned = mode.pinned_object(mode.stack_pop(gtypes.p))
        mode.stack_push(pinned)
   
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

class Address(Instruction):
      
    def __init__(self, val):
        self.name = 'ADDR(%s)' % val
        self.symbol = val
        self.inputs = []
        self.outputs = [ 'address' ]
        self.__doc__ = '''Pushes the address of the global variable %s
            to the stack (as a pointer).'''% val
        
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
        self.__doc__ = ('Only valid in an interpreter defintion. '
            'Peeks into the instruction stream and pushes the %sth byte in '
            'the stream to the front of the instruction stream.' % self.value)
    
    def process(self, mode):
        val = mode.stream_peek(self.value)
        mode.stream_push(val)
        
class GC_Safe(Instruction):
    
    def __init__(self):
        self.name = 'GC_SAFE'
        self.inputs = []
        self.outputs = []
        self.__doc__ = ('Declares this point to be a safe point for garbage '
                        'collection to occur at. GC pass should replace with '
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
                        'for garbage collection. Generally users should use '
                        'GC_SAFE and allow the toolkit to substitute '
                        'appropriate inline code.')
        
    def process(self, mode):
        mode.gc_safe()
        
    def may_gc(self):
        return True 
     
class Insert(Instruction):
    
    def __init__(self):
        self.name = 'INSERT'
        self.inputs = [ 'n' ]
        self.outputs = [ 'address' ]
        self.operands = 1
        self.__doc__ = ('Pops count off the stack. Inserts n NULLs into '
            'the stack at offset fetched from the instruction stream.'
            'Ensures that all inserted values are flushed to memory. '
            'Pushes the address of first inserted slot to the stack.')

    def process(self, mode):
        size = mode.stack_pop(gtypes.iptr)
        offset = mode.stream_fetch()
        mode.stack_push(mode.stack_insert(offset, size))
 
class FarJump(Instruction):
    
    def __init__(self):
        self.name = 'FAR_JUMP'
        self.inputs = [ 'ip' ]
        self.outputs = [ ]
        self.__doc__ = ('Continue interpretation, with the current abstract '
            'machine state, at the IP popped from the stack. '
            'FAR_JUMP is intended for unusual flow control in code '
            'processors and the like.'
            'Warning: This instruction is not supported in compiled code, '
            'in order to use jumps in compiled code use JUMP instead.'
            )
        
    def process(self, mode):
        ip = mode.stack_pop(gtypes.p)
        mode.far_jump(ip)
 
class DropN(Instruction):
    
    def __init__(self):
        self.name = 'DROP_N'
        self.inputs = [ 'n' ]
        self.outputs = [ ]
        self.operands = 1
        self.__doc__ = ('Drops n values '
        'from the stack at offset fetched from stream.'
        'E.g. for offset=1 and n=2, TOS would be untouched, but NOS and 3OS '
        'would be discarded')

    def process(self, mode):
        size = mode.stack_pop(gtypes.iptr)
        offset = mode.stream_fetch()
        mode.stack_drop(offset, size)
        
class Drop(Instruction):
    
    def __init__(self):
        self.name = 'DROP'
        self.inputs = [ 'top' ]
        self.outputs = [ ]
        self.__doc__ = 'Drops the top value from the stack.'
        
    def process(self, mode):
        mode.stack_pop(gtypes.p)
        
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
                        'Informational only, like #LINE in C.')
        
    def process(self, mode):
        mode.line(self.line)
        
        
class File(Instruction):
    
    def __init__(self, name):
        self.name = 'FILE(%s)' % name
        self.filename = name.strip('"')
        self.inputs = []
        self.outputs = []
        self.__doc__ = ('Declares the source file for this code. '
                        'Informational only, like #FILE in C.')
        
    def process(self, mode):
        mode.file(self.filename)
        
#These 5 instructions are for use by GC code only.

class ZeroMemory(Instruction):
    
    def __init__(self):
        self.name = '__ZERO_MEMORY'
        self.inputs = [ 'object', 'size' ]
        self.outputs = [ ]
        self.__doc__ = ('Zero the memory for newly allocated object.' 
                        'For internal use only. Used by GC-inline pass')
        
    def process(self, mode):
        size = mode.stack_pop(gtypes.uptr)
        obj = mode.stack_pop(gtypes.r)
        mode.zero_memory(obj, size)


class GC_FreePointerLoad(Instruction):

    def __init__(self):
        self.name = '__GC_FREE_POINTER_LOAD'
        self.inputs = []
        self.outputs = [ 'free_pointer' ]
        self.__doc__ = ('Load the free pointer. ' 
                        'For internal use only. Used by GC-inline pass')
        
    def process(self, mode):
        mode.stack_push(mode.gc_free_pointer_load())

class GC_FreePointerStore(Instruction):
    
    def __init__(self):
        self.name = '__GC_FREE_POINTER_STORE'
        self.inputs = [ 'free_pointer' ]
        self.outputs = []
        self.__doc__ = ('Store to the free pointer. ' 
                        'For internal use only. Used by GC-inline pass')
        
    def process(self, mode):
        mode.gc_free_pointer_store(mode.stack_pop(gtypes.p))

class GC_LimitPointerLoad(Instruction):
    
    def __init__(self):
        self.name = '__GC_LIMIT_POINTER_LOAD'
        self.inputs = []
        self.outputs = [ 'limit_pointer' ]
        self.__doc__ = ('Load the limit pointer. ' 
                        'For internal use only. Used by GC-inline pass')
        
    def process(self, mode):
        mode.stack_push(mode.gc_limit_pointer_load())

class GC_LimitPointerStore(Instruction):
    
    def __init__(self):
        self.name = '__GC_LIMIT_POINTER_STORE'
        self.inputs = [ 'limit_pointer' ]
        self.outputs = []
        self.__doc__ = ('Store to the limit pointer. ' 
                        'For internal use only. Used by GC-inline pass')
        
    def process(self, mode):
        mode.gc_limit_pointer_store(mode.stack_pop(gtypes.p))
        
class Lock(Instruction):
    
    def __init__(self):
        self.name = 'LOCK'
        self.inputs = [ 'lock' ]
        self.outputs = []
        self.__doc__ = 'Lock the gvmt-lock pointed to by TOS. Pop TOS.'
        
    def process(self, mode):
        mode.lock(mode.stack_pop(gtypes.iptr))
        
class Unlock(Instruction):
    
    def __init__(self):
        self.name = 'UNLOCK'
        self.inputs = [ 'lock' ]
        self.outputs = []
        self.__doc__ = 'Unlock the gvmt-lock pointed to by TOS. Pop TOS.'
        
    def process(self, mode):
        mode.unlock(mode.stack_pop(gtypes.iptr))
        
class Lock_Internal(Instruction):
    
    def __init__(self):
        self.name = 'LOCK_INTERNAL'
        self.inputs = [  'offset', 'object' ]
        self.outputs = []
        self.__doc__ = '''Lock the gvmt-lock in object referred to by TOS
        at offset NOS. Pop both reference and offset from stack.'''
        
    def process(self, mode):
        obj = mode.stack_pop(gtypes.r)
        offset = mode.stack_pop(gtypes.uptr)
        mode.lock_internal(obj, offset)
        
class Unlock_Internal(Instruction):
    
    def __init__(self):
        self.name = 'UNLOCK_INTERNAL'
        self.inputs = [  'offset', 'object' ]
        self.outputs = []
        self.__doc__ = '''Unlock the fast-lock in object referred to by TOS
        at offset NOS. Pop both reference and offset from stack.'''
        
    def process(self, mode):
        obj = mode.stack_pop(gtypes.r)
        offset = mode.stack_pop(gtypes.uptr)
        mode.unlock_internal(obj, offset)
        
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
    
    for cls in [ Jump, Sign, GC_Malloc, GC_Malloc_Call, PushCurrentState,
               GC_Safe, GC_Safe_Call, Stack, Insert, Unlock_Internal,
               Opcode, Lock_Internal, Transfer, Raise,
               PopState, IP, Zero, DropN, Symbol, FarJump, ZeroMemory, 
               GC_FreePointerStore, GC_FreePointerLoad, GC_Malloc_Fast, Drop,
               GC_LimitPointerStore, GC_LimitPointerLoad, Next_IP, PinnedObject,
               GC_Allocate_Only, FullyInitialized, Lock, Unlock, Pin ]:
        i = cls()
        instructions[i.name] = i
    for x in (1,2,4):
        i = IP_Fetch(x)
        instructions[i.name] = i
    for tipe in reg_types:
        for cls in [Call, V_Call, Return, Pick]:
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
            instructions[bin.name] = bin
            
    for tipe in int_types:
        unary = UnaryOp(operators.inv, tipe)
        instructions[unary.name] = unary
            
    for tipe in float_types + [ gtypes.i4, gtypes.i8]:
        unary = UnaryOp(operators.neg, tipe)
        instructions[unary.name] = unary
        
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
    instructions[bin.name] = bin
    bin = ComparisonOp(operators.ne, gtypes.r)
    instructions[bin.name] = bin
    i = FieldIsNull(True)
    instructions[i.name] = i
    i = FieldIsNull(False)
    instructions[i.name] = i
              
    for op in [ operators.add, operators.sub, operators.mul, operators.div ]:
        for tipe in int_types + float_types:
            bin = BinaryOp(op, tipe)
            instructions[bin.name] =  bin
    
    for i in instructions.itervalues():
        if not hasattr(i, 'annotations'):
            i.annotations = []
        i.annotations.append('private')
    #Alias pointer sized int ops with PTR suffix
    size_char = str(gtypes.p.size)
    for name, inst in instructions.items():
        if (name[-2] == 'I' or name[-2] == 'U') and name[-1] == size_char:
            instructions[name[:-1] + 'PTR'] = inst
    for name, cls in instruction_factories.items():
        if (name[-2] == 'I' or name[-2] == 'U') and name[-1] == size_char:
            instruction_factories[name[:-1] + 'PTR'] = cls

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
