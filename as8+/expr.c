//
//  expr.c
//  as8+
//
// stuff to do with expressions in the assembler ...
//

//  Created by Harry Reed on 5/11/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>

#include "as.h"

expr *newExpr()
{
    expr *p = (expr*)calloc(1, sizeof(expr));
    p->type = eExprUnknown;
    p->value = -1LL;
    p->lc = ".unk.";
    return p;
}

expr *exprSymbolValue(char *s)
{
    static expr nulExp;
    
    symtab *y = getsym(s);
    
    if (!y)
    {
        nulExp.value = -1LL;
        nulExp.type = eExprUnknown;
        
        if (nPass == 1)
            return &nulExp;
        
        yyprintf("undefined symbol <%s>", s);
        return &nulExp;
    }
    expr *res = y->Value;
    return res;
}

expr *exprWord36Value(word36 i36)
{
    expr *res = newExpr();
    
    res->value = i36;
    res->type = eExprAbsolute;
    res->lc = ".abs.";
    
    return res;
}

expr *exprLiteral(literal *l)
{
    expr *res = newExpr();
    
    res->value = l->addr;
    res->type = eExprRelative;
    res->lc = ".text.";
    
    return res;    
}

expr *exprPtrExpr(int ptr_reg, expr *e)
{
    expr *res = newExpr();
    
    res->value = ((ptr_reg << 15) | (e->value & 077777)) & AMASK;
    
    res->type = eExprRelative;  // XXX should we have a PR relative type?
    res->lc = ".text.";
    res->bit29 = true;  // this is a PR operation. Set bit-29 if needed
    
    return res;    
}

/*
 * the values of symbols and expressions may be wither absolute or relative to some location counter (lc).
 * The operands of the arothmetic operators are restricted to the combinations in the following list:
 * 
 * operand 1        operator        operand 2       = result
 *
 * absolute             +           absolute        = absolute
 * relative to lc       +           absolute        = relative to lc
 * absolute             +           relative to lc  = relative to lc
 * absolute             -           absolute        = absolute
 * relative to lc       -           absolute        = relative to lc
 * relative to lc       -           relative to lc  = relative to lc
 * absolute             *           absolute        = absolute
 * absolute             /           absolute        = absolute
 * absolute             %           absolute        = absolute
 * -none-           (unary)-        absolute        = absolute
 * -none-           (unary)^        absolute        = absolute
 */

expr *add(expr *op1, expr *op2)
{
    /*
     * absolute             +           absolute        = absolute
     * relative to lc       +           absolute        = relative to lc
     * absolute             +           relative to lc  = relative to lc
     */
    expr *res = newExpr();
    
    res->value = (op1->value + op2->value) & DMASK;   // keep to 36-bits
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    }
    else if (op1->type == eExprRelative && op2->type == eExprAbsolute)
    {
        res->type = eExprRelative;
        res->lc = op1->lc;
    }
    else if (op1->type == eExprAbsolute && op2->type == eExprRelative)
    {
        res->type = eExprRelative;
        res->lc = op2->lc;
    }
    else
    {
        if (nPass == 2)
            yyerror("type error for addition");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }
    return res;
}

expr *subtract(expr *op1, expr *op2)
{
    /*
     * absolute             -           absolute        = absolute
     * relative to lc       -           absolute        = relative to lc
     * relative to lc       -           relative to lc  = relative to lc
     */
    expr *res = newExpr();

    res->value = (op1->value - op2->value) & DMASK;   // keep to 36-bits
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    }
    else if (op1->type == eExprRelative && op2->type == eExprAbsolute)
    {
        res->type = eExprRelative;
        res->lc = op1->lc;
    }
    else if (op1->type == eExprRelative && op2->type == eExprRelative)
    {
        if (nPass == 2 && strcasecmp(op1->lc, op2->lc))
            yyprintf("relocation error for subtraction (%s <> %s)", op1->lc, op2->lc);
        res->type = eExprAbsolute;
        res->lc = op2->lc;
    }
    else
    {
        if (nPass == 2)
            yyerror("type error for subtraction");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }

    return res;
}
expr *multiply(expr *op1, expr *op2)
{
    /*
     * absolute             *           absolute        = absolute
     */
    
    expr *res = newExpr();

    res->value = (op1->value * op2->value) & DMASK;   // keep to 36-bits
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for multiplication");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }


    return res;
}
expr *divide(expr *op1, expr *op2)
{
    /*
     * absolute             /           absolute        = absolute
     */
    expr *res = newExpr();

    if (op2->value == 0)
    {
        yyerror("division by 0");
        res->value = op1->value;
    } else
        res->value = (op1->value / op2->value) & DMASK;   // keep to 36-bits
    
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for division");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }

    return res;
}
expr *modulus(expr *op1, expr *op2)
{
    /*
     * absolute             %           absolute        = absolute
     */
    expr *res = newExpr();

    if (op2->value == 0)
    {
        yyerror("modulus by 0");
        res->value = op1->value;
    } else
        res->value = (op1->value / op2->value) & DMASK;   // keep to 36-bits
    
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for modulus");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }
    
    return res;
}

expr *and(expr *op1, expr *op2)
{
    expr *res = newExpr();

    /*
     * absolute             &           absolute        = absolute
     */
    res->value = (op1->value & op2->value) & DMASK;   // keep to 36-bits
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for and");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }

    return res;
}
expr *or(expr *op1, expr *op2)
{
    expr *res = newExpr();

    /*
     * absolute             |           absolute        = absolute
     */
    res->value = (op1->value | op2->value) & DMASK;   // keep to 36-bits
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for or");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }

    return res;
}
expr *xor(expr *op1, expr *op2)
{
    expr *res = newExpr();

    /*
     * absolute             ^           absolute        = absolute
     */
    res->value = (op1->value ^ op2->value) & DMASK;   // keep to 36-bits
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for xor");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }

    return res;
}
expr *andnot(expr *op1, expr *op2)
{
    expr *res = newExpr();
    
    /*
     * absolute           (bool) /           absolute        = absolute
     */
    res->value = op1->value & ~op2->value;
    if (op1->type == eExprAbsolute && op2->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for bool divide (andnot)");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }
    
    return res;
}

expr *not(expr *op1)
{
    expr *res = newExpr();

    /*
     * -none-           (unary)~        absolute        = absolute
     */
    res->value = ~op1->value;
    if (op1->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for unary not");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }

    return res;
}
expr *neg(expr *op1)
{
    expr *res = newExpr();

    /*
     * -none-           (unary)-        absolute        = absolute
     */
    res->value = -op1->value;
    if (op1->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for negate");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }

    return res;
}
expr *neg8(expr *op1)
{
    expr *res = newExpr();

    /*
     * -none-           (unary)-        absolute        = absolute
     */
    res->value = op1->value ^ 0400000000000LL; /* flip the sign bit */
    if (op1->type == eExprAbsolute)
    {
        res->type = eExprAbsolute;
        res->lc = op1->lc;
    } else
    {
        if (nPass == 2)
            yyerror("type error for bool negate");
        res->type = eExprUnknown;
        res->lc = op1->lc;
    }
    
    return res;
}