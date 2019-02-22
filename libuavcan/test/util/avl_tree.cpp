/*
 * Copyright (C) 2019 Theodoros Ntakouris <zarkopafilis@gmail.com>
 */

#include <gtest/gtest.h>
#include <uavcan/util/avl_tree.hpp>
#include <uavcan/dynamic_memory.hpp>
#include <uavcan/driver/system_clock.hpp>

using uavcan::AvlTree;

struct Entry {
    int key;
    int payload;

    bool operator<(const Entry& other) const {
        return this->key < other.key;
    }

    bool operator>(const Entry& other) const {
        return this->key > other.key;
    }

    bool operator==(const Entry& other) const {
        return this->key == other.key;
    }
};

// OOM-Unsafe
inline Entry* makeEntry(uavcan::PoolAllocator<64 * 24, 64>* allocator, int key, int payload) {
    void* praw = allocator->allocate(sizeof(Entry));

    Entry* e = new (praw) Entry();
    UAVCAN_ASSERT(e);

    e->key = key;
    e->payload = payload;
    return e;
}

inline bool matchPostOrder(Entry* expected[], AvlTree<Entry>* tree) {
    int count = 0;
    bool res = true;

    tree->walkPostOrder([expected, &count, &res](Entry*& in) {
        auto res_ = in == expected[count];
        res &= res_;
        count++;
    });

    return res;
}

/* Basic sanity checks */
TEST(AvlTree, Sanity) {
    uavcan::PoolAllocator<64 * 24, 64> pool; // 4 (x2) entries capacity

    AvlTree<Entry> tree(pool, 99999);
    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(0, pool.getNumUsedBlocks());

    auto e1 = makeEntry(&pool, 1, 1);
    auto e2 = makeEntry(&pool, 2, 2);
    auto e3 = makeEntry(&pool, 3, 3);
    auto e4 = makeEntry(&pool, 4, 4);

    EXPECT_EQ(4, pool.getNumUsedBlocks());

    tree.insert(e1);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_EQ(e1, tree.max());
    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());

    tree.remove_entry(e1);
    EXPECT_FALSE(tree.contains(e1));
    EXPECT_EQ(UAVCAN_NULLPTR, tree.max());
    EXPECT_EQ(0, tree.getSize());
    EXPECT_EQ(4, pool.getNumUsedBlocks());

    // Won't break if asked to remove data that do not exist
    tree.remove_entry(e1);
    EXPECT_FALSE(tree.contains(e1));
    EXPECT_EQ(UAVCAN_NULLPTR, tree.max());
    EXPECT_EQ(0, tree.getSize());
    EXPECT_EQ(4, pool.getNumUsedBlocks());

    /*
     * Insert e2 - e1 - e3 - e4
     */

    tree.insert(e2);
    EXPECT_TRUE(tree.contains(e2));
    EXPECT_EQ(e2, tree.max());
    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());

    tree.insert(e1);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_EQ(e2, tree.max());
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());

    tree.insert(e3);
    EXPECT_TRUE(tree.contains(e3));
    EXPECT_EQ(e3, tree.max());
    EXPECT_EQ(3, tree.getSize());
    EXPECT_EQ(7, pool.getNumUsedBlocks());

    tree.insert(e4);
    EXPECT_TRUE(tree.contains(e4));
    EXPECT_EQ(e4, tree.max());
    EXPECT_EQ(4, tree.getSize());
    EXPECT_EQ(8, pool.getNumUsedBlocks());

    /*
     * Remove e2 - e4
     */

    tree.remove_entry(e2);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_FALSE(tree.contains(e2));
    EXPECT_TRUE(tree.contains(e3));
    EXPECT_TRUE(tree.contains(e4));
    EXPECT_EQ(e4, tree.max());
    EXPECT_EQ(3, tree.getSize());
    EXPECT_EQ(7, pool.getNumUsedBlocks());

    tree.remove_entry(e4);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e3));
    EXPECT_FALSE(tree.contains(e4));
    EXPECT_EQ(e3, tree.max());
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());
}

