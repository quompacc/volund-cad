#!/bin/sh
set -eu

# C01: one-time setup of the Gitea runner on the Fedora test host. Run as root:
#   sudo sh tools/ci/install-runner.sh
# Expects in /home/eduard/gitea-runner (prepared without root):
#   gitea-runner        verified release binary (gitea-runner v4.1.0)
#   config.yaml         copy of tools/ci/gitea-runner.yaml
#   gitea-runner.service copy of tools/ci/gitea-runner.service
#   hl-home-root-ca.pem homelab root CA used by https://gitea.hl.home.arpa
#   token               runner registration token (deleted after registration)
runner_user=eduard
runner_home=/home/eduard/gitea-runner
instance=https://gitea.hl.home.arpa

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root: sudo sh $0" >&2
    exit 1
fi

# actions/checkout and actions/upload-artifact run on the host's Node.js.
dnf install -y nodejs
install -m 0644 "$runner_home/hl-home-root-ca.pem" \
    /etc/pki/ca-trust/source/anchors/hl-home-root-ca.pem
update-ca-trust
install -m 0755 "$runner_home/gitea-runner" /usr/local/bin/gitea-runner
install -m 0644 "$runner_home/gitea-runner.service" /etc/systemd/system/gitea-runner.service

if [ ! -f "$runner_home/.runner" ]; then
    runuser -u "$runner_user" -- /usr/local/bin/gitea-runner register --no-interactive \
        --config "$runner_home/config.yaml" --instance "$instance" \
        --token-file "$runner_home/token" --name fedora-dev --labels linux-desktop:host
    rm -f "$runner_home/token"
fi

systemctl daemon-reload
systemctl enable --now gitea-runner.service
systemctl --no-pager --lines=5 status gitea-runner.service
