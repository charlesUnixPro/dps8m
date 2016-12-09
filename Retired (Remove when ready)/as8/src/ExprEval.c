/*
 Copyright 2012-2013 by Harry Reed

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later. 
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file ExprEval
 * \project as8
 * \author Stolen by Harry Reed on 9/25/12.  (from the internet somewhere ....)
 * \date 9/25/12
 * Exprc
 *  A simple recursive-descent evaluator.
*/

//#include <stdio.h>
//
//#include <assert.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//#include <stdbool.h>

#include "as.h"

#define VALIDCHARS    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_$."

extern word18 addr;
extern int32 lineno;

/// ================================
///   Simple expression evaluator
/// ================================

static const char *errors[] = {
    "No error",
    "Unmatched Parenthesis",
    "Invalid Character",
    "Division by Zero"
};

/// Error codes
enum EXPR_EVAL_ERR {
	EEE_NO_ERROR = 0,
	EEE_PARENTHESIS = 1,
	EEE_WRONG_CHAR = 2,
	EEE_DIVIDE_BY_ZERO = 3,
};

//typedef char EVAL_CHAR;

/**
 * Algebraic expression evaluator .....
 */
word36 ParseSummands();

enum EXPR_EVAL_ERR _err;
char* _err_pos;
int _paren_count;
char *expr;

	/// Parse a number or an expression in parenthesis
	///double ParseAtom(EVAL_CHAR*  expr) {
word36 ParseAtom()
{
		// Skip spaces
		while(*expr == ' ')
			expr++;
        
		/// Handle the sign before parenthesis (or before number)
		bool negative = false;
		if(*expr == '-') {
			negative = true;
			expr++;
		}
		if(*expr == '+') {
			expr++;
		}
    
        bool not = false;
        if (*expr == '~')
        {
            not = true;
            expr++;
        }
		// Check if there is parenthesis
		if(*expr == '(') {
			expr++;
			_paren_count++;
			word36 res = ParseSummands();
			if(*expr != ')') {
				// Unmatched opening parenthesis
				_err = EEE_PARENTHESIS;
				_err_pos = expr;
				return 0;
			}
			expr++;
			_paren_count--;
            if (negative)
                return -res;
            else if (not)
                return ~res;
            else
                return res;
			//return negative ? -res : res;
		}
        
		/// It should be a number; convert it to 64-bit int
		char* end_ptr;
		word36 res = strtoll(expr, &end_ptr, 0); ///< allows for octal, decimal and hex
        
		if (end_ptr == expr) {
            // This ain't a number. may be a symbol. Try to resole it
            
            if (expr[0] == '*')
            {
                // a ref to PC/IC
                res = addr;
                end_ptr++;
            }
            else
            {
                char buff[256];
                size_t r = strspn(expr, VALIDCHARS);    ///< "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_$.");
                memset(buff, 0, sizeof(buff));
                strncpy(buff, expr, r);
                
                symtab *s = getsym(buff);
                if (s != NULL) {
                    res = s->value;
                    end_ptr = expr + strlen(buff)   ;//expr);
                } else {
                    //// Report error
                    //_err = EEE_WRONG_CHAR;
                    //_err_pos = expr;
                    //return 0;
                    fprintf(stderr, "Eval(undefined symbol <%s> in expression @ line %d in '%s')\n", buff, lineno, LEXCurrentFilename());  //expr);
                    res = 0;
                    end_ptr = expr + strlen(expr);
                }
            }
		}
		// Advance the pointer and return the result
		expr = end_ptr;
		//return negative ? -res : res;
        if (negative)
            return -res;
        else if (not)
            return ~res;
        else
            return res;
	}

/// Parse multiplication and division
word36 ParseFactors() {
    word36 num1 = ParseAtom();
    for(;;) {
        // Skip spaces
        while(*expr == ' ')
            expr++;
        /// Save the operation and position
        char op = *expr;
        char* pos = expr;
        if(op != '/' && op != '*' && op != '%')
            return num1;
        expr++;
        word36 num2 = ParseAtom();
        // Perform the saved operation
        if(op == '/' || op == '%') {
            // Handle division/modulus by zero
            if(num2 == 0) {
                _err = EEE_DIVIDE_BY_ZERO;
                _err_pos = pos;
                return 0;
            }
            if (op == '/')
                num1 /= num2;
            else
                num1 %= num2;
        }
        else
            num1 *= num2;
    }
}

/// Parse addition and subtraction
word36 ParseSummands() {
    word36 num1 = ParseFactors();
    for(;;) {
        // Skip spaces
        while(*expr == ' ')
            expr++;
        char op = *expr;
        if(op != '-' && op != '+')
            return num1;
        expr++;
        word36 num2 = ParseFactors();
        if(op == '-')
            num1 -= num2;   // test for 18-bit underflow
        else
            num1 += num2;   // test for 18-bit overflow
    }
}

