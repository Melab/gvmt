#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gvmt/gvmt.h>
#include <gvmt/compiler.h>
#include "opcodes.h"
#include "example.h"

// 64 Megabyte heap space
#define HEAP_SPACE 1 << 26

uint8_t is_symbol_byte_codes[] = { op(is_symbol), op(return) };

uint8_t add_byte_codes[] = { op(add), op(return) };
uint8_t sub_byte_codes[] = { op(sub), op(return) };
uint8_t mul_byte_codes[] = { op(mul), op(return) };
uint8_t mod_byte_codes[] = { op(mod), op(return) };
uint8_t div_byte_codes[] = { op(div), op(return) };
uint8_t eq_byte_codes[] = { op(is), op(return) };
uint8_t i_eq_byte_codes[] = { op(i_eq), op(return) };
uint8_t i_ne_byte_codes[] = { op(i_ne), op(return) };
uint8_t i_lt_byte_codes[] = { op(i_lt), op(return) };
uint8_t i_le_byte_codes[] = { op(i_le), op(return) };
uint8_t i_gt_byte_codes[] = { op(i_gt), op(return) };
uint8_t i_ge_byte_codes[] = { op(i_ge), op(return) };
uint8_t equal_byte_codes[] = { op(equal), op(return) };
uint8_t cons_byte_codes[] = { op(cons), op(return) };
uint8_t car_byte_codes[] = { op(car), op(return) };
uint8_t cdr_byte_codes[] = { op(cdr), op(return) };
uint8_t band_byte_codes[] = { op(band), op(return) };
uint8_t bor_byte_codes[] = { op(bor), op(return) };
uint8_t not_byte_codes[] = { op(not), op(return) };
uint8_t null_byte_codes[] = { op(is_null), op(return) };
uint8_t vec_get_byte_codes[] = { op(vec_get), op(return) };
uint8_t vec_set_byte_codes[] = { op(vec_set), op(void), op(return) };
uint8_t vec_len_byte_codes[] = { op(vec_len), op(return) };
uint8_t display_byte_codes[] = { op(display), op(void), op(return) };
uint8_t make_vector_byte_codes[] = { op(make_vector), op(return) };
uint8_t list_to_vec_byte_codes[] = { op(list_to_vec), op(return) };
uint8_t vec_to_list_byte_codes[] = { op(vec_to_list), op(return) };
uint8_t string_to_number_byte_codes[] = { op(str_to_num), op(return) };
uint8_t vector_copy_byte_codes[] = { op(vector_copy), op(void), op(return) };
uint8_t i_shift_byte_codes[] = { op(i_shift), op(return) };
uint8_t exit_byte_codes[] = { op(stop), op(return) };

R_frame globals;

GVMT_Object VECTOR;
GVMT_Object QUOTE;
GVMT_Object QUASI_QUOTE;
GVMT_Object UNQUOTE;
GVMT_Object LAMBDA;
GVMT_Object BEGIN;

