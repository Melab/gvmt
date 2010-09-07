#include "gvmt/internal/core.h"
#include "gvmt/internal/compiler.hpp"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ModuleProvider.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Target/TargetData.h"
//#include "llvm/Target/TargetSelect.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/Passes.h"


using namespace llvm;
const Type* BaseCompiler::TYPE_B;
const Type* BaseCompiler::TYPE_I1;
const Type* BaseCompiler::TYPE_I2;
const Type* BaseCompiler::TYPE_I4;
const Type* BaseCompiler::TYPE_I8;
             
const Type* BaseCompiler::TYPE_U1;
const Type* BaseCompiler::TYPE_U2;
const Type* BaseCompiler::TYPE_U4;
const Type* BaseCompiler::TYPE_U8;

const Type* BaseCompiler::TYPE_X;

const Type* BaseCompiler::TYPE_IPTR;

const Type* BaseCompiler::TYPE_F4;
const Type* BaseCompiler::TYPE_F8;
            
llvm::PointerType* BaseCompiler::TYPE_P; 
llvm::PointerType* BaseCompiler::TYPE_R; 
const Type* BaseCompiler::TYPE_V; 

PointerType* BaseCompiler::POINTER_TYPE_I1;
PointerType* BaseCompiler::POINTER_TYPE_I2;
PointerType* BaseCompiler::POINTER_TYPE_I4;
PointerType* BaseCompiler::POINTER_TYPE_I8;
PointerType* BaseCompiler::POINTER_TYPE_X;
                                   
PointerType* BaseCompiler::POINTER_TYPE_U1;
PointerType* BaseCompiler::POINTER_TYPE_U2;
PointerType* BaseCompiler::POINTER_TYPE_U4;
PointerType* BaseCompiler::POINTER_TYPE_U8;
                                     
PointerType* BaseCompiler::POINTER_TYPE_F4;
PointerType* BaseCompiler::POINTER_TYPE_F8;
PointerType* BaseCompiler::POINTER_TYPE_P;
PointerType* BaseCompiler::POINTER_TYPE_R;

                                                    
Value *BaseCompiler::NO_ARGS[] = { 0 };
Value *BaseCompiler::ZERO;

Value *Architecture::WORD_SIZE;
Function *Architecture::POP_AND_FREE_HANDLER;
Function *Architecture::CREATE_AND_PUSH_HANDLER;
Function *Architecture::POP_HANDLER;
Function *Architecture::PUSH_HANDLER;
Function *Architecture::RAISE_EXCEPTION;
Function *Architecture::TRANSFER;
Function *Architecture::ENTER_NATIVE;
Function *Architecture::EXIT_NATIVE;
Function *Architecture::SET_JUMP;
Function *Architecture::GC_SAFE_POINT;
FunctionType *Architecture::FUNCTION_TYPE;
PointerType *Architecture::POINTER_FUNCTION_TYPE;

CompilerStack::CompilerStack(Module *mod) {
    LLVM_MEMSET = mod->getOrInsertFunction("llvm.memset.i32", BaseCompiler::TYPE_V, BaseCompiler::TYPE_P, BaseCompiler::TYPE_I1, BaseCompiler::TYPE_I4, BaseCompiler::TYPE_I4, NULL);
    assert(LLVM_MEMSET);
//    stack_pointer = 0;
//    stack_offset = 0;
    stack_pointer_addr = 0;
//    modified = false;
}

///** Primitives for load and store
// * These should be changed when llvm JIT supports TLS */ 
//void CompilerStack::load_pointer(BasicBlock* bb) {
//    assert(!modified);
//    assert(stack_pointer_addr);
//    stack_pointer = new LoadInst(stack_pointer_addr, "sp", bb);
//}

void CompilerStack::store_pointer(Value* ptr, BasicBlock* bb) {
    if (stack_pointer_addr == 0) {
        Value* one = ConstantInt::get(APInt(32, 1, true));
        stack_pointer_addr = new AllocaInst(BaseCompiler::POINTER_TYPE_X, one, "gvmt_sp_addr", bb); 
    }
    BaseCompiler::store(ptr, stack_pointer_addr, bb);
//    stack_offset = 0;
//    stack_pointer = ptr;
//    modified = false;
}

