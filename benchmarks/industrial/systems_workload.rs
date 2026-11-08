use std::time::Instant;

const NUM_RECORDS: usize = 100000;

struct Node {
    key: i64,
    value: i64,
    left: Option<Box<Node>>,
    right: Option<Box<Node>>,
}

impl Node {
    fn insert(root: &mut Option<Box<Node>>, key: i64, value: i64) {
        match root {
            None => {
                *root = Some(Box::new(Node {
                    key,
                    value,
                    left: None,
                    right: None,
                }));
            }
            Some(node) => {
                if key < node.key {
                    Node::insert(&mut node.left, key, value);
                } else if key > node.key {
                    Node::insert(&mut node.right, key, value);
                } else {
                    node.value = value;
                }
            }
        }
    }

    fn search(root: &Option<Box<Node>>, key: i64) -> i64 {
        let mut cur = root;
        while let Some(node) = cur {
            if key == node.key {
                return node.value;
            }
            if key < node.key {
                cur = &node.left;
            } else {
                cur = &node.right;
            }
        }
        -1
    }

    fn range_sum(root: &Option<Box<Node>>, min_k: i64, max_k: i64) -> i64 {
        match root {
            None => 0,
            Some(node) => {
                let mut sum = 0;
                if node.key >= min_k && node.key <= max_k {
                    sum += node.value;
                }
                if node.key > min_k {
                    sum += Node::range_sum(&node.left, min_k, max_k);
                }
                if node.key < max_k {
                    sum += Node::range_sum(&node.right, min_k, max_k);
                }
                sum
            }
        }
    }
}

fn main() {
    let start = Instant::now();

    let mut root: Option<Box<Node>> = None;
    let mut pseudo_rand: i64 = 133742;

    // 1. Insertion
    for i in 0..NUM_RECORDS {
        pseudo_rand = (pseudo_rand.wrapping_mul(1103515245).wrapping_add(12345)) & 0x7FFFFFFF;
        let key = pseudo_rand % 200000;
        let val = (i as i64) * 3;
        Node::insert(&mut root, key, val);
    }

    // 2. Lookups
    let mut found_count = 0;
    for _ in 0..50000 {
        pseudo_rand = (pseudo_rand.wrapping_mul(1103515245).wrapping_add(12345)) & 0x7FFFFFFF;
        let key = pseudo_rand % 200000;
        if Node::search(&root, key) >= 0 {
            found_count += 1;
        }
    }

    // 3. Range Aggregation
    let total_sum = Node::range_sum(&root, 50000, 150000);

    // Drop root (deallocation)
    drop(root);

    let elapsed = start.elapsed();
    println!(
        "[RUST-OPT3] Systems Workload: {:.4} s | Found: {} | RangeSum: {}",
        elapsed.as_secs_f64(),
        found_count,
        total_sum
    );
}

