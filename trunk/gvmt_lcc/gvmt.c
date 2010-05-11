#include "c.h"

void gvmt_error(const char *fmt, ...);

enum {
    ERROR = 0,
    STRUCT_TYPE,
    OPAQUE_TYPE,
    INTEGER_TYPE,
    MEMBER, // This is equivalent to ERROR* 
};

#define OPAQUE_POINTERS  0xccccccc0
#define ROOTS 1675

/** Type macros */

/** Non-zero if type t is opaque, int or a pointer to either */
#define is_opaque(t) ((t) & 2)

#define DEREF(t) ((t)-4)
#define POINTER(t) ((t)+4)
#define REFERENCE_TYPE POINTER(STRUCT_TYPE)

static char *N_ARGS[100];
static char **n_args = N_ARGS;

static int insert_gc_safe_point = 0;
static int emit_dot_file = 0;

/** Do not insert GC_SAFE and use N_CALL_NO_GC rather than N_CALL */ 
static int requested_no_gc = 0;

static List type_info_list;

static int opaque_struct(Type t) {
    char *name = t->u.sym->name;
    int l = strlen(name);
    if (strcmp(name, "gvmt_reference_types") == 0)
        return 0;
    else if (strncmp("gvmt_object_", name, 12)) 
        return 1;
    else {
        return 0;
    }
}

static void type_info(Type t) {
    t = unqual(t);
    if (t->x.printed)
        return;
    t->x.printed = 1;
    if (isstruct(t)) {
        type_info_list = append(t, type_info_list);
    } else if (isptr(t)) {
        type_info(t->type);
    }
}

static print_typename(Type t) {
    t = unqual(t);
    if (is_ref(t)) {
        char* tag_name = deref(t)->u.sym->name;
        char* name;
        if (strcmp(tag_name, "gvmt_reference_types"))
            name = stringn(tag_name + 12, strlen(tag_name + 12));
        else
            name = "GVMT_Object";
        print("R(%s)", name);
    } else if (isstruct(t)) {
        assert(opaque_struct(t));
        print("S(%s)", t->u.sym->name);
    } else if (isptr(t) || isarray(t)) {
        print("P(");
        print_typename(t->type);
        print(")");
    } else if (isunsigned(t)) {
        print ("uint%d", t->size*8); 
    } else if (isint(t)) {
        print("int%d", t->size*8); 
    } else if (isfloat(t)) { 
        print("float%d", t->size*8);
    } else {
        print("?");
    }
}

static void print_member(Field f) {
    Type t = unqual(f->type);
    if (isarray(t)) {
        int i, count = t->size / t->type->size;
        print("   .member %s ", f->name);
        print_typename(t->type);
        print("*%d @ %d\n", count, f->offset);
    } else {
        print("   .member %s ", f->name);
        print_typename(f->type);
        print(" @ %d\n", f->offset);     
    }
}

static void print_type(Type t) {
    t = unqual(t);
    if (isstruct(t)) {
        char* tag_name = t->u.sym->name;
        Field f = t->u.sym->u.s.flist;
        if (opaque_struct(t)) {
            print(".type struct %s\n", tag_name);
        } else {
            print(".type object %s\n",  stringn(tag_name + 12, strlen(tag_name + 12)));
        }
        while (f) {
            print_member(f);
            f = f->link;
        }
    }
}

static void print_type_info(void) {
    if (type_info_list) {
        List item = type_info_list;
        do {
            Type t = (Type)item->x; 
            item = item->link;
            print_type(t);
            t->x.printed = 0;
        } while (item != type_info_list);
    }
}

static int valid_type(int t) {
    int v;
    if (t > 31 || t < 0)
        v = 0;
    else 
        v = (((1 << t) & 0xeeeeeefe) != 0);
//    if (v == 0)
//        printf("Invalid type: %d", t);
    return v;
}

static int make_pointer(int t) {
    if (t == ERROR) {
        return ERROR;
    } else {
        int x = POINTER(t);
        assert(valid_type(x));
        return x;
    }
}

small_set OPAQUE_SET = { 1 << OPAQUE_TYPE }; // OPAQUE
small_set INTEGER_SET = { 1 << INTEGER_TYPE  }; // INTEGER
small_set EMPTY_SET = { 0 };
small_set MEMBERS_SET = { (1 << REFERENCE_TYPE) | (1 << INTEGER_TYPE) | (1 << OPAQUE_TYPE) };
small_set NULL_SET = { (1 << REFERENCE_TYPE) | (1 << INTEGER_TYPE) | (1 << POINTER(OPAQUE_TYPE)) }; 

#define NON_OPAQUES 0x33333333
#define ALL_OPAQUES 0xcccccccc
static int ARGS_SET = ALL_OPAQUES | (1 << REFERENCE_TYPE) | (1 << POINTER(REFERENCE_TYPE));
static int RETURN_SET = ALL_OPAQUES | (1 << REFERENCE_TYPE);
static int ALL_POINTERS = (1 << OPAQUE_TYPE) | (1 << MEMBER) | 0xeeeeeee0;
/** Set macros */

#define ONE_MEMBER(s) ((s).bits && ((((s).bits)&-((s).bits)) == ((s).bits)))

static small_set pointer_set(small_set s) {
   small_set result;
   result.bits = s.bits << 4;
   if (s.bits <= (1 << POINTER(OPAQUE_TYPE)))
       result.bits |= (1 << OPAQUE_TYPE);
    return result;
}

static small_set deref_set(small_set s) {
    small_set result;
    result.bits = s.bits >> 4;
    return result;
}

static small_set make_set(int t) {
    small_set result;
    result.bits = 1 << t;
    if (result.bits & OPAQUE_POINTERS)
        result.bits |= (1 << OPAQUE_TYPE);
    return result;
}

#define is_empty(s) (s.bits == 0)   

static char *type_names[] = { "error", "struct", "opaque", "int", "member", "ref" };

char* type_name(int t) {
    assert(t >= 0);
    if (t > 5)
        return stringf("%s*", type_name(DEREF(t)));
    else 
        return type_names[t];
}

static char *long_type_names[] = { "", "", "Opaque type", "Integer", "Member-pointer", "Reference" };

char* long_type_name(int t) {
    assert (t > 1);
    if (t > 5)
        return stringf("Pointer-to-%s", long_type_name(DEREF(t)));
    else 
        return long_type_names[t];
}

