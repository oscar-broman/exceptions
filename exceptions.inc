#include <a_samp>
#tryinclude <amx_assembly\opcode>
#tryinclude <amx_assembly\phys_memory>
#tryinclude <amx_assembly\asm>

// Max number of try statements
#if !defined MAX_TRY_STATEMENTS
	const MAX_TRY_STATEMENTS = 512;
#endif

// Max nested try statements during runtime
#if !defined MAX_CONTEXT_STACK
	const MAX_CONTEXT_STACK = 128;
#endif

// Max size of an exception message
#if !defined MAX_EXCEPTION_MESSAGE
	const MAX_EXCEPTION_MESSAGE = 256;
#endif

// Still use e_asserts if compiled with -d0
#if !debug && defined EXCEPTIONS_ASSERT_ANYWAY
	#define e_assert%1; \
		if (!(%1)) { print(!"An assertion failed somewhere. That's all I know. " \
			                "Compile with -d2 and use crashdetect for more information."); exit; }
#else
	#define e_assert assert
#endif

// Make sure we don't have an outdated version of amx_assembly
#if !defined _inc_opcode
	#error The amx_assembly library is required, get it here: http:/\
	                                                                /goo.gl/qEfLm
#endif

#if tagof(NUM_OPCODES) == tagof(OP_NONE)
	#error Please get the latest version of the amx_assembly files: http:/\
	                                                                      /goo.gl/qEfLm
#endif


// Entry points for the new statements
#define try \
	if (!g_@eInitialized) InitExceptions(); for (new __r = __r | _@eTRY_MAGIC1 | _@eTRY_MAGIC2; __r && TryStart(!_@eTRUE, !_@eFALSE, _@eTRY_MAGIC3); TryEnd(), __r = 0)

#define catch%1(%2) \
	if (_@eCATCH_MAGIC | _@eTRUE) for (new %2[E_EXCEPTION]; !%2[Caught] && CatchStart(%2); %2[Caught] = true, CatchEnd())

#define finally \
	if (_@eFINALLY_MAGIC | _@eTRUE) for (new __f = FinallyStart(); __f; FinallyEnd(), __f = 0)

stock _@eTHROW1, _@eTHROW2;

#define throw \
	_@eTHROW1||_@eTHROW2||

