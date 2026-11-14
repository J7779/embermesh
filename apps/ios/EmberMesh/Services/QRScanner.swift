import AVFoundation
import UIKit

/// host.qr.scan: full-screen camera with AVCaptureMetadataOutput (QR only). No third-party decoder.
final class QRScannerViewController: UIViewController, AVCaptureMetadataOutputObjectsDelegate {
  static var hasCamera: Bool {
    #if targetEnvironment(simulator)
    return false
    #else
    return AVCaptureDevice.default(for: .video) != nil
    #endif
  }

  private let session = AVCaptureSession()
  private let sessionQueue = DispatchQueue(label: "org.embermesh.qr")
  private var completion: ((RPCResult) -> Void)?
  private var preview: AVCaptureVideoPreviewLayer?

  init(completion: @escaping (RPCResult) -> Void) {
    self.completion = completion
    super.init(nibName: nil, bundle: nil)
    modalPresentationStyle = .fullScreen
  }
  required init?(coder: NSCoder) { fatalError() }

  static func scan(presenter: Presenter, completion: @escaping RPCCompletion) {
    guard hasCamera else { completion(.failure(.unsupported("This device has no camera"))); return }
    let go = {
      let vc = QRScannerViewController(completion: completion)
      presenter.present(vc, sourceRectCenter: false)
    }
    switch AVCaptureDevice.authorizationStatus(for: .video) {
    case .authorized: go()
    case .notDetermined:
      AVCaptureDevice.requestAccess(for: .video) { ok in
        DispatchQueue.main.async {
          if ok { go() } else { completion(.failure(.denied("Camera permission is off. Turn it on in Settings > EmberMesh > Camera."))) }
        }
      }
    default: completion(.failure(.denied("Camera permission is off. Turn it on in Settings > EmberMesh > Camera.")))
    }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = .black
    let label = UILabel()
    label.text = "Point the camera at a QR code"
    label.textColor = .white
    label.font = .preferredFont(forTextStyle: .headline)
    label.translatesAutoresizingMaskIntoConstraints = false
    let cancel = UIButton(type: .system)
    cancel.setTitle("Cancel", for: .normal)
    cancel.titleLabel?.font = .preferredFont(forTextStyle: .title3)
    cancel.tintColor = .white
    cancel.addTarget(self, action: #selector(cancelTapped), for: .touchUpInside)
    cancel.translatesAutoresizingMaskIntoConstraints = false

    guard let dev = AVCaptureDevice.default(for: .video), let input = try? AVCaptureDeviceInput(device: dev),
          session.canAddInput(input) else {
      finish(.failure(.unsupported("Camera unavailable")))
      return
    }
    session.addInput(input)
    let out = AVCaptureMetadataOutput()
    if session.canAddOutput(out) {
      session.addOutput(out)
      out.setMetadataObjectsDelegate(self, queue: .main)
      out.metadataObjectTypes = out.availableMetadataObjectTypes.contains(.qr) ? [.qr] : []
    }
    let pl = AVCaptureVideoPreviewLayer(session: session)
    pl.videoGravity = .resizeAspectFill
    view.layer.addSublayer(pl)
    preview = pl
    view.addSubview(label)
    view.addSubview(cancel)
    NSLayoutConstraint.activate([
      label.centerXAnchor.constraint(equalTo: view.centerXAnchor),
      label.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 24),
      cancel.centerXAnchor.constraint(equalTo: view.centerXAnchor),
      cancel.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -24),
    ])
    sessionQueue.async { self.session.startRunning() }
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    preview?.frame = view.bounds
  }

  func metadataOutput(_ output: AVCaptureMetadataOutput, didOutput objects: [AVMetadataObject], from connection: AVCaptureConnection) {
    guard let code = objects.compactMap({ $0 as? AVMetadataMachineReadableCodeObject }).first,
          let text = code.stringValue, !text.isEmpty else { return }
    UINotificationFeedbackGenerator().notificationOccurred(.success)
    finish(.success(["text": text]))
  }

  @objc private func cancelTapped() { finish(.failure(.denied("Scan cancelled"))) }

  private func finish(_ r: RPCResult) {
    guard let c = completion else { return }
    completion = nil
    sessionQueue.async { self.session.stopRunning() }
    if presentingViewController != nil {
      dismiss(animated: true) { c(r) }
    } else {
      DispatchQueue.main.async { c(r) }
    }
  }
}
