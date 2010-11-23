#ifndef GVMT_COMPILER_INTERNAL_H
#define GVMT_COMPILER_INTERNAL_H

// LLVM says we must define this so we do. Don't know why!
#define __STDC_CONSTANT_MACROS

#include "gvmt/internal/core.h"
#include "gvmt/internal/compiler_shared.h"
#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/PassManager.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Support/IRBuilder.h>
#include "llvm/CallingConv.h"
#include "llvm/ExecutionEngine/JIT.h"
#include <map>
#include <deque>
#include <utility>


class CompilerStack {
    std::deque<llvm::Value*> stack;
    /** Ensure that count values are cached at compile-time */
    void ensure_cache(int count, llvm::BasicBlock* bb);
    void push_memory(llvm::Value* val, llvm::BasicBlock* bb);
    llvm::Value* pop_memory(const llvm::Type* type, llvm::BasicBlock* bb);
    llvm::Value* LLVM_MEMSET;
    std::vector<llvm::Value*> join_cache;
    std::vector<llvm::Type*> join_types;
//    llvm::Value* stack_pointer;
    llvm::Value* stack_pointer_addr;
//    int stack_offset;
//    void save_pointer(llvm::BasicBlock* bb);
    void modify_pointer(int diff, llvm::BasicBlock* bb);
    void modify_pointer(llvm::Value* diff, int sign, llvm::BasicBlock* bb);
//    bool modified;
//    void load_pointer(llvm::BasicBlock* bb);
  public: 
    CompilerStack(llvm::Module* mod);
    void push(llvm::Value* val);
    llvm::Value* pop(const llvm::Type* type, llvm::BasicBlock* bb);
    llvm::Value* top(llvm::BasicBlock* bb, int cached=0);
    void flush(llvm::BasicBlock* bb);
    void join(llvm::BasicBlock* bb);
    void start_block(llvm::BasicBlock* bb);
    void drop(int offset, llvm::Value* count,llvm::BasicBlock* bb);
    llvm::Value* insert(int offset, llvm::Value* count, llvm::BasicBlock* bb);
    llvm::Value* pick(const llvm::Type* type, llvm::BasicBlock* bb, unsigned index);
    void poke(llvm::BasicBlock* bb, unsigned index, llvm::Value* value);
    /** Although this is signed, it can be never be less than zero. */
    int join_depth;
    /** max_join_depth must be called a block that dominates all arguments to join.
     *  max must equal or exceed all future values of join_depth */
    void max_join_depth(unsigned max, llvm::BasicBlock* bb);
    llvm::Value* get_pointer(llvm::BasicBlock* bb);
    void store_pointer(llvm::Value* ptr, llvm::BasicBlock* bb);
};

class Architecture {
    static llvm::Function *create_and_push_handler(llvm::Module *mod);
    static llvm::Function *push_handler(llvm::Module *mod);
    static llvm::Function *pop_handler(llvm::Module *mod);
    static llvm::Function *no_args_func(std::string name, const llvm::Type* type, llvm::Module *mod);
    static llvm::Function *void_no_args_func(std::string name, llvm::Module *mod);
    static llvm::Function *raise_exception(llvm::Module *mod);
    static llvm::Function *transfer(llvm::Module *mod);
    static llvm::Function *set_jump(llvm::Module *mod);
    static llvm::Function *pin(llvm::Module *mod);
  public:
    static void init(llvm::Module *mod);
    static llvm::Value *WORD_SIZE;
    static llvm::Value *CURRENT_FRAME;
    static llvm::Function *PIN;
    static llvm::Function *ENTER_NATIVE;
    static llvm::Function *EXIT_NATIVE;
    static llvm::Function *GC_SAFE_POINT;
    static llvm::Function *POP_HANDLER;
    static llvm::Function *PUSH_HANDLER;
    static llvm::Function *POP_AND_FREE_HANDLER;
    static llvm::Function *CREATE_AND_PUSH_HANDLER;
    static llvm::Function *RAISE_EXCEPTION;
    static llvm::Function *TRANSFER;
    static llvm::Function *SET_JUMP;
    static llvm::FunctionType* FUNCTION_TYPE;
    static llvm::PointerType* POINTER_FUNCTION_TYPE;
};

// Toolkit generated class
class Globals;

class BaseCompiler {
    
    inline bool is_jit(void) { return jitting; }
  protected:
    
    llvm::Function* CHECK_FRAME;
    
