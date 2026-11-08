package main

import "fmt"

func main() {
    var total int64
    for turn := int64(0); turn < 10000; turn++ {
        var values [256]int64
        for index := int64(0); index < 256; index++ {
            values[index] = (index*17 + turn) % 997
        }
        for _, value := range values {
            if value%2 == 0 {
                total += value
            } else {
                total -= value
            }
        }
    }
    left, right := int64(0), int64(1)
    for i := 0; i < 40; i++ {
        left, right = right, left+right
    }
    if total != 1277871 || left != 102334155 {
        panic("checksum mismatch")
    }
    fmt.Printf("{\"status\":\"PASS\",\"checksum\":%d,\"fibonacci\":%d}\n", total, left)
}
