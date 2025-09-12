#!/usr/bin/env python3
"""
Monero 3-Node Private Testnet Setup Script

This script sets up a 3-node private Monero testnet with:
- 1 dedicated miner node
- 2 non-mining validation nodes
- Private testnet with network prefix 99
- Isolated network (localhost only)
- Pre-configured mining address

Requirements:
- Monero daemon (monerod) in PATH
- Python 3.6+
- PyYAML (pip install PyYAML)

Usage:
    python setup_private_testnet.py [--start-mining] [--mining-address ADDRESS]

The script will:
1. Initialize the environment
2. Configure 3 nodes with unique ports and data directories
3. Set up exclusive node connections for isolation
4. Start the miner node with the specified mining address
5. Start the validation nodes
6. Monitor the network until interrupted

Network Configuration:
- Node 0 (Miner): P2P port 18080, RPC port 18081
- Node 1 (Validator): P2P port 18090, RPC port 18091
- Node 2 (Validator): P2P port 18100, RPC port 18101
- All nodes connect exclusively to each other
- Mining address: A1pk9KBcz12fEEe4hEB6Kn1EE7QSb3WrtPDw6yHwq2CCE3CfitnmgsFezUnABGaWryKCLSykzZ58uVv4HBcyPoFDGCh6Cyy

Note: For true network prefix 99 support, a custom monerod build may be required.
This script uses regtest mode which provides similar isolation functionality.
"""

import subprocess
import time
import os
import signal
import sys
import json
import urllib.request
import urllib.error
import threading
import argparse
from typing import List, Dict

# Configuration
MINING_ADDRESS = 'A1pk9KBcz12fEEe4hEB6Kn1EE7QSb3WrtPDw6yHwq2CCE3CfitnmgsFezUnABGaWryKCLSykzZ58uVv4HBcyPoFDGCh6Cyy'
OUTPUT_DIR = './simulation_output'
NODES = [
    {'id': 0, 'role': 'miner', 'p2p_port': 18080, 'rpc_port': 18081},
    {'id': 1, 'role': 'validator', 'p2p_port': 18090, 'rpc_port': 18091},
    {'id': 2, 'role': 'validator', 'p2p_port': 18100, 'rpc_port': 18101}
]

