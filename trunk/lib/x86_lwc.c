#include <inttypes.h>

#define REG_EAX 0
#define REG_EBX 1
#define REG_ECX 2
#define REG_EDX 3
#define REG_ESI 4
#define REG_EDI 5
#define REG_ESP 6
#define REG_EBP 7

#define REG_MEM_LENGTH 6

//The tables
extern uint8_t x86_reg_reg_move_table;
extern uint8_t x86_reg_reg_move_table_end;
extern uint8_t x86_reg_reg_add_table;
extern uint8_t x86_reg_reg_add_table_end;
extern uint8_t x86_reg_const_add_table;
extern uint8_t x86_reg_const_add_table_end;
extern uint8_t x86_reg_offset_load_table;
extern uint8_t x86_reg_offset_load_table_end;
extern uint8_t x86_reg_offset_store_table;
extern uint8_t x86_reg_offset_store_table_end;
extern uint8_t x86_return_instruction;
extern uint8_t x86_jump_instruction;
extern uint8_t x86_call_indirect_table;
extern uint8_t x86_call_indirect_table_end;
extern uint8_t x86_branch_true_table;
extern uint8_t x86_branch_true_table_end;
extern uint8_t x86_branch_false_table_end;
extern uint8_t x86_branch_false_table;
extern uint8_t x86_esp_add_instruction;
extern uint8_t x86_esp_add_instruction_end;
extern uint8_t x86_esp_sub_instruction;
extern uint8_t x86_esp_sub_instruction_end;

uint8_t* emit_reg_reg_move(int r1, int r2, uint8_t* code) {  
    assert(2* 6*6 == 
           &x86_reg_reg_move_table_end - &x86_reg_reg_move_table);
    uint8_t* inst = &x86_reg_reg_move_table+r1*6*2+r2*2;
    code[0] = inst[0];
    code[1] = inst[1];    
    return code+2;
}

uint8_t* emit_alloca(int bytes, uint8_t* code) {
    bytes = (bytes + 7) & -8;
    assert(&x86_esp_add_instruction_end - &x86_esp_add_instruction == 6);
    code[0] = x86_esp_add_instruction;
    code[1] = (&x86_esp_add_instruction)[1];
    code[5] = (bytes >> 24) & 0xff;
    code[4] = (bytes >> 16) & 0xff;
    code[3] = (bytes >> 8) & 0xff;
    code[2] = bytes & 0xff;
    return code + 6;
}

uint8_t* emit_dealloca(int bytes, uint8_t* code) {
    bytes = (bytes + 7) & -8;
    assert(&x86_esp_sub_instruction_end - &x86_esp_sub_instruction == 6);
    code[0] = x86_esp_sub_instruction;
    code[1] = (&x86_esp_sub_instruction)[1];
    code[5] = (bytes >> 24) & 0xff;
    code[4] = (bytes >> 16) & 0xff;
    code[3] = (bytes >> 8) & 0xff;
    code[2] = bytes & 0xff;
    return code + 6;
}

uint8_t* save_registers(uint8_t* code) {
    uint8_t* inst = &x86_save_registers_code;
    while (inst < &x86_save_registers_code_end)
        *code++ = *inst++;
    return code;
}

uint8_t* restore_registers(uint8_t* code) {
    uint8_t* inst = &x86_restore_registers_code;
    while (inst < &x86_restore_registers_code_end)
        *code++ = *inst++;
    return code;
}

uint8_t* emit_load_frame_address(int offset, int reg, uint8_t* code) {
    return emit_reg_const_add(REG_EDI, offset, reg, code);
}

static uint8_t* with_constant(int r_addr, int offset, int r_value, uint8_t* code, uint8_t* table) {
    uint8_t* inst = table+r_addr*6*6+r_value*6;
    code[0] = inst[0];
    code[1] = inst[1];
    // Integer value in table is 0x01020304, x86 is little-endian
    assert(inst[5] == 1);
    assert(inst[4] == 2);
    assert(inst[3] == 3);
    assert(inst[2] == 4);
    code[5] = (offset >> 24) & 0xff;
    code[4] = (offset >> 16) & 0xff;
    code[3] = (offset >> 8) & 0xff;
    code[2] = offset & 0xff;
    return code + 6;
}

uint8_t* emit_reg_offset_load(int r_addr, int offset, int r_value, uint8_t* code) {
    assert(REG_MEM_LENGTH* 8*6 == 
           &x86_reg_offset_load_table_end - &x86_reg_offset_load_table);
    return with_constant(r_addr, offset, r_value, code, &x86_reg_offset_load_table);
}

uint8_t* emit_reg_offset_store(int r_addr, int offset, int r_value, uint8_t* code) {
    assert(REG_MEM_LENGTH* 8*6 == 
           &x86_reg_offset_store_table_end - &x86_reg_offset_store_table);
    return with_constant(r_addr, offset, r_value, code, &x86_reg_offset_store_table);
}

uint8_t* emit_load_from_frame(int offset, int reg, uint8_t* code) {
    return with_constant(REG_EDI, offset, reg, code, &x86_reg_offset_load_table);
}

uint8_t* emit_store_to_frame(int offset, int reg, uint8_t* code) {
    return with_constant(REG_EDI, offset, reg, code, &x86_reg_offset_store_table);
}

uint8_t* emit_reg_reg_add(int r1, int r2, int r3, uint8_t* code) {  
    assert(3* 6*6 == 
           &x86_reg_reg_move_table_end - &x86_reg_reg_move_table);
    uint8_t* inst = &x86_reg_reg_move_table+r1*6*6*3+r2*6*3+r3*3;
    code[0] = inst[0];
    code[1] = inst[1];    
    code[2] = inst[2];    
    return code+3;
}

