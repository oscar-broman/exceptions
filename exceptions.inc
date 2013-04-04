#include <a_samp>
#tryinclude <amx_assembly\opcode>
#tryinclude <amx_assembly\phys_memory>
#tryinclude <amx_assembly\asm>

#if !defined MAX_CATCH_STACK
	#define MAX_CATCH_STACK 128
#endif

#if !defined MAX_EXCEPTION_MESSAGE
	#define MAX_EXCEPTION_MESSAGE 256
#endif

#if !defined _inc_opcode
	#error The amx_assembly library is required, get it here: http:/\
	                                                                /goo.gl/qEfLm
#endif

#if tagof(NUM_OPCODES) == tagof(OP_NONE)
	#error Please get the latest version of the amx_assembly files: http:/\
	                                                                      /goo.gl/qEfLm
#endif

#define try \
	if (!g_@eInitialized) InitExceptions(); for (new __r = __r | _@eTRY_MAGIC1 | _@eTRY_MAGIC2 | _@eTRY_MAGIC3 | _@eTRY_MAGIC4, __e = -1; __r && ((__e = TryStart(!_@eTRUE, !_@eFALSE)) || _@eTRUE) || TryEnd(__e); __r = 0)

#define catch%1(%2) \
	if (_@eTRUE || _@eCATCH_MAGIC) for (new %2[E_EXCEPTION]; !%2[Caught] && CatchException(%2); %2[Caught] = true, CatchEnd())

#define finally \
	if (_@eTRUE || _@eFINALLY_MAGIC) for (new __f = 1; __f; FinallyEnd(), __f = 0)

// Used to identify try/catch/finally statements in the AMX instructions
const _@eTRY_MAGIC1    = 0x026A57FD;
const _@eTRY_MAGIC2    = 0xABD8C2BA;
const _@eTRY_MAGIC3    = 0xD3120889;
const _@eTRY_MAGIC4    = 0xFCAB063E;
const _@eCATCH_MAGIC   = 0x4CD52FB1;
const _@eFINALLY_MAGIC = 0xB408FAA2;

// Used to prevent redundant test warnings
stock _@eTRUE          = 1;
stock _@eFALSE         = 0;

// Will be provided in the catch statement
enum E_EXCEPTION {
	     Message[MAX_EXCEPTION_MESSAGE],
	     Code,
	bool:Caught
}

enum E_TRY_INFO {
	FRM,
	HEA,
	STK,
	CIP,
	Try
}

enum E_TRY_STATEMENT {
	// Memory address in COD
	Try_Start,
	Try_End,
	Catch_Start,
	Catch_End,
	Finally_Start,
	Finally_End
}

// Number of operands an opcode takes
// CASETBL is a special case; it doesn't have a fixed number of operands
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
	s_TryStatements[2048][E_TRY_STATEMENT],
	s_ReturnHooks[sizeof(s_TryStatements)][AsmContext],
	s_ReturnHookCode[sizeof(s_TryStatements)][64],
	s_TryStackIndex = -1,
	s_TryStack[128][E_TRY_INFO],
	s_NumTryStatements = 0,
	s_HasException = false,
	s_LastException[E_EXCEPTION],
	s_ShouldReturn = false,
	s_ReturnValue
;
	
new
	g_@eInitialized = false
;

forward OnUncaughtException(e[E_EXCEPTION]);
static Initialize();

stock InitExceptions() {
	Initialize();
}

stock GetTryStackIndex() {
	return s_TryStackIndex;
}

stock TryStart(frm, stk, ...) {
	print("TryStart");
	if (!g_@eInitialized) Initialize();
	
	s_ShouldReturn = false;
	
	new try_idx;
	
	// Adjust to skip the for-loop's local variables (__r and __e)
	stk += 8;
	
	// Third argument
	#emit LOAD.S.pri 20
	#emit STOR.S.pri try_idx
	
	new cip, hea;
	
	// Get the heap pointer
	#emit LCTRL 2
	#emit STOR.S.pri hea
	
	// Get the return address
	#emit LOAD.S.pri 4
	#emit STOR.S.pri cip
	
	new i = ++s_TryStackIndex;
	
	s_TryStack[i][FRM] = frm;
	s_TryStack[i][HEA] = hea;
	s_TryStack[i][STK] = stk;
	s_TryStack[i][CIP] = cip;
	
	s_TryStack[i][Try] = try_idx;
	
	return i;
}