word36 Eval(char* Expr) {
    expr = Expr;
    
    _paren_count = 0;
    _err = EEE_NO_ERROR;
    word36 res = ParseSummands();
    // Now, expr should point to '\0', and _paren_count should be zero
    if(_paren_count != 0 || *expr == ')') {
        _err = EEE_PARENTHESIS;
        _err_pos = expr;
        return 0;
    }
    
    if(*expr != '\0') {
        _err = EEE_WRONG_CHAR;
        _err_pos = expr;
        return 0;
    }
    return res;
};
enum EXPR_EVAL_ERR GetErr() {
    return _err;
}
char* GetErrPos() {
    return _err_pos;
}

/**
 * Boolean expression evaluator .....
 */

word36 parseBoolSummands();

/// Parse a number or an expression in parenthesis
//double ParseAtom(EVAL_CHAR*  expr) {
word36 parseBoolAtom()
{
    // Skip spaces
    while(isspace(*expr))
        expr++;
    
    /// Handle the sign before parenthesis (or before number)
    bool not = false;
    if (*expr == '/' || *expr == '^')   // in alm ^ is not
    {
        not = true;
        expr++;
    }

    // Check if there is parenthesis
    if (*expr == '(')
    {
        expr++;
        _paren_count++;
        word36 res = parseBoolSummands();
        if(*expr != ')') {
            // Unmatched opening parenthesis
            _err = EEE_PARENTHESIS;
            _err_pos = expr;
            return 0;
        }
        expr++;
        _paren_count--;
        return not ? ~res : res;
    }
    
    /// It should be a number; convert it to 64-bit int
    char* end_ptr;
    word36 res = strtoll(expr, &end_ptr, 8); ///< boolean expressions always have octal constants
    
    if (end_ptr == expr) {
        // This ain't a number. may be a symbol. Try to resole it
        
        if (expr[0] == '*')
        {
            // a ref to PC/IC
            res = addr;
            end_ptr++;
        }
        else
        {
            char buff[256];
            size_t r = strspn(expr, VALIDCHARS);    ///< "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_$.");
            memset(buff, 0, sizeof(buff));
            strncpy(buff, expr, r);
            
            symtab *s = getsym(buff);
            if (s != NULL) {
                res = s->value;
                end_ptr = expr + strlen(buff)   ;//expr);
            } else {
                //// Report error
                //_err = EEE_WRONG_CHAR;
                //_err_pos = expr;
                //return 0;
                fprintf(stderr, "EvalBool(undefined symbol <%s> in expression @ line %d in '%s')\n", buff, lineno, LEXCurrentFilename());  //expr);
                res = 0;
                end_ptr = expr + strlen(expr);
            }
        }
    }
    // Advance the pointer and return the result
    expr = end_ptr;
    return not ? ~res : res;
}

/// Parse and & .......
word36 parseBoolFactors()
{
    word36 num1 = parseBoolAtom();
    for(;;) {
        // Skip spaces
        while(*expr == ' ')
            expr++;
        /// Save the operation and position
        char op = *expr;
        char* pos = expr;
        if(op != '/' && op != '*') // alm doesn't have the / operator, but GE/Honeywell/Bull do
            return num1;
        expr++;
        word36 num2 = parseBoolAtom();
        // Perform the saved operation
        switch (op)
        {
            case '/':
                num1 = num1 & ~num2;
                break;
            case '*':
                num1 &= num2;
                break;
        }
    }
}

/// Parse or and xor
word36 parseBoolSummands() {
    word36 num1 = parseBoolFactors();
    for(;;)
    {
        // Skip spaces
        while(*expr == ' ')
            expr++;
        char op = *expr;
        if (op != '-' && op != '+')
            return num1;
        expr++;
        word36 num2 = parseBoolFactors();
        switch (op)
        {
            case '-':           ///< - is XOR
                num1 ^= num2;
                break;
            case '+':           ///< ++ is OR
                num1 |= num2;
                break;
        }
    }
}

word36 boolEval(char* Expr) {
    expr = Expr;
    
    _paren_count = 0;
    _err = EEE_NO_ERROR;
    word36 res = parseBoolSummands();
    // Now, expr should point to '\0', and _paren_count should be zero
    if(_paren_count != 0 || *expr == ')')
    {
        _err = EEE_PARENTHESIS;
        _err_pos = expr;
        return 0;
    }
    
    if(*expr != '\0') {
        _err = EEE_WRONG_CHAR;
        _err_pos = expr;
        return 0;
    }
    return res;
};




// =======
//  Tests
// =======