R_environment make_top_level_environment(void) {
    R_environment env = top_level_environment();
    globals = make_vector(1000);
    bind(env, "display", make_function_c("display", 1, display_byte_codes, 3, NULL));
    bind(env, "+", make_function_c("+", 2, add_byte_codes, 2, NULL));
    bind(env, "-", make_function_c("-", 2, sub_byte_codes, 2, NULL));
    bind(env, "*", make_function_c("*", 2, mul_byte_codes, 2, NULL));
    bind(env, "/", make_function_c("/", 2, div_byte_codes, 2, NULL));
    bind(env, "modulo", make_function_c("modulo", 2, mod_byte_codes, 2, NULL));
    bind(env, "eq?", make_function_c("eq?", 2, eq_byte_codes, 2, NULL));
    // As ints are tagged and chars are not implemented, eq? == eqv?
    bind(env, "eqv?", make_function_c("eqv?", 2, eq_byte_codes, 2, NULL));
    bind(env, "equal?", make_function_c("equal?", 2, equal_byte_codes, 2, NULL));
    bind(env, "symbol?", make_function_c("symbol?", 1, is_symbol_byte_codes, 2, NULL));
    bind(env, "=", make_function_c("=", 2, i_eq_byte_codes, 2, NULL));
    bind(env, "!=", make_function_c("!=", 2, i_ne_byte_codes, 2, NULL));
    bind(env, "<", make_function_c("<", 2, i_lt_byte_codes, 2, NULL));
    bind(env, "<=", make_function_c("<=", 2, i_le_byte_codes, 2, NULL));
    bind(env, ">", make_function_c(">", 2, i_gt_byte_codes, 2, NULL));
    bind(env, ">=", make_function_c(">=", 2, i_ge_byte_codes, 2, NULL));
    bind(env, "arithmetic-shift", make_function_c("arithmetic-shift", 2, i_shift_byte_codes, 2, NULL));
    bind(env, "cons", make_function_c("cons", 2, cons_byte_codes, 2, NULL));
    bind(env, "car", make_function_c("car", 1, car_byte_codes, 2, NULL));
    bind(env, "cdr", make_function_c("cdr", 1, cdr_byte_codes, 2, NULL));
    bind(env, "&", make_function_c("&", 2, band_byte_codes, 2, NULL));
    bind(env, "|", make_function_c("|", 2, bor_byte_codes, 2, NULL));
    bind(env, "vector-ref", make_function_c("vector-ref", 2, vec_get_byte_codes, 2, NULL));
    bind(env, "vector-set!", make_function_c("vector-set!", 3, vec_set_byte_codes, 3, NULL));
    bind(env, "list->vector", make_function_c("list->vector", 1, list_to_vec_byte_codes, 2, NULL));
    bind(env, "vector->list", make_function_c("vector->list", 1, vec_to_list_byte_codes, 2, NULL));
    bind(env, "string->number", make_function_c("string->number", 1, string_to_number_byte_codes, 2, NULL));
    bind(env, "__vector-copy!", make_function_c("__vector-copy!", 5, vector_copy_byte_codes, 3, NULL));
    bind(env, "set!", make_syntax("set!", compile_set_bang));
    bind(env, "vector-length", make_function_c("vector-length", 1, vec_len_byte_codes, 2, NULL));
    bind(env, "not", make_function_c("not", 1, not_byte_codes, 2, NULL));
    bind(env, "null?", make_function_c("null?", 1, null_byte_codes, 2, NULL));
    bind(env, "if", make_syntax("if", compile_if));
    bind(env, "and", make_syntax("and", compile_and));
    bind(env, "or", make_syntax("or", compile_or));
    bind(env, "let", make_syntax("let", compile_let));
    bind(env, "let*", make_syntax("let*", compile_let_star));
    bind(env, "letrec", make_syntax("letrec", compile_let_rec));
    bind(env, "do", make_syntax("do", compile_do));
    LAMBDA = make_syntax("lambda", compile_lambda);
    bind(env, "lambda", LAMBDA);
    bind(env, "define", make_syntax("define", compile_define));
    bind(env, "define-syntax", make_syntax("define-syntax", compile_define_syntax));
    bind(env, "list", make_syntax("list", compile_list));
    QUOTE = make_syntax("'", compile_quote);
    QUASI_QUOTE = make_syntax("`", compile_quasi_quote);
    UNQUOTE = make_syntax(",", compile_unquote);
    VECTOR = make_syntax("vector", compile_vector);
    bind(env, "vector", VECTOR);
    BEGIN = make_syntax("begin", compile_begin);
    bind(env, "begin", BEGIN);
    bind(env, "#t", TRUE);
    bind(env, "#f", FALSE);
    bind(env, "#void", VOID);
    bind(env, "#null", EMPTY_LIST);
    bind(env, "#undefined", UNDEFINED);
    bind(env, "__make-vector", make_function_c("__make-vector", 2, make_vector_byte_codes, 2, NULL));
    bind(env, "exit", make_function_c("exit", 0, exit_byte_codes, 2, NULL));
    return env;
}

/** Compiles an integer literal to bytecode */
void compile_int(GVMT_Object o, R_environment env, R_bytes b) {
    intptr_t i = unbox(o);
    if (((int8_t)i) == i) {
        bytes_append(b, op(byte));
        bytes_append(b, i);
    } else if (((int16_t)i) == i) {
        bytes_append(b, op(short));
        bytes_append(b, (i >> 8));
        bytes_append(b, i);
    } else {
        bytes_append(b, op(literal));
        bytes_append(b, literal_index(env, o));
    }
}

/** Compiles an arbitrary object ot bytecode */
void compile(GVMT_Object o, R_environment env, R_bytes b) {
    struct type *t;
    if (gvmt_is_tagged(o)) {
        compile_int(o, env, b);
    } else if (o == EMPTY_LIST) {
        bytes_append(b, op(nil));
    } else {       
        t = ((R_object)o)->type;  
        t->compile(o, env, b);
    }
}

void cant_apply(GVMT_Object o) {
    error("Cannot apply ", o);
}

void compile_apply(GVMT_Object o, GVMT_Object p, R_environment env, R_bytes b) {
    struct type *t;
    if (o == EMPTY_LIST) {
        error("Cannot apply ", o); 
    } else if (gvmt_is_tagged(o)) {
        error("Cannot apply ", o); 
    } else {       
        t = ((R_object)o)->type;
        t->compile_apply(o, p, env, b);
    }
}

/** Compiles a cons cell by compiing the application of the head of the list to the tail*/
void compile_cons(GVMT_Object l, R_environment env, R_bytes b) {
    R_object h = (R_object)head(l);
    l = tail(l);
    compile_apply((GVMT_Object)h, l, env, b);
}

void compile_user_syntax_error(GVMT_Object o, R_environment env, R_bytes b) {
    R_user_syntax s = (R_user_syntax)o;
    struct type *t = s->type;
    assert(t == &type_user_syntax);
    error("Syntax error: ", (GVMT_Object)s);
}