stock TryEnd(idx) {
	#pragma unused idx
	
	// If there's no try statement, decrement s_TryStackIndex
	if (!s_TryStatements[s_TryStack[idx][Try]][Catch_Start]) {
		s_TryStackIndex--;
	}
	
	print("TryEnd");
	
	return 0;
}

static TryReturn(try_idx, retval) {
	print("TryReturn");
	
	s_ShouldReturn = true;
	s_ReturnValue = retval;
	
	// Restore the stack, frame, and heap then jump to the end of the try statement
	static frm, hea, stk, cip;
	
	new i = s_TryStackIndex;

	frm = s_TryStack[i][FRM];
	hea = s_TryStack[i][HEA];
	stk = s_TryStack[i][STK];
	cip = s_TryStatements[s_TryStack[i][Try]][Try_End];
	
	s_TryStackIndex--;
	
	// Restore the context
	#emit LOAD.pri frm
	#emit SCTRL 5
	#emit LOAD.pri hea
	#emit SCTRL 2
	#emit LOAD.pri stk
	#emit SCTRL 4
	#emit LOAD.pri cip
	#emit SCTRL 6
	
	return 0;
}

stock CatchEnd() {
	s_TryStackIndex--;
	
	return 0;
}

stock FinallyEnd() {
	// Was there a return statement in the try/catch blocks?
	if (s_ShouldReturn) {
		printf("FinallyEnd(%d)", s_ReturnValue);
		// Get the previous frame
		#emit LOAD.S.pri 0
		// Make the previous frame the current one
		#emit SCTRL 5
		// Also set the stack to the current one
		#emit SCTRL 4
		// Load the return value
		#emit LOAD.pri s_ReturnValue
		// Return!
		#emit RETN
	} else {
		printf("FinallyEnd()");
	}
}

stock ThrowError(const message[], code = 0) {
	print("ThrowError");
	if (!g_@eInitialized) Initialize();
	
	strunpack(s_LastException[Message], message, MAX_EXCEPTION_MESSAGE);
	
	s_LastException[Code] = code;
	s_LastException[Caught] = false;
	
	// Not inside a try block?
	if (s_TryStackIndex < 0) {
		new ignore = false;
		
		if (funcidx(!"OnUncaughtException") != -1) {
			ignore = !!CallFunction(GetPublicAddressFromName("OnUncaughtException"), ref(s_LastException));
		}
		
		if (!ignore) {
			printf("Unhandled exception: %s", message);
			
			exit;
		}
	} else {
		s_HasException = true;
		
		// Restore the stack, frame, and heap then jump to the end of the try statement
		static frm, hea, stk, cip;
		new i = s_TryStackIndex;
	
		frm = s_TryStack[i][FRM];
		hea = s_TryStack[i][HEA];
		stk = s_TryStack[i][STK];
		cip = s_TryStatements[s_TryStack[i][Try]][Try_End];
		
		// If there's no catch statement, decrement s_TryStackIndex
		if (!s_TryStatements[s_TryStack[i][Try]][Catch_Start]) {
			s_TryStackIndex--;
		}
		
		// Restore the context
		#emit LOAD.pri frm
		#emit SCTRL 5
		#emit LOAD.pri hea
		#emit SCTRL 2
		#emit LOAD.pri stk
		#emit SCTRL 4
		#emit LOAD.pri cip
		#emit SCTRL 6
	}
}

stock CatchException(e[E_EXCEPTION]) {
	if (!g_@eInitialized) Initialize();
	
	if (!s_HasException) {
		print("CatchException? no");
		return 0;
	}

	print("CatchException? yes");
	
	s_HasException = false;
	
	strunpack(e[Message], s_LastException[Message], MAX_EXCEPTION_MESSAGE);
	
	e[Code] = s_LastException[Code];
	e[Caught] = s_LastException[Caught];
	
	return 1;
}

// Read and relocate an opcode
static Opcode:ReadOpcode(address) {
	assert address < 0;
	
	new Opcode:opcode = Opcode:ReadAmxMemory(address);
	opcode = UnrelocateOpcode(opcode);
	
	assert opcode >= OP_NONE && _:opcode < _:NUM_OPCODES;
	
	return opcode;
}

