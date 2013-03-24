#if !defined MAX_CATCH_STACK
  #define MAX_CATCH_STACK 128
#endif

#if !defined MAX_EXCEPTION_MESSAGE
	#define MAX_EXCEPTION_MESSAGE 256
#endif

#define try \
	for (new __try = 1; __try && TryException() && __try && ((__try = 0) || (__try + 1)); __try = 0)

#define catch%1(%2) \
	for (new %2[E_EXCEPTION], __catch = 1; __catch && CatchException(%2); __catch = 0)

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

static
	     s_CatchStack[MAX_CATCH_STACK][E_CATCH_STACK],
	     s_Exception[E_EXCEPTION],
	     s_CST = -1,
	bool:s_ExceptionThrown = false
;

stock ThrowException(const message[], code = 0) {
	s_ExceptionThrown = true;
	
	if (s_CST == -1) {
		printf("Uncaught exception: %s", message);
		
		// AMX_ERR_NATIVE
		#emit HALT 10
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
	#emit CONST.pri 0
	#emit LOAD.pri s_cip
	#emit SCTRL 6
}

stock TryException() {
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
	
	return 1;
}

stock CatchException(e[E_EXCEPTION]) {
	e[Code] = s_Exception[Code];
	strunpack(e[Message], s_Exception[Message], MAX_EXCEPTION_MESSAGE);
	
	s_CST -= 1;
	
	assert s_CST >= -1;
	
	if (s_ExceptionThrown) {
		s_ExceptionThrown = false;
		
		return true;
	}
	
	return false;
}
