
TEX_FILES=manual.tex instructions.tex intro.tex model.tex

all: manual ../install.html

../install.html: install.tex	
	latex2html -noinfo -nonavigation -noaddress  install.tex
	mv install/install.html ..

manual: manual.pdf
	rm -f *.aux *.log *.toc *.out
	
%.pdf: %.dvi
	dvips -q -o temp.ps $<
	ps2pdf temp.ps $@
	
%.eps: %.dot
	dot -Tps -o$@ $<
    
manual.dvi : ${TEX_FILES} unlabelled.eps c_types.eps gsc_types.eps final.eps Stack.eps build.eps
	export PYTHONPATH="../tools"; python make_options.py
	latex -halt-on-error $<; latex -halt-on-error $<
    
instructions.tex : ../tools/builtin.py tex_instruction.py
	export PYTHONPATH=../tools; python tex_instruction.py > $@
    
build.eps: build.dot
	dot -Tps -o$@ $<
	
%.eps: %.svg
	inkscape -z -E$@ $<
    
clean:
	rm -f *.dvi *.eps instructions.tex *.ps *.backup *options.tex
	rm -rf install
	rm -f *.pyc
