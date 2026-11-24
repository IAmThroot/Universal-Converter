#!/usr/bin/env bash
set -euo pipefail

packages=(cmake ninja-build qt6-base-dev libqt6svg6-dev qt6-qpa-plugins
          ffmpeg imagemagick librsvg2-bin uchardet curl tar xz-utils "$@")
apt_options=(-o Dir::Etc::sourceparts=- -o Acquire::Retries=1
             -o Acquire::http::Timeout=15 -o Acquire::https::Timeout=15
             -o Acquire::Languages=none)
fetch_packages() {
    local source_list=$1
    local options=("${apt_options[@]}" -o "Dir::Etc::sourcelist=$source_list")
    sudo timeout --kill-after=10s 2m apt-get "${options[@]}" --error-on=any update &&
        sudo timeout --kill-after=10s 3m apt-get "${options[@]}" install -y \
            --no-install-recommends --download-only "${packages[@]}"
}

# Use the runner's Ubuntu mirror without refreshing unrelated vendor repositories.
source_list=/etc/apt/sources.list.d/ubuntu.sources
if [[ ! -s "$source_list" ]] || ! fetch_packages "$source_list"; then
    echo "Using the Ubuntu archive and security mirrors for the dependency download."
    source_list="$RUNNER_TEMP/ubuntu-fallback.list"
    cat > "$source_list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] https://archive.ubuntu.com/ubuntu noble main universe restricted multiverse
deb [signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] https://archive.ubuntu.com/ubuntu noble-updates main universe restricted multiverse
deb [signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] https://security.ubuntu.com/ubuntu noble-security main universe restricted multiverse
SOURCES
    fetch_packages "$source_list"
fi

# Only network operations are timed. Let dpkg finish configuring the downloaded packages.
sudo apt-get "${apt_options[@]}" -o "Dir::Etc::sourcelist=$source_list" install -y \
    --no-install-recommends --no-download "${packages[@]}"
