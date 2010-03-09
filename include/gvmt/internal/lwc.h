#ifndef GVMT_LWC_H
#define GVMT_LWC_H 

extern uint8_t* emit_reg_reg_move(int r1, int r2, uint8_t* code);
extern uint8_t* emit_push_c_stack(int bytes, uint8_t* code);
extern uint8_t* emit_pop_c_stack(int bytes, uint8_t* code);
extern uint8_t* emit_load_c_stack_address(int offset, int reg, uint8_t* code);
extern uint8_t* emit_reg_offset_store(int r_addr, int offset, int r_value, uint8_t* code);
extern uint8_t* emit_reg_offset_load(int r_addr, int offset, int r_value, uint8_t* code);
extern uint8_t* emit_load_from_frame(int offset, int reg, uint8_t* code);
extern uint8_t* emit_store_to_frame(int offset, int reg, uint8_t* code);
extern uint8_t* emit_reg_reg_add(int r1, int r2, int r3, uint8_t* code);

struct registers {
    int count;
    int indices[0];
};

extern int gvmt_lwc_usable_registers;
extern int gvmt_lwc_caller_save_registers;
extern int gvmt_lwc_callee_save_registers;
extern int gvmt_lwc_stack_pointer;
extern int gvmt_lwc_frame_pointer;
extern int gvmt_lwc_parameter_registers[8];
extern int gvmt_lwc_parameter_register_count;
extern int gvmt_lwc_return_register;

#endif // GVMT_LWC_H 
