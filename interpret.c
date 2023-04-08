#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*
ISA

Registers │ Description    │ Name  │ Saver
x0        │ zero           │ x0    │ N/A
x1        │ negative one   │ x1    │ N/A
x2        │ return address │ ra    │ Caller
x3        │ stack pointer  │ sp    │ Caller
x4-x6     │ arguments      │ a0-a2 │ Callee
x7-x10    │ saved          │ s0-s3 │ Caller
x11-x14   │ temporaries    │ t0-t3 │ Caller
x15       │                │ t4    │ N/A

Opcode │ Type │ usage                  │ action                           │ memRead │ ALUOp │ useImm │ useRd │ regWrite
0      │ A    │ add rd, rs1, rs2       │ rd = rs1 + rs2                   │ 0       │ 0     │ 0      │ 0     │ 1       
1      │ C    │ addi rd, rs1, imm[3:0] │ rd = rs1 + imm[3:0]              │ 0       │ 0     │ 1      │ 0     │ 1       
2      │ B    │ inc rd, imm[7:0]       │ rd += imm[7:0]                   │ 0       │ 0     │ 1      │ 1     │ 1       
3      │ A    │ sub rd, rs1, rs2       │ rd = rs1 - rs2                   │ 0       │ 1     │ 0      │ 0     │ 1       
4      │ A    │ shl rd, rs1, rs2       │ rd = rs1 << rs2                  │ 0       │ 2     │ 0      │ 0     │ 1       
5      │ A    │ and rd, rs1, rs2       │ rd = rs1 & rs2                   │ 0       │ 3     │ 0      │ 0     │ 1       
6      │ A    │ or  rd, rs1, rs2       │ rd = rs1 | rs2                   │ 0       │ 4     │ 0      │ 0     │ 1       
7      │ A    │ xor rd, rs1, rs2       │ rd = rs1 ^ rs2                   │ 0       │ 5     │ 0      │ 0     │ 1       
8      │ C    │ lw  rd, rs1, imm[3:0]  │ rd = Mem[rs1 + imm[3:0]]         │ 1       │ 0     │ 1      │ 0     │ 1       
9      │ C    │ sw  rd, rs1, imm[3:0]  │ Mem[rs1 + imm[3:0]] = rd         │ 0       │ 0     │ 1      │ 0     │ 0       
10     │ B    │ li  rd, imm[7:0]       │ rd = imm[7:0]                    │ 0       │ 8     │ 1      │ 1     │ 1       
11     │ B    │ lui rd, imm[7:0]       │ rd = imm[7:0] << 8               │ 0       │ 9     │ 1      │ 1     │ 1       
12     │ A    │ eq  rd, rs1, rs2       │ rd = rs1 == rs2                  │ 0       │ 6     │ 0      │ 0     │ 1       
13     │ A    │ lt  rd, rs1, rs2       │ rd = rs1 < rs2                   │ 0       │ 7     │ 0      │ 0     │ 1       
14     │ B    │ bnz rd, imm[7:0]       │ if(rd) PC += imm[7:0]            │ 0       │ 8     │ 1      │ 1     │ 0       
15     │ C    │ jal rd, rs1, imm[3:0]  │ rd = PC + 2; PC = rs1 + imm[3:0] │ 0       │ 0     │ 1      │ 0     │ 1       

ALUOp │ Action
0     │ A + B
1     │ A - B
2     │ A << B
3     │ A & B
4     │ A | B
5     │ A ^ B
6     │ A == B
7     │ A < B
8     │ B
9     │ B << 8

