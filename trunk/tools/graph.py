# All graph algorithms, except SSA based ones. 
import common, flow_graph, sys

class TreeNode(object):
    __slots__ = [ 'node', 'children' ]
    
    def __init__(self, node):
        self.node = node
        self.children = set()
        
    def top_down(self):
        'Returns an iterator which yields the nodes in top-down order'
        yield self.node
        for c in self.children:
            for node in c.top_down():
                yield node

def _weight_branches(node, weight, dominators):
    node.weight += weight
    s = node.successors
    l = [n for n in s if n not in dominators[node]]
    for n in l:
        _weight_branches(n, weight / len(l), dominators)
    
def _weight_loops(root, weight):
    root.header.weight *= weight
    for n in root.nodes:
        n.weight *= weight
    for c in root.children:
        _weight_loops(c, weight * 10.0)
        
def weight(graph, dominators, loop_tree):
    for n in graph:
        n.weight = 0.0
    _weight_branches(graph.nodes[0], 1.0, dominators)
    _weight_loops(loop_tree, 1.0)

def find_dominators(graph):
    all_nodes = set()
    for n in graph.nodes:
        all_nodes.add(n)
    dominators = {}
    for node in graph:
        dominators[node] = all_nodes
    dominators[graph.nodes[0]] = set([graph.nodes[0]])
    done = False
    while not done:
        done = True
        for node in graph.nodes[1:]:
            s = all_nodes
            for p in node.predecessors:
                s = s.intersection(dominators[p])
            s.add(node)
            if s != dominators[node]:
                done = False
                dominators[node] = s
    return dominators
    
def find_reachable_from(graph):
    all_nodes = set()
    for n in graph.nodes:
        all_nodes.add(n)
    reachable = {}
    for node in graph:
        reachable[node] = node
    done = False
    while not done:
        done = True
        for node in graph.nodes[1:]:
            s = set()
            for p in node.predecessors:
                s = s.union(reachable[p])
            s.add(node)
            if s != reachable[node]:
                done = False
                reachable[node] = s
    return reachable
    
def immediate_dominators(dominators):
    l = []
    for node in dominators:
        l.append((len(dominators[node]), node))
    l.sort()
    idoms = {}
    length, node  = l[0]
    assert length == 1
    idoms[node] = None
    for size, node in l[1:]:
        for s, n in l:
            if s + 1 == size and n in dominators[node]:
                idoms[node] = n
    return idoms
    
def dominator_tree(idoms):
    tree_nodes = {}
    for node in idoms:
        tree_nodes[node] = TreeNode(node)
    for node in idoms:
        if idoms[node]:
            tree_nodes[idoms[node]].children.add(tree_nodes[node])
        else:
            root = tree_nodes[node] 
    return root
    
def _df_local(node, idoms):
    return set([n for n in node.successors if idoms[n] != node])
    
def _find_df(root, df, idoms, dominators):
    node = root.node
    dfn = _df_local(node, idoms)
    for child in root.children:
        dfc = _find_df(child, df, idoms, dominators)
        for c in dfc:
            if node not in dominators[c]:
                dfn.add(c)
    df[root.node] = dfn
    return dfn
        
def dominance_frontiers(idoms, tree_root, dominators):
    df = {}
    _find_df(tree_root, df, idoms, dominators)
    return df

def _in_loop(node, head, dominators, nodes, visited):
    if node in visited:
        return
    visited.add(node)
    if head in dominators[node]:
        nodes.add(node)
        for p in node.predecessors:
            _in_loop(p, head, dominators, nodes, visited)
    
def _gather_loop(head, tail, dominators):
    # node must be dominated by head and
    # tail must be reachable from node
    # without going through head
    nodes = set()
    visited = set()
    visited.add(head)
    _in_loop(tail, head, dominators, nodes, visited)
    return nodes
    
class LoopTreeNode(object):
    __slots__ = [ 'header', 'nodes', 'children', 'depth' ]
    
    def __init__(self, header):
        self.header = header
        self.nodes = set()
        self.children = set()
        self.depth = 0
    
def _remove_child_nodes(root):
    for c in root.children:
        root.nodes -= c.nodes
        root.nodes.discard(c.header)
        _remove_child_nodes(c)
    
def loop_nest_tree(dominators, dom_tree):
    loops = {}
    top = LoopTreeNode(dom_tree.node)
    for node in dominators:
        top.nodes.add(node)
        for s in node.successors:
            if s in dominators[node]:
                # Back edge
                if s in loops:
                    loop = loops[s]
                else:
                    loop = LoopTreeNode(s)
                    loops[s] = loop
                loop.nodes |= _gather_loop(s, node, dominators)
    l = [(len(top.nodes), top)]
    for loop in loops.values():
        l.append((len(loop.nodes), loop))
    l.sort()
    for i in range(len(l)):
        l1 = l[i][1]
        for j in range(i+1, len(l)):
            l2 = l[j][1]
            if l1.header in l2.nodes:
                l2.children.add(l1) 
                break
    _remove_child_nodes(top)
    return top

def print_loop_tree_node(root, out):
    out << '"loop_%s" [\n' % root.header.index
    text = '%d\\n%s' % (root.header.index, ' '.join(['%d' % n.index for n in root.nodes]))
    out << 'label = "%s"\nshape = "ellipse"\n];\n' % text
    for c in root.children:
         out << 'loop_%d -> loop_%d;\n' % (root.header.index, c.header.index)
         print_loop_tree_node(c, out)
    
def print_tree_node(root, out):
    out << '"bb_%s" [\n' % root.node.index
    out << 'label = "%d"\nshape = "ellipse"\n];\n' % root.node.index
    for c in root.children:
         out << 'bb_%d -> bb_%d;\n' % (root.node.index, c.node.index)
         print_loop_tree_node(c, out)
   
def print_loop_tree(root, out):
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
    print_loop_tree_node(root, out)
    out << '}\n'   
   
def print_tree(root, out):
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
    print_tree_node(root, out)
    out << '}\n'   
    
def _print_dict(d):
    l = []
    for k in d:
        l.append((k,d[k]))
    l.sort
    for k,v in l:
        print k, ":", v
              
if __name__ == '__main__':
    import flow_graph
    infile = common.In(sys.argv[1])
    name = sys.argv[2]
    import gsc
    gfile = gsc.read(infile)
    inst = gfile.code.instructions
    if gfile.bytecodes:
        inst = inst + gfile.bytecodes.instructions
    for i in inst:
        if name == i.name:
            dominators = find_dominators(i.flow_graph)
            idoms = immediate_dominators(dominators)
            tree = dominator_tree(idoms)
#            print_tree(tree, common.Out(sys.argv[3]))
#            _print_dict(idoms)
#            _print_dict(dominance_frontiers(idoms, tree, dominators))
            print_loop_tree(loop_nest_tree(dominators, tree), common.Out(sys.argv[3]))
            sys.exit(0)
    print name, "not found"
    sys.exit(1)
           
                 
