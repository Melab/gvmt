
import os, common, gtypes, sys_compiler, gvmtlink, heap
_ptr_size = gtypes.p.size 
_ptr_mask = _ptr_size - 1
ADDR_TEMPLATE = '.long %s\n'
ALIGN_TEMPLATE = '.align %s\n'

LINE_SIZE = 1 << 7
BLOCK_SIZE = 1 << 14
SUPER_BLOCK_ALIGN = 1 << 19
LINE_MASK = LINE_SIZE - 1
BLOCK_MASK = BLOCK_SIZE - 1   
BLOCKS_PER_SUPER_BLOCK = 32
USABLE_BLOCKS_PER_SUPER_BLOCK = 30
MATURE_SPACE_ID = 2

#Increase this to 8?
ALIGNMENT = 4

#x86 specific - Should be in own module

def align(a):
    return '.align %d\n' % a
    
class Block(object):
    
    def __init__(self, area):
        self.area = area
        self.text = common.Buffer()
        self.text << align(BLOCK_SIZE)

    def __lshift__(self, s):
        return self.text << s

class Object(object):
    
    def __init__(self, label):
        self.members = []
        self._size = 0
        self.label = label
        
    def _align(self, a):
        if self._size & (a-1):
            self._size = (self._size + (a - 1)) & ~(a-1)
            self.members.append(ALIGN_TEMPLATE % a)
        
    def add_member(self, tipe, value):
        s = gvmtlink._size[tipe]
        offset = self._align(s) 
        if tipe == 'uint64' or tipe == 'int64':
            self.members.append('.long %s\n'% (int(value) & 0xffffffff))
            self.members.append('.long %s\n'% ((int(value)>>32) & 0xffffffff))
        else:
            self.members.append('%s %s\n'% (gvmtlink._asm_types[tipe], value))
        self._size += s
        
    def size(self):
        return (self._size + (ALIGNMENT-1)) & (-ALIGNMENT)
#    
#    def add_string(self, char_list):
#        for c in char_list:
#            self.members.append(BYTE_TEMPLATES % c)
#        self.size += len(char_list)
    
    def write(self, out):
        out << '.globl %s\n' % self.label
        out << '%s:\n' % self.label
        for m in self.members:
            out << m
        out << align(ALIGNMENT)
        
def crosses(base, size, power2):
    return (base & (power2-1)) + size > power2
    
def roundup(val, power2):
    return (val + power2-1) & (-power2)
    
class MainHeap(object):
    
    def __init__(self):
        self.block = Block(MATURE_SPACE_ID)
        self.blocks = [ self.block ]
        self.offset = 0
    
    def add_object(self, obj):
        size = obj.size()
        if crosses(self.offset, size, LINE_SIZE) and (self.offset&LINE_MASK):
            self.block << align(LINE_SIZE)
            self.offset = roundup(self.offset, LINE_SIZE)
            assert (self.offset & LINE_MASK) == 0
        if crosses(self.offset, size, BLOCK_SIZE) or self.offset == BLOCK_SIZE:
            self.block = Block(MATURE_SPACE_ID)
            self.blocks.append(self.block)
            self.offset = 0
        obj.write(self.block)
        self.offset += size
        assert self.offset <= BLOCK_SIZE

    def block_list(self):
        return self.blocks
        
EMPTY_BLOCK = Block(0)
EMPTY_BLOCK << '.long 0\n'

class Zone(object):
    
    def __init__(self, blocks):
        self.block_list = [EMPTY_BLOCK, EMPTY_BLOCK] + blocks
        self.block_list += [ EMPTY_BLOCK ] * (BLOCKS_PER_SUPER_BLOCK - 
                                              len(self.block_list))
        assert len(self.block_list) == BLOCKS_PER_SUPER_BLOCK
        assert (SUPER_BLOCK_ALIGN == BLOCK_SIZE*BLOCKS_PER_SUPER_BLOCK)
        self.spaces = [ b.area for b in self.block_list ]
        
    def space(self):
        return self.size - size.offset
        
    def write(self, out):
        out << align(SUPER_BLOCK_ALIGN)
        # Permanent
        out << '.byte 1\n'
        # Spaces
        for a in self.spaces[1:]:
            out << '.byte %d\n' % a
        out << align(BLOCK_SIZE)
        # Second header block
        out << '.long 0\n'
        out << align(BLOCK_SIZE)
        for b in self.block_list[2:]:
            out << b.text
        out << align(SUPER_BLOCK_ALIGN)

def large_object_size(x):
    base_size = 1 << (x//2)
    if x & 1:
        return int(base_size*4.0/3) & (-16)
    else:
        return base_size

class LargeObjectArea(object):
    
    def __init__(self):
        self.objects = []
    
    def add_object(self, obj):
        self.objects.append(obj)
 
    def block_list(self):
        blist = []
        for obj in self.objects:
            block = Block(1)
            obj.write(block)
            blist.append(block)
            size = obj.size()
            while (size > BLOCK_SIZE):
                size -= BLOCK_SIZE
                b = Block(1)
                b.text = common.Buffer()
                blist.append(b)
        return blist
           
def write_blocks(blocks, out):
    while len(blocks) > USABLE_BLOCKS_PER_SUPER_BLOCK:
        Zone(blocks[:USABLE_BLOCKS_PER_SUPER_BLOCK]).write(out)
        blocks = blocks[USABLE_BLOCKS_PER_SUPER_BLOCK:]
    Zone(blocks).write(out)
           
def to_asm(section, base_name):
    out_file = os.path.join(sys_compiler.TEMPDIR, base_name + 
                            sys_compiler.ASM_EXT)
    out = common.Out(out_file)
    out << '.data\n'
    current_object = None
    main_heap = MainHeap()
    loa = LargeObjectArea()
    for t, n in section.items:
        if t[0] == '.':
            if t == '.object':
                current_object = Object(n)
            elif t == '.end':
                if current_object.size() >= BLOCK_SIZE // 4 * 3:
                    loa.add_object(current_object)
                else:
                    main_heap.add_object(current_object)
            elif t == '.public':
                pass
            elif t == '.label':
                raise common.UnlocatedException(".label not allowed in heap, use .object\n")
        else:
            if t == 'string':
                seq = parse_string(n)
                for c in seq:
                    current_object.add_member('uint8' ,c)
                offset += len(seq)
            else:
                current_object.add_member(t, n)
    out << align(SUPER_BLOCK_ALIGN)
    out << '.globl gvmt_start_heap\n'
    out << 'gvmt_start_heap:\n'
    small_object_blocks = main_heap.block_list();
    if small_object_blocks:
        b = small_object_blocks[-1]
        b << align(BLOCK_SIZE)
        b << '.globl gvmt_small_object_area_end\n'
        b << 'gvmt_small_object_area_end:\n'
        write_blocks(small_object_blocks, out)
    else:
        out << '.globl gvmt_small_object_area_end\n'
        out << 'gvmt_small_object_area_end:\n'
    out << align(SUPER_BLOCK_ALIGN)
    out << '.globl gvmt_large_object_area_start\n'
    out << 'gvmt_large_object_area_start:\n'
    large_objects = loa.block_list()
    if large_objects:
        write_blocks(large_objects, out)
    out << align(BLOCK_SIZE)
    out << '.globl gvmt_large_object_area_end\n'
    out << 'gvmt_large_object_area_end:\n'
    out << align(SUPER_BLOCK_ALIGN)
    out << '.globl gvmt_end_heap\n'
    out << 'gvmt_end_heap:\n'