Datapath:
  ┌───────────────────┐   ┌───────────┐                            ┌──────────┐  
  │        Key        │   │  Control  │                 ┌─────┐    │  Memory  │  
  ├───────────────────┤   ├───────────┤  ┌──────────────╪ Nor ├──┐ ├──────────┤  
  ╪ input1    output1 ┼   │   memRead ┼──│───────┐      └──╥──┘  └─╪ memWrite │  
  ╡ input2    output2 ├   │     ALUOp ┼──│─────┐ └─────────│─────┬─╪ memRead  │  
  └───────────────────┘   │    useImm ┼──│───┐ │           │ ┌───│─╪ addr     │  
  ┌──[12-15]──────────────╡     useRd ┼──│─┐ │ │ ┌───────┐ │ │ ┌─│─╪ data     │  
  │         ┌───────────┐ │  regWrite ┼──┤ │ │ │ │  ALU  │ │ │ │ │ └───┬──────┘  
  │         │ Registers │ └───────────┘  │ │ │ │ ├───────┤ │ │ │ └─┐   │      
  │         ├───────────┤ ┌──────────────┘ │ │ └─╪ ALUOp │ │ │ │ ┌─╨─┐ │    
  │         │  regWrite ╪─┘     ┌──────────┘ │   │       │ │ │ │ │ 1 ╪─┘   
  │         │           │   0 ┌─╨─┐ ┌────────│───╪ A     │ │ ├─│─╪ 0 │     
  ├──[4-7]──╪ rs1     a ┼─────╪ 0 ├─┘    1 ┌─╨─┐ │       ┼─│─┘ │ └─┬─┘      
  │         │           │ ┌───╪ 1 │ ┌──────╪ 0 ├─╪ B     │ │   │   │             
  ├──[0-3]──╪ rs2     b ┼─│─┐ └───┘ │    ┌─╪ 1 │ └───────┘ │   │   │             
  │         │           │ │ └───────┘    │ └───┘           └─┐ │   │           
  ├──[8-11]─╪ rd      c ┼─┴──────────────│───────────────────│─┘   │           
  │         │           │ ┌────────────┐ │         ┌─────────│─────┤  └──┬──┘    
  │       ┌─╪ data      │ │   ImmGen   │ │ 5 ┌───┐ │ ┌───┐ 4 │     │     │       
  │       │ └───────────┘ ├────────────┤ │ ┌─┤ 0 ╪─┴─╪ 1 ├─┐ │     │   ┌─╨─┐ 3   
  ├───────│───────────────╪ instr  imm ┼─┘ │ │ 1 ╪─┬─╪ 0 │ │ │     └───╪ 1 ├──┐  
  │       └─────────┐     └────────────┘   │ └─╥─┘ │ └─╥─┘ │ │   (2) ──╪ 0 │  │  
  │ ┌─────────────┐ └──────────────────────┘   ├───│───┘   │ │         └───┘  │  
  │ │   Program   │ ┌──────────────────────────┘   └───────│─│───┐ ┌───────┐  │  
  │ ├─────────────┤ │  Justin DeWitt                       │ │   │ │  Add  │  │  
  └─┼ instr   jal ┼─┘  1/18/23                             │ │   │ ├───────┤  │  
    │         bnz ┼────────────────────────────────────────│─┘   └─┼     A ╪──┘  
    │          PC ┼────────────────────────────────────────│─────┐ │     B ╪──┐  
    │        next ╪────────────────────────────────────────┘     │ └───────┘  │  
    └─────────────┘                                              └────────────┘  

*/

typedef uint16_t wire;

#define bits(e,s) ((instr >> s) & ((1 << e - s + 1) - 1))
#define bit(n) ((instr >> n) & 1)

//PROCESSOR
#pragma region 
typedef struct breakpoint {
    uint16_t pc;
    int16_t flags;
    char *debug_msg;
} breakpoint;

typedef struct label {
    uint16_t pc;
    char *name;
} label;

typedef struct processor {
    char memory[UINT16_MAX];
    int16_t registers[16];
    uint16_t PC;
    uint16_t instructions;
    uint16_t breakpoint_count;
    breakpoint *breakpoints;
    uint16_t label_count;
    uint16_t label_ref_count;
    label *labels;
    label *label_refs;
} processor;

processor *processor_new() {
    processor *p = calloc(1, sizeof(processor));
    p->registers[1] = -1;
    p->registers[3] = 0x7FFF;
    p->breakpoints = malloc(4 * sizeof(breakpoint));
    p->labels = malloc(4 * sizeof(label));
    p->label_refs = malloc(4 * sizeof(label));
}

