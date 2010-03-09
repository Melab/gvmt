/** Code to parse input and creates syntax tree(s). */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gvmt/gvmt.h>
#include "example.h"

/** Create a new cons object from car and cdr */
R_cons make_cons(GVMT_Object car, GVMT_Object cdr) {
    R_cons c = (R_cons)gvmt_malloc(sizeof(GVMT_OBJECT(cons)));
    struct type *t = &type_cons;
    c->type = t;
    c->car = car;
    c->cdr = cdr;
    return c;
}

/** Create a new built-in syntax object */
GVMT_Object make_syntax(char* name, compile_list_func compile) {
    R_syntax s = (R_syntax) gvmt_malloc(sizeof(GVMT_OBJECT(syntax)));
    struct type *t = &type_syntax;
    s->type = t;
    s->name = name;
    s->compile = compile;
    return (GVMT_Object)s;
}

/** Create a new user syntax object */
R_user_syntax make_user_syntax(GVMT_Object transformer, R_symbol name) {
    R_user_syntax s = (R_user_syntax) gvmt_malloc(sizeof(GVMT_OBJECT(user_syntax)));
    struct type *t = &type_user_syntax;
    s->type = t;
    s->name = name;
    s->transformer = transformer;
    return s;
}

/** Create a new user (uninitialised) vector */
R_frame make_vector(int size) {
    R_frame f = (R_frame) gvmt_malloc(sizeof(GVMT_OBJECT(frame)) + sizeof(GVMT_Object)*(size-1));
    struct type *t = &type_vector;
    f->type = t;
    f->size = size;
    return f;
}

/** Create a new closure */
R_closure make_closure(R_function func, R_frame frame) {
    R_closure c = (R_closure)gvmt_malloc(sizeof(GVMT_OBJECT(closure)));
    struct type *t = &type_closure;
    c->type = t;
    c->function = func;
    c->frame = frame;
    return c;
}

/** Create a new bytes object */
R_bytes make_bytes(void) {
    R_bytes b = (R_bytes)gvmt_malloc(sizeof(GVMT_OBJECT(bytes)));
    struct type *t = &type_bytes;
    b->type = t;
    b->length = 0;
    b->size = 0;
    b->bytes = 0;
    return b;
}

void bytes_append(R_bytes b, int x) {
    if (b->size >= b->length) {
        int new_len = b->length ? b->length * 2 : 64;
        assert(b->size == b->length);
        b->bytes = realloc(b->bytes, new_len);
        b->length = new_len;
    }
    b->bytes[b->size] = x & 0xff;
    b->size++;
}

void bytes_dispose(R_bytes b) {
    free(b->bytes);
    b->size = 0;   
    b->bytes = 0;
}

void bytes_clear(R_bytes b) {
    b->size = 0;   
}

/** Create a new string */
GVMT_Object make_string(char* start, char* end) {
    int size = sizeof(GVMT_OBJECT(symbol)) + (end-start-1);
    R_symbol string = (R_symbol)gvmt_malloc(size);
    int i;
    struct type *t = &type_string; 
    string->type = t;
    string->length = end - start;
    for (i = 0; start + i < end; i++) {
        string->text[i] = start[i];
    }
    return (GVMT_Object)string;
}

/** Create a new symbol */
GVMT_Object make_symbol(char* start, char* end, int line_number) {
    int size = sizeof(GVMT_OBJECT(symbol)) + (end-start-1);
    R_symbol symbol = (R_symbol)gvmt_malloc(size);
    int i;
    struct type *t = &type_symbol; 
    symbol->type = t;
    symbol->length = end - start;
    for (i = 0; start + i < end; i++) {
        symbol->text[i] = start[i];
    }
    symbol->line_number = line_number;
    return (GVMT_Object)symbol;
}

R_symbol symbol_from_c_string(char* str) {
    return (R_symbol)make_symbol(str, str + strlen(str), 0);  
}

GVMT_Object string_from_c_string(char* str) {
    return make_string(str, str + strlen(str));  
}

/** Create a new function */
GVMT_Object make_function(R_symbol name, int parameters, uint8_t* bytecodes, int length, R_frame literals) {
    R_function f = (R_function)gvmt_malloc(sizeof(GVMT_OBJECT(function)));
    struct type* t = &type_function;
    f->type = t;
    f->parameters = parameters;
    f->bytecodes = bytecodes;
    f->length = length;
    f->name = name;
    f->literals = literals;
    if (jit_compile)
        f->execute = compile_then_run;
    else
        f->execute = insert_tailcalls_then_interpret;
    return (GVMT_Object)f;
}

/** As above, just takes a char* rather than a string as a name */
GVMT_Object make_function_c(char* name, int parameters, uint8_t* bytecodes, int length, R_frame literals) {
    GVMT_Object f = make_function(symbol_from_c_string(name), parameters, bytecodes, length, literals);
    if (jit_compile)
        ((R_function)f)->execute = compile_then_run;
    return f;
}

/* Parse an integer */
GVMT_Object integer(char* start, char* end) {
    int i;
    char c = *end;
    *end = 0;
    i = atoi(start);
    *end = c;
    // Tag
    return box(i);
}

/* Parse a floating-point number */
GVMT_Object floating_point(char* start, char* end) {
    double d;
    char c = *end;
    *end = 0;
    d = atof(start);
    *end = c;
    return make_float(d);
}

