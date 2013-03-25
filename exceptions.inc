#include <a_samp>
#include <amx_assembly\opcode>
#include <amx_assembly\phys_memory>

#if !defined MAX_CATCH_STACK
	#define MAX_CATCH_STACK 128
#endif

#if !defined MAX_EXCEPTION_MESSAGE
	#define MAX_EXCEPTION_MESSAGE 256
#endif

#define try \
	g_FC = TRY_MAGIC; for (; !TryException(); TryExceptionEnd())

#define catch%1(%2) \
	g_FC = CATCH_MAGIC; for (new %2[E_EXCEPTION]; CatchException(%2); CatchExceptionEnd())

#define finally \
	g_FC = FINALLY_MAGIC; if (g_FC)

const TRY_MAGIC     = 0x5F3616A8;
const CATCH_MAGIC   = 0x4CD52FB1;
const FINALLY_MAGIC = 0xB408FAA2;

enum E_CATCH_STACK {
	FRM,
	HEA,
	STK,
	CIP
}

enum E_EXCEPTION {
	Code,
	Message[MAX_EXCEPTION_MESSAGE]
}

// Used to avoid constant expression warnings
new g_FC;

static
	     s_CatchStack[MAX_CATCH_STACK][E_CATCH_STACK],
	     s_Exception[E_EXCEPTION],
	     s_CST = -1,
	bool:s_ExceptionThrown = false,
	bool:s_SkipNextTry = false,
	bool:s_SkipNextCatch = false
;
	
static const sc_OpcodeOperands[Opcode:138] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
	1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1,
	1, 0, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, 0, 1, -1, 0, 0, 1, 0, 1,
	1, 0
};

static stock AbsToRel(addr) {
	new dat;
	#emit lctrl 1
	#emit stor.s.pri dat
	return addr - (GetAmxBaseAddress() + dat);
}

stock ThrowError(const message[], code = 0) {
	s_ExceptionThrown = true;
	
	if (s_CST == -1) {
		printf("Uncaught exception: %s", message);
		
		// AMX_ERR_EXIT
		#emit HALT 1
	}
	
	s_Exception[Code] = code;
	strunpack(s_Exception[Message], message, MAX_EXCEPTION_MESSAGE);
	
	static s_frm, s_hea, s_stk, s_cip;
	
	s_frm = s_CatchStack[s_CST][FRM];
	s_hea = s_CatchStack[s_CST][HEA];
	s_stk = s_CatchStack[s_CST][STK];
	s_cip = s_CatchStack[s_CST][CIP];
	
	// Restore the context
	#emit LOAD.pri s_frm
	#emit SCTRL 5
	#emit LOAD.pri s_hea
	#emit SCTRL 2
	#emit LOAD.pri s_stk
	#emit SCTRL 4
	#emit LOAD.pri s_cip
	#emit SCTRL 6
}

stock TryException() {
	if (s_SkipNextTry) {
		s_SkipNextTry = false;
		
		return 1;
	}
	
	static
		Opcode:op_jzer,
		Opcode:op_break,
		Opcode:op_const_pri,
		Opcode:op_not
	;
	
	if (!op_jzer) {
		op_jzer = RelocateOpcode(OP_JZER);
		op_break = RelocateOpcode(OP_BREAK);
		op_const_pri = RelocateOpcode(OP_CONST_PRI);
		op_not = RelocateOpcode(OP_NOT);
		
		if (!op_jzer) {
			op_jzer = OP_JZER;
			op_break = OP_BREAK;
			op_const_pri = OP_CONST_PRI;
			op_not = OP_NOT;
		}
	}
	
	s_CST += 1;
	
	assert s_CST < MAX_CATCH_STACK;
	
	new frm, cip, hea, stk;
	
	// Get the previous frame pointer
	#emit LOAD.S.pri 0
	#emit STOR.S.pri frm
	
	// Get the heap pointer
	#emit LCTRL 2
	#emit STOR.S.pri hea
	
	// Get the stack index
	#emit LCTRL 4
	#emit STOR.S.pri stk
	
	// Get the return address
	#emit LOAD.S.pri 4
	#emit STOR.S.pri cip
	
	s_CatchStack[s_CST][FRM] = frm;
	s_CatchStack[s_CST][HEA] = hea;
	s_CatchStack[s_CST][STK] = stk + 12 + 16;
	s_CatchStack[s_CST][CIP] = cip;
	
	// Look for a finally statement
	new scan_addr, cod, dat, Opcode:opcode, base;
	
	base = GetAmxBaseAddress();
	
	// Get return address relative to DAT
	#emit LOAD.S.alt  4
	#emit LCTRL       0
	#emit STOR.S.pri  cod
	#emit ADD
	#emit MOVE.alt
	#emit LCTRL       1
	#emit STOR.S.pri  dat
	#emit SUB.alt
	#emit STOR.S.pri  scan_addr
	
	#emit LREF.S.pri scan_addr
	#emit STOR.S.pri opcode
	
	if (opcode != op_not) {
		printf("opcode != op_not");
		
		return 0;
	}
	
	// Look for the catch statement
	new data;
	
	scan_addr += 8;
	#emit LREF.S.pri scan_addr
	#emit STOR.S.pri scan_addr
	
	scan_addr = scan_addr - (dat + base);
	
	#emit LREF.S.pri scan_addr
	#emit STOR.S.pri opcode
	
	if (opcode == op_break) {
		scan_addr += 8;
	} else if (opcode == op_const_pri) {
		scan_addr += 4;
	} else {
		printf("opcode != op_break || op_const_pri");
		
		return 0;
	}
	
	#emit LREF.S.pri scan_addr
	#emit STOR.S.pri data
	
	scan_addr += 12;
	
	
	
	if (data == CATCH_MAGIC) {
		printf("catch statement at %08x", scan_addr + dat + base);
		
		#emit LREF.S.pri scan_addr
		#emit STOR.S.pri data
		
		if (data == _:OP_BREAK) {
			scan_addr += 8;
		}
		
		scan_addr += 80;
		#emit LREF.S.pri scan_addr
		#emit STOR.S.pri data
		
		if (data == _:op_jzer) {
			scan_addr += 4;
			#emit LREF.S.pri scan_addr
			#emit STOR.S.pri scan_addr
		} else {
			scan_addr = data;
		}
		
		scan_addr = scan_addr - (dat + base);
		
		scan_addr += 8;
		#emit LREF.S.pri scan_addr
		#emit STOR.S.pri opcode
		
		if (opcode == op_break) {
			scan_addr += 4;
		}
		
		#emit LREF.S.pri scan_addr
		#emit STOR.S.pri opcode
		
		if (opcode == op_const_pri) {
			scan_addr += 4;
			
			#emit LREF.S.pri scan_addr
			#emit STOR.S.pri data
			
			scan_addr += 12;
		} else {
			data = 0;
		}
	} else {
		printf("no catch statement");
	}
	
	if (data == FINALLY_MAGIC) {
		#emit LREF.S.pri scan_addr
		#emit STOR.S.pri data
		
		if (data == _:op_break) {
			scan_addr += 20;
		} else {
			scan_addr += 16;
		}
		
		#emit LREF.S.pri scan_addr
		#emit STOR.S.pri data
		
		printf("finally statement at %08x", scan_addr + dat + base);
	} else {
		printf("no finally statement");
	}
	
	// TODO: do stuff with "finally"
	
	return 0;
}

