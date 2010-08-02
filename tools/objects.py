#!/usr/bin/python
# New types handling code


import sys, re, getopt
import common
import gtypes

TYPE = r'u?int(?:8|16|32|64)?|float|double|(P|R)(?:\(\w+(?:\*| )*\))?'
SUPER = r'(?:\: *(\w+))?'
QUALIFIERS = r'(?:\( *(\w+(?: +\w+)*) *\))?'

HEADER = re.compile(r'^(\w+) *%s *%s *\{$' % (SUPER, QUALIFIERS))
ENTRY = re.compile(r'([^;\[\]]*) (\w+) *(?:\[(\d*)\])? *%s *(?:;(.*))?$' % QUALIFIERS)
SEMICOLON = re.compile(';')
EMPTY = re.compile(r'( |\t)*$')
SPACES = re.compile(r'[ \t]+')
CLOSE = re.compile(r' *\} *')

legal_types = { 'char' : gtypes.u1, 'signed char' : gtypes.i1,  'unsigned char' : gtypes.u1, 
                 'int8_t' : gtypes.i1, 'uint8_t' : gtypes.u1, 
                 'int16_t' : gtypes.i2, 'uint16_t' : gtypes.u2, 
                 'int32_t' : gtypes.i4, 'uint32_t' : gtypes.u4, 
                 'int64_t' : gtypes.i8, 'uint64_t' : gtypes.u8, 
                 'short' : gtypes.i2, 'unsigned short' : gtypes.u2, 
                 'int' : gtypes.i4, 'unsigned' : gtypes.u4, 'unsigned int' : gtypes.u4, 
                 'float' : gtypes.f4, 'double' : gtypes.f8, 
                 'float32' : gtypes.f4, 'float64' : gtypes.f8
                 }
                 
if gtypes.p.size == 4:
    legal_types['long'] = gtypes.i4
    legal_types['unsigned long'] = gtypes.u4
    legal_types['intptr_t'] = gtypes.i4
    legal_types['uintptr_t'] = gtypes.u4
else:
    legal_types['long'] = gtypes.i8
    legal_types['unsigned long'] = gtypes.i8
    legal_types['intptr_t'] = gtypes.i8
    legal_types['uintptr_t'] = gtypes.u8

def parse_type(txt):
    if txt.startswith('R('):
        if txt[-1] != ')':
            raise common.UnlocatedException("Illformed reference type '%s'" % txt)
        return gtypes.r, txt[2:-1]
    elif txt.startswith('P('):
        if txt[-1] != ')':
            raise common.UnlocatedException("Illformed pointer type '%s'" % txt)
        return gtypes.p, txt[2:-1]
    elif txt in legal_types:
        return legal_types[txt], None
    else:
        raise common.UnlocatedException("Unrecognised type '%s'" % txt)

class Entry:
    
    def __init__(self, line): 
        tipe, name, count, qualifiers, comment = ENTRY.match(line).groups()
        self.qualifiers = qualifiers
        self.tipe, self.annotation = parse_type(tipe.strip())
        self.name = name
        if count:
            self.count = int(count)
        elif count == '':
            self.count = 0
        else:
            self.count = None
        self.comment = comment
        
    def write_c(self, out):
        if self.count is None:
            out << '    %s %s;' % (self.type_name(), self.name)
        else:
            if self.count:
                out << '    %s %s[%s];' % (self.type_name(), self.name, self.count)
            else:
                out << '    %s %s[];' % (self.type_name(), self.name)          
        out << '    /* %s %s %s %s %s */\n' % (self.tipe, self.name, 
                                         self.count, self.qualifiers, self.comment)
   
    def header(self, sname, out):
        if self.tipe == gtypes.p or self.qualifiers and 'special' in self.qualifiers:
            out << 'void user_marshal_%s_%s(FILE* out, GVMT_OBJECT(%s)* obj);\n' % (sname, self.name, sname)
        
    def write_member(self, sname, out):
        if self.tipe == gtypes.p or self.qualifiers and 'special' in self.qualifiers:
            out << '    user_marshal_%s_%s(out, object);\n' % (sname, self.name) 
            return
        if self.tipe == gtypes.r:
            write = 'gvmt_write_ref'
            cast = '(GVMT_Object)'
        else:
            write = 'gsc_write_%s' % self.type_name()
            cast = ''
        if self.count == 0:
            count = 'size'
        else:
            count = self.count
        if count:
            out << '    for (i = 0; i < %s; i++) {\n' % count
            out << '        %s(out, %sobject->%s[i]);\n' % ( write, cast, self.name)
            out << '    }\n'
        else:
            out << '    %s(out, %sobject->%s);\n' % ( write, cast, self.name)
    
    def type_name(self):
        if self.annotation:
            if self.tipe == gtypes.r:
                return 'GVMT_OBJECT(%s)*' % self.annotation
            else:
                return self.annotation
        else:
            return self.tipe.c_name
            
