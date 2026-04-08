#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env
require_ssh_key

need_cmd ssh
need_cmd scp
need_cmd helm

bundle_root="$(bundle_dir)"
remote_root="$(remote_bundle_dir)"
SSH_KEY="$(ssh_key_path)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

[[ -f "$bundle_root/bundle-manifest.env" ]] || die "Missing offline bundle manifest. Run 'make offline-bundle' first."
[[ -f "$bundle_root/checksums.txt" ]] || die "Missing offline bundle checksums. Run 'make offline-bundle' first."

seed_node() {
  local role="$1"
  local user="$2"
  local host="$3"
  local raw_arch arch airgap_name remote_manifest registries_file

  raw_arch="$(ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" 'uname -m')"
  arch="$(normalize_arch "$raw_arch")"
  airgap_name="$(k3s_airgap_asset "$arch")"
  registries_file="$tmp_dir/registries-${host}.yaml"
  write_registries_yaml "$registries_file"

  [[ -f "$bundle_root/k3s/$arch/k3s" ]] || die "Missing k3s binary for ${arch} in bundle"
  [[ -f "$bundle_root/k3s/$arch/$airgap_name" ]] || die "Missing k3s airgap archive for ${arch} in bundle"

  echo "==> Preparing ${role} ${host} (${arch})"
  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "REMOTE_ROOT='$remote_root' REMOTE_USER='$user' bash -s" <<'REMOTE'
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
  echo "ERROR: apt-get is required on the target node for dependency setup" >&2
  exit 1
fi

sudo apt-get update -qq
sudo apt-get install -y curl iptables zstd >/dev/null

for cmdline in /boot/firmware/cmdline.txt /boot/cmdline.txt; do
  if [[ -f "$cmdline" ]]; then
    if ! grep -q 'cgroup_memory=1' "$cmdline" 2>/dev/null; then
      sudo sed -i 's/$/ cgroup_memory=1 cgroup_enable=memory/' "$cmdline"
    fi
    break
  fi
done

sudo mkdir -p "$REMOTE_ROOT/bin" /etc/rancher/k3s
sudo chown -R "$REMOTE_USER":"$REMOTE_USER" "$REMOTE_ROOT"
REMOTE

  scp -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    "$bundle_root/bundle-manifest.env" \
    "${user}@${host}:$remote_root/bundle-manifest.env"
  scp -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    "$bundle_root/checksums.txt" \
    "${user}@${host}:$remote_root/checksums.txt"
  scp -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    "$bundle_root/k3s/install.sh" \
    "${user}@${host}:$remote_root/install.sh"
  scp -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    "$bundle_root/k3s/$arch/k3s" \
    "${user}@${host}:$remote_root/bin/k3s"
  scp -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    "$bundle_root/k3s/$arch/$airgap_name" \
    "${user}@${host}:$remote_root/$airgap_name"
  scp -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    "$registries_file" \
    "${user}@${host}:$remote_root/registries.yaml"

  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "sudo install -m 0644 '$remote_root/registries.yaml' /etc/rancher/k3s/registries.yaml"

  remote_manifest="$remote_root/seeded-node.env"
  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "printf 'NODE_ARCH=%s\nSEEDED_AT=%s\nREGISTRY=%s\n' '$arch' '$(date -u +"%Y-%m-%dT%H:%M:%SZ")' '$(registry_advertise_hostport)' > '$remote_manifest'"
}

for_each_node seed_node

echo
echo "Dependency setup finished on all nodes."
echo "If kernel cmdline changed, reboot the nodes before running: make k3s-deploy"
