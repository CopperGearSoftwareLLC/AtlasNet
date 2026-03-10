#
# Native arm64 (Pi) build helper for AtlasNet
#
# Usage on the Pi:
#   cd k8s/pi-native
#   make deps           # (optional) install build deps via apt
#   make vcpkg          # clone + bootstrap vcpkg into /opt/vcpkg
#   make configure      # configure CMake for arm64 with vcpkg
#   make fix-docker-dns # buildx container DNS needs fixing
#   make build-core     # build core AtlasNet runtimes
#   make build-docker   # (optional) build local Docker images via CopyDockerfile
#	make push-docker    # push docker images to docker hub

.PHONY: deps vcpkg configure build-core build-docker push-docker fix-docker-dns all

# Adjust if you prefer a different install location on the Pi.
VCPKG_ROOT ?= /opt/vcpkg

# AtlasNet repo root is two levels up from this directory.
ATLASNET_ROOT := $(abspath ../..)

# Separate build dir for Pi native builds (keeps devcontainer build tree separate).
BUILD_DIR ?= $(ATLASNET_ROOT)/build-pi

# Docker Hub / image publishing settings (edit these or override via env)
# On the Pi we default to pushing the ARM64-specific tag so it won't overwrite the multi-arch deploy tag.
DOCKERHUB_NAMESPACE ?=
ATLASNET_IMAGE_TAG ?= latest-arm64

all: build-core

deps:
	@echo "==> Installing system build dependencies + Docker (requires sudo)..."
	sudo apt-get update
	sudo apt-get install -y \
	  build-essential clang clangd \
	  cmake ninja-build pkg-config \
	  git curl wget unzip zip \
	  python3 python3-pip \
	  uuid-dev libuv1-dev libssl-dev zlib1g-dev \
	  protobuf-compiler libprotobuf-dev \
	  autoconf automake libtool m4 \
	  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
	  swig nodejs npm \
	  docker.io
	@echo "==> Ensuring current user is in 'docker' group (may require re-login)..."
	sudo usermod -aG docker "$$USER" || true
	@echo "==> System dependencies + Docker installed (or already present)."

fix-docker-dns:
	@echo "==> Configuring Docker daemon DNS to use public resolvers (requires sudo)..."
	sudo mkdir -p /etc/docker
	printf '%s\n' '{' '"dns": ["1.1.1.1", "8.8.8.8"]' '}' | sed '2s/^/  /' | sudo tee /etc/docker/daemon.json >/dev/null
	sudo systemctl restart docker
	@echo "==> Docker daemon restarted with updated DNS settings."
	@echo "==> Removing atlasnet-builder buildx builder so it can be recreated with new DNS..."
	docker buildx rm atlasnet-builder >/dev/null 2>&1 || true
	@echo "==> Done. Next build will recreate the buildx builder with updated DNS."

vcpkg:
	@echo "==> Ensuring vcpkg exists at $(VCPKG_ROOT)..."
	if [ ! -d "$(VCPKG_ROOT)" ]; then \
	  sudo mkdir -p "$(VCPKG_ROOT)"; \
	  sudo chown "$$USER":"$$USER" "$(VCPKG_ROOT)"; \
	fi
	if [ ! -d "$(VCPKG_ROOT)/.git" ]; then \
	  git clone https://github.com/microsoft/vcpkg.git "$(VCPKG_ROOT)"; \
	fi
	cd "$(VCPKG_ROOT)" && ./bootstrap-vcpkg.sh
	@echo "==> vcpkg bootstrapped at $(VCPKG_ROOT)."

configure: deps vcpkg
	@echo "==> Configuring CMake for native arm64 build in $(BUILD_DIR)..."
	mkdir -p "$(BUILD_DIR)"
	cd "$(BUILD_DIR)" && cmake \
	  -G Ninja \
	  -DCMAKE_TOOLCHAIN_FILE="$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake" \
	  -DVCPKG_MANIFEST_DIR="$(ATLASNET_ROOT)/AtlasNet" \
	  -DVCPKG_TARGET_TRIPLET=arm64-linux \
	  -DSANDBOX_ADD_CLIENT=ON \
	  -DCMAKE_C_COMPILER=clang \
	  -DCMAKE_CXX_COMPILER=clang++ \
	  "$(ATLASNET_ROOT)"
	@echo "==> CMake configure completed for arm64."

build-core: configure
	@echo "==> Building core AtlasNet runtimes for arm64 (this may take a while on a Pi 5)..."
	cmake --build "$(BUILD_DIR)" --target watchdog proxy shard -- -j2
	@echo "==> Core AtlasNet targets built (watchdog, proxy, shard)."

build-docker: build-core
	@echo "==> Staging binaries and building local Docker images (arm64) via CMake target AtlasnetDockerBuild_Fast..."
	BUILDKITD_CONFIG="$(ATLASNET_ROOT)/AtlasNet/docker/buildkitd.pi.toml" cmake --build "$(BUILD_DIR)" --target AtlasnetDockerBuild_Fast -- -j2
	@echo "==> Docker images built using CopyDockerfile (arm64 local images: watchdog, proxy, shard, cartograph)."

push-docker:
	@echo "==> Preparing to push arm64 images to Docker Hub..."
	@if [ -z "$(DOCKERHUB_NAMESPACE)" ]; then \
	  echo "ERROR: DOCKERHUB_NAMESPACE must be set in the environment (e.g. your Docker Hub username)."; \
	  exit 1; \
	fi
	@if [ -z "$(ATLASNET_IMAGE_TAG)" ]; then \
	  echo "ERROR: ATLASNET_IMAGE_TAG must be set in the environment (e.g. latest or v0.1.0)."; \
	  exit 1; \
	fi
	@echo "==> Using existing Docker credentials (make sure you are logged in with 'docker login')."
	@echo "==> Tagging local images for $(DOCKERHUB_NAMESPACE) with tag $(ATLASNET_IMAGE_TAG) ..."
	docker tag watchdog:latest   "$(DOCKERHUB_NAMESPACE)/watchdog:$(ATLASNET_IMAGE_TAG)"
	docker tag proxy:latest      "$(DOCKERHUB_NAMESPACE)/proxy:$(ATLASNET_IMAGE_TAG)"
	docker tag shard:latest      "$(DOCKERHUB_NAMESPACE)/shard:$(ATLASNET_IMAGE_TAG)"
	docker tag cartograph:latest "$(DOCKERHUB_NAMESPACE)/cartograph:$(ATLASNET_IMAGE_TAG)"
	@echo "==> Pushing images to Docker Hub..."
	docker push "$(DOCKERHUB_NAMESPACE)/watchdog:$(ATLASNET_IMAGE_TAG)"
	docker push "$(DOCKERHUB_NAMESPACE)/proxy:$(ATLASNET_IMAGE_TAG)"
	docker push "$(DOCKERHUB_NAMESPACE)/shard:$(ATLASNET_IMAGE_TAG)"
	docker push "$(DOCKERHUB_NAMESPACE)/cartograph:$(ATLASNET_IMAGE_TAG)"
	@echo "==> Done. Published arm64 images to Docker Hub namespace '$(DOCKERHUB_NAMESPACE)' with tag '$(ATLASNET_IMAGE_TAG)'."
