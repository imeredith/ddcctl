# ddcctl

A small Swift command-line app for switching monitor inputs over DDC/CI on Apple Silicon macOS.

## Build

```sh
swift build -c release
```

The binary will be at `.build/release/ddcctl`.

## Homebrew

This repository includes a Homebrew formula that installs the published release binary.

```sh
brew tap imeredith/ddcctl git@github.com:imeredith/ddcctl.git
brew install ddcctl
```

To install directly from the `main` branch instead:

```sh
brew install --HEAD ddcctl
```

## Release

Build the Homebrew release archive and checksum:

```sh
scripts/package-release.sh 0.1.0
```

Upload the generated archive to the matching GitHub release:

```sh
gh release create v0.1.0 dist/ddcctl-v0.1.0-macos-arm64.tar.gz
```

## Usage

```sh
ddcctl list
ddcctl input hdmi1
ddcctl input dp1 --display 0
ddcctl vcp 0x60 0x11 --display 1
```

Input source aliases:

- `dp1` / `displayport1`: `0x0f`
- `dp2` / `displayport2`: `0x10`
- `hdmi1`: `0x11`
- `hdmi2`: `0x12`
- `usbc` / `usb-c`: `0x1b`

The `input` command writes VCP feature `0x60`, the DDC/CI input-source control. Without `--display`, the command attempts the write on all detected Apple Silicon DDC display services.
