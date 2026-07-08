// SPDX-License-Identifier: GPL-3.0+

import Foundation

enum ImportFileCopier {
    private static let streamBufferSize = 1024 * 1024

    static func copy(from sourceURL: URL, to destinationURL: URL) throws {
        do {
            try FileManager.default.copyItem(at: sourceURL, to: destinationURL)
        } catch {
            NSLog("[ARMSX2 iOS Import] copyItem failed for %@, retrying with streaming copy: %@",
                  sourceURL.lastPathComponent, error.localizedDescription)
            try copyByStreaming(from: sourceURL, to: destinationURL)
        }
    }

    private static func copyByStreaming(from sourceURL: URL, to destinationURL: URL) throws {
        let temporaryURL = streamingTemporaryURL(for: destinationURL)
        var sourceHandle: FileHandle?
        var destinationHandle: FileHandle?

        do {
            let source = try FileHandle(forReadingFrom: sourceURL)
            sourceHandle = source
            guard FileManager.default.createFile(atPath: temporaryURL.path, contents: nil) else {
                throw temporaryFileCreationError(at: temporaryURL)
            }
            let destination = try FileHandle(forWritingTo: temporaryURL)
            destinationHandle = destination

            while true {
                let chunk = try source.read(upToCount: streamBufferSize) ?? Data()
                if chunk.isEmpty {
                    break
                }
                try destination.write(contentsOf: chunk)
            }

            try destination.close()
            destinationHandle = nil
            try source.close()
            sourceHandle = nil

            try FileManager.default.moveItem(at: temporaryURL, to: destinationURL)
        } catch {
            try? destinationHandle?.close()
            try? sourceHandle?.close()
            try? FileManager.default.removeItem(at: temporaryURL)
            throw error
        }
    }

    private static func streamingTemporaryURL(for destinationURL: URL) -> URL {
        let fileName = destinationURL.lastPathComponent
        let temporaryName = ".\(fileName).import-\(UUID().uuidString).tmp"
        return destinationURL.deletingLastPathComponent().appendingPathComponent(temporaryName)
    }

    private static func temporaryFileCreationError(at url: URL) -> NSError {
        NSError(
            domain: NSCocoaErrorDomain,
            code: NSFileWriteUnknownError,
            userInfo: [NSFilePathErrorKey: url.path]
        )
    }
}
