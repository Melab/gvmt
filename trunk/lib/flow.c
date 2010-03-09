#include "bitset.h"
#include "flow.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "arena.h"

void fatal(char* format, ...);

GVMT_FlowGraph new_flowgraph(Arena arena) {
    GVMT_FlowGraph flow_graph = gvmt_allocate(sizeof(struct gvmt_flow_graph), arena);
    flow_graph->arena = arena;
    flow_graph->edges.end = flow_graph->edges.start = gvmt_allocate(sizeof(struct gvmt_edge) * 64, arena);
    flow_graph->edges.limit = flow_graph->edges.start + 64;
    flow_graph->edge_sets.end = flow_graph->edge_sets.start = gvmt_allocate(sizeof(struct gvmt_edge) * 32, arena);
    flow_graph->edge_sets.limit = flow_graph->edge_sets.start + 32;
    return flow_graph;  
}

void gvmt_edge_vector_expand_buffer(GVMT_EdgeVector v, Arena arena) {     
    GVMT_Edge new_edges = gvmt_allocate(sizeof(struct gvmt_edge) * 2 * ((v)->end - (v)->start), arena);
    int new_size = (v->limit - v->start) * 2;
    memcpy(new_edges, v->start, sizeof(struct gvmt_edge) * (v->limit - v->start));
    v->end += new_edges - v->start;
    v->limit =  new_edges + new_size;
    v->start = new_edges;
}

void gvmt_edge_set_vector_expand_buffer(GVMT_EdgeSetVector v, Arena arena) {     
    GVMT_EdgeSet new_edgesets = gvmt_allocate(sizeof(struct gvmt_edge_set) * 2 * ((v)->end - (v)->start), arena);
    int new_size = (v->limit - v->start) * 2;
    memcpy(new_edgesets, v->start, sizeof(struct gvmt_edge_set) * (v->limit - v->start));
    v->end += new_edgesets - v->start;
    v->limit = new_edgesets + new_size;
    v->start = new_edgesets;
}

void gvmt_add_edge_set(BitSet parents, BitSet children, GVMT_EdgeSetVector v, Arena arena) {
    if (v->end == v->limit)
        gvmt_edge_set_vector_expand_buffer(v, arena);
    v->end->parents = parents;
    v->end->children = children;
    v->end->stack = 0;
    v->end++;
}
 
void gvmt_add_edge(int start, int end, GVMT_EdgeVector vector, Arena arena) {
    assert(((unsigned)start) < 100000);
    assert(((unsigned)end) < 100000);
    if (vector->end == vector->limit) {
        gvmt_edge_vector_expand_buffer(vector, arena);
    }
    vector->end->predecessor = start;
    vector->end->successor = end;
    vector->end++;
}

/** Binary search algorithm from NIST - Public domain. */
int get_block_ending_at(int end, GVMT_BlockVector blocks) {
    int n  = blocks->end - blocks->start;
    int high, i, low;
    for ( low=(-1), high=n-1;  high-low > 1;) {
        i = (high+low) / 2;
        if ( end <= blocks->start[i].end ) 
            high = i;
        else           
            low  = i;
    }
    if (end == blocks->start[high].end )  
        return( high );
     else     
        fatal("Cannot find block ending at: %d\n", end);
     return -1;
}

/** Binary search algorithm from NIST - Public domain. */
int get_block_starting_at(int start, GVMT_BlockVector blocks) {
    int n  = blocks->end - blocks->start;
    int high, i, low;
    for ( low=(-1), high=n-1;  high-low > 1;) {
        i = (high+low) / 2;
        if ( start <= blocks->start[i].start ) 
            high = i;
        else           
            low  = i;
    }
    if (start == blocks->start[high].start )  
        return( high );
     else         
        fatal("Cannot find block starting at: %d\n", start);
     return -1;    
}

void process_edge(GVMT_Edge e, GVMT_BlockVector blocks) {
    int from = get_block_ending_at(e->predecessor, blocks);
    int to = get_block_starting_at(e->successor, blocks);
    assert(from < blocks->end - blocks->start);
    assert(to < blocks->end - blocks->start);
    set_add(blocks->start[from].successors, to);
    set_add(blocks->start[to].predecessors, from);
}

void process_edges(GVMT_EdgeVector edges, GVMT_BlockVector blocks) {
    GVMT_Edge e;
    for (e = edges->start; e < edges->end; e++) {
        process_edge(e, blocks);
    }
}

#define SWAP(a,b) do { BitSet t = a; a = b; b = t; } while(0)

