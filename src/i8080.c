#include "i8080.h"
#include <string.h>

// We keep EI-pending state in a private static (single CPU instance, minimalist).

static inline uint16_t HL(const I8080* c) { return (uint16_t)((uint16_t)c->h<<8) | c->l; }
static inline void setHL(I8080* c, uint16_t v) { c->h=(uint8_t)(v>>8); c->l=(uint8_t)v; }

static inline bool parity(uint8_t v)
{
	v ^= (uint8_t)(v>>4);
	v ^= (uint8_t)(v>>2);
	v ^= (uint8_t)(v>>1);
	return ((~v) & 1u) != 0;
}

static inline void set_zsp(I8080* c, uint8_t v)
{
	c->z = (v == 0);
	c->s = (v & 0x80) != 0;
	c->p = parity(v);
}

static inline uint8_t rd(I8080Bus* b, uint16_t a) { return b->mem_read(b, a); }
static inline void    wr(I8080Bus* b, uint16_t a, uint8_t v) { b->mem_write(b, a, v); }
static inline uint8_t in(I8080Bus* b, uint8_t p) { return b->io_in(b, p); }
static inline void    out(I8080Bus* b, uint8_t p, uint8_t v) { b->io_out(b, p, v); }

static inline void push16(I8080* c, I8080Bus* b, uint16_t v)
{
	wr(b, (uint16_t)(c->sp - 1), (uint8_t)(v >> 8));
	wr(b, (uint16_t)(c->sp - 2), (uint8_t)(v));
	c->sp = (uint16_t)(c->sp - 2);
}
static inline uint16_t pop16(I8080* c, I8080Bus* b)
{
	uint8_t lo = rd(b, c->sp);
	uint8_t hi = rd(b, (uint16_t)(c->sp + 1));
	c->sp = (uint16_t)(c->sp + 2);
	return (uint16_t)((uint16_t)hi<<8) | lo;
}

static inline uint8_t pack_flags(const I8080* c)
{
	return (uint8_t)(
	(c->s ? 0x80 : 0) |
	(c->z ? 0x40 : 0) |
	(c->ac? 0x10 : 0) |
	(c->p ? 0x04 : 0) |
	0x02 |
	(c->cy? 0x01 : 0)
	);
}
static inline void unpack_flags(I8080* c, uint8_t f)
{
	c->s  = (f & 0x80) != 0;
	c->z  = (f & 0x40) != 0;
	c->ac = (f & 0x10) != 0;
	c->p  = (f & 0x04) != 0;
	c->cy = (f & 0x01) != 0;
}

static inline uint16_t rp_bc(const I8080* c) { return (uint16_t)((uint16_t)c->b<<8)|c->c; }
static inline uint16_t rp_de(const I8080* c) { return (uint16_t)((uint16_t)c->d<<8)|c->e; }
static inline uint16_t rp_hl(const I8080* c) { return HL(c); }
static inline void set_rp_bc(I8080* c, uint16_t v) { c->b=(uint8_t)(v>>8); c->c=(uint8_t)v; }
static inline void set_rp_de(I8080* c, uint16_t v) { c->d=(uint8_t)(v>>8); c->e=(uint8_t)v; }
static inline void set_rp_hl(I8080* c, uint16_t v) { setHL(c,v); }

static inline uint8_t *reg_ptr(I8080* c, int r)
{
	switch (r) {
	case 0: return &c->b;
	case 1: return &c->c;
	case 2: return &c->d;
	case 3: return &c->e;
	case 4: return &c->h;
	case 5: return &c->l;
	case 7: return &c->a;
	default: return NULL;
	}
}
static inline uint8_t get_r(I8080* c, I8080Bus* b, int r)
{
	if (r == 6) return rd(b, HL(c));
	return *reg_ptr(c,r);
}
static inline void set_r(I8080* c, I8080Bus* b, int r, uint8_t v)
{
	if (r == 6) wr(b, HL(c), v);
	else *reg_ptr(c,r) = v;
}

