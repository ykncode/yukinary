#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define NUM_RECORDS 100000

typedef struct Node {
    int64_t key;
    int64_t value;
    struct Node* left;
    struct Node* right;
} Node;

static Node* insert(Node* root, int64_t key, int64_t value) {
    if (!root) {
        Node* n = (Node*)malloc(sizeof(Node));
        n->key = key;
        n->value = value;
        n->left = n->right = NULL;
        return n;
    }
    if (key < root->key) root->left = insert(root->left, key, value);
    else if (key > root->key) root->right = insert(root->right, key, value);
    else root->value = value;
    return root;
}

static int64_t search(Node* root, int64_t key) {
    Node* cur = root;
    while (cur) {
        if (key == cur->key) return cur->value;
        if (key < cur->key) cur = cur->left;
        else cur = cur->right;
    }
    return -1;
}

static int64_t range_sum(Node* root, int64_t min_k, int64_t max_k) {
    if (!root) return 0;
    int64_t sum = 0;
    if (root->key >= min_k && root->key <= max_k) sum += root->value;
    if (root->key > min_k) sum += range_sum(root->left, min_k, max_k);
    if (root->key < max_k) sum += range_sum(root->right, min_k, max_k);
    return sum;
}

static void free_tree(Node* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main() {
    clock_t start = clock();

    Node* root = NULL;
    int64_t pseudo_rand = 133742;

    // 1. Insertion
    for (int i = 0; i < NUM_RECORDS; ++i) {
        pseudo_rand = (pseudo_rand * 1103515245 + 12345) & 0x7FFFFFFF;
        int64_t key = pseudo_rand % 200000;
        int64_t val = i * 3;
        root = insert(root, key, val);
    }

    // 2. Lookups
    int64_t found_count = 0;
    for (int i = 0; i < 50000; ++i) {
        pseudo_rand = (pseudo_rand * 1103515245 + 12345) & 0x7FFFFFFF;
        int64_t key = pseudo_rand % 200000;
        if (search(root, key) >= 0) found_count++;
    }

    // 3. Range Aggregation
    int64_t total_sum = range_sum(root, 50000, 150000);

    // 4. Deallocation
    free_tree(root);

    clock_t end = clock();
    double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;

    printf("[C-GCC-O3] Systems Workload: %.4f s | Found: %lld | RangeSum: %lld\n",
           elapsed_sec, (long long)found_count, (long long)total_sum);
    return 0;
}

