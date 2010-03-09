/**  Copyright Mark Shannon and the University of York 2006 */

#define STACK_MACHINE
#include "c.h"

#define NODEPTR_TYPE Node
#define OP_LABEL(p) ((p)->op)
#define LEFT_CHILD(p) ((p)->kids[0])
#define RIGHT_CHILD(p) ((p)->kids[1])
#define STATE_LABEL(p) ((p)->x.state)

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
   
#define REGS 0xffffffff
void gvmt_address(Symbol, Symbol, long);
void gvmt_defaddress(Symbol);
static void defconst(int, int, Value);
static void defstring(int, char *);
void gvmt_defsymbol(Symbol);
static void export(Symbol);
static void function(Symbol, Symbol [], Symbol [], int);
void gvmt_global(Symbol);
static void import(Symbol);
static void progbeg(int, char **);
static void progend(void);
static void segment(int);
static void space(int);
static void treeemit(Node forest);
extern Node gvmt_label(Node forest);

extern void swtch(Symbol label);
char* type_name(int t);

static int makeTemp(Symbol p)
{
      if (!isscalar(p->type) || p->type->size == 8) {
        p->sclass = AUTO;
        return 0;
    } else if (p->sclass == AUTO) {
        if (p->addressed) {
            return 0;
        } else {
            return 1;
        }
    } else if (p->sclass == REGISTER) {
        return 1;
    } else {
        return 0;    
    }  
}
int get_type(Type t, int level);
void gvmt_local(Symbol p);
extern int local_variable_count;

static void function(Symbol f, Symbol caller[], Symbol callee[], int ncalls) {
    int i, size, varargs;
    Symbol* p;
    for (p = caller; *p; p++)     
        (*p)->x.type = get_type((*p)->type, 0);
    for (p = callee; *p; p++)     
        (*p)->x.type = get_type((*p)->type, 0);
	for (i = 0; callee[i]; i++) {
        Symbol p = callee[i];
        Symbol q = caller[i];
        gvmt_local(p);
        q->sclass = p->sclass = REGISTER;
        q->x.name = p->x.name;
        q->x.type = p->x.type;
    }
//    print("Function %s \\\n", f->x.name);
	//params = stackParameters(caller, callee);
	gencode(caller, callee);
	// framesize = roundup(offset, 4);
	emitcode();
}


static Node recalcNode(Node temp, Node calc) {
    int op = specific(calc->op);
    if (op == CNST + I) {
        int lit = calc->syms[0]->u.c.v.i;
        if (lit >= -(1<<16)    && lit < (1<<16)) {
            return calc;
        } else {
            return temp;
        }
    } else if (op == LSH + I) {
        if (specific(calc->kids[1]->op) == CNST + I && 
            calc->kids[1]->syms[0]->u.c.v.i == 2) {
            int lhs_op = specific(calc->kids[0]->op);
            if (lhs_op == INDIR + I) {
                int addr_op = specific(calc->kids[0]->kids[0]->op);
                if (addr_op == ADDRL + P || addr_op == ADDRG + P) {
                    return calc;
                }
            }
        }
    }
    return temp;
}


static int call_label = 0;

static int prevline = 0;
static int currentline = 0;


static void defconst(int suffix, int size, Value v) {
}

static void defaddress(Symbol p) {
//       print(".word %s\n", p->x.name);
}

static void defstring(int n, char *str) {
}

static void export(Symbol p) {
}

static void import(Symbol p) {
//        if (!isfunc(p->type))
//                print(".extern %s %d\n", p->name, p->type->size);
}

static void address(Symbol q, Symbol p, long n) {
    if (n)
        q->x.name = stringf("%s%s%d", p->name, n >= 0 ? "+" : "", n);
    else 
        q->x.name = p->name;
}

static void global(Symbol p) {
}

static void segment(int n) {
}

static void space(int n) {
}

static char *currentfile;

static int bitcount(unsigned mask) {
        unsigned i, n = 0;

        for (i = 1; i; i <<= 1)
                if (mask&i)
                        n++;
        return n;
}
 
/* stabinit - initialize stab output */
void gvmt_stabinit(char *file, int argc, char *argv[]);
/* stabline - emit stab entry for source coordinate *cp */
void svg_stabline(Coordinate *cp);

/* stabsym - output a stab entry for symbol p */
void gvmt_stabsym(Symbol p);

extern int bytecodes;
 
static void progbeg(int argc, char *argv[]) {
    print("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\" ");
    print("width=\"210mm\" height=\"297mm\" viewBox = \"0 0 2100 2970\" >\n");
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-xbytecodes") == 0) {
            bytecodes = 1;
        }
    }
}

static void progend(void) {
    print("</svg>\n");
}

