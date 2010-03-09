
import builtin
from delta import DeltaMode, delta_merge, delta_concat
import compound, gtypes
import common
import sys
import graph
import ssa

def unitialised_temp(graph, index):
    filename = None
    line = None
    for bb in graph:
        for i in bb:
            if i.__class__ == builtin.File:
                filename = i.filename
            if i.__class__ == builtin.Line:
                line = i.line
            if i.__class__ is builtin.Name and i.index == index:
                if filename and line:
                    raise common.GVMTException(
                               "%s:%s:Uninitialised variable '%s' declared in this scope" % 
                               (filename, line, i.tname))
                else:
                    raise common.UnlocatedException("Uninitialised temp '%s'" % i.tname)
    raise common.UnlocatedException("Uninitialised temp %d" % index)

class FlowGraph(object):
    '''FlowGraph is a list of nodes, a set of edges and stack and stream
deltas attached. Each node contains successor and predecessor information.'''

    def __init__(self, ilist):
        file_name = ''
        line_number = 0
        try:
            current = Block()
            current.name = 'Start'
            nlist = [ current ]
            nodes = {}
            branches = set()
            targets = set()
            for i in ilist:
                if i.__class__ == builtin.Target:
                    new_node = self._for_target(i.index, nodes)
                    if current is not None:
                        current.add_edge(new_node)
                    current = new_node
                    nlist.append(current)
                    current.instructions.append(i)
                    targets.add(i.index)
                elif i.__class__ == builtin.Branch:
                    successor = Block()
                    if current is not None:
                        current.instructions.append(i)
                        target = self._for_target(i.index, nodes)
                        current.add_edge(target)
                        current.add_edge(successor)
                    current = successor
                    nlist.append(current)
                    branches.add(i.index)
                elif i.__class__ == builtin.Hop:
                    if current is not None:
                        current.instructions.append(i)
                        target = self._for_target(i.index, nodes)
                        current.add_edge(target)
                        current = None
                    branches.add(i.index)
                elif (i.__class__ == builtin.Return or 
                      i.__class__ == builtin.Jump or
                      i.__class__ == builtin.FarJump):
                    if current is not None:
                        current.instructions.append(i)
                        current = None
                elif i.__class__ == compound.CompoundInstruction:
                    if current is not None:
                        current.instructions.append(i)
                    if i.terminates_block():
                        current = None
                else:
                    if current is not None:
                        current.instructions.append(i)
            if current is not None:
                current.exit = True
            if branches != targets:
                no_targets =  branches - targets
                if no_targets:
                    raise common.UnlocatedException('BRANCH or HOP to %d has no target' % no_targets.pop())
            nlist = remove_dead_nodes(nlist)
            self.nodes = remove_redundant_nodes(nlist)
            for i in range(len(self.nodes)):
                self.nodes[i].index = i
            self._build_edge_sets()
            self.calculate_deltas()
            if self.nodes[-1].parent:
                start = self.nodes[-1].parent.deltas
            else:
                start = 0, 0, 0
            self.deltas = delta_concat(start, self.nodes[-1]._deltas)
            refs = set()
            for bb in self:
                for i in bb.instructions:
                    if i.__class__ == builtin.TStore or i.__class__ == builtin.TLoad:
                        if i.tipe == gtypes.r:
                            refs.add(i.index)
            self.ref_set = frozenset(refs)
            self._liveness()
            self._dominators = None
            self._idoms = None
            self._dominator_tree = None
            self._dom_fronts = None
            self._loop_nest_tree = None
            self._dominator_tree = None
            self._may_gc = False
            for bb in self:            
                for i in bb:
                    if i.may_gc():
                        self._may_gc = True
                        return
        except common.UnlocatedException as ex:
            if file_name and line_number:
                raise ex.locate(file_name, line_number)
            else:
                raise ex

    def calculate_deltas(self):
        'All deltas are triples: stream, consume, produce.'  
        for bb in self.nodes:
            bb.deltas
        if not self.edge_sets:
            return
        #Set all edges to some value:
        for es in self.edge_sets:
            es.deltas = None
        self.nodes[0].child.deltas = self.nodes[0]._deltas
        done = False
        while not done:
            done = True
            i = 0
            for bb in self.nodes:
                i += 1
                if bb.child and not bb.child.deltas:
                    if bb.parent.deltas:
                        bb.child.deltas = delta_concat(bb.parent.deltas, bb._deltas)
                    else:
                        done = False
        for bb in self.nodes:
            if bb.parent and bb.child:
                d = delta_concat(bb.parent.deltas, bb._deltas)
                bb.child.deltas = delta_merge(bb.child.deltas, d)
        
    def _for_target(self, index, nodes):
        if index in nodes:
            return nodes[index]
        else:
            n = Block()
            nodes[index] = n
            return n
            
    def __getitem__(self, index):
        bb = self.nodes[index]
        assert bb.__class__ is Block
        return bb
    
    def _build_edge_sets(self):
        index = 0
        # parents and children are mappings of blocks -> edge-sets
        edgesets = []
        parents = {}
        for p in self.nodes:
            for s in p.successors:
                add_edge_to_sets(edgesets, p, s)
        for e in edgesets:
            e.index = index
            index += 1
            for p in e.parents:
                p.child = e
            for c in e.children:
                c.parent = e
        # Check all is correct
        for p in self.nodes:
            for s in p.successors:
                check_edge(edgesets, p, s)
        self.edge_sets = edgesets
        
    def _liveness(self):
        #Iterative algorithm.
        for bb in self.nodes:
            bb.live_in = set(bb.uses)
            bb.live_out = set()
        not_done = True
        while not_done:
            not_done = False
            for bb in self.nodes:
                new_in = bb.live_out - bb.defns
                if not new_in.issubset(bb.live_in):
                    bb.live_in |= new_in
                    not_done = True
                for p in bb.predecessors:
                    if not bb.live_in.issubset(p.live_out):
                        p.live_out |= bb.live_in
                        not_done = True
        for bb in self.nodes:
            bb.ref_live_in = bb.live_in.intersection(self.ref_set)
            bb.ref_live_out = bb.live_out.intersection(self.ref_set)
        if self.nodes[0].live_in:
            for var in self.nodes[0].live_in:
                unitialised_temp(self, var)
        
        
    def may_gc(self):
        return self._may_gc
        
    def ends_block(self):
        return not self.nodes[-1].exit
        
    def temps(self):
        'Returns a set of all the temps defined/used in this graph'
        t = set()
        for bb in self:
            for i in bb.instructions:
                if i.__class__ == builtin.TStore or i.__class__ == builtin.TLoad:
                    t.add(i.index)
        return t
                    
    def gc_temps(self):
        '''Returns a  set containing all the temps
        (the indices of) that are references and live across a GC point.
        That is any point where GC can occur.'''
        in_mem = set()
        refs = set()
        for bb in self:
            for i in bb.instructions:
                if i.__class__ == builtin.TStore or i.__class__ == builtin.TLoad:
                    if i.tipe == gtypes.r:
                        refs.add(i.index)
        for bb in self:
            live = bb.live_out & refs
            for i in bb.instructions[::-1]:
                if i.may_gc():
                    if not live.issubset(in_mem):
                       in_mem |= live
                elif i.__class__ == builtin.TStore:
                    live.discard(i.index)
                elif i.__class__ == builtin.TLoad:
                    if i.tipe == gtypes.r:
                        live.add(i.index)
        return in_mem
           
    def annotate_edges_with_ref_preferences(self, refset):
        prefs = {}
        for r in refset:
            prefs[r] = 0.0
        for es in self.edge_sets:
            for n in es.parents:
                p = n.ref_preferred_end
                w = n.weight
                for r in refset:
                    prefs[r] += p[r] * w
            for n in es.children:
                p = n.ref_preferred_start
                w = n.weight
                for r in refset:
                    prefs[r] += p[r] * w
            for r in refset:
                if prefs[r] > 0:
                    prefs[r] = 1
                else:
                    prefs[r] = -1
            es.ref_preferences = prefs
              

    def dominators(self):
        if self._dominators is None:
            self._dominators = graph.find_dominators(self)
        return self._dominators
        
    def immediate_dominators(self):
        if self._idoms is None:
            self._idoms = graph.immediate_dominators(self.dominators())
        return self._idoms
        
    def _dom_tree(self):
        if self._dominator_tree is None:
            t = graph.dominator_tree(self.immediate_dominators())
            self._dominator_tree = t
        return self._dominator_tree
        
    def dominance_frontiers(self):
        if self._dom_fronts is None:
            idoms = self.immediate_dominators()
            tree = self._dom_tree()
            doms = self.dominators()
            self._dom_fronts = graph.dominance_frontiers(idoms, tree, doms)
        return self._dom_fronts
        
    def loop_nest_tree(self):
        if self._loop_nest_tree is None:
            tree = self._dom_tree()
            doms = self.dominators()
            self._loop_nest_tree = graph.loop_nest_tree(doms, tree)
        return self._loop_nest_tree 
        
    def weight(self):
        graph.weight(self, self.dominators(), self.loop_nest_tree())