void processor_free(processor *p) {
    free(p->breakpoints);
    free(p->label_refs);
    free(p->labels);
    free(p);
}

void processor_push_instr(processor *p, uint16_t instr) {
    *(uint16_t*)(p->memory + p->PC) = instr;
    p->PC += 2;
    p->instructions++;
}

int processor_load(processor *p, FILE *fp) {
    void skip_whitespace(char *buf, int *index);
    int parse_label(processor *p, char *buf);
    int parse_type_a(processor *p, int opcode, char *buf);
    int parse_type_b(processor *p, int opcode, char *buf);
    int parse_type_c(processor *p, int opcode, char *buf);
    int parse_debug(processor *p, char *buf);
    int parse_pause(processor *p, char *buf);
    char buf[512];
    int index, result, errors = 0;
    for(int line = 1; fgets(buf, 512, fp); line++) {
        index = 0;
        skip_whitespace(buf, &index);
        if(!strncmp("//", buf + index, 2)) continue; //skip comments
        if(buf[index] == 0) continue;
        if(buf[index] == ':') result = parse_label(p, buf + index + 1);
        else if(!strncmp("pause", buf + index, 5)) result = parse_pause(p, buf + index + 5);
        else if(!strncmp("debug", buf + index, 5)) result = parse_debug(p, buf + index + 5);
        else if(!strncmp("add",  buf + index, 3)) result = parse_type_a(p, 0,  buf + index + 3);
        else if(!strncmp("addi", buf + index, 4)) result = parse_type_c(p, 1,  buf + index + 4);
        else if(!strncmp("inc",  buf + index, 3)) result = parse_type_b(p, 2,  buf + index + 3);
        else if(!strncmp("sub",  buf + index, 3)) result = parse_type_a(p, 3,  buf + index + 3);
        else if(!strncmp("shl",  buf + index, 3)) result = parse_type_a(p, 4,  buf + index + 3);
        else if(!strncmp("and",  buf + index, 3)) result = parse_type_a(p, 5,  buf + index + 3);
        else if(!strncmp("or",   buf + index, 2)) result = parse_type_a(p, 6,  buf + index + 2);
        else if(!strncmp("xor",  buf + index, 3)) result = parse_type_a(p, 7,  buf + index + 3);
        else if(!strncmp("lw",   buf + index, 2)) result = parse_type_c(p, 8,  buf + index + 2);
        else if(!strncmp("sw",   buf + index, 2)) result = parse_type_c(p, 9,  buf + index + 2);
        else if(!strncmp("li",   buf + index, 2)) result = parse_type_b(p, 10, buf + index + 2);
        else if(!strncmp("lui",  buf + index, 3)) result = parse_type_b(p, 11, buf + index + 3);
        else if(!strncmp("eq",   buf + index, 2)) result = parse_type_a(p, 12, buf + index + 2);
        else if(!strncmp("lt",   buf + index, 2)) result = parse_type_a(p, 13, buf + index + 2);
        else if(!strncmp("bnz",  buf + index, 3)) result = parse_type_b(p, 14, buf + index + 3);
        else if(!strncmp("jal",  buf + index, 3)) result = parse_type_c(p, 15, buf + index + 3);
        else {
            result = -1;
            printf("Unrecognized command at [%s]\n", buf + index);
        }
        if(result < 0) {
            buf[strlen(buf) - 1] = 0;
            printf(" line %d\n       [%s]\n", line, buf);
            errors++;
        }
    }
    return errors;
}
#pragma endregion

//INTERPRETER
#pragma region