/** Parse fractional part of floating-point number */
GVMT_Object parse_fraction(char* start, char* end, struct lexer *l) {
    double d;
    char c = peek_char(l);
    if (c == '.') {
        *end = c;
        get_char(l);
        end++;
        c = peek_char(l);
    }
    while (c >= '0' && c <= '9') {
        *end = c;
        get_char(l);
        end++;
        c = peek_char(l);
    }
    if (c == 'e' || c == 'E') {
        *end = c;
        get_char(l);
        end++;
        c = peek_char(l);
        if (c == '-') {
            *end = c;
            get_char(l);
            end++;
            c = peek_char(l);
        }
        while (c >= '0' && c <= '9') {
            *end = c;
            get_char(l);
            end++;
            c = peek_char(l);
        }
    }
    c = *end;
    *end = 0;
    d = atof(start);
    *end = c;
    return make_float(d);
}

/** Parse a string */
GVMT_Object parse_string(struct lexer *l) {
    int c;
    char buf[200];
    char* ptr = buf;
    c = get_char(l);
    assert(c == '"');
    do {
        c = get_char(l); 
        if (c == '\\') {
            c = get_char(l);    
            if (c == 'n') {
                *ptr++ = '\n';
            } else if (c == 't') {
                *ptr++ = '\t';
            } else if (c == 'r') {
                *ptr++ = '\r';
            } else if (c == '\\') {
                *ptr++ = '\\';
            } else {
                sprintf(buf, "Illegal escape sequence: \\%c", c);
                error(buf, 0);
            }    
        } else {
            *ptr++ = c;
        }
    } while (c != '"' && !is_eof(c));
    return make_string(buf, ptr-1);
}

GVMT_Object parse_expression(struct lexer *l);

/** Parse a list */
GVMT_Object parse_list(struct lexer *l) {
    int i;
    int start_line = l->line_number;
    R_cons first, last;
    int start = get_char(l);
    char closing = 0;
    if (start == '(')
        closing = ')';
    else if (start == '[')
        closing = ']';
    else 
        syntax_error(" expected '(' or '['", l);
    consume_white_space(l);
    if (peek_char(l) == closing) {
        get_char(l);
        return (GVMT_Object)EMPTY_LIST; 
    } 
    first = last = make_cons(parse_expression(l), EMPTY_LIST);
    consume_white_space(l);
    while (peek_char(l) != closing) {
        R_cons next = make_cons(parse_expression(l), EMPTY_LIST);
        last->cdr = (GVMT_Object)next;
        last = next;
        consume_white_space(l);
    }
    get_char(l);
    return (GVMT_Object)first;
}

GVMT_Object syntax_error(char* msg, struct lexer *l) {
    int len = strlen(msg);
    R_cons except;
    consume_line(l);
    except = make_cons((GVMT_Object)make_symbol(msg, msg+len, l->line_number), NULL);
    gvmt_raise((GVMT_Object)except);
    return 0;
}

/** recursive decent expression parser */
GVMT_Object parse_expression(struct lexer *l) {
    int in;
    char* ptr = 0;
    GVMT_Object result;
    char buf[200];
    int neg = 0;
    consume_white_space(l);
    in = peek_char(l);
    if (in == '(' || in == '[') {
        return parse_list(l);
    }
    if (in == '\'') {
        get_char(l);
        result = parse_expression(l);
        return (GVMT_Object)make_cons(QUOTE, (GVMT_Object)make_cons(result, EMPTY_LIST));
    }
    if (in == '"') {
        return parse_string(l);
    }
    if (in == '`') {
        get_char(l);
        result = parse_expression(l);
        return (GVMT_Object)make_cons(QUASI_QUOTE, (GVMT_Object)make_cons(result, EMPTY_LIST));
    }
    if (in == ',') {
        get_char(l);
        result = parse_expression(l);
        return (GVMT_Object)make_cons(UNQUOTE, (GVMT_Object)make_cons(result, EMPTY_LIST));
    }
    in = peek_char(l);
    if (is_eof(in)) {
        sprintf(buf, "Expected symbol, number, '(', '[', '#' or quote. Found end-of-file", in);
        syntax_error(buf, l);
    } else if (in == '#') {
        get_char(l);
        in = peek_char(l);
        if (in == '(' || in == '[')
            return list_to_vector(parse_list(l));
        ptr = buf + 1;
        buf[0] = '#';
    } else if (special_chars[in]) {
        sprintf(buf, "Expected symbol, integer, '(', '[', '#' or quote. Found '%c'", in);
        syntax_error(buf, l);
    } else {
        ptr = buf;
    }
    if (in == '-' || in == '.') {
        *ptr = in;
        ptr++;
        get_char(l);
        in = peek_char(l);
    }
    if (in >= '0' && in <= '9') {
        do {    
            *ptr = get_char(l);
            ptr++;
            in = peek_char(l);
        } while (in >= '0' && in <= '9');
        if (in == '.' && buf[0] != '.' || in == 'e' || in == 'E')
              return parse_fraction(buf, ptr, l);
        if (ptr - buf > 8 && buf[0] != '.') {
            syntax_error("Integer too large", l);
        }
        if (buf[0] == '.')
            return floating_point(buf, ptr);
        else 
            return integer(buf, ptr);
    } 
    while (special_chars[in] == 0 && !is_eof(in)) {    
        *ptr = get_char(l);
        ptr++;
        in = peek_char(l);
    }  
    return make_symbol(buf, ptr, l->line_number);
}

/** Parse input from lexer */
GVMT_Object parse_input(struct lexer *l) {
    GVMT_Object result = parse_expression(l);
    consume_white_space(l);
    if (peek_char(l))
        syntax_error("Expected end-of-input", l);
    return result;
}

