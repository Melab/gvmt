LLVM_LIBS =  -L/usr/local/lib /usr/local/lib/LLVMX86AsmPrinter.o /usr/local/lib/LLVMX86CodeGen.o -lLLVMSelectionDAG -lLLVMAsmPrinter /usr/local/lib/LLVMExecutionEngine.o /usr/local/lib/LLVMJIT.o -lLLVMCodeGen -lLLVMScalarOpts -lLLVMTransformUtils -lLLVMipa -lLLVMAnalysis -lLLVMTarget -lLLVMCore -lLLVMSupport -lLLVMSystem
GVMT_LIBS= -lpthread /usr/local/lib/gvmt.o /usr/local/lib/gvmt_gc_gen_copy_tagged.a
OPT= -O3
NDBG = -DNDEBUG

# You will want to change this for your machine
INSTALL_DIR=/home/mark/bin

install : gvmt_scheme
	cp $< $(INSTALL_DIR); cp lib.scm $(INSTALL_DIR)

%.gso : %.gsc
	gvmtas $(OPT) $(TRACE) $(DEBUG) -mgen_copy -o $@ $<
 
%.gsc : %.c opcodes.h
	gvmtc $(NDBG) -DINSTALL_DIR="\"$(INSTALL_DIR)\"" -I. -o $@ $<

opcodes.h interpreter.gsc : interpreter.vmc
	gvmtic $(NDBG) -I. -b opcodes.h -o interpreter.gsc interpreter.vmc
	
dis.gsc : disassembler.vmc interpreter.gsc
	gvmtxc  -I. -e -ndisassembler -o $@ disassembler.vmc interpreter.gsc
	
cleanup.gsc : cleanup.vmc interpreter.gsc
	gvmtxc  -I. -ncleanup -o $@ cleanup.vmc interpreter.gsc
	
apply.gsc : apply_replacement.vmc interpreter.gsc
	gvmtxc  -I. -napply_replacement -o $@ apply_replacement.vmc interpreter.gsc
	
frame_removal.gsc : frame_removal.vmc interpreter.gsc
	gvmtxc  -I. -nframe_removal -o $@ frame_removal.vmc interpreter.gsc
	
straight_line.gsc : straight_line.vmc interpreter.gsc
	gvmtxc  -I. -nstraight_line -o $@ straight_line.vmc interpreter.gsc
	
can_remove_frames.gsc : can_remove_frames.vmc interpreter.gsc
	gvmtxc  -I. -ncan_remove_frames -o $@ can_remove_frames.vmc interpreter.gsc
	
peephole.gsc : peephole.vmc interpreter.gsc
	gvmtxc  -I. -npeephole -o $@ peephole.vmc interpreter.gsc
	
top_frame_removal.gsc : top_frame_removal.vmc interpreter.gsc
	gvmtxc  -I. -e -ntop_frame_removal -o $@ top_frame_removal.vmc interpreter.gsc
	
GSOS = interpreter.gso support.gso parser.gso eval.gso environment.gso \
	   dis.gso cleanup.gso apply.gso peephole.gso frame_removal.gso \
	   top_frame_removal.gso can_remove_frames.gso straight_line.gso

gvmt.o : $(GSOS)
	gvmtlink -o $@ $(GSOS)
    
native.o : native.c
	gcc $(OPT) -c -g -o $@ $<
    
interpreter : gvmt.o native.o
	g++ -g -o $@ -ldl gvmt.o native.o $(GVMT_LIBS)
	
compiler.cpp: interpreter.gsc
	gvmtcc -mgen_copy -o $@ $<
	
compiler.o: compiler.cpp
	c++ $(OPT) -I. -c -g -D__STDC_LIMIT_MACROS -o $@ $<
	
gvmt_scheme :  gvmt.o native.o compiler.o
	g++ -g -rdynamic -o $@ -ldl gvmt.o native.o /usr/local/lib/gvmt_compiler.o compiler.o  -lc -lrt $(LLVM_LIBS) $(GVMT_LIBS)
	
clean:
	rm -f *.gso *.gsc opcodes.h *.o *.cpp gvmt_scheme