void interpret_control(processor *p, int16_t opcode, int *mem_read, int16_t *ALU_op, int *use_imm, int *use_rd, int *reg_write) {
    switch(opcode) {
        case 0:  *mem_read = 0; *ALU_op = 0; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 1:  *mem_read = 0; *ALU_op = 0; *use_imm = 1; *use_rd = 0; *reg_write = 1; break;
        case 2:  *mem_read = 0; *ALU_op = 0; *use_imm = 1; *use_rd = 1; *reg_write = 1; break;
        case 3:  *mem_read = 0; *ALU_op = 1; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 4:  *mem_read = 0; *ALU_op = 2; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 5:  *mem_read = 0; *ALU_op = 3; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 6:  *mem_read = 0; *ALU_op = 4; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 7:  *mem_read = 0; *ALU_op = 5; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 8:  *mem_read = 1; *ALU_op = 0; *use_imm = 1; *use_rd = 0; *reg_write = 1; break;
        case 9:  *mem_read = 0; *ALU_op = 0; *use_imm = 1; *use_rd = 0; *reg_write = 0; break;
        case 10: *mem_read = 0; *ALU_op = 8; *use_imm = 1; *use_rd = 1; *reg_write = 1; break;
        case 11: *mem_read = 0; *ALU_op = 9; *use_imm = 1; *use_rd = 1; *reg_write = 1; break;
        case 12: *mem_read = 0; *ALU_op = 6; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 13: *mem_read = 0; *ALU_op = 7; *use_imm = 0; *use_rd = 0; *reg_write = 1; break;
        case 14: *mem_read = 0; *ALU_op = 8; *use_imm = 1; *use_rd = 1; *reg_write = 0; break;
        case 15: *mem_read = 0; *ALU_op = 0; *use_imm = 1; *use_rd = 0; *reg_write = 1; break;
    }
}

void interpret_reg_file(processor *p, int rs1, int rs2, int rd, int reg_write, int data, int16_t *rs1_out, int16_t *rs2_out, int16_t *rd_out) {
    *rs1_out = p->registers[rs1];
    *rs2_out = p->registers[rs2];
    *rd_out = p->registers[rd];

    if(reg_write && rd > 1) {
        p->registers[rd] = data;
    }
}

void interpret_imm_gen(processor *p, uint16_t instr, int16_t *imm) {
    switch(bits(15, 12)) {
        case 1:  
        case 8:  
        case 9:  
        case 15: *imm = (bits(3, 0) << 28 >> 28); break; //the shifting is to make the value signed
        case 2:  
        case 10: 
        case 11: 
        case 14: *imm = (bits(7, 0) << 24 >> 24); break; //the shifting is to make the value signed
    }
}

void interpret_ALU(processor *p, int ALU_op, int16_t A, int16_t B, int16_t *ALU_out) {
    switch (ALU_op) {
        case 0: *ALU_out = A + B;    break;
        case 1: *ALU_out = A - B;    break;
        case 2: *ALU_out = A << B;   break;
        case 3: *ALU_out = A & B;    break;
        case 4: *ALU_out = A | B;    break;
        case 5: *ALU_out = A ^ B;    break;
        case 6: *ALU_out = (A == B); break;
        case 7: *ALU_out = (A < B);  break;
        case 8: *ALU_out = B;        break;
        case 9: *ALU_out = (B << 8); break;
    }
}

void interpret_memory(processor *p, int mem_read, int mem_write, int16_t addr, int16_t data, int16_t *mem_out) {
    if(mem_read) {
        *mem_out = ((int16_t*)p->memory)[(uint16_t)addr];
    }
    if(mem_write) {
        ((int16_t*)p->memory)[(uint16_t)addr] = data;
    }
}

