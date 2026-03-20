SHELL := /usr/bin/env bash

.PHONY: help deps vcpkg configure build-core build-images verify-images docker-login push-images ci print-summary all

ATLASNET_ROOT := $(abspath ../..)
BUILD_DIR ?= $(ATLASNET_ROOT)/build-arm64-codebuild
VCPKG_VERSION ?= 2026.01.16
VCPKG_ROOT ?= /opt/vcpkg/$(VCPKG_VERSION)
BUILDKITD_CONFIG ?= $(ATLASNET_ROOT)/AtlasNet/docker/buildkitd.pi.toml
CMAKE_BUILD_TYPE ?= Release
JOBS ?= 4

DOCKERHUB_NAMESPACE ?=
DOCKERHUB_USERNAME ?=
DOCKERHUB_TOKEN ?=
ATLASNET_IMAGE_TAG ?= latest
ATLASNET_IMAGE_TAG_AMD64 ?= latest-amd64
ATLASNET_IMAGE_TAG_ARM64 ?= latest-arm64
ATLASNET_WATCHDOG_IMAGE ?= $(DOCKERHUB_NAMESPACE)/watchdog:$(ATLASNET_IMAGE_TAG_ARM64)
ATLASNET_PROXY_IMAGE ?= $(DOCKERHUB_NAMESPACE)/proxy:$(ATLASNET_IMAGE_TAG_ARM64)
ATLASNET_CARTOGRAPH_IMAGE ?= $(DOCKERHUB_NAMESPACE)/cartograph:$(ATLASNET_IMAGE_TAG_ARM64)
ATLASNET_SANDBOX_SERVER_IMAGE ?= $(DOCKERHUB_NAMESPACE)/sandbox-server:$(ATLASNET_IMAGE_TAG_ARM64)

APT_PACKAGES := \
	build-essential clang clangd \
	cmake ninja-build pkg-config ccache \
	git curl wget unzip zip tar \
	python3 python3-pip \
	uuid-dev libuv1-dev libssl-dev zlib1g-dev linux-libc-dev \
	protobuf-compiler libprotobuf-dev \
	autoconf automake libtool m4 \
	libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxmu-dev \
	libgl-dev libglu1-mesa-dev libglm-dev \
	nlohmann-json3-dev libcurl4-openssl-dev \
	swig nodejs npm docker.io

DNF_PACKAGES := \
	gcc gcc-c++ clang \
	cmake ninja-build pkgconf-pkg-config ccache \
	git curl wget unzip zip tar \
	python3 python3-pip \
	libuuid-devel libuv-devel openssl-devel zlib-devel kernel-headers \
	protobuf-compiler protobuf-devel \
	autoconf automake libtool m4 \
	libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libXmu-devel \
	mesa-libGL-devel mesa-libGLU-devel \
	libcurl-devel swig nodejs npm docker

YUM_PACKAGES := \
	gcc gcc-c++ clang \
	cmake3 ninja-build pkgconfig ccache \
	git curl wget unzip zip tar \
	python3 python3-pip \
	libuuid-devel libuv-devel openssl-devel zlib-devel kernel-headers \
	protobuf-compiler protobuf-devel \
	autoconf automake libtool m4 \
	libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libXmu-devel \
	mesa-libGL-devel mesa-libGLU-devel \
	libcurl-devel swig

ifeq ($(shell id -u),0)
SUDO :=
else
SUDO := sudo
endif

all: build-images

help:
	@printf '%s\n' \
	  'AtlasNet native ARM64 builder (legacy path: k8s/pi-native).' \
	  '' \
	  'Common targets:' \
	  '  make deps          # install host packages used by local ARM64/CodeBuild builds' \
	  '  make configure     # configure the ARM64 CMake build tree' \
	  '  make build-images  # build watchdog/proxy/cartograph/sandbox-server Docker images' \
	  '  make push-images   # tag and push ARM64 images to Docker Hub' \
	  '  make ci            # full CodeBuild flow: build, login, push, summary'

