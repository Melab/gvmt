import platform
_bits = platform.architecture()[0]
assert _bits[-3:] == 'bit'
POINTER_SIZE = int(_bits[:-3])/8

class _Kind:
    
    def __init__(self, name, suffix, doc, c_names):
        self.name = name
        self.suffix = suffix
        self.doc = doc
        self.c_names = c_names
                         
_kind_x = _Kind('X', 'X', 'type X',  ('GVMT_StackItem',)  )
_kind_p = _Kind('ptr', 'P', 'pointer',  ('void*',)  )
_kind_r = _Kind('R', 'R', 'reference', ('GVMT_Object',))
_kind_i = _Kind('int', 'I', 'signed integer', ('x', 'int8_t', 'int16_t', 'int32_t', 'int64_t',))
_kind_u = _Kind('uint', 'U', 'unsigned integer', ('x', 'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',))
_kind_f = _Kind('float', 'F', 'floating point', ('x', 'x', 'x', 'float', 'double',))
_kind_v = _Kind('void', 'V', 'void', ('void',)  )
_kind_b = _Kind('boolean', 'B', 'boolean', ('error',)  )

class Type:
    
    def __init__(self, kind, order = 0):
        if order:
            size = (1 << order) >> 1;
            prefix = '%d bit ' % (size * 8)
            size_name = str(size * 8)
            suffix_name = str(size)
            self.size = size
            self.repr = kind.name.lower() + str(size)
        else:
            prefix = ''
            suffix_name = ''
            size_name = ''
            self.size = POINTER_SIZE
            self.repr = kind.name.lower()
        self.doc = prefix + kind.doc
        self.suffix = kind.suffix + suffix_name
        self.name = kind.name + size_name
        self.c_name = kind.c_names[order]
        self._kind = kind
        
    def __str__(self):
        return self.name
        
    def __repr__(self):
        return 'gtypes.' + self.repr
        
    def is_int(self):
        return self._kind is _kind_i or self._kind is _kind_u
        
    def is_signed(self):
        return self._kind is _kind_i
        
    def is_unsigned(self):
        return self._kind is _kind_u
        
    def is_float(self):
        return self._kind is _kind_f
        
    def same_kind(self, other):
        return self._kind is other._kind
        
    def compatible(self, other):
        if self._kind in (_kind_i,_kind_u) and other._kind in (_kind_i,_kind_u):
            return not self.size <= p.size ^ other.size <= p.size
        else:
            return self is other
      
i1 = Type(_kind_i, 1)
i2 = Type(_kind_i, 2)
i4 = Type(_kind_i, 3)
i8 = Type(_kind_i, 4)
 
u1 = Type(_kind_u, 1)
u2 = Type(_kind_u, 2)
u4 = Type(_kind_u, 3)
u8 = Type(_kind_u, 4)
 
f4 = Type(_kind_f, 3)
f8 = Type(_kind_f, 4)

r = Type(_kind_r)
p = Type(_kind_p)
v = Type(_kind_v)
#Boolean is an internal type only
b = Type(_kind_b)
b.size = 0

types = {}

for t in (i1, i2, i4, i8, u1, u2, u4, u8, f4, f8, r, p, v):
    types[t.name] = t

x = Type(_kind_x)
if POINTER_SIZE == 4:
    x.size = 8
    iptr = i4
    uptr = u4
else:
    iptr = i8
    uptr = u8
v.size = 0

