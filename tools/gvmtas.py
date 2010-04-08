#!/usr/bin/python
# I-gen
# Interpreter Generator

import sys, re
import getopt
import os.path
from common import In, Out, GVMTException, Buffer, UnlocatedException
from c_mode import CMode, IMode
import c_mode
from stacks import *
import gsc, gso, sys_compiler, common
from external import ExternalMode
import builtin, gtypes, gc_inliner, gc_optimiser

c_types = { 'object' : 'GVMT_Object',
            'pointer' : 'void*',
            'int8' : 'int8_t',
            'int16' : 'int16_t',
            'int32' : 'int32_t',
            'uint8' : 'uint8_t',
            'uint16' : 'uint16_t',
            'uint32' : 'uint32_t',
            'float32' : 'float',
            'float64' : 'double',
            'string' : 'char[0]',
            'address' : 'void*'
            }      
    
def get_type(cmpd):
    for i in cmpd.instructions:
        if i.__class__ == builtin.Return:
            return i.tipe
    return gtypes.v
        
def signatures_for_directives(inst, directive):
    pred = 'GVMT_CALL GVMT_StackItem* gvmt_predicate_%s(GVMT_StackItem* sp, GVMT_Frame fp)' % inst.name
    t = get_type(directive.execute)
    exe = 'GVMT_CALL GVMT_StackItem* gvmt_execute_%s(GVMT_StackItem* sp, GVMT_Frame fp)' % inst.name
    return '%s, int* IP)' % pred[:-1], '%s, int* IP)' % exe[:-1]


def assign_opcodes(bytecodes):
    names = [ None ] * 256
    for i in bytecodes.instructions:
        if i.opcode is not None:
            names[i.opcode] = i.name
    count = 0
    index = 0
    for i in bytecodes.instructions:
        if 'private' not in i.qualifiers:
            count += 1
            if i.opcode is None:
                index += 1
                while names[index]:
                    index += 1
                i.opcode = index
    
    
def write_header(bytecodes, header):
    assign_opcodes(bytecodes)
    names = [ None ] * 256
    for i in bytecodes.instructions:
        if 'private' not in i.qualifiers:
            assert i.opcode is not None
            names[i.opcode] = i.name
            header << '#define _gvmt_opcode_length_%s_%s %d\n' % (
                bytecodes.func_name, i.name, i.flow_graph.deltas[0]+1)
    for i in range(256):
        if names[i]:
            header << '#define _gvmt_opcode_%s_%s %d\n' % (bytecodes.func_name, 
                                                           names[i], i)
    header << 'extern char* gvmt_opcode_names_%s[];\n' % bytecodes.func_name

#    header << '#define  TOTAL_OPCODES %d\n' % count

def emit_operand_table(bytecodes, out):
    table = [ None ] * 256;
    out << '#undef L\n#define L(x) &&_gvmt_label_%s_##x\n' % bytecodes.func_name
    for i in bytecodes.instructions:
        if 'private' in i.qualifiers or 'componly'  in i.qualifiers:
            continue
        table[i.opcode] = 'L(%s), ' % i.name
    out << 'static void* gvmt_operand_table[] = { '
    for i in range(256):
        if (i & 7) == 0:
            out << '\n    '
        if table[i] is None:
            out << 'L(0), '
        else:
            out << table[i]
    out << '\n};\n#undef L\n'

