#ifndef _LIT_H_
#define _LIT_H_

struct lit
{
	int clause_num; //clause num, begin with 0
	int var_num;	//variable num, begin with 1
	bool sense;		//is 1 for true literals, 0 for false literals.
};

#endif // _LIT_H_