#include "EudOpsTrigGen.h"
#include <stdarg.h>
#include <cstdarg>
#include <cmath>
#include <cstring>

bool EudOpsTrigGen::GenerateNoArg(std::string &output, GenerationData genData, EudOpDef def, EudAddress eudAddress, bool destructive)
{
    output = "Generate case for " + def.eudOpName + " not found!";
    return false;
}

bool EudOpsTrigGen::GenerateWithConstant(std::string &output, GenerationData genData, EudOpDef def, EudAddress eudAddress, u32 constant, bool destructive)
{
    EudOpsTrigGen eudOpsGen = EudOpsTrigGen(genData, eudAddress);
    bool caseNotFound = false;
    bool success = false;
    switch (def.eudOp)
    {
    case EudOp::SetToConstant:
        success = eudOpsGen.setToConstant(constant);
        break;
    case EudOp::AddConstant:
        success = eudOpsGen.addConstant(constant);
        break;
    case EudOp::CheckEqual:
        success = eudOpsGen.checkEqual(constant);
        break;
    case EudOp::CheckAtLeast:
        success = eudOpsGen.checkAtLeast(constant);
        break;
    case EudOp::CheckAtMost:
        success = eudOpsGen.checkAtMost(constant);
        break;
    case EudOp::CheckGreaterThan:
        success = eudOpsGen.checkGreaterThan(constant);
        break;
    case EudOp::CheckLessThan:
        success = eudOpsGen.checkLessThan(constant);
        break;
    default:
        caseNotFound = true;
        break;
    }

    if (caseNotFound)
    {
        output = "Generate case for " + def.eudOpName + " not found!";
        return false;
    }
    else if ( success )
    {
        eudOpsGen.end();
        eudOpsGen.gen = TextTrigGenerator(genData.useAddressesForMemory, deathTableOffset);
        std::string trigString("");
        return eudOpsGen.gen.GenerateTextTrigs(eudOpsGen.dummyMap, output);
    }
    return success;
}

bool EudOpsTrigGen::GenerateWithDeathCounter(std::string &output, GenerationData genData, EudOpDef def, EudAddress eudAddress, DeathCounter deathCounter, bool destructive)
{
    EudOpsTrigGen eudOpsGen = EudOpsTrigGen(genData, eudAddress);
    bool caseNotFound = false;
    bool success = false;
    switch ( def.eudOp )
    {
        case EudOp::SetToDeaths:
            success = eudOpsGen.setToDeaths(deathCounter, destructive);
            break;
        case EudOp::CopyToDeaths:
            success = eudOpsGen.copyToDeaths(deathCounter, destructive);
            break;
        default:
            caseNotFound = true;
            break;
    }

    if (caseNotFound)
    {
        output = "Generate case for " + def.eudOpName + " not found!";
        return false;
    }
    else if ( success )
    {
        eudOpsGen.end();
        eudOpsGen.gen = TextTrigGenerator(genData.useAddressesForMemory, deathTableOffset);
        std::string trigString("");
        return eudOpsGen.gen.GenerateTextTrigs(eudOpsGen.dummyMap, output);
    }
    return success;
}

bool EudOpsTrigGen::GenerateWithMemory(std::string &output, GenerationData genData, EudOpDef def, EudAddress eudAddress, EudAddress memoryAddress, bool destructive)
{
    EudOpsTrigGen eudOpsGen = EudOpsTrigGen(genData, eudAddress);
    bool caseNotFound = false;
    bool success = false;
    switch (def.eudOp)
    {
    case EudOp::SetToMemory:
        success = eudOpsGen.setToMemory(memoryAddress, destructive);
        break;
    case EudOp::CopyToMemory:
        success = eudOpsGen.copyToMemory(memoryAddress, destructive);
        break;
    default:
        caseNotFound = true;
        break;
    }

    if (caseNotFound)
    {
        output = "Generate case for " + def.eudOpName + " not found!";
        return false;
    }
    else if ( success )
    {
        eudOpsGen.end();
        eudOpsGen.gen = TextTrigGenerator(genData.useAddressesForMemory, deathTableOffset);
        std::string trigString("");
        return eudOpsGen.gen.GenerateTextTrigs(eudOpsGen.dummyMap, output);
    }
    return success;
}