stock TryExceptionEnd() {
	s_SkipNextTry = true;
}

stock CatchException(e[E_EXCEPTION]) {
	if (s_SkipNextCatch) {
		s_SkipNextCatch = false;
		
		return 0;
	}
	
	e[Code] = s_Exception[Code];
	strunpack(e[Message], s_Exception[Message], MAX_EXCEPTION_MESSAGE);
	
	s_CST -= 1;
	
	assert s_CST >= -1;
	
	if (s_ExceptionThrown) {
		s_ExceptionThrown = false;
		
		return 1;
	}
	
	return 0;
}

stock CatchExceptionEnd() {
	s_SkipNextCatch = true;
}

forward OnRuntimeError(code);
public OnRuntimeError(code) {
	static const sc_messages[28][54] = {
		/* AMX_ERR_NONE      */ "None",
		/* AMX_ERR_EXIT      */ "Forced exit",
		/* AMX_ERR_ASSERT    */ "Assertion failed",
		/* AMX_ERR_STACKERR  */ "Stack/heap collision (insufficient stack size)",
		/* AMX_ERR_BOUNDS    */ "Array index out of bounds",
		/* AMX_ERR_MEMACCESS */ "Invalid memory access",
		/* AMX_ERR_INVINSTR  */ "Invalid instruction",
		/* AMX_ERR_STACKLOW  */ "Stack underflow",
		/* AMX_ERR_HEAPLOW   */ "Heap underflow",
		/* AMX_ERR_CALLBACK  */ "No (valid) native function callback",
		/* AMX_ERR_NATIVE    */ "Native function failed",
		/* AMX_ERR_DIVIDE    */ "Divide by zero",
		/* AMX_ERR_SLEEP     */ "sleep mode",
		/* 13 */                "reserved",
		/* 14 */                "reserved",
		/* 15 */                "reserved",
		/* AMX_ERR_MEMORY    */ "Out of memory",
		/* AMX_ERR_FORMAT    */ "Invalid/unsupported P-code file format",
		/* AMX_ERR_VERSION   */ "File is for a newer version of the AMX",
		/* AMX_ERR_NOTFOUND  */ "File or function is not found",
		/* AMX_ERR_INDEX     */ "Invalid index parameter (bad entry point)",
		/* AMX_ERR_DEBUG     */ "Debugger cannot run",
		/* AMX_ERR_INIT      */ "AMX not initialized (or doubly initialized)",
		/* AMX_ERR_USERDATA  */ "Unable to set user data field (table full)",
		/* AMX_ERR_INIT_JIT  */ "Cannot initialize the JIT",
		/* AMX_ERR_PARAMS    */ "Parameter error",
		/* AMX_ERR_DOMAIN    */ "Domain error, expression result does not fit in range",
		/* AMX_ERR_GENERAL   */ "General error (unknown or unspecific error)"
	};
	
	static msg[128];
	
	if (0 <= code < sizeof(sc_messages)) {
		format(msg, sizeof(msg), "runtime error %d: %s", code, sc_messages[code]);
	} else {
		format(msg, sizeof(msg), "runtime error %d", code);
	}
	
	if (funcidx(!"Exceptions_OnRuntimeError") != -1) {
		CallLocalFunction(!"Exceptions_OnRuntimeError", !"i", code);
	}
	
	ThrowError(msg);
}

#if defined _ALS_OnRuntimeError
	#undef OnRuntimeError
#else
	#define _ALS_OnRuntimeError
#endif

#define OnRuntimeError Exceptions_OnRuntimeError