Value* CompilerStack::get_pointer(BasicBlock* bb) {
    return new LoadInst(stack_pointer_addr, "sp", bb);
//    if (stack_pointer == 0) {
//        assert(!modified);
//        load_pointer(bb);
//    }
//    if (stack_offset == 0) {
//        return stack_pointer;
//    } else {
//        ConstantInt* i = ConstantInt::get(APInt(32, stack_offset, true));
//        return GetElementPtrInst::Create(stack_pointer, i, "gvmt_sp", bb);
//    }
}

//void CompilerStack::save_pointer(BasicBlock* bb) {
//    if (modified || stack_offset != 0) {
//        if (stack_pointer == 0) {
//            load_pointer(bb);
//        }
//        if (stack_offset != 0) {
//            ConstantInt* i = ConstantInt::get(APInt(32, stack_offset, true));
//            stack_pointer = GetElementPtrInst::Create(stack_pointer, i, "x", bb);
//        }
//        store_pointer(stack_pointer, bb);
//    }
//}

void CompilerStack::modify_pointer(Value* diff, int sign, BasicBlock* bb) {
    Value* stack_pointer = get_pointer(bb);
    if (sign > 0) {
        stack_pointer = GetElementPtrInst::Create(stack_pointer, diff, "x", bb);
    } else {
        diff = BinaryOperator::CreateNeg(diff, "x", bb);
        stack_pointer = GetElementPtrInst::Create(stack_pointer, diff, "x", bb);
    }
    store_pointer(stack_pointer, bb);
}

void CompilerStack::modify_pointer(int d, BasicBlock* bb) {
    Value* diff = ConstantInt::get(APInt(32, d, true));
    modify_pointer(diff, 1, bb);
}
//    ConstantInt* i = dyn_cast<ConstantInt>(diff);
//    if (i) {
//        stack_offset += (sign * (int32_t)i->getValue().getSExtValue());
//    } else {
//        if (stack_pointer == 0) {
//            load_pointer(bb);
//        }
//        if (stack_offset) {
//            ConstantInt* i = ConstantInt::get(APInt(32, stack_offset, true));
//            stack_pointer = GetElementPtrInst::Create(stack_pointer, i, "x", bb);
//        }
//        if (sign > 0) {
//            stack_pointer = GetElementPtrInst::Create(stack_pointer, diff, "x", bb);
//        } else {
//            diff = BinaryOperator::CreateNeg(diff, "x", bb);
//            stack_pointer = GetElementPtrInst::Create(stack_pointer, diff, "x", bb);
//        }
//        stack_offset = 0; 
//        modified = true;
//    }
//}

void CompilerStack::push(Value* val) {
    stack.push_back(val);
}

Value* CompilerStack::pop(const Type* t, BasicBlock* bb) {
    Value* result;
    if (stack.size()) {
        result = stack.back();
        stack.pop_back();
        return result;
    } else {
        return pop_memory(t, bb);
    }
}

Value* CompilerStack::pick(const Type* type, BasicBlock* bb, unsigned depth) {
    assert(depth >= 0);
    if (stack.size() > depth) {
        return stack[stack.size()-1-depth];
    } else {
        Value* top = get_pointer(bb);
        Value* ptr = GetElementPtrInst::Create(top, ConstantInt::get(APInt(32, depth-stack.size(), true)), "x", bb);
        if (PointerType::get(type, 0) != ptr->getType()) {
            ptr = new BitCastInst(ptr, PointerType::get(type, 0), "p", bb);
        }
        LoadInst* result = new LoadInst(ptr, "val", bb);
        return result;
    }
}

void CompilerStack::poke(BasicBlock* bb, unsigned depth, Value* val) {
    assert(depth >= 0);
    if (stack.size() > depth) {
        stack[stack.size()-1-depth] = val;
    } else {
        Value* top = get_pointer(bb);
        Value* ptr = GetElementPtrInst::Create(top, ConstantInt::get(APInt(32, depth-stack.size(), true)), "x", bb);
        BaseCompiler::store(val, ptr, bb);
    }
}
 
llvm::Value* CompilerStack::top(BasicBlock* bb, int cached) {
    Value* top = get_pointer(bb);
    int offset = cached + stack.size();
    if (offset) {
        top = GetElementPtrInst::Create(top, ConstantInt::get(APInt(32, -offset, true)), "x", bb);
    }
    return top;
}