bool EudOpsTrigGen::setToConstant(u32 constant)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBlocking = 0;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;
    
    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);
    if (bitsAfterAddress > 0)
    {
        u32 bitsBeforeRemainder = bit + bitLength;
        for (; bit < bitsBeforeRemainder; bit++)
            stripBit(strippedBits, bit, false);
        u32 valueToAdd = constant << bitsAfterAddress;
        if (valueToAdd != 0)
        {
            trigger(owners);
            always();
            setDeaths(targetAddress, NumericModifier::Add, valueToAdd);
        }
    }
    else // bitsAfterAddress == 0
    {
        trigger(owners);
        always();
        setDeaths(targetAddress, NumericModifier::SetTo, constant);
    }
    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(strippedBits, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::addConstant(u32 constant)
{

    /**
    0xAABBCCDD in little endian: | DD | CC | BB | AA | ( | byte 0 | byte 1 | byte 2 | byte 3 | )
    Byte 0 mask: 0x000000FF blocked by 0xFFFFFF00
    Byte 1 mask: 0x0000FF00 blocked by 0xFFFF0000
    Byte 2 mask: 0x00FF0000 blocked by 0xFF000000
    Byte 3 mask: 0xFF000000 is not blocked
    */
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    u32 stripFirst = genData.getSlackSpaceSwitchNum();
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;
    std::cout << "bitsBefore: " + bitsBeforeAddress << std::endl;
    std::cout << "bits";

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);
    if (bitsAfterAddress > 0)
    {
        u32 bitsBeforeRemainder = bit + bitLength;
        u32 overflowBoundsCheck = (2 << (bitLength-1)) - constant;
        trigger(owners);
        memory(targetAddress, NumericComparison::AtLeast, overflowBoundsCheck << bitsAfterAddress);
        setSwitch(stripFirst, SwitchModifier::Set);
        trigger(owners);
        memory(targetAddress, NumericComparison::AtMost, ((overflowBoundsCheck+1) << bitsAfterAddress)-1);
        for (; bit < bitsBeforeRemainder; bit++)
        {
            u32 unshiftedValue = pow(2, bitLength-bit-1);
            u32 shiftedValue = unshiftedValue << bitsAfterAddress;
        }
        u32 valueToAdd = constant << bitsAfterAddress;
        if (valueToAdd != 0)
        {
            trigger(owners);
            always();
            setDeaths(targetAddress, NumericModifier::Add, valueToAdd);
        }
    }
    else // bitsAfterAddress == 0
    {
        trigger(owners);
        always();
        setDeaths(targetAddress, NumericModifier::SetTo, constant);
    }
    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(strippedBits, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::subtractConstant(u32 constant)
{
    return false;
}

bool EudOpsTrigGen::setToDeaths(DeathCounter srcValue, bool destructive)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(genData.getSlackSpaceDc(), bit, !destructive);
    if (bitsAfterAddress == 0)
        setDeaths(targetAddress.playerId, targetAddress.unitId, NumericModifier::SetTo, 0);
    else
    {
        for (; bit < bitLength; bit++)
            stripBit(genData.getSlackSpaceDc(), bit, false);
    }
    for (bit = 0; bit < bitLength; bit++)
    {
        trigger(owners);
        u32 unshiftedValue = pow(2, bitLength-bit-1);
        u32 shiftedValue = unshiftedValue << bitsAfterAddress;
        deaths(srcValue, NumericComparison::AtLeast, unshiftedValue);
        setDeaths(srcValue, NumericModifier::Subtract, unshiftedValue);
        setDeaths(targetAddress, NumericModifier::Add, shiftedValue);
    }
    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::addDeaths(DeathCounter addition)
{
    return false;
}

bool EudOpsTrigGen::subtractDeaths(DeathCounter subtraction)
{
    return false;
}

bool EudOpsTrigGen::setToMemory(EudAddress srcMemoryAddress, bool destructive)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, !destructive);
    if (bitsAfterAddress == 0)
        setDeaths(targetAddress.playerId, targetAddress.unitId, NumericModifier::SetTo, 0);
    else
    {
        for (; bit < bitLength; bit++)
            stripBit(strippedBits, bit, false);
    }
    for (bit = 0; bit < bitLength; bit++)
    {
        trigger(owners);
        u32 unshiftedValue = pow(2, bitLength-bit-1);
        u32 shiftedValue = unshiftedValue << bitsAfterAddress;
        deaths(srcMemoryAddress.playerId, srcMemoryAddress.unitId, NumericComparison::AtLeast, unshiftedValue);
        setDeaths(srcMemoryAddress.playerId, srcMemoryAddress.unitId, NumericModifier::Subtract, unshiftedValue);
        setDeaths(targetAddress.playerId, targetAddress.unitId, NumericModifier::Add, shiftedValue);
    }
    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits.playerId, strippedBits.unitId, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits.playerId, strippedBits.unitId, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress.playerId, targetAddress.unitId, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::copyToDeaths(DeathCounter destValue, bool destructive)
{
    std::cout << "destructive: " << (destructive ? "true" : "false") << std::endl;
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter slackSpace = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(slackSpace, bit, !destructive);
    for (bit = 0; bit < bitLength; bit++)
    {
        trigger(owners);
        u32 unshiftedValue = pow(2, bitLength-bit-1);
        u32 shiftedValue = unshiftedValue << bitsAfterAddress;
        deaths(targetAddress.playerId, targetAddress.unitId, NumericComparison::AtLeast, shiftedValue);
        setDeaths(targetAddress.playerId, targetAddress.unitId, NumericModifier::Subtract, shiftedValue);
        setDeaths(destValue.playerId, destValue.unitId, NumericModifier::Add, unshiftedValue);
        if ( !destructive ) {
            setDeaths(slackSpace.playerId, slackSpace.unitId, NumericModifier::Add, shiftedValue);
            restoreActions.push(RestoreAction(slackSpace, shiftedValue));
        }
    }
    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(slackSpace.playerId, slackSpace.unitId, NumericComparison::AtLeast, action.modification);
        setDeaths(slackSpace.playerId, slackSpace.unitId, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress.playerId, targetAddress.unitId, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::copyToMemory(EudAddress destMemoryAddress, bool destructive)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);
    u32 bitsBeforeRemainder = bit + bitLength;
    for (; bit < bitLength; bit++)
        stripBit(strippedBits, bit, false);
    for (bit = 0; bit < bitLength; bit++)
    {
        trigger(owners);
        u32 unshiftedValue = pow(2, bitLength-bit-1);
        u32 shiftedValue = unshiftedValue << bitsAfterAddress;
        deaths(targetAddress.playerId, targetAddress.unitId, NumericComparison::AtLeast, shiftedValue);
        setDeaths(targetAddress.playerId, targetAddress.unitId, NumericModifier::Subtract, shiftedValue);
        setDeaths(destMemoryAddress.playerId, destMemoryAddress.unitId, NumericModifier::Add, unshiftedValue);
        if ( !destructive ) {
            setDeaths(strippedBits.playerId, strippedBits.unitId, NumericModifier::Add, shiftedValue);
            restoreActions.push(RestoreAction(strippedBits, shiftedValue));
        }
    }
    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::checkEqual(u32 constant)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);

    u32 valueMin = constant << bitsAfterAddress;
    u32 valueMax = ((constant+1) << bitsAfterAddress)-1;

    trigger(owners);
    if ( valueMin == valueMax )
        deaths(targetAddress, NumericComparison::Exactly, valueMin);
    else
    {
        deaths(targetAddress, NumericComparison::AtLeast, valueMin);
        deaths(targetAddress, NumericComparison::AtMost, valueMax);
    }

    setSwitch(0, SwitchModifier::Set);
    
    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::checkAtLeast(u32 constant)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);

    u32 valueMin = constant << bitsAfterAddress;
    trigger(owners);
    deaths(targetAddress, NumericComparison::AtLeast, valueMin);
    setSwitch(0, SwitchModifier::Set);

    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::checkAtMost(u32 constant)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);

    u32 valueMax = ((constant+1) << bitsAfterAddress)-1;
    trigger(owners);
    deaths(targetAddress, NumericComparison::AtMost, valueMax);
    setSwitch(0, SwitchModifier::Set);

    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::checkGreaterThan(u32 constant)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);

    u32 valueMin = (constant+1) << bitsAfterAddress;
    trigger(owners);
    deaths(targetAddress, NumericComparison::AtLeast, valueMin);
    setSwitch(0, SwitchModifier::Set);

    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress, NumericModifier::Add, action.modification);
    }
    return true;
}