class PrivateTestnetSetup:
    """Manages the 3-node private testnet setup."""

    def __init__(self, mining_address=None):
        self.nodes = []
        self.output_dir = OUTPUT_DIR
        self.mining_address = mining_address
        self.running = False

    def setup_environment(self):
        """Initialize the testnet environment."""
        print("Setting up private testnet environment...")

        # Create output directory
        os.makedirs(self.output_dir, exist_ok=True)
        os.makedirs(os.path.join(self.output_dir, 'logs'), exist_ok=True)

        # Create node configurations
        for node_config in NODES:
            node = {
                'id': node_config['id'],
                'role': node_config['role'],
                'p2p_port': node_config['p2p_port'],
                'rpc_port': node_config['rpc_port'],
                'data_dir': os.path.join(self.output_dir, 'testnet_data', f'node_{node_config["id"]}'),
                'rpc_url': f'http://localhost:{node_config["rpc_port"]}/json_rpc',
                'process': None
            }
            self.nodes.append(node)

        print("✓ Environment setup complete")

    def start_nodes(self):
        """Start all configured nodes."""
        print("Starting nodes...")

        for node in self.nodes:
            self._start_node(node)

        print("✓ All nodes started")

    def _start_node(self, node):
        """Start a single node."""
        os.makedirs(node['data_dir'], exist_ok=True)

        # Base command
        cmd = [
            'monerod',
            '--data-dir', node['data_dir'],
            '--p2p-bind-port', str(node['p2p_port']),
            '--rpc-bind-port', str(node['rpc_port']),
            '--regtest',  # Private testnet mode
            '--non-interactive',
            '--no-igd',
            '--hide-my-port',
            '--log-level', '1',
            '--no-zmq'  # Disable ZMQ to avoid port conflicts
        ]

        # Add exclusive connections for isolation
        for other_node in self.nodes:
            if other_node['id'] != node['id']:
                cmd.extend(['--add-exclusive-node', f'127.0.0.1:{other_node["p2p_port"]}'])

        # Add mining for miner node
        if node['role'] == 'miner' and self.mining_address:
            cmd.extend(['--start-mining', self.mining_address])

        print(f"Starting {node['role']} node {node['id']} on ports {node['p2p_port']}/{node['rpc_port']}")
        print(f"Command: {' '.join(cmd)}")

        try:
            node['process'] = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )

            # Start log monitoring threads
            threading.Thread(target=self._log_output, args=(node, 'stdout'), daemon=True).start()
            threading.Thread(target=self._log_output, args=(node, 'stderr'), daemon=True).start()

        except Exception as e:
            print(f"❌ Failed to start node {node['id']}: {e}")
            sys.exit(1)

    def _log_output(self, node, stream_type):
        """Log node output."""
        stream = node['process'].stdout if stream_type == 'stdout' else node['process'].stderr
        log_file = os.path.join(self.output_dir, 'logs', f'node_{node["id"]}.log')

        with open(log_file, 'a') as f:
            for line in iter(stream.readline, ''):
                f.write(f"[{time.strftime('%H:%M:%S')}] {line}")
                if any(keyword in line.lower() for keyword in ['error', 'failed', 'mining', 'mined block', 'new block']):
                    print(f"Node {node['id']} {stream_type}: {line.strip()}")

    def wait_for_nodes_ready(self):
        """Wait for all nodes to be ready."""
        print("Waiting for nodes to be ready...")

        for node in self.nodes:
            while not self._is_node_ready(node):
                time.sleep(2)
                print(f"Waiting for node {node['id']}...")

        print("✓ All nodes ready")

    def _is_node_ready(self, node):
        """Check if a node is ready via RPC."""
        try:
            data = {'jsonrpc': '2.0', 'id': '0', 'method': 'get_info'}
            req = urllib.request.Request(node['rpc_url'], data=json.dumps(data).encode(),
                                       headers={'Content-Type': 'application/json'})
            with urllib.request.urlopen(req, timeout=5) as response:
                result = json.loads(response.read().decode())
                return result.get('result', {}).get('status') == 'OK'
        except:
            return False

    def monitor_network(self):
        """Monitor the network status."""
        print("Monitoring network... (Press Ctrl+C to stop)")

        try:
            while self.running:
                self._print_network_status()
                time.sleep(10)
        except KeyboardInterrupt:
            print("\nStopping network...")

    def _print_network_status(self):
        """Print current network status."""
        print(f"\n{'='*50}")
        print(f"Network Status at {time.strftime('%H:%M:%S')}")
        print(f"{'='*50}")

        for node in self.nodes:
            try:
                data = {'jsonrpc': '2.0', 'id': '0', 'method': 'get_info'}
                req = urllib.request.Request(node['rpc_url'], data=json.dumps(data).encode(),
                                           headers={'Content-Type': 'application/json'})
                with urllib.request.urlopen(req, timeout=5) as response:
                    result = json.loads(response.read().decode())
                    info = result.get('result', {})

                    height = info.get('height', 0)
                    connections = info.get('incoming_connections_count', 0) + info.get('outgoing_connections_count', 0)
                    status = "✓" if info.get('status') == 'OK' else "✗"

                    print(f"Node {node['id']} ({node['role']}): {status} Height: {height}, Connections: {connections}")
                    if node['id'] == 0:
                        try:
                            data = {'jsonrpc': '2.0', 'id': '0', 'method': 'get_mining_status'}
                            req = urllib.request.Request(node['rpc_url'], data=json.dumps(data).encode(),
                                                       headers={'Content-Type': 'application/json'})
                            with urllib.request.urlopen(req, timeout=5) as response:
                                result = json.loads(response.read().decode())
                                mining_active = result.get('result', {}).get('active', False)
                                print(f"  Mining Status: {'Active' if mining_active else 'Inactive'}")
                        except Exception as e:
                            print(f"  Mining Status: Error - {str(e)[:30]}...")

            except Exception as e:
                print(f"Node {node['id']} ({node['role']}): ✗ Error: {str(e)[:50]}...")

    def stop_nodes(self):
        """Stop all nodes."""
        print("Stopping nodes...")

        for node in self.nodes:
            if node['process'] and node['process'].poll() is None:
                print(f"Stopping node {node['id']}...")
                node['process'].terminate()
                try:
                    node['process'].wait(timeout=10)
                except subprocess.TimeoutExpired:
                    node['process'].kill()
                    node['process'].wait()

        print("✓ All nodes stopped")

    def cleanup(self):
        """Clean up resources."""
        print("Cleaning up...")
        self.stop_nodes()
        print("✓ Cleanup complete")

def signal_handler(signum, frame):
    """Handle shutdown signals."""
    print("\nReceived shutdown signal...")
    if 'setup' in globals():
        setup.running = False
        setup.cleanup()
    sys.exit(0)

def main():
    """Main setup function."""
    global setup

    parser = argparse.ArgumentParser(description="Monero 3-Node Private Testnet Setup")
    parser.add_argument('--start-mining', action='store_true', help='Enable mining on the miner node')
    parser.add_argument('--mining-address', default=MINING_ADDRESS, help='Mining wallet address')
    args = parser.parse_args()

    print("Monero 3-Node Private Testnet Setup")
    print("=" * 40)
    if args.start_mining:
        print(f"Mining Address: {args.mining_address}")
    print(f"Output Directory: {OUTPUT_DIR}")
    print()

    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    try:
        # Initialize setup
        setup = PrivateTestnetSetup(mining_address=args.mining_address if args.start_mining else None)
        setup.setup_environment()
        setup.start_nodes()
        setup.wait_for_nodes_ready()

        # Start monitoring
        setup.running = True
        setup.monitor_network()

    except Exception as e:
        print(f"❌ Setup failed: {e}")
        if 'setup' in globals():
            setup.cleanup()
        sys.exit(1)

    # Cleanup
    setup.cleanup()

if __name__ == "__main__":
    main()