def write_interpreter(bytecodes, out, tracing, gc_name):
    write_header(bytecodes, out);
    out << '''
extern int sprintf(void*, ...);
uint32_t execution_count[256];

'''
    preamble = Buffer()
    switch = Buffer()
    postamble = Buffer()
    l = len(bytecodes.locals)
    inserts = 0
    mode = ExternalMode()
    for i in bytecodes.instructions:
        i.process(mode)
    mode.declarations(out)
    externals = mode.externals
    max_refs = 0
    return_type = mode.return_type
    post_check = False
    have_preamble = False
    popped = 1
    trace_inst = None
    for i in bytecodes.instructions:
        if i.name == '__preamble':
            have_preamble = True
        elif i.name == '__postamble':
            popped += 1
            post_check = True
        elif i.name == '__trace':
            trace_inst = i
    for i in bytecodes.instructions:
        if i.name == '__preamble':
            temp = Buffer()
            # Use do { } while (0) to allow far_jump in preamble
            preamble << ' do { /* Start __preamble */\n'
            mode = IMode(temp, externals, gc_name, bytecodes.func_name)
            for x in range(popped):
                mode.stack_pop()
            i.top_level(mode)
            mode.close()
            temp.close()
            mode.declarations(preamble)
            if max_refs < mode.ref_temps_max:
                max_refs = mode.ref_temps_max
            preamble << temp
            preamble << '  } while (0); /* End __preamble */\n'
            if post_check:
                preamble << 'if (_gvmt_ip >= gvmt_ip_end) goto gvmt_postamble;\n' 
            flushed = True
        elif i.name == '__postamble':
            temp = Buffer()
            # far_jump is not allowed here.
            postamble << '  gvmt_postamble: {\n'
            mode = IMode(temp, externals, gc_name, bytecodes.func_name)
            i.top_level(mode)
            mode.close()
            temp.close()
            mode.declarations(postamble);
            if max_refs < mode.ref_temps_max:
                max_refs = mode.ref_temps_max
            postamble << temp
            postamble << '  }/* */\n'
    if not have_preamble:
        preamble << '  {\n'
        temp = Buffer()
        mode = IMode(temp, externals, gc_name, bytecodes.func_name)
        for x in range(popped):
            mode.stack_pop()
        mode.close()
        mode.declarations(preamble)
        preamble << temp
        preamble << '  }\n'
    if common.token_threading:
        switch << '  goto *gvmt_operand_table[*_gvmt_ip];\n'
    else:
        switch << '  do {\n'
        switch << '  switch(*_gvmt_ip) {\n'
    for i in bytecodes.instructions:
        if 'private' in i.qualifiers or 'componly'  in i.qualifiers:
            continue
        if common.token_threading:
            switch << '  _gvmt_label_%s_%s: ((void)0); ' % (bytecodes.func_name, i.name)
        else:
            switch << '  case _gvmt_opcode_%s_%s: ' % (bytecodes.func_name, i.name)
        switch << ' /* Delta %s */ ' % i.flow_graph.deltas[1]
        switch << '{\n'
        switch << '#define GVMT_CURRENT_OPCODE _gvmt_opcode_%s_%s\n' % (bytecodes.func_name, i.name)
        if tracing and trace_inst:
            switch << 'if (gvmt_tracing_state) {\n'
            temp = Buffer()
            mode = IMode(temp, externals, gc_name, bytecodes.func_name)
            trace_inst.top_level(mode)
            if max_refs < mode.ref_temps_max:
                max_refs = mode.ref_temps_max
            try:
                mode.close()
            except UnlocatedException, ex:
                raise UnlocatedException("%s in compound instruction '%s'" % (ex.msg, i.name))
            temp.close()
            mode.declarations(switch);
            switch << temp
            switch << ' } \n'
        temp = Buffer()
        mode = IMode(temp, externals, gc_name, bytecodes.func_name)
        mode.stream_fetch() # Opcode
        i.top_level(mode)
        if max_refs < mode.ref_temps_max:
            max_refs = mode.ref_temps_max
        try:
            mode.close()
        except UnlocatedException, ex:
            raise UnlocatedException("%s in compound instruction '%s'" % (ex.msg, i.name))
        temp.close()
        mode.declarations(switch);
        switch << temp
        if post_check:
            switch << 'if (_gvmt_ip >= gvmt_ip_end) goto gvmt_postamble;\n' 
        if common.token_threading:
            switch << ' } goto *gvmt_operand_table[*_gvmt_ip];\n'
        else:
            switch << ' } break;\n'
        switch << '#undef GVMT_CURRENT_OPCODE\n'
    switch.close()
    out << '   struct gvmt_interpreter_frame { struct gvmt_frame gvmt_frame;\n'
    out << '   GVMT_Object refs[%d];\n' % max_refs
    ref_locals = 0
    for t, n in bytecodes.locals:
        if t == 'object':
            ref_locals += 1
            out << '   GVMT_Object %s;\n' % n
    for t, n in bytecodes.locals:
        if t != 'object':
            if t in c_types:
                out << '   %s %s;\n' % (c_types[t], n)
            else:
                raise UnlocatedException("Unrecognised type '%s'" % t)
    out << '   };\n' 
    out << '#define FRAME_POINTER (&gvmt_frame)\n'
    out << 'GVMT_CALL GVMT_StackItem* '
    if bytecodes.func_name:
        name = bytecodes.func_name
    else:
        name = 'gvmt_interpreter'
    out << ' %s(GVMT_StackItem* gvmt_sp, GVMT_Frame _gvmt_caller_frame) {' % name
    out << '''
    register uint8_t* _gvmt_ip; 
//    uint8_t* gvmt_ip_start;
    _gvmt_ip = (uint8_t*)gvmt_sp[0].p;
//    gvmt_ip_start = _gvmt_ip;
    GVMT_StackItem return_value;
''' 
    if common.token_threading:
        emit_operand_table(bytecodes, out)
    if post_check:
        out << '   uint8_t* gvmt_ip_end = (uint8_t*)gvmt_sp[1].p;\n'
    out << '   struct gvmt_interpreter_frame gvmt_frame;\n'
    out << '   gvmt_frame.gvmt_frame.previous = _gvmt_caller_frame;\n' 
    out << '   gvmt_frame.gvmt_frame.count = %d;\n' % (max_refs + ref_locals)
    out << '   gvmt_frame.gvmt_frame.interpreter = 1;\n'
    for i in range(max_refs):                       
        out << ' gvmt_frame.gvmt_frame.refs[%d] = 0;\n' % i
    for t, n in bytecodes.locals:
        if t == 'object':
            out << '   gvmt_frame.%s = 0;\n' % n
    default = Buffer()
    if common.token_threading:
        default << '  _gvmt_label_%s_0: ' % bytecodes.func_name
    else:
        default << '  default:\n'
    default << '''   {
    char buf[100], *p;
    p = buf + sprintf(buf, "Illegal opcode %d\\n", *_gvmt_ip);
    for (int i = -8; i < 0; i++)
        p += sprintf(p, "%d ", _gvmt_ip[i]);
    p += sprintf(p, "| ");
    for (int i = 0; i < 8; i++)
        p += sprintf(p, "%d ", _gvmt_ip[i]);
    sprintf(p, "\\n");
    __gvmt_fatal(buf);
   }
'''
    out << preamble
    out << switch
    out << default
    out.no_line()
    if not common.token_threading:
        out << '   }\n'
        out << '  } while (1);\n'
    out << postamble
    out.no_line()
    out << '} /* End */\n'
    out << '#undef FRAME_POINTER\n'
    if bytecodes.master:
        out << 'uintptr_t gvmt_interpreter_%s_locals = %d;\n' % (bytecodes.func_name, l)
        out << 'uintptr_t gvmt_interpreter_%s_locals_offset = %d;\n' % (bytecodes.func_name, max_refs)
        out <<  'GVMT_CALL GVMT_StackItem* '
        out << ' %s_resume(GVMT_StackItem* gvmt_sp, struct gvmt_frame* fp) {' % name
        out << '''
        uint8_t* gvmt_ip_start = (uint8_t*)gvmt_sp[0].p;
        struct gvmt_interpreter_frame* FRAME_POINTER = (struct gvmt_interpreter_frame*)fp;
        register uint8_t* _gvmt_ip; 
        _gvmt_ip = gvmt_ip_start;
        GVMT_StackItem return_value;
    '''
        if common.token_threading:
            emit_operand_table(bytecodes, out)
        if post_check:
            nos = 'gvmt_sp[1].p'
            out << '   uint8_t* gvmt_ip_end = (uint8_t*)%s\n;' % nos
            out << '   gvmt_sp += 2;\n'
        else:
            out << '   gvmt_sp += 1;\n'
        out << switch
        out << default
        if not common.token_threading:
            out << '   }\n'
            out << '  } while (1);\n'
        out << postamble
        out << '} /* End */\n'
    if bytecodes.master:
        write_offset(bytecodes, out)
        out << '\nchar *gvmt_opcode_names_%s[] = {' % bytecodes.func_name
        names = [ None ] * 256
        for i in bytecodes.instructions:
            if 'private' not in i.qualifiers:
                assert i.opcode is not None
                names[i.opcode] = i.name
        last = 0        
        for i in range(256):
            if names[i]:
                out << '    "%s",\n' % names[i]
            else:
                out << '    "",\n'
        out << '};\n'

