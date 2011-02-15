#Inlines code for object allocation 
#(so should be called allocator_inliner)

import builtin, compound, getopt, sys, common, gsc

_allocate = None
_safe = None

def _get_free(s):
    i = 0
    while i in s:
        i += 1
    return i

def _get_used_temps(ilist):
    s = set()
    for i in ilist:
        if i.__class__ in (builtin.TLoad, builtin.TStore):
            s.add(i.index)
    return s
 
def _get_used_labels(ilist):
    s = set()
    for i in ilist:
        if i.__class__ in (builtin.Branch, builtin.Hop, builtin.Target):
            s.add(i.index)
    return s
    
def get_inlinable(cmpd_inst, into):  
    '''Takes a compound instruction and a list of instructions that it is to be inlined into
    and returns a list of instructions that can be directly inlined
    '''
    assert isinstance(cmpd_inst, compound.CompoundInstruction)
    temps = _get_used_temps(into)
    labels = _get_used_labels(into)
    renumber = {}
    relabel = {}
    inlined = []
    end_label = _get_free(labels)
    labels.add(end_label)
    for bb in cmpd_inst.flow_graph:
        for i in bb:
            if i.__class__ in (builtin.Branch, builtin.Hop, builtin.Target):
                if i.index not in relabel:
                    new_label = _get_free(labels)
                    relabel[i.index] = new_label
                    labels.add(new_label)
                else:
                    new_label = relabel[i.index]
                if i.__class__ is builtin.Branch:
                    inlined.append(i.__class__(new_label, i.way))
                else:
                    inlined.append(i.__class__(new_label))
            elif i.__class__ in (builtin.TLoad, builtin.TStore):
                if i.index not in renumber:
                    new_temp = _get_free(temps)
                    renumber[i.index] = new_temp
                    temps.add(new_temp)
                else:
                    new_temp = renumber[i.index]
                inlined.append(i.__class__(i.tipe, new_temp))
            elif i.__class__ in (builtin.Name, builtin.TypeName):
                if i.index not in renumber:
                    new_temp = _get_free(temps)
                    renumber[i.index] = new_temp
                    temps.add(new_temp)
                else:
                    new_temp = renumber[i.index]
                inlined.append(i.__class__(new_temp, i.tname))
            elif isinstance(i, builtin.Return):
                inlined.append(builtin.Hop(end_label))
            else:
                inlined.append(i)
    inlined.append(builtin.Target(end_label))
    inlined = compound._lcc_post(inlined)
    return inlined
    
# Decide what to inline...
# Inline when private and 1 use - Automatically.
# Otherwise if small enough, (larger threshold for private stuff)

def _effective_size(inst):
    size = 0
    assert isinstance(inst, compound.CompoundInstruction)
    for i in inst.instructions:
        if isinstance(i, compound.CompoundInstruction):
            for bb in i.flow_graph:
                size += _effective_size(bb.instructions)
        elif i.__class__ in (builtin.PushCurrentState, builtin.PopState, builtin.Raise):
            size += 8
        elif i.__class__ in (builtin.GC_Safe, builtin. GC_Safe_Call):
            size += 4
        elif isinstance(i, builtin.FarJump):
            size += 100
        elif isinstance(i, builtin.N_Call):
            size += 10
        elif i.__class__ in (builtin.Call, builtin.V_Call, builtin.N_Call_No_GC):
            size += 6
        elif i.__class__ in (builtin.TLoad, builtin.TStore, builtin.Drop):
            pass
        elif i.__class__ in (builtin.File, builtin.Line, builtin.Name, builtin.TypeName):
            pass
        else:
            size += 1
    return size
            
def _is_inlinable(inst):
    assert isinstance(inst, compound.CompoundInstruction)
    if 'private' in inst.qualifiers:
        limit = 32
    else:
        limit = 16
    return _effective_size(inst) < limit
    
def do_inling(inst, all_codes):
    all_codes = dict(all_codes)
    if not isinstance(inst, compound.CompoundInstruction):
        return
    ilist = inst.instructions
    #Scan instruction looking for address, call pairs.
    done = False
    while not done:
        done = True
        for i in range(2, len(ilist)):
            if (isinstance(ilist[i-2], builtin.Address) and
                isinstance(ilist[i-1], builtin.Immediate) and
                isinstance(ilist[i], builtin.Call)):
                name = ilist[i-2].symbol
                if name in all_codes and _is_inlinable(all_codes[name]):
                    done = False
                    ilist = ilist[:i-2] + get_inlinable(all_codes[name], ilist) + ilist[i+1:]
                    # Prevent recursive inlining
                    del all_codes[name]
    inst.instructions = ilist
    
if __name__ == '__main__':
    opts, args = getopt.getopt(sys.argv[1:], 't:')
    infile = common.In(args[0])
    gsc_file = gsc.read(infile)
    infile.close()
    all_codes = {}
    sect = gsc_file.code
    for i in sect.instructions:
        all_codes[i.name] = i
    for i in sect.instructions:
        do_inling(i, all_codes)
    buf = common.Buffer()
    gsc_file.write(buf)
    print '%s' % buf