void compile_syntax_error(GVMT_Object o, R_environment env, R_bytes b) {
    R_syntax s = (R_syntax)o;
    struct type *t = s->type;
    assert(t == &type_syntax);
    error("Syntax error: ", (GVMT_Object)s);
}

static int expand_compile(GVMT_Object l, R_environment env, R_bytes b) {
    if (l != EMPTY_LIST) {
        int len;
        assert(((R_object)l)->type == &type_cons);
        len = 1 + expand_compile(tail(l), env, b);
        compile(head(l), env, b);
        return len;
    } else {
        return 0;
    }
}

GVMT_Object process_as_self(GVMT_Object o, R_environment env) {
    return o;
}

static void expand_eval(GVMT_Object l, R_environment env) {
    while (l != EMPTY_LIST) {
        assert(((R_object)l)->type == &type_cons);
        GVMT_PUSH(eval(head(l), env));
        l = tail(l);
    }
}

int list_len(GVMT_Object l) {
    int len = 0;
    while (l != EMPTY_LIST) {
        if (((R_object)l)->type != &type_cons) {
            error("Not a list: ", l);    
        }
        len++;
        l = tail(l);
    }
    return len;
}

/** Convert a list to a vector */
GVMT_Object list_to_vector(GVMT_Object list) {
    int i, len = list_len(list);
    R_frame vector = make_vector(len);
    for (i = 0; i < len; i++) {
        GVMT_Object item = head(list);
        vector->values[i] = item;
        list = tail(list);
    }
    assert(list == EMPTY_LIST);
    return (GVMT_Object)vector;
}

/** Convert a vector to a list */
GVMT_Object vector_to_list(R_frame vector) {
    int i, len;
    R_cons list;   
    if (vector->type != &type_vector)
        error("Not a vector: ", (GVMT_Object)vector);
    len = vector->size;
    list = (R_cons)EMPTY_LIST;
    for (i = len-1; i >= 0; i--) {
        list = make_cons(vector->values[i], (GVMT_Object)list);  
    }
    return (GVMT_Object)list;
}

static void wrong_params(GVMT_Object params, char* name, char* expected) {
    int a_len = list_len(params);
    char buf[200];
    sprintf(buf, 
            "Wrong number of parameters for '%s'. Found %d, should be %s", 
            name, a_len, expected); 
    error(buf, 0);
}

static void check_params(GVMT_Object params, int p_len, char* name) {
    int a_len = list_len(params);
    if (a_len != p_len) {
        char buf[20];
        sprintf(buf, "%d", p_len);
        wrong_params(params, name, buf);
    }
}

/** Compile application of a syntax object */
void compile_apply_syntax(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b) {
    R_syntax s = (R_syntax)o;
    struct type *t = s->type;
    assert(t == &type_syntax);
    s->compile(params, env, b);
}

/** Compile general application */
void compile_apply_default(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b) {
    expand_compile(params, env, b);
    compile(o, env, b);
    bytes_append(b, op(apply));
    bytes_append(b, list_len(params));
}

/** Compile ` */
void compile_quote(GVMT_Object params, R_environment env, R_bytes b) {
    assert(is_empty_list(tail(params)));
    bytes_append(b, op(literal));
    bytes_append(b, literal_index(env, head(params)));
}

static void compile_quasi_list(GVMT_Object params, R_environment env, R_bytes b);

static void compile_quasi(GVMT_Object o, R_environment env, R_bytes b) {
    struct type *t;
    if (o == EMPTY_LIST) {
        bytes_append(b, op(nil));
    } else if (gvmt_is_tagged(o)) {
        compile_int(o, env, b);
    } else {
        t = ((R_object)o)->type;
        if (t == &type_cons) {
            compile_quasi_list(o, env, b);
        } else {
            compile_literal(o, env, b);
        }
    }
}

static void compile_quasi_list_recurse(GVMT_Object list, R_environment env, R_bytes b) {
    if (list == EMPTY_LIST) {
        bytes_append(b, op(nil));
        return;   
    }
    compile_quasi_list_recurse(tail(list), env, b);
    compile_quasi(head(list), env, b);
    bytes_append(b, op(cons));
}

static void compile_quasi_list(GVMT_Object params, R_environment env, R_bytes b) {
    if (head(params) == UNQUOTE) {
        compile_unquote(tail(params), env, b);
    } else {
        compile_quasi_list_recurse(params, env, b);
    }
}

/** Compile quasi-quote */
void compile_quasi_quote(GVMT_Object params, R_environment env, R_bytes b) {
    assert(is_empty_list(tail(params)));
    compile_quasi(head(params), env, b);
}

/** Compile unquote */
void compile_unquote(GVMT_Object params, R_environment env, R_bytes b) {
    assert(is_empty_list(tail(params)));
    compile(head(params), env, b);
}

/** Compile vector */
void compile_vector(GVMT_Object params, R_environment env, R_bytes b) {
    expand_compile(params, env, b);
    bytes_append(b, op(vector));
    bytes_append(b, list_len(params));
}

