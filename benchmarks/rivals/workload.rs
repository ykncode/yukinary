fn main() {
    let mut total: i64 = 0;
    for turn in 0_i64..10_000 {
        let mut values = [0_i64; 256];
        for (index, value) in values.iter_mut().enumerate() {
            *value = (index as i64 * 17 + turn) % 997;
        }
        for value in values {
            total += if value % 2 == 0 { value } else { -value };
        }
    }
    let (mut left, mut right) = (0_i64, 1_i64);
    for _ in 0..40 {
        (left, right) = (right, left + right);
    }
    assert_eq!(total, 1_277_871);
    assert_eq!(left, 102_334_155);
    println!("{{\"status\":\"PASS\",\"checksum\":{total},\"fibonacci\":{left}}}");
}
