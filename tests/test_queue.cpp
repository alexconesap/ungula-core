#include <gtest/gtest.h>
#include <ungula/core/util/queue.h>

using ungula::core::util::Queue;

TEST(Queue, EmptyOnCreation) {
    Queue<int, 5> q;
    EXPECT_TRUE(q.isEmpty());
    EXPECT_FALSE(q.isFull());
    EXPECT_EQ(q.count(), 0u);
    EXPECT_EQ(q.capacity(), 5u);
}

TEST(Queue, PushAndPop) {
    Queue<int, 5> q;
    EXPECT_TRUE(q.push(42));
    EXPECT_EQ(q.count(), 1u);

    int val = 0;
    EXPECT_TRUE(q.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(q.isEmpty());
}

TEST(Queue, PeekDoesNotRemove) {
    Queue<int, 5> q;
    q.push(99);

    int val = 0;
    EXPECT_TRUE(q.peek(val));
    EXPECT_EQ(val, 99);
    EXPECT_EQ(q.count(), 1u);
}

TEST(Queue, PeekOnEmptyReturnsFalse) {
    Queue<int, 5> q;
    int val = 0;
    EXPECT_FALSE(q.peek(val));
}

TEST(Queue, PopOnEmptyReturnsFalse) {
    Queue<int, 5> q;
    int val = 0;
    EXPECT_FALSE(q.pop(val));
}

TEST(Queue, FullQueueRejectsPush) {
    Queue<int, 3> q;
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));
    EXPECT_TRUE(q.isFull());
    EXPECT_FALSE(q.push(4));
}

TEST(Queue, FifoOrder) {
    Queue<int, 5> q;
    q.push(10);
    q.push(20);
    q.push(30);

    int val;
    q.pop(val);
    EXPECT_EQ(val, 10);
    q.pop(val);
    EXPECT_EQ(val, 20);
    q.pop(val);
    EXPECT_EQ(val, 30);
}

TEST(Queue, CircularWrapAround) {
    Queue<int, 3> q;

    // Fill and drain twice to exercise wrap
    for (int round = 0; round < 2; round++) {
        q.push(1);
        q.push(2);
        q.push(3);
        EXPECT_TRUE(q.isFull());

        int val;
        q.pop(val);
        EXPECT_EQ(val, 1);
        q.pop(val);
        EXPECT_EQ(val, 2);
        q.pop(val);
        EXPECT_EQ(val, 3);
        EXPECT_TRUE(q.isEmpty());
    }
}

TEST(Queue, ClearResetsQueue) {
    Queue<int, 5> q;
    q.push(1);
    q.push(2);
    q.clear();
    EXPECT_TRUE(q.isEmpty());
    EXPECT_EQ(q.count(), 0u);
}

TEST(Queue, MoveSemantics) {
    Queue<std::string, 3> q;
    std::string s = "hello";
    q.push(std::move(s));
    EXPECT_EQ(q.count(), 1u);

    std::string out;
    q.pop(out);
    EXPECT_EQ(out, "hello");
}

TEST(Queue, CapacityOne) {
    Queue<int, 1> q;
    EXPECT_TRUE(q.push(42));
    EXPECT_TRUE(q.isFull());
    EXPECT_FALSE(q.push(99));

    int val;
    q.pop(val);
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(q.isEmpty());
}
