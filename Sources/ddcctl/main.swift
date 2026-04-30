import AppleSiliconDDC
import Foundation

let inputSources: [String: UInt16] = [
    "dp1": 0x0f,
    "displayport1": 0x0f,
    "dp2": 0x10,
    "displayport2": 0x10,
    "hdmi1": 0x11,
    "hdmi2": 0x12,
    "usbc": 0x1b,
    "usb-c": 0x1b
]

let listedInputSources: [(name: String, value: UInt16)] = [
    ("dp1", 0x0f),
    ("dp2", 0x10),
    ("hdmi1", 0x11),
    ("hdmi2", 0x12),
    ("usbc", 0x1b)
]

func usage() -> Never {
    print("""
    Usage:
      ddcctl list
      ddcctl input <source> [--display <index>]
      ddcctl vcp <control> <value> [--display <index>]

    Sources:
      dp1, dp2, hdmi1, hdmi2, usbc

    Examples:
      ddcctl input hdmi1
      ddcctl input dp1 --display 0
      ddcctl vcp 0x60 0x11 --display 1
    """)
    exit(64)
}

func parseNumber(_ text: String) -> UInt16? {
    if text.lowercased().hasPrefix("0x") {
        return UInt16(text.dropFirst(2), radix: 16)
    }
    return UInt16(text, radix: 10)
}

func parseDisplay(_ args: [String]) -> UInt32? {
    guard let optionIndex = args.firstIndex(of: "--display") else {
        return nil
    }
    let valueIndex = args.index(after: optionIndex)
    guard valueIndex < args.endIndex, let display = UInt32(args[valueIndex]) else {
        usage()
    }
    return display
}

func displays() -> [AppleSiliconDDC.IOregService] {
    AppleSiliconDDC.getIoregServicesForMatching().filter { display in
        display.service != nil
            && !(display.edidUUID.isEmpty && display.manufacturerID.isEmpty && display.transportUpstream.isEmpty && display.transportDownstream.isEmpty)
    }
}

func setVCP(control: UInt8, value: UInt16, display displayIndex: UInt32?) {
    let availableDisplays = displays()
    guard !availableDisplays.isEmpty else {
        fputs("no DDC-capable Apple Silicon display services found\n", stderr)
        exit(1)
    }

    let targets: [(Int, AppleSiliconDDC.IOregService)]
    if let displayIndex {
        guard displayIndex < availableDisplays.count else {
            fputs("display \(displayIndex) was not found\n", stderr)
            exit(1)
        }
        targets = [(Int(displayIndex), availableDisplays[Int(displayIndex)])]
    } else {
        targets = Array(availableDisplays.enumerated())
    }

    var successes = 0
    for (index, target) in targets {
        let ok = AppleSiliconDDC.write(service: target.service, command: control, value: value)
        if ok {
            successes += 1
            print("display \(index): wrote VCP 0x\(String(control, radix: 16)) = 0x\(String(value, radix: 16))")
        } else {
            fputs("display \(index): DDC write failed\n", stderr)
        }
    }

    if successes == 0 {
        exit(1)
    }
}

let args = Array(CommandLine.arguments.dropFirst())
guard let command = args.first else {
    usage()
}

switch command {
case "list":
    let availableDisplays = displays()
    if availableDisplays.isEmpty {
        print("no DDC-capable Apple Silicon display services found")
        exit(1)
    }

    for (index, display) in availableDisplays.enumerated() {
        let serial = display.alphanumericSerialNumber.isEmpty ? String(display.serialNumber) : display.alphanumericSerialNumber
        print("display \(index): \(display.manufacturerID) \(display.productName), serial \(serial)")
        print("  path: \(display.ioDisplayLocation)")
        print("  transport: \(display.transportUpstream) -> \(display.transportDownstream)")

        let currentInput = AppleSiliconDDC.read(service: display.service, command: 0x60)?.current
        let inputs = listedInputSources.map { source in
            let hexValue = "0x\(String(source.value, radix: 16))"
            if source.value == currentInput {
                return "\(source.name)=\(hexValue) current"
            }
            return "\(source.name)=\(hexValue)"
        }.joined(separator: ", ")
        print("  inputs: \(inputs)")
    }

case "input":
    guard args.count >= 2 else {
        usage()
    }
    let source = args[1].lowercased()
    guard let value = inputSources[source] else {
        fputs("unknown input source: \(args[1])\n", stderr)
        usage()
    }
    setVCP(control: 0x60, value: value, display: parseDisplay(args))

case "vcp":
    guard args.count >= 3,
          let controlValue = parseNumber(args[1]), controlValue <= UInt8.max,
          let value = parseNumber(args[2]) else {
        usage()
    }
    setVCP(control: UInt8(controlValue), value: value, display: parseDisplay(args))

default:
    usage()
}
