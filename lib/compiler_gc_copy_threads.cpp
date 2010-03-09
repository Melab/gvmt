#include "gvmt/internal/compiler.hpp"

extern GVMT_THREAD_LOCAL char* gvmt_gc_free_pointer;
extern GVMT_THREAD_LOCAL char* gvmt_gc_limit_pointer;

using namespace llvm;

Function* tls_load_function;
Function* tls_store_function;
int sp_offset;
int free_offset;
int limit_offset;

extern char* gs_register(void);

void GC_llvm::init(Module *mod) {
    // FIX ME - This x86 specific hack should be removed when
    // the x86 JIT supports TLS.
    std::vector<const Type*>args;
    args.push_back(BaseCompiler::TYPE_I4);
    FunctionType * ftype = FunctionType::get(BaseCompiler::POINTER_TYPE_R, args, false);
    tls_load_function = Function::Create(ftype, GlobalValue::ExternalLinkage, 
                                         "x86_tls_load", mod);
    args.clear();
    args.push_back(BaseCompiler::TYPE_P);
    args.push_back(BaseCompiler::TYPE_I4);
    ftype = FunctionType::get(Type::VoidTy, args, false);
    tls_store_function = Function::Create(ftype, GlobalValue::ExternalLinkage, 
                                          "x86_tls_store",  mod);
    sp_offset = ((char*)&gvmt_stack_pointer) - gs_register();
    free_offset = ((char*)&gvmt_gc_free_pointer) - gs_register();
    limit_offset = ((char*)&gvmt_gc_limit_pointer) - gs_register();
}

Value* GC_llvm::gc_free_load(BasicBlock* bb) {
    Value* args[] = { ConstantInt::get(APInt(32, free_offset)) };   
    CallInst* call = CallInst::Create(tls_load_function, &args[0], &args[1], "", bb);
    call->setCallingConv(CallingConv::X86_FastCall);
    return call;
}

Value* GC_llvm::gc_limit_load(BasicBlock* bb) {
    Value* args[] = { ConstantInt::get(APInt(32, limit_offset)) };    
    CallInst* call = CallInst::Create(tls_load_function, &args[0], &args[1], "", bb);
    call->setCallingConv(CallingConv::X86_FastCall);
    return call;
}

void GC_llvm::gc_free_store(Value* value, BasicBlock* bb) {
    Value* args[] = { value, ConstantInt::get(APInt(32, limit_offset)) };    
    CallInst* call = CallInst::Create(tls_store_function, &args[0], &args[2], "", bb);
    call->setCallingConv(CallingConv::X86_FastCall);
}

void GC_llvm::gc_limit_store(Value* value, BasicBlock* bb) {
    Value* args[] = { value, ConstantInt::get(APInt(32, limit_offset)) };    
    CallInst* call = CallInst::Create(tls_store_function, &args[0], &args[2], "", bb);
    call->setCallingConv(CallingConv::X86_FastCall);
}