llvm::Value* CompilerStack::pop_memory(const Type* t, BasicBlock* bb) {
    Value* top = get_pointer(bb);
    top = new BitCastInst(top, PointerType::get(t, 0), "valp", bb);
    LoadInst* result = new LoadInst(top, "val", bb);
    modify_pointer(1, bb);
    return result;
}

void CompilerStack::push_memory(llvm::Value* val, BasicBlock* bb) {
    modify_pointer(-1, bb);
    Value* top = get_pointer(bb);
    top = new BitCastInst(top, PointerType::get(val->getType(), 0), "valp", bb);
    BaseCompiler::store(val, top, bb);
}

void CompilerStack::flush(BasicBlock* bb) {
    ensure_cache(0, bb);  
//    save_pointer(bb);
//    assert(!modified);
//    stack_pointer = 0;
}

void CompilerStack::max_join_depth(unsigned max, BasicBlock* bb) {
    Constant* one = ConstantInt::get(APInt(32, 1, true));
    while(max > join_cache.size()) {
        join_cache.push_back(new AllocaInst(BaseCompiler::TYPE_X, one, "join", bb));
        join_types.push_back(BaseCompiler::POINTER_TYPE_X);
    }
}

void CompilerStack::join(BasicBlock* bb) {
    assert(join_depth >= 0);
    assert(join_cache.size() >= (unsigned)join_depth);
    ensure_cache(join_depth, bb);
//    save_pointer(bb);
//    assert(!modified);
//    stack_pointer = 0;
    int n = join_depth;
    while (n) {
        Value* v = stack.front();
        stack.pop_front();
        n--;
        Type* ptr_type = PointerType::get(v->getType(), 0);
        Value* ptr = new BitCastInst(join_cache[n], ptr_type, "p", bb);
        BaseCompiler::store(v, ptr, bb);
        join_types[n] = ptr_type;
    }
}

void CompilerStack::start_block(BasicBlock* bb) {
    assert(stack.size() == 0);
    assert(join_depth >= 0);
    for(int n = 0; n < join_depth; n++) {
        Value* ptr = new BitCastInst(join_cache[n], join_types[n], "p", bb);
        stack.push_front(new LoadInst(ptr, "", bb));
    }    
}

void CompilerStack::ensure_cache(int count, BasicBlock* bb) {
    int n = stack.size();
    while (n > count) {
        push_memory(stack.front(), bb);
        stack.pop_front();
        n--;
    }
    while (n < count) {
        stack.push_front(pop_memory(BaseCompiler::TYPE_X, bb));
        n++;
    }
}

void CompilerStack::drop(int offset, llvm::Value* count, BasicBlock* bb) {
    ensure_cache(offset, bb);
    // Increment runtime SP.
    modify_pointer(count, 1, bb);
}

Value* CompilerStack::insert(int offset, llvm::Value* count, BasicBlock* bb) {
    ensure_cache(offset, bb);
    modify_pointer(count, -1, bb);
//    save_pointer(bb);
    Value* ptr = get_pointer(bb);
    // Also need to zero all new items..
    ptr = new BitCastInst(ptr, BaseCompiler::TYPE_P, "x", bb);
    Value* bytes = BinaryOperator::Create(Instruction::Mul, count, Architecture::WORD_SIZE, "", bb);
    Value *params[] = { ptr, ConstantInt::get(APInt(8, 0, true)), bytes, Architecture::WORD_SIZE };
//  TO DO - Should just emit writes directly for small constant sizes.
    CallInst::Create(LLVM_MEMSET, &params[0], &params[4], "", bb);
    return ptr;
}