/** Compile various builtin primitives */
void compile_object(GVMT_Object o, R_environment env, R_bytes b) {
    if (o == (GVMT_Object)EMPTY_LIST) {
         bytes_append(b, op(nil));  
    } else if (o == (GVMT_Object)VOID) {
         bytes_append(b, op(void));  
    } else if (o == (GVMT_Object)UNDEFINED) {
         bytes_append(b, op(undefined));  
    } else{
        error("Unrecognised object: ", o);   
    }
}

/** Compile closure creation */
void compile_function(GVMT_Object o, R_environment env, R_bytes b) {
    compile_literal(o, env, b);
    bytes_append(b, op(make_closure));
}

/** Compile BEGIN */
void compile_begin(GVMT_Object params, R_environment env, R_bytes b) {
    GVMT_Object item;
    if (params == EMPTY_LIST) {
        wrong_params(NULL, "begin", "at least 1");
    }
    item = head(params);
    params = tail(params);
    while (params != EMPTY_LIST) {
        compile(item, env, b);
        bytes_append(b, op(drop));
        item = head(params);
        params = tail(params);       
    }
    compile(item, env, b);
}

static void compile_list_reverse(GVMT_Object list, R_environment env, R_bytes b) {
    if (list == EMPTY_LIST)
        return;
    compile_list_reverse(tail(list), env, b);
    compile(head(list), env, b);
    bytes_append(b, op(cons));
}

/** Compile creation of a list */
void compile_list(GVMT_Object params, R_environment env, R_bytes b) {
    bytes_append(b, op(nil));
    compile_list_reverse(params, env, b);
}

static void compile_lambda_expanded(GVMT_Object parameters, GVMT_Object code,
                                    R_environment env, R_bytes b, R_symbol name) {
    GVMT_Object lambda;
    R_bytes x;
    R_environment local_env;
    int i, patch, param_count = list_len(parameters);
    local_env = local_environment(env);
    x = make_bytes();
    bytes_append(x, op(new_frame));
    // Need to patch in frame size.
    patch = x->size;
    bytes_append(x, 0);
    while (parameters != EMPTY_LIST) {
        R_symbol name = as_symbol(head(parameters));
        R_variable var = new_variable(local_env, name);
        store_variable(var, x);
        parameters = tail(parameters);
    }
//    printf("Code: ");
//    print(code);
//    printf("\n");
    compile_begin(code, local_env, x);
    x->bytes[patch] = local_env->size;
    bytes_append(x, op(return));
    if (disassemble) {
        print((GVMT_Object)name);
        printf("\n");
        disassembler(x->bytes, x->bytes + x->size, stdout);
    }
    lambda = make_function(name, param_count, x->bytes, x->size, make_literals(local_env));
    bytes_append(b, op(literal));
    bytes_append(b, literal_index(env, lambda));
    bytes_append(b, op(make_closure));
}

/** Compile a lambda */
void compile_lambda(GVMT_Object params, R_environment env, R_bytes b) {
    GVMT_Object parameters, code;
    if (list_len(params) < 2) {
        wrong_params(params, "lambda", "at least 2");
    }
    parameters = head(params);
    code = tail(params);
    compile_lambda_expanded(parameters, code, env, b, symbol_from_c_string("lambda"));
}

static void unzip2(GVMT_Object list, R_cons* left, R_cons* right) {
    GVMT_Object pair;
    if (is_empty_list(list))
        return;
    pair = head(list);
    if (!is_list(pair) || list_len(pair) != 2)
        error("Syntax error in let variables: ", pair);
    unzip2(tail(list), left, right);
    *left = make_cons(head(pair), (GVMT_Object)*left);
    *right = make_cons(head(tail(pair)), (GVMT_Object)*right);
}


static void parse_let(GVMT_Object let, R_cons *names, R_cons *values, GVMT_Object* body) {
    GVMT_Object name_values, t;
    if (let == EMPTY_LIST)
        error("Syntax error empty let", NULL); 
    name_values = head(let);
    if (!is_list(name_values))
        error("Syntax error in let variables: ", name_values);
    t = tail(let);
    if (t == EMPTY_LIST)
        error("Syntax error: let body missing", NULL);
    *body = t;
    *names = (R_cons)EMPTY_LIST;
    *values = (R_cons)EMPTY_LIST;
    unzip2(name_values, names, values);
}

static void store_reverse(R_cons names, R_environment env, R_bytes b) {
    R_variable var;
    if (is_empty_list(names))
        return;
    store_reverse((R_cons)tail(names), env, b);
    var = lookup(env, as_symbol(head(names)));
    store_variable(var, b);   
}

static void make_variables_and_store_reverse(R_cons names, R_environment env, R_bytes b) {
    R_variable var;
    if (is_empty_list(names))
        return;
    make_variables_and_store_reverse((R_cons)tail(names), env, b);
    var = new_variable(env, as_symbol(head(names)));
    store_variable(var, b);
}

