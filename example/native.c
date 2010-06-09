#include <stdlib.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <assert.h>
#include "gvmt/native.h"
#include "opcodes.h"
#include "example.h"

/** Native code, that is code that should be compiled with the native C compiler,
 * eg. GCC, rather than gvmtc */
 
 
/* The shape function for GVMT Garbage Collection.
 * It is allowed to use internal pointer as the object will not
 * be moved during this call */
GVMT_CALL int* gvmt_shape(GVMT_Object obj, int* buffer) {
    struct type* type = ((R_cons)obj)->type;
    if (type == &type_symbol || type == &type_string) {
        buffer[0] = -3;
        buffer[1] = -((((R_symbol)obj)->length + 3)/4);
        buffer[2] = 0;
        return buffer;
    } else if (type == &type_frame || type == &type_vector) {
        buffer[0] = -2;
        buffer[1] = ((R_frame)obj)->size;
        buffer[2] = 0;
        return buffer;
    } else {
        return ((R_cons)obj)->type->shape;
    }
}


extern int test_lexer(void);
extern signed char fib[];
int print_expression = 0;
int disassemble = 0;
int jit_compile = 1;
int flags_for_lib = 0;
int tracing_on = 0;

// 64 K stack space
#define STACK_SPACE 1 << 16

/** Parse command line args and start VM */
int main(int argc, char** argv) {
    int i;
    char* program_name = 0;
    int show_collections = 0;
    gvmt_warn_on_unexpected_parameter_usage = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0)
            tracing_on = 1;
        else if (strcmp(argv[i], "-j") == 0)
            jit_compile = 0;
        else if (strcmp(argv[i], "-h") == 0) {
            printf("-t Turn on tracing\n");
            printf("-h Show this help and exit\n");
            printf("-p Print parser output before evaluating\n");
            printf("-d Print disassembled bytecode before evaluating\n");
            printf("-j No-JIT. Interpreter only\n");
            printf("-G Show number and times of garbage collection\n");
            return 0;
        } else if (strcmp(argv[i], "-p") == 0)
            print_expression = 1;
        else if (strcmp(argv[i], "-d") == 0)
            disassemble = 1;
        else if (strcmp(argv[i], "-X") == 0)
            flags_for_lib = 1;
        else if (strcmp(argv[i], "-G") == 0)
            show_collections = 1;
        else {
            program_name = argv[i];
            argc -= i;
            argv += i;
            break;
        }
    }
    GVMT_MAX_SHAPE_SIZE = 4;
    if (program_name) {
        gvmt_start_machine(STACK_SPACE, (gvmt_func_ptr)run_program, 3, program_name, argc, argv);
    } else {
        gvmt_start_machine(STACK_SPACE, (gvmt_func_ptr)read_eval_print_loop, 0, 0);  
    }
    if (show_collections) {
        printf("%d minor collections in %f ms\n", gvmt_minor_collections, gvmt_minor_collection_time/1000000.0);
        printf("%d major collections in %f ms\n", gvmt_major_collections, gvmt_major_collection_time/1000000.0);
        printf("Total collection time: %f ms\n", gvmt_total_collection_time/1000000.0);
    }
    return 0;
}

void user_gc_out_of_memory(void *mem_block) {
    exit(-1);   
}

char* read_whole_file(char* filename) {
    struct stat status;
    int fd, count;
    unsigned char* bytes;
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Cannot read %s\n", filename);
        exit(1);   
    }
    fstat(fd, &status);
    bytes = malloc(status.st_size+1);
    count = read(fd, bytes, status.st_size);
    if (count < status.st_size) {
        fprintf(stderr, "Can only read %s partially\n", filename);
        exit(1);
    }
    bytes[status.st_size] = 0;
    return bytes;
}

/** Simple Lexer stuff */
static void _init_lexer(struct lexer *l, FILE* f) {
    l->source = f;
    l->pre_fetched = 0;
    l->line_number = 1; 
}

void init_lexer(struct lexer *l, char* filename) {
    FILE* f = fopen(filename, "r");
    if (f == 0) {
        fprintf(stderr, "Cannot open '%s' for reading\n", filename);
        exit(1);
    }
    _init_lexer(l, f);
}

void terminal_lexer(struct lexer *l) {
    printf("> ");
    _init_lexer(l, stdin);
}

static int fetch_char(struct lexer *l) {
    int c = getc(l->source);
    if (c == '\n' && l->source == stdin)
        printf("> ");
    return c;
}

int get_char(struct lexer *l) {
    if (l->pre_fetched) {
        l->pre_fetched = 0;
        return l->next_char;
    } else {
        return fetch_char(l);  
    }
}

int peek_char(struct lexer *l) {
    if (!l->pre_fetched) {
        l->next_char = fetch_char(l);
        l->pre_fetched = 1;
    }
    return l->next_char;
}

void consume_white_space(struct lexer *l) {
    char in = peek_char(l);
    int in_comment;
    do {
        in_comment = 0;
        while(in == ' ' || in == '\t' || in == '\n') {
            if (in == '\n') {
               l->line_number++;   
            }
            get_char(l);
            in = peek_char(l);
        }
        if (in == ';') {
            do {
                get_char(l);
                in = peek_char(l);
            } while (!is_eof(in) && in != '\n');
            in_comment = 1;
        }
    } while (in_comment);
}

void consume_line(struct lexer *l) {
    char in = get_char(l);
    while(!is_eof(in) && in != '\n') {
       in = get_char(l);    
    }
}

char special_chars[256];
static char* special_characters = "()[]`',#{}\"\n\t ;\0";

void init_parser(void) {
    int i;
    for (i = 0; i < 256;i++)
        special_chars[i] = 0;
    for(i = 0; special_characters[i]; i++)
        special_chars[special_characters[i]] = 1;
}

