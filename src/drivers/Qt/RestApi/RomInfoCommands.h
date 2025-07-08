#ifndef __ROM_INFO_COMMANDS_H__
#define __ROM_INFO_COMMANDS_H__

#include "RestApiCommands.h"
#include "../../../fceu.h"
#include "../../../git.h"
#include "../../../cart.h"
#include <sstream>
#include <iomanip>

/**
 * @brief ROM information structure
 */
struct RomInfo {
    bool loaded;
    std::string filename;
    std::string name;
    int size;
    int mapper;
    std::string mirroring;
    bool has_battery;
    std::string md5;
    
    /**
     * @brief Convert mirroring value to string
     * @param mirror Mirroring type value
     * @return String representation of mirroring type
     */
    static std::string getMirroringString(int mirror) {
        switch(mirror) {
            case 0: return "horizontal";
            case 1: return "vertical";
            case 2: return "4screen";
            case 3: return "none";
            default: return "unknown";
        }
    }
    
    /**
     * @brief Convert MD5 bytes to hex string
     * @param md5 MD5 byte array
     * @return Hex string representation
     */
    static std::string md5ToHexString(const uint8 md5[16]) {
        std::ostringstream oss;
        for (int i = 0; i < 16; i++) {
            oss << std::hex << std::setfill('0') << std::setw(2) 
                << static_cast<int>(md5[i]);
        }
        return oss.str();
    }
    
    /**
     * @brief Convert to JSON string
     * @return JSON representation of the ROM info
     */
    std::string toJson() const {
        std::ostringstream json;
        json << "{";
        json << "\"loaded\":" << (loaded ? "true" : "false");
        
        if (loaded) {
            json << ",";
            json << "\"filename\":\"" << filename << "\",";
            json << "\"name\":\"" << name << "\",";
            json << "\"size\":" << size << ",";
            json << "\"mapper\":" << mapper << ",";
            json << "\"mirroring\":\"" << mirroring << "\",";
            json << "\"has_battery\":" << (has_battery ? "true" : "false") << ",";
            json << "\"md5\":\"" << md5 << "\"";
        }
        
        json << "}";
        return json.str();
    }
};

/**
 * @brief Command to get ROM information
 */
class RomInfoCommand : public ApiCommandWithResult<RomInfo> {
public:
    void execute() override {
        RomInfo info;
        
        // Check if ROM is loaded
        if (!GameInfo) {
            info.loaded = false;
            resultPromise.set_value(info);
            return;
        }
        
        info.loaded = true;
        
        // Get filename - prefer regular filename over archive filename
        if (GameInfo->filename) {
            info.filename = GameInfo->filename;
        } else if (GameInfo->archiveFilename) {
            info.filename = GameInfo->archiveFilename;
        } else {
            info.filename = "";
        }
        
        // Get game name (UTF-8 encoded)
        if (GameInfo->name) {
            info.name = reinterpret_cast<const char*>(GameInfo->name);
        } else {
            info.name = "";
        }
        
        // Get mapper number
        info.mapper = GameInfo->mappernum;
        
        // Calculate ROM size (PRG + CHR)
        info.size = 0;
        if (PRGsize[0] > 0) {
            info.size += PRGsize[0];
        }
        if (CHRsize[0] > 0) {
            info.size += CHRsize[0];
        }
        
        // Get mirroring and battery info from CartInfo if available
        if (currCartInfo) {
            info.mirroring = RomInfo::getMirroringString(currCartInfo->mirror);
            info.has_battery = (currCartInfo->battery != 0);
        } else {
            info.mirroring = "unknown";
            info.has_battery = false;
        }
        
        // Get MD5 hash
        info.md5 = RomInfo::md5ToHexString(GameInfo->MD5.data);
        
        resultPromise.set_value(info);
    }
    
    const char* name() const override {
        return "RomInfoCommand";
    }
};

#endif // __ROM_INFO_COMMANDS_H__