import pytest
import time
import subprocess

from testcontainers.core.container import DockerContainer
from testcontainers.core.network import Network

VALKEY_IMAGE = "valkey/valkey:8"
NUM_NODES = 3

BASE_PORT = 7000
BASE_BUS_PORT = 17000


@pytest.fixture(scope="session")
def valkey_cluster():
    network = Network()
    network.create()

    containers = []
    addresses = []

    # -------------------------
    # 1. Start nodes
    # -------------------------
    for i in range(NUM_NODES):
        port = BASE_PORT + i
        bus_port = BASE_BUS_PORT + i

        container = (
            DockerContainer(VALKEY_IMAGE)
            .with_name(f"valkey-node-{i}")
            .with_network(network)
            .with_network_aliases(f"node{i}")
            .with_bind_ports(port, port)
            .with_bind_ports(bus_port, bus_port)
            .with_command(
                f"valkey-server "
                f"--port {port} "
                "--cluster-enabled yes "
                "--cluster-config-file nodes.conf "
                "--cluster-node-timeout 5000 "
                "--appendonly yes "
                "--protected-mode no "
                "--bind 0.0.0.0 "
                f"--cluster-announce-ip 127.0.0.1 "
                f"--cluster-announce-port {port} "
                f"--cluster-announce-bus-port {bus_port}"
            )
        )

        container.start()
        containers.append(container)

        addr = f"127.0.0.1:{port}"
        addresses.append(addr)

        print(f"[cluster] node{i} → {addr}")

    # -------------------------
    # 2. Wait for nodes to boot
    # -------------------------
    time.sleep(3)

    # -------------------------
    # 3. Create cluster
    # -------------------------
    print("[cluster] creating cluster...")

    create_cmd = [
        "valkey-cli",
        "--cluster",
        "create",
        *addresses,
        "--cluster-replicas",
        "0",
        "--cluster-yes",
    ]

    # Run from first container
    exit_code, output = containers[0].exec(" ".join(create_cmd))
    print(output)

    if exit_code != 0:
        raise RuntimeError("Cluster creation failed")

    # -------------------------
    # 4. Wait until cluster is ready
    # -------------------------
    print("[cluster] waiting for cluster to be ready...")

    for _ in range(20):
        exit_code, output = containers[0].exec(
            f"valkey-cli -p {BASE_PORT} cluster info"
        )

        if "cluster_state:ok" in output:
            print("[cluster] ready ✅")
            break

        time.sleep(1)
    else:
        raise RuntimeError("Cluster did not become ready")

    # -------------------------
    # 5. Yield connection info
    # -------------------------
    yield {
        "hosts": ["127.0.0.1"],
        "ports": [BASE_PORT + i for i in range(NUM_NODES)],
    }

    # -------------------------
    # 6. Teardown
    # -------------------------
    for c in containers:
        c.stop()

    network.remove()


def test_redis_conn_cpp(valkey_cluster):
    # Use first node mapped to host
    container = valkey_cluster[0]

    host = container.get_container_host_ip()
    port = container.get_exposed_port(6379)

    print(f"Cluster entry: {host}:{port}")
    print("Running C++ client test...", os.environ["NATIVE_BINARY"])
    result = subprocess.run(
        [os.environ["NATIVE_BINARY"]],
        env={
            **os.environ,
            "VALKEY_HOST": str(host),
            "VALKEY_PORT": str(port),
            "VALKEY_CLUSTER": "1",
        },
        capture_output=True,
        text=True
    )

    print(result.stdout)
    print(result.stderr)

    assert result.returncode == 0, (
        f"ValkeyConnTest failed!\n"
        f"STDOUT:\n{result.stdout}\n"
        f"STDERR:\n{result.stderr}"
    )