/* Test multiple entries with same 'key' */
TEST(AvlTree, MultipleEntriesPerKey) {
    uavcan::PoolAllocator<64 * 24, 64> pool; // 4 (x2) entries capacity

    AvlTree<Entry> tree(pool, 99999);

    auto e1 = makeEntry(&pool, 1, 1);
    auto e1_1 = makeEntry(&pool, 1, 11);
    auto e1_11 = makeEntry(&pool, 1, 111);

    auto e2 = makeEntry(&pool, 2, 2);

    /*
     * Insert 2 entries with same key
     */
    tree.insert(e1);

    tree.insert(e1_1);
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e1_1));
    EXPECT_EQ(e1, tree.max());
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());


    tree.remove_entry(e1);
    EXPECT_FALSE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e1_1));

    EXPECT_EQ(e1_1, tree.max());
    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());

    tree.remove_entry(e1);

    /*
     * Insert another with higher priority and
     * test again: removing in the middle and end of queue
     * */
    tree.insert(e2);

    tree.insert(e1);
    tree.insert(e1_1);
    tree.insert(e1_11);

    EXPECT_TRUE(tree.contains(e2));
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_TRUE(tree.contains(e1_1));
    EXPECT_TRUE(tree.contains(e1_11));

    EXPECT_EQ(e2, tree.max());
    EXPECT_EQ(4, tree.getSize());
    EXPECT_EQ(8, pool.getNumUsedBlocks());

    tree.remove_entry(e2);
    tree.remove_entry(e1_1); // middle one in node with key == 1
    EXPECT_FALSE(tree.contains(e2));
    EXPECT_TRUE(tree.contains(e1));
    EXPECT_FALSE(tree.contains(e1_1));
    EXPECT_TRUE(tree.contains(e1_11));

    EXPECT_EQ(e1, tree.max()); // peeked in the order they were inserted
    EXPECT_EQ(2, tree.getSize());
    EXPECT_EQ(6, pool.getNumUsedBlocks());

    tree.remove_entry(e1_11); // last one in queue
    EXPECT_EQ(e1, tree.max());
    EXPECT_FALSE(tree.contains(e1_11));

    EXPECT_EQ(1, tree.getSize());
    EXPECT_EQ(5, pool.getNumUsedBlocks());
}

/* Check all possible rotation / balancing cases
 * Test cases from: https://stackoverflow.com/questions/3955680/how-to-check-if-my-avl-tree-implementation-is-correct
 * */