void Architecture::init(Module *module) {
    EXIT_NATIVE = Architecture::void_no_args_func("gvmt_exit_native", module);
    ENTER_NATIVE = Architecture::void_no_args_func("gvmt_enter_native", module);
    GC_SAFE_POINT = Architecture::void_no_args_func("gvmt_gc_safe_point", module);
    POP_HANDLER = Architecture::void_no_args_func("gvmt_pop_handler", module);
    PUSH_HANDLER = Architecture::push_handler(module);
    POP_AND_FREE_HANDLER = Architecture::void_no_args_func("gvmt_pop_and_free_handler", module);
    CREATE_AND_PUSH_HANDLER = Architecture::create_and_push_handler(module);
    SET_JUMP = Architecture::set_jump(module);
    RAISE_EXCEPTION = Architecture::raise_exception(module);
    TRANSFER = Architecture::transfer(module);
    WORD_SIZE = ConstantInt::get(APInt(32, 4, true));
    std::vector<const llvm::Type*> cc;
    cc.push_back(BaseCompiler::POINTER_TYPE_X);
    cc.push_back(BaseCompiler::TYPE_P);
    FUNCTION_TYPE = FunctionType::get(BaseCompiler::POINTER_TYPE_X, cc, false);
    POINTER_FUNCTION_TYPE = PointerType::get(FUNCTION_TYPE, 0);
}

Function* Architecture::set_jump(Module *mod) {
    std::vector< const Type * >args;
    args.push_back(BaseCompiler::TYPE_P);
    args.push_back(BaseCompiler::TYPE_P);
    FunctionType * ftype = FunctionType::get(BaseCompiler::TYPE_I8, args, false);
    Function* f = Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_setjump", mod);
    f->setCallingConv(llvm::CallingConv::X86_FastCall);
    return f;
}

Function* Architecture::raise_exception(Module *mod) {
    std::vector< const Type * >args;
    args.push_back(BaseCompiler::TYPE_R);
    FunctionType * ftype = FunctionType::get(Type::VoidTy, args, false);
    Function* f = Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_raise_exception", mod);
    f->setCallingConv(llvm::CallingConv::X86_FastCall);
    f->setDoesNotReturn(true); 
    return f;
}

Function* Architecture::transfer(Module *mod) {
    std::vector< const Type * >args;
    args.push_back(BaseCompiler::TYPE_R);
    args.push_back(BaseCompiler::POINTER_TYPE_X);
    FunctionType * ftype = FunctionType::get(Type::VoidTy, args, false);
    Function* f = Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_transfer", mod);
    f->setCallingConv(llvm::CallingConv::X86_FastCall);
    f->setDoesNotReturn(true); 
    return f;
}

Function* Architecture::push_handler(Module *mod) {
    std::vector< const Type * >args;
    args.push_back(BaseCompiler::TYPE_P);
    FunctionType * ftype = FunctionType::get(Type::VoidTy, args, false);
    return Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_push_handler", mod);
}

Function* Architecture::pop_handler(Module *mod) {
    std::vector< const Type * >args;
    FunctionType * ftype = FunctionType::get(BaseCompiler::TYPE_P, args, false);
    return Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_pop_handler", mod);
}

Function* Architecture::create_and_push_handler(Module *mod) {
    std::vector< const Type * >args;
    FunctionType * ftype = FunctionType::get(BaseCompiler::TYPE_P, args, false);
    return Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_create_and_push_handler", mod);
}

Function* Architecture::void_no_args_func(std::string name, Module *mod) {
    std::vector< const Type * >args;
    FunctionType * ftype = FunctionType::get(Type::VoidTy, args, false);
    return Function::Create(ftype, GlobalValue::ExternalLinkage, name, mod);
}