static int edge_sets = 0;

void build_edge_set(int seed_parent, GVMT_BlockVector blocks, BitSet parents, BitSet children, int block_count, Arena arena) {
    BitSet work_parents = set_new(block_count, arena);
    BitSet next_parents = set_new(block_count, arena);
    BitSet work_children = set_new(block_count, arena);
    BitSet next_children = set_new(block_count, arena);
    assert(set_empty(parents));
    assert(set_empty(children));
    int i;
    set_add(work_parents, seed_parent);
    do {
        FOR_EACH(i, work_parents)
            set_addSet(next_children, blocks->start[i].successors);
        FOR_EACH(i, work_children)    
            set_addSet(next_parents, blocks->start[i].predecessors);
        set_addSet(parents, work_parents);
        set_addSet(children, work_children);
        set_removeSet(next_parents, parents);
        set_removeSet(next_children, children);
        if (set_empty(next_parents) && set_empty(next_children)) {
            printf("Edge set %d built\n", ++edge_sets);
            return;
        }
        SWAP(work_parents, next_parents);
        SWAP(work_children, next_children);
        set_clear(next_parents);
        set_clear(next_children);
    } while(1);   
}

void add_edge_sets_to_blocks(GVMT_EdgeSetVector edge_sets, GVMT_BlockVector blocks) {
    GVMT_EdgeSet es;
    int i;
    for (es = edge_sets->start; es < edge_sets->end; es++) {
        FOR_EACH(i, es->parents)
            blocks->start[i].edge_set = es;
    }
}

GVMT_EdgeSet gvmt_get_edge_set_for_ip(int offset, GVMT_BlockVector blocks) {
    int block =  get_block_ending_at(offset, blocks);
    return blocks->start[block].edge_set;
}

static void print_block(GVMT_Block b, FILE *out) {
    fprintf(out, "block_%d [\n", b->start);
    fprintf(out, "label = \"{");
    fprintf(out, "Start: %d|", b->start);
    fprintf(out, "End: %d}\"\n", b->end);
    fprintf(out, "shape = \"record\"\n];\n");
}

static void print_edge(GVMT_Edge e, FILE *out) {
    fprintf(out, "edge_%d [\n", e->predecessor);
    fprintf(out, "label = \"{");
    fprintf(out, "Pre: %d|", e->predecessor);
    fprintf(out, "Succ: %d}\"\n", e->successor);
    fprintf(out, "shape = \"record\"\n];\n");
}

void print_flowgraph(GVMT_FlowGraph graph, char* name) {
    FILE *out;
    char buf[100];
    sprintf(buf, "/tmp/%s.dot", name);
    out = fopen(buf, "w");
    fprintf(out, "digraph g {           \n");
    fprintf(out, "graph [               \n");
    fprintf(out, "rankdir = \"TB\"      \n");
    fprintf(out, "ratio = \"2\"         \n");
    fprintf(out, "];                    \n");
    fprintf(out, "node [                \n");
    fprintf(out, "fontsize = \"8\"      \n");
    fprintf(out, "shape = \"rectangle\" \n");
    fprintf(out, "];                    \n");
    fprintf(out, "edge [                \n");
    fprintf(out, "];                    \n");
    for(GVMT_Block b = graph->blocks.start; b < graph->blocks.end; b++) {
        print_block(b, out);   
    }
    for(GVMT_Block b = graph->blocks.start; b < graph->blocks.end; b++) {
        if (b->successors) {
            int i;
            FOR_EACH(i, b->successors) {
                fprintf(out, "block_%d -> block_%d\n", b->start, graph->blocks.start[i].start);
            }
        }
    }
//    for(GVMT_Edge e = graph->edges.start; e < graph->edges.end; e++) {
//        print_edge(e, out);   
//    }
    fprintf(out, "}\n");
}

void depth_first_recursive(GVMT_FlowGraph graph, GVMT_Block b, BitSet visited, block_func func, void *prev) {
    void* next = func(b, prev);
    int i;
    FOR_EACH(i, b->successors) {
        if (!set_contains(visited, i)) {
            set_add(visited, i);
            depth_first_recursive(graph, &graph->blocks.start[i], visited, func, next); 
        }
    }
}

void depth_first(GVMT_FlowGraph graph, block_func func, void* start) {
    BitSet visited = set_new(graph->blocks.end - graph->blocks.start, graph->arena);
    set_add(visited, 0);
    depth_first_recursive(graph, graph->blocks.start, visited, func, start);
}

