#!/usr/bin/python
# I-gen
# Interpreter Generator

import sys, re, getopt
import os.path
from common import In, Out, GVMTException
from c_mode import CMode
from stacks import *
import gsc, objects
import driver
import sys_compiler
import parser, lex, common
import gtypes

#def _get_type(decl):
#    t = decl.type
#    assert isinstance(t, str)
#    if t[-1] == '*':
#        return gtypes.p
#    elif t[:2] == 'R_':
#        return gtypes.r
#    elif t in objects.legal_types:
#        return objects.legal_types[t]
#    else:
#        raise GVMTException("%s: Unrecognised type: '%s'" % (decl.location, t))

def c_line(line, file):
    'Returns a C preprocessor directive to set the line number and filename'
    return '#line %d "%s"\n' % (line , file)

def _ip_fetch(c, location):
    if c == 1:
        return 'gvmt_fetch()'
    elif c == 2 or c == 4:
        return 'gvmt_fetch%d()' % c
    elif c > gtype.p.size:
        raise GVMTException("%s: Too large an operand", location)
    elif c & 1:
        return '((%s << 8) | gvmt_fetch())' % _ip_fetch(c-1)
    elif c == 6:
        return '((gvmt_fetch4() << 16) | gvmt_fetch2())'
    else:
        assert "Impossible count" and False
        
def _c_function(name, location, qualifiers, code, out, local_vars):
    out << c_line(location.line, location.file)
    if 'private' in qualifiers:
            out << 'static '
    out << 'struct gvmt_illegal_return*  gvmt_lcc_%s (' % name
    sep = ''
    defn = {}
    varargs = False
    inputs = []
    arguments = []
    for item in code.stack.inputs:
        if item.name[0] == '#':
            arguments.append(item)
        else:
            inputs.append(item)
    if inputs:
        for item in inputs[::-1]:
            if item.name not in defn:
                defn[item.name] = item.type
                if item.type == '...':
                    varargs = item.name
                else:
                    out << sep
                    out << '%s %s' % (item.type, item.name)
                    sep = ', '
            else:
                raise GVMTException("%s: Multiple use of name '%s'" % (item.location, item.name)) 
    if not sep:
        out << 'void'
    out << ') {\n'
    if varargs:
        out << '    GVMT_Object *%s = gvmt_stack_top();\n' % varargs
    for v in local_vars:
        if v.location:
            out << c_line(v.location.line, v.location.file)
        out << '    { %s %s;\n' % (v.type, v.name)  
    out << c_line(code.start.line, code.start.file)
    for item in arguments:
        if item.name not in defn:
            defn[item.name] = item.type
            out << '    %s %s;' % (item.type, item.name.lstrip('#'))
        else:
            raise GVMTException("%s: Multiple use of name '%s'" % (item.location, item.name.lstrip('#')))
    for item in code.stack.outputs:
        if item.name not in defn:
            defn[item.name] = item.type
            out << '    %s %s;' % (item.type, item.name)
        elif item.type != defn[item.name]:
            raise GVMTException("%s: '%s' has different types" % (item.location, item.name))
    out << '\n'
    out << c_line(location.line, location.file)
    for item in arguments:
        out << '    %s = %s;\n' % (item.name.lstrip('#'), _ip_fetch(item.name.count('#'), item.location))
    for v in local_vars:
        out << '    (void)&%s;\n' % v.name
    out << c_line(code.start.line, code.start.file)
    out << code.code << '\n'
    out << c_line(code.start.line, code.start.file)
    for item in code.stack.outputs:
        out << 'GVMT_PUSH(%s); ' % item.name 
    out << '    return (struct gvmt_illegal_return*)0;\n'
    for v in local_vars:
        out << '}'    
    out << '}\n\n'


