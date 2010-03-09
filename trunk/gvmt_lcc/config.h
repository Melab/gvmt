typedef struct {
    unsigned int bits;
} small_set;

typedef struct {
    int type;
} Xinterface;

typedef struct {
    int offset;
    unsigned freemask[2];
} Env;

typedef struct {
    small_set type_set;
    int exact_type;
    int centre;
    char *error;
    Node link;
    Type ptype;
//    int offset;
    char eliminated;
    char native;
} Xnode;

typedef struct {
    char *name;
    Node cse;
    Type ptype;
    char type;
    char export;
    char named;
    char allocated;
    char emitted;
} Xsymbol;