BaseCompiler::BaseCompiler() {
    TYPE_B = IntegerType::get(1); 
    TYPE_I1 = IntegerType::get(8); 
    TYPE_I2 = IntegerType::get(16); 
    TYPE_I4 = IntegerType::get(32); 
    TYPE_I8 = IntegerType::get(64); 
    TYPE_X = IntegerType::get(sizeof(GVMT_StackItem)*8);
    TYPE_IPTR = IntegerType::get(8*sizeof(void*));
    ZERO = ConstantInt::get(APInt(32, 0, true));
                                  
    TYPE_U1 = TYPE_I1; 
    TYPE_U2 = TYPE_I2; 
    TYPE_U4 = TYPE_I4; 
    TYPE_U8 = TYPE_I8; 
           
    TYPE_F4 = Type::FloatTy;
    TYPE_F8 = Type::DoubleTy;
    TYPE_V = Type::VoidTy;

    POINTER_TYPE_I1 = PointerType::get(TYPE_I1, 0) ;
    POINTER_TYPE_I2 = PointerType::get(TYPE_I2, 0) ;
    POINTER_TYPE_I4 = PointerType::get(TYPE_I4, 0) ;
    POINTER_TYPE_I8 = PointerType::get(TYPE_I8, 0) ;
    POINTER_TYPE_X = PointerType::get(TYPE_X, 0) ;
  
    POINTER_TYPE_U1 = POINTER_TYPE_I1;
    POINTER_TYPE_U2 = POINTER_TYPE_I2;
    POINTER_TYPE_U4 = POINTER_TYPE_I4;
    POINTER_TYPE_U8 = POINTER_TYPE_I8;
                                                  
    POINTER_TYPE_F4 = PointerType::get(TYPE_F4, 0);
    POINTER_TYPE_F8 = PointerType::get(TYPE_F8, 0);

    TYPE_P = POINTER_TYPE_I1; 
    TYPE_R = POINTER_TYPE_I1;  /* Probably should change this to something different from TYPE_P */                                  

    POINTER_TYPE_P  = PointerType::get(TYPE_P, 0);
    POINTER_TYPE_R  = PointerType::get(TYPE_R, 0);
    bb_index = 0;
    module = new Module("gvmt_compiler_module");
    // FIX ME - Really shouldn't be hardwiring this in:
    module->setDataLayout("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:32:32");
    module->setTargetTriple("i386-pc-linux-gnu");
    std::vector<const Type*>args;
    FunctionType* ftype = FunctionType::get(TYPE_V, args, false);
    PRINT_STACK = Function::Create(ftype, GlobalValue::ExternalLinkage, "debug_print_stack", module);
    args.push_back(TYPE_R);
    ftype = FunctionType::get(TYPE_IPTR, args, false);
    Architecture::init(module);
    CHECK_FRAME = Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_check_frame", module);
    args.clear();
    args.push_back(TYPE_I4);    
    ftype = FunctionType::get(TYPE_R, args, false);
    GC_MALLOC_FAST_FUNC = Function::Create(ftype, GlobalValue::ExternalLinkage, "gvmt_fast_allocate", module);
    GC_MALLOC_FAST_FUNC->setCallingConv(CallingConv::X86_FastCall);
    execution_engine = 0;
    ref_temps_base = 0;
}

Value* BaseCompiler::ref_temp(int index, BasicBlock* bb) {
    Value* ref_offset = ConstantInt::get(APInt(32, offsetof(struct gvmt_frame, refs)));
    Value* refs = GetElementPtrInst::Create(FRAME, ref_offset, "", bb);
    refs = new BitCastInst(refs, POINTER_TYPE_R, "", bb);
    return GetElementPtrInst::Create(refs, ConstantInt::get(APInt(32, index+ref_temps_base)), "", bb);
}

void BaseCompiler::initialiseFrame(Value* caller_frame, int locals, 
                                   int count, BasicBlock* bb) {
    Value* frame_size = ConstantInt::get(APInt(32, locals*sizeof(void*)+sizeof(struct gvmt_frame)));
    Value* frame = new AllocaInst(TYPE_I1, frame_size, "frame", bb);
    Value* previous_offset = ConstantInt::get(APInt(32, offsetof(struct gvmt_frame, previous)));
    Value* frame_previous = new BitCastInst(GetElementPtrInst::Create(frame, previous_offset, "frame_previous", bb), POINTER_TYPE_P, "x", bb);
    store(caller_frame, frame_previous, bb);
    Value* count_offset = ConstantInt::get(APInt(32, offsetof(struct gvmt_frame, count)));
    Value* frame_count = new BitCastInst(GetElementPtrInst::Create(frame, count_offset, "x", bb), POINTER_TYPE_I4, "frame_count", bb);
    store(ConstantInt::get(APInt(32, count)), frame_count, bb);
    FRAME = frame;
    for (int i = 0; i < locals; i++) {
        store(ConstantPointerNull::get(TYPE_R), ref_temp(i, bb), bb);
    }    
//    Call to check_frame function - for debugging.
//    Value *params[] = { frame, top_frame };
//    CallInst::Create(CHECK_FRAME, &params[0], &params[2], "", bb);
}

extern "C" {
    void  gvmt_print_int(int x) {  
        fprintf(stderr, "%d\n", x);
    }
}