/** Compile an unnamed let */
void compile_unnamed_let(GVMT_Object params, R_environment env, R_bytes b) {
    R_cons names, values;
    GVMT_Object body;
    R_environment let_env;
    parse_let(params, &names, &values, &body);
    while (!is_empty_list(values)) {
        compile(head(values), env, b);
        values = (R_cons)tail(values);
    }
    let_env = let_environment(env);
    bytes_append(b, op(new_frame));
    bytes_append(b, list_len((GVMT_Object)names));
    make_variables_and_store_reverse(names, let_env, b);
    compile_begin(body, let_env, b);
    bytes_append(b, op(release_frame));
}

static void compile_reverse(GVMT_Object list, R_environment env, R_bytes b) {
    if (!is_empty_list(list)) {
        compile_reverse(tail(list), env, b);
        compile(head(list), env, b);
    }
}

/** Compile a named let */
void compile_named_let(GVMT_Object params, R_environment env, R_bytes b) {
    R_cons names, values;
    GVMT_Object body;
    R_environment let_env;
    R_symbol name = as_symbol(head(params));
    R_variable var;
    parse_let(tail(params), &names, &values, &body);
    let_env = let_environment(env);
    var = new_variable(let_env, name);
    bytes_append(b, op(new_frame));
    bytes_append(b, 1);
    bytes_append(b, op(undefined));
    store_variable(var, b);
    compile_lambda_expanded((GVMT_Object)names, body, let_env, b, name);
    store_variable(var, b);
    compile_reverse((GVMT_Object)values, let_env, b);
    compile((GVMT_Object)var, let_env, b);
    bytes_append(b, op(apply));
    bytes_append(b, list_len((GVMT_Object)names));
    bytes_append(b, op(release_frame));
}

/** Compile a let */
void compile_let(GVMT_Object params, R_environment env, R_bytes b) {
    GVMT_Object first;
    if (is_empty_list(params)) {
        error("Syntax error empty let", 0); 
    }
    first = head(params);
    if (is_symbol(first)) {
        compile_named_let(params, env, b);
    } else {
         compile_unnamed_let(params, env, b);  
    }
}     

/** Compile a let-rec */
void compile_let_rec(GVMT_Object params, R_environment env, R_bytes b) {
    R_cons names, values;
    GVMT_Object body, temp;
    R_environment let_env;
    R_variable var;
    parse_let(params, &names, &values, &body);
    let_env = let_environment(env);
    bytes_append(b, op(new_frame));
    bytes_append(b, list_len((GVMT_Object)names));
    temp = (GVMT_Object)names;
    while (!is_empty_list(temp)) {
        var = new_variable(let_env, as_symbol(head(temp)));
        bytes_append(b, op(undefined));
        store_variable(var, b);
        temp = tail(temp);
    }
    while (!is_empty_list(values)) {
        compile(head(values), let_env, b);
        values = (R_cons)tail(values);
    }
    store_reverse(names, let_env, b);
    compile_begin(body, let_env, b);
    bytes_append(b, op(release_frame));
}

/** Compile a let* */
void compile_let_star(GVMT_Object params, R_environment env, R_bytes b) {
    R_cons names, values;
    GVMT_Object body;
    R_environment let_env;
    R_variable var;
    parse_let(params, &names, &values, &body);
    let_env = let_environment(env);   
    bytes_append(b, op(new_frame));
    bytes_append(b, list_len((GVMT_Object)names));
    while (!is_empty_list(values)) {
        compile(head(values), let_env, b);
        var = new_variable(let_env, as_symbol(head(names)));
        store_variable(var, b);
        values = (R_cons)tail(values);
        names = (R_cons)tail(names);
    }
    compile_begin(body, let_env, b);
    bytes_append(b, op(release_frame));
}

static void unzip3(GVMT_Object list, R_cons* left, R_cons* middle, R_cons* right) {
    GVMT_Object pair;
    if (is_empty_list(list))
        return;
    pair = head(list);
    if (!is_list(pair) || list_len(pair) != 3)
        error("Syntax error in do declarations: ", pair);
    unzip3(tail(list), left, middle, right);
    *left = make_cons(head(pair), (GVMT_Object)*left);
    *middle = make_cons(head(tail(pair)), (GVMT_Object)*middle);
    *right = make_cons(head(tail(tail(pair))), (GVMT_Object)*right);
}

static void parse_do(GVMT_Object let, R_cons *names, R_cons *inits, R_cons *steps, GVMT_Object* test, GVMT_Object* body) {
    GVMT_Object name_values, t;
    if (let == EMPTY_LIST)
        error("Syntax error do", NULL); 
    name_values = head(let);
    if (!is_list(name_values))
        error("Syntax error in do declarations: ", name_values);
    t = tail(let);
    if (t == EMPTY_LIST)
        error("Syntax error: do test missing", NULL);
    *test = head(t);
    *body = tail(t);
    *names = (R_cons)EMPTY_LIST;
    *inits = (R_cons)EMPTY_LIST;
    *steps = (R_cons)EMPTY_LIST;
    unzip3(name_values, names, inits, steps);
}