def check_edge(sets, pre, succ):
    p = None
    c = None
    for es in sets:
        if pre in es.parents:
            assert p is None
            p = es
        if succ in es.children:
            assert c is None
            c = es
    if p is None:
        print "%r (%r -> %r) is not in parents of any edge set" % (pre, pre, succ)    
        print sets
        sys.exit(1)
    if c is None:
        print "%r (%r -> %r) is not in children of any edge set" % (succ, pre, succ)  
        print sets
        sys.exit(1)
    if p is not c:
        print "%r -> %r spans two edge sets" % (pre, succ)
        print sets
        sys.exit(1)
        
def add_edge_to_sets(sets, pre, succ):
    p = None
    c = None
    for es in sets:
        if pre in es.parents:
            assert p is None
            p = es
        if succ in es.children:
            assert c is None
            c = es
    if p is None:
        if c is None:
            sets.append(EdgeSet(set([pre]), set([succ])))
        else:
            c.parents.add(pre)
    else:
        if p is c: 
            pass
        elif c is None:
            p.children.add(succ)
        else:
            p.merge(c)
            del sets[sets.index(c)]
 
class EdgeSet:
    
    def __init__(self, parents, children):
        self.parents =  parents 
        self.children = children       
        
    def merge(self, other):
        self.parents = self.parents.union(other.parents)
        self.children = self.children.union(other.children)
        
    def __repr__(self):
        return 'EdgeSet(%r, %r, %r)' % (self.parents, self.children, self.index)
                
