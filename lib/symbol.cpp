#include <dlfcn.h>
#include "gvmt/internal/core.h"
#include <vector>

std::vector<char*> _gvmt_symbol_name_vector;
std::vector<void*> _gvmt_symbol_vector;

extern "C" {
    
void* _gvmt_global_symbols;
char cant_resolve[] = "Cannot resolve symbol: %s";

uint16_t gvmt_symbol_id(char* symbol) {
    uint16_t result = _gvmt_symbol_name_vector.size();
    int len = strlen(symbol) + 1;
    char* copy = (char*)malloc(len);
    memcpy(copy, symbol, len);
    _gvmt_symbol_vector.push_back(0);
    _gvmt_symbol_name_vector.push_back(copy);
    return result;
}

void* _gvmt_get_symbol(uint16_t index) {
    void* sym = _gvmt_symbol_vector[index];
    if (sym == 0) {
        char* name = _gvmt_symbol_name_vector[index];
        sym = _gvmt_symbol_vector[index] = dlsym(_gvmt_global_symbols, name);
        if (sym == 0)
            __gvmt_fatal(cant_resolve, name);
    }
    return sym;
}

char* _gvmt_get_symbol_name(uint16_t index) {
    return _gvmt_symbol_name_vector[index];
}
    
}