/** Compile a do */
void compile_do(GVMT_Object params, R_environment env, R_bytes b) {
    R_cons names, inits, steps, temp;
    GVMT_Object test, body;
    GVMT_Object condition, exit_values;
    R_environment let_env;
    R_variable var;
    int patch, offset;
    parse_do(params, &names, &inits, &steps, &test, &body);
    if (!is_list(test) || is_empty_list(test))
    error("Syntax error in do exit-clause: ", test);
    condition = head(test);
    exit_values = tail(test);
    let_env = let_environment(env);   
    bytes_append(b, op(new_frame));
    bytes_append(b, list_len((GVMT_Object)names));
    temp = names;
    while (!is_empty_list(inits)) {
        compile(head(inits), let_env, b);
        var = new_variable(let_env, as_symbol(head(temp)));
        store_variable(var, b);
        inits = (R_cons)tail(inits);
        temp = (R_cons)tail(temp);
    }
    patch = b->size;
    bytes_append(b, op(jump));
    bytes_append(b, 0);
    bytes_append(b, 0);
    if (!is_empty_list(body)) {
        compile_begin(body, let_env, b);
        bytes_append(b, op(drop));
    }
    temp = names;
    while (!is_empty_list(steps)) {
        compile(head(steps), let_env, b);
        var = lookup(let_env, as_symbol(head(temp)));
        store_variable(var, b);
        steps = (R_cons)tail(steps);
        temp = (R_cons)tail(temp);
    }
    offset = b->size-patch;
    b->bytes[patch+1] = (offset >> 8) & 0xff;
    b->bytes[patch+2] = offset & 0xff;
    compile(condition, let_env, b);
    offset = patch+3-b->size;
    bytes_append(b, op(branch_f));
    bytes_append(b, (offset >> 8) & 0xff);
    bytes_append(b, offset & 0xff);
    if (is_empty_list(exit_values)) {
        bytes_append(b, op(void));
    } else {
        compile_begin(exit_values, let_env, b);
    }
    bytes_append(b, op(release_frame));
}

/** Compile define-syntax */
void compile_define_syntax(GVMT_Object params, R_environment env, R_bytes b) {
    R_symbol name;
    GVMT_Object trans;
    check_params(params, 2, "define-syntax");
    name = as_symbol(head(params));
    trans = head(tail(params));
    compile(trans, env, b);
    compile_literal((GVMT_Object)name, env, b);
    bytes_append(b, op(define_syntax));
    bytes_append(b, op(void));
}

/** Compile the application of a user-syntax object */
void compile_apply_user_syntax(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b) {
    R_user_syntax s = (R_user_syntax)o;
    GVMT_Object trans, syntax;    
    struct type *t = s->type;
    assert(t == &type_user_syntax);
    params = prepend((GVMT_Object)s->name, params);
    params = (GVMT_Object)make_cons(QUOTE, (GVMT_Object)make_cons(params, EMPTY_LIST));
    syntax = (GVMT_Object)make_cons((GVMT_Object)s->transformer, 
                                    (GVMT_Object)make_cons(params, EMPTY_LIST));
    trans = eval(syntax, env);
    if (print_expression) {
        print(trans);
        printf("\n");
    }
    compile(trans, env, b);
}

/** Compile a define */
void compile_define(GVMT_Object params, R_environment env, R_bytes b) {
    R_symbol name;
    R_variable var;
    if (list_len(params) < 2) {
        wrong_params(params, "define", "at least 2");
    }
    name = (R_symbol)head(params);
    if (name->type == &type_symbol) {
        check_params(params, 2, "define");
        var = lookup(global_environment, name);
        compile(head(tail(params)), env, b);
    } else {
        GVMT_Object o = (GVMT_Object)name;
        name = as_symbol(head(o));
        var = lookup(global_environment, name);
        compile_lambda_expanded(tail(o), tail(params), env, b, name);
    }
    store_variable(var, b);
    bytes_append(b, op(void));
}

/** Compile set! */
void compile_set_bang(GVMT_Object params, R_environment env, R_bytes b) {
    R_symbol name;
    R_variable v;
    struct type *t;
    check_params(params, 2, "set!");
    name = as_symbol(head(params));
    v = lookup(env, name);
    compile(head(tail(params)), env, b);
    store_variable(v, b);
    bytes_append(b, op(void));
}

/** Compile the application of a builtin function */
void compile_apply_function(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b) {   
    R_function f = (R_function)o;
    struct type *t = f->type;
    int i;    
    int p_len;
    assert(t == &type_function);
    assert(f->bytecodes[f->length-1] == op(return));
    p_len = list_len(params);
    if (p_len != f->parameters) {
        char buf[20];
        char name[100];
        symbol_to_buffer(f->name, name);
        sprintf(buf, "%d", f->parameters);
        wrong_params(params, name, buf);
    }
    expand_compile(params, env, b);
    for (i = 0; i < f->length-1; i++) {
        bytes_append(b, f->bytecodes[i]); 
    }
}