def merge_nodes(n1, n2):
    n1.successors = n2.successors
    n2.predecessors = set()
    n2.successors = set()
    #Append instructions removing any internal Hop-Target pair.
    if n1.instructions and isinstance(n1.instructions[-1], builtin.Hop):
        assert isinstance(n2.instructions[0], builtin.Target)
        assert n1.instructions[-1].index == n2.instructions[0].index
        n1.instructions = n1.instructions[:-1] + n2.instructions[1:]
    else:
        n1.instructions += n2.instructions
    n2.instructions = []
    for n in n1.successors:
        n.predecessors.remove(n2)
        n.predecessors.add(n1)
    
# Merge nodes that directly follow one another and
# have no other predecessors/successors
def try_to_merge(nlist, i):
    node = nlist[i]
    if len(node.successors) != 1:
        return
    next = list(node.successors)[0]
    i += 1
    if next.exit or next != nlist[i]:
        return
    while len(next.predecessors) == 1:
        merge_nodes(node, next)
        if len(node.successors) != 1:
            return
        next = list(node.successors)[0]
        i += 1
        if next.exit or i >= len(nlist) or next != nlist[i]:
            return
    
def remove_redundant_nodes(nlist):
    for i in range(len(nlist)-1):
        try_to_merge(nlist, i)
    live = [nlist[0]]
    for n in nlist[1:]:
        if n.successors or n.predecessors:
            live.append(n)
    return live
    
def remove_dead_nodes(nlist):
    for n in nlist:
        n.live = False
    nlist[0].live = True
    work_set = set(nlist[:1])
    while work_set:
        n = work_set.pop()
        assert n.live
        for s in n.successors:
            if not s.live:
                s.live = True 
                work_set.add(s)
    nodes =  [n for n in nlist if n.live] 
    node_set = set(nodes)
    for n in nodes:
        n.predecessors = n.predecessors.intersection(node_set)
    return nodes
       
