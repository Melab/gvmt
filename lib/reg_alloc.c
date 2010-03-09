#include "buffer.h"
#include "emit.h"
#include <assert.h>
#include "flow.h" 

#define GVMT_MAX_TEMPS 100

/********* THE STACK **********
The stack may contain:
    Registers.
    Values - Compile time constant values.
    Spills - When we run out of registers, values on the stack are spilled to memory.

At branches/joins, the stack may only contain:    
    Registers.
    Spills.
*/

/** Arrays *********/

struct gvmt_virtual registers[REGISTER_COUNT];
struct gvmt_memory temps[256];
struct gvmt_memory frame[256];
struct gvmt_virtual values[32];
struct gvmt_virtual spills[32];

#define ALL_REGISTERS_MASK (~(-1 << REGISTER_COUNT))

#define LOCK(reg) (reg)->u.r.locked = 1
#define UNLOCK(reg) (reg)->u.r.locked = 0

int gvmt_stack_purity(GVMT_Stack stack, int depth) {
    for (int i = 1; i <= depth; i++) {
        if (stack->top[-i]->kind != VALUE_KIND)
            return 0;
    }
    return 1;
}

void gvmt_values_reset(void) {
    int i;
    for (i = 0; i < 32; i++) {
        values[i].u.v.value = 0;
    }
}

void gvmt_compiler_init(void) {
    int i;
    for (i = 0; i < REGISTER_COUNT; i++) {
        registers[i].kind = REGISTER_KIND;
        registers[i].u.r.index = i;
        registers[i].u.r.locked = 0;
    }
    gvmt_values_reset();
    for (i = 0; i < 256; i++) {
        temps[i].address = gvmt_address_of_temp(i);
    }
    for (i = 0; i < 256; i++) {
        frame[i].address = gvmt_address_of_frame(i);
    }
    for (i = 0; i < 32; i++) {
        values[i].kind = VALUE_KIND;
    }
    for (i = 0; i < 32; i++) {
        spills[i].kind = SPILL_KIND;
        spills[i].u.s.address = gvmt_address_of_spill(i);
    }
}

void gvmt_local_state_init(GVMT_LocalState state, Arena a, GVMT_Stack init_stack) {
    state->stack.base = gvmt_allocate(128, a);
    state->stack.top = state->stack.base + (init_stack->top - init_stack->base);
    Virtual *to, *from;
    to = state->stack.base;
    from = init_stack->base;
    while (to < state->stack.top) {
        *to = *from;
        to++;
        from++;
    }
    for (int i = 0; i < REGISTER_COUNT; i++) {
        state->locals_in_registers[i] = 0;
    }
    for (int i = 0; i < 256; i++) {
        state->registers_in_locals[i] = -1;
    }
    state->unused_registers = -1;
    state->unused_spills = -1;
}

Virtual reload_spill(Virtual v, GVMT_Buffer out, GVMT_LocalState state) {
    Virtual reg = gvmt_reg_alloc(out, state);
    assert(v->kind == SPILL_KIND);
    assert(reg->kind == REGISTER_KIND);
    gvmt_emit_load_memory(reg->u.r.index, v->u.s.address, out);
    Variable *s;
    FOR_EACH_ON_STACK(s, &state->stack) {
        if (*s == v) {
            *s = reg;
        } 
    }
    return reg;
}

#define REGISTER_INDEX(r) ((r)-registers)

#define SPILL_INDEX(s) ((s)-spills)

static Virtual spill_register(Virtual r, GVMT_Buffer out, GVMT_Stack stack) {
    assert(r->kind == REGISTER_KIND);
    assert(r->u.r.locked == 0);
    int32_t used_spills = 0;
    Virtual *s;
    FOR_EACH_ON_STACK(s, stack) {
        if((*s)->kind == SPILL_KIND)
            used_spills |= (1 << SPILL_INDEX(*s));
    }
    int32_t unused_spills = ~used_spills;
//  gvmt_printf(out, "/* Spilling register %d */\n", r->u.r.index);
    assert(unused_spills);
    Virtual spill = &spills[trailing_zeroes(unused_spills)];
    gvmt_emit_store_memory(r->u.r.index, spill->u.s.address, out);
    FOR_EACH_ON_STACK(s, stack) {
        if (*s == r) {
            *s = spill;
        } 
    }
    return spill;
}

#define LOCAL_INDEX(l) ((l) - (frame + GVMT_LOCALS_INDEX))