void interpret(processor *p, int debug) {
    p->PC = 0;
    p->registers[0] = 0;
    p->registers[1] = -1;
    p->registers[3] = 0x7FFF;
    int next_bp = 0;
    if(debug) printf("addr   | instruction\n");
    void print_instruction(uint16_t instr);
    int cycle = 0;

    while(1) {

        //program unit
        uint16_t instr = *(uint16_t*)(p->memory + p->PC);
        int bnz = (bits(15, 12) == 14);
        int jal = (bits(15, 12) == 15);

        if(debug) {
            printf("0x%04X | ", p->PC);
            print_instruction(instr);
            printf("\n", instr);
        }

        //control
        int mem_read;
        int16_t ALU_op;
        int use_imm;
        int use_rd;
        int reg_write;
        interpret_control(p, bits(15, 12), &mem_read, &ALU_op, &use_imm, &use_rd, &reg_write);

        //ImmGen
        int16_t imm;
        interpret_imm_gen(p, instr, &imm);

        //RegFile read
        int16_t data;
        int16_t rd_out = p->registers[bits(11, 8)];
        int16_t rs1_out = p->registers[bits(7, 4)];
        int16_t rs2_out = p->registers[bits(3, 0)];

        interpret_reg_file(p, bits(7, 4), bits(3, 0), bits(11, 8), 0, data, &rs1_out, &rs2_out, &rd_out);
        
        //ALU
        int16_t ALU_out;
        int16_t A = use_rd ? rd_out : rs1_out;
        int16_t B = use_imm ? imm : rs2_out;
        interpret_ALU(p, ALU_op, A, B, &ALU_out);

        //Memory
        int16_t mem_out;
        int mem_write = !reg_write && !bnz;
        
        //IO
        if(ALU_out == 0x400 && bits(15, 12) == 8) {
            int i;
            printf("input: ");
            fscanf(stdin, "%i", &i);
            ((int16_t*)p->memory)[0x400] = i;
        }

        interpret_memory(p, mem_read, mem_write, ALU_out, rd_out, &mem_out);

        if(ALU_out == 0x402 && bits(15, 12) == 9) {
            printf("%i\n", ((int16_t*)p->memory)[0x402]);
            printf("cycles: %i\n", cycle);
            exit(0);
        }
        
        //muxes
        int16_t mux2_out = mem_read ? mem_out : ALU_out;
        int not_zero = (rd_out != 0);
        int16_t and_value = bnz && not_zero;

        int16_t add_in = and_value ? mux2_out : 2;
        uint16_t add_out = add_in + p->PC;

        uint16_t next_PC = jal ? mux2_out : add_out;
        data = jal ? add_out : mux2_out;

        //RegFile write
        interpret_reg_file(p, bits(7, 4), bits(3, 0), bits(11, 8), reg_write, data, &rs1_out, &rs2_out, &rd_out);
        
        // fprintf(fp, "sample_mux2_out[%i] = %i;\n", cycle - 1, mux2_out);
        // fprintf(fp, "sample_not_zero[%i] = %i;\n\n", cycle - 1, not_zero);
        
        //debug stuff
        cycle++;

        if(next_PC == p->PC) return;
        p->PC = next_PC;

        if(jal || bnz) {
            for(int i = p->breakpoint_count - 1; i >= 0 && p->breakpoints[i].pc >= p->PC; i--)
                next_bp = i;
        }
        if(p->PC == p->breakpoints[next_bp].pc) {
            char *str = p->breakpoints[next_bp].debug_msg;
            if(str) {
                #define reg(n) p->registers[str[n + 1]]
                switch (str[0]) {
                    case 0:  printf(str + 1); break;
                    case 1:  printf(str + 2, reg(0)); break;
                    case 2:  printf(str + 3, reg(0), reg(1)); break;
                    case 3:  printf(str + 5, reg(0), reg(1), reg(2)); break;
                    case 4:  printf(str + 6, reg(0), reg(1), reg(2), reg(3)); break;
                    case 5:  printf(str + 7, reg(0), reg(1), reg(2), reg(3), reg(4)); break;
                    default: printf(str + 8, reg(0), reg(1), reg(2), reg(3), reg(4), reg(5)); break;
                }
                printf("\n");
                #undef reg
            }
            next_bp++;
        }
        #undef rd
        #undef rs1
        #undef rs2
    }
}


#pragma endregion

//PARSER
#pragma region 
void skip_whitespace(char *buf, int *index) {
    while(buf[*index] && strchr(" \t\r\n", buf[*index])) (*index)++;
}

int parse_comma(char *buf, int *index) {
    skip_whitespace(buf, index);
    if(buf[*index] != ',') {
        printf("Expected comma at [%s]\n", buf);
        return -1;
    }
    (*index)++;
    return 0;
}

