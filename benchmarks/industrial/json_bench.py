import json
import time

count = 50000
json_strings = [
    f'{{"id":{(i * 17) % 100000},"user":"usr_{i % 500}","score":{(i * 23) % 1000},"active":{str(i % 2 == 0).lower()}}}'
    for i in range(count)
]

t1 = time.perf_counter()
sum_score = 0
active_count = 0

for s in json_strings:
    obj = json.loads(s)
    sum_score += obj["score"]
    if obj["active"]:
        active_count += 1

t2 = time.perf_counter()
elapsed_ms = (t2 - t1) * 1000.0
msgs_per_sec = count / (elapsed_ms / 1000.0)

print("JSON Python 3.13 (json.loads) Sonucu:")
print(f"50,000 JSON Kaydi | Toplam Skor: {sum_score} | Aktif Sayisi: {active_count}")
print(f"Sure: {elapsed_ms:.2f} ms | Throughput: {msgs_per_sec:.0f} json/saniye")

