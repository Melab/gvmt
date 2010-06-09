
typedef GVMT_OBJECT(cons) *R_cons;
typedef R_cons R_list;
typedef GVMT_OBJECT(bytes) *R_bytes;
typedef GVMT_OBJECT(symbol) *R_symbol;
typedef GVMT_OBJECT(object) *R_object;
typedef GVMT_OBJECT(function) *R_function;
typedef GVMT_OBJECT(environment) *R_environment;
typedef GVMT_OBJECT(closure) *R_closure;
typedef GVMT_OBJECT(frame) *R_frame;
typedef GVMT_OBJECT(variable) *R_variable;
typedef GVMT_OBJECT(syntax) *R_syntax;
typedef GVMT_OBJECT(user_syntax) *R_user_syntax;
typedef GVMT_OBJECT(undefined) *R_undefined;
typedef GVMT_OBJECT(float) *R_float;

typedef void (*compile_func)(GVMT_Object self, R_environment env, R_bytes b);
typedef void (*compile_apply_func)(GVMT_Object self, GVMT_Object params, R_environment env, R_bytes b);
typedef void (*compile_list_func)(GVMT_Object params, R_environment env, R_bytes b);
typedef void (*print_func)(GVMT_Object self);
typedef GVMT_Object (*unary_func)(R_object self);
typedef GVMT_Object (*binary_func)(R_object self, R_object other);
typedef int (*compare_func)(R_object self, R_object other);

typedef GVMT_Object (*lookup_func)(R_environment env, R_symbol name);
typedef void (*bind_func)(R_environment env, R_symbol name, GVMT_Object value);
typedef R_closure (*executable)(R_closure c);
    
GVMT_Object error_negate(R_object self);
GVMT_Object error_add(R_object self, R_object other);
GVMT_Object error_sub(R_object self, R_object other);
GVMT_Object error_mul(R_object self, R_object other);
GVMT_Object error_div(R_object self, R_object other);
int error_compare(R_object self, R_object other);
    
GVMT_Object float_negate(R_object self);
GVMT_Object float_add(R_object self, R_object other);
GVMT_Object float_sub(R_object self, R_object other);
GVMT_Object float_mul(R_object self, R_object other);
GVMT_Object float_div(R_object self, R_object other);
int float_compare(R_object self, R_object other);

GVMT_OBJECT(object) {
    struct type *type;
};

GVMT_OBJECT(cons) {
    struct type *type;   
    GVMT_Object car;
    GVMT_Object cdr;
};

GVMT_OBJECT(environment) {
    struct type *type;   
    struct type *var_type;
    int size;
    R_cons variables;
    R_cons literals;
    R_environment enclosing;
};

GVMT_OBJECT(symbol) {
    struct type *type;
    int length;
    int line_number;
    char text[1]; // Variable sized object.
};

GVMT_OBJECT(bytes) {
    struct type *type;
    int size; 
    int length; 
    uint8_t* bytes;
};

GVMT_OBJECT(syntax) {
    struct type *type;
    char* name;
    compile_list_func compile;
};

GVMT_OBJECT(user_syntax) {
    struct type *type;
    R_symbol name;
    GVMT_Object transformer;
};

struct type {
    int shape[3];
    char* name;
    compile_func compile;
    compile_apply_func compile_apply;
    print_func print;
    unary_func negate;
    binary_func add;
    binary_func sub;
    binary_func mul;
    binary_func div;
    compare_func compare;
};

struct lexer {
    FILE* source;
    int pre_fetched;
    int next_char;
    int line_number;
};

GVMT_OBJECT(variable) {
    struct type *type;
    int index;  
    R_environment env;
};

GVMT_OBJECT(function) {
    struct type *type;
    uint8_t* bytecodes;
    int parameters;
    int length;
    executable execute;
    R_symbol name;
    R_frame literals;
};

GVMT_OBJECT(frame) {
   struct type *type;
   int size;
   GVMT_Object values[1];
};

GVMT_OBJECT(closure) {
   struct type *type;
   R_function function;
   R_frame frame;
};

GVMT_OBJECT(undefined) {
   struct type *type;
   R_symbol name;    
};

GVMT_OBJECT(float) {
   struct type *type;
   double value;
};

void print(GVMT_Object o);
void print_error(GVMT_Object o);
void print_cons(GVMT_Object o);
void print_vector(GVMT_Object o);
void print_symbol(GVMT_Object o);
void print_boolean(GVMT_Object o);
void print_object(GVMT_Object o);
void print_syntax(GVMT_Object o);
void print_string(GVMT_Object o);
void print_user_syntax(GVMT_Object o);
void print_function(GVMT_Object o);
void print_closure(GVMT_Object o);
void print_error(GVMT_Object o);
void print_undefined_variable(GVMT_Object o);
void print_float(GVMT_Object o);