void BaseCompiler::emit_print(int x, BasicBlock* bb) {
    Constant *f = module->getOrInsertFunction("gvmt_print_int", TYPE_V, TYPE_I4, NULL);
    Value* args[] = { ConstantInt::get(APInt(32, x)) };
    CallInst::Create(f,  &args[0], &args[1], "", bb);
}

#define ELEMENT_OFFSET(s, f) ConstantInt::get(APInt(32, offsetof(struct s, f)))
#define ELEMENT_ADDR(str, fld, obj, blk) GetElementPtrInst::Create(obj, ELEMENT_OFFSET(str, fld), "x", blk)

Value* BaseCompiler::push_current_state(BasicBlock* bb) {
    Value* handler = CallInst::Create(Architecture::CREATE_AND_PUSH_HANDLER, &NO_ARGS[0], &NO_ARGS[0], "x", bb);
    Value* handler_sp = ELEMENT_ADDR(gvmt_exception_handler, sp, handler, bb);
    Value* sp = stack->get_pointer(bb);
    handler_sp = new BitCastInst(handler_sp, PointerType::get(sp->getType(), 0), "x", bb);
    store(sp, handler_sp, bb);
    Value* handler_registers = ELEMENT_ADDR(gvmt_exception_handler, registers, handler, bb);
    Value* args[] = { sp, handler_registers, 0 };
    CallInst* ret = CallInst::Create(Architecture::SET_JUMP, &args[0], &args[2], "x", bb);
    ret->setCallingConv(CallingConv::X86_FastCall);
    sp = new TruncInst(ret, TYPE_I4, "", current_block);
    Value* rsh = BinaryOperator::Create(Instruction::LShr, ret, ConstantInt::get(APInt(32, 32)), "", current_block);
    Value* ex = new TruncInst(rsh, TYPE_I4, "", current_block);
    stack->store_pointer(sp, bb);
    return ex;
}

Value* BaseCompiler::pop_state(BasicBlock* bb) {
    CallInst* pop = CallInst::Create(Architecture::POP_HANDLER, &NO_ARGS[0], 
                                     &NO_ARGS[0], "", current_block);
    pop->setCallingConv(CallingConv::X86_FastCall);
    return pop;
}

void BaseCompiler::push_state(Value* handler, BasicBlock* bb) {
    Value* args[] = { handler, 0 };
    CallInst::Create(Architecture::PUSH_HANDLER, &args[0], &args[1], 
        "", current_block)->setCallingConv(CallingConv::X86_FastCall);
}

Value* BaseCompiler::gc_read(Value* object, Value* offset, BasicBlock* bb) {
    Value* gep = GetElementPtrInst::Create(object, offset, "x", bb);
    return new LoadInst(new BitCastInst(gep, POINTER_TYPE_R, "x", bb), "x", true, bb);
}

void BaseCompiler::gc_write(Value* object, Value* offset, Value* value, BasicBlock* bb) {
    Value* gep = GetElementPtrInst::Create(object, offset, "x", bb);
    assert(value->getType() == TYPE_R);
    new StoreInst(value, new BitCastInst(gep, POINTER_TYPE_R, "x", bb), true, bb);
}

Value* BaseCompiler::gc_malloc(Value* size, BasicBlock* bb) {
    Value* params[] = {stack->get_pointer(bb), FRAME, size, 0};
    CallInst* c = CallInst::Create(GC_MALLOC_FUNC, &params[0], &params[3], "x", bb);
    return c;
}

Value* BaseCompiler::gc_malloc_fast(Value* size, BasicBlock* bb) {
    Value* params[] = { size, 0};
    CallInst* c = CallInst::Create(GC_MALLOC_FAST_FUNC, &params[0], &params[1], "x", bb);
    c->setCallingConv(CallingConv::X86_FastCall);
    return c;
}

void BaseCompiler::zero_memory(Value* object, Value* size, BasicBlock* bb) {
    Value *memset = module->getOrInsertFunction("llvm.memset.i32", TYPE_V, TYPE_P, TYPE_I1, TYPE_I4, TYPE_I4, NULL);
    Value *params[] = { object, ConstantInt::get(APInt(8, 0, true)), size, Architecture::WORD_SIZE };
    CallInst::Create(memset, &params[0], &params[4], "", bb);   
}

