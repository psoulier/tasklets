#ifndef __TASKLETS_HPP__
#define __TASKLETS_HPP__

#include <stdint.h>

#if !defined(__GNUC__) || !defined(__clang__)
// Whatever is compiling this isn't supported.  I know GCC and clang support "computed" gotos, not
// sure about other compilers.
#:error: unsupported compiler
#endif

/*
 * Align value UP to the nearest "uintptr_t" number of bytes.
 */
#define _TSKL_ALIGN(_X) (((_X) + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1))

/*
 * Defines base class for tasklets.  Contains all state necessary for managing jumps.
 * Everything is public since macros need access to most variables (leading "_" 
 * indicates a "private" variable.
 */
struct Tasklet {
    uint16_t        _stack_size;        // Max stack size (check for stack overflow)
    uint16_t        _sp;                // Stack pointer
    uint16_t        _fresh      :1,     // True if first call to Tasklet "main"
                    _norecurse  :1,     // Used for TASKLET_WAIT
                    _nopopv     :1,     // Used for TASKLET_WAIT
                    _ended      :1;     // True when Tasklet passed TASKLET_END
    void            **_stack;           // Pointer to stack memory

    Tasklet(void *stack, uintptr_t stack_size) {
        _sp = 0;
        _stack = (void**)stack;
        _stack_size = stack_size / sizeof(uintptr_t);
        _fresh = true;
        _nopopv = false;
        _norecurse = false;
    }

    virtual void main() = 0;

    /*
     * True if Tasklet is still active (i.e., has not reached TASKLET_END)
     */
    bool is_running() const {
        return !_ended;
    }

    /*
     * Starts a Tasklet (it's the same as resume at this point).
     */
    void start() {
        main();
    }

    /*
     * Resumes a Tasklet from where it left off.
     */
    void resume() {
        main();
    }
};

/*
 * Provides a mechanism to allocate local variables on the Tasklet stack.
 * Variables declared using this class must be declared _prior_ to the
 * TASKLET_BEGIN() statement.
 *
 * Variables created with this class are accessed with pointer semantics
 * (i.e., the "*" and "->" operators).
 */
template <typename T>
class TaskletVar {
    T           *var;
    Tasklet     *tskl;

    public:
        // Must be created with a Tasklet.
        TaskletVar() = delete;
        TaskletVar(const TaskletVar&) = delete;

        /*
         * Default construct Tasklet-local variable.
         */
        TaskletVar(Tasklet *tskl) : tskl(tskl)  {
            /*
             * Winding stack back up, move SP past this variable.
             */
            push();

            /*
             * Only construct Tasklet-local variable when it is first started.
             * Do this explicitly with placement new.
             */
            if (tskl->_fresh) {
                new(var) T();
            }
        }

        /*
         * Copy-construct the Tasklet-local variable (not the TaskletVar).
         */
        TaskletVar(Tasklet *tskl, const T &v) : tskl(tskl) {
            /*
             * Winding stack back up, move SP to where this should be variable.
             */
            push();

            /*
             * Only construct Tasklet-local variable when it is first started.
             */
            if (tskl->_fresh) {
                new(var) T(v);
            }
        }

        ~TaskletVar() {
            /*
             * Only destruct the variable when the Tasklet has completed.  Even
             * though the lifetime of this proxy has ended, the actual Tasklet
             * variable is still alive.
             */
            if (tskl->_ended) {
                var->~T();
            }

            /*
             * Adjust stack so Tasklet can "rewind" itself when it's resumed.
             * Same as "push" from above, but on the destruct side of things when the 
             * immediate return finishes.  In other words, if "_nopopv" is set, the
             * construction didn't increment SP, so it shouldn't be decremented here.
             */
            if (!tskl->_nopopv) {
                /*
                * Proxy lifetime is ending, adjust SP for safe rewind.
                */
                tskl->_sp -= _TSKL_ALIGN(sizeof(T))/sizeof(uintptr_t);
            }

        }

        T* operator -> () {
            return var;
        }

        T& operator * () {
            return *var;
        }

        private:

            /*
             * Adjusts the stack pointer as necessary when variable constructed.
             * Doesn't actually "push" anything on the stack though.
             */
            void push() {
                var = (T*)&tskl->_stack[tskl->_sp];

                /*
                * Handle case of immediate resume recursion.  In the case where a 
                * TASKLET_WAIT doesn't actually wait, the stack needs to be left alone
                * on the next resume.
                */
                if (!tskl->_norecurse) {
                    tskl->_sp += _TSKL_ALIGN(sizeof(T))/sizeof(uintptr_t);
                    assert(tskl->_sp < tskl->_stack_size);
                }
            }
        
};