TEST(AvlTree, AllRotations) {
    uavcan::PoolAllocator<64 * 24, 64> pool; // 4 (x2) entries capacity

    AvlTree<Entry> tree(pool, 99999);
    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(0, pool.getNumUsedBlocks());

    auto a = makeEntry(&pool, 1, 1);
    auto b = makeEntry(&pool, 2, 2);
    auto c = makeEntry(&pool, 3, 3);
    auto d = makeEntry(&pool, 4, 4);
    auto e = makeEntry(&pool, 5, 5);
    auto f = makeEntry(&pool, 6, 6);
    auto g = makeEntry(&pool, 7, 7);
    auto h = makeEntry(&pool, 8, 8);
    auto i = makeEntry(&pool, 9, 9);
    auto j = makeEntry(&pool, 10, 10);
    auto k = makeEntry(&pool, 11, 11);
    auto l = makeEntry(&pool, 12, 12);

    /*
     * Simple test cases for insert
     * */

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
    /*
     * a                   b
         \                 / \
          b   == 1L ==>   a   c
           \
            c
     */
    tree.insert(a);
    tree.insert(b);
    tree.insert(c);

    Entry* match_1l[] = {a, c, b};
    EXPECT_TRUE(matchPostOrder(match_1l , &tree));

    tree.remove_entry(a);
    tree.remove_entry(b);
    tree.remove_entry(c);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
    /*
     *
     *      c               b
           /               / \
          b   == 1R ==>   a   c
         /
        a
     */
    tree.insert(c);
    tree.insert(b);
    tree.insert(a);

    Entry* match_1r[] = {a,c,b};
    EXPECT_TRUE(matchPostOrder(match_1r , &tree));

    tree.remove_entry(c);
    tree.remove_entry(b);
    tree.remove_entry(a);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
    /*
     *
     * a                  b
         \                / \
          c   == 2L ==>  a   c
         /
        b
     */
    tree.insert(a);
    tree.insert(c);
    tree.insert(b);

    Entry* match_2l[] = {a,c,b};
    EXPECT_TRUE(matchPostOrder(match_2l , &tree));

    tree.remove_entry(a);
    tree.remove_entry(c);
    tree.remove_entry(b);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
    /*
     *
     *    c                b
         /                / \
        a     == 2R ==>  a   c
         \
          b
     */

    tree.insert(c);
    tree.insert(a);
    tree.insert(b);

    Entry* match_2r[] = {a,c,b};
    EXPECT_TRUE(matchPostOrder(match_2r , &tree));

    tree.remove_entry(c);
    tree.remove_entry(a);
    tree.remove_entry(b);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());

    /*
     * Simple cases for deletion
     */

    /*
     *    b                   c
         x \                 / \
        a   c   == 1L ==>   b   d
             \
              d
     */

    tree.insert(b);
    tree.insert(a);
    tree.insert(c);
    tree.insert(d);

    Entry* match_pre_del_1l[] = {a,d,c,b};
    EXPECT_TRUE(matchPostOrder(match_pre_del_1l , &tree));

    tree.remove_entry(a);

    Entry* match_post_del_1l[] = {b,d,c};
    EXPECT_TRUE(matchPostOrder(match_post_del_1l , &tree));

    tree.remove_entry(b);
    tree.remove_entry(c);
    tree.remove_entry(d);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());

    /*
     *
     *      c                  b
           / x                / \
          b   d  == 1R ==>   a   c
         /
        a
     */
    tree.insert(c);
    tree.insert(d);
    tree.insert(b);
    tree.insert(a);

    Entry* match_pre_del_1r[] = {a,b,d,c};
    EXPECT_TRUE(matchPostOrder(match_pre_del_1r , &tree));

    tree.remove_entry(d);

    Entry* match_post_del_1r[] = {a,c,b};
    EXPECT_TRUE(matchPostOrder(match_post_del_1r , &tree));

    tree.remove_entry(c);
    tree.remove_entry(b);
    tree.remove_entry(a);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
    /*
     *    b                  c
         x \                / \
        a   d   == 2L ==>  b   d
           /
          c
     */
    tree.insert(b);
    tree.insert(a);
    tree.insert(d);
    tree.insert(c);

    Entry* match_pre_del_2l[] = {a,c,d,b};
    EXPECT_TRUE(matchPostOrder(match_pre_del_2l , &tree));

    tree.remove_entry(a);

    Entry* match_post_del_2l[] = {b,d,c};
    EXPECT_TRUE(matchPostOrder(match_post_del_2l , &tree));

    tree.remove_entry(b);
    tree.remove_entry(d);
    tree.remove_entry(c);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
    /*
     *
     *    c                  b
         / x                / \
        a   d   == 2R ==>  a   c
         \
          b
     */
    tree.insert(c);
    tree.insert(d);
    tree.insert(a);
    tree.insert(b);

    Entry* match_pre_del_2r[] = {b,a,d,c};
    EXPECT_TRUE(matchPostOrder(match_pre_del_2r , &tree));

    tree.remove_entry(d);

    Entry* match_post_del_2r[] = {a,c,b};
    EXPECT_TRUE(matchPostOrder(match_post_del_2r , &tree));

    tree.remove_entry(c);
    tree.remove_entry(a);
    tree.remove_entry(b);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());

    /*
     * More Complex Tests
     */

    /*
     *
     *        c                 e
             / \               / \
            b   e  == 1R ==>  c   f
           x   / \           / \   \
          a   d   f         b   d   g
                   \
                    g
     */

    tree.insert(c);
    tree.insert(b);
    tree.insert(e);
    tree.insert(a);
    tree.insert(d);
    tree.insert(f);
    tree.insert(g);
    Entry* match_c_pre_del_1r[] = {a,b,d,g,f,e,c};
    EXPECT_TRUE(matchPostOrder(match_c_pre_del_1r , &tree));

    tree.remove_entry(a);

    Entry* match_c_post_del_1r[] = {b,d,c,g,f,e};
    EXPECT_TRUE(matchPostOrder(match_c_post_del_1r , &tree));

    tree.remove_entry(c);
    tree.remove_entry(b);
    tree.remove_entry(e);
    tree.remove_entry(d);
    tree.remove_entry(f);
    tree.remove_entry(g);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());

    /*
     *
     *        - e -                 c
             /     \               / \
            c       f  == 1R ==>  b   e
           / \     x             /   / \
          b   d   g             a   d   f
         /
        a
     */

    tree.insert(e);
    tree.insert(c);
    tree.insert(f);
    tree.insert(b);
    tree.insert(d);
    tree.insert(g);
    tree.insert(a);

    Entry* match_c2_pre_del_1r[] = {a,b,d,c,g,f,e};
    EXPECT_TRUE(matchPostOrder(match_c2_pre_del_1r , &tree));

    tree.remove_entry(g);

    Entry* match_c2_post_del_1r[] = {a,b,d,f,e,c};
    EXPECT_TRUE(matchPostOrder(match_c2_post_del_1r , &tree));

    tree.remove_entry(e);
    tree.remove_entry(c);
    tree.remove_entry(f);
    tree.remove_entry(b);
    tree.remove_entry(d);
    tree.remove_entry(a);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());

    /*
     *
     *      - e -                       —- h —-
           /     \                     /       \
          c       j                   - e-      j
         / \     / \   == 2L ==>     /    \    / \
        a   d   h   k               c      g  i   k
         x     / \   \             / \    /        \
          b   g   i   l           a   d  f          l
             /
            f
     */

    tree.insert(e);
    tree.insert(c);
    tree.insert(j);
    tree.insert(a);
    tree.insert(d);
    tree.insert(h);
    tree.insert(k);
    tree.insert(b);
    tree.insert(g);
    tree.insert(i);
    tree.insert(l);
    tree.insert(f);

    Entry* match_c_pre_del_2l[] = {b,a,d,c,f,g,i,h,l,k,j,e};
    EXPECT_TRUE(matchPostOrder(match_c_pre_del_2l , &tree));

    tree.remove_entry(b);

    Entry* match_c_post_del_2l[] = {a,d,c,f,g,e,i,l,k,j,h};
    EXPECT_TRUE(matchPostOrder(match_c_post_del_2l , &tree));

    tree.remove_entry(e);
    tree.remove_entry(c);
    tree.remove_entry(j);
    tree.remove_entry(a);
    tree.remove_entry(d);
    tree.remove_entry(h);
    tree.remove_entry(k);
    tree.remove_entry(g);
    tree.remove_entry(i);
    tree.remove_entry(l);
    tree.remove_entry(f);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
    /*
     *
     *        - h -                    - e -
             /     \                  /     \
            c       k                c       - h -
           / \     / \  == 2R ==>   / \     /     \
          b   e   i   l            b   d   f       k
         /   / \   x              /         \     / \
        a   d   f   j            a           g   i   l
                 \
                  g
     */
    tree.insert(h);
    tree.insert(c);
    tree.insert(k);
    tree.insert(b);
    tree.insert(e);
    tree.insert(i);
    tree.insert(l);
    tree.insert(a);
    tree.insert(d);
    tree.insert(f);
    tree.insert(j);
    tree.insert(g);

    Entry* match_c_pre_del_2r[] = {a,b,d,g,f,e,c,j,i,l,k,h};
    EXPECT_TRUE(matchPostOrder(match_c_pre_del_2r , &tree));

    tree.remove_entry(j);

    Entry* match_c_post_del_2r[] = {a,b,d,c,g,f,i,l,k,h,e};
    EXPECT_TRUE(matchPostOrder(match_c_post_del_2r , &tree));

    tree.remove_entry(h);
    tree.remove_entry(c);
    tree.remove_entry(k);
    tree.remove_entry(b);
    tree.remove_entry(e);
    tree.remove_entry(i);
    tree.remove_entry(l);
    tree.remove_entry(a);
    tree.remove_entry(d);
    tree.remove_entry(f);
    tree.remove_entry(g);

    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(12, pool.getNumUsedBlocks());
}