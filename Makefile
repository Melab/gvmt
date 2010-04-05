#Copyright (C) 2008, 2009, 2010 Mark Shannon
#GNU General Public License
#For copyright terms see: http://www.gnu.org/copyleft/gpl.html

#Add this to fetch lcc, need to set variables for lcc: BUILD, etc.
#wget -q -O lcc.tar.gz http://sites.google.com/site/lccretargetablecompiler/downloads/4.2.tar.gz
# Add -nc (no clobber option) to put in standard build, clean can delete whole lcc directory...

LCC_HOME=~/lcc/
NDBG=-DNDEBUG
OPT=-O3
CC= gcc -std=c99 $(OPT) -fPIC -fomit-frame-pointer -Wall -iquote include -c
# Add -fno-rtti to this when using LLVM 2.7
CPP= g++ -fno-exceptions $(OPT) -D__STDC_LIMIT_MACROS -fomit-frame-pointer -Wall -iquote include -c
I=include/gvmt/internal
HEADERS = $I/compiler.hpp $I/compiler_shared.h $I/core.h
GC_HEADERS =  $I/gc.hpp $I/gc_threads.hpp $I/memory.hpp

build/debug/%.o : lib/%.c $(HEADERS)
	$(CC) -g -o $@ $<

build/debug/%.o : lib/%.cpp $(HEADERS)
	$(CPP) -g -o $@ $<
         
build/fast/%.o : lib/%.c $(HEADERS)
	$(CC) $(NDBG) -g -o $@ $<

build/fast/%.o : lib/%.cpp $(HEADERS)
	$(CPP) $(NDBG) -g -o $@ $<

build/gc/%.o : lib/%.c $(HEADERS)
	$(CC) $(NDBG) -g -o $@ $<
           
build/gc/%.o : lib/%.cpp $(HEADERS) $(GC_HEADERS)
	$(CPP) -fno-rtti $(NDBG) -g -o $@ $<
           
build/gc/%.o : gc/%.cpp $(HEADERS) $(GC_HEADERS)
	$(CPP) -fno-rtti $(NDBG) -g -o $@ $<

DEBUG_LIB = build/debug/core.o build/debug/scan.o \
	build/debug/exceptions.o build/x86.o build/debug/symbol.o \
	build/debug/lock.o \
	build/debug/arena.o build/debug/marshal.o build/debug/machine.o
      
FAST_LIB = build/fast/core.o build/fast/scan.o  \
	build/fast/exceptions.o build/x86.o build/fast/symbol.o \
	build/fast/lock.o \
	build/fast/arena.o build/fast/marshal.o  build/fast/machine.o
	
GC_LIB = build/gc/gc_threads.o build/gc/gc_semispace.o build/gc/gc_generational.o build/gc/gc.o

build: prepare build_lcc doc \
	   build/gvmt.o build/gvmt_compiler.o build/gvmt_debug.o \
	   build/gvmt_gc_copy.a build/gvmt_gc_copy_threads.a \
	   build/gvmt_gc_gen_copy.a build/gvmt_gc_copy_tagged.a \
	   build/gvmt_gc_gen_copy_tagged.a build/gvmt_gc_copy2.a \
	   build/gvmt_gc_gencopy2.a build/gvmt_gc_genimmix2.a \
	   build/gvmt_gc_none.o

build/gvmt_debug.o: $(DEBUG_LIB)
	ld -r -o build/gvmt_debug.o $(DEBUG_LIB)
	
build/gvmt.o: $(FAST_LIB)
	ld -r -o build/gvmt.o $(FAST_LIB)
	
build/gvmt_gc_none.o: build/gc/none.o
	cp build/gc/none.o build/gvmt_gc_none.o
	
build/gvmt_gc_copy.a: build/gc/cheney.o build/gc/copy.o
	ar rcs  build/gvmt_gc_copy.a build/gc/cheney.o build/gc/copy.o 
	
build/gvmt_gc_copy_threads.a: $(GC_LIB) build/gc/gc_copy.o 
	ar rcs  build/gvmt_gc_copy_threads.a $(GC_LIB) build/gc/gc_copy.o 

