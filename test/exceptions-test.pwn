#include <a_samp>
#include "..\exceptions"

new TRUE = true;
new finally_ok, catch_ok;

#define Assert(%1,%2);                     \
	if (TRUE && !(%2)) {                   \
		print("assertion " #%1 " failed"); \
		exit;                              \
	}

public OnFilterScriptInit() {
	Entry();
	
	print("ok");
}

// Make sure the stack and heap looks OK after
stock Entry(arg1 = 516427, arg2[] = "arg2", const arg3[] = "arg3") {
	new local1 = 958072, local2[] = "local2";
	new const local3[] = "local3";
	
	Assert(1, Try() == 55667788);
	Assert(2, GetTryStackIndex() == -1);
	Assert(3, TryCatch() == 55667788);
	Assert(4, GetTryStackIndex() == -1);
	Assert(5, TryFinally() == 55667788);
	Assert(6, GetTryStackIndex() == -1);
	Assert(7, TryFinallyCatch() == 632453);
	Assert(8, GetTryStackIndex() == -1);
	finally_ok = 0;
	catch_ok = 0;
	Assert(9, RecursiveTry() == 1234);
	Assert(10, finally_ok);
	Assert(11, catch_ok);
	Assert(12, GetTryStackIndex() == -1);
	Assert(13, GetTryStackIndex() == -1);
	
	PublicFunctions();
	
	Assert(14, arg1 == 516427);
	Assert(15, arg2[0] && strcmp(arg2, "arg2") == 0);
	Assert(16, arg3[0] && strcmp(arg3, "arg3") == 0);
	Assert(17, local1 == 958072);
	Assert(18, local2[0] && strcmp(local2, "local2") == 0);
	Assert(19, local3[0] && strcmp(local3, "local3") == 0);
}

stock Try() {
	try {
		ThrowError("errorrrr");
	}
	
	Assert(20, GetTryStackIndex() == -1);

	try {
		
	}
	
	Assert(21, GetTryStackIndex() == -1);
	
	try {
		return 55667788;
	}
	
	Assert(22, GetTryStackIndex() == -1);
	
	return 222;
}

stock TryCatch() {
	try {
		ThrowError("errorrrr");
	} catch (e) {
		Assert(23, e[Message][0] && strcmp(e[Message], "errorrrr") == 0);
	}
	
	Assert(24, GetTryStackIndex() == -1);

	try {
		
	} catch (e) {
		Assert(25, TRUE && false);
	}
	
	Assert(26, GetTryStackIndex() == -1);
	
	try {
		return 55667788;
	} catch (e) {
		Assert(27, TRUE && false);
	}
	
	Assert(28, GetTryStackIndex() == -1);
	
	return 222;
}

stock TryFinally() {
	finally_ok = 0;
	
	try {
		ThrowError("errorrrr");
	} finally {
		finally_ok = 1;
	}
	
	Assert(29, finally_ok);
	Assert(30, GetTryStackIndex() == -1);
	
	finally_ok = 0;
	
	try {
		
	} finally {
		finally_ok = 1;
	}
	
	Assert(31, finally_ok);
	Assert(32, GetTryStackIndex() == -1);
	
	finally_ok = 0;
	Assert(33, TryFinally_Return1() == 1234);
	Assert(34, finally_ok);
	finally_ok = 0;
	Assert(35, TryFinally_Return2() == 546);
	Assert(36, finally_ok);
	finally_ok = 0;
	Assert(37, TryFinally_Return3() == 652);
	Assert(38, finally_ok);
	Assert(39, GetTryStackIndex() == -1);
	
	try {
		return 1234;
	} finally {
		return 55667788;
	}
	
	Assert(40, false);
	
	return 632;
}

stock TryFinally_Return1() {
	try {
		return 1234;
	} finally {
		finally_ok = 1;
	}
	
	return 234;
}

stock TryFinally_Return2() {
	try {
		
	} finally {
		finally_ok = 1;
		return 546;
	}
	
	return 111;
}

stock TryFinally_Return3() {
	try {
		return 2345;
	} finally {
		finally_ok = 1;
		return 652;
	}
	
	return 111;
}

stock TryFinallyCatch() {
	finally_ok = 0;
	
	try {
		
	} catch (e) {
		Assert(41, false);
	} finally {
		finally_ok = 1;
	}
	
	Assert(42, finally_ok);
	Assert(43, GetTryStackIndex() == -1);
	
	finally_ok = 0;
	catch_ok = 0;
	
	try {
		ThrowError("asdf");
	} catch (e) {
		Assert(44, e[Message][0] && strcmp(e[Message], "asdf") == 0);
		catch_ok = 1;
	} finally {
		finally_ok = 1;
	}
	
	Assert(45, finally_ok);
	Assert(46, catch_ok);
	Assert(47, GetTryStackIndex() == -1);
	
	finally_ok = 0;
	catch_ok = 1;
	Assert(48, TryCatchFinally_Return1() == 1234);
	Assert(49, finally_ok);
	Assert(51, catch_ok);
	finally_ok = 0;
	catch_ok = 0;
	Assert(50, TryCatchFinally_Return2() == 546);
	Assert(51, finally_ok);
	Assert(51, catch_ok);
	finally_ok = 0;
	catch_ok = 1;
	Assert(52, TryCatchFinally_Return3() == 652);
	Assert(53, finally_ok);
	Assert(51, catch_ok);
	Assert(54, GetTryStackIndex() == -1);
	
	catch_ok = 0;
	
	try {
		ThrowError("asd");
	} catch (e) {
		catch_ok = 1;
	} finally {
		Assert(55, catch_ok);
		
		return 632453;
	}
	
	Assert(56, false);
	
	return 245;
}

stock TryCatchFinally_Return1() {
	try {
		return 1234;
	} catch (e) {
		catch_ok = 0;
	} finally {
		finally_ok = 1;
	}
	
	return 234;
}

stock TryCatchFinally_Return2() {
	try {
		ThrowError("asdf");
	} catch (e) {
		catch_ok = 1;
		return 546;
	} finally {
		finally_ok = 1;
	}
	
	return 111;
}

stock TryCatchFinally_Return3() {
	try {
		
	} catch (e) {
		catch_ok = 0;
	} finally {
		finally_ok = 1;
		return 652;
	}
	
	return 111;
}

stock NestedCalls() {
	try {
		NestedCalls_1();
		catch_ok = 0;
		finally_ok = 0;
	} catch (e) {
		catch_ok = 1;
	} finally {
		finally_ok = 1;
	}
	
	Assert(57, finally_ok);
	Assert(58, catch_ok);
	Assert(59, GetTryStackIndex() == -1);
}

stock NestedCalls_1() {
	catch_ok = 0;
	finally_ok = 0;
	
	try {
		NestedCalls_2();
	} catch (e) {
		catch_ok = 1;
	} finally {
		finally_ok = 1;
	}
	
	Assert(60, finally_ok);
	Assert(61, catch_ok);
	Assert(62, GetTryStackIndex() == 0);
}

stock NestedCalls_2() {
	ThrowError("test");
}

stock RecursiveTry(recursion = 0) {
	new stuff = 541245;
	
	if (recursion < 100) {
		try {
			RecursiveTry(recursion + 1);
		} catch (e) {
			catch_ok = 1;
			Assert(63, stuff == 541245);
		} finally {
			finally_ok = 1;
			
			return 1234;
		}
	} else {
		ThrowError("test");
	}
	
	return 222;
}

new pub_used = false;

stock PublicFunctions() {
	finally_ok = 0;
	catch_ok = 0;
	
	try {
		Assert(, CallLocalFunction("PublicFunction", "i", 0) == 666);
		//CallLocalFunction("PublicFunction", "i", 1);
	} catch (e) {
		Assert(64, e[Code] == 54789);
		catch_ok = 1;
	} finally {
		finally_ok = 1;
	}
	
	Assert(65, finally_ok);
	//Assert(66, catch_ok);
	Assert(67, GetTryStackIndex() == -1);
	
	finally_ok = 0;
	catch_ok = 1;
	
	try {
		Assert(68, CallRemoteFunction("PublicFunction", "i", 0) == 666);
	} catch (e) {
		catch_ok = 0;
	} finally {
		finally_ok = 1;
	}
	
	pub_used = true;
	
	Assert(69, finally_ok);
	Assert(70, catch_ok);
	Assert(71, GetTryStackIndex() == -1);
}

forward PublicFunction(throw_error);
public PublicFunction(throw_error) {
	// Avoid other FSes calling this later
	if (pub_used) {
		return 1;
	}
	
	if (throw_error) {
		ThrowError("pub", 54789);
	} else {
		try {
			ThrowError("pub", 631);
		} catch (e) {
			Assert(72, e[Code] == 631);
			return 555;
		} finally {
			return 666;
		}
	}
	
	return 777;
}