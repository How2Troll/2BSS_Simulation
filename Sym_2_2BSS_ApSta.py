#!/usr/bin/env python3

import subprocess
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import scipy.stats as stats


# d2_distances = np.arange(40, 120, 10)  # AP <==> STA
d1_distances = np.arange(100, 380, 20)  # AP1 <==> AP2
obss_pd_thresholds = [-64, -72, -78]  # dBm
simulation_file = "scratch/2BSS"
num_runs = 5

rngRun=100

results = []
data_columns = ['Distance', 'Threshold', 'EnableObssPd', 'Mean Throughput (Mbps)', '95% Confidence Interval']

# Iterate through the thresholds with enableObssPd = True
for threshold in obss_pd_thresholds:
    for d1 in d1_distances:
        throughputs = []
        rngRun=100
        for _ in range(num_runs):
            rngRun+=1
            # Run simulation
            cmd = [
                './ns3', 'run',
                f"{simulation_file} --d1={d1} --obssPdThreshold={threshold} --enableObssPd=True --rngRun={rngRun}"
            ]
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
            # Calculate 95% confidence interval using t-distribution
            t_value = stats.t.ppf(0.975, num_runs - 1)
            error_margin = t_value * std_dev / np.sqrt(num_runs)
        else:
            mean_throughput = None
            error_margin = None

        results.append([d1, threshold, True, mean_throughput, error_margin])
        print(f"Distance: {d1}m, Threshold: {threshold} dBm, enableObssPd: True, Throughput: {mean_throughput:.2f} +/- {error_margin:.2f} Mbps")

# Iterate once with enableObssPd = False
for d1 in d1_distances:
    throughputs = []
    for _ in range(num_runs):
        rngRun+=1
        # Run simulation
        cmd = [
            './ns3', 'run',
            f"{simulation_file} --d1={d1} --enableObssPd=False"
        ]
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
        # Calculate 95% confidence interval using t-distribution
        t_value = stats.t.ppf(0.975, num_runs - 1)
        error_margin = t_value * std_dev / np.sqrt(num_runs)
    else:
        mean_throughput = None
        error_margin = None

    results.append([d1, 'N/A', False, mean_throughput, error_margin])
    print(f"Distance: {d1}m, enableObssPd: False, Throughput: {mean_throughput:.2f} +/- {error_margin:.2f} Mbps")

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
plt.xlabel('Distance D1 (m)')
plt.ylabel('Throughput (Mbps)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig('throughput_vs_distance_d1.png')
plt.show()