extern struct type type_cons;
extern struct type type_symbol;
extern struct type type_string;
extern struct type type_function;
extern struct type type_bytes;
extern struct type type_environment;
extern struct type type_frame;
extern struct type type_closure;
extern struct type type_local_variable;
extern struct type type_global_variable;
extern struct type type_vector;
extern struct type type_syntax;
extern struct type type_user_syntax;
extern struct type type_undefined;
extern struct type type_float;

extern GVMT_Object IF;
extern GVMT_Object OR;
extern GVMT_Object AND;
extern GVMT_Object LIST;

extern GVMT_Object TRUE;
extern GVMT_Object FALSE;

extern GVMT_Object VOID;
extern GVMT_Object EMPTY_LIST;
extern GVMT_Object UNDEFINED;

extern GVMT_Object BEGIN;
extern GVMT_Object VECTOR;
extern GVMT_Object QUOTE;
extern GVMT_Object QUASI_QUOTE;
extern GVMT_Object UNQUOTE;
extern GVMT_Object LAMBDA;
extern GVMT_Object DEFINE;

extern R_frame globals;
extern R_environment global_environment;
extern int print_expression;
extern int disassemble;
extern int jit_compile;
extern int flags_for_lib;
extern char special_chars[256];
extern int recursion_depth;
extern int recursion_limit;
extern int tracing_on; 

#define head(l) (((R_cons)l)->car)
#define tail(l) (((R_cons)l)->cdr)

char* read_whole_file(char* filename);

void consume_white_space(struct lexer *l);

GVMT_Object parse_input(struct lexer *l);

#define is_eof(ch) ((ch)<0)
void init_lexer(struct lexer *l, char* filename);
void terminal_lexer(struct lexer *l);
int get_char(struct lexer *l);
int peek_char(struct lexer *l);
void consume_line(struct lexer *l);

GVMT_Object syntax_error(char* fmt, struct lexer *l);
GVMT_Object undefined_variable(R_symbol name);

GVMT_Object test_eval(char* filename);
GVMT_Object make_string(char* start, char* end);
GVMT_Object string_from_c_string(char* str);
GVMT_Object run_program(char* filename, int argc, char** argv);

GVMT_Object eval(GVMT_Object o, R_environment env);
void compile_literal(GVMT_Object o, R_environment env, R_bytes b);
void compile(GVMT_Object o, R_environment env, R_bytes b);
 
void compile_lambda(GVMT_Object params, R_environment env, R_bytes b);
void compile_define(GVMT_Object params, R_environment env, R_bytes b);
void compile_define_syntax(GVMT_Object params, R_environment env, R_bytes b);
void compile_let(GVMT_Object params, R_environment env, R_bytes b);
void compile_let_star(GVMT_Object params, R_environment env, R_bytes b);
void compile_let_rec(GVMT_Object params, R_environment env, R_bytes b);
void compile_do(GVMT_Object params, R_environment env, R_bytes b);

void compile_quote(GVMT_Object params, R_environment env, R_bytes b);
void compile_quasi_quote(GVMT_Object params, R_environment env, R_bytes b);
void compile_unquote(GVMT_Object params, R_environment env, R_bytes b);
void compile_vector(GVMT_Object params, R_environment env, R_bytes b);
void compile_begin(GVMT_Object params, R_environment env, R_bytes b);

void compile_object(GVMT_Object o, R_environment env, R_bytes b);
void compile_function(GVMT_Object o, R_environment env, R_bytes b);

void compile_list(GVMT_Object params, R_environment env, R_bytes b);
void compile_if(GVMT_Object params, R_environment env, R_bytes b);
void compile_and(GVMT_Object params, R_environment env, R_bytes b);
void compile_or(GVMT_Object params, R_environment env, R_bytes b);

void compile_local(GVMT_Object l, R_environment env, R_bytes b);
void compile_global(GVMT_Object l, R_environment env, R_bytes b);
void compile_set_bang(GVMT_Object l, R_environment env, R_bytes b);

void compile_cons(GVMT_Object l, R_environment env, R_bytes b);
void compile_apply_default(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b);

void compile_apply_function(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b);

GVMT_Object make_symbol(char* start, char* end, int line_number);

void compile_symbol(GVMT_Object l, R_environment env, R_bytes b);
void compile_apply_symbol(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b);
void compile_apply_syntax(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b);
void compile_apply_user_syntax(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b);

void compile_syntax_error(GVMT_Object o, R_environment env, R_bytes b);
void compile_user_syntax_error(GVMT_Object o, R_environment env, R_bytes b);

