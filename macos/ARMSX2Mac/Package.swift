// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "ARMSX2Mac",
    platforms: [
        .macOS(.v13),
    ],
    products: [
        .executable(name: "ARMSX2Mac", targets: ["ARMSX2Mac"]),
    ],
    targets: [
        .executableTarget(
            name: "ARMSX2Mac",
            path: "Sources/ARMSX2Mac"
        ),
    ]
)