// To do -- Inline uncontended case.
void BaseCompiler::lock(Value* lock, BasicBlock* bb) {
    Value *func = module->getOrInsertFunction("gvmt_fast_lock", TYPE_V, TYPE_P, NULL);
    Value *params[] = { lock };
    CallInst::Create(func, &params[0], &params[1], "", bb)->setCallingConv(CallingConv::X86_FastCall); 
}

// To do -- Inline uncontended case.
void BaseCompiler::unlock(Value* lock, BasicBlock* bb) {
    Value *func = module->getOrInsertFunction("gvmt_fast_unlock", TYPE_V, TYPE_P, NULL);
    Value *params[] = { lock };
    CallInst::Create(func, &params[0], &params[1], "", bb)->setCallingConv(CallingConv::X86_FastCall); 
}

void BaseCompiler::store(Value* val, Value* ptr, BasicBlock* bb) {
    assert(PointerType::get(val->getType(), 0) == ptr->getType());
    new StoreInst(val, ptr, bb);
}

Value* BaseCompiler::save_and_restore(Value* from, const Type* to, BasicBlock* bb) {
    assert(from->getType() == TYPE_X);
    assert(isa<LoadInst>(from));
    //fprintf(stderr, "Save and restore\n");
    Value* ptr = ((LoadInst*)from)->getPointerOperand();
    store(from, ptr, bb);
    ptr = new BitCastInst(ptr, PointerType::get(to, 0), "x", bb);
    return new LoadInst(ptr, "val", bb);
}

Value* BaseCompiler::cast_to_B(Value* from, BasicBlock* bb) {
    const Type* from_type = from->getType();
    if (from_type == IntegerType::get(1)) {
        return from;        
    }
    if (from_type == TYPE_P) {
        Value* null =  ConstantPointerNull::get(TYPE_P);
        return new ICmpInst(ICmpInst::ICMP_NE, from, null, "", bb);     
    } else if (from_type == TYPE_F4 || from_type == TYPE_F8) {
        Value* f0 = ConstantFP::get(APFloat(0.000000e+00f));
        return new FCmpInst(FCmpInst::FCMP_UNE, from, f0, "", bb);
    } else {
        return new ICmpInst(ICmpInst::ICMP_NE, from, ZERO, "", bb);
    }
}

Value* BaseCompiler::cast_to_I4(Value* from, BasicBlock* bb) {
    const Type* from_type = from->getType();
    if (from_type == TYPE_I4)
        return from;
    if (from_type == TYPE_F4) {
        return new BitCastInst(from, TYPE_I4, "x", bb);
    } else if (from_type == TYPE_X) {
        return save_and_restore(from, TYPE_I4, bb);
    } else {
        assert((from_type == TYPE_P) && "Illegal type");
        return new PtrToIntInst(from, TYPE_I4, "x", bb);
    }
}

Value* BaseCompiler::cast_to_F4(Value* from, BasicBlock* bb) {
    const Type* from_type = from->getType();
    if (from_type == TYPE_F4)
        return from;
    if (from_type == TYPE_I4) {
        return new BitCastInst(from, TYPE_F4, "x", bb);
    } else if (from_type == TYPE_X) {
        return save_and_restore(from, TYPE_F4, bb);
    } else {
        assert((from_type == TYPE_P) && "Illegal type");
        Value* int_tmp = new PtrToIntInst(from, TYPE_I4, "x", bb);
        return new BitCastInst(int_tmp, TYPE_F4, "x", bb);
    }
}

Value* BaseCompiler::cast_to_I8(Value* from, BasicBlock* bb) {
    const Type* from_type = from->getType();
    if (from_type == TYPE_I8)
        return from;
    if (from_type == TYPE_F8) {
        return new BitCastInst(from, TYPE_I8, "x", bb);
    } else {
        assert(0 && "Illegal type");
        return NULL;
    }
}

Value* BaseCompiler::cast_to_F8(Value* from, BasicBlock* bb) {
    const Type* from_type = from->getType();
    if (from_type == TYPE_F8)
        return from;
    if (from_type == TYPE_I8) {
        return new BitCastInst(from, TYPE_F8, "x", bb);
    } else if (from_type == TYPE_X) {
        return save_and_restore(from, TYPE_I4, bb);
    } else {
        assert(0 && "Illegal type");
        return NULL;
    }
}

