#!/usr/bin/env python3
import subprocess
import os
import sys
import time
import re
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Dict, List

@dataclass
class SyscallStats:
    stats: Dict[str, float] = field(default_factory=lambda: defaultdict(float))
    total_time: float = 0.0

    def add(self, name: str, t: float):
        self.stats[name] += t
        self.total_time += t

    def top_n(self, n: int = 5) -> List[tuple]:
        if self.total_time == 0:
            return []
        sorted_stats = sorted(self.stats.items(), key=lambda x: x[1], reverse=True)
        return [(name, time_val, (time_val / self.total_time) * 100)
                for name, time_val in sorted_stats[:n]]

def parse_strace_line(line: str) -> tuple:
    """Parse a strace -T line: syscall_name(<args>) = <ret> <time>"""
    match = re.match(r'^(\w+)\([^)]*\)\s*=\s*\S+\s*<([0-9.]+)>', line)
    if match:
        return match.group(1), float(match.group(2))
    return None, None

def run_sperf_vis(cmd: List[str], interval_ms: int = 100, top_n: int = 5):
    """Run command with strace and display real-time syscall stats."""

    env = os.environ.copy()
    proc = subprocess.Popen(
        ['strace', '-T', '-f'] + cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env
    )

    stats = SyscallStats()
    last_print = time.time()
    buffer = []

    print(f"\n\033[1;36m▶ Running: {' '.join(cmd)}\033[0m")
    print(f"\033[1;33m{'='*60}\033[0m")
    print(f"\033[1;32m实时系统调用分析 (每{interval_ms}ms更新)\033[0m")
    print(f"\033[1;33m{'='*60}\033[0m\n")

    try:
        while True:
            char = proc.stdout.read(1)
            if not char and proc.poll() is not None:
                break

            buffer.append(char.decode('utf-8', errors='ignore'))
            line = ''.join(buffer)

            if '\n' in line:
                lines = line.split('\n')
                for l in lines[:-1]:
                    name, t = parse_strace_line(l)
                    if name and t is not None:
                        stats.add(name, t)
                buffer = [lines[-1]]

            now = time.time()
            diff_ms = (now - last_print) * 1000

            if diff_ms >= interval_ms and stats.stats:
                display_stats(stats, top_n)
                last_print = now

    except KeyboardInterrupt:
        proc.terminate()

    proc.wait()

    if stats.stats:
        print(f"\n\033[1;34m{'='*60}\033[0m")
        print(f"\033[1;35m最终统计 (共 {len(stats.stats)} 种系统调用, 总耗时 {stats.total_time:.3f}s)\033[0m")
        display_stats(stats, top_n)

def bar(pct: float, width: int = 30) -> str:
    """Create a visual bar."""
    filled = int(pct / 100 * width)
    return '█' * filled + '░' * (width - filled)

def display_stats(stats: SyscallStats, top_n: int):
    """Display current top syscalls with visual bars."""
    top = stats.top_n(top_n)

    print("\033[2J\033[H")
    print(f"\033[1;36m▶ 实时 Top {top_n} 系统调用\033[0m")
    print(f"\033[90m总耗时: {stats.total_time:.3f}s\033[0m\n")

    for i, (name, t, pct) in enumerate(top, 1):
        bar_str = bar(pct)
        color = get_color(i)
        print(f"\033[{color}m{i}. {name:<12} {t:>8.3f}s  {pct:5.1f}% |{bar_str}|\033[0m")

    print(f"\n\033[90m更新于: {time.strftime('%H:%M:%S')}\033[0m")

def get_color(i: int) -> str:
    """Get ANSI color code for ranking."""
    colors = ['33', '31', '32', '36', '35']
    return colors[(i - 1) % len(colors)]

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <命令> [参数...]\n")
        print("示例:")
        print(f"  {sys.argv[0]} find / -name '*.py'")
        print(f"  {sys.argv[0]} ls -la /tmp")
        sys.exit(1)

    run_sperf_vis(sys.argv[1:])