static inline void add8(I8080* c, uint8_t x, bool with_carry)
{
	uint16_t a = c->a;
	uint16_t y = (uint16_t)x + ((with_carry && c->cy) ? 1u : 0u);
	uint16_t r = a + y;
	c->ac = ((a & 0x0F) + (y & 0x0F)) > 0x0F;
	c->cy = r > 0xFF;
	c->a  = (uint8_t)r;
	set_zsp(c, c->a);
}
static inline void sub8(I8080* c, uint8_t x, bool with_borrow)
{
	uint16_t a = c->a;
	uint16_t y = (uint16_t)x + ((with_borrow && c->cy) ? 1u : 0u);
	uint16_t r = a - y;
	c->ac = ((a & 0x0F) < (y & 0x0F));
	c->cy = a < y;
	c->a  = (uint8_t)r;
	set_zsp(c, c->a);
}
static inline void cmp8(I8080* c, uint8_t x)
{
	uint16_t a = c->a;
	uint16_t r = a - x;
	c->ac = ((a & 0x0F) < (x & 0x0F));
	c->cy = a < x;
	set_zsp(c, (uint8_t)r);
}

static inline void ana8(I8080* c, uint8_t x)
{
	c->a &= x;
	c->cy = false;
	c->ac = true;
	set_zsp(c, c->a);
}
static inline void xra8(I8080* c, uint8_t x)
{
	c->a ^= x;
	c->cy = false;
	c->ac = false;
	set_zsp(c, c->a);
}
static inline void ora8(I8080* c, uint8_t x)
{
	c->a |= x;
	c->cy = false;
	c->ac = false;
	set_zsp(c, c->a);
}

static inline uint8_t inr8(I8080* c, uint8_t v)
{
	uint8_t r = (uint8_t)(v + 1u);
	c->ac = ((v & 0x0F) + 1u) > 0x0F;
	set_zsp(c, r);
	return r;
}
static inline uint8_t dcr8(I8080* c, uint8_t v)
{
	uint8_t r = (uint8_t)(v - 1u);
	c->ac = ((v & 0x0F) == 0x00);
	set_zsp(c, r);
	return r;
}

static inline void daa(I8080* c)
{
	uint8_t a = c->a;
	uint8_t adj = 0;
	bool cy = c->cy;

	if (c->ac || ((a & 0x0F) > 9)) adj |= 0x06;
	if (cy || (a > 0x99)) { adj |= 0x60; cy = true; }

	uint16_t r = (uint16_t)a + adj;
	c->a = (uint8_t)r;
	c->cy = cy;
	c->ac = ((a & 0x0F) + (adj & 0x0F)) > 0x0F;
	set_zsp(c, c->a);
}

static inline bool cond(const I8080* c, int cc)
{
	switch (cc) {
	case 0: return !c->z;  // NZ
	case 1: return  c->z;  // Z
	case 2: return !c->cy; // NC
	case 3: return  c->cy; // C
	case 4: return !c->p;  // PO
	case 5: return  c->p;  // PE
	case 6: return !c->s;  // P
	case 7: return  c->s;  // M
	default: return false;
	}
}

void i8080_reset(I8080* cpu)
{
	memset(cpu, 0, sizeof(*cpu));
	cpu->pc = 0x0000;
	cpu->sp = 0x0000;
	cpu->inte = false;
	cpu->halted = false;
	cpu->ei_pending = false;
}

void i8080_set_ei_pending(I8080* cpu)
{
	if (!cpu) return;
	cpu->ei_pending = true;
}

void i8080_intr_service(I8080* cpu, I8080Bus* bus, uint8_t rst_vector)
{
	// 8080 interrupt acknowledge behaves like executing RST n, with 11 t-states.
	// Emulator is responsible for accounting cycles externally.
	cpu->halted = false;
	cpu->inte = false;
	push16(cpu, bus, cpu->pc);
	cpu->pc = (uint16_t)((rst_vector & 7u) * 8u);
}

