#!/usr/bin/env python3

import subprocess
import numpy as np
import matplotlib.pyplot as plt
import scipy.stats as stats

# Corrected range for d3 distances


#("d3", "Distance between AP1 and AP2
#("d2", "Distance between AP and STA

#d3_distances = np.arange(40, 120, 10)  # [1, 201, 401]
d3_distances = np.arange(20, 300, 20)
obss_pd_thresholds = [-64, -72, -78]  # dBm
simulation_file = "scratch/2BSS"

num_runs = 1

results = {}

for threshold in obss_pd_thresholds:
    results[threshold] = {
        'means': [],
        'errors': []
    }

    for d3 in d3_distances:
        throughputs = []
        for _ in range(num_runs):
            # Run simulation
            cmd = [
                './ns3', 'run',
                f"{simulation_file} --d3={d3} --obssPdThreshold={threshold}  --enableObssPd={True}"
            ]#d2 only for tesst!
            print("Running simulation:", ' '.join(cmd))
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
            stdout, _ = process.communicate()

            # Fetching data from sim
            try:
                for line in stdout.split('\n'):
                    if "Throughput per STA:" in line:
                        throughput = float(line.split('\t')[1].split(' ')[0])
                        throughputs.append(throughput)
                        break
            except ValueError as e:
                print("Error parsing throughput: ", e)

        if throughputs:
            mean_throughput = np.mean(throughputs)
            std_dev = np.std(throughputs)
            # Calculate 95% confidence interval jak u papiego siuu
            error_margin = 1.96 * std_dev / np.sqrt(num_runs)
            results[threshold]['means'].append(mean_throughput)
            results[threshold]['errors'].append(error_margin)
        else:
            results[threshold]['means'].append(None)
            results[threshold]['errors'].append(None)

        print(f"Distance: {d3}m, Threshold: {threshold} dBm, Throughput: {mean_throughput:.2f} +/- {error_margin:.2f} Mbps")

plt.figure(figsize=(10, 6))
for thr, data in results.items():
    plt.errorbar(d3_distances, data['means'], yerr=data['errors'], fmt='o-', label=f'OBSS_PD threshold = {thr} dBm')

plt.title('Throughput vs. Distance with Different OBSS_PD Thresholds and 95% CI')
plt.xlabel('Distance D3 (m)')
plt.ylabel('Throughput (Mbps)')
plt.legend()
plt.grid(True)
plt.tight_layout() 
plt.savefig('throughput_vs_distance_with_CI_corrected.png')
plt.show()