/** Lookup symbol, then compile application */
void compile_apply_symbol(GVMT_Object o, GVMT_Object params, R_environment env, R_bytes b) {
    struct type *t = ((R_object)o)->type;
    assert(t == &type_symbol);
    o = (GVMT_Object)lookup(env, (R_symbol)o);
    compile_apply(o, params, env, b);
}

/** Compile a local ``variable''. */
void compile_local(GVMT_Object o, R_environment env, R_bytes b) {
    R_variable l = (R_variable)o;
    int deref = 0;
    struct type *t = l->type;
    R_environment l_env = l->env;
    assert(t == &type_local_variable);
    while (env != l_env) {
        env = env->enclosing;
        deref++;
    }
    if (deref) {
        bytes_append(b, op(load_deref));
        bytes_append(b, deref);
    } else {
          bytes_append(b, op(load_local)); 
    }
    bytes_append(b, l->index);
}

/** Compile a global ``variable''. */
void compile_global(GVMT_Object o, R_environment env, R_bytes b) {
    R_variable g = (R_variable)o;
    struct type *t = g->type;
    assert(t == &type_global_variable);
    bytes_append(b, op(load_global)); 
    bytes_append(b, g->index>>8);
    bytes_append(b, g->index&0xff);
}

/** Compile a (non-integer) literal */
void compile_literal(GVMT_Object o, R_environment env, R_bytes b) {
    bytes_append(b, op(literal));
    bytes_append(b, literal_index(env, o));
}

/** Compile a symbol, by looking up symbol, the compiling resulting value */
void compile_symbol(GVMT_Object o, R_environment env, R_bytes b) {
    struct type *t = ((R_object)o)->type;
    assert(t == &type_symbol);
    o = (GVMT_Object)lookup(env, (R_symbol)o);
    compile(o, env, b);
}

#define is_true(o) ((o)==FALSE)

/** Compile an `if' expression */
void compile_if(GVMT_Object params, R_environment env, R_bytes b) {
     GVMT_Object c, r, l;
     GVMT_Object t = params;
     int patch, offset;
     check_params(params, 3 , "if");
     c = head(t);
     t = tail(t);
     l = head(t);
     t = tail(t);
     r = head(t);
     compile(c, env, b);
     patch = b->size;
     bytes_append(b, op(branch_f));
     bytes_append(b, 0);
     bytes_append(b, 0);
     compile(l, env, b);
     offset = b->size-patch+3;
     b->bytes[patch+1] = (offset >> 8) & 0xff;
     b->bytes[patch+2] = offset & 0xff;
     patch = b->size;
     bytes_append(b, op(jump));
     bytes_append(b, 0);
     bytes_append(b, 0);
     compile(r, env, b);
     offset = b->size-patch;
     b->bytes[patch+1] = (offset >> 8) & 0xff;
     b->bytes[patch+2] = offset & 0xff;
}

/** Compile an `and' expression */
void compile_and(GVMT_Object params, R_environment env, R_bytes b) {
     GVMT_Object r, l;
     GVMT_Object t = params;
     int patch, offset;
     check_params(params, 2 , "and");
     l = head(t);
     t = tail(t);
     r = head(t);
     compile(l, env, b);
     bytes_append(b, op(copy));
     patch = b->size;
     bytes_append(b, op(branch_f));
     bytes_append(b, 0);
     bytes_append(b, 0);   
     bytes_append(b, op(drop));
     compile(r, env, b);
     offset = b->size-patch;
     b->bytes[patch+1] = (offset >> 8) & 0xff;
     b->bytes[patch+2] = offset & 0xff;
}

/** Compile an `or' expression */
void compile_or(GVMT_Object params, R_environment env, R_bytes b) {
     GVMT_Object r, l;
     GVMT_Object t = params;
     int patch, offset;
     check_params(params, 2 , "or");
     l = head(t);
     t = tail(t);
     r = head(t);
     compile(l, env, b);
     bytes_append(b, op(copy));
     patch = b->size;
     bytes_append(b, op(branch_t));
     bytes_append(b, 0);
     bytes_append(b, 0);   
     bytes_append(b, op(drop));
     compile(r, env, b);
     offset = b->size-patch;
     b->bytes[patch+1] = (offset >> 8) & 0xff;
     b->bytes[patch+2] = offset & 0xff;
}

GVMT_Object execute(R_bytes code, R_frame literals);

/** Evualate by compiling, executing, then disposing of bytecodes. */
GVMT_Object eval(GVMT_Object o, R_environment env) {
    R_bytes b = make_bytes();
    GVMT_Object value;
    compile(o, env, b);
    if (disassemble) {
        print(o);
        printf("\n");
        (disassembler(b->bytes, b->bytes+b->size, stdout));
    }
    value = execute(b, make_literals(env));
    bytes_dispose(b);
    return value;
}

