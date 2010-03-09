
import common, gsc, builtin, gtypes, graph, compound

__GC_ALLOCATE = builtin.GC_Allocate_Only()
__RSTORE_R = builtin.RStoreSimple(gtypes.r)


def optimise_allocate(flow_graph):
    consts = ssa_constants(flow_graph)
    equivalents = find_equivalents(flow_graph, consts)
#    print "Equivalents", equivalents
    remove_write_barriers(flow_graph, equivalents)
    remove_redundant_initialisation(flow_graph, equivalents)

def child_instruction(node, bb, index):
    obj_node = node[1][index]
    if obj_node is None:
        return None
    return bb[obj_node[0]]
    
def _add_ident(equiv, a):
    if a not in equiv:
        equiv[a] = set()
        equiv[a].add(a)
    
def _equivalent(a, b, equiv):
    _add_ident(equiv, a)
    _add_ident(equiv, b)
    equiv[a].add(b)
    equiv[b].add(a)
    
#Find equivalent temps.
def find_equivalents(flow_graph, consts):
    equivalents = {}
    for bb in flow_graph:
        forest = block_to_forest(bb)
        for node in forest:
            index, children = node
            inst = bb[index]
            if inst.__class__ is builtin.TStore and inst.index in consts:
                _add_ident(equivalents, inst.index)
                val_node = children[0]
                if val_node is not None:
                    val_index = val_node[0]
                    val_inst = bb[val_index]
                    if (val_inst.__class__ is builtin.TLoad and 
                        val_inst.index in consts and
                        inst.tipe == val_inst.tipe):
                        _equivalent(val_inst.index, inst.index, equivalents)
    return equivalents
    
def remove_fixed_initialisation(bb, equivalents):
    forest = block_to_forest(bb)
    mallocs = {}
    for node in forest:
        index, children = node
        inst = bb[index]
        if mallocs and _any_may_gc(node, bb):
            return
        if inst.__class__ is builtin.TStore and inst.index in equivalents:
            val_node = children[0]
            if val_node is not None:
                val_index = val_node[0]
                val_inst = bb[val_index]
                if val_inst.__class__ is builtin.GC_Malloc: 
                    size_inst = child_instruction(val_node, bb, 0)
                    if size_inst and size_inst.__class__ is builtin.Number:
                        words = (size_inst.value + gtypes.p.size - 1) // gtypes.p.size
                        vector = (1 << words) - 1
                        mallocs[inst.index] = [vector, val_index, index]
        elif inst.__class__ is builtin.RStore or inst.__class__ is builtin.RStoreSimple:
            obj_inst = child_instruction(node, bb, 1)
            if obj_inst is not None:
                if obj_inst.__class__ is builtin.TLoad and obj_inst.index in equivalents:
                    idx_inst = child_instruction(node, bb, 2)
                    if idx_inst.__class__ is builtin.Number:
                        offset = idx_inst.value  // gtypes.p.size
                        for eq in equivalents[obj_inst.index]:
                            if eq in mallocs:
                                m = mallocs[eq]
                                vector = m[0]
                                if inst.tipe.size > gtypes.p.size:
                                    vector &= (~( 3 << offset))
                                else:
                                    vector &= (~( 1 << offset))
                                m[0] = vector
    for t in mallocs:       
        vector, malloc, store = mallocs[t]
        index = 0
        while (1 << index) <= vector:
            if (1 << index) & vector:                
                bb.insert(store+1, __RSTORE_R)
                bb.insert(store+1, builtin.Number(str(index*gtypes.p.size)))
                bb.insert(store+1, builtin.TLoad(gtypes.r, t))
                bb.insert(store+1, builtin.Number('0'))
            index += 1
        bb[malloc] = __GC_ALLOCATE

