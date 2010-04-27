#!/usr/bin/python
# linker
# Links gso files to form executable.

import gso, gsc, getopt, sys, sys_compiler, os, common, heap
    
def parse_string(s):
    clist = []
    i = 1
    while i < len(s)-1:
        if s[i] == '\\':
            if s[i+1] in '01234567':            
                clist.append(int(s[i+1:i+4], 8))
                i += 4
            elif s[i+1] == 'n':
                clist.append(ord('\n'))
                i += 2
            elif s[i+1] == 't':
                clist.append(ord('\t'))
                i += 2
            else:
                clist.append(ord(s[i+1]))
                i += 2
        else:
            clist.append(ord(s[i]))
            i += 1
    return clist
    
def align(offset, size, out):
    if offset % size:
        out << '.align %d\n' % size
        return offset + size - offset % size
    else:    
        return offset

_size = {
    'address' : 4,
    'uint64' :  8,
    'int64' : 8,
    'int32' : 4,
    'uint32' :  4,
    'int16' : 2,
    'uint16' :  2,
    'int8'   : 1,
    'uint8' :  1,
    'float32' : 4,
    'float64' : 8,
}

_asm_types = {
    'address' : '.long',
    'int32' : '.long',
    'uint32' :  '.long',
    'int16' : '.short',
    'uint16' :  '.short',
    'int8'   : '.byte',
    'uint8' :  '.byte',
    'float32' : '.float',
    'float64' : '.double',
}
    
# This is GCC x86 specific:
# To do - Generalise this - using templates and GC info
def to_asm(section, base_name, title_symbol):
    out_file = os.path.join(sys_compiler.TEMPDIR, base_name + 
                            sys_compiler.ASM_EXT)
    out = common.Out(out_file)
    public = {}
    starts = set()
    out << '.data\n'
    align(1, 32, out)
    offset = 0
    if title_symbol:
        out << '.globl gvmt_start_%s\n' % section.name
        out << 'gvmt_start_%s:\n'% section.name
    object_id = 0
    for t, n in section.items:
        if t[0] == '.':
            if t == '.object':
                offset = align(offset, 4, out)
                out << '.globl %s\n' % n
                out << '.type %s, @object\n' % n
                out << '%s:\n' % n
                name = n
                starts.add(offset)
            elif t == '.end':
                out << '.size %s, .-%s\n' % (name, name)
            elif t == '.public':
                pass
            elif t == '.label':
                out << '.globl %s\n' % n
                offset = align(offset, 8, out)
                out << '%s:\n' % n
        else:
            if t == 'string':
                seq = parse_string(n)
                for c in seq:
                    out << '.byte %d\n' % c
                offset += len(seq)
            else:
                s = _size[t]
                offset = align(offset, s, out) 
                if t == 'uint64' or t == 'int64':
                    out << '    .long %s\n'% (int(n) & 0xffffffff)
                    out << '    .long %s\n'% ((int(n)>>32) & 0xffffffff)
                else:
                    out << '%s %s\n'% (_asm_types[t], n)
                offset += s
    if title_symbol:
        out << '.globl gvmt_end_%s\n' % section.name
        out << 'gvmt_end_%s:\n' % section.name
    out.close()

options = {
    'h' : "Print this help and exit",
    'o outfile-name' : 'Specify output file name',
    'v' : 'verbose',
    'l' : 'Output a library (.dll or .so)',
    'n' : 'No-heap, error if heap or roots section exist'
}

if __name__ == '__main__':    
    opts, args = getopt.getopt(sys.argv[1:], 'o:vln2')
    if not args:
        common.print_usage(options)
        sys.exit(1)
    gsos = []
    outfile = 'gvmt.out'
    verbose = False
    library = False
    no_heap = False
    zones = False
    for opt, value in opts:
        if opt == '-h':
            common.print_usage(options)
            sys.exit(0)
        elif opt == '-o':
            outfile = value
        elif opt == '-v':
            verbose = True
        elif opt == '-l':
            library = True
        elif opt == '-n':
            no_heap = True
        elif opt == '-2':
            zones = True
    for name in args:
        f = open(name, 'rb')
        g = gso.read(f)
        gsos.append(g)
        f.close()
        c = open('%s%s' % (name, sys_compiler.OBJ_EXT), 'wb')
        c.write(g.code)
        c.close()
    obj_list = [name for name in args]
    base_name = os.path.join(sys_compiler.TEMPDIR, 'gvmt_memory')
    tmp = common.Buffer()
    for g in gsos:
        tmp << g.memory
    merged = gsc.read(common.In(tmp))
    if no_heap:
        if not merged.roots.empty():
            print "Roots section present"
            sys.exit(6)
        elif not merged.heap.empty():
            print "Heap section present"
            sys.exit(7)
        sections = (merged.opaque, )
    else:
        sections = (merged.opaque, merged.roots, merged.heap)
    for sect in sections:
        name = '%s_%s' % (base_name, sect.name)
        if sect is merged.heap and zones:
            heap.to_asm(sect, name)
        else:
            to_asm(sect, name, not no_heap)
        code = sys_compiler.asm_to_object(name, False)
        if code:
            sys.exit(code)
        obj_list.append(name)
    code = sys_compiler.link(obj_list, outfile, library, verbose)
    if code:
        sys.exit(code)

