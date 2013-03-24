#if !defined MAX_CATCH_STACK
	#define MAX_CATCH_STACK 128
#endif

#if !defined MAX_EXCEPTION_MESSAGE
	#define MAX_EXCEPTION_MESSAGE 256
#endif

#define try \
	for (; !TryException(); TryExceptionEnd())

#define catch%1(%2) \
	for (new %2[E_EXCEPTION]; CatchException(%2); CatchExceptionEnd())

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
	bool:s_ExceptionThrown = false,
	bool:s_SkipNextTry = false,
	bool:s_SkipNextCatch = false
;

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