bool EudOpsTrigGen::checkLessThan(u32 constant)
{
    u32 address = targetAddress.address;
    u32 bitLength = targetAddress.bitLength;
    u32 bitsBeforeAddress = 8 * (address % 4 == 0 ? 0 : 3-address % 4);
    u32 bitsAfterAddress = 32 - bitsBeforeAddress - bitLength;
    DeathCounter strippedBits = genData.getSlackSpaceDc();
    u32 bit = 0;

    for (; bit < bitsBeforeAddress; bit++)
        stripBit(strippedBits, bit, true);

    u32 valueMax = (constant << bitsAfterAddress)-1;
    trigger(owners);
    deaths(targetAddress, NumericComparison::AtMost, valueMax);
    setSwitch(0, SwitchModifier::Set);

    while (!restoreActions.empty())
    {
        RestoreAction action = restoreActions.top();
        restoreActions.pop();
        trigger(owners);
        deaths(strippedBits, NumericComparison::AtLeast, action.modification);
        setDeaths(strippedBits, NumericModifier::Subtract, action.modification);
        setDeaths(targetAddress, NumericModifier::Add, action.modification);
    }
    return true;
}

void EudOpsTrigGen::stripBit(DeathCounter slackSpace, u32 bit, bool restore)
{
    s64 change = pow(2, (31 - bit));
    trigger(owners);
    deaths(targetAddress, NumericComparison::AtLeast, change);
    setDeaths(targetAddress, NumericModifier::Subtract, change);
    if (restore)
    {
        setDeaths(slackSpace, NumericModifier::Add, change);
        restoreActions.push(RestoreAction(slackSpace, change));
    }
}