static const int MultiplyDeBruijnBitPosition[32] = 
{
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

#define TRAILING_ZEROES(v) MultiplyDeBruijnBitPosition[(((v) & -(v)) * 0x077CB531UL) >> 27]

static int first_set_bit(small_set s) {
    if (s.bits)
        return TRAILING_ZEROES(s.bits);
    else
        return -1;
}

static int next_set_bit(small_set s, int i) {
    assert(i >= 0 && i <= 32);
    unsigned int x = (s.bits >> i)  & (~(-1 << (32-i)));
    if (x)
        return TRAILING_ZEROES(x) + i;
    else 
        return -1;
}

#define FOR_EACH(i, s) for(i = first_set_bit(s); i >= 0; i = next_set_bit((s), i+1))

void print_type_set(small_set s) {
    int i;
    print("{ ");
    FOR_EACH(i, s) 
        print("%s ", type_name(i));
    print("}");
}

static int is_null(Node p) {
    int gen = generic(p->op);
    if (gen != CNST)
        return 0;
    switch(optype(p->op)) {
    case I: case U:
        if (p->syms[0]->u.c.v.i == 0) {
            return 1;
        } 
    }
    return 0;
}

int legal_bits_for_node(Node n) {
    switch(optype(n->op)) {
    case I: case U:
        if (is_null(n)) 
            return NULL_SET.bits;
        else
            return (1 << OPAQUE_TYPE) | (1 << INTEGER_TYPE);        
    case F:
        return 1 << OPAQUE_TYPE;
    case P:
        return ALL_POINTERS;
    case B:
        n->x.error = stringf("Cannot handle structs as values, use pointers instead.\n");    
        n->x.exact_type = ERROR;
        return 0;
    case V: default: 
        assert(0);
    }
}

int disambiguate(small_set s, Node n) {
    if (ONE_MEMBER(s))
        return first_set_bit(s);
    s.bits &= legal_bits_for_node(n);
    if (is_empty(s)) {
        return ERROR;
    } else if (ONE_MEMBER(s)) {
        return first_set_bit(s);
    } else {
        int non_opaques = NON_OPAQUES & s.bits;
        if (non_opaques == 0) {
            return OPAQUE_TYPE;
        } else {
            return ERROR;
        }
    }
}

int type_match(int t, small_set options) {
    if ((1 << t) & options.bits)
        return t;
    if (((1 << t) & ALL_OPAQUES) && (options.bits & ALL_OPAQUES))  
        return OPAQUE_TYPE;
    return ERROR;
}

typedef small_set (*unary_func)(int i, Type t);

typedef small_set (*binary_func)(int i1, int i2, Type t);

char *c_op(int op) {
    switch(generic(op)) {
    case BCOM: 
        return "~";
    case NEG:
        return "-";
    case INDIR:
        return "*";
    case SUB:
        return "-";
	case ADD:
        return "+";
    case BOR: 
        return "|";
    case BAND: 
        return "&";
    case BXOR: 
        return "^";
    case RSH: 
        return ">>";
    case LSH:
        return "<<";
    case DIV: 
        return "/";
    case MUL: 
        return "*";
    case MOD:
        return "%";
 	case EQ: 
        return "==";
    case NE: 
        return "!=";
    case GT: 
        return ">";
    case GE: 
        return ">=";
    case LE: 
        return "<=";
    case LT:
        return ">";
    default:
        assert(0);
     }
}

char * c_expr1(int op, int t) {
    return stringf("%s %s", c_op(op), type_name(t));
}

char * c_expr2(int t1, int op, int t2) {
    if (generic(op) == ASGN) {
        if (t1 >= 4) {
            return stringf("%s = %s", type_name(DEREF(t1)), type_name(t2));
        } else {
            return stringf("&%s = %s", type_name(t1), type_name(t2));
        }
    } else {
        return stringf("%s %s %s", type_name(t1), c_op(op), type_name(t2));
    }
}

small_set bottom_up1(Node n, unary_func f) {
    Node kid = n->kids[0];
    Type t = n->x.ptype;
    small_set result = EMPTY_SET;
    assert(result.bits == 0);
    int i, or = 0;
    small_set s = kid->x.type_set;
    if (kid->x.error) {
        n->x.error = kid->x.error;
        kid->x.error = 0;
    }
    if (s.bits == 0)
        return EMPTY_SET;
    FOR_EACH(i, s) {
        result.bits |= f(i, t).bits;      
    }
    if (result.bits == 0) {
        n->x.error = "Cannot find a type for";
        FOR_EACH(i, s) {
           if (or)
               n->x.error = stringf("%s or '%s'", n->x.error, c_expr1(n->op, i)); 
           else
               n->x.error = stringf("%s '%s'", n->x.error, c_expr1(n->op, i));
           or = 1;        
        }
        n->x.error = stringf("%s\n", n->x.error);
    }
    return result;
}

small_set bottom_up2(Node n, binary_func f) {
    Node kid1 = n->kids[0];
    Node kid2 = n->kids[1];
    Type t = n->x.ptype;
    small_set result = EMPTY_SET;
    assert(result.bits == 0);
    int i, j, or = 0;
    small_set s1 = kid1->x.type_set;
    small_set s2 = kid2->x.type_set;
    if (kid1->x.error) {
        n->x.error = kid1->x.error;
        kid1->x.error = 0;
    }
    if (kid2->x.error) {
        n->x.error = kid2->x.error;
        kid2->x.error = 0;
    }
    if (s1.bits == 0 || s2.bits == 0)
        return EMPTY_SET;
    FOR_EACH(i, s1) {
        FOR_EACH(j, s2) {
            result.bits |= f(i, j, t).bits;
        }
    }
    if (result.bits == 0) {
        if (ONE_MEMBER(s1) && (s1.bits & (1 << MEMBER))) {
            n->x.error = "Illegal use of member-pointer";
        } else if (ONE_MEMBER(s2) && (s2.bits & (1 << MEMBER))) {
            n->x.error = "Illegal use of member-pointer";
        } else {
            n->x.error = "Cannot find a type for";
            FOR_EACH(i, s1) {
                FOR_EACH(j, s2) {
                    if (or)
                       n->x.error = stringf("%s or '%s'", n->x.error, c_expr2(i, n->op, j));
                    else
                       n->x.error = stringf("%s '%s'", n->x.error, c_expr2(i, n->op, j));
                   or = 1;
                }
            }
        }
        n->x.exact_type == ERROR; 
        n->x.error = stringf("%s\n", n->x.error);
    }
    return result;
}

void label_error(Node n, small_set possibles) {
//    fprintf(stderr, "label_error\n");
    n->x.exact_type = ERROR;
    if (n->x.error)
        return;
    small_set x = possibles;
    x.bits &= legal_bits_for_node(n);
    if (is_empty(x)) {
        small_set legals;
        legals.bits = legal_bits_for_node(n);
        if (!is_empty(possibles)) {
            print_type_set(possibles);
            print_type_set(legals);
        }
        n->x.error = stringf("No legal typing found for expression\n");
    } else {
        n->x.error = stringf("Ambiguous typing for expression. Insert temporary variable and simplify.\n", opname(n->op));
    }
}


void top_down1(Node n, int type, unary_func f) {
    Type t = n->x.ptype;
    Node kid = n->kids[0];
    if (type == ERROR) {
        kid->x.exact_type = ERROR;
    }
    small_set possibles = EMPTY_SET;
    int i;
    small_set s = kid->x.type_set;
    FOR_EACH(i, s) {
        small_set x = f(i, t);
        if (x.bits & make_set(type).bits) {
            possibles.bits |= (1 << i);
        }
    }
    kid->x.exact_type = disambiguate(possibles, kid);
    if (kid->x.exact_type == ERROR) {
        label_error(kid, possibles);
    }
}

void top_down2(Node n, int type, binary_func f) {
    Type t = n->x.ptype;
    Node kid1 = n->kids[0];
    Node kid2 = n->kids[1];
    if (type == ERROR) {
        kid1->x.exact_type = ERROR;
        kid2->x.exact_type = ERROR;       
    }
    small_set possibles1 = EMPTY_SET;
    small_set possibles2 = EMPTY_SET;
    int i, j;
    small_set s1 = kid1->x.type_set;
    small_set s2 = kid2->x.type_set;
    FOR_EACH(i, s1) {
        FOR_EACH(j, s2) {
            small_set x = f(i, j, t);
            if (x.bits & make_set(type).bits) {
                possibles1.bits |= (1 << i);
                possibles2.bits |= (1 << j);
            }
        }
    }
    kid1->x.exact_type = disambiguate(possibles1, kid1);
    if (kid1->x.exact_type == ERROR) {
        label_error(kid1, possibles1);
    }
    kid2->x.exact_type = disambiguate(possibles2, kid2);
    if (kid2->x.exact_type == ERROR) {
        label_error(kid2, possibles2);
    }
}

static small_set addition_sets[] = {     
    { 0 }, // ERROR
    { 0 }, // STRUCT (Cannot add to struct)
    { 1 << OPAQUE_TYPE }, // OPAQUE
    { 1 << INTEGER_TYPE }, // INTEGER
    { 1 << MEMBER }, // MEMBER
    { (1 << MEMBER) } // REFERENCE
};

static small_set subtraction_sets[] = {     
    { 0 }, // ERROR
    { 0 }, // STRUCT (Cannot subtract from struct)
    { 1 << OPAQUE_TYPE }, // OPAQUE
    { 1 << INTEGER_TYPE }, // INTEGER
    { 0 }, // MEMBER
    { 0 } // REFERENCE
};

static small_set add_int(int t) {
    if (t > REFERENCE_TYPE) {
        return make_set(t);
    } else {
        return addition_sets[t];
    }
}

static small_set equals(int t1, int t2, Type type) {
    if  (t1 == t2)
        return OPAQUE_SET;
    else
        return EMPTY_SET;
}

static small_set comp_func(int t1, int t2, Type t) {
    if (t1 == t2 || 
        (t1 == INTEGER_TYPE && t2 == OPAQUE_TYPE) ||
        (t1 == OPAQUE_TYPE && t2 == INTEGER_TYPE) ||
        (t1 >= POINTER(OPAQUE_TYPE) && t2 >= POINTER(OPAQUE_TYPE)))
        return OPAQUE_SET;
    else {
        return EMPTY_SET;
    }
}

static small_set add_func(int t1, int t2, Type type) {
    if (t1 == INTEGER_TYPE)
        return add_int(t2);
    if (t2 == INTEGER_TYPE)
        return add_int(t1);
    if (t1 == OPAQUE_TYPE && t2 == OPAQUE_TYPE)
        return OPAQUE_SET;
    return EMPTY_SET;
}

static small_set sub_func(int t1, int t2, Type type) {
    if ((t1 >= REFERENCE_TYPE) && (t1 == t2)) {
        return INTEGER_SET;
    }
    if (t2 == INTEGER_TYPE) {
        if (t1  > REFERENCE_TYPE) {
            return make_set(t1);
        } else {
           return subtraction_sets[t1];
        }
    }
    if (is_opaque(t1) && is_opaque(t2))
        return OPAQUE_SET;
    return EMPTY_SET;
}

static small_set indir(int t, Type type) {
    switch(t) {
    case MEMBER: case REFERENCE_TYPE:
        if (type) {
            return make_set(get_type(type));
        } else {
            return MEMBERS_SET;
        }
    case STRUCT_TYPE: case OPAQUE_TYPE: case INTEGER_TYPE:
        return EMPTY_SET;
    case ERROR:
        return EMPTY_SET;
    default:
        return make_set(t - 4);
    }
}

static small_set convert(int t, Type type) {
    small_set s;
    switch(t) {
    case MEMBER: 
        s.bits = 1 << MEMBER;
        return s;
    case REFERENCE_TYPE: case OPAQUE_TYPE: 
        return make_set(t);
    case INTEGER_TYPE:
        s.bits = 1 << OPAQUE_TYPE | 1 << INTEGER_TYPE;
        return s;
    case ERROR: case STRUCT_TYPE:
        return EMPTY_SET;
    default:
        return make_set(t);
    }
}

static small_set unary_op(int t, Type type) {
    if (t == INTEGER_TYPE)
        return INTEGER_SET;
    if (is_opaque(t)) {
        return OPAQUE_SET;
    }
    return EMPTY_SET;
}

static small_set binary_op(int t1, int t2, Type type) {
    if (t1 == INTEGER_TYPE && t2 == INTEGER_TYPE)
        return INTEGER_SET;
    if (is_opaque(t1) && is_opaque(t2))
        return OPAQUE_SET;
    return EMPTY_SET;
}
  
static void emit_store(Symbol s, char* type);
static void label_tree(Node root);
 
static void preamble(Symbol f, Symbol caller[], Symbol callee[], int ncalls);

static int makeTemp(Symbol p) {
	if (p->sclass == AUTO) {
		if (p->addressed || isunion(p->type)) {
			return 0;
		} else {
            p->sclass = REGISTER;
          	return 1;
		}
	} else if (p->sclass == REGISTER) {
		return 1;
	} else {
		return 0;	
	}
}
static int c_offset = 0;
static int r_offset = 0;
static int c_temp = 0;
static int r_temp = 0;
static int temps = 0;
int bytecodes = 0;

static char* local_names[100];
static int local_offsets[100];

char *local_name(Symbol p) { 
    int i;
    if (p->sclass == REGISTER) {
        return stringd(temps++);        
    } else {
        assert(p->x.type);
        for (i = 0; local_names[i]; i++)
            if (strcmp(local_names[i], p->name) == 0)
                return stringd(local_offsets[i]);
        if (p->x.type != REFERENCE_TYPE) {
            c_offset = roundup(c_offset, p->type->align);
            char * name = stringd(c_offset);
            c_offset += p->type->size;
            return name;
        } else {
            char * name = stringd(r_offset);
            r_offset += p->type->size;
            return name;
        }
    }
}
     
int get_type(Type t);
static Type local_struct = 0;

int is_local(char* name) {
    int i;
    for (i = 0; local_names[i]; i++)
        if (strcmp(local_names[i], name) == 0)
            return 1;
    return 0;
}

void gvmt_local(Symbol p) {  
    if (strcmp(p->name, "__gvmt_bytecode_locals") == 0) {
        int names = 0;
        int size = 0;
        Type t = p->type;
        local_struct = t;
        Field f;
        assert(bytecodes);
        assert(isstruct(t));
        f = t->u.sym->u.s.flist;
        while (f) {
            local_names[names] = f->name;
            size = roundup(size, f->type->align);
            local_offsets[names++] = size;
            size += f->type->size;
            f = f->link;
            assert(names <= 100);
        }
        local_names[names] = 0;
        return;   
    }
    if (p->temporary)
        p->x.type = 0;
    else
        p->x.type = get_type(p->type);
    makeTemp(p);
    p->x.name = local_name(p);
}

static int ret_type = 0;
static char *currentfile;

static char* return_type_ext(Type t) {
    int size;
    if (isptr(t))
        if (is_ref(t))
            return "R";
        else
            return "P";
    size = t->size > IR->ptrmetric.size ? t->size : IR->ptrmetric.size;
    if (t == voidtype)
        return "V";
    else if (isunsigned(t))
        return stringf("U%d", size);
    else if (isfloat(t))
        return stringf("F%d", size);
    else if (isint(t) || isenum(t))
        return stringf("I%d", size);
    else {
        fprint(stderr, "Unexpected type: %t\n", t);   
        assert(0);
    }
}

static int currentline;

void gvmt_function(Symbol f, Symbol caller[], Symbol callee[], int ncalls) {
	int i, size, varargs;
    char* shadows = 0;
    if (unqual(freturn(f->type)) == voidtype) {
        ret_type = 0;   
    } else {
        ret_type = get_type(freturn(f->type));
    }   
    c_offset = 0;
    r_offset = 0;
    temps = 0;
    for (i = 0; callee[i]; i++) {
        Symbol p = callee[i];
        Symbol q = caller[i];
        assert(q);
        if(is_local(p->name)) {
            currentline = p->src.y;
            gvmt_error("Instruction parameter '%s' shadows Interpreter local", p->name);
        }
        gvmt_local(p);
        q->sclass = p->sclass;
        q->x.name = p->x.name;
        q->x.type = p->x.type;
    }
	gencode(caller, callee);
	preamble(f, caller, callee, ncalls);
	emitcode();  
	if (!emit_dot_file) {
        if (bytecodes)
            print("DROP ");
        else
            print("RETURN_%s ", return_type_ext(freturn(f->type)));
        print(";\n");
    }
}

char * pointer_type(int t);
static int label_id = 0;

static char* c_declaration(Type t, char *name) {
    int i, count;
    if (isptr(t)) {
        t = deref(t);
        if (isfunc(t)) {
            count = 0;
            for (i = 0; t->u.f.proto[i]; i++)
                if (opaque_type(t->u.f.proto[i]))
                    count++;
            return stringf("gvmt_funcptr_%d %s", count, name);
        } else {
            return c_declaration(t, stringf("*%s", name));
        }
    } else {
        if (isunion(t)) {
            return stringf("union %s %s", t->u.sym->name, name);
        } else if (isstruct(t)) { 
            return stringf("struct %s %s", t->u.sym->name, name);
        } else if (isarray(t)) { 
            return stringf("%s[]", c_declaration(t->type, name));
        } else {
            char * tname;
            if (t->op == INT)
                tname = stringf("int%d_t", t->size * 8);
            else if (t->op == UNSIGNED)
                tname = stringf("int%d_t", t->size * 8);
            else if (t->op == FLOAT)
                tname = t->u.sym->name;
            else {
                fprint(stderr, "%t %s\n", t, name);   
                assert(0);
                
            }
            return stringf("%s %s", tname, name);
        }
    }
}

static int is_native(Type t) {
    if (!opaque_type(freturn(t)))
        return 0;
    if (!hasproto(t))
        return 1;
    Type* p;
    for (p = t->u.f.proto; *p; p++) 
        if (!opaque_type(*p))
            return 0;
    return 1;
}

static void preamble(Symbol f, Symbol caller[], Symbol callee[], int ncalls) {
    int pcount = 0;
    int i = 0;
    Symbol *p;
    if (emit_dot_file)
        print("//");
    else 
        print("\n");
    if (is_native(f->type) && !bytecodes) {
        currentline = f->src.y;
        gvmt_error("%s is a native function, compile using native compiler\n", f->name);
        exit(1);
    }\
    if (strncmp("gvmt_lcc_", f->x.name, 9) == 0)
        print("%s", f->x.name+9);
    else
    	print("%s", f->x.name);
    if (f->sclass == STATIC) 
        print(" [private] ");
    print(": ");
    if (c_offset) {
        c_temp = temps++;
        print("%d ALLOCA_I1 ", c_offset);
        print("NAME(%d,\"__lcc_c_locals\") ", c_temp);
        print("TSTORE_P(%d) ", c_temp);
        assert(c_offset > 0);
    }
    if (r_offset) {
        if (bytecodes) {
            currentline = f->src.y;
            gvmt_error("Cannot use temporary arrays of references in interpreter bytecodes\n", f->name);
            exit(1);
        }
        r_temp = temps++;
        assert(r_offset  > 0);
        print("NAME(%d,\"__lcc_r_locals\") ", r_temp);
        print("%d ALLOCA_R ", r_offset / IR->ptrmetric.size);
        print("TSTORE_P(%d) ", r_temp);
    }
    for (p = callee; *p; p++) {
        int t = get_type((*p)->type);
        if (is_opaque(t)) {
            char* tc;
            if (isunsigned((*p)->type))
                tc = stringf("U%d", ((*p)->type)->size);
            else if (isfloat((*p)->type))
                tc = stringf("F%d", ((*p)->type)->size);
            else if (isptr((*p)->type))
                tc = "P";
            else
                tc = stringf("I%d", ((*p)->type)->size);
            emit_store(*p, tc);
        } else {
            emit_store(*p, pointer_type(t));
        }
    }
    print("FILE(\"%s\") ", currentfile);
}

static void gvmt_defconst(int suffix, int size, Value v) {
    if (!emit_dot_file) {
        char buf[40];
        switch(suffix) {
        case P:
           if (v.i)
                gvmt_error("Cannot use integral value as pointer\n");
            // Intentional fall through
        case I: case U:
            print("int%d %d\n", size*8, v.i);
            break;
        case F:
            if (size == 8) {
                sprintf(buf, "%.20e", (double)v.d);
                print("float64 %s\n", buf);
            } else {
                sprintf(buf, "%.10e", (float)v.d);
                print("float32 %s\n", buf);
            }
            break;
        }
    }
}

void gvmt_defaddress(Symbol p) {
    if (!emit_dot_file)
        print("address %s\n", p->x.name);
}

static void gvmt_defstring(int n, char *str) {
    if (!emit_dot_file) {
        int i;
        print("string \"");
        for (i = 0; i < n; i++) {
            char c = str[i];
            if (c == '\n')
                print("\\n");
            else if (c == '\t')
                print("\\t");
            else if (c == '\"')
                print("\\\"");
            else if (c == '\'')
                print("\\\'");
            else if (c == '\\')
                print("\\\\");
            else if (((unsigned char)c) > 126)
                print("\\%o", (unsigned char)c);
            else if (((unsigned char)c) < 8)
                print("\\00%o", (unsigned char)c);
            else if (((unsigned char)c) < 32)
                print("\\0%o", (unsigned char)c);
            else
                putc(c, stdout);
        }
        print("\"\n", stdout);
    }
}

static void gvmt_export(Symbol p) {
    p->x.export = 1;
}

int opaque_type(Type t) {
    t = unqual(t);
    if (isptr(t)) {
        return opaque_type(deref(t));
    } else if (isarray(t)) {
        return opaque_type(t->type);
    } else if (isint(t) || isenum(t)) {
        return 1;
    } else if (isunion(t)) {
        return 0;
    } else if (isstruct(t)) {
        return opaque_struct(t);
    } else if (isfunc(t) || isfloat(t) || t == voidtype) {
        return 1;
    } else {
        print("Illegal type %t\n", t);
        assert(0);
        return 0;
    }
}  

print_locals(Type t) {
    Field f;
    int has_refs = 0;
    assert(bytecodes);
    assert(isstruct(t));
    f = t->u.sym->u.s.flist;
    while (f) {
        print(".local");
        if (is_ref(f->type)) {
            print(" object ");
        } else if (isptr(f->type)) {
            print(" pointer ");
        } else if (f->type->op == INT || f->type->op == UNSIGNED) {
            print(" int%d ", f->type->size * 8);
        } else if (f->type->op == FLOAT) {
            if (f->type->size == 8)
                print(" double ");
            else
                print(" float ");
        } else {
            fprint(stderr, "%t %s\n", f->type, f->name);   
            assert(0);
        }
        print("%s\n", f->name);   
        f = f->link;
    }
    return;   
}

static void gvmt_import(Symbol p) {
}

void gvmt_defsymbol(Symbol p) {
	if (p->scope == LABELS) {
		p->x.name = stringf("%d", label_id++ );
        p->x.type = OPAQUE_TYPE;
	} else if (p->temporary) {
		p->x.name = stringf("t%s", p->name);
        p->x.type = 0;
	} else if (p->generated) {
		p->x.name = stringf("x%s", p->name);
        p->x.type = OPAQUE_TYPE;
	} else {
		p->x.name = stringf("%s", p->name);
        if (p->type) {
            p->x.type = get_type(p->type);
        } else {
            p->x.type = 0;
        }
    }
}
static int segment = 0;
static int in_object = 0;

void gvmt_global(Symbol p) {
    if (!emit_dot_file) {
        Type t = p->type;
        type_info(t);
        print ("\n");
        int new_segment;
        char* segment_title;
        if (isstruct(t) && !opaque_struct(t)) {
            new_segment = LIT;  
            segment_title = ".heap\n"; //  ...
        } else if (is_ref(t) || isarray(t) && is_ref(t->type)) {
            new_segment = ROOTS;
            segment_title = ".roots\n";
        } else {
            new_segment = DATA;
            segment_title = ".opaque\n";
        }
        if (new_segment != segment) {
            if (in_object) {
                in_object = 0;
                print(".end\n");
            }
            segment == new_segment;
            print(segment_title);
        }
        if (p->x.export)
            print (".public %s\n", p->x.name);
        if (isstruct(t) && !opaque_struct(t)) {
            print (".object %s\n", p->x.name);   
            in_object = 1;
        } else {
            print (".label %s\n", p->x.name);
        }
    }
}

static void gvmt_segment(int n) {
    if (!emit_dot_file) {
        switch (n) {
        case CODE:
            if (in_object) {
                in_object = 0;
                print(".end\n");
            }
            segment = CODE;
            if (bytecodes)
                print(".bytecodes\n");            
            else
                print(".code\n");  
            break;
        case LIT: case DATA: case BSS: 
            segment = 0;
            break;
        }
    }
}

static small_set typeset_from_type(Type t) {
    small_set result;
    int i = get_type(t);
    result.bits = 1 << i;
    return result;
}
         
static small_set typeset_from_symbol(Symbol s, char** msg) {
    small_set result;
    if (s->temporary && s->u.t.cse) {
        result = s->u.t.cse->x.type_set;
        if (result.bits >= 1) {
            *msg = s->u.t.cse->x.error;
        }
        return result;
    } else {
        if (s->x.type == 0) {
            if (s->type) {
                s->x.type = get_type(s->type);
                assert(valid_type(s->x.type) || s->x.type == ERROR);
            } else {
                assert(0);
            }
        }
        assert(valid_type(s->x.type) || s->x.type == ERROR);
        result.bits = 1 << s->x.type;
//        if (is_opaque(s->x.type) && s->x.type > REFERENCE_TYPE)
//            result.bits |= 1 << OPAQUE_TYPE;
        return result;
    }
}

static void gvmt_space(int n) {
    while (n > 0) {
        n -= IR->ptrmetric.size;
        print("int32 0\n");
    }
}

void gvmt_stabinit(char *, int, char *[]);
void gvmt_stabline(Coordinate *);
void gvmt_stabsym(Symbol);

/* stabinit - initialize stab output */
void gvmt_stabinit(char *file, int argc, char *argv[]) {
    if (file) {
        currentfile = file;
    }
    currentline = 1;
}

/* stabline - emit stab entry for source coordinate *cp */
void gvmt_stabline(Coordinate *cp) {
    if (emit_dot_file)
        print("//");
    if (cp->file && cp->file != currentfile) {
            print("FILE(\"%s\") ", cp->file);
            currentfile = cp->file;
    }
    currentline = cp->y;
    print("LINE(%d) \n", cp->y);
}

/* stabsym - output a stab entry for symbol p */
void gvmt_stabsym(Symbol p) {
}
 
static void gvmt_progbeg(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-xbytecodes") == 0) {
            RETURN_SET = ARGS_SET;
            bytecodes = 1;
        } else if (strcmp(argv[i], "-xgcsafe") == 0) {
            insert_gc_safe_point = 1;
        } else if (strcmp(argv[i], "-xdot") == 0) {
            emit_dot_file = 1;
            print("digraph g {\n");
        }
    }
}

