#!/usr/bin/env python3

import subprocess
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import scipy.stats as stats

# Corrected range for d3 distances
d3_distances = np.arange(20, 300, 20)  # AP1 <==> AP2
obss_pd_thresholds = [-64, -72, -78]  # dBm
simulation_file = "scratch/2BSS"
num_runs = 5

results = []
data_columns = ['Distance', 'Threshold', 'EnableObssPd', 'Mean Throughput (Mbps)', '95% Confidence Interval']

# Mock function to simulate realistic throughput with some variability
def run_simulation(d3, threshold, enable_obss_pd):
    base_throughput = np.random.uniform(30, 40)
    variability = np.random.normal(0, 1, num_runs)  # Adding some variability
    return base_throughput + variability

# Iterate through the thresholds with enableObssPd = True
for threshold in obss_pd_thresholds:
    for d3 in d3_distances:
        throughputs = run_simulation(d3, threshold, True)
        
        if throughputs.size > 0:
            mean_throughput = np.mean(throughputs)
            std_dev = np.std(throughputs)
            # Calculate 95% confidence interval using t-distribution
            t_value = stats.t.ppf(0.975, num_runs - 1)
            error_margin = t_value * std_dev / np.sqrt(num_runs)
        else:
            mean_throughput = None
            error_margin = None

        results.append([d3, threshold, True, mean_throughput, error_margin])
        print(f"Distance: {d3}m, Threshold: {threshold} dBm, enableObssPd: True, Throughput: {mean_throughput:.2f} +/- {error_margin:.2f} Mbps")

# Iterate once with enableObssPd = False
for d3 in d3_distances:
    throughputs = run_simulation(d3, None, False)
    
    if throughputs.size > 0:
        mean_throughput = np.mean(throughputs)
        std_dev = np.std(throughputs)
        # Calculate 95% confidence interval using t-distribution
        t_value = stats.t.ppf(0.975, num_runs - 1)
        error_margin = t_value * std_dev / np.sqrt(num_runs)
    else:
        mean_throughput = None
        error_margin = None

    results.append([d3, 'N/A', False, mean_throughput, error_margin])
    print(f"Distance: {d3}m, enableObssPd: False, Throughput: {mean_throughput:.2f} +/- {error_margin:.2f} Mbps")

# Create DataFrame and save as CSV
results_df = pd.DataFrame(results, columns=data_columns)
results_csv_path = 'throughput_results.csv'
results_df.to_csv(results_csv_path, index=False)

plt.figure(figsize=(10, 6))
for (thr, enable_obss_pd), group in results_df.groupby(['Threshold', 'EnableObssPd']):
    if enable_obss_pd:
        label = f'OBSS_PD Enabled threshold = {thr} dBm'
    else:
        label = 'OBSS_PD Disabled'
    plt.errorbar(group['Distance'], group['Mean Throughput (Mbps)'], yerr=group['95% Confidence Interval'], fmt='o-', label=label)

plt.title('Throughput vs. Distance with Different OBSS_PD Thresholds and 95% CI')
plt.xlabel('Distance D3 (m)')
plt.ylabel('Throughput (Mbps)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig('throughput_vs_distance_with_CI_corrected.png')
plt.show()
