#!/bin/bash
set -e

# ==============================
# Configuration
# ==============================
DOCKER_PORT=2375           # default Docker TCP port (unencrypted)
DOCKER_IP="0.0.0.0"        # listen on all interfaces
SERVICE_FILE="/lib/systemd/system/docker.service"
OVERRIDE_DIR="/etc/systemd/system/docker.service.d"
OVERRIDE_FILE="$OVERRIDE_DIR/override.conf"

# ==============================
# Prerequisites
# ==============================
if ! command -v docker >/dev/null 2>&1; then
    echo "🐳 Docker not found. Installing..."
    curl -fsSL https://get.docker.com | sh
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "🔧 jq not found. Installing..."
    sudo apt-get update -y
    sudo apt-get install -y jq
fi

# ==============================
# Configure Docker daemon
# ==============================
echo "Configuring Docker to listen on TCP $DOCKER_IP:$DOCKER_PORT ..."

sudo mkdir -p "$OVERRIDE_DIR"
cat <<EOF | sudo tee "$OVERRIDE_FILE" >/dev/null
[Service]
ExecStart=
ExecStart=/usr/bin/dockerd -H unix:///var/run/docker.sock -H tcp://$DOCKER_IP:$DOCKER_PORT
EOF

# ==============================
# Apply and verify
# ==============================
echo "Reloading systemd and restarting Docker..."
sudo systemctl daemon-reload
sudo systemctl restart docker
sudo systemctl enable docker

# ==============================
# Verify listener
# ==============================
echo "Verifying Docker socket and TCP listener..."
sudo ss -tuln | grep $DOCKER_PORT || echo "⚠️ Could not verify TCP listener. Check logs with: sudo journalctl -u docker"

echo
echo "✅ Docker remote API should now be available at:"
hostname -I | awk '{print "   tcp://" $1 ":2375"}'

echo
echo "Test remotely from another machine with:"
echo "   curl http://<this_machine_ip>:2375/version"

# ==============================
# Enable user to use docker
# ==============================
sudo usermod -aG docker $USER
newgrp docker

# ==============================
# Enable root SSH login
# ==============================
#echo "Enabling root SSH login..."
#sudo passwd -u root 2>/dev/null || true
#sudo sed -i 's/^#\?PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config
#sudo systemctl restart ssh || sudo systemctl restart sshd
#echo "✅ Root SSH login enabled."