static void gvmt_blockbeg(Env* env) {
}

static void gvmt_blockend(Env* env) {
}

static void gvmt_progend(void) {
    if (emit_dot_file) {
        print("}\n");
    } else {
        if (bytecodes && local_struct) {
            print(".bytecodes\n");
            print_locals(local_struct);
        }
        print("\n");
        print_type_info();
    }
}

static int string_eq(char* c1, char* c2) {
    return strcmp(c1, c2) ? 1000 : 0;
}

void gvmt_error(const char *fmt, ...) {
	va_list ap;
    va_start(ap, fmt);
    fprint(stderr, "%s:%d: error: ", file, currentline);
    vfprint(stderr, NULL, fmt, ap);
	va_end(ap);
    errcnt++;
}

void gvmt_warning(const char *fmt, ...) {
	va_list ap;
    va_start(ap, fmt);
    fprint(stderr, "%s:%d: warning: ", file, currentline);
    vfprint(stderr, NULL, fmt, ap);
	va_end(ap);
}

char * pointer_type(int t) {
    if (t == ERROR) {
       return "E";
    }
    assert(valid_type(t));
    switch(t) {
    case ERROR:
        return "E";
    case STRUCT_TYPE: 
        return "?";
    case MEMBER:
        return "X";
    case REFERENCE_TYPE:
        return "R";
    default:
        return "P";
    }
}