void EudOpsTrigGen::trigger(u8* players) {
    if ( triggerCount > 0 )
        dummyMap->addTrigger(currTrig);

    triggerCount++;
    didComment = false;
    currTrig = Trigger();
    memcpy(currTrig.players, players, 28);
    /*out << "Trigger(" << players << "){" << endl
        << "Conditions:" << endl;*/
}

void EudOpsTrigGen::end() {
    if ( triggerCount > 0 )
        dummyMap->addTrigger(currTrig);
}

// Accumulate
// Always
bool EudOpsTrigGen::always() {
    Condition condition = Condition(ConditionId::Always);
    return currTrig.addCondition(condition);
    //out << "	Always();" << endl;
}
// Bring
// Command
// Command the Least
// Command the Least At
// Command the Most
// Commands the Most At
// Countdown Timer
// Deaths
bool EudOpsTrigGen::deaths(u32 playerId, u32 unitId, NumericComparison numericComparison, u32 amount) {
    Condition condition(ConditionId::Deaths);
    condition.players = playerId;
    condition.unitID = unitId;
    condition.comparison = (u8)numericComparison;
    condition.amount = amount;
    return currTrig.addCondition(condition);
}
bool EudOpsTrigGen::deaths(DeathCounter deathCounter, NumericComparison numericComparison, u32 amount) {
    Condition condition(ConditionId::Deaths);
    condition.players = deathCounter.playerId;
    condition.unitID = deathCounter.unitId;
    condition.comparison = (u8)numericComparison;
    condition.amount = amount;
    return currTrig.addCondition(condition);
}
bool EudOpsTrigGen::deaths(EudAddress eudAddress, NumericComparison numericComparison, u32 amount) {
    Condition condition(ConditionId::Deaths);
    condition.players = eudAddress.playerId;
    condition.unitID = eudAddress.unitId;
    condition.comparison = (u8)numericComparison;
    condition.amount = amount;
    return currTrig.addCondition(condition);
}
// Elapsed Time
// Highest Score
// Kill
// Least Kills
// Least Resources
// Lowest Score
// Memory
bool EudOpsTrigGen::memory(u32 address, NumericComparison numericComparison, u32 value) {
    Condition condition(ConditionId::Deaths);
    condition.players = address;
    condition.unitID = 0;
    condition.comparison = (u8)numericComparison;
    condition.amount = value;
    return currTrig.addCondition(condition);
}
bool EudOpsTrigGen::memory(DeathCounter deathCounter, NumericComparison numericComparison, u32 value) {
    Condition condition(ConditionId::Deaths);
    condition.players = deathCounter.playerId;
    condition.unitID = deathCounter.unitId;
    condition.comparison = (u8)numericComparison;
    condition.amount = value;
    return currTrig.addCondition(condition);
}
bool EudOpsTrigGen::memory(EudAddress eudAddress, NumericComparison numericComparison, u32 value) {
    Condition condition(ConditionId::Deaths);
    condition.players = eudAddress.playerId;
    condition.unitID = eudAddress.unitId;
    condition.comparison = (u8)numericComparison;
    condition.amount = value;
    return currTrig.addCondition(condition);
}
// Most Kills
// Most Resources
// Never
// Opponents
// Score
// Switch
bool EudOpsTrigGen::switchState(u32 switchNum, SwitchState state) {
    Condition condition(ConditionId::Switch);
    condition.typeIndex = switchNum;
    condition.comparison = (u8)state;
    return currTrig.addCondition(condition);
    //out << "	Switch(\"" << switchTitle << "\", " << state << ");" << endl;
}


