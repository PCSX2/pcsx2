// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI
import UniformTypeIdentifiers
import UIKit

struct ImportDocumentPicker: UIViewControllerRepresentable {
	let allowedContentTypes: [UTType]
	let allowsMultipleSelection: Bool
	let legacyDocumentTypes: [String]?
	let legacyDocumentMode: UIDocumentPickerMode
	let asCopy: Bool
	let onComplete: (Result<[URL], Error>) -> Void

	init(
		allowedContentTypes: [UTType],
		allowsMultipleSelection: Bool,
		legacyDocumentTypes: [String]? = nil,
		legacyDocumentMode: UIDocumentPickerMode = .import,
		asCopy: Bool = true,
		onComplete: @escaping (Result<[URL], Error>) -> Void
	) {
		self.allowedContentTypes = allowedContentTypes
		self.allowsMultipleSelection = allowsMultipleSelection
		self.legacyDocumentTypes = legacyDocumentTypes
		self.legacyDocumentMode = legacyDocumentMode
		self.asCopy = asCopy
		self.onComplete = onComplete
	}

    func makeCoordinator() -> Coordinator {
        Coordinator(onComplete: onComplete)
    }

    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
		let picker: UIDocumentPickerViewController
		if let legacyDocumentTypes {
			picker = UIDocumentPickerViewController(documentTypes: legacyDocumentTypes, in: legacyDocumentMode)
			let modeName = legacyDocumentMode == .open ? "open" : "import"
			NSLog("[ARMSX2 iOS Import] opening legacy document picker types=%@ multiple=%d mode=%@",
			      legacyDocumentTypes.joined(separator: ","), allowsMultipleSelection ? 1 : 0, modeName)
		} else {
			picker = UIDocumentPickerViewController(forOpeningContentTypes: allowedContentTypes, asCopy: asCopy)
			NSLog("[ARMSX2 iOS Import] opening document picker types=%@ multiple=%d asCopy=%d",
			      allowedContentTypes.map(\.identifier).joined(separator: ","), allowsMultipleSelection ? 1 : 0, asCopy ? 1 : 0)
		}
        picker.allowsMultipleSelection = allowsMultipleSelection
        picker.delegate = context.coordinator
        return picker
    }

    func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}

    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        private let onComplete: (Result<[URL], Error>) -> Void

        init(onComplete: @escaping (Result<[URL], Error>) -> Void) {
            self.onComplete = onComplete
        }

        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            NSLog("[ARMSX2 iOS Import] picker returned %d URL(s): %@",
                  urls.count, urls.map(\.lastPathComponent).joined(separator: ", "))
            onComplete(.success(urls))
        }

        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            NSLog("[ARMSX2 iOS Import] picker cancelled")
            onComplete(.failure(CocoaError(.userCancelled)))
        }
    }
}