def _write_func(inst, out, externals, gc_name, signature = None):
    buf = Buffer()
    mode = CMode(buf, externals, gc_name)
    out.no_line()
    buf.no_line()
    inst.top_level(mode)
    mode.stack_flush()
    buf.close()
    out << '\n'
    if 'private' in inst.qualifiers:
        out << 'static '
    if signature is None:
        signature = externals[inst.name]
    out << '%s {' % signature
#    else:
#        out << 'GVMT_StackItem %s (void) {\n' % i.name
    mode.declarations(out);
    out << buf
    out << '}\n'
    
    
def write_offset(bytecodes, out):
    out << 'unsigned gvmt_frame_index(char* name) {\n'
    el = ''
    for t, n in bytecodes.locals:
        out << '    %sif (strcmp(name, "%s") == 0)\n' % (el, n)
        out << '        return offsetof(struct gvmt_interpreter_frame, %s);\n' % n
        el = 'else '
    out << '   %s\n' % el
    out << '''        __gvmt_fatal("Unrecognised local name: '%s'\\n", name);\n'''
    out << '}\n\n'

   
def write_code(code, out, gc_name):
    mode = ExternalMode()
    for i in code.instructions:
        i.process(mode)
        if mode.return_type is None:
            mode.function(i.name, 'private' in i.qualifiers, gtypes.v)
        else:
            mode.function(i.name, 'private' in i.qualifiers, mode.return_type)
            mode.return_type = None
    externals = mode.externals   
    mode.declarations(out)
    for i in code.instructions:
        _write_func(i, out, externals, gc_name)
        
