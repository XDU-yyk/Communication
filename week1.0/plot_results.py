#!/usr/bin/env python3
"""
OLSR 仿真结果可视化
读取 FlowMonitor XML + 摘要 CSV，生成 PDR/延迟/路由开销图

使用：
  cd ns-3.43
  python3 scratch/plot_results.py --results-dir results
"""

import os
import sys
import argparse
import xml.etree.ElementTree as ET
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def parse_flowmon_xml(xml_path):
    """解析 FlowMonitor XML，返回 flow 列表"""
    if not os.path.exists(xml_path):
        print(f"[WARN] FlowMonitor XML not found: {xml_path}")
        return []

    tree = ET.parse(xml_path)
    root = tree.getroot()
    flows = []

    for flow in root.findall(".//Flow"):
        f = {}
        f['flowId'] = int(flow.get('flowId', 0))
        f['srcAddr'] = flow.get('sourceAddress', '')
        f['dstAddr'] = flow.get('destinationAddress', '')

        txPackets = int(flow.get('txPackets', 0))
        rxPackets = int(flow.get('rxPackets', 0))
        f['txPackets'] = txPackets
        f['rxPackets'] = rxPackets
        f['pdr'] = (rxPackets / txPackets * 100) if txPackets > 0 else 0.0

        # Delay in seconds from XML
        delay_sum_s = float(flow.get('delaySum', 0))
        f['delaySum_ms'] = delay_sum_s * 1000
        f['avgDelay_ms'] = (delay_sum_s * 1000 / rxPackets) if rxPackets > 0 else 0.0

        max_delay_s = float(flow.get('maxDelay', 0))
        f['maxDelay_ms'] = max_delay_s * 1000

        f['txBytes'] = int(flow.get('txBytes', 0))
        f['rxBytes'] = int(flow.get('rxBytes', 0))

        flows.append(f)

    return flows


def parse_summary_csv(csv_path):
    """读取摘要 CSV 中的聚合数据"""
    if not os.path.exists(csv_path):
        print(f"[WARN] Summary CSV not found: {csv_path}")
        return {}

    data = {}
    with open(csv_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('=') or line.startswith('---'):
                continue
            parts = line.split(',')
            if len(parts) >= 2:
                key = parts[0].strip()
                val = parts[1].strip()
                data[key] = val
    return data


def plot_pdr(flows, output_dir, scenario_name):
    """绘制各流 PDR 直方图"""
    if not flows:
        return

    fig, ax = plt.subplots(figsize=(10, 5))
    flow_ids = [f['flowId'] for f in flows]
    pdrs = [f['pdr'] for f in flows]
    colors = ['green' if p >= 90 else ('orange' if p >= 50 else 'red') for p in pdrs]

    bars = ax.bar(flow_ids, pdrs, color=colors, alpha=0.7)
    ax.axhline(y=90, color='green', linestyle='--', alpha=0.5, label='90% threshold')
    ax.set_xlabel('Flow ID')
    ax.set_ylabel('PDR (%)')
    ax.set_title(f'Packet Delivery Ratio - {scenario_name}')
    ax.set_ylim([0, 105])
    ax.legend()

    plt.tight_layout()
    path = os.path.join(output_dir, f'olsr_pdr_{scenario_name}.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"[OK] Saved: {path}")


def plot_delay(flows, output_dir, scenario_name):
    """绘制各流平均延迟"""
    if not flows:
        return

    fig, ax = plt.subplots(figsize=(10, 5))
    flow_ids = [f['flowId'] for f in flows]
    delays = [f['avgDelay_ms'] for f in flows]
    max_delays = [f['maxDelay_ms'] for f in flows]

    ax.bar(flow_ids, delays, alpha=0.7, label='Avg Delay')
    ax.scatter(flow_ids, max_delays, color='red', s=20, alpha=0.5, label='Max Delay')
    ax.set_xlabel('Flow ID')
    ax.set_ylabel('Delay (ms)')
    ax.set_title(f'End-to-End Delay - {scenario_name}')
    ax.legend()

    plt.tight_layout()
    path = os.path.join(output_dir, f'olsr_delay_{scenario_name}.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"[OK] Saved: {path}")


def plot_overhead(data, output_dir, scenario_name):
    """路由开销概要（文字表格化）"""
    fig, ax = plt.subplots(figsize=(6, 3))
    ax.axis('tight')
    ax.axis('off')

    table_data = [[k, v] for k, v in data.items() 
                  if k in ['TotalTxPkts', 'TotalRxPkts', 'OverallPDR', 'AvgDelay_ms']]
    table = ax.table(cellText=table_data, colLabels=['Metric', 'Value'],
                     loc='center', cellLoc='left')
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    ax.set_title(f'Summary - {scenario_name}', fontsize=12, fontweight='bold')

    plt.tight_layout()
    path = os.path.join(output_dir, f'olsr_summary_{scenario_name}.png')
    plt.savefig(path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"[OK] Saved: {path}")


def main():
    parser = argparse.ArgumentParser(description='OLSR Simulation Results Plotter')
    parser.add_argument('--results-dir', default='results',
                        help='Results directory containing flowmon XML and CSV')
    parser.add_argument('--output-dir', default='figures',
                        help='Output directory for plots')
    parser.add_argument('--scenario', default='static',
                        help='Scenario name for plot labels')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    # 查找 XML 和 CSV
    xml_path = os.path.join(args.results_dir, 'olsr_flowmon.xml')
    csv_path = os.path.join(args.results_dir, 'olsr_summary.csv')

    flows = parse_flowmon_xml(xml_path)
    summary = parse_summary_csv(csv_path)

    print(f"Loaded {len(flows)} flows from FlowMonitor")
    print(f"Summary keys: {list(summary.keys())}")

    # 画图
    plot_pdr(flows, args.output_dir, args.scenario)
    plot_delay(flows, args.output_dir, args.scenario)
    plot_overhead(summary, args.output_dir, args.scenario)

    print(f"\nAll plots saved to {args.output_dir}/")


if __name__ == '__main__':
    main()