char* type_char(Node p) {
    switch(optype(p->op)) {
    case I: 
        return stringf("I%d", opsize(p->op));
    case U:
        return stringf("U%d", opsize(p->op));
    case F:
        return stringf("F%d", opsize(p->op));
    case P:
        assert(p->kids[0]);
        return pointer_type(p->x.exact_type);
    default: 
        assert(0);
    }  
}

int get_type(Type t) {
    int result;
    t = unqual(t);
    if (isptr(t)) {
        t = unqual(deref(t));
        if (t->op == VOID) {
            result = make_pointer(OPAQUE_TYPE);
        } else {
            int x = get_type(t);
            assert(valid_type(x));
            assert(x != MEMBER && x != ERROR);
            result = make_pointer(x);
        }
    } else if (isarray(t)) {
        result = get_type(t->type);
    } else if (isint(t) || isenum(t)) {
        result = INTEGER_TYPE;
    } else if (isunion(t)) {
        if (opaque_struct(t))
            result = OPAQUE_TYPE;
        else
            result = STRUCT_TYPE;
    } else if (isstruct(t)) {
        if (opaque_struct(t))
            result = OPAQUE_TYPE;
        else
            result = STRUCT_TYPE;
    } else if (isfunc(t) || isfloat(t)) {
        result = OPAQUE_TYPE;
    } else {
        fprint(stderr, "Illegal type %t\n", t);
        assert(0);
        result = 0;
    }
    return result;
}  