Value* BaseCompiler::cast_to_P(Value* from, BasicBlock* bb) {
    const Type* from_type = from->getType();
    if (from_type == TYPE_P)
        return from;
    if (from_type == TYPE_I4)
        return new IntToPtrInst(from, TYPE_P, "x", bb);
    if (from_type == TYPE_F4) {
        Value* int_tmp = new BitCastInst(from, TYPE_I4, "x", bb);
        return new IntToPtrInst(int_tmp, TYPE_P, "x", bb);
    } else if (from_type == TYPE_X) {
        return save_and_restore(from, TYPE_I4, bb);
    } else {
        assert(0 && "Illegal type");
        abort();
    }
}

Value* BaseCompiler::cast_to_R(Value* from, BasicBlock* bb) {
    return cast_to_P(from, bb);
}

BasicBlock* BaseCompiler::makeBB(std::string name, int index) {
    char buf[12];
    if (index)
        sprintf(buf, "%d", index);
    else
        sprintf(buf, "%d", ++bb_index);
    return BasicBlock::Create("bb_" + name + buf, current_function);
}

void BaseCompiler::write_ir(void) {
//    fprintf(stderr, "Verifying\n");
//    verifyModule(*module, PrintMessageAction);
    PassManager PM;
//    fprintf(stderr, "Printing\n");
    PM.add(createPrintModulePass(&llvm::outs()));
    PM.run(*module);
    //    fprintf(stderr, "Verifying\n");
    verifyModule(*module, PrintMessageAction);
}

static inline int64_t high_res_time(void) {
    struct timespec ts; 
    clock_gettime(CLOCK_REALTIME, &ts);
    int64_t billion = 1000000000;
    return billion * ts.tv_sec + ts.tv_nsec;
}
 
void* BaseCompiler::jit_compile(Function* func, char* name) {
    void* code;
    int64_t t0, t1, t2;
    t0 = high_res_time();
    // Uncomment line below to run verifier.
    //verifyFunction(*func, PrintMessageAction);
    if (execution_engine == 0) {
        // InitializeNativeTarget()
        module_provider = new ExistingModuleProvider(module);
        execution_engine = ExecutionEngine::createJIT(module_provider, 0, 0, true);
//                                                      CodeGenOpt::Aggressive);
        execution_engine->DisableLazyCompilation(true);
        func_pass_manager = new FunctionPassManager(module_provider);
        func_pass_manager->add(new TargetData(*execution_engine->getTargetData()));
        func_pass_manager->add(createCFGSimplificationPass());    
        func_pass_manager->add(createPromoteMemoryToRegisterPass()); 
        func_pass_manager->add(createBasicAliasAnalysisPass());
        func_pass_manager->add(createInstructionCombiningPass()); 
        func_pass_manager->add(createSimplifyLibCallsPass());     // - Is this useful?
        func_pass_manager->add(createJumpThreadingPass());        
        func_pass_manager->add(createCFGSimplificationPass());    
        func_pass_manager->add(createInstructionCombiningPass()); 
        func_pass_manager->add(createCondPropagationPass());      
                                                                  
        func_pass_manager->add(createCFGSimplificationPass());    
        func_pass_manager->add(createReassociatePass());          
        func_pass_manager->add(createLICMPass());
        func_pass_manager->add(createSCCPPass());                 
        func_pass_manager->add(createGVNPass());  
        func_pass_manager->add(createInstructionCombiningPass()); 
        func_pass_manager->add(createCondPropagationPass());      
                                                                  
        func_pass_manager->add(createDeadStoreEliminationPass()); 
        func_pass_manager->add(createCFGSimplificationPass());    
        // Uncomment line below to generate CFGs.
        // func_pass_manager->add(createCFGPrinterPass());            
    }
    func_pass_manager->run(*func);
    t1 = high_res_time();
    gvmt_total_optimisation_time += (t1 - t0);
    code = execution_engine->getPointerToFunction(func);
    // Function is no longer required.
    func->deleteBody();
    t2 = high_res_time();
    gvmt_total_codegen_time += (t2 - t1);
    gvmt_total_compilation_time += (t2 - t0);
    return code;
}