static Opcode:FindNextOpcode(&address) {
	new Opcode:opcode = ReadOpcode(address);
	
	address += 4;
	
	if (opcode == OP_CASETBL) {
		address += 8 + 8 * ReadAmxMemory(address);
	} else {
		assert sc_OpcodeOperands[opcode] != -1;
		
		address += 4 * sc_OpcodeOperands[opcode];
	}
	
	if (address >= 0) {
		return OP_NONE;
	}
	
	return ReadOpcode(address);
}

static Initialize() {
	if (g_@eInitialized) return;
	
	g_@eInitialized = true;
	
	new try_pattern[12];
	
	// Relocate needed opcodes
	new op_const_alt = _:RelocateOpcode(OP_CONST_ALT);
	new op_or        = _:RelocateOpcode(OP_OR);
	new op_not       = _:RelocateOpcode(OP_NOT);
	new op_nop       = _:RelocateOpcode(OP_NOP);
	new op_load_pri  = _:RelocateOpcode(OP_LOAD_PRI);
	new op_push_pri  = _:RelocateOpcode(OP_PUSH_PRI);
	new op_push_c    = _:RelocateOpcode(OP_PUSH_C);
	new op_lctrl     = _:RelocateOpcode(OP_LCTRL);
	new op_zero_pri  = _:RelocateOpcode(OP_ZERO_PRI);
	new op_jump      = _:RelocateOpcode(OP_JUMP);
	new op_jzer      = _:RelocateOpcode(OP_JZER);
	new op_stack     = _:RelocateOpcode(OP_STACK);
	
	// There's a bug in an earlier version causing issues on Windows
	if (op_const_alt == 0) {
		print(!"\nERROR: Please get the latest version of the amx_assembly files: http://goo.gl/qEfLm\n");
		
		exit;
	}
	
	// This should be inside the try macro's for loop
	try_pattern[0] = op_const_alt; try_pattern[1] = _@eTRY_MAGIC1;
	try_pattern[2] = op_or;
	try_pattern[3] = op_const_alt; try_pattern[4] = _@eTRY_MAGIC2;
	try_pattern[5] = op_or;
	try_pattern[6] = op_const_alt; try_pattern[7] = _@eTRY_MAGIC3;
	try_pattern[8] = op_or;
	try_pattern[9] = op_const_alt; try_pattern[10] = _@eTRY_MAGIC4;
	try_pattern[11] = op_or;
	
	// Gather some info about the AMX
	new hdr[AMX_HDR], base, cod, dat;
	
	GetAmxHeader(hdr);
	
	base = GetAmxBaseAddress();
	cod = hdr[AMX_HDR_COD];
	dat = hdr[AMX_HDR_DAT];
	
	new cod_start = cod - dat;
	new cod_end = cod_start + dat - cod;
	new address = cod_start;
	
	// Get the addresses of some functions we'll need
	new addr_TryStart, addr_TryReturn, addr_CatchException, addr_CatchEnd;

	#emit CONST.pri   TryStart
	#emit STOR.S.pri  addr_TryStart
	#emit CONST.pri   TryReturn
	#emit STOR.S.pri  addr_TryReturn
	#emit CONST.pri   CatchException
	#emit STOR.S.pri  addr_CatchException
	#emit CONST.pri   CatchEnd
	#emit STOR.S.pri  addr_CatchEnd
	
	// Relocate them (so they can be used for AMX instructions)
	addr_TryStart += base + cod;
	addr_TryReturn += base + cod;
	addr_CatchException += base + cod;
	addr_CatchEnd += base + cod;
	
	// Read the first opcode
	new Opcode:opcode = ReadOpcode(address);
	
	// Scan the code top to bottom
	while (address < cod_end) {
		// Could be a try statement
		if (opcode == OP_CONST_ALT) {
			new match = true, start = address;
			
			for (new i = 0; i < sizeof(try_pattern); i++) {
				if (ReadAmxMemory(address + i * 4) != try_pattern[i]) {
					match = false;
					
					break;
				}
			}
			
			// It's a try statement
			if (match) {
				new i = s_NumTryStatements++;
				
				s_TryStatements[i][Try_Start] = address;
				
				// Search for this part of the try macro:
				//   TryStart(!_@eTRUE, !_@eFALSE)
				do {
					opcode = FindNextOpcode(address);
				} while (opcode != OP_CALL);
				
				assert ReadAmxMemory(address + 4) == addr_TryStart;
				assert ReadAmxMemory(address - 40) == op_load_pri;
				assert ReadAmxMemory(address - 36) == ref(_@eFALSE);
				assert ReadAmxMemory(address - 32) == op_not;
				assert ReadAmxMemory(address - 16) == op_not;
				
				// Push this try statement's index
				WriteAmxMemory(address - 32, op_push_c);
				WriteAmxMemory(address - 28, i);
				
				// Replace !_@eFALSE with the value from LCTRL 4 (STK)
				WriteAmxMemory(address - 40, op_lctrl);
				WriteAmxMemory(address - 36, 4);
				WriteAmxMemory(address - 24, op_push_pri);
				
				// Replace !_@eTRUE with the value from LCTRL 5 (FRM)
				WriteAmxMemory(address - 20, op_lctrl);
				WriteAmxMemory(address - 16, 5);
				WriteAmxMemory(address - 12, op_push_pri);
				
				// Push the argument count
				WriteAmxMemory(address - 8, op_push_c);
				WriteAmxMemory(address - 4, 3 * 4);
				
				// Find the end of the try block
				assert ReadAmxMemory(address + 40) == op_zero_pri;
				assert ReadAmxMemory(address + 44) == op_jump;
				
				address = ReadAmxMemory(address + 80) - base - dat;
				address = ReadAmxMemory(address + 4) - base - dat;
				
				assert ReadAmxMemory(address + 8) == op_jzer;
				
				address = ReadAmxMemory(address + 12) - base - dat;
				
				new end = address;
				
				assert ReadAmxMemory(end) == op_stack;
				assert ReadAmxMemory(end + 4) == 8;
				
				s_TryStatements[i][Try_End] = address + dat - cod + 8;
				
				// Look for a catch statement
				address = end;
				
				if (address + 32 >= cod_end) {
					break;
				}
				
				if (ReadAmxMemory(address + 28) == _@eCATCH_MAGIC || ReadAmxMemory(address + 32) == _@eCATCH_MAGIC) {
					do {
						opcode = FindNextOpcode(address);
					} while (opcode != OP_CALL);
					
					assert ReadAmxMemory(address + 4) == addr_CatchEnd;
					
					do {
						opcode = FindNextOpcode(address);
					} while (opcode != OP_CALL);
					
					assert ReadAmxMemory(address + 4) == addr_CatchException;
					
					// Find the end of the catch block
					do {
						opcode = FindNextOpcode(address);
					} while (opcode != OP_JUMP);
					do {
						opcode = FindNextOpcode(address);
					} while (opcode != OP_JZER);
					
					s_TryStatements[i][Catch_Start] = address + dat - cod + 8;
					
					end = ReadAmxMemory(address + 4) - base - dat;
					
					assert ReadOpcode(end) == OP_STACK;
					
					// Skip the STACK opcode
					end += 8;
					
					// Save it
					s_TryStatements[i][Catch_End] = end + dat - cod + 8;
				}
				
				// Find return statements
				address = start;
				
				new Opcode:prev_opcode = OP_NONE;
				opcode = ReadOpcode(address);
				
				while (address < end) {
					if (opcode == OP_RETN) {
						assert prev_opcode == OP_STACK;
						
						// Lazily create the return hook function
						if (!s_ReturnHooks[i][AsmContext_buffer_size]) {
							AsmInit(s_ReturnHooks[i], s_ReturnHookCode[i]);
							
							// Push the return value
							AsmEmitPushPri(s_ReturnHooks[i]);
							// Push the try statement's index (s_TryStatements)
							AsmEmitPushC(s_ReturnHooks[i], i);
							// Push argument count
							AsmEmitPushC(s_ReturnHooks[i], 8);
							// Call TryReturn
							AsmEmitCall(s_ReturnHooks[i], addr_TryReturn);
						}
						
						// Hook the return statement
						WriteAmxMemory(address - 8, op_nop);
						WriteAmxMemory(address - 4, _:RelocateOpcode(OP_CALL));
						WriteAmxMemory(address    , base + cod + AsmGetCode(s_ReturnHooks[i]));
						
						address += 4;
					}
					
					prev_opcode = opcode;
					opcode = FindNextOpcode(address);
				}
				
				
				// if (ReadAmxMemory(address + 28) == _@eFINALLY_MAGIC || ReadAmxMemory(address + 32) == _@eFINALLY_MAGIC) {
				// 	
				// 	print("found finally");
				// }
			}
		}
		
		opcode = FindNextOpcode(address);
	}
}