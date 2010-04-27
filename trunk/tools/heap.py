
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

#x86 specific - Should be in own module

def align(a):
    return '.align %d\n' % a
    
class Block(object):
    
    def __init__(self, area):
        self.area = area
        self.text = common.Buffer()
        self.mark_map = [ 0 ] * (BLOCK_SIZE /LINE_SIZE)
        self.text << align(BLOCK_SIZE)

    def __lshift__(self, s):
        return self.text << s

    def mark(self, offset):
        assert offset <= BLOCK_SIZE
        self.mark_map[offset//LINE_SIZE] |= 1 << ((offset & 127)//4);


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
        return (self._size + 3) & -4
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
        out << align(4)
        
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
        #Set mark bit for this object.
        self.block.mark(self.offset)
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
        assert (SUPER_BLOCK_ALIGN == BLOCK_SIZE*32)
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
        for b in self.block_list:
            for i in b.mark_map:
                out << '.long %d\n' % i
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
        assert large_object_size(20) == 1024
        self.sizes = [large_object_size(x) for x in range(22, 28)]
        self.per_block = [ BLOCK_SIZE // s for s in self.sizes]
        self.objects = [[] for x in range(0, 7)]
    
    def add_object(self, obj):
        size = obj.size() + gtypes.p.size
        for i in range(0, 6):
            if size <= self.sizes[i]:
                self.objects[i].append(obj)
                break
        else:
            self.objects[-1].append(obj)
            
    def _write_label(self, out, label, has_list):
            out << '.globl %s_first_block\n' % label
            
    def block_list(self):
        blist = []
        for i in range(0, 6):           
            object_list = self.objects[i]
            objects = len(self.objects[i])
            per_block = self.per_block[i]
            block_count = (objects + per_block - 1) // per_block
            for e, obj in enumerate(self.objects[i]):
                if e % per_block == 0:
                    block = Block(i+22)
                    if not blist:
                        block << '.globl gvmt_large_object_area_start\n'
                        block << 'gvmt_large_object_area_start:\n'
                    #Debugging label:
                    block << 'gvmt_large_object_block_%d_%d:\n' % (i+22, e)
                    blist.append(block)
                    offset = 0
                block.mark(offset)
                obj.write(block)
                assert obj.size() <= self.sizes[i]
                for x in range(obj.size(), self.sizes[i]):
                    block << '.byte 0\n'
                offset += self.sizes[i]
        return blist

    def very_large_object_block(self):
        if self.objects[-1]:
            b = Block(128)
            for i, obj in enumerate(self.objects[-1][:-1]):
                b << 'gvmt_large_objects_big_%d:\n' % i
                b << '.long  gvmt_large_objects_big_%d\n' % (i+1) #Link word
                obj.write(b)
                b << align(BLOCK_SIZE)
            b << 'gvmt_large_objects_big_%d:\n' % (len(self.objects[-1])-1)
            b << '.long 0\n' #Link word
            self.objects[-1][-1].write(b)
            b << align(BLOCK_SIZE)
        else:
            b = Block(129)
            b << '.long 0\n'
            b << align(BLOCK_SIZE)
        return b
           
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
                if current_object.size() >= 2048:
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
    out << '.globl gvmt_start_heap\n'
    out << 'gvmt_start_heap:\n'
    write_blocks(main_heap.block_list(), out)
    write_blocks(loa.block_list(), out)
    Zone([loa.very_large_object_block()]).write(out)
    out << '.globl gvmt_end_heap\n'
    out << 'gvmt_end_heap:\n'
    
    
    
    
    
    