//#ifdef _DEBUG
void TestExprEval() {
	/// Some simple expressions
	assert(Eval("1234") == 1234 && GetErr() == EEE_NO_ERROR);
    assert(Eval("0x123456789abcdef") == 0x123456789abcdef && GetErr() == EEE_NO_ERROR);
    assert(Eval("01234") == 01234 && GetErr() == EEE_NO_ERROR);
    assert(Eval("~01234") == ~01234 && GetErr() == EEE_NO_ERROR);
    
    //printf("%f %d\n", Eval("1234"), GetErr());
    //return;
    
	assert(Eval("1+2*3") == 7 && GetErr() == EEE_NO_ERROR);    
    assert(Eval("98+76%54") == 98+76%54 && GetErr() == EEE_NO_ERROR);
    
	/// Parenthesis
	assert(Eval("5*(4+4+1)") == 45 && GetErr() == EEE_NO_ERROR);
	assert(Eval("5*(2*(1+3)+1)") == 45 && GetErr() == EEE_NO_ERROR);
	assert(Eval("5*((1+3)*2+1)") == 45 && GetErr() == EEE_NO_ERROR);
    
	/// Spaces
	assert(Eval("5 * ((1 + 3) * 2 + 1)") == 45 && GetErr() == EEE_NO_ERROR);
	assert(Eval("5 - 2 * ( 3 )") == -1 && GetErr() == EEE_NO_ERROR);
	assert(Eval("5 - 2 * ( ( 4 )  - 1 )") == -1 && GetErr() == EEE_NO_ERROR);
    
	/// Sign before parenthesis
	assert(Eval("-(2+1)*4") == -12 && GetErr() == EEE_NO_ERROR);
	assert(Eval("-4*(2+1)") == -12 && GetErr() == EEE_NO_ERROR);
	
	/// Fractional numbers
	//assert(Eval("1.5/5") == 0.3 && GetErr() == EEE_NO_ERROR);
	//assert(Eval("1/5e10") == 2e-11 && GetErr() == EEE_NO_ERROR);
	//assert(Eval("(4-3)/(4*4)") == 0.0625 && GetErr() == EEE_NO_ERROR);
	//assert(Eval("1/2/2") == 0.25 && GetErr() == EEE_NO_ERROR);
	//assert(Eval("0.25 * .5 * 0.5") == 0.0625 && GetErr() == EEE_NO_ERROR);
	//assert(Eval(".25 / 2 * .5") == 0.0625 && GetErr() == EEE_NO_ERROR);
	
	/// Repeated operators
	assert(Eval("1+-2") == -1 && GetErr() == EEE_NO_ERROR);
	assert(Eval("--2") == 2 && GetErr() == EEE_NO_ERROR);
	assert(Eval("2---2") == 0 && GetErr() == EEE_NO_ERROR);
	assert(Eval("2-+-2") == 4 && GetErr() == EEE_NO_ERROR);
    
	/// === Errors ===
	/// Parenthesis error
	Eval("5*((1+3)*2+1");
	assert(GetErr() == EEE_PARENTHESIS && strcmp(GetErrPos(), "") == 0);
	Eval("5*((1+3)*2)+1)");
	assert(GetErr() == EEE_PARENTHESIS && strcmp(GetErrPos(), ")") == 0);
	
	/// Repeated operators (wrong)
	Eval("5*/2");
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "/2") == 0);
	
	/// Wrong position of an operator
	Eval("*2");
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "*2") == 0);
	Eval("2+");
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "") == 0);
	Eval("2*");
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "") == 0);
	
	/// Division by zero
	Eval("2/0");
	assert(GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(GetErrPos(), "/0") == 0);
	Eval("3+1/(5-5)+4");
	assert(GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(GetErrPos(), "/(5-5)+4") == 0);
	Eval("2/"); /// Erroneously detected as division by zero, but that's ok for us
	assert(GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(GetErrPos(), "/") == 0);
	
	/// Invalid characters
	//Eval("~5");
	//assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "~5") == 0);
	Eval("5x");
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "x") == 0);
    
	/// Multiply errors
	Eval("3+1/0+4$"); /// Only one error will be detected (in this case, the last one)
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "$") == 0);
	Eval("3+1/0+4");
	assert(GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(GetErrPos(), "/0+4") == 0);
	Eval("q+1/0)"); /// ...or the first one
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "q+1/0)") == 0);
	Eval("+1/0)");
	assert(GetErr() == EEE_PARENTHESIS && strcmp(GetErrPos(), ")") == 0);
	Eval("+1/0");
	assert(GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(GetErrPos(), "/0") == 0);
	
	/// An emtpy string
	Eval("");
	assert(GetErr() == EEE_WRONG_CHAR && strcmp(GetErrPos(), "") == 0);
}
//#endif

/// ============
/// Main program
/// ============

int main2() {
#ifdef _DEBUG
	TestExprEval();
#endif
	    
	puts("Enter an expression (or an empty string to exit):");
	for(;;) {
		/// Get a string from console
		static char buff[256];
		char *expr = fgets(buff, sizeof(buff), stdin);
        
		// If the string is empty, then exit
		if(*expr == '\0')
			return 0;
        
		/// Evaluate the expression
		//ExprEval eval;
		double res = Eval(expr);
		if(GetErr() != EEE_NO_ERROR) {
			printf("  Error: %s at %s\n", errors[GetErr()], GetErrPos());
		} else {
			printf("  = %g\n", res);
		}
	}
}