void legal_arg(Node p) {
    int t = disambiguate(p->x.type_set, p);
    if ((1 << t) & ARGS_SET) {
        p->x.exact_type = t;
    } else if (t == ERROR) {
        label_error(p, p->x.type_set);
    } else {
        p->x.error = stringf("Illegal type for parameter: %s\n", long_type_name(t));    
        p->x.exact_type = ERROR;
    } 
}

void legal_return(Node p) {
    int t = type_match(ret_type, p->x.type_set);
    if ((1 << t) & RETURN_SET) {
        p->x.exact_type = t;
    } else if (t == ERROR) {
        label_error(p, p->x.type_set);
    } else {
        p->x.error = stringf("Illegal return type: %s\n", long_type_name(t));    
        p->x.exact_type = ERROR;
    }
}

static small_set check_assign(int t1, int t2, Type t) {
    small_set s;
    if (t1 == MEMBER) {
        if (is_opaque(t2) || t2 == REFERENCE_TYPE || t2 >= POINTER(OPAQUE_TYPE)) {
            s.bits = 1 << t2;
            return s;
        } else {
            return EMPTY_SET;
        }
    }
    s = indir(t1, t);
    s.bits &= (1 << t2);
    if (s.bits)
        return s;
    if (is_opaque(t1) && is_opaque(t2))
        return OPAQUE_SET;
    return EMPTY_SET;
}

static Node clean(Node p) {
    if (p == NULL)
        return NULL;
    Node r = newnode(p->op, clean(p->kids[0]), clean(p->kids[1]), p->syms[0]);
    r->x.exact_type = p->x.exact_type;
    r->x.type_set = p->x.type_set;
    r->x.error = 0;
    r->x.ptype = p->x.ptype;
    return r;
}

static Node undoCSE(Node p) {
    if (p == NULL)
        return NULL;
    if (generic(p->op) == INDIR && is_cse(p->kids[0])) {
        return clean(p->kids[0]->syms[0]->x.cse);
    }
    p->kids[0] = undoCSE(p->kids[0]);
    p->kids[1] = undoCSE(p->kids[1]);
    return p; 
}

static void top_down_node(Node p) {
    int gen = generic(p->op);
    small_set type_set;
	switch (gen) {
	case ASGN:
        p->x.exact_type = disambiguate(p->x.type_set, p);
        if (generic(p->kids[0]->op) == ADDRL && p->kids[0]->syms[0]->temporary) {
            Node cse = p->kids[0]->syms[0]->u.t.cse;
            if (cse) {
                if ((generic(cse->op) == INDIR && generic(cse->kids[0]->op) == ADDRL) ||
                    (generic(cse->op) == INDIR && generic(cse->kids[0]->op) == ADDRG) ||
                    (generic(cse->op) == CNST) ||
                    (generic(cse->op) == ADDRL) ||
                    (generic(cse->op) == ADDRG) ||
                    p->x.exact_type == MEMBER ||
                    p->x.exact_type == ERROR) {
                    p->kids[0]->syms[0]->x.cse = undoCSE(cse);
                    p->x.eliminated = 1;
                    break;
                }
            }
            top_down2(p, p->x.exact_type, check_assign);
        } else if (p->x.exact_type == MEMBER) {
            p->x.exact_type = ERROR;
            p->x.error = "Cannot store an member-pointer\n";
        } else if (p->x.exact_type == ERROR) {
            label_error(p, p->x.type_set);
        } else {
            top_down2(p, p->x.exact_type, check_assign);
        }
        break;
	case CNST: 
    case ADDRG: case ADDRF: case ADDRL:
        break;
	case CVF: case CVI: case CVP: case CVU:
        top_down1(p, p->x.exact_type, convert);
		break;
	case ARG: 
        if (p->x.ptype) {
            int t = get_type(p->x.ptype);
            t = type_match(t, p->x.type_set);
            p->x.exact_type = t;
            if (t != ERROR) {
                p->kids[0]->x.exact_type = t;
            } else {
                t = disambiguate(p->x.type_set, p);
                if (t == MEMBER)
                    p->x.error = "Illegal use of member-pointer\n";
                else
                    p->x.error = "Actual parameter type does not match declared type\n";
            }
        } else {
            legal_arg(p);
            p->kids[0]->x.exact_type = p->x.exact_type;
        }
        break;
    case BCOM: case NEG:
        assert(p->kids[0]);
		assert(!p->kids[1]);
        top_down1(p, p->x.exact_type, unary_op);
        break;
    case INDIR:
        top_down1(p, p->x.exact_type, indir);
		break;
	case CALL:
		assert(p->kids[0]);
		assert(!p->kids[1]);
        p->kids[0]->x.exact_type = OPAQUE_TYPE;
        if (freturn(p->syms[0]->type) != voidtype)
            p->x.exact_type = get_type(freturn(p->syms[0]->type));
		break;
    case SUB:
        top_down2(p, p->x.exact_type, sub_func);
        break;
	case ADD:
        top_down2(p, p->x.exact_type, add_func);
        break;
    case BOR: case BAND: case BXOR: case RSH: case LSH:
    case DIV: case MUL: case MOD:
        top_down2(p, p->x.exact_type, binary_op);
        break;
 	case EQ: case NE:
		//Special case for comparison to 0/NULL.
		if (is_null(p->kids[0]))
		     p->kids[0]->x.type_set = NULL_SET;   
		if (is_null(p->kids[1]))
		     p->kids[1]->x.type_set = NULL_SET;
    case GT: case GE: case LE: case LT:
		p->x.exact_type = OPAQUE_TYPE;
        top_down2(p, p->x.exact_type, comp_func);
        break;
    case JUMP: 
        break;
    case LABEL:
        assert(p->x.exact_type);
        break;
    case RET:
        legal_return(p);
        p->kids[0]->x.exact_type = p->x.exact_type;
        break;
    default:
        gvmt_error("Unknown node: %d\n", gen);
	}
}

int is_ref(Type t) {
    if (!isptr(t)) 
        return 0;
    t = deref(t);
    return isstruct(t) && !opaque_struct(t);
}

static void print_ref_name(char* name, Type t) {
    char* tname;
    assert(is_ref(t));
    t = deref(t); 
    tname = t->u.sym->name;
    if (strcmp(tname, "gvmt_reference_types")) {
        print("TYPE_NAME(%s,\"", name);
        print("%S", t->u.sym->name + 12, strlen(t->u.sym->name)-12);
        print("\") ");
    }
}


int is_cse(Node p) {
    if (generic(p->op) == ADDRL) {
        Symbol s = p->syms[0];
        return s->x.cse != 0;
    } else {
        return 0;
    }
}

Type ref_add(Type ref, int offset) {
    Field f = deref(ref)->u.sym->u.s.flist;
    while (f) {
        if (f->offset == offset) {
            return f->type;
        }
        f = f->link;
    }
    return 0;
}

static void symbolise_node(Node p) {
    int gen = generic(p->op);
    int type;
    Type t;
	switch (gen) {
	case ASGN:
        if (generic(p->kids[0]->op) == ADDRL && p->kids[0]->syms[0]->temporary) {
            t = p->kids[1]->x.ptype;
            if (t) {
                p->x.ptype = t;
                p->kids[0]->syms[0]->x.ptype = t;
                p->kids[0]->x.ptype = ptr(t);
            }
        } 
        break;
	case CNST: 
        break;
    case ADDRG: case ADDRF: case ADDRL:
        if (p->syms[0]->x.ptype)
            p->x.ptype = ptr(p->syms[0]->x.ptype);
        else if (p->syms[0]->type)
            p->x.ptype = ptr(p->syms[0]->type);
        break;
	case CVF: case CVI: case CVP: case CVU:
        p->x.ptype = p->kids[0]->x.ptype;
		break;
	case ARG: 
        break;
    case BCOM: case NEG:
        p->x.ptype = p->kids[0]->x.ptype;
        break;
    case INDIR:
        if (is_cse(p->kids[0])) {
            assert(p->kids[0]->syms[0]->x.cse);
            p->x.ptype = p->kids[0]->syms[0]->x.cse->x.ptype;
        } else {
            t = p->kids[0]->x.ptype;
            if (t) {
                if (is_ref(t)) {
                    p->x.ptype = ref_add(t, 0);
                } else if (isptr(t) && t != voidptype) {
                    p->x.ptype = deref(t);
                }
            }
        }
		break;
	case CALL:
        p->x.ptype = freturn(p->syms[0]->type);
		break;
    case SUB:
        p->x.ptype = p->kids[0]->x.ptype;
        break;
	case ADD:
        t = p->kids[0]->x.ptype;
        if (t && (specific(p->kids[1]->op) == CNST + I || specific(p->kids[1]->op) == CNST + U)) {
            if (is_ref(t)) {
                t = ref_add(t, p->kids[1]->syms[0]->u.c.v.i);
                if (t)
                    p->x.ptype = ptr(t);
            } else if (isptr(t)) {
                p->x.ptype = t;
            }
        }
        break;
    case BOR: case BAND: case BXOR: case RSH: case LSH:
    case DIV: case MUL: case MOD:
        p->x.ptype = p->kids[0]->x.ptype;
        break;
 	case EQ: case NE: case GT: case GE: case LE: case LT:
		break;
    case JUMP: case LABEL:
        break;
     case RET:   
        break;
    default:
        gvmt_error("Unknown node: %d\n", gen);
	}
}

