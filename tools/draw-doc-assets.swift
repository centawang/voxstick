import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

private struct Point {
  let x: CGFloat
  let y: CGFloat
}

private let uprightURL = URL(string: "https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_07.webp")!
private let flatURL = URL(string: "https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_05.webp")!

private func loadImage(from url: URL) throws -> CGImage {
  let data = try Data(contentsOf: url)
  guard
    let source = CGImageSourceCreateWithData(data as CFData, nil),
    let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
  else {
    throw NSError(domain: "voxstick-doc-assets", code: 1, userInfo: [
      NSLocalizedDescriptionKey: "Could not decode \(url.absoluteString)"
    ])
  }
  return image
}

private func makeContext(width: Int, height: Int) throws -> CGContext {
  let colorSpace = CGColorSpaceCreateDeviceRGB()
  guard let context = CGContext(
    data: nil,
    width: width,
    height: height,
    bitsPerComponent: 8,
    bytesPerRow: width * 4,
    space: colorSpace,
    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
  ) else {
    throw NSError(domain: "voxstick-doc-assets", code: 2, userInfo: [
      NSLocalizedDescriptionKey: "Could not create CGContext"
    ])
  }
  return context
}

private func writePNG(_ image: CGImage, to url: URL) throws {
  guard
    let destination = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil)
  else {
    throw NSError(domain: "voxstick-doc-assets", code: 3, userInfo: [
      NSLocalizedDescriptionKey: "Could not create PNG destination"
    ])
  }
  CGImageDestinationAddImage(destination, image, nil)
  if !CGImageDestinationFinalize(destination) {
    throw NSError(domain: "voxstick-doc-assets", code: 4, userInfo: [
      NSLocalizedDescriptionKey: "Could not write \(url.path)"
    ])
  }
}

private func fillRoundedRect(_ context: CGContext, rect: CGRect, radius: CGFloat, color: CGColor) {
  context.setFillColor(color)
  context.addPath(CGPath(roundedRect: rect, cornerWidth: radius, cornerHeight: radius, transform: nil))
  context.fillPath()
}

private func addPolygon(_ context: CGContext, _ points: [Point]) {
  guard let first = points.first else { return }
  context.beginPath()
  context.move(to: CGPoint(x: first.x, y: first.y))
  for point in points.dropFirst() {
    context.addLine(to: CGPoint(x: point.x, y: point.y))
  }
  context.closePath()
}

private func fillPolygon(_ context: CGContext, _ points: [Point], color: CGColor) {
  context.setFillColor(color)
  addPolygon(context, points)
  context.fillPath()
}

private func drawRects(_ context: CGContext, rects: [CGRect]) {
  for rect in rects {
    context.fill(rect)
  }
}

private func drawMicGlyph(_ context: CGContext, color: CGColor, muted: Bool) {
  context.setFillColor(color)

  let rects = [
    CGRect(x: 57, y: 68, width: 20, height: 4),
    CGRect(x: 52, y: 72, width: 30, height: 4),
    CGRect(x: 49, y: 76, width: 4, height: 46),
    CGRect(x: 81, y: 76, width: 4, height: 46),
    CGRect(x: 52, y: 122, width: 30, height: 4),
    CGRect(x: 57, y: 126, width: 20, height: 4),
    CGRect(x: 37, y: 102, width: 4, height: 22),
    CGRect(x: 93, y: 102, width: 4, height: 22),
    CGRect(x: 41, y: 130, width: 52, height: 4),
    CGRect(x: 65, y: 134, width: 4, height: 24),
    CGRect(x: 47, y: 158, width: 40, height: 4)
  ]
  drawRects(context, rects: rects)

  context.setFillColor(CGColor(red: 0.063, green: 0.141, blue: 0.176, alpha: 1))
  context.fill(CGRect(x: 37, y: 178, width: 61, height: 4))

  if muted {
    context.setStrokeColor(CGColor(red: 0.937, green: 0.267, blue: 0.267, alpha: 1))
    context.setLineWidth(5)
    context.setLineCap(.round)
    context.move(to: CGPoint(x: 39, y: 154))
    context.addLine(to: CGPoint(x: 95, y: 66))
    context.strokePath()
  } else {
    context.setFillColor(CGColor(red: 0.086, green: 0.306, blue: 0.388, alpha: 1))
    context.fill(CGRect(x: 37, y: 178, width: 24, height: 4))
  }
}

