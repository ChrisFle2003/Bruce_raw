import subprocess
import re
import statistics
import time

RUNS = 20  # reicht, wenn das exe intern median macht

CMD = [
    r"./out/build/x64-Release/bench_mix_v4.exe",
    "--test", "allowed",
    "--dims", "7",
    "--loops", "200000",
    "--runs", "50",
    "--warmup", "5",
    "--mode", "worst",
]

TIME_RE = re.compile(r":\s*([0-9]+)\s*us")
times = []

for i in range(RUNS):
    result = subprocess.run(CMD, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    match = TIME_RE.search(result.stdout)
    if not match:
        print(f"[{i+1}] ❌ No timing found!\nstdout={result.stdout}\nstderr={result.stderr}")
        continue

    us = int(match.group(1))
    times.append(us)
    print(f"[{i+1:03}] {us} us")

    time.sleep(0.01)  # optional

print("\n=== RESULTS ===")
times_sorted = sorted(times)
print(f"Runs collected : {len(times)}")
print(f"Min           : {min(times)} us")
print(f"Max           : {max(times)} us")
print(f"Median        : {statistics.median(times_sorted)} us")
print(f"Mean          : {round(statistics.mean(times_sorted), 2)} us")
p95_index = int(0.95 * len(times_sorted)) - 1
print(f"P95           : {times_sorted[p95_index]} us")
