// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "ddcctl",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "ddcctl", targets: ["ddcctl"])
    ],
    dependencies: [
        .package(path: "AppleSiliconDDC")
    ],
    targets: [
        .target(
            name: "DDCBridge",
            publicHeadersPath: "include",
            linkerSettings: [
                .linkedFramework("CoreGraphics"),
                .linkedFramework("IOKit")
            ]
        ),
        .executableTarget(
            name: "ddcctl",
            dependencies: ["AppleSiliconDDC"]
        )
    ]
)