static void break_local_reg_link(Variable r, GVMT_LocalState local_state) {
    assert(r->kind == REGISTER_KIND);
    Memory l = local_state->locals_in_registers[r->u.r.index];
    if (l) {
        local_state->locals_in_registers[r->u.r.index] = 0;
        local_state->registers_in_locals[LOCAL_INDEX(l)] = -1;
    }
}

Variable lowest_register_on_stack(GVMT_Stack stack) {
    Variable *s;
    int32_t regs_found = 0;
    Variable r = 0;
    for(s = stack->top-1; s >= stack->base; s--) {
        if ((*s)->kind == REGISTER_KIND && (!((*s)->u.r.locked))) {
            int index = (*s)->u.r.index;
            if ((regs_found & (1 << index)) == 0) {
                regs_found |= (1 << index);
                r = *s;
            }
        }
    }
    return r;
}

static uint32_t all_registers_on_stack(GVMT_Stack stack) {
    uint32_t all_regs = 0;
    Variable *s;
    FOR_EACH_ON_STACK(s, stack) {
        if((*s)->kind == REGISTER_KIND) {
            all_regs |= (1 << REGISTER_INDEX(*s));
        }
    }
    return all_regs;
}

Variable gvmt_reg_alloc(GVMT_Buffer out, GVMT_LocalState local_state) {
    Variable local = 0;
    Variable *s;
    uint32_t used_regs = all_registers_on_stack(&local_state->stack);
    uint32_t unused_regs_without_local = ~used_regs;
    for (int i = 0; i < REGISTER_COUNT; i++) {
        if (local_state->locals_in_registers[i] != 0)
            unused_regs_without_local &= ~(1 << i);
    }
    if (unused_regs_without_local & ALL_REGISTERS_MASK) {
        Virtual reg = gvmt_preferred_register(unused_regs_without_local & ALL_REGISTERS_MASK);
        local_state->unused_registers &= ~(1 << REGISTER_INDEX(reg));
        return reg;
    }
    int unused_regs  = ~used_regs;
    if (unused_regs & ALL_REGISTERS_MASK) {
        Virtual reg = gvmt_preferred_register(unused_regs & ALL_REGISTERS_MASK);
        local_state->unused_registers &= ~(1 << REGISTER_INDEX(reg));
        break_local_reg_link(reg, local_state);
        return reg;
    }
    // Need to spill
    Variable r = lowest_register_on_stack(&local_state->stack);
    assert(r);
    spill_register(r, out, &local_state->stack);
    break_local_reg_link(r, local_state);
    local_state->unused_registers &= ~(1 << r->u.r.index);
    return r;
}

Virtual as_register(Virtual v, GVMT_Buffer out, GVMT_LocalState local_state) {
    if (v->kind == REGISTER_KIND)
        return v;
    if (v->kind == SPILL_KIND)
        return reload_spill(v, out, local_state);
    assert(v->kind == VALUE_KIND);
    Virtual reg;
    reg = gvmt_reg_alloc(out, local_state);
    gvmt_emit_load_const(reg->u.r.index, v->u.v.value, out);
    return reg;
}

Virtual load_from_memory(GVMT_Buffer out, GVMT_LocalState state, Memory m) {
    Virtual reg = gvmt_reg_alloc(out, state);
    gvmt_emit_load_memory(reg->u.r.index, m->address, out);
    return reg;
}

Virtual gvmt_temp_fetch(int index, GVMT_Buffer out, GVMT_LocalState state) {
    return load_from_memory(out, state, &(temps[index]));
}

Virtual gvmt_frame_fetch(int index, GVMT_Buffer out, GVMT_LocalState state) {
    return load_from_memory(out, state, &(frame[index]));
}

Virtual gvmt_fetch(int index, GVMT_Buffer out, GVMT_LocalState state) {
    return load_from_memory(out, state, &(frame[index + GVMT_LOCALS_INDEX]));
}

static void store_to_memory(Virtual value, GVMT_Buffer out, Memory m, GVMT_LocalState state) {
    if (value->kind == VALUE_KIND) {
        gvmt_emit_store_const(m->address, value->u.v.value, out);
        return;
    }
    if (value->kind == SPILL_KIND) {
#ifdef GVMT_HAVE_MEM_MEM_MOVE
       gvmt_emit_mem_mem_move(value->u.s.address, m->address);
       return;
#else
       value = reload_spill(value, out, state);
       assert(value->kind == REGISTER_KIND);
       gvmt_emit_store_memory(REGISTER_INDEX(value), m->address, out);
#endif      
    }
}