static void blockbeg(Env* env) {
}

static void blockend(Env* env) {
}

struct left_right {
    int left; 
    int right;
};

struct shape {
    int length;
    struct left_right items[1];
};

typedef struct shape* Shape;

static Shape new_shape(int length) {
    Shape result = (Shape) allocate(length * sizeof(struct left_right) + sizeof(int), FUNC);
    assert(length > 0);
    result->length = length;
    return result;
}
 
static int next_id = 1;
static int X_SCALE = 80;
static int Y_SCALE = 60;
static int X_OFFSET = 600;
static int Y_OFFSET = 20;
static int CHAR_SIZE = 5;
static int FONT_SIZE = 14;
static int Y_SPACING = 30;

static Shape treeWalk(Node p) {
    int i, min_len, max_len;
    int sep, half_sep, offset;
    Shape shape;
    if (p->kids[1]) {
        Shape shape0 = treeWalk(p->kids[0]);
        Shape shape1 = treeWalk(p->kids[1]);
        Shape deeper;
        min_len = min(shape0->length, shape1->length);
        sep = 0;
        for (i = 0; i < min_len; i++) {
            sep = max(sep, shape0->items[i].right + shape1->items[i].left);
        }            
        half_sep = sep / 2;
        max_len = max(shape0->length, shape1->length);
        shape = new_shape(max_len + 1);
        for (i = 0; i < min_len; i++) {
            shape->items[i + 1].left = shape0->items[i].left + half_sep;
            shape->items[i + 1].right = shape1->items[i].right + half_sep;
        }
        if (shape0->length > shape1->length) {
            deeper = shape0;
            offset = -half_sep;
        } else {
            deeper = shape1;
            offset = half_sep;
        }
        for (i = min_len; i < max_len; i++) {
            shape->items[i + 1].left = deeper->items[i].left - offset;
            shape->items[i + 1].right = deeper->items[i].right + offset;
        }
        p->kids[0]->x.centre = -half_sep;
        p->kids[1]->x.centre = half_sep;
    } else if (p->kids[0]) {
        Shape kid_shape = treeWalk(p->kids[0]);
        shape = new_shape(kid_shape->length + 1);
        for (i = 0; i < kid_shape->length; i++) {
            shape->items[i + 1] = kid_shape->items[i];
        }
        p->kids[0]->x.centre = 0;
    } else {
        shape = new_shape(1);
    } 
    shape->items[0].left = X_SCALE;
    shape->items[0].right = X_SCALE;
    return shape;
}

char* getNodeText(Node p) {
    switch (generic(p->op)) {
    case LABEL:
        return stringf("LABEL %s", p->syms[0]->x.name);
    case ADDRL:
        if (isdigit(p->syms[0]->name[0]))
            return stringf("ADDRL t%s", p->syms[0]->name);
        else
            return stringf("ADDRL %s", p->syms[0]->name);
    case ADDRG:
        return stringf("ADDRG %s", p->syms[0]->x.name);
    case ADDRF:
        return stringf("ADDRF %s", p->syms[0]->name);
    case EQ:
        return stringf("BREQ %s", p->syms[0]->x.name);
    case NE:
        return stringf("BRNE %s", p->syms[0]->x.name);
    case LT:
        return stringf("BRLT %s", p->syms[0]->x.name);
    case GT:
        return stringf("BRGT %s", p->syms[0]->x.name);
    case LE:
        return stringf("BRLE %s", p->syms[0]->x.name);
    case GE:
        return stringf("BRGE %s", p->syms[0]->x.name);
    case CNST:
        if (optype(p->op) == I)
            return stringf("CNST %d", p->syms[0]->u.c.v.i);
        else if (optype(p->op) == U)
            return stringf("CNST %u", p->syms[0]->u.c.v.u);
        else
            return opname(p->op);
    default:   
        return opname(p->op);
    }
}

int getTextSize(Node p) {
    return strlen(getNodeText(p)); 
}

extern void print_type_set(small_set s);

static void drawNode(Node p, int x, int y) {
    int textSize = getTextSize(p);
    print("<rect style=\"stroke: black; fill: white;\" x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"%d\" ry=\"%d\" ",
            x + X_OFFSET - X_SCALE/4 - CHAR_SIZE * textSize, y, 2 * CHAR_SIZE * textSize + X_SCALE/2, Y_SPACING, X_SCALE/4, X_SCALE/4);
    print("/>\n");
    print("<text ");
    print("style=\"font-size:%d;font-style:normal;font-weight:normal;text-anchor:start;fill:#000000;font-family:sans\" ", FONT_SIZE);
    print("x=\"%d\" ", (x + X_OFFSET - CHAR_SIZE * textSize));
    print("y=\"%d\" ", (y  + Y_OFFSET + CHAR));
    print("id=\"id%d\">", next_id++);
    print(getNodeText(p));
    print(" [%s]" , type_name(p->x.exact_type));
    print_type_set(p->x.type_set);
    if (p->x.ptype)
        print(" %t ", p->x.ptype);
    if (p->x.eliminated)
        print(" ELIMINATED ");
    print("</text>\n");
}

