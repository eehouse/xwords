/* copied from _Palm Programming_ p. 79*/

#ifndef __CALLBACK__
#define __CALLBACK__

#if defined MW_COMPILER
/* these are no-ops for MW as I understand it */
# define CALLBACK_PROLOGUE()
# define CALLBACK_EPILOGUE()

#elif defined XW_TARGET_PNO || defined XW_TARGET_X86

#define CALLBACK_PROLOGUE()
#define CALLBACK_EPILOGUE()

#else

register void *reg_a4 asm("%a4");

#define CALLBACK_PROLOGUE() \
    { void* __save_a4 = reg_a4; asm("move.l %%a5,%%a4; sub.l #edata,%%a4" : :);

#define CALLBACK_EPILOGUE() reg_a4 = __save_a4;}

#endif /* MW_COMPILER */

#endif
