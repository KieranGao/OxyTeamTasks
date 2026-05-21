#ifndef SEGMENTTREE_H
#define SEGMENTTREE_H

#define ls(p) (p << 1)
#define rs(p) (p << 1 | 1)

#include <vector>
// 支持单点修改，区间求取最小值下标的权值线段树
class SegmentTree {
public:
    SegmentTree() = default;
    ~SegmentTree() = default;
    explicit SegmentTree(const std::vector<int>& _arr);
    void updateVal(int x, int val);
    int getVal(int x);
    int queryMin(int ql, int qr);
    int queryMinidx(int ql, int qr);
private:
    int n;
    struct Node {
        int l, r;
        int min, minidx;
    }; 
    std::vector<int> arr;
    std::vector<Node> tree;
    void push_up(int p);
    void build(int p, int l, int r);
    void updateVal(int p, int x, int val);
    int queryMin(int p, int ql, int qr);
    int queryMinidx(int p, int ql, int qr);
};


#endif /* SEGMENTTREE_H */
