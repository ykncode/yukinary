#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    int64_t total = 0;
    for (int64_t turn = 0; turn < 10000; ++turn) {
        int64_t values[256];
        for (int64_t index = 0; index < 256; ++index)
            values[index] = (index * 17 + turn) % 997;
        for (int64_t index = 0; index < 256; ++index)
            total += values[index] % 2 == 0 ? values[index] : -values[index];
    }
    int64_t left = 0, right = 1;
    for (int i = 0; i < 40; ++i) {
        int64_t next = left + right;
        left = right;
        right = next;
    }
    if (total != INT64_C(1277871) || left != INT64_C(102334155)) return 1;
    printf("{\"status\":\"PASS\",\"checksum\":%" PRId64 ",\"fibonacci\":%" PRId64 "}\n", total, left);
    return 0;
}
