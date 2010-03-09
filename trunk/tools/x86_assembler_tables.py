
regs = ['%eax', '%ebx', '%ecx', '%edx', '%esi', '%edi']
all_regs = regs + ['%esp', '%ebp']

def align():
    print '.align 8'

def reg_reg_move():
    print '.globl x86_reg_reg_move_table'
    print 'x86_reg_reg_move_table:'
    for r1 in regs:
        for r2 in regs:
            print '    movl %s, %s' % (r1, r2)    
    print '.globl x86_reg_reg_move_table_end'
    print 'x86_reg_reg_move_table_end:'
    print 

def reg_offset_load():
    print '.globl x86_reg_offset_load_table'
    print 'x86_reg_offset_load_table:'
    for r1 in all_regs:
        for r2 in regs:
            print '    movl 16909060(%s), %s' % (r1, r2)    
    print '.globl x86_reg_offset_load_table_end'
    print 'x86_reg_offset_load_table_end:'
    print 

def reg_offset_store():
    print '.globl x86_reg_offset_store_table'
    print 'x86_reg_offset_store_table:'
    for r1 in all_regs:
        for r2 in regs:
            print '    movl %s, 16909060(%s)' % (r2, r1)    
    print '.globl x86_reg_offset_store_table_end'
    print 'x86_reg_offset_store_table_end:'
    print 

def reg_reg_add():
    print '.globl x86_reg_reg_add_table'
    print 'x86_reg_reg_add_table:'
    for r1 in regs:
        for r2 in regs:
            for r3 in regs:
                print '    leal (%s,%s), %s' % (r1, r2, r3) 
    print '.globl x86_reg_reg_add_table_end'
    print 'x86_reg_reg_add_table_end:'
    print 

def reg_const_add():
    print '.globl x86_reg_const_add_table'
    print 'x86_reg_const_add_table:'
    for r1 in regs:
        for r2 in regs:
            print '    leal 16909060(%s), %s' % (r1, r2) 
    print '.globl x86_reg_const_add_table_end'
    print 'x86_reg_const_add_table_end:'
    print 
    
def call_indirect():
    print '.globl x86_call_indirect_table' 
    print 'x86_call_indirect_table:' 
    for r in regs:
        print '    call *%s' % r
    print '.globl x86_call_indirect_table_end' 
    print 'x86_call_indirect_table_end:' 
    print
    
def test_zero(name, jmp):
    print '.align 16'
    print '.string "x86_branch_%s_table"' % name
    print '.align 16'
    print '.globl x86_branch_%s_table' % name
    print 'x86_branch_%s_table:' % name
    for r in regs:
        print '    testl %s, %s' % (r, r) 
        print '    %s wherever' % jmp 
    print '.globl x86_branch_%s_table_end' % name
    print 'x86_branch_%s_table_end:' % name
    print
    
def test_reg(name, jmp):
    print '.align 16'
    print '.string "x86_branch_%s_table"' % name
    print '.align 16'
    print '.globl x86_branch_%s_table' % name
    print 'x86_branch_%s_table:' % name
    for r1 in regs:
        for r2 in regs:
            print '    cmpl %s, %s' % (r1, r2)
            print '    %s wherever' % jmp 
    print '.globl x86_branch_%s_table_end' % name
    print 'x86_branch_%s_table_end:' % name
    print
    
def left_shift():
    print '.align 16'
    print '.string "left_shift_table"'
    print '.align 16'
    print '.globl x86_left_shift_table'
    print 'x86_left_shift_table:'
    for k in (1,2,3):
        for r1 in regs:
            for r2 in regs:
                print ' leal	0(,%s,%d), %s' % (r1, 1<<k, r2)
    print '.globl x86_left_shift_table_end'
    print 'x86_left_shift_table_end:'
    print '.string "left_shift_table_end"'
    print
    
def branch_ne():
    test_reg('reg_ne', 'jne')
    align()
    test_zero('nonzero', 'jne')

def branch_eq():
    test_reg('reg_eq', 'je')
    align()
    test_zero('zero', 'je')
    
if __name__ == '__main__':
    print '.text'
    align()
    reg_reg_move()
    align()
    reg_offset_load()    
    align()
    reg_offset_store()
    align()
    reg_reg_add()
    align()
    reg_const_add()
    align()
    call_indirect()
    align()
    branch_ne()
    align()
    branch_eq()
    align()
    left_shift()
    align()
    print '''
.global x86_jump_instruction
x86_jump_instruction:
    jmp wherever2
.global x86_jump_instruction_end
x86_jump_instruction_end:
.align 8
.global x86_return_instruction
x86_return_instruction:
    ret
.global x86_return_instruction_end
x86_return_instruction_end:
.align 8
.global x86_call_instruction
x86_call_instruction:
    call wherever3
.global x86_call_instruction_end
x86_call_instruction_end:
.align 8
.global x86_esp_add_instruction
x86_esp_add_instruction:
    addl %esp, 16909060
.global x86_esp_add_instruction_end
x86_esp_add_instruction_end:
.align 8
.global x86_esp_sub_instruction
x86_esp_sub_instruction:
    subl %esp, 16909060
.global x86_esp_sub_instruction_end
x86_esp_sub_instruction_end:
.align 8
.global x86_save_registers_code
x86_save_registers_code:
    pushl %esi
    pushl %edi
    pushl %ebx
.global x86_save_registers_code_end
x86_save_registers_code_end:
.align 8
.global x86_restore_registers_code
x86_restore_registers_code:
    popl %ebx
    popl %edi
    popl %esi
.global x86_restore_registers_code_end
x86_restore_registers_code_end:
'''