int parse_register(char *buf, int *index) {
    int r, tmp;
    skip_whitespace(buf, index);
    char letter = buf[*index];
    if(!strncmp(buf + *index, "ra", 2)) {
        (*index) += 2;
        return 2;
    }
    if(!strncmp(buf + *index, "sp", 2)) {
        (*index) += 2;
        return 3;
    }
    (*index)++;
    if(sscanf(buf + *index, "%i%n", &r, &tmp) != 1) {
        return -1;
    }
    (*index) += tmp;
    switch (letter) {
        case 'a':
            if(r < 0 || r > 2) goto ERR;
            return r + 4;
        case 's':
            if(r < 0 || r > 3) goto ERR;
            return r + 7;
        case 't':
            if(r < 0 || r > 3) goto ERR;
            return r + 11;
        case 'x':
            if(r < 0 || r > 15) goto ERR;
            return r;
    }
    ERR:
    return -1;
}

int parse_pause(processor *p, char *buf) {
    breakpoint bp = (breakpoint){ .debug_msg = 0, .pc = p->PC };
    if(p->breakpoint_count > 3 && !((p->breakpoint_count - 1) & p->breakpoint_count))
        p->breakpoints = realloc(p->breakpoints, p->breakpoint_count * 2 * sizeof(breakpoint));
    p->breakpoints[p->breakpoint_count++] = bp;
}
int parse_debug(processor *p, char *buf) {
    int index = 0;
    skip_whitespace(buf, &index);
    if(buf[index] != '"') {
        printf("Expected debug string on");
        return -1;
    }
    int start = ++index;
 
    int expected = 0;
    for(; buf[index] != '"'; index++) {
        if(buf[index] == '%') {
            if(buf[index + 1] == '%') index++;
            else expected++;
        }
    }

    int len = index - start;
    char *debug_msg = malloc(len + expected + 1);
    memcpy(debug_msg + expected + 1, buf + start, len);
    debug_msg[0] = expected;

    index++;
    for(int i = 0; i < expected; i++) {
        parse_comma(buf, &index);
        if((debug_msg[i + 1] = parse_register(buf, &index)) < 0) {
            printf("Error: Malformed register on");
            free(debug_msg);
            return -1;
        }
    }
    breakpoint bp = (breakpoint){ .debug_msg = debug_msg, .pc = p->PC };
    if(p->breakpoint_count > 3 && !((p->breakpoint_count - 1) & p->breakpoint_count))
        p->breakpoints = realloc(p->breakpoints, p->breakpoint_count * 2 * sizeof(breakpoint));
    p->breakpoints[p->breakpoint_count++] = bp;
    return 0;
}

int parse_label(processor *p, char *buf) {
    char id[256];
    if(sscanf(buf, "%s", id) != 1) {
        printf("Error: Expected label definition on");
        return -1;
    }
    //add label
    int len = strlen(id);
    char *copy = malloc(len + 1);
    memcpy(copy, id, len + 1);
    if(p->label_count > 3 && !((p->label_count - 1) & p->label_count))
        p->labels = realloc(p->labels, p->label_count * 2 * sizeof(label));
    p->labels[p->label_count++] = (label){ .pc = p->PC, .name = copy };

    //search label refs and apply updates
    for(int i = 0; i < p->label_ref_count; i++) {
        if(strcmp(p->label_refs[i].name, id)) continue;
        uint16_t *instr = (uint16_t*)(p->memory + p->label_refs[i].pc);
        int opcode = (*instr >> 12);
        if(opcode == 14)
            *(char*)instr = p->PC - p->label_refs[i].pc;
        else
            *(char*)instr = p->PC;
    }
    return 0;
}

int parse_type_a(processor *p, int opcode, char *buf) {
    int index = 0, rd, rs1, rs2;
    if((rd = parse_register(buf, &index)) < 0) {
        printf("Error: Malformed register on");
        return -1;
    }
    if(parse_comma(buf, &index) < 0) {
        printf("Error: expected comma after register on");
        return -1;
    }
    if((rs1 = parse_register(buf, &index)) < 0) {
        printf("Error: Malformed 2nd register on");
        return -1;
    }
    if(parse_comma(buf, &index) < 0) {
        printf("Error: expected comma after 2nd register on");
        return -1;
    }
    if((rs2 = parse_register(buf, &index)) < 0) {
        printf("Error: Malformed 3rd register on");
        return -1;
    }
    processor_push_instr(p, (opcode << 12) + (rd << 8) + (rs1 << 4) + rs2);
    return 0;
}