static void bottom_up_node(Node p) {
    int gen = generic(p->op);
    int type;
    switch (gen) {
	case ASGN:
		assert(p->kids[0]);
		assert(p->kids[1]);
        p->x.type_set = bottom_up2(p, check_assign);
        break;
	case CNST: 
        switch(optype(p->op)) {
        case I: case U:
//            if (p->syms[0]->u.c.v.i == 0) {
//                p->x.type_set = MEMBERS_SET;
//            } else {
//                p->x.type_set = INTEGER_SET;
//            }
            p->x.type_set = INTEGER_SET;
            break;
        case P:
            p->x.type_set = typeset_from_symbol(p->syms[0], &p->x.error);
            break;
        default:
            p->x.type_set = OPAQUE_SET;
        }
        break;
    case ADDRG: 
        if (strcmp(p->syms[0]->name, "NULL") == 0) {
            p->x.type_set.bits = 1 << REFERENCE_TYPE | 1 << POINTER(OPAQUE_TYPE);
            break;
        }
        // Intentional fall through
    case ADDRF: case ADDRL:
		assert(!p->kids[0]);
		assert(!p->kids[1]);
        if (p->x.ptype) 
            p->x.type_set = typeset_from_type(p->x.ptype);
        else
            p->x.type_set = pointer_set(typeset_from_symbol(p->syms[0], &p->x.error));
        break;
	case CVF: case CVI: case CVP: case CVU:
		assert(p->kids[0]);
		assert(!p->kids[1]);
        p->x.type_set = bottom_up1(p, convert);
		break;
	case ARG: 
        p->x.type_set = p->kids[0]->x.type_set;
        p->x.error = p->kids[0]->x.error;
        break;
    case BCOM: case NEG:
        assert(p->kids[0]);
		assert(!p->kids[1]);
        p->x.type_set = bottom_up1(p, unary_op);
        break;
    case INDIR:
		assert(p->kids[0]);
		assert(!p->kids[1]);
        p->x.type_set = bottom_up1(p, indir);
		break;
	case CALL:
		assert(p->kids[0]);
		assert(!p->kids[1]);
        if (freturn(p->syms[0]->type) != voidtype)
            p->x.type_set = typeset_from_type(freturn(p->syms[0]->type));
		break;
    case SUB:
        p->x.type_set = bottom_up2(p, sub_func);
        break;
	case ADD:
        if (p->kids[0]->x.type_set.bits == (1 << INTEGER_TYPE) && 
            generic(p->kids[1]->op) == ADDRG && 
            p->kids[1]->x.type_set.bits == (1 << REFERENCE_TYPE)) {
            p->x.type_set.bits = (1 << REFERENCE_TYPE);
        } else if (p->kids[1]->x.type_set.bits == (1 << INTEGER_TYPE) && 
            generic(p->kids[0]->op) == ADDRG && 
            p->kids[0]->x.type_set.bits == (1 << REFERENCE_TYPE)) {
            p->x.type_set.bits = (1 << REFERENCE_TYPE);
        } else {
            p->x.type_set = bottom_up2(p, add_func);
        }
        break;
    case BOR: case BAND: case BXOR: case RSH: case LSH:
    case DIV: case MUL: case MOD:
        p->x.type_set = bottom_up2(p, binary_op);
        break;
 	case EQ: case NE: case GT: case GE: case LE: case LT:
		assert(p->kids[0]);
		assert(p->kids[1]);
        p->x.type_set = OPAQUE_SET;
        p->x.exact_type = OPAQUE_TYPE;
		break;
    case JUMP: case LABEL:
        p->x.type_set = OPAQUE_SET;
        p->x.exact_type = OPAQUE_TYPE;
        break;
     case RET:   
        p->x.type_set = p->kids[0]->x.type_set;
        break;
    default:
        gvmt_error("Unknown node: %d\n", gen);
	}
}

static void bottom_up_tree(Node root) {
    if (root) {
        bottom_up_tree(root->kids[1]);
        bottom_up_tree(root->kids[0]);
        bottom_up_node(root);
    }
}

static void symbolise_tree(Node root) {
    if (root) {
        symbolise_tree(root->kids[1]);
        symbolise_tree(root->kids[0]);
        symbolise_node(root);
    }
}

static void top_down_tree(Node root) {
    if (root) {
        top_down_node(root);
        top_down_tree(root->kids[0]);
        top_down_tree(root->kids[1]);
    }
}

static int is_temp(Symbol s) {
    return s->sclass == REGISTER;   
}

static void emit_tree(Node p);
static void emit_subtree(Node p);

static void local_address(Symbol s) {
    if (is_local(s->name)) {
        print("LADDR(%s) ", s->name);
    } else {
        char* add;
        if (strcmp(s->x.name, "0"))
            add = stringf("%s ADD_P ", s->x.name);
        else
            add = "";
        if (s->x.type == REFERENCE_TYPE)
            print("TLOAD_P(%d) %s", r_temp, add);
        else
            print("TLOAD_P(%d) %s", c_temp, add);
    }
}

static void emit_store(Symbol s, char* type) {
    char prefix;
    if (is_temp(s)) {
        if (!s->generated && !s->x.named) {
            print ("NAME(%s,\"%s\") ", s->x.name, s->name);
            if (is_ref(s->type))
                print_ref_name(s->x.name, s->type);
            s->x.named = 1;
            type_info(s->type);
        }
        if (type[1] == '1' || type[1] == '2') {
            print("TSTORE_%c4(%s) ", type[0], s->x.name);
        } else { 
            print("TSTORE_%s(%s) " , type, s->x.name);
        }
    } else {   
        local_address(s);
        print("PSTORE_%s ", type);
    }
}

void emit_r_recursive(Node p) {
    Node left = p->kids[0];
    Node right = p->kids[1];
    int temp;
    assert(p->x.exact_type == MEMBER);
    if (generic(p->op) != ADD && generic(p->op) != SUB) {
        fprintf(stderr, "%s\n", opname(p->op));   
    }
    assert(generic(p->op) == ADD || generic(p->op) == SUB);
    if (left->x.exact_type == MEMBER) {
        emit_r_recursive(left);
        emit_subtree(right);
        print("ADD_%s ", type_char(right));
    } else if (left->x.exact_type == REFERENCE_TYPE) {
        emit_subtree(left);
        emit_subtree(right);
    } else if (right->x.exact_type == MEMBER) {
        temp = temps++;
        emit_subtree(left);
        print("TSTORE_%s(%d) ", type_char(left), temp);
        emit_r_recursive(right);        
        print("TLOAD_%s(%d) ", type_char(left), temp);
        print("ADD_%s ", type_char(left));
    } else if (right->x.exact_type == REFERENCE_TYPE) {
        temp = temps++;
        emit_subtree(left);
        print("TSTORE_%s(%d) ", type_char(left), temp);
        emit_subtree(right);
        print("TLOAD_%s(%d) ", type_char(left), temp);
    } else {
        print(" ********* ");
        return;
        gvmt_error("Unexpected types: %s and %s\n", type_name(left->x.exact_type), type_name(right->x.exact_type));
        fprintf(stderr, "Unexpected types: %s and %s\n", type_name(left->x.exact_type), type_name(right->x.exact_type));
        assert(0);   
    }
}

void emit_lhs(Node p, char* type) {
    Symbol s = p->syms[0];
    if (p->x.exact_type == ERROR && p->x.error) {
        gvmt_error("%s", p->x.error);
        return;
    }
    if (p->x.error) {
        gvmt_warning("%s", p->x.error);
    }
    switch(generic(p->op)) {
    case ADDRL: case ADDRF:
        emit_store(s, type);
        return;
    case ADDRG:
        if (strcmp(p->syms[0]->name, "NULL_P") == 0 ||
            strcmp(p->syms[0]->name, "NULL_R") == 0) {
            gvmt_error("Cannot assign to NULL\n");
        }
    }
    if (p->x.exact_type == MEMBER) {
        emit_r_recursive(p);
        print("RSTORE_%s ", type);
    } else if (p->x.exact_type == REFERENCE_TYPE) {
        emit_subtree(p);
        print("0 RSTORE_%s ", type);
    } else {  
        emit_subtree(p);
        print("PSTORE_%s ", type);
    }
}