private func drawScreenContent(_ context: CGContext, muted: Bool) {
  let glyphColor = muted
    ? CGColor(red: 0.80, green: 0.84, blue: 0.89, alpha: 1)
    : CGColor(red: 0.22, green: 0.85, blue: 1.00, alpha: 1)
  drawMicGlyph(context, color: glyphColor, muted: muted)
}

private func drawUprightAsset(to output: URL) throws {
  let base = try loadImage(from: uprightURL)
  let width = base.width
  let height = base.height
  let context = try makeContext(width: width, height: height)

  context.draw(base, in: CGRect(x: 0, y: 0, width: width, height: height))

  context.saveGState()
  context.translateBy(x: 0, y: CGFloat(height))
  context.scaleBy(x: 1, y: -1)

  let screen = CGRect(x: 480, y: 276, width: 248, height: 420)
  fillRoundedRect(
    context,
    rect: screen,
    radius: 9,
    color: CGColor(red: 0.008, green: 0.024, blue: 0.090, alpha: 1)
  )

  context.saveGState()
  context.translateBy(x: screen.minX, y: screen.minY)
  context.scaleBy(x: screen.width / 135, y: screen.height / 240)
  drawScreenContent(context, muted: false)
  context.restoreGState()
  context.restoreGState()

  guard let image = context.makeImage() else { return }
  try writePNG(image, to: output)
}

private func drawFlatAsset(to output: URL) throws {
  let base = try loadImage(from: flatURL)
  let width = base.width
  let height = base.height
  let context = try makeContext(width: width, height: height)

  context.saveGState()
  context.translateBy(x: 0, y: CGFloat(height))
  context.scaleBy(x: 1, y: -1)
  context.draw(base, in: CGRect(x: 0, y: 0, width: width, height: height))
  context.restoreGState()

  context.saveGState()
  context.translateBy(x: 0, y: CGFloat(height))
  context.scaleBy(x: 1, y: -1)

  let topLeft = Point(x: 606, y: 611)
  let topRight = Point(x: 891, y: 642)
  let bottomRight = Point(x: 764, y: 777)
  let bottomLeft = Point(x: 466, y: 748)
  let screenPolygon = [topLeft, topRight, bottomRight, bottomLeft]

  fillPolygon(
    context,
    screenPolygon,
    color: CGColor(red: 0.008, green: 0.024, blue: 0.090, alpha: 1)
  )

  context.saveGState()
  addPolygon(context, screenPolygon)
  context.clip()

  let xAxis = CGVector(dx: bottomLeft.x - topLeft.x, dy: bottomLeft.y - topLeft.y)
  let yAxis = CGVector(dx: topRight.x - topLeft.x, dy: topRight.y - topLeft.y)
  context.concatenate(CGAffineTransform(
    a: xAxis.dx / 135,
    b: xAxis.dy / 135,
    c: yAxis.dx / 240,
    d: yAxis.dy / 240,
    tx: topLeft.x,
    ty: topLeft.y
  ))
  drawScreenContent(context, muted: true)
  context.restoreGState()
  context.restoreGState()

  guard let image = context.makeImage() else { return }
  try writePNG(image, to: output)
}

let outputDirectory = URL(fileURLWithPath: CommandLine.arguments.dropFirst().first ?? "docs/assets")
try FileManager.default.createDirectory(at: outputDirectory, withIntermediateDirectories: true)
try drawUprightAsset(to: outputDirectory.appendingPathComponent("voxstick-upright-live.png"))
try drawFlatAsset(to: outputDirectory.appendingPathComponent("voxstick-flat-muted.png"))