int parse_type_b(processor *p, int opcode, char *buf) {
    int index = 0, rd, imm = 0;
    char id[256];
    if((rd = parse_register(buf, &index)) < 0) return -1;
    if(parse_comma(buf, &index) < 0) return -1;
    if(sscanf(buf + index, "%i", &imm) == 1);
    else if(sscanf(buf + index, "%s", id) == 1) {
        int len = strlen(id);
        char *copy = malloc(len + 1);
        memcpy(copy, id, len + 1);
        char found = 0;

        //add label ref
        if(p->label_ref_count > 3 && !((p->label_ref_count - 1) & p->label_ref_count))
            p->label_refs = realloc(p->label_refs, p->label_ref_count * 2 * sizeof(label));
        p->label_refs[p->label_ref_count++] = (label){ .pc = p->PC, .name = copy };
        //search for label
        for(int i = 0; i < p->label_count; i++) {
            if(strcmp(p->labels[i].name, id)) continue;
            if(opcode == 14)
                imm = p->labels[i].pc - p->PC;
            else
                imm = p->labels[i].pc;
            break;
        }
    } else {
        printf("Error: Expected immediate or label after register on");
        return -1;
    }
    if(imm < -128 || imm > 127) {
        printf("Error: Immediate %i is out of range [128..127] in [%s]\n", imm, buf);
        return -1;
    }
    processor_push_instr(p, (opcode << 12) + (rd << 8) + (imm & 255));
    return 0;
}

int parse_type_c(processor *p, int opcode, char *buf) {
    int index = 0, rd, rs1, imm;
    if((rd = parse_register(buf, &index)) < 0) return -1;
    if(parse_comma(buf, &index) < 0) return -1;
    if((rs1 = parse_register(buf, &index)) < 0) return -1;
    skip_whitespace(buf, &index);
    char sign = buf[index];
    if(buf[index] != '-' && buf[index] != '+') {
        printf("Error: Malformed offset on");
        return -1;
    }
    if(sscanf(buf + index + 1, "%i", &imm) != 1) {
        printf("Error: Expected immediate after register on");
        return -1;
    }
    if(sign == '-') imm = -imm;
    if(imm < -8 || imm > 7) {
        printf("Error: Immediate %i is out of range [-8..7] in [%s]\n", imm, buf);
        return -1;
    }
    processor_push_instr(p, (opcode << 12) + (rd << 8) + (rs1 << 4) + (imm & 15));
    return 0;
}

#pragma endregion

//PRINTING
#pragma region 

int sprintreg(char *buf, int n) {
    if(n == 2) return sprintf(buf, "ra"); 
    if(n == 3) return sprintf(buf, "sp");
    if(n >= 4 && n <= 6) return sprintf(buf, "a%d", n - 4);
    if(n >= 7 && n <= 10) return sprintf(buf, "s%d", n - 7);
    if(n >= 11 && n <= 14) return sprintf(buf, "t%d", n - 11);
    return sprintf(buf, "x%d", n);
}

