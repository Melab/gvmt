#GSC objects are the in-memory representation of a GSC file
import re
import sys
import common
import builtin
import gtypes
from compound import CompoundInstruction

import random
random.seed()

def error(s):
    raise common.UnlocatedException(s)

class GSC(object):
    '''
    A GSC object represents a GSC file or files.
    @ivar bytecodes: The bytecode section
    @ivar code: The code section
    @ivar roots: The roots section
    @ivar opaque: The opaque section
    @ivar heap: The heap section
    '''
    
    def __init__(self, bytecodes, code, heap, opaque, roots, info):
        self.bytecodes = bytecodes
        self.code = code
        self.heap = heap
        self.roots = roots
        self.opaque = opaque
        self.info = info
        assert bytecodes is None or isinstance(bytecodes, CodeSection)
        assert isinstance(code, CodeSection)
        assert isinstance(heap, DataSection)
        assert isinstance(roots, DataSection)
        assert isinstance(opaque, DataSection)
        
    def write_data(self, out):
        self.heap.write(out)
        self.roots.write(out)
        self.opaque.write(out)
        
    def write(self, out):
        if self.bytecodes:
            out << '.bytecodes\n'
            self.bytecodes.write(out)
        out << '.code\n'
        self.code.write(out)
        self.write_data(out)
        self.info.write(out)
        
    def make_unique(self):
        renames = {}
        public = set()
        if self.bytecodes:
            for sect in self.heap, self.opaque, self.roots:
                if self.bytecodes.func_name:
                    sect.make_unique(renames, public, self.bytecodes.func_name)
                else:
                    sect.make_unique(renames, public, "gvmt_interpreter")
            self.bytecodes.rename(renames)
        else:
            for sect in self.heap, self.opaque, self.roots:
                sect.make_unique(renames, public, False)
            self.code.rename(renames)

def read(infile):
    '''
    Read a GSC file.
    @return: A GSC object.
    '''
    line = infile.readline()
    bytecodes = None
    code = CodeSection('code')
    heap = DataSection('heap', True)
    roots = DataSection('roots', False)
    opaque = DataSection('opaque', False)
    info = InformationSection()
    section = code
    while line:
        stripped = line.strip()
        try:
            if not stripped:
                pass
            elif stripped and stripped[0] == '.':
                if stripped == '.bytecodes':
                    if bytecodes is None:
                        bytecodes = BytecodeSection()
                    section = bytecodes
                elif stripped == '.code':
                    section = code
                elif stripped == '.heap':
                    section = heap
                elif stripped == '.opaque':
                    section = opaque
                elif stripped == '.roots':
                    section = roots
                elif stripped.startswith('.local'):
                    if section == bytecodes:
                        section.local(stripped[6:].strip())
                    else:
                        error('.local only allowed in bytecodes section')
                elif stripped.startswith('.master'):
                    if section == bytecodes:
                        section.master = True
                    else:
                        error('.master only allowed in bytecodes section')
                elif stripped.startswith('.public'):
                    section.public(stripped.split()[1].strip())
                elif stripped.startswith('.label'):
                    section.label(stripped.split()[1].strip())
                elif stripped.startswith('.object'):
                    section.object(stripped.split()[1].strip())
                elif stripped.startswith('.name'):
                    section.set_name(stripped.split()[1].strip())
                elif stripped.startswith('.end'):
                    section.end()
                elif stripped.startswith('.type'):
                    info.type(stripped[5:])
                elif stripped.startswith('.member'):
                    info.member(stripped[7:])
                else:
                    error("Illegal directive '%s'" % stripped)
            else:
                items = _ITEM.findall(line)
                if '//' in items:
                    items = items[:items.index('//')]
                section.line(items)
        except common.UnlocatedException as ex:
            raise ex.locate(infile.name, infile.line-1)
        
        line = infile.readline()
    code.close()
    if bytecodes:
        bytecodes.close()
    heap.close()
    roots.close()
    return GSC(bytecodes, code, heap, opaque, roots, info) 
 
def _remove_semi(insts):
    if not insts[-1].endswith(';'):
        error("Stuff after semicolon")
    result = insts[:-1]
    if insts[-1] != ';':
        result.append(insts[-1][:-1])
    return result
 
_ITEM = re.compile(r'\"(?:\\"|[^"])*\"|;|:|=|\[|\]|//|#[^ \t\n]+|[^ "\t\n\[\]=:;]+')
_NAME = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
_NUMBER = re.compile(r'[0-9]+')
 