_node_id = 0
  
class Block:
    'A Block contains a list of instructions, a set of predeccessor nodes and a set of successor nodes'
    def __init__ (self):
        global _node_id
        self.instructions = []
        _node_id +=1 
        self.name = 'Block_%d' % _node_id
        self.successors = set()
        self.predecessors = set()
        self.parent = None
        self.child = None
        self._deltas = None
        self.index = 0
        self.exit = False
        self._defs = None
        self._uses = None
        self._may_gc = None
        
    def _modified(self):
        self._deltas = None
        self._defs = None
        self._uses = None
        self._may_gc = None
        
    def __setitem__(self, index, inst):
        assert isinstance(inst, builtin.Instruction)
        self.instructions[index] = inst
        self._modified()
        
    def insert(self, index, inst):
        assert isinstance(inst, builtin.Instruction)
        self.instructions.insert(index, inst)
        self._modified()
        
    def pop(self, index):
        self.instructions.pop(index)
        self._modified()
        
    def __getitem__(self, index):
        return self.instructions[index]
        
    def __len__(self):
        return len(self.instructions)
        
    def _get_deltas(self):
        if self._deltas is None:
            mode = DeltaMode()
            for i in self.instructions:
                i.process(mode)
            self._deltas = mode.deltas()
        return self._deltas
        
    def may_gc(self):
        if self._may_gc is None:
            self._may_gc = False
            for i in self.instructions:
                if i.may_gc():
                    self._may_gc = True
        return self._may_gc
        
    def _get_uses(self):
        if self._uses is None:
            self._liveness()
        return self._uses
        
    def _get_defs(self):
        if self._defs is None:
            self._liveness()
        return self._defs
        
    def _liveness(self):
        used = set()
        defined = set()
        for i in self.instructions:
            if i.__class__ is builtin.TStore:
                defined.add(i.index)
            elif i.__class__ is builtin.TLoad and i.index not in defined:
                used.add(i.index)
        self._defs, self._uses = defined, used
        
    def add_edge(self, other):
        self.successors.add(other)
        other.predecessors.add(self)
        
    def __repr__(self):
        return 'Block_%d' % self.index
        
    deltas = property(_get_deltas)
    defns = property(_get_defs)
    uses = property(_get_uses)
    
    # -1 is in reg, +1 is in mem.
    def ref_preferred_location(self, refset):
        start = {}
        end = {}
        live = self.live_in & refset
        for i in self.instructions:
            if i.__class__ is builtin.TStore:
                if i.index in refset and i.index not in start:
                    start[i.index] = 0
            elif i.__class__ is builtin.TLoad:
                if i.index in refset and i.index not in start:
                    start[i.index] = -1
            elif i.may_gc():
                for r in live:
                    if r not in start:
                        start[r] = 1
        live = self.live_out & refset
        for i in self.instructions[::-1]:
            if i.__class__ is builtin.TStore:
                if i.index in refset and i.index not in end:
                    end[i.index] = -1
            elif i.__class__ is builtin.TLoad:
                if i.index in refset and i.index not in end:
                    end[i.index] = -1
            elif i.may_gc():
                for r in live:
                    if r not in end:
                        end[r] = 1
        for r in refset:
            if r not in end:
                end[r] = 0
            if r not in start:
                start[r] = 0
        self.ref_preferred_start = start
        self.ref_preferred_end = end
       
def draw_graph(g, out, opt):
    esets = g.edge_sets
    out << '''digraph g {
    graph [
    rankdir = "TB"
    ratio = "2"
    ];
    node [
    fontsize = "8"
    shape = "rectangle"
    ];
    edge [
    ];
'''
    refset = g.gc_temps()
    doms = graph.find_dominators(g)
    lnt = graph.loop_nest_tree(doms, graph.dominator_tree(graph.immediate_dominators(doms)))
    graph.weight(g, doms, lnt)
    for block in g:
        block.ref_preferred_location(refset)
    g.annotate_edges_with_ref_preferences(refset)
    for block in g:
        draw_block(block, out, opt)
        for s in block.successors:
            draw_edge(block, s, out, opt)
    out << '}\n'   
    
