import os
import time
from dataclasses import dataclass

import pytest
from testcontainers.core.container import DockerContainer


IMAGE = "ubuntu:24.04"
SERVER_STARTUP_SECONDS = float(os.environ.get("SERVER_STARTUP_SECONDS", "1.0"))
PROCESS_TIMEOUT_SECONDS = int(os.environ.get("PROCESS_TIMEOUT_SECONDS", "60"))


@dataclass
class ContainerRunResult:
    name: str
    exit_code: int | None
    logs: str


def _require_env(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise RuntimeError(f"Missing required environment variable: {name}")
    return os.path.abspath(value)


def _container_binary_path(host_binary: str, mount_dir: str) -> str:
    return f"{mount_dir}/{os.path.basename(host_binary)}"


def _start_binary_container(
    *,
    image: str,
    host_binary: str,
    mount_dir: str,
    test_num: int,
    name: str,
) -> DockerContainer:
    container_binary = _container_binary_path(host_binary, mount_dir)

    container = (
        DockerContainer(image) 
        .with_volume_mapping(
            os.path.dirname(host_binary),
            mount_dir,
            mode="ro",
        )
        .with_command(
            f'/bin/sh -lc " '
            f'\'{container_binary}\' --test-num {test_num}"'
        )
    )

    container.start()
    return container


def _wait_for_container(container: DockerContainer, timeout: int) -> int:
    wrapped = container.get_wrapped_container()
    result = wrapped.wait(timeout=timeout)
    return int(result["StatusCode"])


def _read_logs(container: DockerContainer) -> str:
    wrapped = container.get_wrapped_container()
    raw = wrapped.logs(stdout=True, stderr=True)
    if isinstance(raw, bytes):
        return raw.decode("utf-8", errors="replace")
    return str(raw)


def _stop_container_quietly(container: DockerContainer | None) -> None:
    if container is None:
        return
    try:
        container.stop()
    except Exception:
        pass


def _run_server_client_test(test_num: int) -> tuple[ContainerRunResult, ContainerRunResult]:
    server_binary = _require_env("SERVER_BINARY")
    client_binary = _require_env("CLIENT_BINARY")

    server_container = None
    client_container = None

    server_result = ContainerRunResult(name="Server", exit_code=None, logs="")
    client_result = ContainerRunResult(name="Client", exit_code=None, logs="")

    try:
        server_container = _start_binary_container(
            image=IMAGE,
            host_binary=server_binary,
            mount_dir="/opt/serverbin",
            test_num=test_num,
            name="Server",
        )

        time.sleep(SERVER_STARTUP_SECONDS)

        client_container = _start_binary_container(
            image=IMAGE,
            host_binary=client_binary,
            mount_dir="/opt/clientbin",
            test_num=test_num,
            name="Client",
        )

        client_result.exit_code = _wait_for_container(client_container, PROCESS_TIMEOUT_SECONDS)
        client_result.logs = _read_logs(client_container)

        server_result.exit_code = _wait_for_container(server_container, PROCESS_TIMEOUT_SECONDS)
        server_result.logs = _read_logs(server_container)

        return server_result, client_result

    finally:
        if client_container is not None and not client_result.logs:
            try:
                client_result.logs = _read_logs(client_container)
            except Exception:
                pass

        if server_container is not None and not server_result.logs:
            try:
                server_result.logs = _read_logs(server_container)
            except Exception:
                pass

        _stop_container_quietly(client_container)
        _stop_container_quietly(server_container)


def _assert_results(test_num: int, server_result: ContainerRunResult, client_result: ContainerRunResult) -> None:
    print(f"\n===== TEST {test_num} SERVER OUTPUT =====\n{server_result.logs}")
    print(f"\n===== TEST {test_num} CLIENT OUTPUT =====\n{client_result.logs}")

    assert server_result.exit_code == 0, (
        f"Server failed for test {test_num} with exit code {server_result.exit_code}\n"
        f"--- Server logs ---\n{server_result.logs}"
    )
    assert client_result.exit_code == 0, (
        f"Client failed for test {test_num} with exit code {client_result.exit_code}\n"
        f"--- Client logs ---\n{client_result.logs}"
    )


def test_message_0():
    server_result, client_result = _run_server_client_test(0)
    _assert_results(0, server_result, client_result)


def test_message_1():
    server_result, client_result = _run_server_client_test(1)
    _assert_results(1, server_result, client_result)


def test_message_2():
    server_result, client_result = _run_server_client_test(2)
    _assert_results(2, server_result, client_result)