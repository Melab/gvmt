#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gvmt/gvmt.h>
#include "example.h"
#include "opcodes.h"

/** Helper functions for environments.
 *  Environments are implemented as lists.
 *  Hash tables would be better (faster)
 */

int symbol_eqs(R_symbol s1, R_symbol s2) {
    int i;
    if (s1->length != s2->length)
        return 0;
    for (i = 0; i < s1->length; i++)
        if (s1->text[i] != s2->text[i])
            return 0;
    return 1;
}

static R_environment make_environment(struct type *var_type, R_environment enclosing, int new_literals) {
    R_environment e = (R_environment)gvmt_malloc(sizeof(GVMT_OBJECT(environment)));
    struct type *t = &type_environment;
    e->type = t;
    e->enclosing = enclosing;
    e->var_type = var_type;
    e->size = 0;
    e->variables = (R_cons)EMPTY_LIST;
    if (new_literals) {
        GVMT_Object first_item;
        e->literals = make_cons((GVMT_Object)make_cons(box(0), box(0)), EMPTY_LIST);
    } else {
        e->literals = enclosing->literals;
    }
    return e;
}


R_environment top_level_environment(void) {
    return make_environment(&type_global_variable, NULL, 1);
}

R_environment local_environment(R_environment enclosing) {
    return make_environment(&type_local_variable, enclosing, 1);
}

R_environment let_environment(R_environment enclosing) {
    return make_environment(&type_local_variable, enclosing, 0);
}

R_symbol as_symbol(GVMT_Object o) {
    R_symbol s = (R_symbol)o;
    struct type *t = &type_symbol;
    if (gvmt_is_tagged(o) || s->type != t) {
        error("Must be a symbol: ", o);
    }
    return s;
}

static R_cons find_cell(R_cons list, R_symbol name) {
    R_cons cell;
    R_symbol c_name;
    while (list != (R_cons)EMPTY_LIST) {
        cell = (R_cons)list->car;
        c_name = (R_symbol)cell->car;
        if (symbol_eqs(name, c_name)) {
            return cell;
        }
        list = (R_cons)list->cdr;
    }
    return 0;
}

void bind_s(R_environment env, R_symbol name, GVMT_Object value) {
    R_cons cell = find_cell(env->variables, name);
    if (cell) {
        cell->cdr = value;
    } else {
        R_cons list;
        cell = make_cons((GVMT_Object)name, (GVMT_Object)value);
        list = make_cons((GVMT_Object)cell, (GVMT_Object)env->variables);
        env->variables = list;
    }
}

void bind(R_environment env, char* name, GVMT_Object value) {  
    int len = strlen(name);
    R_symbol s = (R_symbol)make_symbol(name, name+len, 0);
    bind_s(env, s, value);
}

R_variable new_variable(R_environment env, R_symbol name) {
    struct type *t = env->var_type;
    R_variable v = (R_variable)gvmt_malloc(sizeof(GVMT_OBJECT(variable)));
    v->type = t;
    env->size++;
    v->index = env->size;
    v->env = env;
    bind_s(env, name, (GVMT_Object)v);
    return v;
}

R_variable lookup(R_environment env, R_symbol name) {
    R_environment search = env;
    R_variable v;
    GVMT_Object o;
    while (search) {
        R_cons cell = find_cell(search->variables, name); 
        if (cell)
            return (R_variable)cell->cdr;
        search = search->enclosing;
    }
    v = new_variable(global_environment, name);
    // Create a new "Undefined variable object with name.
    // Then use literal to access that object...
    o = globals->values[v->index];
    if (o == NULL)
        globals->values[v->index] = undefined_variable(name);
    return v;
} 

void error(char* msg, GVMT_Object object) {
    int len = strlen(msg);
    GVMT_Object text = (GVMT_Object)make_symbol(msg, msg+len, 0);
    R_cons pair = make_cons(text, object);
    gvmt_raise((GVMT_Object)pair);
}

void symbol_to_buffer(R_symbol s, char* buffer) {
    int i;
    for (i = 0; i < s->length; i++) {
        buffer[i] = s->text[i];   
    }
    buffer[i] = 0;
}

R_frame make_literals(R_environment env) {
    int i, size = list_len((GVMT_Object)env->literals);
    R_frame f = make_vector(size);
    GVMT_Object list = (GVMT_Object)env->literals;
    while (list != EMPTY_LIST) {
        R_cons pair = (R_cons)head(list); 
        f->values[unbox(pair->cdr)] = pair->car;
        list = tail(list);
    }
    return f;
}

int literal_index(R_environment env, GVMT_Object literal) {
    R_cons pair; 
    GVMT_Object list = (GVMT_Object)env->literals;
    int index;
    while (list != EMPTY_LIST) {
        pair = (R_cons)head(list);   
        if (pair->car == literal) {
            return unbox(pair->cdr);
        }
        list = tail(list);
    }
    index = list_len((GVMT_Object)env->literals);
    append((GVMT_Object)env->literals, 
           (GVMT_Object)make_cons(literal, box(index)));
    return index;
}

void store_variable(R_variable v, R_bytes b) {
    struct type *t = v->type;
    if (t == &type_global_variable) {
        bytes_append(b, op(store_global)); 
        bytes_append(b, v->index>>8);
        bytes_append(b, v->index&0xff);
    } else if (t == &type_local_variable) {
        bytes_append(b, op(store_local)); 
        bytes_append(b, v->index);
    } else {
        error("Internal Error, (not a variable): ", (GVMT_Object)v);   
    }
}

R_environment global_environment;

