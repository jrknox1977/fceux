#include "ScreenshotCommands.h"
#include "../../fceuWrapper.h"
#include "../../../../video.h"
#include "../../../../driver.h"
#include "../../../../fceu.h"
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QThread>
#include <sstream>
#include <iomanip>
#include <cstring>

// Static members for tracking last screenshot
std::string LastScreenshotCommand::lastScreenshotPath;
std::string LastScreenshotCommand::lastScreenshotFormat;

// External declarations
extern uint8 *XBuf;
void FCEUD_GetPalette(uint8 index, uint8 *r, uint8 *g, uint8 *b);

ScreenshotCommand::ScreenshotCommand(const std::string& fmt, const std::string& enc, const std::string& savePath)
    : format(fmt), encoding(enc), path(savePath)
{
    // Validate and normalize format
    if (format.empty()) {
        format = "png";
    }
    
    // Validate encoding
    if (encoding != "file" && encoding != "base64") {
        encoding = "file";
    }
}

std::string ScreenshotCommand::generateFilename() const {
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyyMMdd-HHmmss");
    
    std::stringstream ss;
    ss << "fceux-" << timestamp.toStdString() << "." << format;
    return ss.str();
}

void ScreenshotCommand::executeFileMode(ScreenshotResult& result) {
    // Just capture to base64
    captureToBase64(result);
}

void ScreenshotCommand::executeBase64Mode(ScreenshotResult& result) {
    captureToBase64(result);
}

void ScreenshotCommand::captureToBase64(ScreenshotResult& result) {
    // Check if XBuf is available
    if (XBuf == NULL) {
        result.success = false;
        result.error = "Video buffer not available";
        return;
    }
    
    // NES resolution
    const int width = 256;
    const int height = 240;
    
    // Create QImage with RGB32 format
    QImage image(width, height, QImage::Format_RGB32);
    
    // Get pixel data from XBuf and palette
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8 pixel = XBuf[y * 256 + x];
            uint8 r, g, b;
            FCEUD_GetPalette(pixel, &r, &g, &b);
            image.setPixel(x, y, qRgb(r, g, b));
        }
    }
    
    // Convert to requested format
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    
    bool saved = false;
    if (format == "png") {
        saved = image.save(&buffer, "PNG");
    } else if (format == "jpg" || format == "jpeg") {
        saved = image.save(&buffer, "JPEG", 90); // 90% quality
    } else if (format == "bmp") {
        saved = image.save(&buffer, "BMP");
    } else {
        // Default to PNG
        saved = image.save(&buffer, "PNG");
    }
    
    if (saved) {
        // Convert to base64
        result.data = imageData.toBase64().toStdString();
        result.success = true;
        result.format = format;
        result.encoding = "base64";
    } else {
        result.success = false;
        result.error = "Failed to encode image";
    }
}

void ScreenshotCommand::execute() {
    // Ensure game is loaded
    if (!ensureGameLoaded()) {
        return;
    }
    
    ScreenshotResult result;
    
    FCEU_WRAPPER_LOCK();
    
    try {
        // Always use file mode now - it handles both base64 capture and optional file saving
        executeFileMode(result);
    } catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("Screenshot failed: ") + e.what();
    }
    
    FCEU_WRAPPER_UNLOCK();
    
    resultPromise.set_value(result);
}

void LastScreenshotCommand::execute() {
    ScreenshotResult result;
    
    if (lastScreenshotPath.empty()) {
        result.success = false;
        result.error = "No screenshot has been taken yet";
    } else {
        // Check if file still exists
        QFile file(QString::fromStdString(lastScreenshotPath));
        if (file.exists()) {
            result.success = true;
            result.format = lastScreenshotFormat;
            result.encoding = "file";
            result.filename = QFileInfo(file).fileName().toStdString();
            result.path = lastScreenshotPath;
        } else {
            result.success = false;
            result.error = "Last screenshot file no longer exists";
        }
    }
    
    resultPromise.set_value(result);
}