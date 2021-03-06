#ifndef EUDOPSTRIGGEN_H
#define EUDOPSTRIGGEN_H
#include "CommonFiles\CommonFiles.h"
#include "GenerationData.h"
#include "DeathCounter.h"
#include "EudAddress.h"
#include "EudOpsDef.h"
#include "TextTrigGenerator.h"
#include <stack>

class RestoreAction
{
public:
    const DeathCounter slackSpace;
    const s64 modification;
    RestoreAction(DeathCounter slackSpace, s64 modification) : slackSpace(slackSpace), modification(modification) {}
};

class EudOpsTrigGen
{
    public:
        static bool GenerateNoArg(std::string &output, GenerationData genData, EudOpDef def, EudAddress address);
        static bool GenerateWithConstant(std::string &output, GenerationData genData, EudOpDef def, EudAddress address, u32 constant);
        static bool GenerateWithDeathCounter(std::string &output, GenerationData genData, EudOpDef def, EudAddress address, DeathCounter deathCounter);

        EudAddress targetAddress;
        GenerationData genData;
        TextTrigGenerator gen;
        ScenarioPtr dummyMap;
        
        u8 owners[28];
        u32 nextStringId;
        std::map<u32, std::string> strings;
        std::stack<RestoreAction> restoreActions;
        //std::stringstream out;

        EudOpsTrigGen(GenerationData genData, EudAddress targetAddress)
            : genData(genData), targetAddress(targetAddress), didComment(false), nextStringId(0),
        noComments(false), emptyComments(false), commentAll(true), triggerCount(0)
        {
            endl[0] = '\r';
            endl[1] = '\n';
            owners[0] = 1;
            dummyMap = ScenarioPtr(new Scenario());
            dummyMap->CreateNew(64, 64, Tileset::TERRAIN_BADLANDS, 0, 0);
        }

    protected:
        bool setToConstant(u32 constant);
        bool setToDeaths(DeathCounter srcValue);
        bool copyToDeaths(DeathCounter destValue);
        bool checkEqual(u32 constant);
        bool checkAtLeast(u32 constant);
        bool checkAtMost(u32 constant);
        bool checkGreaterThan(u32 constant);
        bool checkLessThan(u32 constant);

        void stripBit(DeathCounter slackSpace, u32 bit, bool restore);

    private:
        char endl[2];
        bool didComment;
        bool noComments;
        bool emptyComments;
        bool commentAll;
        int triggerCount;

        void trigger(u8* players);
        void end();
        Trigger currTrig;

        // Accumulate
        // Always
        bool always();
        // Bring
        // Command
        // Command the Least
        // Command the Least At
        // Command the Most
        // Commands the Most At
        // Countdown Timer
        // Deaths
        bool deaths(u32 playerId, u32 unitId, NumericComparison numericComparison, u32 amount);
        // Elapsed Time
        // Highest Score
        // Kill
        // Least Kills
        // Least Resources
        // Lowest Score
        // Memory
        bool memory(u32 address, NumericComparison numericComparison, u32 value);
        // Most Kills
        // Most Resources
        // Never
        // Opponents
        // Score
        // Switch
        bool switchState(u32 switchNum, SwitchState state);


        // Center View
        // Comment
        bool comment(const std::string &text);
        // Create Unit
        // Create Unit with Properties
        // Defeat
        // Display Text Message
        // Draw
        // Give Units to Player
        // Kill Unit
        // Kill Unit At Location
        // Leaderboard (Control At Location)
        // Leaderboard (Control)
        // Leaderboard (Greed)
        // Leaderboard (Kills)
        // Leaderboard (Points)
        // Leaderboard (Resources)
        // Leaderboard Computer Players(State)
        // Leaderboard Goal (Control At Location)
        // Leaderboard Goal (Control)
        // Leaderboard Goal (Kills)
        // Leaderboard Goal (Points)
        // Leaderboard Goal (Resources)
        // Minimap Ping
        // Modify Unit Energy
        // Modify Unit Hanger Count
        // Modify Unit Hit Points
        // Modify Unit Resource Amount
        // Modify Unit Shield Points
        // Move Location
        // Move Unit
        // Mute Unit Speech
        // Order
        // Pause Game
        // Pause Timer
        // Play WAV
        // Preserve Trigger
        bool preserveTrigger();
        // Remove Unit
        // Remove Unit At Location
        // Run AI Script
        // Run AI Script At Location
        // Set Alliance Status
        // Set Countdown Timer
        // Set Deaths
        bool setDeaths(u32 playerId, u32 unitId, NumericModifier numericModifier, u32 value);
        // Set Doodad State
        // Set Invincibility
        // Set Mission Objectives
        // Set Next Scenario
        // Set Resources
        // Set Score
        // Set Switch
        bool setSwitch(u32 switchNum, SwitchModifier switchModifier);
        // Talking Portrait
        // Transmission
        // Unmute Unit Speech
        // Unpause Game
        // Unpause Timer
        // Victory
        // Wait
};

#endif