class CodeSection(object):
    
    def __init__(self, name):
        self.instructions = []
        self.locals = []
        self.list = []
        self.name = name
        self.idict = dict(builtin.instructions)
        
    def _semi(self):
        qualifiers = []
        # Parse start of instruction here...
        if not self.list:
            error("Syntax Error")
        if not _NAME.match(self.list[0]):
            error("Expecting name found '%s'" % self.list[0])
        else:
            name = self.list[0]
        index = 1
        opcode = None
        if self.list[index] == '=':
            if not _NUMBER.match(self.list[index + 1]):
                error("Expecting number found '%s'" % self.list[index+1])
            opcode = int(self.list[index + 1])
            index += 2
        if self.list[index] == '[':
            index += 1
            while _NAME.match(self.list[index]):
                qualifiers.append(self.list[index])
                index += 1
            if self.list[index] != ']':
                error("Expecting ']' found '%s'" % self.list[index])
            index += 1
        if self.list[index] != ':':
            error("Expecting ':' found '%s'" % self.list[index])
        index += 1
        ilist = builtin.parse(self.list[index:], name, self.idict)
        if name in self.idict:
            error("Duplicate name '%s'" % name)
        try:
            inst = CompoundInstruction(name, opcode, qualifiers, ilist)
        except common.UnlocatedException as ex:
            filename = None
            line_number = 0
            for i in ilist: 
                if i.__class__ == builtin.File:
                    filename = i.filename
                if i.__class__ == builtin.Line:
                    line_number = i.line
            if filename and line_number:
                raise ex.locate(filename, line_number)
            else:
                raise ex
        self.instructions.append(inst)
        self.idict[name] = inst
        self.list = []
    
    def line(self, items):
        for i in items:
            if i == ';':
                self._semi()
            else:
                self.list.append(i)
        
    def rename(self, renames):
        for i in self.instructions:
            i.rename(renames)
                
    def close(self):
        if self.list:
            error("Unterminated instruction at end of file")
        
    def local(self, name):
        self.locals.append(name.split())
        
    def label(self, name):        
        error('.label not allowed in %s section' % self.name)
        
    def public(self, name):
        error('.public not allowed in %s section' % self.name)
                
    def object(self, name):
        error('.object not allowed in %s section' % self.name)
        
    def end(self):
        error('.end not allowed in %s section' % self.name)
        
    def write(self, out):
        for i in self.instructions:
            i.write(out)
        for l in self.locals:
            out << '.local ' <<  l[0] << ' ' << l[1] << '\n'
            
    def lcc_post(self):
        for i in self.instructions:
            i.lcc_post()    
            
    def set_name(self, name):
        error('.name not allowed in %s section' % self.name)
           
class BytecodeSection(CodeSection):
    
    def __init__(self):
        CodeSection.__init__(self, 'bytecodes')
        self.func_name = 'interpreter'
        self.master = False
    
    def write(self, out):
        for i in self.instructions:
            i.write(out)
        for l in self.locals:
            out << '.local ' <<  l[0] << ' ' << l[1] << '\n'
        if self.func_name:
            out << '.name ' <<  self.func_name << '\n'
        if self.master:
            out << '.master\n'
            
    def set_name(self, name):
        self.func_name = name
            
        
_TYPE = r'int8|int16|int32|uint8|uint16|uint32|float32|float64|label|public|string|object|address'
#VAL = r'\".*\"|\w+|\d+(?:\.\d+)'
_DATA = re.compile(r' *(%s) +(.*) *' % _TYPE)


_ID_CHARS = set('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_')

def _legal_identifier(name):
    for c in name:
        if c not in _ID_CHARS:
            return False
    return True

_MEMBER = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*) +([^ @]+) *@ *(\d+)$')

def verify_type(tipe):
    if not tipe:
        error("'' is not a legal type")
    if '*' in tipe:
        l = tipe.split('*')
        if len(l) != 2:
            error("'%s' is not a legal type" % tipe)
        try:
            count = int(l[1])
        except:
            error("'%s' is not an integer" % l[1])
        verify_type(l[0])
    elif tipe[0] == 'P':
        if tipe[1] == '(' and tipe[-1] == ')':
            verify_type(tipe[2:-1])
        else:
            error("'%s' is not a legal type" % tipe)
    elif tipe[0] == 'S' or tipe[0] == 'R':
        if tipe[1] != '(' or tipe[-1] != ')': 
            error("'%s' is not a legal type" % tipe)
    else:
        if tipe not in ('?', 'int8' ,'int16', 'int32', 'int64',
                            'uint8' ,'uint16', 'uint32', 'uint64',
                            'float32', 'float64'):
             error("'%s' is not a legal type" % tipe)
       
