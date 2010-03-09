#Note all uses of strings here are really bytes.
from common import GVMTException

_sentinel = '#gso sentinel#' # ASCII

class GSO(object):
    
    def __init__(self, code, memory):
        self.code = code
        self.memory = memory
        
    def write(self, file):
        file.write('\000gso')  # ASCII bytes
        l = len(self.code)
        file.write('%c%c%c%c' % (l >> 24, (l >> 16) & 255, (l >> 8) & 255, l & 255)) 
        file.write(self.code)
        file.write(_sentinel)
        file.write(self.memory)
        
        
def read(file):
    header = file.read(4)
    if header != '\000gso':
        raise GVMTException("%s is not a .gso file" % file.name);
    l = file.read(4)
    l = (ord(l[0]) << 24) + (ord(l[1]) << 16) + (ord(l[2]) << 8) + ord(l[3])
    code = file.read(l)
    sentinel = file.read(len(_sentinel))
    if sentinel != _sentinel:
        print sentinel
        raise GVMTException("%s is not a .gso file or is corrupted" % file.name)
    memory = file.read()
    return GSO(code, memory)