#define _@eTHROW1||_@eTHROW2||%1new%2Error%3( \
	ThrowError(

#define _@eTHROW2||%1; \
	ThrowException(%1);

// Give each variable a unique value
#define UniqueErrorCode<%1> \
	%1;                     \
	@_eC%1(v, n[32]);       \
	@_eC%1(v, n[32]) {      \
		%1 = %1 | v;        \
		strpack(n, !#%1);   \
	} _@eCEND0:_@eCEND1:

// Allow for multiple declarations
#define _@eCEND0:_@eCEND1:%1, \
		stock

// End of declarations? Put this to swallow the semicolon
#define _@eCEND1: \
		_@eNULL()

// Used to distinguish the statements among AMX instructions
const _@eTRY_MAGIC1    = 0x026A57FD;
const _@eTRY_MAGIC2    = 0xABD8C2BA;
const _@eTRY_MAGIC3    = 0xD3120889;
const _@eCATCH_MAGIC   = 0x4CD52FB1;
const _@eFINALLY_MAGIC = 0xB408FAA2;


// Prevent redundant test warnings
stock _@eTRUE  = true;
stock _@eFALSE = false;


// Will be provided in the catch statement
enum E_EXCEPTION {
	Message[MAX_EXCEPTION_MESSAGE],
	Code,
	Caught
}

// Context of a try statement
enum E_TRY_CONTEXT {
	// FRM, HEA, STK are set to what they were before the try block
	FRM,
	HEA,
	STK,
	// The index in s_TryStatements
	TryIdx,
	// Was there a return statement in try or catch?
	ShouldReturn,
	ReturnValue,
	// The current block we're in
	CurrentBlock
}

enum {
	CurrentBlockTry,
	CurrentBlockCatch,
	CurrentBlockFinally
}

// Information about each try statement
enum E_TRY_STATEMENT {
	// Memory addresses in COD
	Try_End,
	HasCatch,
	Catch_End,
	HasFinally,
	Finally_End
}


// Number of operands an opcode takes
static const sc_OpcodeOperands[Opcode:138] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
	1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1,
	1, 0, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, 0, 1, -1, 0, 0, 1, 0, 1,
	1, 0
};


static
	s_TryStatements[MAX_TRY_STATEMENTS][E_TRY_STATEMENT],
	s_NumTryStatements = 0,
	
	s_ContextStack[MAX_CONTEXT_STACK][E_TRY_CONTEXT],
	// Current context index
	s_CTX = -1,
	
	// Tailored functions for hooked return statements in try and catch
	s_ReturnHooks[MAX_TRY_STATEMENTS][AsmContext],
	s_ReturnHookCode[MAX_TRY_STATEMENTS][20],
	
	// The exception that was thrown
	s_LastException[E_EXCEPTION],
	s_HasException = false
;

new
	g_@eInitialized = false
;

// Scan the AMX for try statements and do some modifications to it
stock InitExceptions();
// Invoked before try
stock TryStart(frm, stk, try_idx);
// Invoked after try
stock TryEnd();
// Invoked before catch
stock CatchStart(e[E_EXCEPTION]);
// Invoked after catch
stock CatchEnd();
// Invoked before finally
stock FinallyStart();
// Invoked after finally
stock FinallyEnd();
// Get the name of a UniqueErrorCode
stock GetErrorCodeName(code, bool:pack = false);
// Getter for s_CTX
stock GetTryStackIndex();
// Create and throw an exception
stock ThrowError(const message[], code = -1);
// Throw an existing exception
stock ThrowException(e[E_EXCEPTION]);
// Called when an exception happens outside of try
forward OnUncaughtException(e[E_EXCEPTION]);
// Used when hooking return statements
static stock ReturnHook(retval);
// Restore the frame, stack, and heap the way it was before the try statement,
// then jump to the specified position
static stock JumpToContext(_cip, pop = false);
// Return from the function containing the try statement
static stock DoReturn(_retval);
// Increment s_CTX
static stock PushContext();
// Decrement s_CTX
static stock PopContext();
// Read and relocate an opcode
static stock Opcode:ReadOpcode(address);
// Read the operand for the current opcode, assuming it has one
static stock ReadOperand(address, end = 0);
// Write an opcode and its operand (if it has one)
static stock WriteOpcode(address, Opcode:opcode, operand = 0);
// Write an operand
static stock WriteOperand(address, operand);
// Find and store the next opcode
static stock FindNextOpcode(&address, &Opcode:opcode, end = 0);
// Find a specific opcode
// Modifies address only on success
static stock FindOpcode(&address, Opcode:opcode, end = 0);
// Find a specific opcode and operand pair
// Modifies address only on success
static stock FindOpcodeWithOperand(&address, Opcode:opcode, operand, end = 0);

stock TryStart(frm, stk, try_idx) {
	e_assert g_@eInitialized;
	e_assert 0 <= try_idx < s_NumTryStatements;
	
	// Skip the for-loop's local variable (__r) and the first pushed argument
	stk += 4 + 4;
	
	// Get the heap pointer
	new hea;
	#emit LCTRL 2
	#emit STOR.S.pri hea
	
	PushContext();
	
	s_ContextStack[s_CTX][FRM] = frm;
	s_ContextStack[s_CTX][HEA] = hea;
	s_ContextStack[s_CTX][STK] = stk;
	s_ContextStack[s_CTX][TryIdx] = try_idx;
	s_ContextStack[s_CTX][ShouldReturn] = false;
	s_ContextStack[s_CTX][CurrentBlock] = CurrentBlockTry;
	
	return 1;
}

stock TryEnd() {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	new try_idx = s_ContextStack[s_CTX][TryIdx];
	
	e_assert 0 <= try_idx < s_NumTryStatements;
	
	// If there's no catch or finally after this, pop the context
	if (!s_TryStatements[try_idx][HasCatch] && !s_TryStatements[try_idx][HasFinally]) {
		s_HasException = false;
		
		PopContext();
	}
	
	return 1;
}

stock CatchStart(e[E_EXCEPTION]) {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	// Nothing more to do here
	if (!s_HasException) {
		CatchEnd();
		
		return 0;
	}
	
	s_HasException = false;
	s_ContextStack[s_CTX][CurrentBlock] = CurrentBlockCatch;
	
	// Copy the last exception's values to e
	strunpack(e[Message], s_LastException[Message], MAX_EXCEPTION_MESSAGE);
	e[Code] = s_LastException[Code];
	e[Caught] = s_LastException[Caught];
	
	return 1;
}

stock CatchEnd() {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	new try_idx = s_ContextStack[s_CTX][TryIdx];
	
	e_assert 0 <= try_idx < s_NumTryStatements;
	
	// If there's no finally after this, pop the context
	if (!s_TryStatements[try_idx][HasFinally]) {
		if (s_ContextStack[s_CTX][ShouldReturn]) {
			DoReturn(s_ContextStack[s_CTX][ReturnValue]);
		} else {
			PopContext();
		}
	}
	
	return 1;
}

stock FinallyStart() {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	if (s_HasException) {
		s_HasException = false;
	}
	
	s_ContextStack[s_CTX][CurrentBlock] = CurrentBlockFinally;
	
	return 1;
}

stock FinallyEnd() {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	new try_idx = s_ContextStack[s_CTX][TryIdx];
	
	e_assert 0 <= try_idx < s_NumTryStatements;
	
	if (s_ContextStack[s_CTX][ShouldReturn]) {
		DoReturn(s_ContextStack[s_CTX][ReturnValue]);
	} else {
		JumpToContext(s_TryStatements[try_idx][Finally_End], true);
	}
	
	return 1;
}

stock GetErrorCodeName(code, bool:pack = false) {
	new name[32];
	
	new func = GetPublicAddressFromIndex(code);
	
	CallFunction(func, code, ref(name));
	
	if (!pack) {
		strunpack(name, name);
	}
	
	return name;
}

stock GetTryStackIndex() {
	return s_CTX;
}

stock ThrowError(const message[], code = -1) {
	e_assert g_@eInitialized;
	e_assert s_CTX >= -1;
	
	if (s_CTX > -1) {
		// Was the error thrown in catch/finally?
		if (s_ContextStack[s_CTX][CurrentBlock] != CurrentBlockTry) {
			PopContext();
		}
	}
	
	new try_idx = s_ContextStack[s_CTX][TryIdx];
	
	e_assert 0 <= try_idx < s_NumTryStatements;
	
	strunpack(s_LastException[Message], message, MAX_EXCEPTION_MESSAGE);
	s_LastException[Code] = code;
	s_LastException[Caught] = false;
	
	if (s_CTX == -1) {
		new ignore = false;
		
		if (funcidx(!"OnUncaughtException") != -1) {
			ignore = !!CallFunction(GetPublicAddressFromName("OnUncaughtException"), ref(s_LastException));
		}
		
		if (!ignore) {
			printf("Unhandled exception: %s", message);
			
			exit;
		}
	} else {
		new pop = false;
		
		if (!s_TryStatements[try_idx][HasCatch] && !s_TryStatements[try_idx][HasFinally]) {
			pop = true;
			s_HasException = false;
		} else {
			s_HasException = true;
		}
		
		JumpToContext(s_TryStatements[try_idx][Try_End], pop);
	}
}

stock ThrowException(e[E_EXCEPTION]) {
	ThrowError(e[Message], e[Code]);
}

static stock ReturnHook(retval) {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	new try_idx = s_ContextStack[s_CTX][TryIdx];
	
	e_assert 0 <= try_idx < s_NumTryStatements;
	
	s_ContextStack[s_CTX][ReturnValue] = retval;
	
	switch (s_ContextStack[s_CTX][CurrentBlock]) {
		case CurrentBlockTry: {
			// Should we return now?
			if (!s_TryStatements[try_idx][HasCatch] && !s_TryStatements[try_idx][HasFinally]) {
				DoReturn(retval);
			} else {
				s_ContextStack[s_CTX][ShouldReturn] = true;
				
				JumpToContext(s_TryStatements[try_idx][Try_End]);
			}
		}
		
		case CurrentBlockCatch: {
			// Should we return now?
			if (!s_TryStatements[try_idx][HasFinally]) {
				DoReturn(retval);
			} else {
				s_ContextStack[s_CTX][ShouldReturn] = true;
				
				JumpToContext(s_TryStatements[try_idx][Catch_End]);
			}
		}
		
		case CurrentBlockFinally: {
			DoReturn(retval);
		}
	}
	
	return 0;
}

static stock JumpToContext(_cip, pop = false) {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	static frm, hea, stk, cip;
	
	frm = s_ContextStack[s_CTX][FRM];
	hea = s_ContextStack[s_CTX][HEA];
	stk = s_ContextStack[s_CTX][STK];
	cip = _cip;
	
	if (pop) {
		PopContext();
	} else {}
	
	// Restore the context and jump
	#emit LOAD.pri frm
	#emit SCTRL 5
	#emit LOAD.pri hea
	#emit SCTRL 2
	#emit LOAD.pri stk
	#emit SCTRL 4
	#emit LOAD.pri cip
	#emit SCTRL 6
}

static stock DoReturn(_retval) {
	e_assert g_@eInitialized;
	e_assert s_CTX > -1;
	
	// Use static so they can be accessed after the frame has been modified
	static frm, hea, retval;
	
	frm = s_ContextStack[s_CTX][FRM];
	hea = s_ContextStack[s_CTX][HEA];
	retval = _retval;
	
	PopContext();
	
	// Set the heap
	#emit LOAD.pri hea
	#emit SCTRL 2
	// Set the frame
	#emit LOAD.pri frm
	// Make it the current one
	#emit SCTRL 5
	// Also set the stack to its value
	#emit SCTRL 4
	// Load the return value
	#emit LOAD.pri retval
	// Return!
	#emit RETN
}

stock InitExceptions() {
	e_assert !g_@eInitialized;
	
	g_@eInitialized = true;
	
	// Find all UniqueErrorCode init functions and invoke them
	for (new idx = 0, name[32]; GetPublicNameFromIndex(idx, name); idx++) {
		if (strcmp(!"@_eC", name, false, 4) == 0) {
			new func = GetPublicAddressFromIndex(idx);
			
			CallFunction(func, idx, ref(name));
		}
	}
	
	// Gather some info about the AMX
	new hdr[AMX_HDR], base, cod, dat;
	
	GetAmxHeader(hdr);
	
	base = GetAmxBaseAddress();
	cod = hdr[AMX_HDR_COD];
	dat = hdr[AMX_HDR_DAT];
	
	new cod_start = cod - dat;
	new address = cod_start;
	
	// Get the addresses of some functions we'll need
	new addr_TryStart, addr_TryEnd, addr_ReturnHook, addr_CatchEnd, addr_FinallyEnd;

	#emit CONST.pri   TryStart
	#emit STOR.S.pri  addr_TryStart
	#emit CONST.pri   TryEnd
	#emit STOR.S.pri  addr_TryEnd
	#emit CONST.pri   ReturnHook
	#emit STOR.S.pri  addr_ReturnHook
	#emit CONST.pri   CatchEnd
	#emit STOR.S.pri  addr_CatchEnd
	#emit CONST.pri   FinallyEnd
	#emit STOR.S.pri  addr_FinallyEnd
	
	// Relocate them (so they can be used for AMX instructions)
	addr_TryStart += base + cod;
	addr_TryEnd += base + cod;
	addr_ReturnHook += base + cod;
	addr_CatchEnd += base + cod;
	addr_FinallyEnd += base + cod;
	
	new operand, found;
	
	// Look for try statements
	while (FindOpcodeWithOperand(address, OP_CONST_ALT, _@eTRY_MAGIC1)
	    && FindOpcodeWithOperand(address, OP_CONST_ALT, _@eTRY_MAGIC2)) {
		new try_idx = s_NumTryStatements++;
		new start = address;
		
		found = FindOpcode(address, OP_CALL);
		e_assert found;
		e_assert ReadOperand(address) == addr_TryEnd;
		
		// Search for TryStart(!_@eTRUE, !_@eFALSE, _@eTRY_MAGIC3)
		found = FindOpcodeWithOperand(address, OP_PUSH_C, _@eTRY_MAGIC3);
		
		// Compiled with -O0
		if (!found) {
			found = FindOpcodeWithOperand(address, OP_CONST_PRI, _@eTRY_MAGIC3);
			e_assert found;
			
			WriteOpcode(address, OP_NOP);
			address += 4;
			WriteOpcode(address, OP_PUSH_C);
		}
		
		// Replace _@eTRY_MAGIC3 with the value of try_idx
		WriteOperand(address, try_idx);
		// Replace !_@eFALSE with the value from LCTRL 4 (STK)
		WriteOpcode(address + 8, OP_LCTRL, 4);
		WriteOpcode(address + 16, OP_NOP);
		// Replace !_@eTRUE with the value from LCTRL 5 (FRM)
		WriteOpcode(address + 24, OP_LCTRL, 5);
		WriteOpcode(address + 32, OP_NOP);
		
		// Find the end of the try block
		found = FindOpcode(address, OP_CALL);
		e_assert found;
		e_assert ReadOperand(address) == addr_TryStart;
		found = FindOpcode(address, OP_JUMP);
		e_assert found;
		found = FindOpcode(address, OP_JZER);
		e_assert found;
		
		operand = ReadOperand(address);
		
		new end = operand - base - dat;
		
		e_assert ReadOpcode(end) == OP_STACK;
		e_assert ReadOperand(end) == 4;
		
		// Skip the STACK opcode
		end += 8;
		
		s_TryStatements[try_idx][Try_End] = operand + 8 - base - cod;
		
		// Look for a catch statement
		address = end;
		
		if (ReadOpcode(address) == OP_BREAK) {
			address += 4;
		}
		
		if (ReadOpcode(address) == OP_LOAD_PRI && ReadOperand(address) == ref(_@eTRUE)
		 && ReadOpcode(address + 8) == OP_CONST_ALT && ReadOperand(address + 8) == _@eCATCH_MAGIC) {
			s_TryStatements[try_idx][HasCatch] = true;
			
			address += 16;
			
			e_assert ReadOpcode(address) == OP_OR;
			
			address += 4;
			
			// Find the end of the catch statement
			e_assert ReadOpcode(address) == OP_JZER;
			
			operand = ReadOperand(address);
			
			end = operand - base - dat;
			
			// Make sure the STACK opcode from the loop is correct
			e_assert ReadOpcode(end - 8) == OP_STACK;
			e_assert ReadOperand(end - 8) == _:E_EXCEPTION * 4;
			
			s_TryStatements[try_idx][Catch_End] = operand - base - cod;
		}
		
		// Look for a finally statement
		address = end;
		
 		if (ReadOpcode(address) == OP_BREAK) {
 			address += 4;
 		}
		
		if (ReadOpcode(address) == OP_LOAD_PRI && ReadOperand(address) == ref(_@eTRUE)
		 && ReadOpcode(address + 8) == OP_CONST_ALT && ReadOperand(address + 8) == _@eFINALLY_MAGIC) {
			s_TryStatements[try_idx][HasFinally] = true;
			
			address += 16;
			
			e_assert ReadOpcode(address) == OP_OR;
			
			address += 4;
			
			// Find the end of the catch statement
			e_assert ReadOpcode(address) == OP_JZER;
			
			operand = ReadOperand(address);
			
			end = operand - base - dat;
			
			// Make sure the STACK opcode from the loop is correct
			e_assert ReadOpcode(end - 8) == OP_STACK;
			e_assert ReadOperand(end - 8) == 4;
			
			s_TryStatements[try_idx][Finally_End] = operand - base - cod;
		}
		
		// Naked function that invokes ReturnHook
		AsmInit(s_ReturnHooks[try_idx], s_ReturnHookCode[try_idx]);
		// Push the return value (was set before jumping here)
		AsmEmitPushPri(s_ReturnHooks[try_idx]);
		// Push argument count
		AsmEmitPushC(s_ReturnHooks[try_idx], 4);
		// Call ReturnHook
		AsmEmitCall(s_ReturnHooks[try_idx], addr_ReturnHook);
		
		// Hook RETN opcodes
		// TODO: Hook RET opcodes
		address = start;
		
		while (FindOpcode(address, OP_RETN, end)) {
			e_assert ReadOpcode(address - 8) == OP_STACK;
			
			// Hook the return statement
			WriteOpcode(address - 8, OP_NOP);
			WriteOpcode(address - 4, OP_CALL, AsmGetCode(s_ReturnHooks[try_idx]) + base + cod);
			
			// Don't leave the address at an operand
			address += 4;
		}
		
		address = end;
	}
}

static stock Opcode:ReadOpcode(address) {
	new Opcode:opcode = Opcode:ReadAmxMemory(address);
	opcode = UnrelocateOpcode(opcode);
	
	// Make sure it's valid
	e_assert _:OP_NONE <= _:opcode < NUM_OPCODES;
	
	return opcode;
}

static stock ReadOperand(address, end = 0) {
	e_assert sc_OpcodeOperands[ReadOpcode(address)] == 1;
	e_assert address + 4 < end;
	
	return ReadAmxMemory(address + 4);
}

static stock WriteOpcode(address, Opcode:opcode, operand = 0) {
	// Are we dealing with a valid opcode?
	e_assert _:OP_NONE <= _:opcode < NUM_OPCODES;
	e_assert sc_OpcodeOperands[opcode] != -1;
	
	new Opcode:opcode_reloc = RelocateOpcode(opcode);
	
	// There's a bug in an earlier version causing issues on Windows
	if (opcode && !opcode_reloc) {
		print(!"\nERROR: Please get the latest version of the amx_assembly files: http://goo.gl/qEfLm\n");
		
		exit;
	}
	
	WriteAmxMemory(address, _:opcode_reloc);
	
	if (sc_OpcodeOperands[opcode]) {
		WriteAmxMemory(address + 4, operand);
	}
}

static stock WriteOperand(address, operand) {
	e_assert sc_OpcodeOperands[ReadOpcode(address)] == 1;
	
	WriteAmxMemory(address + 4, operand);
}

static stock FindNextOpcode(&address, &Opcode:opcode, end = 0) {
	new Opcode:current_opcode = ReadOpcode(address);
	new start_address = address;
	
	address += 4;
	
	if (address >= end) {
		address = start_address;
		
		return 0;
	}
	
	// Case tables have dynamic operand sizes
	if (current_opcode == OP_CASETBL) {
		address += 8 + 8 * ReadAmxMemory(address);
	} else {
		// Are we dealing with a valid opcode?
		e_assert sc_OpcodeOperands[current_opcode] != -1;
		
		address += 4 * sc_OpcodeOperands[current_opcode];
	}
	
	// Nothing more to read
	if (address >= end) {
		address = start_address;
		
		return 0;
	}
	
	opcode = ReadOpcode(address);
	
	return 1;
}

static stock FindOpcode(&address, Opcode:opcode, end = 0) {
	new start_address = address;
	new Opcode:current_opcode;
	
	while (FindNextOpcode(address, current_opcode, end)) {
		if (current_opcode == opcode) {
			return 1;
		}
	}
	
	// Nothing was found
	address = start_address;
	
	return 0;
}

static stock FindOpcodeWithOperand(&address, Opcode:opcode, operand, end = 0) {
	new start_address = address;
	
	while (FindOpcode(address, opcode, end)) {
		if (ReadOperand(address, end) == operand) {
			return 1;
		}
	}
	
	// Nothing was found
	address = start_address;
	
	return 0;
}

static stock PushContext() {
	e_assert s_CTX >= -1;
	
	s_CTX++;
	
	e_assert s_CTX < MAX_CONTEXT_STACK;
}

static stock PopContext() {
	e_assert s_CTX > -1;
	
	s_CTX--;
}