static void emit_indir(Node p, char* type) {
    if (p->x.exact_type == ERROR && p->x.error) {
        gvmt_error("%s", p->x.error);
        return;
    }
    if (p->x.error) {
        gvmt_warning("%s", p->x.error);
    }
    if (p->x.exact_type == MEMBER) {
        emit_r_recursive(p);
        print("RLOAD_%s ", type);
        return;
    }   
    if (p->x.exact_type == REFERENCE_TYPE) {
        emit_subtree(p);
        print("0 RLOAD_%s ", type);
        return;
    }
    Symbol s = p->syms[0];
    switch(generic(p->op)) {
    case ADDRL: case ADDRF:
        assert(s->x.name);
        if (is_temp(s))  {
            if (!s->generated && !s->x.named) {
                print ("NAME(%s,\"%s\") ", s->x.name, s->name);
                if (is_ref(s->type))
                    print_ref_name(s->x.name, s->type);
                s->x.named = 1;
                type_info(s->type);
            }
            if (type[1] == '1' || type[1] == '2') {
                print("TLOAD_%c4(%s) ", type[0], p->syms[0]->x.name);
            } else { 
                print("TLOAD_%s(%s) ", type, p->syms[0]->x.name);
            }
        } else {
            emit_subtree(p);
            print("PLOAD_%s ", type);
        }
        break;
    case ADDRG:
        if (strcmp(p->syms[0]->name, "NULL_P") == 0 ||
            strcmp(p->syms[0]->name, "NULL_R") == 0) {
            print("0 ");
            break;
        }
        // Intentional fall through
    default:
        emit_subtree(p);
        print("PLOAD_%s ", type);
        break;
    }
}

#define set1248 ((1 << 1) | (1 << 2) | (1 << 4) | (1 << 8))
#define set48 ((1 << 4) | (1 << 8))

static char *f_letters = "0123F567D";
static char *i_letters = "0123I567L";

void gvmt_address(Symbol q, Symbol p, long n) {
    assert(!p->temporary);
    if (p->scope == GLOBAL || p->sclass == STATIC || p->sclass == EXTERN) {
        if (n)
            q->x.name = stringf("%s  %d ADD_%s", p->x.name, n, pointer_type(make_pointer(get_type(q->type))));
        else 
            q->x.name = p->x.name;
    } else {
		q->x.name = stringd(atoi(p->x.name) + n);
    }
    q->x.type = p->x.type;
}

static void print_conversion(Node p) {
    int to = opsize(p->op);
    int from = p->syms[0]->u.c.v.i;
    // Assert to and from are both in [1, 2, 4, 8]
    #define set1248 ((1 << 1) | (1 << 2) | (1 << 4) | (1 << 8))
    assert(((1 << to) & set1248) && ((1 << from) & set1248));
    switch(specific(p->op)) {
    case CVI+U: case CVU+U: case CVP+U: case CVU+P: 
    case CVU+I: case CVI+I:
        if (from == 4) {
            if (to < 4) {
                print("EXT_%s " , type_char(p));
            } else if (to == 8 && IR->ptrmetric.size == 4) {
                 if (optype(p->op) == I)
                    print("SIGN ");
                 else
                    print("ZERO ");
            }
        } else if (from == 8) {
            if (to == 4) {
                if (IR->ptrmetric.size == 4) {
                    print("L2I ");
                } else {
                    print("EXT_%s " , type_char(p));
                }
            } else {
                assert(to == 8);   
            }
        }
        break;
    case CVI+F: case CVU+F:
        assert(((1 << to) & set48) && ((1 << from) & set48));
        print("%c2%c ", i_letters[from], f_letters[to]);
        break;
    case CVF+F:
        assert(((1 << to) & set48) && ((1 << from) & set48));
        print("%c2%c ", f_letters[from], f_letters[to]);
        break;
    case CVF+I: case CVF+U:
        assert(((1 << to) & set48) && ((1 << from) & set48));
        print("%c2%c ", f_letters[from], i_letters[to]);
        break;
    default:
        assert(0);
    }
}

static void emit_comp(Node p) {
    int gen = generic(p->op);
    assert((1 << opsize(p->op)) & set48);
    char* name;
    switch(gen) {
    case EQ: 
        name = "EQ";
        break;
    case NE: 
        name = "NE";
        break;
    case LT: 
        name = "LT";
        break;
    case GT: 
        name = "GT";
        break;
    case LE: 
        name = "LE";
        break;
    case GE: 
        name = "GE";
        break;
    }
    print("%s_%s ", name, type_char(p));
    print("BRANCH_T(%s)\n", p->syms[0]->x.name);
    return;
}

static void emit_binary(Node p) {
    int gen = generic(p->op);
    assert((1 << opsize(p->op)) & set48);
    char* name;
    switch(gen) {
    case ADD:
        name = "ADD";
        break;
    case SUB:
        name = "SUB";
        break;
    case BOR: 
        name = "OR";
        break;
    case BAND: 
        name = "AND";
        break;
    case BXOR: 
        name = "XOR";
        break;
    case LSH: 
        name = "LSH";
        break;
    case RSH: 
        name = "RSH";
        break;
    case MUL: 
        name = "MUL";
        break;
    case DIV: 
        name = "DIV";
        break;
    case MOD: 
        name = "MOD";
        break;
    }
    print("%s_%s ", name, type_char(p));
    return;
}

static int arg_count = 0;

static void intrinsic(char* txt) {
    print(txt);
    n_args = N_ARGS;
    arg_count = 0;
}

static void emit_subtree(Node p) {
    int narg_count = 0;
    int gen = generic(p->op);  
    if (p->x.exact_type == ERROR && p->x.error) {
        gvmt_error("%s", p->x.error);
        return;
    }
    if (p->x.error) {
        gvmt_warning("%s", p->x.error);
    }
    switch(gen) {
    case ADD: case BOR: case BAND: case BXOR: case RSH: 
    case LSH: case SUB: case DIV: case MUL: case MOD:
        emit_subtree(p->kids[0]);
        emit_subtree(p->kids[1]);
        emit_binary(p);
        return;
    case CNST: 
        switch(optype(p->op)) {
        case U: case I:
            print("%d ", p->syms[0]->u.c.v.i);
            if (opsize(p->op) > sizeof(void*)) {
                gvmt_warning("Large (double word) integer may be truncated\n");
                if (optype(p->op) == I) 
                    print("SIGN ");
                else
                    print("ZERO ");
            }
            return;
        case P:
            print("%u ", p->syms[0]->u.c.v.i);
            return;
        default:
            assert(0);
            // ?
            return;
        }
    case INDIR:
        emit_indir(p->kids[0], type_char(p));
        return;
    case ADDRG:
        {
            Type t = p->syms[0]->type;
            if (isstruct(t) && !opaque_struct(t)) {  
                gvmt_error("Cannot use address of heap object %s in code\n", p->syms[0]->name);
            } else {
                // Could be a name, or a complex address like: name 8 ADD_P
                char buf[100];
                char* space;
                assert(strlen(p->syms[0]->x.name) < 92);
                sprintf(buf, "ADDR(%s  ", p->syms[0]->x.name);
                space = strchr(buf, ' ');
                *space = ')';
                print("%s", buf);
            }
        }
        break;
    case BCOM: 
        emit_subtree(p->kids[0]);
        print("INV_%s ", type_char(p));
        break;
    case NEG: 
        emit_subtree(p->kids[0]);
        print("NEG_%s ", type_char(p));
        break;
    case CVU: case CVI: case CVP: case CVF:
        emit_subtree(p->kids[0]);
        print_conversion(p);
        return;
    case CALL:
        if (generic(p->kids[0]->op) == ADDRG && 
            strncmp(p->kids[0]->syms[0]->x.name, "gvmt_", 5) == 0) {
            char* name = p->kids[0]->syms[0]->x.name + 5;
            if (strcmp(name, "insert") == 0) {
                intrinsic("#0 INSERT ");
                return;
            }
            if (strcmp(name, "stack_top") == 0) {
                intrinsic("STACK ");
                return;
            }
            if (strcmp(name, "malloc") == 0) {
                intrinsic("GC_MALLOC ");
                return;
            }
            if (strncmp(name, "push_", 5) == 0) {
                if (name[5] == 'x' && name[6] == 0) {
                    intrinsic("DROP ");
                    return;
                }
            }
            if (strcmp(name, "__tag__") == 0 || strcmp(name, "__untag__") == 0) {
                intrinsic("");
                return;
            }
            if (strncmp(name, "ireturn_", 8) == 0 && name[9] == 0) {
                if (name[8] == 'i') {
                    intrinsic("RETURN_I");
                    print("%d ", sizeof(void*));
                } else if (name[8] == 'p') {
                    intrinsic("RETURN_P ");
                } else if (name[5] == 'r') {
                    intrinsic("RETURN_R ");
                }
                return;
            }
            if (strcmp(name, "drop") == 0) {
                intrinsic("#0 DROP_N ");
                return;
            }
            if (strcmp(name, "transfer") == 0) {
                intrinsic("TRANSFER ");
                return;
            }
            if (strcmp(name, "raise") == 0) {
                intrinsic("RAISE ");
                return;
            }
            if (strcmp(name, "push_current_state") == 0) {
                intrinsic("PUSH_CURRENT_STATE ");
                return;
            } 
            if (strcmp(name, "discard_state") == 0) {
                intrinsic("DISCARD_STATE ");
                return;
            }
            if (strcmp(name, "alloca") == 0) {
                intrinsic("ALLOCA_I1 ");
                return;
            }
            if (strcmp(name, "ip") == 0) {
                intrinsic("IP ");
                return;
            }
            if (strcmp(name, "opcode") == 0) {
                intrinsic("OPCODE ");
                return;
            }
            if (strcmp(name, "next_ip") == 0) {
                intrinsic("NEXT_IP ");
                return;
            }
            if (strncmp(name, "fetch", 5) == 0) {
                if (name[5] =='\0') {
                    intrinsic("#@ ");
                    return;
                }
                if (name[5] =='2') {
                    intrinsic("#2@ ");
                    return;
                }
                if (name[5] =='4') {
                    intrinsic("#4@ ");
                    return;
                }
                assert(0);
            }
            if (strcmp(name, "gc_read_root") == 0) {
                intrinsic("PLOAD_R ");
                return;
            }
            if (strcmp(name, "gc_write_root") == 0) {
                intrinsic("PSTORE_R ");
                return;
            }
            if (strcmp(name, "gc_safe_point") == 0) {
                intrinsic("GC_SAFE ");
                return;
            }
            if (strcmp(name, "gc_suspend") == 0) {
                intrinsic("");
                requested_no_gc = 1;
                return;
            }
            if (strcmp(name, "gc_resume") == 0) {
                intrinsic("");
                requested_no_gc = 0;
                return;
            }
            if (strcmp(name, "far_jump") == 0) {
                intrinsic("FAR_JUMP ");
                return;
            }
            if (strcmp(name, "fully_initialized") == 0) {
                intrinsic("FULLY_INITIALIZED ");
                return;   
            }
            if (strcmp(name, "lock") == 0) {
                intrinsic("LOCK "); 
                return;
            }
            if (strcmp(name, "unlock") == 0) {
                intrinsic("UNLOCK "); 
                return;
            }
            if (strcmp(name, "lock_internal") == 0) {
                intrinsic("LOCK_INTERNAL "); 
                return;
            }
            if (strcmp(name, "unlock_internal") == 0) {
                intrinsic("UNLOCK_INTERNAL "); 
                return;
            }
        }
        if (is_native(p->syms[0]->type)) {
            narg_count = n_args - N_ARGS;
            while (n_args != N_ARGS) {
                n_args--;
                print ("NARG_%s ", *n_args);
            }
        }
        emit_subtree(p->kids[0]);
        if (is_native(p->syms[0]->type)) {
            char* inst;
            if (requested_no_gc)
                inst = "N_CALL_NO_GC";
            else
                inst = "N_CALL";
            print("%s_%s(%d) ", inst, 
                  return_type_ext(freturn(p->syms[0]->type)), narg_count);
        } else {
            if (variadic(p->syms[0]->type))
                print("#%d V_CALL_%s ", arg_count, 
                      return_type_ext(freturn(p->syms[0]->type)));
            else
                print("CALL_%s ",
                      return_type_ext(freturn(p->syms[0]->type)));
        }
        arg_count = 0;
        return;
    case ADDRL: case ADDRF:
        assert(p->syms[0]->sclass == AUTO);
        assert(p->syms[0]->type);
        local_address(p->syms[0]);
        return;    
    default:
        fprintf(stderr, opname(p->op));
        assert(0);
    }
}        