build/gvmt_gc_gen_copy.a: $(GC_LIB) build/gc/gc_gen_copy.o build/gc/gc_generational.o	
	ar rcs  build/gvmt_gc_gen_copy.a $(GC_LIB) build/gc/gc_gen_copy.o build/gc/gc_generational.o

build/gvmt_gc_copy_tagged.a: $(GC_LIB) build/gc/gc_copy_tagged.o	
	ar rcs  build/gvmt_gc_copy_tagged.a $(GC_LIB) build/gc/gc_copy_tagged.o

build/gvmt_gc_gen_copy_tagged.a: $(GC_LIB) build/gc/gc_gen_copy_tagged.o build/gc/gc_generational.o
	ar rcs  build/gvmt_gc_gen_copy_tagged.a $(GC_LIB) build/gc/gc_gen_copy_tagged.o build/gc/gc_generational.o
           
build/gvmt_gc_copy2.a: build/gc/NonGenCopy.o build/gc/gc_threads.o build/gc/gc.o build/gc/memory.o
	ar rcs  build/gvmt_gc_copy2.a build/gc/NonGenCopy.o build/gc/gc_threads.o build/gc/gc.o build/gc/memory.o

build/gvmt_gc_gencopy2.a: build/gc/GenCopy.o build/gc/gc_threads.o build/gc/gc.o build/gc/memory.o
	ar rcs  build/gvmt_gc_gencopy2.a build/gc/GenCopy.o build/gc/gc_threads.o build/gc/gc.o build/gc/memory.o
	
build/gvmt_gc_genimmix2.a: build/gc/GenImmix.o build/gc/gc_threads.o build/gc/gc.o build/gc/memory.o
	ar rcs  build/gvmt_gc_genimmix2.a build/gc/GenImmix.o build/gc/gc_threads.o build/gc/gc.o build/gc/memory.o
	
build/gc/GenImmix.o : gc/GenImmix.cpp $I/Immix.hpp $I/Generational.hpp $(HEADERS) $(GC_HEADERS)
	$(CPP) $(NDBG) -g -o $@ $<
          
build/gc/GenCopy.o : gc/GenCopy.cpp $I/SemiSpace.hpp $I/Generational.hpp $(HEADERS) $(GC_HEADERS)
	$(CPP) $(NDBG) -g -o $@ $<
    
build/gvmt_compiler.o : lib/compiler_support.cpp $(HEADERS)
	$(CPP) $(NDBG) -o $@ $<
    
lib/scan.gsc: lib/scan.c
	python tools/gvmtc.py -Llcc/build -Iinclude -o lib/scan.gsc lib/scan.c

build/debug/scan.gso: lib/scan.gsc
	python tools/gvmtas.py -Hinclude -g -o build/debug/scan.gso lib/scan.gsc

build/debug/scan.o: build/debug/scan.gso
	python tools/gvmtlink.py -n -o build/debug/scan.o build/debug/scan.gso

build/fast/scan.gso: lib/scan.gsc
	python tools/gvmtas.py -Hinclude -O3 -o build/fast/scan.gso lib/scan.gsc

build/fast/scan.o: build/fast/scan.gso
	python tools/gvmtlink.py -n -o build/fast/scan.o build/fast/scan.gso
	
build/gc/gc_copy_tagged.o: lib/gc_copy.cpp
	$(CPP) $(NDBG) -g -DGVMT_SCHEME_TAGGING -o $@ $<
	
build/gc/gc_gen_copy_tagged.o: lib/gc_gen_copy.cpp
	$(CPP) $(NDBG) -g -DGVMT_SCHEME_TAGGING -o $@ $<
    
prepare:
	mkdir -p build/debug
	mkdir -p build/fast
	mkdir -p build/gc
	cp lib/x86.s build

lcc.tar.gz: 
	wget -q -O lcc.tar.gz http://sites.google.com/site/lccretargetablecompiler/downloads/4.2.tar.gz
	
