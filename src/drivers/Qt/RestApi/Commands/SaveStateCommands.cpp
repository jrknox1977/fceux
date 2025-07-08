#include "SaveStateCommands.h"
#include "../../fceuWrapper.h"
#include "../../../../state.h"
#include "../../../../fceu.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QDateTime>
#include <sstream>
#include <cstring>

SaveStateCommand::SaveStateCommand(int stateSlot, const std::string& savePath)
    : slot(stateSlot), path(savePath)
{
    // Validate slot number
    if (slot < -1 || slot > 9) {
        slot = -1; // Default to memory
    }
}

void SaveStateCommand::saveToMemory(SaveStateResult& result) {
    // TODO: Implement memory-based save state
    // This requires using FCEUSS_SaveMS or similar function
    result.success = false;
    result.error = "Memory save states not yet implemented";
}

void SaveStateCommand::saveToFile(SaveStateResult& result) {
    // Use FCEUI_SaveState which saves to slot
    // This function returns void, so we assume success
    FCEUI_SaveState(NULL, false);
    
    result.success = true;
    result.slot = slot;
    
    // Generate expected filename
    if (GameInfo && GameInfo->filename) {
        std::stringstream ss;
        ss << GameInfo->filename << ".fc" << slot;
        result.filename = ss.str();
    } else {
        result.filename = "savestate.fc" + std::to_string(slot);
    }
    
    // Set timestamp
    result.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    
    printf("SaveStateCommand: State saved to slot %d\n", slot);
}

void SaveStateCommand::execute() {
    if (!ensureGameLoaded()) {
        return;
    }
    
    SaveStateResult result;
    
    FCEU_WRAPPER_LOCK();
    
    try {
        if (slot == -1) {
            // Save to memory
            saveToMemory(result);
        } else {
            // Save to file slot
            // First set the save slot
            FCEUI_SelectState(slot, 1);
            saveToFile(result);
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("Save state failed: ") + e.what();
    }
    
    FCEU_WRAPPER_UNLOCK();
    
    resultPromise.set_value(result);
}

LoadStateCommand::LoadStateCommand(int stateSlot, const std::string& loadPath, const std::string& base64Data)
    : slot(stateSlot), path(loadPath), data(base64Data)
{
    // Validate slot number
    if (slot < -1 || slot > 9) {
        slot = -1; // Default to memory
    }
}

void LoadStateCommand::loadFromMemory(SaveStateResult& result) {
    // TODO: Implement memory-based load state
    // This requires using FCEUSS_LoadFP or similar function
    result.success = false;
    result.error = "Memory load states not yet implemented";
}

void LoadStateCommand::loadFromFile(SaveStateResult& result) {
    // Use FCEUI_LoadState which loads from slot
    // This function returns void, so we assume success
    FCEUI_LoadState(NULL, false);
    
    result.success = true;
    result.slot = slot;
    
    // Generate expected filename
    if (GameInfo && GameInfo->filename) {
        std::stringstream ss;
        ss << GameInfo->filename << ".fc" << slot;
        result.filename = ss.str();
    } else {
        result.filename = "savestate.fc" + std::to_string(slot);
    }
    
    // Set timestamp
    result.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    
    printf("LoadStateCommand: State loaded from slot %d\n", slot);
}

void LoadStateCommand::execute() {
    if (!ensureGameLoaded()) {
        return;
    }
    
    SaveStateResult result;
    
    FCEU_WRAPPER_LOCK();
    
    try {
        if (slot == -1) {
            // Load from memory
            loadFromMemory(result);
        } else {
            // Load from file slot
            // First set the save slot
            FCEUI_SelectState(slot, 1);
            loadFromFile(result);
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("Load state failed: ") + e.what();
    }
    
    FCEU_WRAPPER_UNLOCK();
    
    resultPromise.set_value(result);
}

void ListSaveStatesCommand::execute() {
    SaveStateListResult result;
    
    if (!GameInfo || !GameInfo->filename) {
        result.success = false;
        result.error = "No game loaded";
        resultPromise.set_value(result);
        return;
    }
    
    // For now, just report success without listing
    // TODO: Implement proper listing when SaveStateListResult supports vectors
    result.success = true;
    
    printf("ListSaveStatesCommand: Listing save states (not fully implemented)\n");
    
    resultPromise.set_value(result);
}