def write_type(tipe, out):
    if tipe[0] == 'P':
        write_type(tipe[2:-1], out)
        out << '*'
    elif tipe[0] == 'R':
        name = tipe[2:-1]
        if name == 'GVMT_Object':
            out << name
        else:
            out << 'GVMT_OBJECT_%s*' % name
    elif tipe[0] == 'S':
        out << 'struct _%s' % tipe[2:-1]
    elif tipe == '?':
        out << 'void'
    else:
        out << c_types[tipe]
       
def size_type(tipe):
    if tipe[0] == 'P' or tipe[0] == 'R':
        return gtypes.p.size
    elif tipe[-1] == '8':
        return 1
    elif tipe.endswith('16'):
        return 2
    elif tipe.endswith('32'):
        return 4
    elif tipe.endswith('64'):
        return 8
    else:
        raise GVMTException("GVMT Internal Error -- Do know size of '%s'" % tipe) 
        
def write_members(members, out):
    size = 0
    for m in members:
        name, tipe, offset = m
        while size < offset:
            out << 'char pad_' << size << ';\n' 
            size += 1
        if '*' in tipe:
            member_tipe, count = tipe.split('*')
            write_type(member_tipe, out)
            size += int(count)*size_type(member_tipe)
            out << ' ' << name << '[' << count << '];\n'
        elif tipe[0] == 'S':
            out << ('int%d_t ' % (gtypes.p.size*8)) << name << ';\n'
            size += gtypes.p.size
        else:
            write_type(tipe, out)
            size += size_type(tipe)
            out << ' ' << name << ';\n'