// Center View
// Comment
bool EudOpsTrigGen::comment(const std::string &text) {
    bool success = false;
    if (!noComments)
    {
        ChkdString str(emptyComments ? "" : text);
        Action action(ActionId::Comment);
        u32 stringNum = 0;
        if ( dummyMap->addString<u32>(ChkdString(text), stringNum, false) )
        {
            action.stringNum = stringNum;
            success = currTrig.addAction(action);
        }
        //out << "	Comment(\"\");" << endl;
        //out << "	Comment(\"" << text << "\");" << endl;
    }
    didComment = true;
    return success;
}
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
bool EudOpsTrigGen::preserveTrigger() {
    Action action = Action(ActionId::PreserveTrigger);
    return currTrig.addAction(action);
    //out << "	Preserve Trigger();" << endl;
}
// Remove Unit
// Remove Unit At Location
// Run AI Script
// Run AI Script At Location
// Set Alliance Status
// Set Countdown Timer
// Set Deaths
bool EudOpsTrigGen::setDeaths(u32 playerId, u32 unitId, NumericModifier numericModifier, u32 value) {
    Action action = Action(ActionId::SetDeaths);
    action.group = playerId;
    action.type = unitId;
    action.type2 = (u8)numericModifier;
    action.number = value;
    return currTrig.addAction(action);
}
bool EudOpsTrigGen::setDeaths(DeathCounter deathCounter, NumericModifier numericModifier, u32 value) {
    Action action = Action(ActionId::SetDeaths);
    action.group = deathCounter.playerId;
    action.type = deathCounter.unitId;
    action.type2 = (u8)numericModifier;
    action.number = value;
    return currTrig.addAction(action);
}
bool EudOpsTrigGen::setDeaths(EudAddress eudAddress, NumericModifier numericModifier, u32 value) {
    Action action = Action(ActionId::SetDeaths);
    action.group = eudAddress.playerId;
    action.type = eudAddress.unitId;
    action.type2 = (u8)numericModifier;
    action.number = value;
    return currTrig.addAction(action);
}
// Set Doodad State
// Set Invincibility
// Set Mission Objectives
// Set Next Scenario
// Set Resources
// Set Score
// Set Switch
bool EudOpsTrigGen::setSwitch(u32 switchNum, SwitchModifier switchModifier) {
    Action action = Action(ActionId::SetSwitch);
    action.number = switchNum;
    action.type2 = (u8)switchModifier;
    return currTrig.addAction(action);
    //out << "	Set Switch(\"" << Switch << "\", " << state << ");" << endl;
}
// Talking Portrait
// Transmission
// Unmute Unit Speech
// Unpause Game
// Unpause Timer
// Victory
// Wait