def _pref(t):
    if t[1] <= 0:
        return str(t[0]) + ":reg"
    else:
        return str(t[0]) + ":mem"
        
def draw_block(block, out, opt):
    out << '"%s" [\n' % block.name.replace('"', r'\"')
    out << 'label = "{'
    out << 'Block %d ' % block.index
    if 'w' in opt:
        out << ' | weight: ' << block.weight
    if 'd' in opt:
        out << ' | uses: ' << ', '.join([str(x) for x in block.uses])
    if 'l' in opt:
        out << ' | live: ' << ', '.join([str(x) for x in block.live_in])
    if 'p' in opt:
        out << ' | pref: ' << ', '.join([_pref(t) for t in block.ref_preferred_start.items()])
    if 'f' in opt:
        out << ' | phis: ' << ', '.join([str(x) for x in block.phis])
    if 'i' in opt:
        for i in block.instructions:
            out << ' | '  << i.name.replace('"', r'\"')
    if 'd' in opt:
        out << ' | defns: ' << ', '.join([str(x) for x in block.defns])
    if 'l' in opt:
        out << ' | ' << 'live: ' << ', '.join([str(x) for x in block.live_out])
    if 'p' in opt:
        out << ' | ' << 'pref: ' << ', '.join([_pref(t) for t in block.ref_preferred_end.items()])
    if 'x' in opt and block.exit:
        out << ' | ' << 'exits'
    out << '}"\nshape = "record"\n];\n'
 
def draw_edge(start, end, out, opt):
    if start.child and 'p' in opt:
        prefs = ','.join([_pref(x) for x in start.child.ref_preferences.items()])
        out << '%s -> %s [ label = "%s" ];\n' % (start.name, end.name, prefs)
    else:
        out << '%s -> %s;\n' % (start.name, end.name)

def pp_print(d):
    l = []
    for k in d:
        l.append((k,d[k]))
    l.sort(key=lambda t: t[0].index)
    for k, v in l:
        print k, ":", ', '.join([str(i) for i in sorted(v)])
        
def print_summary(graph):
    print "SSA definitions: "
    pp_print(ssa.definitions(graph))
    print "________________________________________"
    print
    print "SSA dominators: "
    pp_print(graph.dominators())
    print "________________________________________"
    print
    print "SSA dominance frontiers: "
    pp_print(graph.dominance_frontiers())
    print "________________________________________"
    print
    print "SSA Phi nodes: "
    pp_print(ssa.phi_nodes(graph))
    
def print_options():
    print "-i Show instructions"
    print "-l Show liveness"
    print "-p Show perferred storage on edges(register or memory)"
    print "-d Show definitions"
    print "-f Show phi-nodes"
    print "-w Show node weights"
    print "-x Show exit"
    
def get_graph_from_argv():
    import getopt
    opts, args = getopt.getopt(sys.argv[1:], 'iwpldfxO')
    verbose = False
    for o,v in opts:
        assert o[0] == '-'
        if o == '-h':
            print ("Usage: %s gsc_file instruction_name output_file" % sys.argv[0])
            print_options()
            sys.exit(0)
    if len(args) != 3:
        print ("Usage: %s gsc_file instruction_name output_file" % sys.argv[0])
        print_options()
        sys.exit(1)
    infile = common.In(args[0])
    name = args[1]
    import gsc
    gfile = gsc.read(infile)
    inst = gfile.code.instructions
    if gfile.bytecodes:
        inst = inst + gfile.bytecodes.instructions
    for i in inst:
        if name == i.name:
            return i.flow_graph
    print name, "not found"
    sys.exit(1)
    
    
if __name__ == '__main__':
    flow = get_graph_from_argv()
    import getopt
    opts, args = getopt.getopt(sys.argv[1:], 'iwpldfxO')
    opt = set()
    for o,v in opts:
        opt.add(o[1])
    if 'O' in opt:
        import gc_optimiser
        gc_optimiser.optimise_allocate(flow)
    if 's' in opt:
        defns = ssa.definitions(flow)
        phis = ssa.phi_nodes(flow)
        for bb in flow:
            bb.phis = phis[bb]
            bb.defns = defns[bb]
    draw_graph(flow, common.Out(args[2]), opt)
    print_summary(flow)
    sys.exit(0)
            