uin8_t* emit_shift_left(int r1, int k, int r2, uint8_t* code) {
    assert(k > 0 && k < 4);
    assert(3*6*6*7 == &x86_left_shift_table_end - &x86_left_shift_table);
    uint8_t* inst = &x86_left_shift_table_end + (k-1)*6*6*7 + r1*6*7 + r2*7;
    code[0] = inst[0];
    code[1] = inst[1];
    code[2] = inst[2];
    code[3] = inst[3];
    code[4] = inst[4];
    code[5] = inst[5];
    code[6] = inst[6];
    return code+3;   
}

uint8_t* emit_reg_const_add(int r1, int k, int r2, uint8_t* code) {
    assert(REG_MEM_LENGTH*6*6 == 
           &x86_reg_const_add_table_end - &x86_const_add_table);
    return with_constant(r1, k, r2, code, &x86_const_add_table);
}

uint8_t* emit_jump(uint8_t* code) {
    assert(&x86_jump_instruction_end - &x86_jump_instruction = 5);
    code[0] = x86_jump_instruction;
    return code+5;
}
    
static void patch(uint8_t* addr, uint8_t* target) {
    int offset = target - (addr + 4);
    addr[3] = (offset >> 24) & 0xff;
    addr[2] = (offset >> 16) & 0xff;
    addr[1] = (offset >> 8) & 0xff;
    addr[0] = offset & 0xff;
}
    
void patch_jump(uint8_t* jump_addr, uint8_t* target) {
    patch(jump_addr+1, target);
}

static uint8_t* emit_branch(int reg, uint8_t* code, uint8_t* table) {
    uint8_t* inst = &x86_branch_false_table + 8*reg;
    code[0] = inst[0];
    code[1] = inst[1];    
    code[2] = inst[2];    
    code[3] = inst[3]; 
    return code + 8;
}

uint8_t* emit_branch_unequal(int r1, int r2, uint8_t* code) {
    assert(&x86_branch_reg_ne_table_end - &x86_branch_reg_ne_table == 8*6*6);
    return emit_branch(r2, code, &x86_branch_reg_ne_table+r1*8*6);
}

uint8_t* emit_branch_equal(int r1, int r2, uint8_t* code) {
    assert(&x86_branch_reg_eq_table_end - &x86_branch_reg_eq_table == 8*6*6);
    return emit_branch(reg, code, &x86_branch_reg_eq_table+r1*8*6);
}

uint8_t* emit_branch_zero(int reg, uint8_t* code) {
    assert(&x86_branch_zero_table_end - &x86_branch_zero_table == 8*6);
    return emit_branch(reg, code, &x86_branch_zero_table);
}

uint8_t* emit_branch_non_zero(int reg, uint8_t* code) {
    assert(&x86_branch_nonzero_table_end - &x86_branch_nonzero_table == 8*6);
    return emit_branch(reg, code, &x86_branch_nonzero_table);
}

void patch_branch(uint8_t* branch_addr, uint8_t* target) {
    patch(branch_addr+4, target);
}

uint8_t* emit_call_direct(uint8_t* addr, uint8_t* code) {
    assert(&x86_call_indirect_table_end - &x86_call_indirect_table == 5);
    code[0] = x86_call_indirect_table;
    return code+5;
}

uint8_t* emit_call_indirect(int reg, uint8_t* code) {
    assert(&x86_call_indirect_table_end - &x86_call_indirect_table == 6*2);
    uint8_t* inst = &x86_call_indirect_table + 2*reg;
    code[0] = inst[0];
    code[1] = inst[1]; 
    return code + 2;
}

uint8_t* emit_return(uint8_t* code) {
    *code = x86_return_instruction;
    return code + 1;
}

void flush_caches(void) {
    // No need to do anything on x86.
}

// Move to general lwc

#define REG_RETURN 0
#define REG_P0 2
#define REG_P1 3
#define REG_STACK_POINTER 4
#define REG_FRAME_POINTER 5

int gvmt_lwc_usable_registers = ((1<<REG_EAX) | (1<<REG_EBX) | (1<<REG_ECX) | (1<<REG_EDX)) 
int gvmt_lwc_usable_register_count = 2;
int gvmt_lwc_caller_save_registers = ((1<<REG_EAX) | (1<<REG_ECX) | (1<<REG_EDX));
int gvmt_lwc_callee_save_registers = (1<<REG_EBX);
int gvmt_lwc_stack_pointer = REG_ESI;
int gvmt_lwc_frame_pointer = REG_EDI;
int gvmt_lwc_parameter_registers[8] = { REG_ECX, REG_EDX, -1, -1, -1, -1, -1, -1 };
int gvmt_lwc_parameter_register_count = 2;
int gvmt_lwc_return_register = REG_EAX;

uint8_t* preamble(uint8_t*code, int locals) {
    code = emit_save_registers();
    code = emit_alloca(sizeof(struct gvmt_frame) + sizeof(void*)*locals, code);
    code = emit_reg_reg_move(gvmt_lwc_parameter_registers[0], gvmt_lwc_stack_pointer, code);
    code = emit_load_c_stack_address(sizeof(void*)*3, REG_FRAME_POINTER, code);
    code = emit_reg_offset_store(gvmt_lwc_frame_pointer, 0, gvmt_lwc_parameter_registers[1], code);
    return code;
}

uint8_t* postamble(uint8_t*code, int locals) {
    code = emit_reg_reg_move(gvmt_lwc_stack_pointer, gvmt_lwc_return_register, code);
    code = emit_dealloca(sizeof(struct gvmt_frame) + sizeof(void*)*locals, code);
    code = emit_restore_registers();
    code = emit_return(code);
    return code;
}