void print_instruction(uint16_t instr) {
    char type = 0;
    switch ((instr >> 12) & 0xF) {
        case 0:  printf("add "); type = 'A'; break;
        case 1:  printf("adi "); type = 'C'; break;
        case 2:  printf("inc "); type = 'B'; break;
        case 3:  printf("sub "); type = 'A'; break;
        case 4:  printf("shl "); type = 'A'; break;
        case 5:  printf("and "); type = 'A'; break;
        case 6:  printf("or  "); type = 'A'; break;
        case 7:  printf("xor "); type = 'A'; break;
        case 8:  printf("lw  "); type = 'C'; break;
        case 9:  printf("sw  "); type = 'C'; break;
        case 10: printf("li  "); type = 'B'; break;
        case 11: printf("lui "); type = 'B'; break;
        case 12: printf("eq  "); type = 'A'; break;
        case 13: printf("lt  "); type = 'A'; break;
        case 14: printf("bnz "); type = 'B'; break;
        case 15: printf("jal "); type = 'C'; break;
    }
    char arg1[4], arg2[4], arg3[4];
    switch (type) {
        case 'A':
            sprintreg(arg1, bits(11, 8));
            sprintreg(arg2, bits(7, 4));
            sprintreg(arg3, bits(3, 0));
            printf("%3s, %3s, %3s", arg1, arg2, arg3);
            break;
        case 'B':
            sprintreg(arg1, bits(11, 8));
            printf("%3s, %8i", arg1, (bits(7, 0) << 24 >> 24));
            break;
        case 'C':
            sprintreg(arg1, bits(11, 8));
            sprintreg(arg2, bits(7, 4));
            int n = (bits(3, 0) << 28 >> 28);
            char buf[16];
            sprintf(buf, "%s%s%d", arg2, n < 0 ? "" : "+", n);
            printf("%3s, %8s", arg1, buf);
            break;
    }
}

#pragma endregion

int main(int argc, char **argv) {
    int flags = 0;
    if(argc == 1) {
        printf("Usage: ./interpret <file> <args> <options>\n\n");
        printf("File: a path to the assembly file\n");
        printf("Args: up to 3 integers to be stored in a0-a2\n");
        printf("Options:\n");
        printf("  -r  display raw instructions\n");
        printf("  -m  display machine code\n");
        printf("  -x  create hex file\n");
        printf("  -d  debug mode\n");
        return 0;
    }
    FILE *fp = fopen(argv[1], "r");
    if(!fp) {
        printf("No such file: %s\n", argv[1]);
        return -1;
    }
    int i;
    int args[3];
    int argn = 0;
    for(i = 2; i < argc; i++) {
        int n;
        if(sscanf(argv[i], "%i", &n) != 1) break;
        if(argn > 2) {
            printf("Only 3 arguments can be passed in\n");
            return -1;
        }
        args[argn++] = n;
    }
    for(; i < argc; i++) {
        if(argv[i][0] != '-' || argv[i][2] != 0) {
            printf("Invalid option: %s\n", argv[i]);
            return -1;
        }
        switch (argv[i][1]) {
            case 'r': flags |= 1; break; 
            case 'm': flags |= 2; break;
            case 'x': flags |= 4; break;
            case 'd': flags |= 8; break;
            default:
                printf("Invalid option: %s\n", argv[i]);
                return -1;
        }
    }
    processor *p = processor_new();
    int errors = processor_load(p, fp);

    for(int i = 0; i < argn; i++) {
        p->registers[i + 4] = args[i];
    }
    
    if(errors > 0) {
        printf("Compilation failed with %d errors.\n", errors);
    } else if((flags & 1) || (flags & 2)) {
        printf("addr   ");
        if(flags & 1) printf("| instruction       ");
        if(flags & 2) printf("| machine code");
        printf("\n");

        for(int i = 0; i < p->instructions; i++) {
            uint16_t instr = ((uint16_t*)p->memory)[i];
            printf("0x%04X", i * 2);
            if(flags & 1) {
                printf(" | ");
                print_instruction(instr);
            }
            if(flags & 2) {
                printf(" | ");
                for(int j = 15; j >= 0; j--) {
                    printf("%c", bit(j) ? '1' : '0');
                }
            }
            printf("\n");
        }
    }
    else if(flags & 4) {
        char *last = strrchr(argv[1], '.');
        if(!last) last = argv[1] + strlen(argv[1]);
        char name[100];
        snprintf(name, 100, "%.*s.hex", last - argv[1], argv[1]);
        FILE *fout = fopen(name, "w");
        for(int i = 0; i < p->instructions; i++) {
            uint16_t instr = ((uint16_t*)p->memory)[i];
            fprintf(fout, "%04X\n", instr);
        }
        fclose(fout);
        printf("%s generated\n", name);
    } else {
        interpret(p, (flags & 8) > 0);
    }
}