static void emit_link(int from, int to) {
    print ("node_%d -> node_%d\n", from, to);   
}

static int emit_dot_node(Node p) {
    static int next_node = 1;
    int node = next_node++;
    print("\"node_%d\" [\n", node);
    print("shape = \"record\"\n");
    print("label = \"%s |", opname(p->op));
    print(" %s | ", type_name(p->x.exact_type));
    print_type_set(p->x.type_set);
    print("\"\n]\n");
    return node;
}

static int emit_dot_tree(Node p) {
    int label = emit_dot_node(p);
    if (p->kids[0])
         emit_link(label, emit_dot_tree(p->kids[0]));
    if (p->kids[1])
         emit_link(label, emit_dot_tree(p->kids[1]));
    return label;
}
        
static void emit_tree(Node p) {
    int gen = generic(p->op);  
    if (p->x.eliminated)
        return;
    if (p->x.exact_type == ERROR && p->x.error) {
        gvmt_error("%s", p->x.error);
        return;
    }
    if (p->x.error) {
        gvmt_warning("%s", p->x.error);
    }
    char* name;
    switch(gen) {
    case ASGN:   
        emit_subtree(p->kids[1]);
        emit_lhs(p->kids[0], type_char(p));
		return;
    case EQ: case NE: case GT: case GE: case LE: case LT:
        emit_subtree(p->kids[0]);
        emit_subtree(p->kids[1]);
        emit_comp(p);
        return;
    case CALL: 
        emit_subtree(p);
        if (freturn(p->syms[0]->type) != voidtype) {
            if (freturn(p->syms[0]->type)->size > IR->ptrmetric.size)
                print("DROP ");
            print("DROP ");
        }
        return;
    case ARG:
        emit_subtree(p->kids[0]);
        if (p->x.native) {
            if (p->x.exact_type == REFERENCE_TYPE) {
                gvmt_error("Reference parameter for a native function\n");                
            } else {
                *n_args =  type_char(p);
                n_args++;
            }
        }
        if (p->syms[0]->u.c.v.i > IR->ptrmetric.size)
            arg_count+= 2;
        else
            arg_count++;
        return;
    case JUMP: 
        if (generic(p->kids[0]->op) ==  ADDRG) {
            if (insert_gc_safe_point && p->kids[0]->syms[0]->x.emitted
                && !requested_no_gc)
                print("GC_SAFE ");
            print("HOP(%s)\n", p->kids[0]->syms[0]->x.name);
        } else {
            gvmt_error("Cannot handle computed jumps\n");   
        }
        return;
    case LABEL: 
        print("TARGET(%s)\n", p->syms[0]->x.name);
        p->syms[0]->x.emitted = 1;
        return;
    case RET:
        if (p->kids[0])
            emit_subtree(p->kids[0]);
        // Leave value on stack...
        return;
    default:
        fprintf(stderr, opname(p->op));
        assert(0);
    }
}

void gvmt_emit(Node forest) {
    Node p;
    assert(forest);
    for (p = forest; p; p = p->link) {
        if (emit_dot_file) 
            emit_dot_tree(p);  
        else
            emit_tree(p);
    }
}

void label_args(Node arguments, Node call) {
    Type ftype = call->syms[0]->type;
    if (ftype->u.f.proto == 0)
        return;
//    fprint(stderr, "Function type is %t\n", ftype);
    int args = 0;
    Node p;
    int native = is_native(ftype);
    for (p = arguments; p; p = p->x.link) {
        args++;
        p->x.native = native;
    }
    int param_count;
    for (param_count = 0; ftype->u.f.proto[param_count]; param_count++);
    if (variadic(ftype))
        param_count--;
    if (args < param_count) {
        return;
    }
    int i = 0;
    for (p = arguments; i < param_count; p = p->x.link) {
        p->x.ptype = ftype->u.f.proto[i];
        i++;
    }
}

void link_arguments(Node forest) {
    Node p;
    Node arguments = 0;
    for (p = forest; p; p = p->link) {
        switch(generic(p->op)) {
        case ARG:
            p->x.link = arguments;
            arguments = p;
            break;
        case ASGN:
            if (generic(p->kids[1]->op) == CALL) {
                label_args(arguments, p->kids[1]);
                arguments = 0;
            }
            break;
        case CALL:
            label_args(arguments, p);
            arguments = 0;
        }
    }
}

int initial_temp_store(Node p) {
    return (generic(p->op) == ASGN) &&
           (generic(p->kids[0]->op) == ADDRL) &&
           (p->kids[0]->syms[0]->temporary) &&
           (p->kids[0]->x.type_set.bits == 0);
}

Node gvmt_label(Node forest) {
    Node p;
    assert(forest);
    link_arguments(forest);
    for (p = forest; p; p = p->link) {
        undoCSE(p);
        symbolise_tree(p);
        bottom_up_tree(p);
        top_down_tree(p);
    }
    return forest;
}

Interface gvmtIR = {
        { 1, 1, 0 },  /*  char_metrics */
        { 2, 2, 0 },  /*  short_metrics */
        { 4, 4, 0 },  /*  int_metrics */
        { 4, 4, 0 },  /*  long_metrics */
        { 8, 8, 0 },  /*  long_long_metrics */
        { 4, 4, 1 },  /*  float_metrics */
        { 8, 8, 1 },  /*  double_metrics */
        { 8, 8, 1 },  /*  long_double_metrics */
        { 4, 4, 0 },  /*  word_metrics */
        { 0, 4, 0 },  /*  struct_metrics */
        0,  /* little_endian */
        0,  /* mulops_calls */
        0,  /* wants_callb */
        0,  /* wants_argb */
        0,  /* left_to_right */
        0,  /* wants_dag */
        1,  /* unsigned_char */
        0, // gvmt_address,
        gvmt_blockbeg,
        gvmt_blockend,
        gvmt_defaddress,
        gvmt_defconst,
        gvmt_defstring,
        gvmt_defsymbol,
        gvmt_emit,
        gvmt_export,
        gvmt_function,
        gvmt_label,
        gvmt_global,
        gvmt_import,
        gvmt_local,
        gvmt_progbeg,
        gvmt_progend,
        gvmt_segment,
        gvmt_space,
        0,		/* gvmt_gen_stabblock */
        0,		/* gvmt_gen_stabend */
        0,		/* gvmt_gen_stabfend */
        gvmt_stabinit,
        gvmt_stabline,
        gvmt_stabsym,
        0,		/* gvmt_gen_stabtype */
};
