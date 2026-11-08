#include <array>
#include <cstdint>
#include <iostream>

int main() {
    std::int64_t total = 0;
    for (std::int64_t turn = 0; turn < 10000; ++turn) {
        std::array<std::int64_t, 256> values{};
        for (std::int64_t index = 0; index < 256; ++index)
            values[static_cast<std::size_t>(index)] = (index * 17 + turn) % 997;
        for (const auto value : values)
            total += value % 2 == 0 ? value : -value;
    }
    std::int64_t left = 0, right = 1;
    for (int i = 0; i < 40; ++i) {
        const auto next = left + right;
        left = right;
        right = next;
    }
    if (total != 1277871 || left != 102334155) return 1;
    std::cout << "{\"status\":\"PASS\",\"checksum\":" << total
              << ",\"fibonacci\":" << left << "}\n";
}