def write_info(info, out):
    decl = set()
    for t in info.types:
        if t.kind == 'object':
            decl.add(t.name)
            for m in t.members:
                tipe = m[1]
                if '*' in tipe:
                    tipe = tipe.split('*')[0]
                if tipe[0] == 'R':
                    decl.add(tipe[2:-1]) 
    for t in decl:
        out << 'typedef struct gvmt_object_%s GVMT_OBJECT_%s;\n' % (t, t)
    for t in info.types:
        if t.kind == 'struct':
            out << 'struct _' << t.name << ' {\n'
            write_members(t.members, out)
            out << '};\n'
        else:
            assert t.kind == 'object'
            out << 'struct gvmt_object_%s {\n' % t.name
            write_members(t.members, out)
            out << '};\n'    
     
from sys_compiler import CC_PATH, CC_ARGS

def to_object(src_file, base_name, library, tracing, optimise, sys_headers, gc_name):
    code = src_file.code
    bytecodes = src_file.bytecodes
    info = src_file.info
    c_file = os.path.join(sys_compiler.TEMPDIR, '%s.c' % base_name)
    out = Out(c_file)
    out << '#include <alloca.h>\n'
    out << '#include "gvmt/internal/core.h"\n'
    out << 'extern void* malloc(uintptr_t size);\n'
    out << 'extern int strcmp(void* s1, void* s2);\n'
    out << 'extern GVMT_Object gvmt_%s_malloc(GVMT_StackItem* sp, GVMT_Frame fp, uintptr_t size);\n' % gc_name
    out << 'extern void __gvmt_fatal(char* fmt, ...);\n'
    out << '#define GVMT_COMPILED\n'
    out << '\n'
    out << '\n'
    write_info(info, out)
    if bytecodes:
        write_interpreter(bytecodes, out, tracing, gc_name)
    write_code(code, out, gc_name)
    out.close()
    code = sys_compiler.c_to_object(base_name, optimise, library, True, sys_headers)
    if code:
        raise GVMTException("GVMT Internal Error -- %s failed with exit code %d" % (CC_PATH, code))
    
def to_asm(section, base_name):
    c_file = os.path.join(sys_compiler.TEMPDIR, '%s.c' % base_name)
    out = Out(c_file)
    out << '#include <inttypes.h>\n'
    to_c(section, out)
    out.close()
    code = sys_compiler.c_to_asm(base_name)
    if code:
        raise GVMTException("GVMT Internal Error -- %s failed with exit code %d" % (CC_PATH, code))

def get_output(opts):
    outfile = None
    for opt, value in opts:
        if opt == '-o':
            outfile = open(value, 'wb')
    return outfile 

name = None          
next_name = 0

def merge_section(itemlist, index, public, renames):
    result = []
    for i1, i2 in itemlist:
        if i1[0] == '.':
            if i1 == '.private':
                renames[i2] = 'gvmt_%d' % index
                index += 1
            else:
                if i2 in public:
                    raise UnlocatedException("Redefintion of %s" % i2)
                public.add(i2)
    for i1, i2 in itemlist:
        if i1[0] == '.':
            if i1 == '.private':
                i2 = renames[i2]
            result.append('%s %s' % (i1, i2))
        else:
            result.append('%s %s' % (i1, i2))
    return result

def char_seq(txt):
    clist = []
    i = 1
    while i < len(txt)-1:
        if txt[i] == '\\':
            if txt[i+1] in '01234567':            
                clist.append("'%s'" % txt[i:i+4])
                i += 4
            else:
                clist.append("'%s'" % txt[i:i+2])
                i += 2
        else:
            clist.append("'%s'" % txt[i])
            i += 1
    return clist
    
def to_c(section, out):
    global next_name
    qual = ''
    in_struct = False
    id = 0
    public = set()
    a = {}
    b = []
    c = []
    for t, n in section.items:
        if t[0] == '.':
            if t == '.object':
                next_name += 1
                a[n] = 'struct s_%d' % next_name
                b.append('struct s_%d {\n' % next_name)
