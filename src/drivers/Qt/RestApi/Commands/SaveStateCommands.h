#ifndef __SAVE_STATE_COMMANDS_H__
#define __SAVE_STATE_COMMANDS_H__

#include "MediaCommands.h"
#include <string>
#include <vector>

// Command to save current emulation state
class SaveStateCommand : public BaseMediaCommand<SaveStateResult> {
private:
    int slot;           // -1 for memory, 0-9 for file slots
    std::string path;   // optional custom path for file saves
    
public:
    SaveStateCommand(int stateSlot = -1, const std::string& savePath = "");
    
    const char* name() const override { return "SaveStateCommand"; }
    
protected:
    void execute() override;
    
private:
    void saveToMemory(SaveStateResult& result);
    void saveToFile(SaveStateResult& result);
};

// Command to load a previously saved state
class LoadStateCommand : public BaseMediaCommand<SaveStateResult> {
private:
    int slot;           // -1 for memory, 0-9 for file slots
    std::string path;   // optional custom path for file loads
    std::string data;   // base64 data for memory loads
    
public:
    LoadStateCommand(int stateSlot = -1, const std::string& loadPath = "", const std::string& base64Data = "");
    
    const char* name() const override { return "LoadStateCommand"; }
    
protected:
    void execute() override;
    
private:
    void loadFromMemory(SaveStateResult& result);
    void loadFromFile(SaveStateResult& result);
};

// Command to list available save states
class ListSaveStatesCommand : public BaseMediaCommand<SaveStateListResult> {
public:
    const char* name() const override { return "ListSaveStatesCommand"; }
    
protected:
    void execute() override;
};

#endif // __SAVE_STATE_COMMANDS_H__