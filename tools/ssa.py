import builtin

# Algorithm from "Modern compiler implementation" [Appel]
# BEWARE - Check errata before comparing to book.
def phi_nodes(graph):
    '''variables: collection of all variables.
    graph: The flow graph 
    df: Dominance frontiers. Mapping node -> dominance frontier for that node
    defns: Definitions:
    Mapping node -> collection of variables defined in that node 
    '''
    variables = graph.temps()
    df = graph.dominance_frontiers()
    defns = definitions(graph)
    phis = {}
    defsites = {}
    for a in variables:
        defsites[a] = set()
    for y in graph:
        phis[y] = set()
    for node in graph:
        for a in defns[node]:
            defsites[a].add(node)
    for a in variables:
        work_set = defsites[a].copy()
        while work_set:
            n = work_set.pop()
            for y in df[n]:
                if a not in phis[y]:
                    #Insert phi node for a into y
                    phis[y].add(a)
                    if a not in defns[y]:
                        work_set.add(y)
    return phis
       
def definitions(graph):
    'Return a dictionary mapping nodes -> set of variables defined in that node'
    # Defined if defined and not stored in memory at the end of the block.
    defns = {}
    refset = set(graph.gc_temps())
    graph.weight()
    for bb in graph: 
        bb.ref_preferred_location(refset)
    graph.annotate_edges_with_ref_preferences(refset)
    for bb in graph: 
        # Defined variables are those defined conventionally...
        defined = set(bb.defns)
        # plus those which are live out and saved to memory across a call.
        if bb.child:
            for i in bb.child.ref_preferences:
                if bb.child.ref_preferences[i] > 0:
                    defined.discard(i)
                elif bb.may_gc():
                    defined.add(i)
        defns[bb] = defined
    return defns

