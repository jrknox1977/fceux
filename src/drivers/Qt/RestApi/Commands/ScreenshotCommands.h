#ifndef __SCREENSHOT_COMMANDS_H__
#define __SCREENSHOT_COMMANDS_H__

#include "MediaCommands.h"
#include <QDateTime>

/**
 * @brief Command to capture a screenshot
 * 
 * Supports both file-based and base64 encoding modes
 */
class ScreenshotCommand : public BaseMediaCommand<ScreenshotResult> {
private:
    std::string format;     // png (default), jpg, bmp
    std::string encoding;   // file (default), base64
    std::string path;       // optional path to save to
    
    /**
     * @brief Generate timestamped filename
     * @return Filename like "fceux-20250108-123456.png"
     */
    std::string generateFilename() const;
    
    /**
     * @brief Execute file-based screenshot
     * @param result Result object to populate
     */
    void executeFileMode(ScreenshotResult& result);
    
    /**
     * @brief Execute base64 screenshot encoding
     * @param result Result object to populate
     */
    void executeBase64Mode(ScreenshotResult& result);
    
    /**
     * @brief Capture screenshot to base64
     * @param result Result object to populate
     */
    void captureToBase64(ScreenshotResult& result);
    
public:
    /**
     * @brief Constructor
     * @param fmt Image format (png, jpg, bmp)
     * @param enc Encoding mode (file, base64)
     * @param savePath Optional path to save the screenshot
     */
    ScreenshotCommand(const std::string& fmt = "png", const std::string& enc = "file", const std::string& savePath = "");
    
    void execute() override;
    const char* name() const override { return "ScreenshotCommand"; }
};

/**
 * @brief Command to get information about the last screenshot
 */
class LastScreenshotCommand : public BaseMediaCommand<ScreenshotResult> {
private:
    static std::string lastScreenshotPath;
    static std::string lastScreenshotFormat;
    
public:
    void execute() override;
    const char* name() const override { return "LastScreenshotCommand"; }
    
    // Allow ScreenshotCommand to update last screenshot info
    friend class ScreenshotCommand;
};

#endif // __SCREENSHOT_COMMANDS_H__