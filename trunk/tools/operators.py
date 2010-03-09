#Low-level ASMC instructions.

#c_names = [ '&', '|', '^', '<<', '>>', '%', '+', '-', '/', '*', '==', '!=', '<', '>', '<=', '>=' ]
 
#gvmt_names = [ 'and', 'or', 'xor', 'lsh', 'rsh', 'mod', 'add', 'sub', 'div', 'mul' ]

class Op:
    
    def __init__(self, name, description, c_name, llvm_signed, llvm_unsigned = None, llvm_float = None):
        self.name = name
        self.c_name = c_name
        self.description = description
        self.llvm_signed = llvm_signed
        self.llvm_unsigned = llvm_unsigned or llvm_signed
        self.llvm_float = llvm_float or llvm_signed
        self._invert = NotImplemented
    
    def invert(self):
        return self._invert
        
    def __repr__(self):
        if self.name in ('AND', 'OR'):
            return 'Op.%s_' % self.name.lower()
        else:
            return 'Op.%s' % self.name.lower()
     
and_ =Op('AND', 'bitwise and', '&', 'Instruction::And')
or_ =  Op('OR', 'bitwise or', '|', 'Instruction::Or')
xor = Op('XOR', 'bitwise exclusive or', '^', 'Instruction::Xor')
lsh = Op('LSH', 'left shift', '<<', 'Instruction::Shl')
rsh = Op('RSH', 'right shift', '>>', 'Instruction::AShr', 'Instruction::LShr')
add = Op('ADD', 'add', '+', 'Instruction::Add')
sub = Op('SUB', 'subtract', '-', 'Instruction::Sub')
mod = Op('MOD', 'modulo', '%', 'Instruction::SRem', 'Instruction::URem')
div = Op('DIV', 'divide', '/', 'Instruction::SDiv', 'Instruction::UDiv', 'Instruction::FDiv')
mul = Op('MUL', 'multiply', '*', 'Instruction::Mul')

eq = Op('EQ', 'equals', '==', 'ICmpInst::ICMP_EQ', 'ICmpInst::ICMP_EQ', 'ICmpInst::FCMP_OEQ')
ne = Op('NE', 'not equals', '!=', 'ICmpInst::ICMP_NE', 'ICmpInst::ICMP_NE', 'ICmpInst::FCMP_ONE')
lt = Op('LT', 'less than', '<', 'ICmpInst::ICMP_SLT', 'ICmpInst::ICMP_ULT', 'ICmpInst::FCMP_OLT')
gt = Op('GT', 'greater than', '>', 'ICmpInst::ICMP_SGT', 'ICmpInst::ICMP_UGT', 'ICmpInst::FCMP_OGT')
le = Op('LE', 'less than or equals', '<=', 'ICmpInst::ICMP_SLE', 'ICmpInst::ICMP_ULE', 'ICmpInst::FCMP_OLE')
ge = Op('GE', 'greater than or equals', '>=', 'ICmpInst::ICMP_SGE', 'ICmpInst::ICMP_UGE', 'ICmpInst::FCMP_OGE')

neg = Op('NEG', 'negate', '-', "Error: Use 0-X")
inv = Op('INV', 'bitwise invert', '~', "Error: Use -1^X")

eq._invert = ne
ne._invert = eq
lt._invert = ge
gt._invert = le
le._invert = gt
ge._invert = lt