    static llvm::Value *NO_ARGS[1];
    static llvm::Value *ZERO;
    int bb_index;
    llvm::BasicBlock* makeBB(std::string name, int index=0);
    llvm::Function* GC_MALLOC_FUNC;
    llvm::Function* GC_MALLOC_FAST_FUNC;
    llvm::Module* module;
    void initialiseFrame(llvm::Value* caller_frame, int number_of_locals, int ref_count, llvm::BasicBlock* bb);
    llvm::Value* gc_read(llvm::Value* object, llvm::Value* offset, llvm::BasicBlock* bb);
    void gc_write(llvm::Value* object, llvm::Value* offset, llvm::Value* value, llvm::BasicBlock* bb);
    llvm::Value* gc_malloc(llvm::Value* size, llvm::BasicBlock* bb);
    llvm::Value* gc_malloc_fast(llvm::Value* size, llvm::BasicBlock* bb);
    llvm::Value* push_current_state(llvm::BasicBlock* bb);
    llvm::Value* pop_state(llvm::BasicBlock* bb);
    void push_state(llvm::Value* handler, llvm::BasicBlock* bb);
    llvm::Value* ref_temp(int index, llvm::BasicBlock* bb);
    void zero_memory(llvm::Value* object, llvm::Value* size, llvm::BasicBlock* bb);
    void lock(llvm::Value* lock, llvm::BasicBlock* bb);
    void unlock(llvm::Value* lock, llvm::BasicBlock* bb);
    int ref_temps_base;
    llvm::Value *GC_SAFE;
    llvm::Function *PRINT_STACK;
    llvm::ExecutionEngine* execution_engine;
    llvm::ModuleProvider* module_provider;
    llvm::FunctionPassManager* func_pass_manager;
    BaseCompiler(void);
    /** llvm::Variables local to each function */
    llvm::Function *current_function;
    llvm::BasicBlock *current_block;
    llvm::BasicBlock *start_block; 
    bool unterminated_block;
    CompilerStack *stack;
    llvm::Value *FRAME;
    uint8_t *IP;
    uint8_t *ip_start;
    std::map<int, llvm::BasicBlock*> block_map;
    bool jitting;
    Globals *globals;
    int stack_cache_size;
    void emit_print(int x, llvm::BasicBlock* bb);
  public:
    static llvm::Value* save_and_restore(llvm::Value* from, 
                            const llvm::Type* to, llvm::BasicBlock* bb);
    static void store(llvm::Value* val, llvm::Value* ptr, llvm::BasicBlock* bb);
    static llvm::Value* cast_to_I4(llvm::Value* from, llvm::BasicBlock* bb);
    static llvm::Value* cast_to_F4(llvm::Value* from, llvm::BasicBlock* bb);
    static llvm::Value* cast_to_I8(llvm::Value* from, llvm::BasicBlock* bb);
    static llvm::Value* cast_to_F8(llvm::Value* from, llvm::BasicBlock* bb);
    static llvm::Value* cast_to_P(llvm::Value* from, llvm::BasicBlock* bb);
    static llvm::Value* cast_to_R(llvm::Value* from, llvm::BasicBlock* bb);
    static llvm::Value* cast_to_B(llvm::Value* from, llvm::BasicBlock* bb);
    static llvm::Value* load_laddr(llvm::Value* addr, llvm::BasicBlock* bb);
    
    static const llvm::Type* TYPE_B;
    static const llvm::Type* TYPE_I1;
    static const llvm::Type* TYPE_I2;
    static const llvm::Type* TYPE_I4;
    static const llvm::Type* TYPE_I8;
    static const llvm::Type* TYPE_U1;
    static const llvm::Type* TYPE_U2;
    static const llvm::Type* TYPE_U4;
    static const llvm::Type* TYPE_U8;
    static const llvm::Type* TYPE_IPTR; 
    static const llvm::Type* TYPE_F4;
    static const llvm::Type* TYPE_F8;
    static llvm::PointerType* TYPE_P; 
    static llvm::PointerType* TYPE_R; 
    static const llvm::Type* TYPE_V; 
    static const llvm::Type* TYPE_X;
    
    static llvm::PointerType* POINTER_TYPE_I1;
    static llvm::PointerType* POINTER_TYPE_I2;
    static llvm::PointerType* POINTER_TYPE_I4;
    static llvm::PointerType* POINTER_TYPE_I8;
    static llvm::PointerType* POINTER_TYPE_U1;
    static llvm::PointerType* POINTER_TYPE_U2;
    static llvm::PointerType* POINTER_TYPE_U4;
    static llvm::PointerType* POINTER_TYPE_U8;
    static llvm::PointerType* POINTER_TYPE_F4;
    static llvm::PointerType* POINTER_TYPE_F8;
    static llvm::PointerType* POINTER_TYPE_P;
    static llvm::PointerType* POINTER_TYPE_R;
    static llvm::PointerType* POINTER_TYPE_X;
    llvm::Function* compile(GVMT_Object function, int ret_type, char* name, bool jit = false);
    void write_ir(void);
    void* jit_compile(llvm::Function *function, char* name);
    
};

#endif