#                if n not in public:
#                    c.append('static ')
                c.append('struct s_%d %s = {\n' % (next_name, n))
                in_struct = True
            elif t == '.end':
                c.append('};\n')
                b.append('};\n')
                in_struct = False
            elif t == '.public':
                public.add(n)
            elif t == '.label':
                if in_struct:
                    c.append('};\n')
                    b.append('};\n')
                elif section.name == 'roots':
                    c.append('void *begin_roots = &%s;\n' % n) 
                next_name += 1
                a[n] = 'struct s_%d' % next_name
                b.append('struct s_%d {\n' % next_name)
                c.append('struct s_%d %s = {\n' % (next_name, n))
                in_struct = True
        else:
            assert in_struct
            next_name += 1
            if t == 'string':
                b.append('char _%d[%d];\n' % (next_name, len(char_seq(n))))
            else:
                b.append('%s _%d;\n' % (c_types[t], next_name))
            if t == 'address':
                if n == '0':
                    c.append('0,\n')
                else:
                    if n not in a:
                        a[n] = 'void *'
                    c.append('&%s,\n' % n)
            elif t == 'string':
                seq = char_seq(n)
                txt = '{'
                txt += ', '.join(seq)
                txt += '}'
                c.append('%s,\n' % txt)
            else:
                c.append('%s,\n' % n)
    if in_struct:
        c.append('};\n')
        b.append('};\n')
    for name in a:
        out << 'extern %s %s;\n' % (a[name], name)
    for line in b:
        out << line
    for line in c:
        out << line

options = {
    'h' : "Print this help and exit",
    'O' : "Optimise. Levels 0 to 3",
    'H dir' : "Look for gvmt internal headers in dir.",
    'o outfile-name' : 'Specify output file name',
    't' : 'Turn on tracing (in interpreter)',
    'g' : 'Debug',
    'l' : 'Output GSO suitable for library code, no bytecode, root or heap sections allowed',
    'm memory_manager' : 'Memory manager (garbage collector) used',
    'T' : 'Use token-threading dispatch',
}       

if __name__ == '__main__':    
    opts, args = getopt.getopt(sys.argv[1:], 'ho:tlgO:H:m:T')
    if not args:
        common.print_usage(options)
        sys.exit(1)
    try:
        tracing = False
        library = False
        optimise = '-O0'
        sys_headers = None
        gc_name = 'none'
        for opt, value in opts:
            if opt == '-h':
                common.print_usage(options)
                sys.exit(0)
            elif opt == '-t':
                tracing = True
            elif opt == '-l':
                library = True
            elif opt == '-T':
                common.token_threading = True
            elif opt == '-g':
                common.global_debug = True
            elif opt == '-m':
                gc_name = value
            elif opt == '-H':
                sys_headers = value
            elif opt == '-O':
                optimise = opt + value
        src_file = gsc.read(In(args))
        src_file.make_unique()
        if gc_name != 'none':
            if optimise != '-O0':
                gc_optimiser.gc_optimise(src_file)
            gc_inliner.gc_inline(src_file, gc_name)
        base_name = os.path.basename(args[0])
        m_file = os.path.join(sys_compiler.TEMPDIR, '%s.txt' % base_name)
        to_object(src_file, base_name, 
                  library, tracing, optimise, sys_headers, gc_name)
#        out = open(m_file, 'w')
#        sep = ''
        full_base = os.path.join(sys_compiler.TEMPDIR, base_name);
        o = get_output(opts)
        if not o:
            if len(args) == 1:
                dir, f = os.path.split(args[0])
                o = open(os.path.join(dir, f.split('.')[0] + '.gso'), 'wb')
            else:
                raise GVMTException("No output file specified");
        tmp = Buffer()
        src_file.write_data(tmp)
        gso_file = gso.GSO(open('%s%s' % (full_base, sys_compiler.OBJ_EXT), 'rb').read(), tmp.get_contents())            
        gso_file.write(o)
        o.close()
    except GVMTException, ex:
#        raise
        print ex
        sys.exit(1)
        
                
        
