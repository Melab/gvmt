#Assorted useful objects

import re, sys
import os
import StringIO

legal_qualifiers = ( 'protected', 'private', 'nocomp', 'componly')
global_debug = False

class GVMTException(Exception): 
    
    def __init__(self, *args):
        Exception.__init__(self, *args)

class LocatedException(GVMTException):
    
    def __init__(self, msg, file, line):
        GVMTException.__init__(self, '%s:%s: %s' % (file, line, msg))

class UnlocatedException(GVMTException):
    
    def __init__(self, msg):
        self.msg = msg
        GVMTException.__init__(self, msg)
        
    def locate(self, file, line):
        return LocatedException(self.msg, file, line)
   
# Output class, tracks line number.
class Out:
    
    def __init__(self, name):
        self.name = name
        self.file = open(name, "w")
        self.line = 1
        
    def __lshift__(self, obj):
        txt = str(obj)
        self.line += txt.count(os.linesep)
        self.file.write(txt)
        return self
        
    def write(self, obj):
       self << obj
        
    def close(self):
        self.file.close()    
        
    def no_line(self):
        self.line += 2
        self.file.write('\n#line 1 "no_source"\n')
        
# Conforms to same interface as Out        
class Buffer:
        
    def __init__(self):
        self.buffer = StringIO.StringIO()
        self.line = 1
        self.name = ''
        
    def close(self):
        pass
        
    def __lshift__(self, obj):
        txt = str(obj)
        self.line += txt.count(os.linesep)
        self.buffer.write(txt)
        return self
        
    def get_contents(self):
        return self.buffer.getvalue()
        
    def __str__(self):
        return self.buffer.getvalue()
        
    def no_line(self):
        self.line += 2
        self.buffer.write('\n#line 1 "no_source"\n')
      
_COMMENT = re.compile(r' *//.*')
_LINE = re.compile(r'@LINE +\d+')
_FILE = re.compile(r'@FILE +(\S*)')
        
class In:
    
    def __init__(self, files):
        if isinstance(files, str):
            self.file = open(files)
            self.name = files
            self.remaining = None
        elif isinstance(files, Buffer):
            self.file = StringIO.StringIO(files.buffer.getvalue())
            self.name = ''
            self.remaining = None
        else:
            self.file = open(files[0])
            self.name = files[0]
            self.remaining = files[1:]
        self.line = 1
        
    def _getline(self):
        line = self.file.readline()
        while not line and self.remaining:
            self.file = open(self.remaining[0])
            self.name = self.remaining[0]
            line = self.file.readline()
            self.remaining = self.remaining[1:]  
            self.line = 1
        return line
        
    def readline(self):
        self.line += 1
        text = self._getline()
        while _COMMENT.match(text):
            self.line += 1
            text = self._getline()
        if _LINE.match(text):
            self.line = int(text.split()[1])
            self.line += 1
            text = self._getline()
        if _FILE.match(text):
            self.name = text.split()[1]
            self.line += 1
            text = self._getline()
        while text.rstrip().endswith('\\'):
            text = text.rstrip()[:-1] + self.readline()
        return text
        
    def close(self):
        self.file.close()           

# This needs to be modified to support other platforms.
def llvm_cc():
    return 'CallingConv::X86_FastCall'
  
from operator import itemgetter as _itemgetter
import sys as _sys

def struct(struct_name, *items):
    "Class factory for dumb 'struct' like classes, handy for AST classes"
    names = ', '.join([x[0] for x in items])
    template = 'class %s(object):\n'% struct_name
    template += "    '''\n"
    for i in items:
        template += '    @ivar %s:\n' % i[0]
        template += '    @type %s: %s\n' % (i[0], i[1])
    template += "    '''\n"
    template += '    __slots__ = %s\n' % str(tuple(x[0] for x in items))
    template += '    def __init__(self, %s):\n' % names
    template += "        'Initialises attributes.'\n"
    for i in items:
        template += '        self.%s = %s\n' % (i[0],i[0])
    template += '    def __repr__(self):\n' 
    template += "        return '%s(%s)' %% (%s)\n" % (struct_name, 
                ', '.join(['%s' for x in items]), 
                ', '.join(['self.%s' % x[0] for x in items]))
#    print template
    namespace = dict(itemgetter=_itemgetter)
    try:
        exec template in namespace
    except SyntaxError, e:
        raise SyntaxError(e.message + ':\n' + template)
    result = namespace[struct_name]
    result.__module__ = _sys._getframe(1).f_globals['__name__']
    return result
    
       
def print_usage(options, input = 'input-file(s)'):
    print "Usage: %s [options] %s" % (sys.argv[0], input)
    print "Options are:"
    olist = [(opt, options[opt]) for opt in options]
    olist.sort()
    for opt, txt in olist:
        print "-%s: %s" % (opt, txt)    
        
