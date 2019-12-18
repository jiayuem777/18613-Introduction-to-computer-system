/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  long Funct(long arg1, long arg2, ...) {
      /* brief description of how your implementation works */
      long var1 = Expr1;
      ...
      long varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. (Long) integer constants 0 through 255 (0xFFL), inclusive. You are
      not allowed to use big constants such as 0xffffffffL.
  3. Function arguments and local variables (no global variables).
  4. Local variables of type int and long
  5. Unary integer operations ! ~
     - Their arguments can have types int or long
     - Note that ! always returns int, even if the argument is long
  6. Binary integer operations & ^ | + << >>
     - Their arguments can have types int or long
  7. Casting from int to long and from long to int
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting other than between int and long.
  7. Use any data type other than int or long.  This implies that you
     cannot use arrays, structs, or unions.
 
  You may assume that your machine:
  1. Uses 2s complement representations for int and long.
  2. Data type int is 32 bits, long is 64.
  3. Performs right shifts arithmetically.
  4. Has unpredictable behavior when shifting if the shift amount
     is less than 0 or greater than 31 (int) or 63 (long)

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 63
   */
  long pow2plus1(long x) {
     /* exploit ability of shifts to compute powers of 2 */
     /* Note that the 'L' indicates a long constant */
     return (1L << x) + 1L;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 63
   */
  long pow2plus4(long x) {
     /* exploit ability of shifts to compute powers of 2 */
     long result = (1L << x);
     result += 4L;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implement floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants. You can use any arithmetic,
logical, or comparison operations on int or unsigned data.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operations (integer, logical,
     or comparison) that you are allowed to use for your implementation
     of the function.  The max operator count is checked by dlc.
     Note that assignment ('=') is not counted; you may use as many of
     these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* Copyright (C) 1991-2012 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */
/* This header is separate from features.h so that the compiler can
   include it implicitly at the start of every compilation.  It must
   not itself include <features.h> or any other header that includes
   <features.h> because the implicit include comes before any feature
   test macros that may be defined in a source file before it first
   explicitly includes a system header.  GCC knows the name of this
   header in order to preinclude it.  */
/* We do support the IEC 559 math functionality, real and complex.  */
/* wchar_t uses ISO/IEC 10646 (2nd ed., published 2011-03-15) /
   Unicode 6.0.  */
/* We do not support C11 <threads.h>.  */
//1
/*
 * bitMatch - Create mask indicating which bits in x match those in y
 *            using only ~ and &
 *   Example: bitMatch(0x7L, 0xEL) = 0xFFFFFFFFFFFFFFF6L
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
long bitMatch(long x, long y) {
  long result = ~(x & ~y) & ~(y & ~x);
  return result;
}
//2
/* 
 * copyLSB - set all bits of result to least significant bit of x
 *   Examples:
 *     copyLSB(5L) = 0xFFFFFFFFFFFFFFFFL,
 *     copyLSB(6L) = 0x0000000000000000L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
long copyLSB(long x) {
  long a = 0L;
  long b = ~a;
  long result = b + (x & 1L);
  return ~result;
}
/*
 * distinctNegation - returns 1 if x != -x.
 *     and 0 otherwise 
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 5
 *   Rating: 2
 */
long distinctNegation(long x) {
  long a = x ^ (~x + 1);
  return !!a;
}
/* 
 * leastBitPos -  
 *               least significant 1 bit. If x == 0, return 0
 *   Example: leastBitPos(96L) = 0x20L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2 
 */
long leastBitPos(long x) {
  return x & (~x + 1);
}
/* 
 * dividePower2 - Compute x/(2^n), for 0 <= n <= 62
 *  Round toward zero
 *   Examples: dividePower2(15L,1L) = 7L, dividePower2(-33L,4L) = -2L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
long dividePower2(long x, long n) {
  long a = (1L << 63) & x;
  long b = a >> 63;
  long c = a >> (63 + (~n + 1));
  long d = ~c;
  long e = b & d;
  x = x + e;
  return x >> n;
}
//3
/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4L,5L) = 4L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
long conditional(long x, long y, long z) {
  long val = (long) !x;
  val = (val << 63) >> 63;
  return (val & z) | (~val & y);
}
/* 
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4L,5L) = 1L.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
long isLessOrEqual(long x, long y) {
  long mask = 1L << 63;
  long yMinusX = y + (~x + 1);
  int isSameSign = !((mask & x) ^ (mask & y));
  int result1 = !(mask & yMinusX);
  int result2 = !(!(mask & x) | (mask & y));
  return (long) (isSameSign & result1) | (~isSameSign & result2);
}
//4
/*
 * hexAllLetters - return 1 if the hex representation of x
 *   contains only characters 'a' through 'f'
 *   Example: hexAllLetters(0xabcdefabcdefabcdL) = 1L.
 *            hexAllLetters(0x4031323536373839L) = 0L.
 *            hexAllLetters(0x00AAABBBCCCDDDEEL) = 0L.
 *   Legal ops: ! ~ & ^ | << >>
 *   Max ops: 30
 *   Rating: 4
 */
long hexAllLetters(long x) {
  long mask1 = (0x88L << 8) | 0x88L;
  long mask2 = (0x22L << 8) | 0x22L;
  long mask3 = (0x44L << 8) | 0x44L;
  long s1, s2, s3;
  int result1, result2;
  mask1 = (mask1 << 16) | mask1;
  mask1 = (mask1 << 32) | mask1;
  mask2 = (mask2 << 16) | mask2;
  mask2 = (mask2 << 32) | mask2;
  mask3 = (mask3 << 16) | mask3;
  mask3 = (mask3 << 32) | mask3;
  s1 = x & mask1;
  s2 = x & mask2;
  s3 = (x & mask3) >> 1;
  s2 = s2 | s3;
  result1 = !(s1 ^ mask1);
  result2 = !(s2 ^ mask2);
  return (long)(result1 & result2);

}
/*
 * trueThreeFourths - multiplies by 3/4 rounding toward 0,
 *   avoiding errors due to overflow
 *   Examples:
 *    trueThreeFourths(11L) = 8
 *    trueThreeFourths(-9L) = -6
 *    trueThreeFourths(4611686018427387904L) = 3458764513820540928L (no overflow)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 4
 */
long trueThreeFourths(long x)
{
	long mask, sign, result, remain;
	mask = 3L;
	sign = x >> 63;
	result = (x >> 2) + (x >> 1);
	remain = x & mask;
	return result + (long)!!((remain & sign) ^ 0L) + (long)!((remain) ^ mask) ;


  /*long sign, result1, remain, result;
  int bias;
  long mask = ~0L + (1L << 63);
  sign = x >> 63;
  x = (sign & (~x + 1)) | (~sign & x);
  result1 = ((x >> 2) & (mask >> 1)) + ((x >> 1) & mask);
  remain = x & 3L;
  bias = !(remain ^ 3L);
  result = result1 + (long)bias; 
  return (sign & (~result + 1)) | (~sign & result);*/
  
}
/* 
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5L) = 2, bitCount(7L) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 50
 *   Rating: 4
 */
long bitCount(long x) {
  long mask = (1L << 8) | 1L;
  long s, result;
  mask = (mask << 16) | mask;
  mask = (mask << 32) | mask;
  s = x & mask;
  s += (x >> 1) & mask;
  s += (x >> 2) & mask;
  s += (x >> 3) & mask;
  s += (x >> 4) & mask;
  s += (x >> 5) & mask;
  s += (x >> 6) & mask;
  s += (x >> 7) & mask;
  result = (s & 0xFFL) + ((s >> 8) & 0xFFL) + ((s >> 16) & 0xFFL) + ((s >> 24) & 0xFFL)
  + ((s >> 32) & 0xFFL) + ((s >> 40) & 0xFFL) + ((s >> 48) & 0xFFL) + ((s >> 56) & 0xFFL);
  return result;
}
/*
 * isPalindrome - Return 1 if bit pattern in x is equal to its mirror image
 *   Example: isPalindrome(0x0F0F0F0123c480F0F0F0F0L) = 1L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 70
 *   Rating: 4
 */
long isPalindrome(long x) {
  long mask1 = (0xFFL << 24) + (0xFFL << 16) + (0xFFL << 8) + 0xFFL;
  long x1 = x & mask1;
  long x2 = (x >> 32) & mask1;
  long mask3 = (0xFFL << 16) + 0xFFL;
  long mask2 = (0xFFL << 8) + 0xFFL;
  long mask4 = (0x0FL << 24) + (0x0FL << 16) + (0x0FL << 8) + 0x0FL;
  long mask5 = (0x33L << 24) + (0x33L << 16) + (0x33L << 8) + 0x33L;
  long mask6 = (0x55L << 24) + (0x55L << 16) + (0x55L << 8) + 0x55L;
  x2 = ((x2 & mask2) << 16) + ((x2 & ~mask2) >> 16);
  x2 = ((x2 & mask3) << 8) + ((x2 & ~mask3) >> 8);
  x2 = ((x2 & mask4) << 4) + ((x2 & ~mask4) >> 4);
  x2 = ((x2 & mask5) << 2) + ((x2 & ~mask5) >> 2);
  x2 = ((x2 & mask6) << 1) + ((x2 & ~mask6) >> 1);
  return !(x1 ^ x2);
}
//float
/* 
 * floatIsEqual - Compute f == g for floating point arguments f and g.
 *   Both the arguments are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   If either argument is NaN, return 0.
 *   +0 and -0 are considered equal.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 25
 *   Rating: 2
 */
int floatIsEqual(unsigned uf, unsigned ug) {
  unsigned maskFrac = (1 << 23) - 1;
  unsigned fracF = uf & maskFrac;
  unsigned fracG = ug & maskFrac;
  unsigned maskExp = (1 << 8) - 1;
  unsigned expF = (uf >> 23) & maskExp;
  unsigned expG = (ug >> 23) & maskExp;
  if(fracF == 0 && fracG == 0 && expF == 0 && expG == 0) {
    return 1;
  }
  if((fracF != 0 && expF == 0xFF) ||(fracG != 0 && expG == 0xFF)) {
    return 0;
  }
  return uf == ug;

  /*unsigned sign = 1 << 31;
  unsigned mask = ~sign;
  unsigned f = mask & uf, g = mask & ug;
  //To deal with uf == ug == 0
  if (f == 0 && g == 0) {
    return 1;
  }
  //To deal with NaN number
  unsigned maskFrac = mask >> 8;
  unsigned expF = f >> 23;
  unsigned expG = g >> 23;
  if ((maskFrac & f) != 0 && expF == 0xFF || (maskFrac & g) != 0 && expG == 0xFF) {
    return 0;
  }
  return uf == ug;
  */
}
/* 
 * floatScale4 - Return bit-level equivalent of expression 4*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatScale4(unsigned uf) {
  unsigned sign = (uf >> 31) & 1;
  unsigned maskFrac = (1 << 23) - 1;
  unsigned frac = uf & maskFrac;
  unsigned exp = (uf >> 23) & 0xFF;
  int cont = 2;
  if(exp == 0xFF) {
    return uf;
  }
  if(exp != 0) {
    while (cont > 0){
      cont--;
      exp++;
      if(exp == 0xFF) {
        return (sign << 31) + (0xFF << 23);
      }
    }
  }else {
    if((frac >> 21) == 0) {
      frac = frac << 2;
    }else {
      if ((frac >> 22) == 0){
        exp += 1;
        frac = frac << 2;
      }else {
        exp += 2;
        frac = frac << 1;
      }
    }
  }
  return (sign << 31) | (exp << 23) | (frac & maskFrac);
}
/* 
 * floatUnsigned2Float - Return bit-level equivalent of expression (float) u
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatUnsigned2Float(unsigned u) {
  unsigned mask = 1 << 31;
  int cont = 0;
  unsigned num, frac, remain, remainHead, bias = 127, exp, num2;
  while((u & mask) == 0) {
    mask = mask >> 1;
    cont += 1;
    if(cont == 32) {
    	return 0;
  	}
  }
  num = 31 - cont;
  num2 = num - 23;
  frac = u - (1 << num);
  if(num > 23){
    //round to even
    remain = frac & (1 << num2) - 1; 
    remainHead = (remain >> (num2 - 1)) & 1;
    frac = frac >> num2;
    if (remainHead){
    	if((remain > (1 << (num2 - 1))) || frac & 1){
    		frac += 1;
    	}
    }
  }else {
    frac = frac << (23 - num);
  }
  exp = bias + num;
  return (exp << 23) + frac;
}