void gvmt_temp_store(int index, Virtual value, GVMT_Buffer out, GVMT_LocalState state) {
    store_to_memory(value, out, &(temps[index]), state);
}

void gvmt_frame_store(int index, Virtual value, GVMT_Buffer out, GVMT_LocalState state) {
    store_to_memory(value, out, &(frame[index]), state);
}

void gvmt_store(int index, Virtual value, GVMT_Buffer out, GVMT_LocalState state) {
    store_to_memory(value, out, &(frame[index + GVMT_LOCALS_INDEX]), state);
}

void gvmt_array_store(int size, Variable array, int offset, Variable value, GVMT_Buffer out, GVMT_LocalState state) {
   array = as_register(array, out, state);
//   LOCK(array);
   if (value->kind == SPILL_KIND)
       value = reload_spill(value, out, state);
//   UNLOCK(array);
   if (value->kind == VALUE_KIND) {
       assert(size == sizeof(void*));
       gvmt_emit_store_const_offset(array->u.r.index, size, offset, value->u.v.value, out);
   } else {
       assert(value->kind == REGISTER_KIND);
       gvmt_emit_store_reg_offset(array->u.r.index, size, offset, value->u.r.index, out);
   }
}

Variable gvmt_array_fetch(int size, Variable array, int offset, GVMT_Buffer out, GVMT_LocalState state) {
   if (array->kind == SPILL_KIND)
       array = as_register(array, out, state);
    Variable result = gvmt_reg_alloc(out, state);
    assert(array->kind == REGISTER_KIND);
    assert(result->kind == REGISTER_KIND);
    gvmt_emit_load_reg_offset(array->u.r.index, size, offset, result->u.r.index, out);
    return result;
}

GVMT_Object gvmt_unwrap(Virtual var) {
     assert(var->kind == VALUE_KIND);
     return var->u.v.value;
}

#define VALUE_INDEX(v) ((v) - values)

Virtual gvmt_wrap(GVMT_Object val, GVMT_Stack stack) {
    int32_t used_values = 0;
    Virtual *s;
    FOR_EACH_ON_STACK(s, stack) {
        if((*s)->kind == VALUE_KIND)
            used_values |= (1 << VALUE_INDEX(*s));
    }
    int32_t unused_values = ~used_values;
    assert(unused_values);
    Virtual value = &values[trailing_zeroes(unused_values)];
    value->u.v.value = val;
    return value;
}

void rotate_up(GVMT_Stack stack, int count) {
    Variable temp = stack->top[-count];
    for (int i = 1; i < count; i++) {
        stack->top[-i] = stack->top[-i-1];
    }
    stack->top[-1] = temp;
}

void gvmt_stack_implode(GVMT_LocalState state, Variable array, int count, int offset, GVMT_Buffer out) {
    array = as_register(array, out, state);
    LOCK(array);
    for (int i = 0; i < count; i++) {
        if (offset) {
            rotate_up(&state->stack, offset + 1);
        }
        Variable item = GVMT_POP(&state->stack);
        gvmt_array_store(sizeof(void*), array, i*sizeof(void*), item, out, state);
    }
    UNLOCK(array);
}

void gvmt_stack_explode(GVMT_LocalState state, int count, GVMT_Buffer out) {
    GVMT_Stack stack = &state->stack;
    Variable array = stack->top[-1];
    if (array->kind == SPILL_KIND) {
        array = reload_spill(array, out, state);
    }
    for (int i = 0; i < count; i++) {
        Variable reg = gvmt_array_fetch(sizeof(void*), array, i*sizeof(void*), out, state);
        stack->top[-1] = reg;
        stack->top[0] = array;
        stack->top++;
    }
    stack->top--;  
}

void move_to_memory(Virtual v, int address, GVMT_Buffer out) {
    if (v->kind == REGISTER_KIND) {
        gvmt_emit_store_memory(v->u.r.index, address, out);
    } else {
        gvmt_emit_mem_mem_move(v->u.s.address, address, out);
    }
}

void move_to_register(Virtual v, int reg, GVMT_Buffer out) {
    if (v->kind == REGISTER_KIND) {
        gvmt_emit_reg_reg_move(v->u.r.index, reg, out);
    } else {
        gvmt_emit_load_memory(v->u.s.address, reg, out);
    }
}

