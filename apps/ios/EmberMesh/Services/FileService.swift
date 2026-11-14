import UIKit
import UniformTypeIdentifiers

/// host.file.save (share sheet, includes "Save to Files", AirDrop) and host.file.pick (document picker).
final class FileService: NSObject, UIDocumentPickerDelegate {
  static let maxPickBytes = 32 * 1024 * 1024
  private var pickCompletion: RPCCompletion?

  static func sanitize(_ name: String) -> String {
    let bad = CharacterSet(charactersIn: "/\\:?%*|\"<>\0").union(.controlCharacters)
    var s = name.components(separatedBy: bad).joined(separator: "_").trimmingCharacters(in: .whitespaces)
    while s.hasPrefix(".") { s.removeFirst() }
    if s.isEmpty { s = "embermesh-file" }
    return String(s.prefix(120))
  }

  /// "accept" like an <input accept>: ".ember,application/json,image/*". Empty → anything.
  static func contentTypes(accept: String?) -> [UTType] {
    let parts = (accept ?? "").split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces).lowercased() }.filter { !$0.isEmpty }
    var types: [UTType] = []
    for p in parts {
      if p == "*/*" || p == "*" { return [.item] }
      if p.hasPrefix(".") {
        if let t = UTType(filenameExtension: String(p.dropFirst())) { types.append(t) }
      } else if p.hasSuffix("/*") {
        switch p {
        case "image/*": types.append(.image)
        case "text/*": types.append(.text)
        case "audio/*": types.append(.audio)
        case "video/*": types.append(.movie)
        default: types.append(.data)
        }
      } else if let t = UTType(mimeType: p) {
        types.append(t)
      }
    }
    return types.isEmpty ? [.item] : types
  }

  func save(params: JSONObject, presenter: Presenter?, completion: @escaping RPCCompletion) {
    guard let presenter else { completion(.failure(.unsupported("no window to present from"))); return }
    guard let b64 = params.string("data_b64"), let data = Data(base64Encoded: b64, options: .ignoreUnknownCharacters) else {
      completion(.failure(.badParams("data_b64 must be base64"))); return
    }
    let name = FileService.sanitize(params.string("name") ?? "embermesh-file")
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("share-\(UUID().uuidString)", isDirectory: true)
    do {
      try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
      let url = dir.appendingPathComponent(name)
      try data.write(to: url, options: [.atomic, .completeFileProtection])
      let vc = UIActivityViewController(activityItems: [url], applicationActivities: nil)
      vc.completionWithItemsHandler = { _, done, _, err in
        try? FileManager.default.removeItem(at: dir)
        if let err { completion(.failure(.io(err.localizedDescription))) }
        else if done { completion(.success([:])) }
        else { completion(.failure(.denied("cancelled"))) }
      }
      presenter.present(vc, sourceRectCenter: true)
    } catch {
      completion(.failure(.io(error.localizedDescription)))
    }
  }

  func pick(params: JSONObject, presenter: Presenter?, completion: @escaping RPCCompletion) {
    guard let presenter else { completion(.failure(.unsupported("no window to present from"))); return }
    if pickCompletion != nil { completion(.failure(.busy("a file picker is already open"))); return }
    let vc = UIDocumentPickerViewController(forOpeningContentTypes: FileService.contentTypes(accept: params.string("accept")), asCopy: true)
    vc.allowsMultipleSelection = false
    vc.delegate = self
    pickCompletion = completion
    presenter.present(vc, sourceRectCenter: false)
  }

  func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
    guard let c = pickCompletion else { return }
    pickCompletion = nil
    guard let url = urls.first else { c(.failure(.denied("cancelled"))); return }
    let scoped = url.startAccessingSecurityScopedResource()
    defer { if scoped { url.stopAccessingSecurityScopedResource() } }
    let size = (try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0
    guard size <= FileService.maxPickBytes else { c(.failure(.tooLarge("file is larger than 32 MB"))); return }
    guard let data = try? Data(contentsOf: url) else { c(.failure(.io("could not read the file"))); return }
    let mime = UTType(filenameExtension: url.pathExtension)?.preferredMIMEType ?? "application/octet-stream"
    c(.success(["name": url.lastPathComponent, "mime": mime, "data_b64": data.base64EncodedString()]))
  }

  func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
    pickCompletion?(.failure(.denied("cancelled")))
    pickCompletion = nil
  }
}
