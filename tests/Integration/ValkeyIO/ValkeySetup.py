import pytest
import subprocess
import os
from testcontainers.core.container import DockerContainer
from testcontainers.core.waiting_utils import wait_for_logs

@pytest.fixture(scope="session")
def valkey_container():
    with DockerContainer("valkey/valkey:8") \
        .with_exposed_ports(6379) as container:
        wait_for_logs(container, "Ready to accept connections", timeout=30)
        yield container

def test_redis_conn_cpp(valkey_container):
    host = valkey_container.get_container_host_ip()
    port = valkey_container.get_exposed_port(6379)
    
    print(f"Valkey container running at {host}:{port}")
    print("Running C++ client test...", os.environ["NATIVE_BINARY"])
    
    result = subprocess.run(
        [os.environ["NATIVE_BINARY"]],  # your compiled C++ binary
        env={
            **os.environ,
            "VALKEY_HOST": str(host),
            "VALKEY_PORT": str(port),
        },
        capture_output=True,
        text=True
    )

    print(result.stdout)
    print(result.stderr)
    assert result.returncode == 0