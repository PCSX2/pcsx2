// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

struct ShareSheetItem: Identifiable {
    let id = UUID()
    let url: URL
}

struct ActivityShareSheet: UIViewControllerRepresentable {
    let activityItems: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        let controller = UIActivityViewController(activityItems: activityItems, applicationActivities: nil)
        Self.configurePopoverPresentation(for: controller, sourceView: controller.view)
        return controller
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}

    static func configurePopoverPresentation(for controller: UIViewController, sourceView: UIView) {
        guard let popover = controller.popoverPresentationController else {
            return
        }
        popover.sourceView = sourceView
        popover.sourceRect = CGRect(
            x: sourceView.bounds.midX,
            y: sourceView.bounds.midY,
            width: 1,
            height: 1
        )
        popover.permittedArrowDirections = []
    }
}
