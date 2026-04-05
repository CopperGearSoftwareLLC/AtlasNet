import os
import time
import pytest

from testcontainers.core.container import DockerContainer
from testcontainers.core.network import Network

VALKEY_IMAGE = "valkey/valkey:8"
RUNNER_IMAGE = "ubuntu:24.04"

NUM_NODES = 3
BASE_PORT = 7000
BASE_BUS_PORT = 17000


def exec_text(container, command):
    """
    Run a command in a container and always return:
      (exit_code: int, output: str)
    """
    exit_code, output = container.exec(["sh", "-lc", command])

    if isinstance(output, bytes):
        output = output.decode("utf-8", errors="replace")

    return exit_code, output


@pytest.fixture(scope="session")
def valkey_cluster():
    network = Network()
    network.create()

    containers = []
    addresses = []

    try:
        # -------------------------
        # 1. Start cluster nodes
        # -------------------------
        for i in range(NUM_NODES):
            port = BASE_PORT + i
            bus_port = BASE_BUS_PORT + i
            alias = f"node{i}"

            container = (
                DockerContainer(VALKEY_IMAGE)
                .with_name(f"valkey-node-{i}")
                .with_network(network)
                .with_network_aliases(alias)
                .with_command(
                    f"valkey-server "
                    f"--port {port} "
                    f"--cluster-enabled yes "
                    f"--cluster-config-file nodes.conf "
                    f"--cluster-node-timeout 5000 "
                    f"--appendonly yes "
                    f"--protected-mode no "
                    f"--bind 0.0.0.0 "
                    f"--cluster-announce-ip {alias} "
                    f"--cluster-announce-port {port} "
                    f"--cluster-announce-bus-port {bus_port}"
                )
            )

            container.start()
            containers.append(container)
            addresses.append(f"{alias}:{port}")

            print(f"[cluster] started {alias} -> {alias}:{port}")

        # -------------------------
        # 2. Give nodes time to boot
        # -------------------------
        time.sleep(5)

        # -------------------------
        # 3. Create the cluster
        # -------------------------
        print("[cluster] creating cluster...")

        create_cmd = (
            "valkey-cli --cluster create "
            + " ".join(addresses)
            + " --cluster-replicas 0 --cluster-yes"
        )

        exit_code, output = exec_text(containers[0], create_cmd)
        print(output)

        if exit_code != 0:
            raise RuntimeError(
                "Cluster creation failed.\n"
                f"Command: {create_cmd}\n"
                f"Output:\n{output}"
            )

        # -------------------------
        # 4. Wait until cluster is really ready
        # -------------------------
        print("[cluster] waiting for cluster to be ready...")

        for attempt in range(60):
            exit_code, output = exec_text(
                containers[0],
                f"valkey-cli -p {BASE_PORT} cluster info"
            )

            print(f"[cluster] readiness check {attempt + 1}/60")
            print(output)

            if (
                exit_code == 0
                and "cluster_state:ok" in output
                and f"cluster_known_nodes:{NUM_NODES}" in output
                and "cluster_slots_assigned:16384" in output
            ):
                print("[cluster] ready ✅")
                break

            time.sleep(1)
        else:
            # Extra debug info if readiness never happened
            exit_code_nodes, nodes_output = exec_text(
                containers[0],
                f"valkey-cli -p {BASE_PORT} cluster nodes"
            )

            raise RuntimeError(
                "Cluster did not become ready.\n\n"
                "Last cluster info output:\n"
                f"{output}\n\n"
                "cluster nodes output:\n"
                f"{nodes_output}"
            )

        # -------------------------
        # 5. Yield info for the test
        # -------------------------
        yield {
            "network": network,
            "containers": containers,
            "entry_host": "node0",
            "entry_port": BASE_PORT,
        }

    finally:
        for c in containers:
            try:
                c.exec(["sh", "-lc", "kill -9 1 || true"])
            except Exception:
                pass

        for c in containers:
            try:
                c.stop()
            except Exception as e:
                print(f"[cluster] warning stopping container: {e}")

        try:
            network.remove()
        except Exception as e:
            print(f"[cluster] warning removing network: {e}")


def test_redis_conn_cpp(valkey_cluster):
    native_binary = os.environ["NATIVE_BINARY"]
    native_binary = os.path.abspath(native_binary)

    if not os.path.exists(native_binary):
        raise FileNotFoundError(
            f"NATIVE_BINARY does not exist: {native_binary}"
        )

    runner = None

    try:
        # Start a container in the SAME docker network as the Valkey cluster.
        # The test binary is mounted into the runner container and executed there.
        runner = (
            DockerContainer(RUNNER_IMAGE)
            .with_network(valkey_cluster["network"])
            .with_volume_mapping(native_binary, "/opt/testbin", mode="ro")
            .with_command("sleep infinity")
        )

        runner.start()

        entry_host = valkey_cluster["entry_host"]
        entry_port = valkey_cluster["entry_port"]

        print(f"[runner] entry node: {entry_host}:{entry_port}")
        print(f"[runner] binary: /opt/testbin")

        # Make sure the binary is executable inside the container.
        exit_code, chmod_output = exec_text(runner, "chmod +x /opt/testbin || true")
        print(chmod_output)

        run_cmd = (
            f"VALKEY_HOST={entry_host} "
            f"VALKEY_PORT={entry_port} "
            f"VALKEY_CLUSTER=1 "
            f"/opt/testbin"
        )

        exit_code, output = exec_text(runner, run_cmd)

        print("[runner] program output:")
        print(output)

        assert exit_code == 0, (
            "ValkeyConnTest failed inside container.\n"
            f"Exit code: {exit_code}\n"
            f"Output:\n{output}"
        )

    finally:
        if runner is not None:
            try:
                runner.exec(["sh", "-lc", "kill -9 1 || true"])
            except Exception:
                pass
            try:
                runner.stop()
            except Exception as e:
                print(f"[runner] warning stopping runner: {e}")