#include "SegmentTree.h"

SegmentTree::SegmentTree(const std::vector<int>& _arr) : arr(_arr) {
    this->n = _arr.size();
    this->tree.resize(n << 2);
    build(1, 1, n - 1);
}

void SegmentTree::build(int p, int l, int r) {
    tree[p].l = l, tree[p].r = r;
    if(l == r) {
        tree[p].min = arr[l];
        tree[p].minidx = l;
        return;
    }
    int mid = (l + r) >> 1;
    build(ls(p), l, mid);
    build(rs(p), mid + 1, r);
    push_up(p);
}

void SegmentTree::push_up(int p) {
    tree[p].min = std::min(tree[ls(p)].min, tree[rs(p)].min);
    tree[p].minidx = (tree[ls(p)].min > tree[rs(p)].min) ? tree[rs(p)].minidx : tree[ls(p)].minidx;
}

void SegmentTree::updateVal(int p, int x, int val) {
    if(tree[p].l == tree[p].r) {
        tree[p].min = val;
        arr[tree[p].l] = val;
        return;
    }
    int mid = (tree[p].l + tree[p].r) >> 1;
    if(mid >= x) updateVal(ls(p), x, val);
    if(x > mid) updateVal(rs(p), x, val);
    push_up(p);
}

int SegmentTree::queryMin(int p, int ql,int qr) {
    if(ql <= tree[p].l and tree[p].r <= qr) {
        return tree[p].min;
    }
    int mid = (tree[p].l + tree[p].r) >> 1;
    int res = __INT_MAX__;
    if(mid >= ql) res = std::min(res, queryMin(ls(p), ql, qr));
    if(qr > mid) res = std::min(res, queryMin(rs(p), ql ,qr));
    return res;
}

int SegmentTree::queryMinidx(int p, int ql, int qr) {
    if(ql <= tree[p].l and tree[p].r <= qr) {
        return tree[p].minidx;
    }
    int mid = (tree[p].l + tree[p].r) >> 1;
    int left = -1, right = -1;
    if(mid >= ql) left = queryMinidx(ls(p), ql, qr);
    if(qr > mid) right = queryMinidx(rs(p), ql, qr);
    if(left == -1) return right;
    if(right == -1) return left;
    return arr[left] <= arr[right] ? left : right;
}

void SegmentTree::updateVal(int x, int val) {
    return updateVal(1, x, val);
}

int SegmentTree::queryMin(int ql, int qr) {
    return queryMin(1, ql, qr);
}

int SegmentTree::queryMinidx(int ql, int qr) {
    return queryMinidx(1, ql, qr);
}

int SegmentTree::getVal(int x) {
    return arr[x];
}