def compile_instructions(int_ast, flags, lcc_dir, prefix = None):
    cpp = int_ast.directives
    variables = int_ast.locals
    c_insts = []
    gsc_insts = []
    stack_insts = []
    opcodes = {}
    names = set()
    for i in int_ast.instructions:
        if i.name in names:
            raise GVMTException("%s: Duplicate instruction name '%s'" % 
                                (i.location, i.name))
        else:
            names.add(i.name)
        if i.code.__class__ == parser.CCode:
            assert i.code.code is not None
            c_insts.append(i)
            if i.opcode is not None:
                opcodes[i.name] = i.opcode
        else:
            gsc_insts.append(process_compound(i))
    tempdir = os.path.join(sys_compiler.TEMPDIR, 'gvmtic')
    if not os.path.exists(tempdir):
        os.makedirs(tempdir)
    lcc_in = os.path.join(tempdir, 'functions.c')
    out = Out(lcc_in)
    for tkn in cpp:
        out << '#line %d "%s"\n' % (tkn.location.line, tkn.location.file)
        out << tkn.text
    out << '#include "gvmt/gvmt.h"\n'
    if prefix:
        out << prefix
    out << '\n'
    if variables:
        out << 'struct __gvmt_bytecode_locals_t {\n' 
        for v in variables:
            out << '#line %d "%s"\n' % (v.location.line, v.location.file)
            out << v.type << ' ' << v.name << ';\n'     
        out << '};\n'
        struct = [ parser.Declaration('struct __gvmt_bytecode_locals_t', 
                                       '__gvmt_bytecode_locals', None) ]
    else:
        struct = []
    if c_insts:
        i = c_insts[0]
        _c_function(i.name, i.location, i.qualifiers, i.code, out, struct + variables)
    for i in c_insts[1:]:
        _c_function(i.name, i.location,  i.qualifiers, i.code, out, variables)
    out.close()
    lcc_out = os.path.join(tempdir, 'functions.s')
    flags.append('-Wf-xbytecodes');
    driver.run_lcc('gvmt', lcc_in, flags, lcc_out, lcc_dir)
    f = open(lcc_out, 'a')
    print >> f, '.bytecodes'
    for i in stack_insts:
        print >> f, i 
    for i in gsc_insts:
        print >> f, i
    f.close()
    gsc_file = gsc.read(In(lcc_out))
    if gsc_file.bytecodes:
        gsc_file.bytecodes.lcc_post()
        for i in gsc_file.bytecodes.instructions:
            if i.name in opcodes:
                i.opcode = opcodes[i.name]
    return gsc_file
    
def get_output(opts):
    outfile = None
    for opt, value in opts:
        if opt == '-o':
            outfile = Out(value)
    return outfile

def process_compound(inst):
    lines = True
    for t in inst.code:
        if t.text == 'FILE' or t.text == 'LINE':
            lines = False
    sep = True
    code = ['%s' % inst.name]
    if inst.opcode is not None:
        code.append('=%d' % inst.opcode)
    for q in inst.qualifiers:
        if q not in common.legal_qualifiers:
            raise GVMTException("%s: Illegal qualifier '%s'" % (inst.location, q))
    if inst.qualifiers:
        code.append('[%s]' % ' ' .join(inst.qualifiers))
    code.append(':')
    file = None
    line = -1
    for t in inst.code:
        if lines:
            if t.location.file != file:
                file = t.location.file
                code.append('FILE("%s") ' % file)
            if t.location.line != line:
                assert sep
                line = t.location.line
                code.append('\nLINE(%d) ' % line)
        if t.kind == lex.LPAREN:
            sep = False
        elif  t.kind == lex.RPAREN:
            sep = True 
        else:
            if sep:
                code.append(' ')
        code.append(t.text)
    code.append(';')
    return ''.join(code)


options = {
    'h' : "Print this help and exit",
    'I file' : "Add file to list of #include file for lcc sub-process, can be used multiple times",
    'o outfile-name' : 'Specify output file name',
    'v' : 'Echo sub-processes invoked',
    'a' : 'Warn about non-ANSI C.',
    'b bytecode-header-file' : 'Specify bytecode header file',
    'n name' : 'Name of generated interpreter',
    'D symbol' : 'Define symbol in preprocessor',
    'z' : 'Do not put gc-safe-points on backward edges'
}        


if __name__ == '__main__':
    opts, args = getopt.getopt(sys.argv[1:], 'avI:b:hzo:D:n:L:')
    flags = []
    int_name = None
    if len(args) != 1:
        common.print_usage(options, 'interpreter-defn')
        sys.exit(1)
    try:       
        bytecode_h = None
        gc_safe = True
        lcc_dir = None
        for opt, value in opts:
            if opt == '-h':
                common.print_usage(options, 'interpreter-defn')
                sys.exit(0)
            elif opt == '-I':
                flags.append('-I%s' % value)
            elif opt == '-b':
                bytecode_h = value
            elif opt == '-D':
                flags.append('-D%s' % value)
            elif opt == '-n':
                int_name = value
            elif opt == '-a':
                flags.append('-A')
            elif opt == '-v':
                flags.append('-v')
            elif opt == '-z':
                gc_safe = False
            elif opt == '-L':
                self.lcc_dir = value
        if gc_safe:
            flags.append('-Wf-xgcsafe')
        int_ast = parser.parse_interpreter(lex.Lexer(args[0]))
        gsc_file = compile_instructions(int_ast, flags, lcc_dir)
        if int_name:
            gsc_file.bytecodes.set_name(int_name)
        gsc_file.bytecodes.master = True
        if bytecode_h:
            import gvmtas
            gvmtas.write_header(gsc_file.bytecodes, Out(bytecode_h))
        o = get_output(opts)
        if not o:
            if len(args) == 1:
                dir, f = os.path.split(args[0])
                o = Out(os.path.join(dir, f.split('.')[0] + '.gsc'))
            else:
                raise GVMTException("No output file specified");
        gsc_file.write(o)
        o.close()
    except GVMTException, ex:
        print ex
        sys.exit(1)
        
                
        