static void drawLine(int x1, int y1, int x2, int y2) {
    print("<line style=\"stroke: black;\" x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\"/>\n",
        (x1 + X_OFFSET), (y1  + Y_OFFSET), (x2 + X_OFFSET), (y2  + Y_OFFSET));
}

static void drawLines(Node p, int x, int y) {  
    if (p->kids[0]) {
        drawLines(p->kids[0], x + p->x.centre, y + Y_SCALE);
        drawLine(x + p->x.centre, y, x + p->x.centre + p->kids[0]->x.centre, y + Y_SCALE);
    }
    if (p->kids[1]) {   
        drawLines(p->kids[1], x + p->x.centre, y + Y_SCALE);
        drawLine(x + p->x.centre, y, x + p->x.centre + p->kids[1]->x.centre, y + Y_SCALE);
    }
}

static void drawNodes(Node p, int x, int y) {  
    drawNode(p, x + p->x.centre, y);
    if (p->kids[0]) {
        drawNodes(p->kids[0], x + p->x.centre, y + Y_SCALE);
    }
    if (p->kids[1]) {   
        drawNodes(p->kids[1], x + p->x.centre, y + Y_SCALE);
    }
}

static int doTree(Node p, int y) {
    Shape shape = treeWalk(p);
    int i, centre;
    int left = 0;
    int right = 0;
    p->x.centre = 0;
    for (i = 0; i < shape->length; i++) {
        left = max(left, shape->items[i].left);   
        right = max(right, shape->items[i].right);   
    }
    centre = (left - right)/2;
    drawLines(p, centre, y);
    drawNodes(p, centre, y);
    return y + shape->length * Y_SCALE + Y_SPACING;
}

static int y = 50;

static void treeemit(Node forest) {
    Node p;
    for (p = forest; p; p = p->link) {
        y = doTree(p, y);
    }    
}

static Node pass(Node forest) {
    return forest;
}

static char *currentfile;
static int currentline;

/* stabinit - initialize stab output */
void svg_stabinit(char *file, int argc, char *argv[]) {
        if (file) {
                currentfile = file;
        }
        currentline = 1;
}

/* stabline - emit stab entry for source coordinate *cp */
void svg_stabline(Coordinate *cp) {
    
    if (cp->file && cp->file != currentfile) {
            // print(".file 2,\"%s\"\n", cp->file);
            currentfile = cp->file;
    }
    currentline = cp->y;
    print("<text ");
    print("style=\"font-size:%d;font-style:normal;font-weight:normal;text-anchor:start;fill:#000000;font-family:sans\" ", FONT_SIZE);
    print("x=\"100\" y=\"%d\" ", (y  + Y_OFFSET + CHAR));
    print("id=\"id%d\">", next_id++);
    print("Line %d</text>\n", currentline);
    y += 20;
    // print(".loc 2,%d \\\n", cp->y);
}

#define char_metrics          { 1, 1, 0 }
#define short_metrics         { 2, 2, 0 } 
#define int_metrics           { 4, 4, 0 } 
#define long_metrics          { 4, 4, 0 }
#define long_long_metrics     { 4, 4, 0 }
#define float_metrics         { 4, 4, 0 } 
#define double_metrics        { 4, 4, 0 }
#define long_double_metrics   { 4, 4, 0 } 
#define word_metrics          { 4, 4, 0 } 
#define struct_metrics        { 0, 4, 0 } 

Interface svgIR = {
        char_metrics,
        short_metrics,
        int_metrics,
        long_metrics,
        long_long_metrics,
        float_metrics,
        double_metrics,
        long_double_metrics,
        word_metrics,
        struct_metrics,
        0,
        0,  /* mulops_calls */
        0,  /* wants_callb */
        0,  /* wants_argb */
        0,  /* left_to_right */
        0,  /* wants_dag */
        0,  /* unsigned_char */
        gvmt_address, 
        blockbeg,
        blockend,
        gvmt_defaddress,
        defconst,
        defstring,
        gvmt_defsymbol,
        treeemit,
        export,
        function,
        gvmt_label,
        global,
        import,
        gvmt_local,
        progbeg,
        progend,
        segment,
        space,
        0, 0, 0, svg_stabinit, svg_stabline, gvmt_stabsym, 0
};