/** Generalised read-eval-print loop */
GVMT_Object eval_loop(struct lexer *l, int print_values, int stop_on_error) {
    GVMT_Object value = 0;
    R_bytes code = make_bytes();
    do {
        R_cons ex;
        R_symbol message;
        GVMT_Object cause;
        GVMT_TRY(ex)   
            GVMT_Object tree = parse_expression(l);
            if (print_expression) {
                print(tree);
                printf("\n");
            }
            compile(tree, global_environment, code);
            if (disassemble) {
                print(tree);
                printf("\n");
                disassembler(code->bytes, code->bytes+code->size, stdout);
            }
            value = execute(code, make_literals(global_environment));
            if (print_values) {
                print(value);
                printf("\n");
            }
        GVMT_CATCH
            printf("Error: ");
            message = (R_symbol)head(ex);
            cause = tail(ex);
            print((GVMT_Object)message);
            if (cause)
                print(cause);
            if (message->line_number)
                printf(" at line %d", message->line_number);
            printf("\n");
            if (stop_on_error)
                return NULL;
        GVMT_END_TRY
        consume_white_space(l);
        bytes_clear(code);
    } while (!is_eof(peek_char(l)));
    bytes_dispose(code);
    return value;
}

/** Initialise the VM */
GVMT_Object init_vm(void) {
    struct lexer l;
    GVMT_Object result;
    int print = print_expression;
    int trace = tracing_on;
    int dis = disassemble;
    gvmt_malloc_init(HEAP_SPACE);
    recursion_depth = 0;
    recursion_limit = 2000;
    init_parser();
    init_lexer(&l, INSTALL_DIR "/lib.scm");
    global_environment = make_top_level_environment();
    if (flags_for_lib == 0) {
        print_expression = 0;
        tracing_on = 0;
        disassemble = 0;
    }
    result = eval_loop(&l, 0, 1);
    if (result == NULL)
        exit(100);
    print_expression = print;
    tracing_on = trace;
    disassemble = dis;
    return result;
}

/** Batch execution */
GVMT_Object run_program(char* filename, int argc, char** argv) {
    int i;
    struct lexer l;
    GVMT_Object value;
    R_symbol ex;
    R_frame argv_vector;
    init_vm();
    argv_vector = make_vector(argc);
    for (i = 0; i < argc; i++)
        argv_vector->values[i] = string_from_c_string(argv[i]);
    init_lexer(&l, filename);
    bind(global_environment, "argv", (GVMT_Object)argv_vector);
    return eval_loop(&l, 0, 1);
}

/** Interactive read-eval-print loop */
GVMT_Object read_eval_print_loop(void) {
    struct lexer l;
    init_vm();
    terminal_lexer(&l);
    eval_loop(&l, 1, 0);
    return NULL;
}

/** Optimise bytecodes for a closure */
void optimise(R_closure c) {
    R_function f = c->function; 
    if (can_remove_frames(f->bytecodes, f->bytecodes + f->length) == TRUE) {
        if (straight_line(f->bytecodes, f->bytecodes + f->length) == TRUE) {
            top_frame_removal(f->bytecodes, f->bytecodes + f->length, f->parameters);
        } else {
            frame_removal(f->bytecodes, f->bytecodes + f->length);
            while(cleanup(f->bytecodes, f->bytecodes + f->length) != FALSE);
        }
    } else {
        while(cleanup(f->bytecodes, f->bytecodes + f->length) != FALSE);
    }
    peephole(f->bytecodes, f->bytecodes + f->length);
}

/* JIT compile bytecodes and run */
R_closure compile_then_run(R_closure c) {
    char name[100];
    R_function f = c->function;
    symbol_to_buffer(f->name, name);
//    printf("%s\n", name);
//    disassembler(f->bytecodes, f->bytecodes + f->length, stdout);
//    printf("------------------------\n");
    optimise(c);
    apply_replacement(f->bytecodes, f->bytecodes + f->length, c);    
//    disassembler(f->bytecodes, f->bytecodes + f->length, stdout);
//    printf("\n");
    /* GVMT generated compiler generates machine code. */
    f->execute = gvmt_compile_jit(f->bytecodes, f->bytecodes + f->length, name);
    return f->execute(c);
}

/** Insert tailcalls into bytecode, but do no other optimisations, then interpret */
R_closure insert_tailcalls_then_interpret(R_closure c) {
    R_function f = c->function; 
    while(cleanup(f->bytecodes, f->bytecodes + f->length) != FALSE);
    apply_replacement(f->bytecodes, f->bytecodes + f->length, c);
    c->function->execute = interpret;
    return interpreter(c->function->bytecodes, c);
}

/** Simply interpret */
R_closure interpret(R_closure c) {
    return interpreter(c->function->bytecodes, c);   
}

/** Interpret once before compiling, 
 * prevents run-once code from being needlessly compiled */
R_closure interpret_once_then_compile(R_closure c) {
    // TO DO - Fix this.
    // Need to clone bytecodes before compiling.
    c->function->execute = compile_then_run;
    return interpreter(c->function->bytecodes, c); 
}