/*
 * These macros allow the creation of unique labels based on line numbers
 * and a user-supplied symbol.  For example, if a _TASKLET_LABLE(Beta) exists on
 * line 99, this would expand to "_Beta99".
 */
#define _TASKLET_SYMBOLIZE(P,S)     _##P##S
#define _TASKLET_SYMBOL(P,S)        _TASKLET_SYMBOLIZE(P,S)
#define _TASKLET_LABEL(PREFIX)      _TASKLET_SYMBOL(PREFIX, __LINE__)

/*
 * Used for non-class implementations.  Needs a pointer to the
 * current Tasklet.
 */
#define TASKLET_BEGIN_FUNC(_TASKLET) \
    Tasklet     *__tskl = (_TASKLET); \
    __TASKLET_BEGIN()

/*
 * Used for Tasklet member functions.  The "this" pointer is used for
 * the Tasklet pointer.
 */
#define TASKLET_BEGIN()  \
    Tasklet     *__tskl = this; \
    __TASKLET_BEGIN(); \

/*
 * Notes: 
 *  - "_ended" is always set to false since it could have been set in
 *      a function call.  
 *  - When a Tasklet is first created, we don't jump.
 *  - The jump address is always on the top of the stack.
 */
#define __TASKLET_BEGIN()  \
    __tskl->_ended = false; \
    if (__tskl->_fresh) { __tskl->_fresh = false; } \
    else { goto *__tskl->_stack[__tskl->_sp]; } \

/*
 * Suspends a Tasklet.  When the "resume" method is called, the Tasklet function
 * will continue executing _after_ the yield statement.
 */
#define TASKLET_YIELD() \
    __tskl->_stack[__tskl->_sp] = &&_TASKLET_LABEL(ALPHA); \
    return ; \
    _TASKLET_LABEL(ALPHA):; \

/*
 * Ends a Tasklet function.
 */
#define TASKLET_END() \
    __tskl->_ended = true; \
    __tskl->_fresh = false; \


/*
 * Calls a Tasklet function/method where "FN" is the function name followed by
 * whatever arguments it needs.
 *
 * Notes: 
 *  - "_fresh" is set to true so subroutine knows it shouldn't jump.
 *  - The "_sp" inc/dec is for the jump address.
 */
#define TASKLET_CALL(FN, ...)    \
    __tskl->_stack[__tskl->_sp] = &&_TASKLET_LABEL(ALPHA); \
    __tskl->_fresh = true; \
    _TASKLET_LABEL(ALPHA):; \
    __tskl->_sp += 1; \
    assert(__tskl->_sp < __tskl->_stack_size); \
    __tskl->_ended = true; \
    FN(__VA_ARGS__); \
    __tskl->_fresh = false; \
    __tskl->_sp -= 1; \
    if (!__tskl->_ended) { \
        return ; \
    } \

/*
 * Predfined macro that resolves to a pointer for the current Tasklet.
 */
#define TASKLET_PTR     ((void*)(__tskl))

/*
 * Predefined callback.  This can be used for blocking routines
 * that accept a function pointer and a single argument.  The argument
 * must be a valid pointer to a Tasklet.
 */
static void Tasklet_Resume(void *t) {
    Tasklet     *tskl = (Tasklet*)t;

    tskl->resume();
}

/*
 * Combines a call to an asynchronous operation and a TASKLET_YIELD.  Some
 * functions may return immediately if they do not need to block.  This can
 * cause some ugly recursion.  There's a lot of state setting in this macro
 * to ensure a function that immediately calls the "resume" function doesn't
 * wreak havoc.  In particular, Tasklet-local variables are a special case;
 * the Tasklet stack needs to be unmodifed in a direct return case.
 */
#define TASKLET_WAIT(FN, ...)    \
    __tskl->_stack[__tskl->_sp] = &&_TASKLET_LABEL(ALPHA); \
    __tskl->_norecurse = true; \
    FN(__VA_ARGS__); \
    if (__tskl->_norecurse) { \
        __tskl->_norecurse = false; \
        return ; \
    } \
    __tskl->_nopopv = false; \
    _TASKLET_LABEL(ALPHA): \
    if (__tskl->_norecurse) { \
        __tskl->_nopopv = true; \
        __tskl->_norecurse = false; \
        return ; \
    } \



#endif