void transform_stack(GVMT_Stack from, GVMT_Stack to, Arena arena, GVMT_Buffer out) {
    printf("Transforming stack\n");   
    int depth = from->top - from->base; 
    BitSet to_be_moved = set_new(depth, arena);
    BitSet safe_regs = set_new(REGISTER_COUNT, arena);
    set_invert(safe_regs);
    set_invert(to_be_moved);
    for (int i = 0; i < depth; i++) {
        if (from->base[i]->kind == REGISTER_KIND)
            set_remove(safe_regs, REGISTER_INDEX(from->base[i]));
    }
    int count, new_count = depth;
    do {
        count = new_count;
        for (int i = 0; i < depth; i++) {
            if (set_contains(to_be_moved, i)) {
                if (to->base[i]->kind == SPILL_KIND) {
                    move_to_memory(from->base[i], to->base[i]->u.s.address, out);
                    set_remove(to_be_moved, i);
                } else {
                    assert(to->base[i]->kind == REGISTER_KIND);
                    if (set_contains(safe_regs, REGISTER_INDEX(to->base[i]))) {
                        move_to_register(from->base[i], REGISTER_INDEX(to->base[i]), out);
                        set_remove(to_be_moved, i);
                    }
                }
            }
        }
        int new_count = set_size(to_be_moved);
        if (new_count == 0)
            return;
    } while (count != new_count);
    do {
        int i;
        for (i = 0; i < depth; i++) {
            if (from->base[i]->kind == REGISTER_KIND) {
                if (set_contains(to_be_moved, i) && set_contains(safe_regs, i)) {
                    Virtual v = spill_register(from->base[i], out, from);
                    set_add(safe_regs, REGISTER_INDEX(to->base[i]));
                    break;
                }
            }
        }
        // Should have broken out of loop - Otherwise no new regs available.
        assert(i < depth);
        for (int i = 0; i < depth; i++) {
            if (set_contains(to_be_moved, i) && 
                to->base[i]->kind == REGISTER_KIND &&
                set_contains(safe_regs, REGISTER_INDEX(to->base[i]))) {
                    move_to_register(from->base[i], REGISTER_INDEX(to->base[i]), out);
                    set_remove(to_be_moved, i);
            }
        }
    } while (!set_empty(to_be_moved));
}

void reg_alloc_synch_to_state(GVMT_EdgeSet es, GVMT_Stack stack, Arena arena, GVMT_Buffer out) {
    if (es == 0)
        return;
    if (es->stack) {
        transform_stack(stack, es->stack, arena, out);
    } else {
        int depth = stack->top - stack->base; 
        es->stack = gvmt_allocate(sizeof(struct gvmt_stack), arena);
        es->stack->base = gvmt_allocate(sizeof(Variable) * depth, arena);
        es->stack->top = es->stack->base + depth;
        for (int i = 0; i < depth; i++) {
            Variable item = stack->base[i];
            assert(item->kind == REGISTER_KIND || item->kind == SPILL_KIND);
            es->stack->base[i] = item;
        }
    }
}

void init_local_register_state(struct gvmt_local_register_state* local_register_state) {
    local_register_state->unused_spills = -1;
    local_register_state->unused_registers = -1;
    for (int i = 0; i < REGISTER_COUNT; i++) {        
        local_register_state->locals_in_registers[i] = 0;
    }
    for (int i = 0; i < 256; i++) {
        local_register_state->registers_in_locals[i] = -1;
    }
}

Variable push_value_base_stack(GVMT_Stack block_start_stack, struct gvmt_local_register_state* local_register_state) {
    Variable result;
    for(Virtual* item = block_start_stack->top; item > block_start_stack->base; item--) {
        item[0] = item[-1];
           
    }
    block_start_stack->top++;
    int reg = trailing_zeroes(local_register_state->unused_registers);
    if (reg < REGISTER_COUNT) {
        local_register_state->unused_registers &= ~(1 << reg);
        result = &registers[reg];
    } else {
        int spill = trailing_zeroes(local_register_state->unused_spills);
        assert(spill < 32);
        local_register_state->unused_spills &= ~(1 << spill);
        result = &spills[spill];
    }
    block_start_stack->base[0] = result;
    return result;
}

/** X86 version */

#define ALL_REGS 0x3f
#define NOT_A 0x3e

Variable gvmt_preferred_register(int possibles) {
    assert(possibles & ALL_REGS);
    if (possibles & NOT_A)
        return &registers[trailing_zeroes(possibles & NOT_A)];
    else
        return &registers[0];
}