GVMT_Object eval_as_self(GVMT_Object o, R_environment env);

R_environment make_top_level_environment(void);

GVMT_Object make_function(R_symbol name, int parameters, uint8_t* bytecodes, int length, R_frame literals);

GVMT_Object make_function_c(char* name, int parameters, uint8_t* bytecodes, int length, R_frame literals);

R_closure make_closure(R_function func, R_frame frame);
GVMT_Object make_float(double d);
R_bytes make_bytes(void);

R_cons make_cons(GVMT_Object car, GVMT_Object cdr);

void bytes_append(R_bytes b, int x);

void bytes_dispose(R_bytes);
void bytes_clear(R_bytes b);

/* Wrapper for interpreter*/
R_closure interpret(R_closure c);

R_closure compile_then_run(R_closure c);
R_closure insert_tailcalls_then_interpret(R_closure c);
R_closure interpret_once_then_compile(R_closure c);

R_closure interpreter(uint8_t* bytecodes, R_closure c);
GVMT_Object disassembler(uint8_t* bytes_start, uint8_t* bytes_end, FILE* out);
void optimise(R_closure c);
GVMT_Object cleanup(uint8_t* bytes_start, uint8_t* bytes_end);
int apply_replacement(uint8_t* bytes_start, uint8_t* bytes_end, R_closure c);
int inlineable(uint8_t* bytes_start, uint8_t* bytes_end, R_function c);
GVMT_Object can_remove_frames(uint8_t* bytes_start, uint8_t* bytes_end);
GVMT_Object peephole(uint8_t* bytes_start, uint8_t* bytes_end);
GVMT_Object frame_removal(uint8_t* bytes_start, uint8_t* bytes_end);
GVMT_Object straight_line(uint8_t* bytes_start, uint8_t* bytes_end);
GVMT_Object top_frame_removal(uint8_t* bytes_start, uint8_t* bytes_end, int p_count);
GVMT_Object parse_expression(struct lexer *l);

R_frame make_vector(int size);

void error(char* msg, GVMT_Object object);

R_symbol symbol_from_c_string(char* str) ;

void symbol_to_buffer(R_symbol s, char* buffer);

/** Environment code */

int symbol_eqs(R_symbol s1, R_symbol s2);

R_symbol as_symbol(GVMT_Object o);

R_variable new_variable(R_environment env, R_symbol name);

R_environment top_level_environment(void);
R_environment local_environment(R_environment enclosing);
R_environment let_environment(R_environment enclosing);

GVMT_Object make_syntax(char* name, compile_list_func compile);

R_frame make_literals(R_environment env);
R_user_syntax make_user_syntax(GVMT_Object transformer, R_symbol name);
int literal_index(R_environment env, GVMT_Object literal);

void bind(R_environment env, char* name, GVMT_Object value);
void bind_s(R_environment env, R_symbol name, GVMT_Object value);

R_variable lookup(R_environment env, R_symbol name);

GVMT_Object undefined_variable(R_symbol name);

void store_variable(R_variable var, R_bytes b);

int list_len(GVMT_Object l);
int is_list(GVMT_Object l);
#define is_empty_list(l) (((GVMT_Object)l)==EMPTY_LIST)
#define is_symbol(o) (((R_object)o)->type == &type_symbol)

int unbox(GVMT_Object o);

GVMT_Object box(int i);

GVMT_Object read_eval_print_loop(void);

void init_parser(void);

int equal(GVMT_Object o1, GVMT_Object o2);

GVMT_Object list_to_vector(GVMT_Object list);

GVMT_Object vector_to_list(R_frame vec);
void vector_copy(R_frame dest, int dest_start, 
                 R_frame src, int src_start, int src_end); 

/** Takes a list of pairs, returns a pair of lists */
//GVMT_Object unzip(GVMT_Object list_of_pairs);

/**  (Destructive) List functions */
GVMT_Object append(GVMT_Object list, GVMT_Object item);
GVMT_Object prepend(GVMT_Object item, GVMT_Object list);
GVMT_Object concat(GVMT_Object list1, GVMT_Object list2);

GVMT_Object not_an_integer(R_object i);
void not_a_vector(GVMT_Object o);
void not_a_cons(GVMT_Object o);
void not_a_closure(GVMT_Object o);
GVMT_Object out_of_bounds(R_object i);
void incorrect_parameters(R_function func, int actual);
GVMT_Object recursion_too_deep(void);
int string_to_number(R_symbol string);

#define as_int(o) ((int)gvmt_untag(o))

GVMT_Object interpreter_trace(uint8_t* ip);

#define op(x) GVMT_OPCODE(interpreter, x)

