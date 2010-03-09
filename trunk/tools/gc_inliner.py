#Inlines code for object allocation 
#(so should be called allocator_inliner)

import sys, common, gsc, os.path, getopt, builtin, gtypes

_allocate = None
_safe = None
_read_barrier = None
_write_barrier = None
_alloc_only = None

def get_inline_inst(gc_name):
    global _allocate, _safe, _read_barrier, _write_barrier, _alloc_only
    dirname = os.path.dirname(sys.argv[0])
    infile = common.In(os.path.join(dirname, gc_name + '.gsc'))
    gsc_file = gsc.read(infile)
    infile.close()
    for i in gsc_file.code.instructions:
        if i.name == 'GC_MALLOC_INLINE':
            _allocate = i
        elif i.name == 'GC_ALLOC_ONLY_INLINE':
            _alloc_only = i
        elif i.name == 'GC_SAFE_INLINE':
            _safe = i
        elif i.name == 'GC_READ_BARRIER':
            _read_barrier = i
        elif i.name == 'GC_WRITE_BARRIER':
            _write_barrier = i
        else:
            print "Unexpected instruction '%s' in gc-inline file" % i.name
            sys.exit(1)

def gc_inline_section(gsc_section):
    inserted_allocate = False
    inserted_alloc_only = False
    inserted_safe = False
    inserted_read_barrier = False
    inserted_write_barrier = False
    for inst in gsc_section.instructions:
        for bb in inst.flow_graph:               
            for index, i in enumerate(bb.instructions):
                if i.__class__ is builtin.GC_Malloc and _allocate:
                    bb[index] = _allocate
                    inserted_allocate = True
                elif i.__class__ is builtin.GC_Allocate_Only and _alloc_only:
                    bb[index] = _alloc_only
                    inserted_alloc_only = True
                elif i.__class__ is builtin.GC_Safe and _safe:
                    bb[index] = _safe
                    inserted_safe = True
                elif i.__class__ is builtin.RLoad and i.tipe == gtypes.r and _read_barrier:
                    bb[index] = _read_barrier
                    inserted_read_barrier = True
                elif i.__class__ is builtin.RStore and i.tipe == gtypes.r and _write_barrier:
                    bb[index] = _write_barrier
                    inserted_write_barrier = True
    if inserted_alloc_only:
        gsc_section.instructions.insert(0, _alloc_only)
    if inserted_allocate:
        gsc_section.instructions.insert(0, _allocate)
    if inserted_safe:
        gsc_section.instructions.insert(0, _safe)    
    if inserted_read_barrier:
        gsc_section.instructions.insert(0, _read_barrier)
    if inserted_write_barrier:
        gsc_section.instructions.insert(0, _write_barrier)    
    
def gc_inline(gsc_file, collector_name):
    get_inline_inst(collector_name)  
    gc_inline_section(gsc_file.code)
    if gsc_file.bytecodes:
        gc_inline_section(gsc_file.bytecodes)
     
if __name__ == '__main__':
    opts, args = getopt.getopt(sys.argv[1:], 'c:')
    collector_name = None
    for opt, value in opts:
        if opt == '-c':
            collector_name = value
    if len(args) != 1 or collector_name is None:
        print "Usage: %s -c gc_name file_name"
        sys.exit(1)
    infile = common.In(args[0])
    gsc_file = gsc.read(infile)
    infile.close()
    gc_inline(gsc_file, collector_name)   
    outfile = common.Out(args[0])
    gsc_file.write(outfile)
    outfile.close()
    
    