int i8080_step(I8080* c, I8080Bus* b)
{
	// EI takes effect after one following instruction
	bool apply_ei_after = c->ei_pending;

	if (c->halted) {
		if (apply_ei_after) { c->inte = true; c->ei_pending = false; }
		return 4;
	}

	uint8_t op = rd(b, c->pc++);

	// MOV group
	if ((op & 0xC0) == 0x40) {
		if (op == 0x76) { c->halted = true; if (apply_ei_after) { c->inte=true; c->ei_pending=false; } return 7; }
		int d = (op >> 3) & 7;
		int s = op & 7;
		uint8_t v = get_r(c,b,s);
		set_r(c,b,d,v);
		int t = (d==6 || s==6) ? 7 : 5;
		if (apply_ei_after) { c->inte=true; c->ei_pending=false; }
		return t;
	}

	// ALU group
	if ((op & 0xC0) == 0x80) {
		int s = op & 7;
		uint8_t v = get_r(c,b,s);
		switch ((op >> 3) & 7) {
		case 0: add8(c,v,false); break;
		case 1: add8(c,v,true ); break;
		case 2: sub8(c,v,false); break;
		case 3: sub8(c,v,true ); break;
		case 4: ana8(c,v); break;
		case 5: xra8(c,v); break;
		case 6: ora8(c,v); break;
		case 7: cmp8(c,v); break;
		}
		int t = (s==6) ? 7 : 4;
		if (apply_ei_after) { c->inte=true; c->ei_pending=false; }
		return t;
	}

	int t = 4;

	switch (op) {
	case 0x00: t=4; break; // NOP

		// LXI
	case 0x01: c->c=rd(b,c->pc++); c->b=rd(b,c->pc++); t=10; break;
	case 0x11: c->e=rd(b,c->pc++); c->d=rd(b,c->pc++); t=10; break;
	case 0x21: c->l=rd(b,c->pc++); c->h=rd(b,c->pc++); t=10; break;
	case 0x31: { uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); c->sp=(uint16_t)((uint16_t)hi<<8)|lo; t=10; } break;

		// STAX/LDAX
	case 0x02: wr(b, rp_bc(c), c->a); t=7; break;
	case 0x12: wr(b, rp_de(c), c->a); t=7; break;
	case 0x0A: c->a = rd(b, rp_bc(c)); t=7; break;
	case 0x1A: c->a = rd(b, rp_de(c)); t=7; break;

		// INX/DCX
	case 0x03: set_rp_bc(c, (uint16_t)(rp_bc(c)+1)); t=5; break;
	case 0x13: set_rp_de(c, (uint16_t)(rp_de(c)+1)); t=5; break;
	case 0x23: set_rp_hl(c, (uint16_t)(rp_hl(c)+1)); t=5; break;
	case 0x33: c->sp = (uint16_t)(c->sp+1); t=5; break;

	case 0x0B: set_rp_bc(c, (uint16_t)(rp_bc(c)-1)); t=5; break;
	case 0x1B: set_rp_de(c, (uint16_t)(rp_de(c)-1)); t=5; break;
	case 0x2B: set_rp_hl(c, (uint16_t)(rp_hl(c)-1)); t=5; break;
	case 0x3B: c->sp = (uint16_t)(c->sp-1); t=5; break;

		// INR
	case 0x04: c->b = inr8(c,c->b); t=5; break;
	case 0x0C: c->c = inr8(c,c->c); t=5; break;
	case 0x14: c->d = inr8(c,c->d); t=5; break;
	case 0x1C: c->e = inr8(c,c->e); t=5; break;
	case 0x24: c->h = inr8(c,c->h); t=5; break;
	case 0x2C: c->l = inr8(c,c->l); t=5; break;
	case 0x34: { uint8_t v=rd(b,HL(c)); v=inr8(c,v); wr(b,HL(c),v); t=10; } break;
	case 0x3C: c->a = inr8(c,c->a); t=5; break;

		// DCR
	case 0x05: c->b = dcr8(c,c->b); t=5; break;
	case 0x0D: c->c = dcr8(c,c->c); t=5; break;
	case 0x15: c->d = dcr8(c,c->d); t=5; break;
	case 0x1D: c->e = dcr8(c,c->e); t=5; break;
	case 0x25: c->h = dcr8(c,c->h); t=5; break;
	case 0x2D: c->l = dcr8(c,c->l); t=5; break;
	case 0x35: { uint8_t v=rd(b,HL(c)); v=dcr8(c,v); wr(b,HL(c),v); t=10; } break;
	case 0x3D: c->a = dcr8(c,c->a); t=5; break;

		// MVI
	case 0x06: c->b=rd(b,c->pc++); t=7; break;
	case 0x0E: c->c=rd(b,c->pc++); t=7; break;
	case 0x16: c->d=rd(b,c->pc++); t=7; break;
	case 0x1E: c->e=rd(b,c->pc++); t=7; break;
	case 0x26: c->h=rd(b,c->pc++); t=7; break;
	case 0x2E: c->l=rd(b,c->pc++); t=7; break;
	case 0x36: wr(b,HL(c),rd(b,c->pc++)); t=10; break;
	case 0x3E: c->a=rd(b,c->pc++); t=7; break;

		// Rotates
	case 0x07: { uint8_t x=c->a; c->cy=(x&0x80)!=0; c->a=(uint8_t)((x<<1) | (c->cy?1:0)); t=4; } break; // RLC
	case 0x0F: { uint8_t x=c->a; c->cy=(x&0x01)!=0; c->a=(uint8_t)((x>>1) | (c->cy?0x80:0)); t=4; } break; // RRC
	case 0x17: { uint8_t x=c->a; bool old=c->cy; c->cy=(x&0x80)!=0; c->a=(uint8_t)((x<<1) | (old?1:0)); t=4; } break; // RAL
	case 0x1F: { uint8_t x=c->a; bool old=c->cy; c->cy=(x&0x01)!=0; c->a=(uint8_t)((x>>1) | (old?0x80:0)); t=4; } break; // RAR

		// DAD
	case 0x09: { uint32_t r=(uint32_t)HL(c) + rp_bc(c); c->cy = (r>0xFFFF); setHL(c,(uint16_t)r); t=10; } break;
	case 0x19: { uint32_t r=(uint32_t)HL(c) + rp_de(c); c->cy = (r>0xFFFF); setHL(c,(uint16_t)r); t=10; } break;
	case 0x29: { uint32_t r=(uint32_t)HL(c) + HL(c);   c->cy = (r>0xFFFF); setHL(c,(uint16_t)r); t=10; } break;
	case 0x39: { uint32_t r=(uint32_t)HL(c) + c->sp;   c->cy = (r>0xFFFF); setHL(c,(uint16_t)r); t=10; } break;

		// DAA/CMA/STC/CMC
	case 0x27: daa(c); t=4; break;
	case 0x2F: c->a = (uint8_t)~c->a; t=4; break;
	case 0x37: c->cy = true; t=4; break;
	case 0x3F: c->cy = !c->cy; t=4; break;

		// Direct memory
	case 0x22: { uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); uint16_t a=(uint16_t)((uint16_t)hi<<8)|lo; wr(b,a,c->l); wr(b,(uint16_t)(a+1),c->h); t=16; } break; // SHLD
	case 0x2A: { uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); uint16_t a=(uint16_t)((uint16_t)hi<<8)|lo; c->l=rd(b,a); c->h=rd(b,(uint16_t)(a+1)); t=16; } break; // LHLD
	case 0x32: { uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); uint16_t a=(uint16_t)((uint16_t)hi<<8)|lo; wr(b,a,c->a); t=13; } break; // STA
	case 0x3A: { uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); uint16_t a=(uint16_t)((uint16_t)hi<<8)|lo; c->a=rd(b,a); t=13; } break; // LDA

		// XCHG/XTHL/SPHL/PCHL
	case 0xEB: { uint8_t th=c->h, tl=c->l; c->h=c->d; c->l=c->e; c->d=th; c->e=tl; t=5; } break;
	case 0xE3: { uint8_t lo=rd(b,c->sp); uint8_t hi=rd(b,(uint16_t)(c->sp+1)); wr(b,c->sp,c->l); wr(b,(uint16_t)(c->sp+1),c->h); c->l=lo; c->h=hi; t=18; } break;
	case 0xF9: c->sp = HL(c); t=5; break;
	case 0xE9: c->pc = HL(c); t=5; break;

		// JMP and Jcond
	case 0xC3: { uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); c->pc=(uint16_t)((uint16_t)hi<<8)|lo; t=10; } break;
	case 0xC2: case 0xCA: case 0xD2: case 0xDA: case 0xE2: case 0xEA: case 0xF2: case 0xFA: {
			int cc = (op>>3)&7; uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); uint16_t a=(uint16_t)((uint16_t)hi<<8)|lo; if (cond(c,cc)) c->pc=a; t=10;
		} break;

		// CALL and Ccond
	case 0xCD: { uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); uint16_t a=(uint16_t)((uint16_t)hi<<8)|lo; push16(c,b,c->pc); c->pc=a; t=17; } break;
	case 0xC4: case 0xCC: case 0xD4: case 0xDC: case 0xE4: case 0xEC: case 0xF4: case 0xFC: {
			int cc=(op>>3)&7; uint8_t lo=rd(b,c->pc++), hi=rd(b,c->pc++); uint16_t a=(uint16_t)((uint16_t)hi<<8)|lo; if (cond(c,cc)) { push16(c,b,c->pc); c->pc=a; t=17; } else { t=11; }
		} break;

		// RET and Rcond
	case 0xC9: c->pc = pop16(c,b); t=10; break;
	case 0xC0: case 0xC8: case 0xD0: case 0xD8: case 0xE0: case 0xE8: case 0xF0: case 0xF8: {
			int cc=(op>>3)&7; if (cond(c,cc)) { c->pc=pop16(c,b); t=11; } else { t=5; }
		} break;

		// RST
	case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF: {
			uint8_t n = (uint8_t)((op>>3)&7); push16(c,b,c->pc); c->pc=(uint16_t)(n*8u); t=11;
		} break;

		// PUSH/POP
	case 0xC5: push16(c,b,rp_bc(c)); t=11; break;
	case 0xD5: push16(c,b,rp_de(c)); t=11; break;
	case 0xE5: push16(c,b,rp_hl(c)); t=11; break;
	case 0xF5: { uint16_t psw = (uint16_t)((uint16_t)c->a<<8) | pack_flags(c); push16(c,b,psw); t=11; } break;

	case 0xC1: { uint16_t v=pop16(c,b); set_rp_bc(c,v); t=10; } break;
	case 0xD1: { uint16_t v=pop16(c,b); set_rp_de(c,v); t=10; } break;
	case 0xE1: { uint16_t v=pop16(c,b); set_rp_hl(c,v); t=10; } break;
	case 0xF1: { uint16_t v=pop16(c,b); uint8_t f=(uint8_t)v; c->a=(uint8_t)(v>>8); unpack_flags(c,f); t=10; } break;

		// Immediate ALU
	case 0xC6: add8(c,rd(b,c->pc++),false); t=7; break;
	case 0xCE: add8(c,rd(b,c->pc++),true ); t=7; break;
	case 0xD6: sub8(c,rd(b,c->pc++),false); t=7; break;
	case 0xDE: sub8(c,rd(b,c->pc++),true ); t=7; break;
	case 0xE6: ana8(c,rd(b,c->pc++)); t=7; break;
	case 0xEE: xra8(c,rd(b,c->pc++)); t=7; break;
	case 0xF6: ora8(c,rd(b,c->pc++)); t=7; break;
	case 0xFE: cmp8(c,rd(b,c->pc++)); t=7; break;

		// IN/OUT
	case 0xDB: { uint8_t p=rd(b,c->pc++); c->a = in(b,p); t=10; } break;
	case 0xD3: { uint8_t p=rd(b,c->pc++); out(b,p,c->a); t=10; } break;

		// EI/DI
	case 0xF3: c->inte=false; c->ei_pending=false; t=4; break;
	case 0xFB: i8080_set_ei_pending(c); t=4; break;

		// HLT
	case 0x76: c->halted=true; t=7; break;

		// NOP "holes"
	case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: case 0x38:
		t=4; break;

	default:
		// Any remaining undocumented/unused opcodes treat as NOP.
		t=4; break;
	}

	if (apply_ei_after) { c->inte = true; c->ei_pending = false; }
	return t;
}