def remove_redundant_initialisation(graph, equivalents):
    pairs = _find_malloc_init_pairs(graph, equivalents)
    for malloc, init in pairs:
        m_block, m_index = malloc
        i_block, i_index = init
        if _escapes_between(m_block, i_block):
            continue
        ok = True
        if m_block == i_block:
            for i in range(m_index +1, i_index):
                if m_block[i].may_gc():
                    ok = False
        else:
            for i in range(m_index +1, len(m_block)):
                if m_block[i].may_gc():
                    ok = False
            for i in range(0, i_index):
                if i_block[i].may_gc():
                    ok = False
        if ok:
            m_block[m_index] = __GC_ALLOCATE
    for bb in graph:
        remove_fixed_initialisation(bb, equivalents)
   
def _any_may_gc(node, bb):
    if node is None:
       return False 
    inst, children = node
    if bb[inst].may_gc():
        return True
    for c in children:
        if _any_may_gc(c, bb):
            return True
    return False
    
def _escapes_between(a, b):
    if a == b:
        return False
    done = set()
    done.add(b)
    to_do = set()
    for s in a.successors:
        to_do.add(s)
    to_do.discard(b)
    while to_do:
        n = to_do.pop()
        if n.may_gc() or not n.successors:
            return True
        done.add(n)
        to_do |= n.successors
        to_do -= done
    return False
    
def _find_malloc_init_pairs(graph, equivalents):
    malloced = {}
    inited = {}
    for bb in graph:
        forest = block_to_forest(bb)
        for node in forest:
            index, children = node
            inst = bb[index]
            if inst.__class__ is builtin.FullyInitialized:
                obj_inst = child_instruction(node, bb, 0)
                if obj_inst is not None:
                    if obj_inst.__class__ is builtin.TLoad and obj_inst.index in equivalents:
                        inited[obj_inst.index] = (bb, index)
            elif inst.__class__ is builtin.TStore and inst.index in equivalents:
                val_node = children[0]
                if val_node is not None:
                    val_index = val_node[0]
                    val_inst = bb[val_index]
                    if val_inst.__class__ is builtin.GC_Malloc:
                        malloced[inst.index] = (bb, val_index)
    pairs = []
    for index in inited:
        for eq in equivalents[index]:
            if eq in malloced:
                pairs.append((malloced[eq], inited[index]))
                break
    return pairs
                        
def _get_barriers(graph, equivalents):
    U = set(equivalents.keys())
    start_barriers = {}
    end_barriers = {}
    local_barriers = {}
    for bb in graph:
        end_barriers[bb] = U
        local_barriers[bb] = _local_barriers(bb, equivalents)
        start_barriers[bb] = set()
    start_barriers[graph[0]] = set()
    end_barriers[graph[0]] = local_barriers[graph[0]]
    work = set()
    for bb in graph[0].successors:
        work.add(bb)
    while work:
        n = work.pop()
        in_ = set(U)
        for p in n.predecessors:
            in_ &= end_barriers[p]
        start_barriers[n] = in_
        if n.may_gc():
            out = local_barriers[n]
        else:
            out = in_ | local_barriers[n]
        if out != end_barriers[n]:
            end_barriers[n] = out
            for s in n.successors:
                work.add(s)
    return start_barriers, local_barriers, end_barriers

#Work list algorithm, propoagates which barriers are safe to remove at start of blocks.    
def remove_write_barriers(graph, equivalents):
    start_barriers, local_barriers, end_barriers = _get_barriers(graph, equivalents)
    for bb in graph:
        _remove_barriers(bb, start_barriers[bb], equivalents)
    
def _local_barriers(bb, equivalents):
    barriers = set()
    forest = block_to_forest(bb)
    for node in forest:
        index, children = node
        inst = bb[index]
        if inst.__class__ is builtin.RStore and inst.tipe is gtypes.r:
            obj_inst = child_instruction(node, bb, 1)
            if obj_inst is not None:
                if obj_inst.__class__ is builtin.TLoad and obj_inst.index in equivalents:
                    assert obj_inst.tipe is gtypes.r
                    if obj_inst.index not in barriers:
                        barriers |= equivalents[obj_inst.index]
        if _any_may_gc(node, bb):
            barriers = set()
        if inst.__class__ is builtin.TStore and inst.index in equivalents:
            val_inst = child_instruction(node, bb, 0)
            if val_inst is not None:
                if val_inst.__class__ is builtin.GC_Malloc:
                    barriers |= equivalents[inst.index]
    return barriers
    
