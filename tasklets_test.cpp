#include <stdio.h>
#include "gtest/gtest.h"
#include "tasklets.hpp"



class SimpleTasklet : public Tasklet {
    public:
        uintptr_t   stack[16];
        int         i;

        SimpleTasklet() : Tasklet(stack, sizeof(stack)), i(0) {
        }

        virtual void main();
};

void add_8(SimpleTasklet *tskl) {
    TaskletVar<int>     j(tskl, 0);

    TASKLET_BEGIN_FUNC(tskl);

    for (*j = 0; *j < 8; *j += 1) {
        tskl->i += 1;
        TASKLET_YIELD();
    }

    TASKLET_END();
}

void SimpleTasklet::main() {
    TASKLET_BEGIN();

    TASKLET_YIELD();
    i += 1;

    TASKLET_YIELD();
    i += 2;

    TASKLET_YIELD();
    i += 4;

    TASKLET_CALL(add_8, this);

    TASKLET_END();            
}

TEST(TaskletTest, Simple) {
    SimpleTasklet t;

    t.start();

    while (t.is_running()) {
        t.resume();
    }

    ASSERT_EQ(15, t.i);
}

void immediate_resume(Tasklet *tskl) {
    tskl->resume();
}

class WaitTasklet : public SimpleTasklet {
    virtual void main() {
        TaskletVar<int>     j(this, 0);

        TASKLET_BEGIN();

        for (i = 0; i < 10; i += 1) {
            *j = i;
            TASKLET_WAIT(immediate_resume, this);
            ASSERT_EQ(i, *j);
        }

        TASKLET_END();
    }
};

TEST(TaskletTest, Wait) {
    WaitTasklet wt;

    wt.start();
    while (wt.is_running()) {
        wt.resume();
    }
}