class Type(object):
    
    def __init__(self, line):
        kn = line.strip().split()
        if len(kn) != 2:
            error("'%s' is not a legal Type header" % line)
        self.kind, self.name = kn
        self.members = []
        
    def member(self, line):
        items = _MEMBER.match(line.strip())
        if items is None:
            error("'%s' is not a legal member desription" % line)
        name, tipe, offset = items.groups()
        verify_type(tipe)
        self.members.append((name, tipe, int(offset)))
        
    def write(self, out):
        out << '.type ' << self.kind << ' ' << self.name << '\n'
        for m in self.members:
            out << '.member %s %s@%s\n' % m
        
class InformationSection(object):
    
    def __init__(self):
        self.types = []
    
    def type(self, line):
        self.types.append(Type(line))
       
    def member(self, line):
        if self.types:
            self.types[-1].member(line)
        else:
            error("'.member without a .type")
            
    def write(self, out):
        for t in self.types:
            t.write(out)

class DataSection(object):
    
    def __init__(self, name, objects):
        self.items = []
        self.objects = objects
        self.name = name
        self.struct = None
        self.unlabelled = True
        
    def line(self, items):
        if len(items) != 2:
            error("Illegal data item: '%s'" %  ' '.join(items))
        type, val = items[0], items[1]
        val = val.strip()
        if type == 'string':
            if val[0] != '"' or val[-1] != '"':
                error("Malformed string: %s" % val)
        elif type == 'address':
            if (not _legal_identifier(val)) and val != '0':
                error("Illegal name: %s" % val)
        elif type == 'object':
            if not _legal_identifier(val):
                error("Illegal name: %s" % val)
        elif type.startswith('float'):
            try:
                float(val)
            except ValueError:
                error("Illegal float: %s"  % val)
        else:
            try:
                int(val)
            except ValueError:
                print len(items), items
                error("Illegal integer: %s"  % val)               
        self.items.append((type, val))
        if self.unlabelled:
            if self.objects:
                error("Data outside of object")
            else:
                error("Unlabelled data")
        
    def close(self):
        pass
        
    def label(self, name):
        if self.objects:
            error('.label not allowed in %s section' % self.name)
        else:
            self.unlabelled = False
            self.items.append((".label", name))
        
    def public(self, name):
        self.items.append((".public", name))
            
    def set_name(self, name):
        error('.name not allowed in %s section' % self.name)
        
    def object(self, name):
        if self.objects:
            self.items.append((".object", name))
        else:
            error('objects not allowed in %s section' % self.name)
        if not self.unlabelled:
            error('Unterminated object')
        self.unlabelled = False
        
    def end(self):
        if self.objects:
            self.items.append((".end", ''))
        else:
            error('objects not allowed in %s section' % self.name)
        if self.unlabelled:
            error('No matching .object for .end')
        self.unlabelled = True

    def write(self, out):
        if self.items:
            out << '.%s\n' % self.name
            for typ, val in self.items:
                out << typ << ' ' << val << '\n'
        
    def make_unique(self, renames, public, name):
        prefix = 'gvmt_%s' % random.randint(10000000, 99999999)
        index = 0
        result = []
        for i1, i2 in self.items:
            if i1[0] == '.':
                if i1 == '.label':
                    if i2 not in public:
                       if name:
                           renames[i2] = 'gvmt_idata_%s_%s' % (name, i2)
                       else:
                           renames[i2] = '%s%d' % (prefix, index)
                    index += 1
                elif i1 == '.public':
                    if i2 in public:
                        error("Multiple defintions of %s" % i2)
                    public.add(i2)
        for i1, i2 in self.items:
            if i2 in renames:
                if i1 == '.label' or i1 == 'address':
                    i2 = renames[i2]
            result.append((i1, i2))
        self.items = result
        
    def empty(self):
        return len(self.items) == 0
         
def _write_insts(c, out):
    no_line = True
    for i in c.instructions:
        if no_line:
            no_line = False
        elif i.__class__ is File :
            out << '\n'
            no_line = True
        elif i.__class__ is Line :
            out << '\n'
        out << i.name << ' '