def _remove_barriers(bb, barriers, equivalents):
    'Remove surplus write-barriers for this block'
    forest = block_to_forest(bb)
    for node in forest:
        index, children = node
        inst = bb[index]
        if inst.__class__ is builtin.RStore and inst.tipe is gtypes.r:
            obj_inst = child_instruction(node, bb, 1)
            if obj_inst is not None:
                if obj_inst.__class__ is builtin.TLoad and obj_inst.index in equivalents:
                    assert obj_inst.tipe is gtypes.r
                    if obj_inst.index in barriers:
                        bb[index] = __RSTORE_R
                    else:
                        barriers |= equivalents[obj_inst.index]
                val_inst = child_instruction(node, bb, 0)      
                if val_inst is not None:
                    if val_inst.__class__ is builtin.Number and val_inst.value == 0:
                        bb[index] = __RSTORE_R
        if _any_may_gc(node, bb):
            barriers = set()
        if inst.__class__ is builtin.TStore and inst.index in equivalents:
            val_inst = child_instruction(node, bb, 0)
            if val_inst is not None:
                if val_inst.__class__ is builtin.GC_Malloc:
                    barriers = equivalents[inst.index]

def pop(stack):
    if stack:
        return stack.pop()
    else:
        return None
        
def print_node(node, bb):
    if node is None:
        print '? ',
    else:
        index, children = node
        print '(%s ' % bb[index].name,
        for c in children:
            print_node(c, bb)
        print ') ',
        
_COMPEX_INSTRUCTIONS = (builtin.NativeArg, builtin.Call,
    builtin.N_Call, builtin.V_Call, builtin.N_Call_No_GC, 
    builtin.Alloca, builtin.Pick, builtin.Stack, builtin.Flush,
    builtin.Insert, builtin.GC_Safe, builtin.GC_Safe_Call, 
    builtin.Drop, builtin.DropN, compound.CompoundInstruction)
        
def block_to_forest(bb):
    forest = []
    stack = []
    for index, inst in enumerate(bb):
        if inst.__class__ in _COMPEX_INSTRUCTIONS:
            if inst.may_gc():
                node = [index, [inst]]
                forest.append(node)
            stack = []
        else:
            children = [ pop(stack) for x in inst.inputs ]
            children.reverse()
            node = [index, children]
            if inst.outputs:
                stack.append(node)
            else:
                forest.append(node)
    return forest
            
def ssa_constants(graph):
    consts = set()
    variables = set()
    for bb in graph:
        for i in bb:
            if i.__class__ is builtin.TStore:
                if i.index not in variables:
                    if i.index in consts:
                        consts.remove(i.index)
                        variables.add(i.index)
                    else:
                        consts.add(i.index)
    return consts
    
def gc_optimise_section(gsc_section):
    for inst in gsc_section.instructions:
        optimise_allocate(inst.flow_graph)              
    
def gc_optimise(gsc_file):
    if common.global_debug:
        return
    gc_optimise_section(gsc_file.code)
    if gsc_file.bytecodes:
        gc_optimise_section(gsc_file.bytecodes)
    
if __name__ == '__main__':
    from flow_graph import get_graph_from_argv
    graph = get_graph_from_argv()
    consts = ssa_constants(graph)
    equivalents = find_equivalents(graph, consts)
    start_barriers, local_barriers, end_barriers = _get_barriers(graph, equivalents)
    for bb in graph:
        print bb, 
        if bb.may_gc():
            print "(may_gc)"
        else:
            print
        print "Start barriers: ", start_barriers[bb]
        print "Local barriers: ", local_barriers[bb]
        print "End barriers: ", end_barriers[bb]

