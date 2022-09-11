#pragma once

#define HGBF_OPCODE_LIST \
	HGBF_OPCODE_LIST_ENTRY(NXT , 0x00, 0) /* next data cell */ \
	HGBF_OPCODE_LIST_ENTRY(PRV , 0x01, 0) /* previous data cell */ \
	HGBF_OPCODE_LIST_ENTRY(INC , 0x02, 0) /* increase data */ \
	HGBF_OPCODE_LIST_ENTRY(DEC , 0x03, 0) /* decrease data */ \
	HGBF_OPCODE_LIST_ENTRY(OUT , 0x04, 0) /* output data as ASCII */ \
	HGBF_OPCODE_LIST_ENTRY(IN  , 0x05, 0) /* input data as ASCII */ \
	HGBF_OPCODE_LIST_ENTRY(JFZ , 0x06, 4) /* jump forward if data is zero */ \
	HGBF_OPCODE_LIST_ENTRY(JBN , 0x07, 4) /* jump backward if data is nonzero */ \
	HGBF_OPCODE_LIST_ENTRY(HLT , 0x08, 0) /* halt */ \
	HGBF_OPCODE_LIST_ENTRY(NXTn, 0x09, 2) /* NXT * n */ \
	HGBF_OPCODE_LIST_ENTRY(PRVn, 0x0a, 2) /* PRV * n */ \
	HGBF_OPCODE_LIST_ENTRY(INCn, 0x0b, 1) /* INC * n */ \
	HGBF_OPCODE_LIST_ENTRY(DECn, 0x0c, 1) /* DEC * n */ \
// HGBF_OPCODE_LIST

typedef enum {
#define HGBF_OPCODE_LIST_ENTRY(NAME, CODE, OPRD) HGBF_OP_ ##NAME = CODE ,
	HGBF_OPCODE_LIST
#undef HGBF_OPCODE_LIST_ENTRY
} hgbf_opcode_t;