lcc: lcc.tar.gz gvmt_lcc/gvmt.c gvmt_lcc/svg.c gvmt_lcc/config.h \
     gvmt_lcc/bind.c gvmt_lcc/error.c gvmt_lcc/decl.c gvmt_lcc/error.c \
     gvmt_lcc/makefile
	mkdir -p lcc_extract
	cd lcc_extract; tar -zxf ../lcc.tar.gz
	cp -ru lcc_extract lcc
	rm -rf lcc_extract
	cp gvmt_lcc/gvmt.c lcc/src
	cp gvmt_lcc/svg.c lcc/src
	cp gvmt_lcc/config.h lcc/src
	cp gvmt_lcc/bind.c lcc/src
	cp gvmt_lcc/error.c lcc/src
	cp gvmt_lcc/decl.c lcc/src
	cp gvmt_lcc/error.c lcc/src
	cp gvmt_lcc/makefile lcc

prepare_lcc: prepare lcc 
	mkdir -p lcc/build 

install: build install_lcc
	mkdir -p  /usr/local/lib/gvmt
	cp -r include/gvmt /usr/local/include
	cp build/gvmt.o /usr/local/lib/
	cp build/gvmt_compiler.o /usr/local/lib/
	cp build/gvmt_debug.o /usr/local/lib/
	cp build/gvmt_gc_none.o /usr/local/lib/
	cp build/gvmt_gc_copy.a /usr/local/lib/
	cp build/gvmt_gc_copy_tagged.a /usr/local/lib/
	cp build/gvmt_gc_copy_threads.a /usr/local/lib/
	cp build/gvmt_gc_gen_copy.a /usr/local/lib/
	cp build/gvmt_gc_gen_copy_tagged.a /usr/local/lib/
	cp build/gvmt_gc_copy2.a /usr/local/lib/
	cp build/gvmt_gc_gencopy2.a /usr/local/lib/
	cp build/gvmt_gc_genimmix2.a /usr/local/lib/
	cp tools/*.py /usr/local/lib/gvmt
	cp gc/*.gsc /usr/local/lib/gvmt
	cp scripts/* /usr/local/bin
	export PYTHONPATH=/usr/local/lib/gvmt; python /usr/local/lib/gvmt/install.py
    
install_lcc:
	mkdir -p /usr/local/lib/gvmt/lcc/gcc
	ln -s -f /usr/bin/cpp /usr/local/lib/gvmt/lcc/gcc/cpp
	mkdir -p  /usr/local/lib/gvmt/include
	cp -r gvmt_lcc/include /usr/local/lib/gvmt
	cp lcc/build/rcc /usr/local/lib/gvmt/lcc/rcc
	cp lcc/build/lcc /usr/local/lib/gvmt/lcc/lcc
	
uninstall:
	rm -rf /usr/local/lib/gvmt
	rm -f /usr/local/lib/gvmt.o
	rm -f /usr/local/lib/gvmt_compiler.o
	rm -f /usr/local/lib/gvmt_debug.o 
	rm -f /usr/local/lib/gvmt_gc_none.o 
	rm -f /usr/local/lib/gvmt_gc_copy.a 
	rm -f /usr/local/lib/gvmt_gc_copy_tagged.a 
	rm -f /usr/local/lib/gvmt_gc_copy_threads.a
	rm -f /usr/local/lib/gvmt_gc_gen_copy.a 
	rm -f /usr/local/lib/gvmt_gc_gen_copy_tagged.a 
	rm -f /usr/local/lib/gvmt_gc_copy2.a
	rm -f /usr/local/lib/gvmt_gc_gencopy2.a 
	rm -f /usr/local/lib/gvmt_gc_genimmix2.a 
    
build_lcc: prepare_lcc
	cd lcc; make all
	mkdir -p lcc/build/gcc
	ln -s -f /usr/bin/cpp lcc/build/gcc/cpp
	
doc:
	cd docs; make all
    
clean:
	rm -rf build
	rm -rf distro
	rm -f *~
	rm -f */*~
	rm -f tools/*.pyc
	rm -f gvmt.zip
	rm -rf lcc
	rm -f lib/scan.gsc
	cd docs ; make clean
	cd example; make clean
	
extra_clean: clean
	rm -f lcc.tar.gz
	rm -f docs/manual.pdf
	rm -f install.html
    
.PHONY: clean extra_clean