def part_shape(entry):
    if entry.count == 0:
        return 0
    elif entry.count:
        if entry.tipe == gtypes.r:
            return entry.count
        else:
            return - ((entry.count * entry.tipe.size + gtypes.p.size - 1) //  gtypes.p.size)    
    elif entry.tipe == gtypes.r:
        return 1
    elif entry.tipe.size > gtypes.p.size:
        return -2
    else:
        return -1
            
def _get_shape(entries):
    shape = [part_shape(entries[0])]
    total_size = shape[0]
    for e in entries[1:]:
        if e.tipe.size > gtypes.p.size and total_size % 2:
            # Needs aligning
            if shape[-1] > 0:
                shape.append(-1)
            else:
                shape[-1] -= 1
        s = part_shape(e)
        if shape[-1] * s > 0:
            shape[-1] += s
        else:
            shape.append(s)
        if s < 0:
            total_size -= s
        else:
            total_size += s
    return tuple(shape)
    
marshal_id = 0
            
class Struct:
    
    def __init__(self, name, qualifiers):
        self.name = name
        self.entries = []
        self._flattened = False
        self.qualifiers = qualifiers
        self.base = None
        
    def _flatten(self):
        if not self._flattened:
            self._flattened = True
            if self.base:
                self.entries = self.base._flatten() + self.entries
            self.shape = _get_shape(self.entries)
        return self.entries

    def write_c(self, out, pad):
        out << 'GVMT_OBJECT(%s) {\n' % self.name
        if pad:
            offset = 0
            for e in self.entries:
                align = e.tipe.size             
                if offset % align:
                    pad = align - offset % align
                    out << '    char %s_align[%d];\n' % (e.name, pad)
                    offset += pad
                e.write_c(out)
                offset += e.tipe.size
        else:
            for e in self.entries:
                e.write_c(out)
        out << '};\n'
        
    def typedef(self, pattern, out):
        out << 'typedef GVMT_OBJECT(%s) *%s;\n' % (self.name, pattern % self.name)

    def write_func(self, out):
        if self.qualifiers and 'special' in self.qualifiers:
            return
        out << 'void gvmt_marshal_%s(FILE* out, GVMT_OBJECT(%s)* object) {\n' % (self.name, self.name) 
        for e in self.entries:
            if e.count is not None:
                out << '    int i;\n'
                break
        if self.entries and self.entries[-1].count == 0:
            out << '    int size;\n'
            out << '    size = (object_size((GVMT_Object)object) - sizeof(GVMT_OBJECT(%s)))/ sizeof(%s);\n' % (self.name, self.entries[-1].type_name())
        for e in self.entries:
            e.write_member(self.name, out)
        out << '    gsc_footer(out);\n'
        out << '}\n\n'
        
    def header(self, out):
        global marshal_id
        marshal_id += 1
        out << '#define GVMT_MARSHALL_ID_%s %d\n' % (self.name, marshal_id)
        for e in self.entries:
            e.header(self.name, out)
        if self.qualifiers and 'special' in self.qualifiers:
            out << 'void gvmt_marshal_%s(FILE* out, GVMT_OBJECT(%s)* object);\n' % (self.name, self.name) 
        
def read_file(src):
    'Reads the object description file and returns a list of Structs'
    current = None
    line_no = src.line
    line = src.readline()
    structs = {}
    bases = {}
    try:
        while line:
            line = line.strip()
            if HEADER.match(line):
                if current:
                    print "Nested struct at line %d" line_no
                    sys.exit(1)
                current, base, quals = HEADER.match(line).groups()
                current = current.strip()
                structs[current] = Struct(current, quals)
                bases[current] = base
            elif ENTRY.match(line):
                if current:
                    structs[current].entries.append(Entry(line))
                else:
                    print "Entry outside of struct at line %d", line_no
                    sys.exit(1)
            elif EMPTY.match(line):
                pass
            elif CLOSE.match(line):
                current = None
            else:
                print "Unrecognised line, '%s' at line %d" % (line, line_no) 
                sys.exit(1)
            line_no = src.line
            line = src.readline()
        for key in structs:
            if bases[key]:
                structs[key].base = structs[bases[key]]
            else:
                structs[key].base = None
        result = list(structs.values())
        for s in result:
            s._flatten()
        return result
    except common.UnlocatedException, ex:
        raise ex.locate(src.name, line_no)

    
options = {
    'h' : "Print this help and exit",
    't' : 'Output header with typedefs for C code',
    's' : 'Output header with stuct definitions for C code',
    'm' : 'Output marshalling code for all kinds',
    'p' : 'Define a pattern to use for typedefs',
    'o outfile-name' : 'Specify output file name',
}

## Will need to modify this to support compilers other than GCC
def pragma_pack(s):
    return '#pragma pack(%s)\n' % s
    
def _typedefs(structs, out):
    out << '#include <inttypes.h>\n'
    out << '#include "gvmt/gvmt.h"\n'
    if pattern:
        for s in structs:
            s.typedef(pattern, out);
    
def _structs(structs, out, pack = False):
    out << '#include <inttypes.h>\n'
    if pack:
        out << '#include "gvmt/native.h"\n'
    else:
        out << '#include "gvmt/gvmt.h"\n'
    if pack:
        out << pragma_pack(1)
    for s in structs:
        s.write_c(out, pack)
    
def header(structs, out):
    _header(structs, out)
    out.close()
    
def _header(structs, out):
    out << '#include "gvmt/marshaller.h"\n'
    for s in structs:
        s.header(out)
   
def sizeof(size):
    if size == '-' or size == '+':
        return 'sizeof(void*)'
    else:
        return size
    
def funcs(structs, c, h):
    _header(structs, h)
    h.close()
    c << '#include "gvmt/marshaller.h"\n'
    c << '#include "%s"\n' % h.name
    for s in structs:
        s.write_func(c)
    c.close()
       
pattern = 'R_%s'

if __name__ == '__main__':
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'mstho:p:')
    except:
        common.print_usage(options)
        sys.exit(1)
    if len(args) != 1:
        common.print_usage(options)
        sys.exit(1)
    action = ''
    out = None
    for opt, value in opts:
        if opt == '-h':
            common.print_usage(options)
            sys.exit(1)
        elif opt == '-t':
            if 'm' in action:
                print "-m may not be chosen with any other option"
                sys.exit(1)
            action += 't'
        elif opt == '-s':
            if 'm' in action:
                print "-m may not be chosen with any other option"
                sys.exit(1)
            action += 's'
        elif opt == '-m':
            if action:
                print "-m may not be chosen with any other option"
                sys.exit(1)
            action = 'm'
        elif opt == '-o':
            out = common.Out(value)
        elif opt == '-t':
            pattern = value
    if not action:
        print "Either -c or -m must be chosen"
        sys.exit(1)
    shapes = read_file(common.In(args[0]))  
    if out is None:
        out = common.Out(args[0] + '.c')
    if action == 'm':  
        if out.name[-2:] == '.c':
            hname = out.name[:-2] + '.h'
        else:
            hname = out.name + '.h'
        funcs(shapes, out, common.Out(hname))
    else:
        out << '''#ifndef __GVMT_OBJECTS_%s_H
#define __GVMT_OBJECTS_%s_H

''' % (action.upper(), action.upper())
        if 's' in action:
            _structs(shapes, out, False)
        if 't' in action:
            _typedefs(shapes, out)
        out << '#endif\n'
        out.close()
 