deps:
	@echo "==> Installing system packages required for native ARM64 builds..."
	@if command -v apt-get >/dev/null 2>&1; then \
	  echo "==> Detected apt-based environment."; \
	  $(SUDO) apt-get update; \
	  $(SUDO) apt-get install -y --no-install-recommends $(APT_PACKAGES); \
	  if ! docker buildx version >/dev/null 2>&1; then \
	    echo "==> Installing Docker buildx plugin..."; \
	    $(SUDO) apt-get install -y docker-buildx || $(SUDO) apt-get install -y docker-buildx-plugin; \
	  fi; \
	  $(SUDO) rm -rf /var/lib/apt/lists/*; \
	elif command -v dnf >/dev/null 2>&1; then \
	  echo "==> Detected dnf-based environment."; \
	  $(SUDO) dnf install -y $(DNF_PACKAGES); \
	  $(SUDO) dnf install -y glm-devel nlohmann-json-devel || true; \
	  $(SUDO) systemctl enable docker >/dev/null 2>&1 || true; \
	  $(SUDO) systemctl start docker >/dev/null 2>&1 || true; \
	  if ! docker buildx version >/dev/null 2>&1; then \
	    echo "==> Docker buildx not reported separately; continuing with the Docker CLI plugin bundled in the environment."; \
	  fi; \
	  $(SUDO) dnf clean all; \
	elif command -v yum >/dev/null 2>&1; then \
	  echo "==> Detected yum-based environment (Amazon Linux 2 style)."; \
	  $(SUDO) yum install -y $(YUM_PACKAGES); \
	  $(SUDO) yum install -y nodejs npm || true; \
	  if command -v amazon-linux-extras >/dev/null 2>&1 && ! command -v npm >/dev/null 2>&1; then \
	    $(SUDO) amazon-linux-extras install -y nodejs || true; \
	  fi; \
	  if ! command -v cmake >/dev/null 2>&1 && command -v cmake3 >/dev/null 2>&1; then \
	    echo "==> Creating cmake shim for cmake3..."; \
	    $(SUDO) ln -sf "$$(command -v cmake3)" /usr/local/bin/cmake; \
	  fi; \
	  if ! command -v ninja >/dev/null 2>&1 && command -v ninja-build >/dev/null 2>&1; then \
	    echo "==> Creating ninja shim for ninja-build..."; \
	    $(SUDO) ln -sf "$$(command -v ninja-build)" /usr/local/bin/ninja; \
	  fi; \
	  $(SUDO) systemctl enable docker >/dev/null 2>&1 || true; \
	  $(SUDO) systemctl start docker >/dev/null 2>&1 || true; \
	  $(SUDO) yum clean all; \
	else \
	  echo "ERROR: Unsupported package manager. Expected apt-get, dnf, or yum."; \
	  exit 1; \
	fi
	@echo "==> System dependencies installed."

vcpkg:
	@echo "==> Ensuring pinned vcpkg $(VCPKG_VERSION) exists at $(VCPKG_ROOT)..."
	@VCPKG_PARENT_DIR="$$(dirname "$(VCPKG_ROOT)")"; \
	$(SUDO) mkdir -p "$$VCPKG_PARENT_DIR"; \
	$(SUDO) chown "$$USER":"$$(id -gn)" "$$VCPKG_PARENT_DIR"; \
	if [ ! -f "$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake" ]; then \
	  TMP_DIR="$$(mktemp -d)"; \
	  trap 'rm -rf "$$TMP_DIR"' EXIT; \
	  echo "==> Downloading vcpkg $(VCPKG_VERSION)..."; \
	  curl -fsSL "https://github.com/microsoft/vcpkg/archive/refs/tags/$(VCPKG_VERSION).tar.gz" \
	    | tar -xz --strip-components=1 -C "$$TMP_DIR"; \
	  $(SUDO) rm -rf "$(VCPKG_ROOT)"; \
	  $(SUDO) mkdir -p "$(VCPKG_ROOT)"; \
	  $(SUDO) cp -a "$$TMP_DIR"/. "$(VCPKG_ROOT)/"; \
	  $(SUDO) chown -R "$$USER":"$$(id -gn)" "$(VCPKG_ROOT)"; \
	  trap - EXIT; \
	  rm -rf "$$TMP_DIR"; \
	fi
	cd "$(VCPKG_ROOT)" && ./bootstrap-vcpkg.sh
	@echo "==> vcpkg $(VCPKG_VERSION) bootstrapped at $(VCPKG_ROOT)."

configure: deps vcpkg
	@echo "==> Configuring ARM64 CMake build in $(BUILD_DIR)..."
	@NINJA_BIN="$$(command -v ninja 2>/dev/null || command -v ninja-build 2>/dev/null || true)"; \
	test -n "$$NINJA_BIN" || { echo "ERROR: Ninja was not found after dependency installation."; exit 1; }; \
	mkdir -p "$(BUILD_DIR)"; \
	cd "$(BUILD_DIR)" && cmake \
	  -G Ninja \
	  -DCMAKE_MAKE_PROGRAM="$$NINJA_BIN" \
	  -DCMAKE_BUILD_TYPE="$(CMAKE_BUILD_TYPE)" \
	  -DCMAKE_TOOLCHAIN_FILE="$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake" \
	  -DVCPKG_MANIFEST_DIR="$(ATLASNET_ROOT)/AtlasNet" \
	  -DVCPKG_TARGET_TRIPLET=arm64-linux \
	  -DSANDBOX_ADD_CLIENT=ON \
	  -DCMAKE_C_COMPILER=clang \
	  -DCMAKE_CXX_COMPILER=clang++ \
	  "$(ATLASNET_ROOT)" || { \
	    LOG_FILE="$(BUILD_DIR)/vcpkg-manifest-install.log"; \
	    echo "==> CMake configure failed."; \
	    if [ -f "$$LOG_FILE" ]; then \
	      echo "==> Tail of $$LOG_FILE"; \
	      tail -n 240 "$$LOG_FILE"; \
	      echo "==> End tail of $$LOG_FILE"; \
	    else \
	      echo "==> vcpkg manifest log not found at $$LOG_FILE"; \
	    fi; \
	    for EXTRA_LOG in \
	      "$(BUILD_DIR)/CMakeFiles/CMakeError.log" \
	      "$(BUILD_DIR)/CMakeFiles/CMakeOutput.log" \
	      "$(BUILD_DIR)/CMakeFiles/CMakeConfigureLog.yaml"; do \
	      if [ -f "$$EXTRA_LOG" ]; then \
	        echo "==> Tail of $$EXTRA_LOG"; \
	        tail -n 120 "$$EXTRA_LOG"; \
	        echo "==> End tail of $$EXTRA_LOG"; \
	      fi; \
	    done; \
	    exit 1; \
	  }
	@echo "==> CMake configure completed for ARM64."

build-core: configure
	@echo "==> Building native ARM64 binaries required by the Docker image targets..."
	cmake --build "$(BUILD_DIR)" --target watchdog proxy Web_swig SandboxServer -- -j"$(JOBS)"
	@echo "==> Core runtime binaries built."

build-images: configure
	@echo "==> Building ARM64 Docker images via CMake targets AtlasnetDockerBuild_Fast + SandboxServerDockerBuild..."
	BUILDKITD_CONFIG="$(BUILDKITD_CONFIG)" cmake --build "$(BUILD_DIR)" --target AtlasnetDockerBuild_Fast SandboxServerDockerBuild -- -j"$(JOBS)"
	@echo "==> ARM64 Docker images built: watchdog, proxy, cartograph, sandbox-server."

verify-images:
	@echo "==> Verifying expected local Docker images exist..."
	docker image inspect watchdog:latest >/dev/null
	docker image inspect proxy:latest >/dev/null
	docker image inspect cartograph:latest >/dev/null
	docker image inspect sandbox-server:latest >/dev/null
	@echo "==> Verified local Docker images are present."

docker-login:
	@test -n "$(DOCKERHUB_USERNAME)" || { echo "ERROR: DOCKERHUB_USERNAME is required."; exit 1; }
	@test -n "$(DOCKERHUB_TOKEN)" || { echo "ERROR: DOCKERHUB_TOKEN is required."; exit 1; }
	@echo "==> Logging into Docker Hub as $(DOCKERHUB_USERNAME)..."
	@printf '%s' "$(DOCKERHUB_TOKEN)" | docker login --username "$(DOCKERHUB_USERNAME)" --password-stdin

push-images: verify-images
	@test -n "$(DOCKERHUB_NAMESPACE)" || { echo "ERROR: DOCKERHUB_NAMESPACE is required."; exit 1; }
	@echo "==> Tagging ARM64 images for namespace $(DOCKERHUB_NAMESPACE)..."
	docker tag watchdog:latest "$(ATLASNET_WATCHDOG_IMAGE)"
	docker tag proxy:latest "$(ATLASNET_PROXY_IMAGE)"
	docker tag cartograph:latest "$(ATLASNET_CARTOGRAPH_IMAGE)"
	docker tag sandbox-server:latest "$(ATLASNET_SANDBOX_SERVER_IMAGE)"
	@echo "==> Pushing ARM64 images to Docker Hub..."
	docker push "$(ATLASNET_WATCHDOG_IMAGE)"
	docker push "$(ATLASNET_PROXY_IMAGE)"
	docker push "$(ATLASNET_CARTOGRAPH_IMAGE)"
	docker push "$(ATLASNET_SANDBOX_SERVER_IMAGE)"
	@echo "==> Done. Published ARM64 images."

print-summary:
	@printf '%s\n' \
	  '' \
	  'Published ARM64 images:' \
	  "  - $(ATLASNET_WATCHDOG_IMAGE)" \
	  "  - $(ATLASNET_PROXY_IMAGE)" \
	  "  - $(ATLASNET_CARTOGRAPH_IMAGE)" \
	  "  - $(ATLASNET_SANDBOX_SERVER_IMAGE)" \
	  '' \
	  'Next step after amd64 images exist:' \
	  '  make -C k8s/k3s atlasnet-merge-manifests'

ci: build-images verify-images docker-login push-images print-summary
