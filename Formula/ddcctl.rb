class Ddcctl < Formula
  desc "Switch monitor inputs over DDC/CI on Apple Silicon macOS"
  homepage "https://github.com/imeredith/ddcctl"
  url "https://github.com/imeredith/ddcctl/releases/download/v0.1.0/ddcctl-v0.1.0-macos-arm64.tar.gz"
  sha256 "258ab85ef95e48e5799e02c8d37acd9013058d977ba5ac65757d91c5ce9bbea4"
  license "MIT"
  head do
    url "https://github.com/imeredith/ddcctl.git", branch: "main"

    depends_on xcode: ["16.0", :build]
  end

  depends_on arch: :arm64
  depends_on macos: :ventura

  def install
    if build.head?
      system "swift", "build", "-c", "release", "--disable-sandbox"
      bin.install ".build/release/ddcctl"
    else
      bin.install "ddcctl"
    end
  end

  test do
    assert_match "Usage:", shell_output("#{bin}/ddcctl --help", 64)
  end
end
