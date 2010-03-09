#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <gvmt/gvmt.h>
#include "opcodes.h"
#include "example.h"

void compile_to_do(GVMT_Object self, R_environment env, R_bytes b) {
    struct type *t = ((R_object)self)->type;
    printf("Compile for %s not implemented\n", t->name);
    assert(0 && "To do...");  
}

void compile_apply_to_do(GVMT_Object self, GVMT_Object params, R_environment env, R_bytes b) {
    struct type *t = ((R_object)self)->type;
    printf("Compile-apply for %s not implemented\n", t->name);
    assert(0 && "To do...");  
}
    
struct type type_object = {
    -1,
    0,
    0,
    "object",
    compile_object,
    compile_apply_to_do,
    print_object,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};
    
struct type type_boolean = {
    -1,
    0,
    0,
    "boolean",
    compile_literal,
    compile_apply_to_do,
    print_boolean,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};
    
struct type type_symbol = {
    -3,
    0,
    0,
    "symbol",
    compile_symbol,
    compile_apply_symbol,
    print_symbol,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};
    
struct type type_string = {
    -3,
    0,
    0,
    "string",
    compile_literal,
    compile_apply_to_do,
    print_string,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_cons = {
    -1,
    2,
    0,
    "cons",
    compile_cons,
    compile_apply_default,
    print_cons,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_environment = {
    -3,
    3,
    0,
    "environment",
    compile_to_do,
    compile_apply_to_do,
    print_error,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_function = {
    -5,
    2,
    0,
    "function",
    compile_function,
    compile_apply_function,
    print_function,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_bytes = {
    -4,
    0,
    0,
    "bytes",
    compile_to_do,
    compile_apply_to_do,
    print_error,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_vector = {
    -1, 
    0,
    0,
    "vector",
    compile_literal,
    compile_apply_default,
    print_vector,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_closure = {
    -1,
    2,
    0,
    "closure",
    compile_literal,
    compile_apply_default,
    print_closure,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_user_syntax = {
    -1,
    2,
    0,
    "defined syntax",
    compile_user_syntax_error,
    compile_apply_user_syntax,
    print_user_syntax, 
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_syntax = {
    -3,
    0,
    0,
    "syntax",
    compile_syntax_error,
    compile_apply_syntax,
    print_syntax, 
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_local_variable = {
    -2,
    1,
    0,
    "let binding",
    compile_local,
    compile_apply_default,
    print_error,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_global_variable = {
    -2,
    1,
    0,
    "global variable",
    compile_global,
    compile_apply_default,
    print_error,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_frame = {
    0,
    0,
    0,
    "frame",
    compile_to_do,
    compile_apply_to_do,
    print_error,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};

struct type type_undefined = {
    -1,
    1,
    0,
    "undefined",
    compile_literal,
    compile_apply_default,
    print_undefined_variable,
    error_negate,
    error_add,
    error_sub,
    error_mul,
    error_div,
    error_compare
};
    
struct type type_float = {
    -3,
    0,
    0,
    "float",
    compile_literal,
    compile_apply_default,
    print_float,
    float_negate,
    float_add,
    float_sub,
    float_mul,
    float_div,
    float_compare
};


GVMT_Object undefined_variable(R_symbol name) {
    R_undefined u = (R_undefined)gvmt_malloc(sizeof(GVMT_OBJECT(undefined)));
    struct type *t = &type_undefined;
    u->type = t;
    u->name = name;
    return (GVMT_Object)u;
}

GVMT_OBJECT(object) _true = {
    &type_boolean
};

GVMT_OBJECT(object) _false = {
    &type_boolean
};

GVMT_OBJECT(object) _void = {
    &type_object
};

GVMT_OBJECT(object) _undefined = {
    &type_object
};

GVMT_OBJECT(object) _null = {
    &type_object
};

GVMT_Object TRUE = (GVMT_Object)&_true;
GVMT_Object FALSE = (GVMT_Object)&_false;

GVMT_Object VOID = (GVMT_Object)&_void;
GVMT_Object EMPTY_LIST = (GVMT_Object)&_null;
GVMT_Object UNDEFINED = (GVMT_Object)&_undefined;
int recursion_depth;
int recursion_limit;

int is_list(GVMT_Object l) {
    do {
        if (gvmt_is_tagged(l))
            return 0;
        if (is_empty_list(l))
            return 1;
        if (((R_object)l)->type != &type_cons) {
            return 0;
        }
        l = tail(l);
    } while (1);
    return 0;
}

void print_vector(GVMT_Object o) {
    R_frame vector = (R_frame)o;
    int i, len = vector->size;
    printf("#(");
    if (len) {
        print(vector->values[0]);
        for (i = 1; i < len; i++) {
            printf(" ");
            print(vector->values[i]);
        }
    }
    printf(")");  
}

int print_short(GVMT_Object o) {
    if (o == NULL) {
        return fprintf(stderr, "NULL");
    } else if (o == EMPTY_LIST) {
        return fprintf(stderr, "() ");
    } else if (gvmt_is_tagged(o)) { 
        void* p = gvmt_untag(o);
        return fprintf(stderr, "%d ", ((intptr_t)p)/2);
    } else {
        struct type *t = ((R_object)o)->type;
        return fprintf(stderr, "<%s> ", t->name);
    }   
}

void print_frame(GVMT_Object o) {
    R_frame vector = (R_frame)o;
    int i, len = vector->size;
    printf("{");
    if (len) {
        print(vector->values[0]);
        for (i = 1; i < len; i++) {
            printf(" ");
            print_short(vector->values[i]);
        }
    }
    printf("}");  
}

void print_list(R_cons cons) {
    printf("(");
    while (cons->cdr != EMPTY_LIST) {
        print(cons->car);
        printf(" ");
        cons = (R_cons)cons->cdr;
        assert (cons == (R_cons)EMPTY_LIST || cons->type == &type_cons);
    }
    print(cons->car);
    printf(")");
}

void print_cons(GVMT_Object o) {
    R_cons cons = (R_cons)o;
    if (is_list(o)) {
        print_list(cons);
    } else {
        printf("(cons ");
        print(cons->car);
        printf(" ");
        print(cons->cdr);
        printf(")");
    }
}

void print_undefined_variable(GVMT_Object o) {
    R_undefined u = (R_undefined)o;
    printf("Undefined variable ");
    print((GVMT_Object)u->name);
    if (u->name->line_number)
        printf(" at line %d", u->name->line_number);
}    

void print_function(GVMT_Object o) {
   R_function f = (R_function)o;
   print_symbol((GVMT_Object)f->name);
}


void print_closure(GVMT_Object o) {
    R_closure c = (R_closure)o;
    print_function((GVMT_Object)c->function);
    printf("-");
    if (c->frame)
        print_frame((GVMT_Object)c->frame);
    else
        printf("0");
}

void print_syntax(GVMT_Object o) {
    R_syntax s = (R_syntax)o;
    printf(s->name);
}

void print_user_syntax(GVMT_Object o) {
    R_user_syntax s = (R_user_syntax)o;
    print_symbol((GVMT_Object)s->name);
}

void print_boolean(GVMT_Object o) {
    if (o == (GVMT_Object)FALSE) {
        printf("#f");
    } else {
        printf("#t");  
    }
}

void print_float(GVMT_Object o) {
    R_float f = (R_float)o;
    printf("%g", f->value);
}

int equal(GVMT_Object o1, GVMT_Object o2) {
    struct type *t1, *t2;
    if (o1 == o2) {
        return 1;
    }
    t1 = ((R_object)o1)->type;
    t2 = ((R_object)o2)->type;
    if (t1 != t2) {
        return 0;
    } 
    if (t1 == &type_cons) {
        return equal(head(o1), head(o2)) && equal(tail(o1), tail(o2));
    } else if (t1 == &type_symbol) {
        return symbol_eqs((R_symbol)o1, (R_symbol)o2);
    } else if (t1 == &type_vector) {
        int i, l1, l2;
        R_frame v1, v2;
        v1 = (R_frame)o1;
        v2 = (R_frame)o2;
        l1 = v1->size;
        l2 = v2->size;
        if (l1 != l2)
            return 0;
        for (i = 0; i < l1; i++) {
            if (!equal(v1->values[i], v2->values[i]))
                return 0;
        }
        return 1;
    } else {
        return 0;
    }
}

void print_object(GVMT_Object o) {
    if (o == (GVMT_Object)EMPTY_LIST) {
        printf("()");  
    } else if (o == (GVMT_Object)VOID) {
        printf("#void");
    } else if (o == (GVMT_Object)UNDEFINED) {
        printf("undefined");  
    } else{
        printf("Unrecognised object");   
    }
}

void print_symbol(GVMT_Object o) {
    R_symbol s = (R_symbol)o;
    int i;
    for (i = 0; i < s->length; i++) {
        printf("%c", s->text[i]);   
    }
}

void print_string(GVMT_Object o) {
    print_symbol(o);
}

void print_error(GVMT_Object o) {
    char buf[200];
    struct type* t = ((R_symbol)o)->type;
    sprintf(buf, "Unprintable object of type %s", t->name);
    error(buf, NULL);
}

void print(GVMT_Object o) {
    if (o == NULL) {
        printf("NULL");
    } else if (o == EMPTY_LIST) {
        printf("()");
    } else if (gvmt_is_tagged(o)) {  
        int i = unbox(o);
        printf("%d", i);
    } else {
        struct type *t = ((R_symbol)o)->type;
        t->print(o);
    }
}

void vector_copy(R_frame dest, int dest_start, 
                 R_frame src, int src_start, int src_end) { 
    int i, len;
    if (dest_start < 0 || src_start < 0 || src_end > src->size ||
        dest_start + src_end - src_start > dest->size ||
        src_end < src_start)
        error("Vector bound exceeded: ", (GVMT_Object)dest);
    len = src_end - src_start;
    for(i = 0; i < len; i++) {
        GVMT_Object o = src->values[src_start+i];
        dest->values[dest_start+i] = o;
    }
}

/** (Destructive) list functions */
GVMT_Object append(GVMT_Object list, GVMT_Object item) {
    R_cons pair = (R_cons)list;
    struct type *t = pair->type;
    if (is_empty_list(list))
        return (GVMT_Object)make_cons(item, EMPTY_LIST);
    assert(t == &type_cons);
    while (!is_empty_list(pair->cdr)) {
        pair = (R_cons)pair->cdr;  
        t = pair->type;
        assert(t == &type_cons);
    }
    pair->cdr = (GVMT_Object)make_cons(item, EMPTY_LIST);
    return list;
}

GVMT_Object prepend(GVMT_Object item, GVMT_Object list) {
    return (GVMT_Object)make_cons(item, list); 
}
//
//GVMT_Object unzip(GVMT_Object list) {
//   GVMT_Object h, t, l, r, pair;
//   if (list == EMPTY_LIST) 
//       return (GVMT_Object)make_cons(EMPTY_LIST, EMPTY_LIST);
//   if (gvmt_is_tagged(list) || ((R_object)list)->type != &type_cons)
//       error("Expecting a list");
//   h = head(list);
//   t = tail(list);
//   if (gvmt_is_tagged(h) || ((R_object)h)->type != &type_cons)
//       error("Expecting a pair");
//   pair = unzip(t);
//   l = (GVMT_Object)make_cons(head(h), head(pair));
//   r = (GVMT_Object)make_cons(head(tail(h)), tail(pair));
//   return (GVMT_Object)make_cons(l, r); 
//}

GVMT_Object concat(GVMT_Object list1, GVMT_Object list2) {
    R_cons pair = (R_cons)list1;
    struct type *t = pair->type;
    assert(t == &type_cons);
    while (pair->cdr) {
        pair = (R_cons)pair->cdr;  
        t = pair->type;
        assert(t == &type_cons);
    }
    pair->cdr = list2;
    return list1;
}


static char *dots = "............";
static char *indent = 0;

GVMT_Object execute(R_bytes code, R_frame literals) {
    R_closure c;
    GVMT_Object result;
    bytes_append(code, op(return));  
    c = make_closure((R_function)make_function(symbol_from_c_string(""), 0, code->bytes, code->size, literals), 0);
    c = interpreter(code->bytes, c);
    while (c) {
        c = c->function->execute(c);
    }
    result = gvmt_stack_top()[0];
    gvmt_drop(1);
    return result;
} 

/* To allow tracing */

GVMT_Object interpreter_trace(uint8_t* ip) {
    int i, n, printed;  
    char* opname = gvmt_opcode_names_interpreter[*ip];
    GVMT_Object *stack = gvmt_stack_top();
    n = gvmt_stack_depth(); 
    printed = fprintf(stderr, indent);
    fprintf(stderr, "[");
    for (i = n-1; i >= 0; i--)
        printed += print_short(stack[i]);
    fprintf(stderr, "]  ");
    while (printed < 20) {
        printed ++;
        fprintf(stderr, " ");
    }
    if (*ip == op(byte) || *ip == op(literal) || 
        *ip == op(load_local) || *ip == op(store_local) ||
        *ip == op(new_frame) || *ip == op(pick))
        fprintf(stderr, "%s %d\n", opname, ip[1]); 
    else if (*ip == op(load_global) || *ip == op(store_global))
        fprintf(stderr, "%s %d\n", opname, (ip[1] << 8) | ip[2] ); 
    else if (*ip == op(load_deref))
        fprintf(stderr, "%s %d %d\n", opname, ip[1], ip[2] ); 
    else        
        fprintf(stderr, "%s\n", opname); 
    if (*ip == op(apply))
        indent--;
    else if (*ip == op(return))
        indent++;
    return 0;
}

/* Finalization - Should never be called. */
void user_finalise_object(GVMT_Object o) {
    abort();
}

// Marshalling support. Do not require marshalling, 
// but need these to get code to link.
void* gvmt_marshaller_for_object(GVMT_Object object) {
    return 0;   
}

void gvmt_readable_name(GVMT_Object obj, char* bufffer) {   
}

int get_unique_id_for_object(GVMT_Object object) {
    return 0;
}
// End marshalling (lack-of) support.

int unbox(GVMT_Object o) {
    void* v = gvmt_untag(o);
    int i_tag = (int)v;
    int i = (i_tag-1) >> 1;
//    printf("Unboxing %d to %d\n", i_tag, i);
    return i;
}

GVMT_Object box(int i) {
    int i_tag = (i << 1) + 1;
    void* v = (void*)i_tag;
//    printf("Boxing %d to %d\n", i, i_tag);
    return gvmt_tag(v);
}

GVMT_Object not_an_integer(R_object o) {
    assert((((int)gvmt_untag(o)) & 1) == 0);
    error("Not an integer: ", (GVMT_Object)o);
    return 0;
}

void not_a_vector(GVMT_Object v) {
    error("Not a vector: ", v);
}

void not_a_closure(GVMT_Object c) {
    error("Can only apply closures not ", c);
}

void incorrect_parameters(R_function func, int actual) {
    char buf[100];
    void* tagged; 
    char* p = buf + sprintf(buf, "Incorrect number of parameters for '");
    symbol_to_buffer(func->name, p);
    sprintf(p+func->name->length, "', requires %d, provided ",
        func->parameters);
    tagged = (void*)((actual<<1)+1);
    error(buf, gvmt_tag(tagged));
}

GVMT_Object out_of_bounds(R_object i) {
    assert(gvmt_is_tagged(i));
    error("Vector index out of bounds: ", (GVMT_Object)i);
    return 0;
}

int string_to_number(R_symbol string) {
    int x = 0;
    int i, start = 0;
    int sign = 1;
    if (gvmt_is_tagged(string) || string->type != &type_string)
        error("Not a string: ", (GVMT_Object)string);
    if (string->text[0] == '-') {
        sign = -1;
        start = 1;
    }
    for (i = start; i < string->length; i++) {
        if (string->text[i] < '0' || string->text[i] > '9') {
            error("Cannot convert to a number: ", (GVMT_Object)string);
        }
        x *= 10;
        x += string->text[i] - '0';
    }
    x *= sign;
    return x;
}

GVMT_Object recursion_too_deep(void) {
     void* tagged = (void*)((recursion_limit<<1)+1);
     error("Recursion too deep: ", gvmt_tag(tagged));   
    return 0;
}

void not_a_cons(GVMT_Object c) {
    error("Not a cons: ", c);
}
               
GVMT_Object error_negate(R_object self) {
    error("Cannot negate ", (GVMT_Object)self);  
    return NULL;
}

static void binary_error(char* msg, R_object self, R_object other) {
    error(msg, (GVMT_Object)make_cons((GVMT_Object)self, (GVMT_Object)other));
}

GVMT_Object error_add(R_object self, R_object other) {
    binary_error("Cannot add ", self, other); 
    return NULL;
}

GVMT_Object error_sub(R_object self, R_object other) {
    binary_error("Cannot subtract ", self, other); 
    return NULL;
}

GVMT_Object error_mul(R_object self, R_object other) {
    binary_error("Cannot multiply ", self, other); 
    return NULL;
}                

GVMT_Object error_div(R_object self, R_object other) {
    binary_error("Cannot divide ", self, other); 
    return NULL;
}

int error_compare(R_object self, R_object other) {
    binary_error("Cannot compare ", self, other); 
    return 0;
}

GVMT_Object make_float(double d) {
    R_float f = (R_float)gvmt_malloc(sizeof(GVMT_OBJECT(float)));
    struct type *t = &type_float;
    f->type = t;
    f->value = d;
    return (GVMT_Object)f;
}
  
GVMT_Object float_negate(R_object self) {
    return make_float(-((R_float)self)->value);   
}

GVMT_Object float_add(R_object self, R_object other) {
    if (gvmt_is_tagged(other)) {
        int i = as_int(other);
        return make_float(((R_float)self)->value + (i >> 1));
    } else if (other->type == &type_float) {   
        return make_float(((R_float)self)->value + ((R_float)other)->value);
    } else {
        return error_add(self, other);
    }
}

GVMT_Object float_sub(R_object self, R_object other) {
    if (gvmt_is_tagged(other)) {
        int i = as_int(other);
        return make_float(((R_float)self)->value - (i >> 1));
    } else if (other->type == &type_float) {   
        return make_float(((R_float)self)->value - ((R_float)other)->value);
    } else {
        return error_sub(self, other);
    }
}

GVMT_Object float_mul(R_object self, R_object other) {
    if (gvmt_is_tagged(other)) {
        int i = as_int(other);
        return make_float(((R_float)self)->value * (i >> 1));
    } else if (other->type == &type_float) {   
        return make_float(((R_float)self)->value * ((R_float)other)->value);
    } else {
        return error_mul(self, other);
    }
}

GVMT_Object float_div(R_object self, R_object other) {
    if (gvmt_is_tagged(other)) {
        int i = as_int(other);
        return make_float(((R_float)self)->value / (i >> 1));
    } else if (other->type == &type_float) {   
        return make_float(((R_float)self)->value / ((R_float)other)->value);
    } else {
        return error_div(self, other);
    }
}

int float_compare(R_object self, R_object other) {
    if (gvmt_is_tagged(other)) {
        int i = as_int(other);
        double d = ((R_float)self)->value;
        if (d > (i>>1))
            return 1;
        if (d == (i>>1))
            return 0;
        else
            return -1;
    } else if (other->type == &type_float) {   
        double d1 = ((R_float)self)->value;
        double d2 = ((R_float)other)->value;
        if (d1 > d2)
            return 1;
        if (d1 == d2)
            return 0;
        else
            return -1;
    } else {
        return error_compare(self, other);
    }
}

void gvmt_user_uncaught_exception(GVMT_Object ex) {
    // Shouldn't happen.
    abort();
}

