//
// Copyright (C)

#include <string>

#include "../Common/xkslangUtils.h"
#include "../SpxMixer/SpxCompiler.h"
#include "../XkslParser/XkslParser.h"
#include "../SpxMixer/SpxMixer.h"
#include "XkfxParser.h"

using namespace std;
using namespace xkfxProcessor;
using namespace xkslang;

//===================================================================================================================================
//===================================================================================================================================
// Utils
static bool error(vector<string>& msgs, const string& msg)
{
    msgs.push_back(msg);
    return false;
}

bool XkfxParser::SeparateAdotB(const string str, string& A, string& B)
{
    size_t pdot = str.find_first_of('.');
    if (pdot == string::npos) return false;
    A = str.substr(0, pdot);
    B = str.substr(pdot + 1);
    return true;
}

bool XkfxParser::StartWith(const char* txt, const char* word)
{
    while (*txt && *word)
    {
        if (*txt++ != *word++) return false;
    }

    return (*word == 0);
}

bool XkfxParser::GetNextStringExpression(const char* txt, char* const outputBuffer, int bufferMaxSize, int* expressionLen)
{
    if (txt == nullptr) return false;

    const char* startPos = txt;
    while (*startPos != '\"' && *startPos != '\0') startPos++;
    if (*startPos == '\0') return false;
    const char* endPos = startPos + 1;

    char c;
    while ((c = *endPos) != '\0')
    {
        if (c == '\"') break;
        endPos++;
    }

    if (*endPos == '\0') return false;

    int wordLen = (endPos - startPos) + 1;
    if (wordLen <= 0 || wordLen >= bufferMaxSize - 1) return false;
    strncpy_s(outputBuffer, bufferMaxSize, startPos, wordLen);

    return true;
}

bool XkfxParser::GetNextWord(const char* txt, const char** nextWordStart, int* nextWordLen, const char** followingWordStart, char additionnalStopDelimiters)
{
    if (txt == nullptr) return false;

    const char* startPos = txt;
    while (*startPos == ' ' || *startPos == '\t') startPos++; //skip starting spaces and tabulations
    const char* endPos = startPos;

    char c;
    while ((c = *endPos) != '\0')
    {
        if (c == ' ' || c == '\t' || c == additionnalStopDelimiters)
        {
            break;
        }
        endPos++;
    }

    int wordLen = (endPos - startPos);
    if (wordLen <= 0) return false;

    *nextWordStart = startPos;
    *nextWordLen = wordLen;

    if (c == '\0') *followingWordStart = nullptr;
    else *followingWordStart = endPos;

    return true;
}

bool XkfxParser::GetAndCopyNextWord(const char* txt, char* nextWordBuffer, int bufferMaxSize, int* nextWordLen, const char** followingWordStart, char additionnalStopDelimiters)
{
    const char* startPos;
    if (!XkfxParser::GetNextWord(txt, &startPos, nextWordLen, followingWordStart, additionnalStopDelimiters)) return false;

    int wordLen = *nextWordLen;
    if (wordLen <= 0 || wordLen >= bufferMaxSize - 1) return false;
    strncpy_s(nextWordBuffer, bufferMaxSize, startPos, wordLen);

    return true;
}

bool XkfxParser::SplitLine(char* txt, char** nextLine)
{
    if (txt == nullptr) return false;

    char* curPos = txt;
    char c;
    while ((c = *curPos) != '\0')
    {
        if (c == '\n')
        {
            *curPos = '\0';
            *nextLine = curPos + 1;
            return true;
        }
        curPos++;
    }

    if (curPos == txt) return false; //empty string

    *nextLine = nullptr;
    return true;
}

bool XkfxParser::IsCommandLineInstructionComplete(const char* pInstruction)
{
    int countParenthesis = 0;
    int countBrackets = 0;
    int countComparaisonSigns = 0;

    while (*pInstruction != 0)
    {
        char c = *pInstruction++;
        switch (c)
        {
            case '[': countBrackets++; break;
            case ']': countBrackets--; break;
            case '(': countParenthesis++; break;
            case ')': countParenthesis--; break;
            case '<': countComparaisonSigns++; break;
            case '>': countComparaisonSigns--; break;
        }
    }

    return (countParenthesis == 0 && countComparaisonSigns == 0 && countBrackets == 0);
}

bool XkfxParser::getFunctionParameterString(const char* txt, const char** stringStart, int* stringLen)
{
    if (txt == nullptr) return false;
    int txtLen = strlen(txt);

    const char* pStart = txt;
    while (*pStart == ' ' || *pStart == '\t') pStart++;
    if (*pStart != '(') return false;
    pStart++;
    while (*pStart == ' ' || *pStart == '\t') pStart++;

    const char* pEnd = txt + (txtLen - 1);
    while (pEnd > pStart && *pEnd != ')') pEnd--;
    if (*pEnd != ')') return false;
    pEnd--;
    while (*pEnd == ' ' || *pEnd == '\t') pEnd--;

    int len = (pEnd - pStart) + 1;
    if (len <= 0)
    {
        *stringLen = 0;
        return true;
    }

    *stringLen = len;
    *stringStart = pStart;
    
    return true;
}

bool XkfxParser::GetNextInstruction(const std::string& line, std::string& firstInstruction, std::string& remainingLine)
{
    unsigned int startPos = 0;
    unsigned int len = line.size();

    while (startPos < len && line[startPos] == ' ') startPos++; // skip front spaces
    if (startPos == len) return false;

    char c;
    unsigned int endPos = startPos;
    while (true)
    {
        c = line[endPos];
        if (c == ' ') break;
        if (++endPos == len) break;
    }

    if (endPos == startPos) firstInstruction = "";
    else firstInstruction = line.substr(startPos, (endPos - startPos));
    if (endPos == len) remainingLine = "";
    else remainingLine = line.substr(endPos + 1);

    return true;
}

bool XkfxParser::GetNextInstruction(const string& line, string& firstInstruction, string& remainingLine, const char stopDelimiters, bool keepTheStopDelimiterInTheRemainingString)
{
    unsigned int startPos = 0;
    unsigned int len = line.size();

    while (startPos < len && line[startPos] == ' ') startPos++; // skip front spaces
    if (startPos == len) return false;

    char c;
    unsigned int endPos = startPos;
    while (true)
    {
        c = line[endPos];

        if (c == ' ')
        {
            firstInstruction = line.substr(startPos, (endPos - startPos));
            remainingLine = line.substr(endPos);
            return true;
        }

        if (c == stopDelimiters)
        {
            firstInstruction = line.substr(startPos, (endPos - startPos));
            if (!keepTheStopDelimiterInTheRemainingString) endPos++;
            remainingLine = line.substr(endPos);
            return true;
        }

        if (++endPos == len)
        {
            firstInstruction = line;
            remainingLine = "";
            return true;
        }
    }
}

string XkfxParser::GetUnmangledName(const string& fullName)
{
    size_t pos = fullName.find_first_of('<');
    if (pos == string::npos) return fullName;
    return fullName.substr(0, pos);
}

bool XkfxParser::SplitParametersString(const char* parameterStr, vector<string>& parameters)
{
    if (parameterStr == nullptr) return false;

    const char* ptrStr = parameterStr;
    int len = strlen(parameterStr);

    char c;
    int start = 0;
    int countParenthesis = 0;
    int countBrackets = 0;
    int countComparaisonSigns = 0;
    while (true)
    {
        while (ptrStr[start] == ' ' || ptrStr[start] == ',') start++;
        if (start >= len) return true;

        int end = start;
        bool loop = true;
        while (loop)
        {
            if (end == len) {
                end--;
                break;
            }

            c = ptrStr[end++];
            switch (c)
            {
                case '[': countBrackets++; break;
                case ']': countBrackets--; break;
                case '(': countParenthesis++; break;
                case ')': countParenthesis--; break;
                case '<': countComparaisonSigns++; break;
                case '>': countComparaisonSigns--; break;
                case ',':
                {
                    if (countParenthesis == 0 && countComparaisonSigns == 0 && countBrackets == 0) {
                        loop = false;
                        end--;
                    }
                    break;
                }
            }
        }

        int lastChar = end;
        while (ptrStr[lastChar] == ' ' || ptrStr[lastChar] == ',') lastChar--;
        
        parameters.push_back(string(ptrStr + start, (lastChar - start) + 1));

        start = end + 1;
        if (start >= len) return true;
    }
}

//===================================================================================================================================
//===================================================================================================================================
// Xkfx parsing management
SpxBytecode* XkfxParser::GetSpxBytecodeForShader(const string& shaderName, string& shaderFullName,
    unordered_map<string, SpxBytecode*>& mapShaderNameBytecode, bool canLookIfUnmangledNameMatch, vector<string>& errorMsgs)
{
    auto it = mapShaderNameBytecode.find(shaderName);
    if (it != mapShaderNameBytecode.end())
    {
        SpxBytecode* spxBytecode = it->second;
        shaderFullName = it->first;
        return spxBytecode;
    }

    if (canLookIfUnmangledNameMatch)
    {
        SpxBytecode* spxBytecode = nullptr;
        for (auto it = mapShaderNameBytecode.begin(); it != mapShaderNameBytecode.end(); it++)
        {
            string anUnmangledShaderName = GetUnmangledName(it->first);
            if (anUnmangledShaderName == shaderName)
            {
                if (spxBytecode == nullptr)
                {
                    spxBytecode = it->second;
                    shaderFullName = it->first;
                }
                else
                {
                    error(errorMsgs, "2 or more shaders match the unmangled shader name: " + anUnmangledShaderName);
                    return nullptr;
                }
            }
        }
        return spxBytecode;
    }

    return nullptr;
}

class XkEffectMixerObject
{
public:
    string name;
    SpxMixer* mixer;

    XkEffectMixerObject(string name, SpxMixer* mixer) : name(name), mixer(mixer) {}
};

//===================================================================================================================================
//===================================================================================================================================
// Parsing
static bool RecordSPXShaderBytecode(string shaderFullName, SpxBytecode* spxBytecode, unordered_map<string, SpxBytecode*>& mapShaderNameBytecode, vector<string>& errorMsgs)
{
    mapShaderNameBytecode[shaderFullName] = spxBytecode;
    return true;
}

static bool RecursivelyParseAndConvertXkslShader(XkslParser* parser, const string& shaderName, glslang::CallbackRequestDataForShader callbackRequestDataForShader,
    const vector<ShaderGenericValues>& listGenericsValue, const vector<XkslUserDefinedMacro>& listUserDefinedMacros, SpxBytecode& spirXBytecode, string& infoMsg)
{
    ostringstream errorAndDebugMessages;
    bool success = parser->ConvertShaderToSpx(shaderName, callbackRequestDataForShader, listGenericsValue, listUserDefinedMacros, spirXBytecode, &errorAndDebugMessages);
    infoMsg = errorAndDebugMessages.str();

    return success;
}

static bool ConvertAndLoadRecursif(const string& stringShaderAndgenericsValue, const vector<XkslUserDefinedMacro>& listUserDefinedMacros, glslang::CallbackRequestDataForShader callbackRequestDataForShader,
    unordered_map<string, SpxBytecode*>& mapShaderNameBytecode, vector<SpxBytecode*>& listAllocatedBytecodes, XkslParser* parser, vector<string>& errorMsgs)
{
    //================================================
    //Parse and get the list of shader defintion
    vector<ShaderParsingDefinition> listshaderDefinition;
    if (!XkslParser::ParseStringWithShaderDefinitions(stringShaderAndgenericsValue.c_str(), listshaderDefinition))
        return error(errorMsgs, "Failed to parse the shaders definition from: " + stringShaderAndgenericsValue);
    vector<ShaderGenericValues> listShaderAndGenerics;
    for (unsigned int is = 0; is < listshaderDefinition.size(); is++) {
        listShaderAndGenerics.push_back(ShaderGenericValues(listshaderDefinition[is].shaderName, listshaderDefinition[is].genericsValue));
    }
    if (listShaderAndGenerics.size() == 0) return error(errorMsgs, "No shader name found");
    string shaderName = listShaderAndGenerics[0].shaderName;

    //================================================
    //Parse and convert the shader and its dependencies
    SpxBytecode* spxBytecode = nullptr;
    vector<string> vecShadersParsed;
    
    string infoMsg;
    spxBytecode = new SpxBytecode;
    listAllocatedBytecodes.push_back(spxBytecode);
    string spxOutputFileName;

    bool success = RecursivelyParseAndConvertXkslShader(parser, shaderName, callbackRequestDataForShader, listShaderAndGenerics, listUserDefinedMacros, *spxBytecode, infoMsg);

    if (!success) {
        error(errorMsgs, infoMsg);
        return error(errorMsgs, "Failed to parse and convert the XKSL file name: " + shaderName);
    }

    //Query the list of shaders from the bytecode
    if (!SpxMixer::GetListAllShadersFromBytecode(*spxBytecode, vecShadersParsed, errorMsgs))
    {
        error(errorMsgs, "Failed to get the list of shader names for: " + shaderName);
        return false;
    }

    //Store the new shaders bytecode in our map
    for (unsigned int is = 0; is < vecShadersParsed.size(); ++is)
    {
        string shaderName = vecShadersParsed[is];
        if (!RecordSPXShaderBytecode(shaderName, spxBytecode, mapShaderNameBytecode, errorMsgs))
        {
            return error(errorMsgs, "Can't add the shader into the bytecode: " + shaderName);
            return false;
        }
    }

    return true;
}

//===================================================================================================================================
//===================================================================================================================================
// Compilation
static bool CompileMixer(SpxMixer* mixer, vector<uint32_t>* compiledBytecode, vector<string>& errorMsgs)
{
    bool success = true;

    string vertexShaderMethodName = "VSMain(";
    string hullShaderMethodName = "HSMain(";
    string hullConstantShaderMethodName = "HSConstantMain(";
    string domainShaderMethodName = "DSMain(";
    string geometryShaderMethodName = "GSMain(";
    string pixelShaderMethodName = "PSMain(";
    string computeShaderMethodName = "CSMain(";

    //We look for the output stages, depending on the stage functions found in the mixer
    vector<MethodInfo> vecMethods;
    if (!mixer->GetListAllMethodsInfo(vecMethods, errorMsgs))
        return error(errorMsgs, "Failed to get the mixer list of methods");
    unsigned int countMethods = vecMethods.size();
    vector<OutputStageBytecode> outputStages;
    for (unsigned int m = 0; m < countMethods; m++)
    {
        const MethodInfo& aMethod = vecMethods[m];

        if (aMethod.Name == vertexShaderMethodName) outputStages.push_back(OutputStageBytecode(ShadingStageEnum::Vertex, "VSMain"));
        else if (aMethod.Name == hullShaderMethodName) outputStages.push_back(OutputStageBytecode(ShadingStageEnum::Vertex, "HSMain"));
        else if (aMethod.Name == hullConstantShaderMethodName) outputStages.push_back(OutputStageBytecode(ShadingStageEnum::Vertex, "HSConstantMain"));
        else if (aMethod.Name == domainShaderMethodName) outputStages.push_back(OutputStageBytecode(ShadingStageEnum::Vertex, "DSMain"));
        else if (aMethod.Name == geometryShaderMethodName) outputStages.push_back(OutputStageBytecode(ShadingStageEnum::Vertex, "GSMain"));
        else if (aMethod.Name == pixelShaderMethodName) outputStages.push_back(OutputStageBytecode(ShadingStageEnum::Vertex, "PSMain"));
        else if (aMethod.Name == computeShaderMethodName) outputStages.push_back(OutputStageBytecode(ShadingStageEnum::Vertex, "CSMain"));
    }

    success = mixer->Compile(outputStages, errorMsgs, nullptr, nullptr, nullptr, nullptr, compiledBytecode, nullptr);

    if (!success) return error(errorMsgs, "Compilation Failed");
    return success;
}

//===================================================================================================================================
//===================================================================================================================================
// Compositions

//function prototype
static bool MixinShaders(const char* mixinShadersInstructions, XkEffectMixerObject* mixerTarget, glslang::CallbackRequestDataForShader callbackRequestDataForShader,
    unordered_map<string, SpxBytecode*>& mapShaderNameBytecode, unordered_map<string, XkEffectMixerObject*>& mixerMap,
    vector<SpxBytecode*>& listAllocatedBytecodes, const vector<XkslUserDefinedMacro>& listUserDefinedMacros, XkslParser* parser, vector<string>& errorMsgs);
static XkEffectMixerObject* CreateAndAddNewMixer(unordered_map<string, XkEffectMixerObject*>& mixerMap, string newMixerName, vector<string>& errorMsgs);

static bool AddCompositionToMixer(XkEffectMixerObject* mixerTarget, const char* compositionStringInstructions, const string& targetedShaderName, glslang::CallbackRequestDataForShader callbackRequestDataForShader,
    unordered_map<string, SpxBytecode*>& mapShaderNameBytecode, unordered_map<string, XkEffectMixerObject*>& mixerMap,
    vector<SpxBytecode*>& listAllocatedBytecodes, const vector<XkslUserDefinedMacro>& listUserDefinedMacros, XkslParser* parser, vector<string>& errorMsgs)
{
    bool success = false;

    //===================================================
    //composition variable target
    const char* pCompositionTarget;
    int nextWordLen;
    const char* pCompositionInstructionStart;
    if (!XkfxParser::GetNextWord(compositionStringInstructions, &pCompositionTarget, &nextWordLen, &pCompositionInstructionStart, '=')) {
        return error(errorMsgs, string("Failed to get the next instruction from: ") + compositionStringInstructions);
    }

    string shaderName;
    string variableName;

    //We can either have shader.variableName, or only a variableName, look for '.' character
    const char* dotPos = strchr(pCompositionTarget, '.');
    if (dotPos != nullptr)
    {
        variableName = string(dotPos + 1, nextWordLen - (dotPos - pCompositionTarget) - 1);
        shaderName = string(pCompositionTarget, (dotPos - pCompositionTarget));
    }
    else
    {
        variableName = string(pCompositionTarget, nextWordLen);
    }

    //===================================================
    //Check for expected '=' character
    if (pCompositionInstructionStart == nullptr) return error(errorMsgs, "\"=\" expected");
    while (*pCompositionInstructionStart == ' ' || *pCompositionInstructionStart == '\t') pCompositionInstructionStart++;
    if (*pCompositionInstructionStart != '=') return error(errorMsgs, "\"=\" expected");
    pCompositionInstructionStart++;

    //trim start
    while (*pCompositionInstructionStart == ' ' || *pCompositionInstructionStart == '\t') pCompositionInstructionStart++;
    //trim end
    int compositionsInstructionLen = strlen(pCompositionInstructionStart);
    while (compositionsInstructionLen > 0 && (pCompositionInstructionStart[compositionsInstructionLen - 1] == ' ' || pCompositionInstructionStart[compositionsInstructionLen - 1] == '\t'))
        compositionsInstructionLen--;
    if (compositionsInstructionLen == 0) return error(errorMsgs, "No composition instruction found");

    //===================================================
    //Find or create the composition source mixer
    //We can either have a mixer name, or a mixin instruction
    //string mixerCompositionInstructionsStr = pCompositionInstructionStart;

    vector<string> listCompositionInstructions;
    if (*pCompositionInstructionStart == '[')
    {
        if ((pCompositionInstructionStart[compositionsInstructionLen - 1] != ']'))
            return error(errorMsgs, string("Invalid compositions instruction string: ") + pCompositionInstructionStart);

        string mixerCompositionInstructionsStr(pCompositionInstructionStart + 1, compositionsInstructionLen - 1);

        //If the instruction start with '[', there are several compositions assigned to the target
        if (!XkfxParser::SplitParametersString(mixerCompositionInstructionsStr.c_str(), listCompositionInstructions))
            return error(errorMsgs, "Failed to split the compositions instruction string: " + mixerCompositionInstructionsStr);
    }
    else listCompositionInstructions.push_back(string(pCompositionInstructionStart));

    const char* instruction_mixin = "mixin";

    for (unsigned int iComp = 0; iComp < listCompositionInstructions.size(); iComp++)
    {
        const string& aCompositionInstruction = listCompositionInstructions[iComp];
        const char* paCompositionInstruction = aCompositionInstruction.c_str();

        XkEffectMixerObject* compositionSourceMixer = nullptr;

        if (XkfxParser::StartWith(paCompositionInstruction, instruction_mixin))
        {
            paCompositionInstruction += strlen(instruction_mixin);

            //trim start
            while (*paCompositionInstruction == ' ' || *paCompositionInstruction == '\t') paCompositionInstruction++;
            //trim end
            int mixinInstructionLen = strlen(paCompositionInstruction);
            while (mixinInstructionLen > 0 && (paCompositionInstruction[mixinInstructionLen - 1] == ' ' || paCompositionInstruction[mixinInstructionLen - 1] == '\t')) mixinInstructionLen--;
            if (mixinInstructionLen == 0) return error(errorMsgs, "Invalid composition instruction: " + aCompositionInstruction);

            //we can either have "mixin(...)" or "mixin ..."
            if (*paCompositionInstruction == '(')
            {
                if (paCompositionInstruction[mixinInstructionLen - 1] != ')') return error(errorMsgs, "Invalid composition instruction: \")\" expected. " + aCompositionInstruction);
                paCompositionInstruction++;
                mixinInstructionLen -= 2;
            }

            //We create a new, anonymous mixer and directly mix the shader specified in the function parameter
            string anonymousMixerInstruction(paCompositionInstruction, mixinInstructionLen);

            //Create the anonymous mixer
            string anonymousMixerName = "_anonMixer_" + to_string(mixerMap.size());
            XkEffectMixerObject* anonymousMixer = CreateAndAddNewMixer(mixerMap, anonymousMixerName, errorMsgs);
            if (anonymousMixer == nullptr) return error(errorMsgs, "Failed to create a new mixer object");

            //Mix the new mixer with the shaders specified in the function parameter
            success = MixinShaders(anonymousMixerInstruction.c_str(), anonymousMixer, callbackRequestDataForShader, mapShaderNameBytecode, mixerMap, listAllocatedBytecodes, listUserDefinedMacros, parser, errorMsgs);
            if (!success) return error(errorMsgs, "Mixin failed: " + anonymousMixerInstruction);

            compositionSourceMixer = anonymousMixer;
        }
        else
        {
            const string& mixerName = aCompositionInstruction;

            //find the mixer in our mixer map
            if (mixerMap.find(mixerName) == mixerMap.end()) {
                return error(errorMsgs, "addComposition: no mixer found with the name: \"" + mixerName + "\"");
            }
            compositionSourceMixer = mixerMap[mixerName];
        }

        if (compositionSourceMixer == nullptr) {
            return error(errorMsgs, "No mixer source found to make the composition");
        }

        //=====================================
        // Add the composition to the mixer
        success = mixerTarget->mixer->AddCompositionInstance(shaderName, variableName, compositionSourceMixer->mixer, errorMsgs);
        if (!success) return error(errorMsgs, "Failed to add the composition to the mixer");
    }

    return true;
}

static bool AddCompositionsToMixer(XkEffectMixerObject* mixerTarget, const char* compositionsParametersString, const string& targetedShaderName, glslang::CallbackRequestDataForShader callbackRequestDataForShader,
    unordered_map<string, SpxBytecode*>& mapShaderNameBytecode, unordered_map<string, XkEffectMixerObject*>& mixerMap,
    vector<SpxBytecode*>& listAllocatedBytecodes, const vector<XkslUserDefinedMacro>& listUserDefinedMacros, XkslParser* parser, vector<string>& errorMsgs)
{
    //split the string
    vector<string> compositions;
    if (!XkfxParser::SplitParametersString(compositionsParametersString, compositions))
        return error(errorMsgs, "failed to split the parameters");

    if (compositions.size() == 0)
        return error(errorMsgs, "No composition found in the parameter string");

    for (unsigned int k = 0; k < compositions.size(); ++k)
    {
        const string& compositionStr = compositions[k];
        bool success = AddCompositionToMixer(mixerTarget, compositionStr.c_str(), targetedShaderName, callbackRequestDataForShader, mapShaderNameBytecode, mixerMap,
            listAllocatedBytecodes, listUserDefinedMacros, parser, errorMsgs);

        if (!success) return error(errorMsgs, "Failed to add the composition into the mixer: " + compositionStr);
    }

    return true;
}

//===================================================================================================================================
//===================================================================================================================================
// Mixin

static XkEffectMixerObject* CreateAndAddNewMixer(unordered_map<string, XkEffectMixerObject*>& mixerMap, string newMixerName, vector<string>& errorMsgs)
{
    if (mixerMap.find(newMixerName) != mixerMap.end()) {
        error(errorMsgs, "CreateAndAddNewMixer: a mixer already exists with the name:" + newMixerName);
        return nullptr;
    }

    XkEffectMixerObject* mixerObject = nullptr;
    SpxMixer* mixer = new SpxMixer();
    mixerObject = new XkEffectMixerObject(newMixerName, mixer);

    mixerMap[newMixerName] = mixerObject;
    return mixerObject;
}

static bool MixinShaders(const char* mixinShadersInstructions, XkEffectMixerObject* mixerTarget, glslang::CallbackRequestDataForShader callbackRequestDataForShader,
    unordered_map<string, SpxBytecode*>& mapShaderNameBytecode, unordered_map<string, XkEffectMixerObject*>& mixerMap,
    vector<SpxBytecode*>& listAllocatedBytecodes, const vector<XkslUserDefinedMacro>& listUserDefinedMacros, XkslParser* parser, vector<string>& errorMsgs)
{
    if (mixinShadersInstructions == nullptr) return error(errorMsgs, "No shader to mix");
    bool success = true;

    //================================================
    //Parse and get the list of shader defintion
    vector<ShaderParsingDefinition> listShaderDefinition;
    if (!XkslParser::ParseStringWithShaderDefinitions(mixinShadersInstructions, listShaderDefinition))
        return error(errorMsgs, string("mixin: failed to parse the shaders definition from: ") + mixinShadersInstructions);
    if (listShaderDefinition.size() == 0) return error(errorMsgs, "mixin: list of shader is empty");
    string shaderName = listShaderDefinition[0].shaderName;
    
    //================================================
    //Get the bytecode file and the fullName of all shader to mix into the mixer
    vector<pair<string, SpxBytecode*>> listShaderBytecodeToMix; //list of shaders to mix, and their corresponding bytecode
    {
        for (auto its = listShaderDefinition.begin(); its != listShaderDefinition.end(); its++)
        {
            const ShaderParsingDefinition& shaderDef = *its;
            string shaderName = shaderDef.GetShaderNameWithGenerics();
            string shaderFullName;   //we can omit to specify the generics when mixin a shader, we will search the best match

            SpxBytecode* shaderBytecode = XkfxParser::GetSpxBytecodeForShader(shaderName, shaderFullName, mapShaderNameBytecode, true, errorMsgs);
            if (shaderBytecode == nullptr)
            {
                //the shader bytecode does not exist, we can try its xksl file to parse and generate it
                success = ConvertAndLoadRecursif(shaderName, listUserDefinedMacros, callbackRequestDataForShader, mapShaderNameBytecode, listAllocatedBytecodes, parser, errorMsgs);
                if (!success) return error(errorMsgs, "Failed to recursively convert and load the shaders: " + shaderName);

                shaderBytecode = XkfxParser::GetSpxBytecodeForShader(shaderName, shaderFullName, mapShaderNameBytecode, true, errorMsgs);

                if (shaderBytecode == nullptr) {
                    return error(errorMsgs, "cannot find or generate a bytecode for the shader: " + shaderName);
                }
            }

            listShaderBytecodeToMix.push_back(pair<string, SpxBytecode*>(shaderFullName, shaderBytecode));
        }
    }

    unsigned int countShadersToMix = listShaderBytecodeToMix.size();
    if (countShadersToMix == 0) return error(errorMsgs, "No bytecode to mix");
    if (countShadersToMix != listShaderDefinition.size()) return error(errorMsgs, "Invalid size");

    //Previous version could mix several shaders at once into the mixer (provided that all shaders are converted into the same bytecode)
    //We now mix one shader after the previous one (to fix the same bytecode issue)
    //(also mixin shaders all-together won't give the same result at mixin one after another one)
    for (unsigned int cs = 0; cs < countShadersToMix; cs++)
    {
        const ShaderParsingDefinition& shaderDefinition = listShaderDefinition[cs];
        string shaderFullName = listShaderBytecodeToMix[cs].first;
        SpxBytecode* shaderBytecode = listShaderBytecodeToMix[cs].second;
        vector<string> listShaderToMix = { shaderFullName }; //mixer can accept a list of shaders to mix (but we disable it for now)

        //=====================================================
        //Mixin the bytecode into the mixer
        {
            success = mixerTarget->mixer->Mixin(*shaderBytecode, listShaderToMix, errorMsgs);
            if (!success) return error(errorMsgs, "Failed to a mix the shader: " + shaderFullName + " into mixer: " + mixerTarget->name);
        }

        string compositionString = shaderDefinition.compositionString;
        if (compositionString.size() > 0)
        {
            //Directly add compositions into the mixed shader
            success = AddCompositionsToMixer(mixerTarget, compositionString.c_str(), shaderFullName, callbackRequestDataForShader, mapShaderNameBytecode, mixerMap,
                listAllocatedBytecodes, listUserDefinedMacros, parser, errorMsgs);

            if (!success) 
                return error(errorMsgs, "Failed to add the compositions instruction to the mixer: " + compositionString);
        }
    }

    return true;
}

//===================================================================================================================================
//===================================================================================================================================
// XKFX command lines parsing
bool XkfxParser::ProcessXkfxCommandLines(XkslParser* p_parser, const string& effectCmdLines, glslang::CallbackRequestDataForShader callbackRequestDataForShader,
    vector<uint32_t>* compiledBytecode, vector<string>& errorMsgs)
{
    bool success = true;
    vector<XkslUserDefinedMacro> listUserDefinedMacros;
    vector<SpxBytecode*> listAllocatedBytecodes;
    unordered_map<string, XkEffectMixerObject*> mixerMap;
    unordered_map<string, SpxBytecode*> mapShaderNameBytecode;

    if (p_parser == nullptr) return error(errorMsgs, "Parser parameter is null");

    //XkslParser parser;
    //if (!parser.InitialiseXkslang())
    //{
    //    error(errorMsgs, "Failed to initialize the XkslParser");
    //    return false;
    //}

    const char* const instruction_break = "break";
    const char* const instruction_setSampleTestOptions = "setSampleTestOptions";
    const char* const instruction_convertAndLoadRecursif = "convertAndLoadRecursif";
    const char* const instruction_setDefine = "setDefine";
    const char* const instruction_mixer = "mixer";
    const char* const instruction_mixin = "mixin";
    const char* const instruction_addComposition = "addComposition";
    const char* const instruction_compile = "compile";
    const char* const instruction_setStageEntryPoint = "setStageEntryPoint";
    const char* const mixerNameInvalidCharacter = "<>()[].,+-/*\\?:;\"{}=&%^";

    int wordBufferMaxSize = 256;
    int instructionBufferSize = 1024;
    char* instructionToParse = new char[instructionBufferSize];
    char* previousPartialInstructionLine = new char[instructionBufferSize];
    char* functionParameterBuffer = new char[instructionBufferSize];
    char* nextWordBuffer = new char[wordBufferMaxSize];
    char* splitWordBuffer = new char[wordBufferMaxSize];
    int nextWordLen;
    previousPartialInstructionLine[0] = '\0';

    unsigned int fileSize = effectCmdLines.length();
    char* const cmdLinesBuffer = new char[fileSize + 1];
    strcpy_s(cmdLinesBuffer, fileSize + 1, effectCmdLines.c_str());
    cmdLinesBuffer[fileSize] = '\0';

    char* currentPosInLinesBuffer = cmdLinesBuffer;
    char* currentLine = nullptr;
    char* nextLine = nullptr;
    while (SplitLine(currentPosInLinesBuffer, &nextLine))
    {
        currentLine = currentPosInLinesBuffer;
        currentPosInLinesBuffer = nextLine;
        if (currentLine == nullptr) break;

        //=======================================================
        //trim line start
        char c = *currentLine;
        while (c == ' ' || c == '\t') c = *(++currentLine);
        
        if (c == '/' && currentLine[1] == '/') {
            //a comment: ignore the line
            continue;
        }

        int currentLineSize = strlen(currentLine);

        //trim line end
        while (currentLineSize > 0 && (currentLine[currentLineSize - 1] == ' ' || currentLine[currentLineSize - 1] == '\t')) {
            currentLine[currentLineSize - 1] = '\0';
            currentLineSize--;
        }

        if (currentLineSize == 0) {
            continue;
        }

        //concatenate with the previous line in the case of the instruction wasn't complete
        if (previousPartialInstructionLine[0] != '\0')
        {
            int instructionToAddsize = strlen(previousPartialInstructionLine);
            int totalSize = instructionToAddsize + currentLineSize;

            if (totalSize >= instructionBufferSize - 1)
            {
                //increment the buffer size
                delete[] instructionToParse;
                delete[] previousPartialInstructionLine;
                delete[] functionParameterBuffer;
                while (totalSize >= instructionBufferSize - 1) instructionBufferSize *= 2;
                instructionToParse = new char[instructionBufferSize];
                previousPartialInstructionLine = new char[instructionBufferSize];
                functionParameterBuffer = new char[instructionBufferSize];
            }
            strcpy_s(instructionToParse, instructionBufferSize, previousPartialInstructionLine);
            strcpy_s(instructionToParse + instructionToAddsize, instructionBufferSize - instructionToAddsize, currentLine);
        }
        else
        {
            if (currentLineSize >= instructionBufferSize - 1)
            {
                //increment the buffer size
                delete[] instructionToParse;
                delete[] previousPartialInstructionLine;
                delete[] functionParameterBuffer;
                while (currentLineSize >= instructionBufferSize - 1) instructionBufferSize *= 2;
                instructionToParse = new char[instructionBufferSize];
                previousPartialInstructionLine = new char[instructionBufferSize];
                functionParameterBuffer = new char[instructionBufferSize];
            }
            strcpy_s(instructionToParse, instructionBufferSize, currentLine);
        }

        //check if the instruction is complete
        if (!XkfxParser::IsCommandLineInstructionComplete(instructionToParse))
        {
            strcpy_s(previousPartialInstructionLine, instructionBufferSize, instructionToParse);
            continue;
        }
        else previousPartialInstructionLine[0] = '\0';

        //=======================================================
        //get the first instruction's word
        const char* followingWordStart;
        if (!XkfxParser::GetAndCopyNextWord(instructionToParse, nextWordBuffer, wordBufferMaxSize, &nextWordLen, &followingWordStart, '(')) {
            error(errorMsgs, string("Failed to get the next instruction from: ") + instructionToParse); break;
        }
    
        if (XkfxParser::StartWith(nextWordBuffer, instruction_break))
        {
            //quit parsing the effect
            break;
        }
        else if (XkfxParser::StartWith(nextWordBuffer, instruction_setSampleTestOptions) ||
                 XkfxParser::StartWith(nextWordBuffer, instruction_convertAndLoadRecursif))
        {
            //ignore those options
            continue;
        }
        else if (XkfxParser::StartWith(nextWordBuffer, instruction_setDefine))
        {
            if (!XkfxParser::GetAndCopyNextWord(followingWordStart, nextWordBuffer, wordBufferMaxSize, &nextWordLen, &followingWordStart)) {
                return error(errorMsgs, string("Failed to get the macro name from: ") + followingWordStart); break;
            }
            string macroName = nextWordBuffer;

            if (!XkfxParser::GetNextStringExpression(followingWordStart, nextWordBuffer, wordBufferMaxSize, &nextWordLen)) {
                return error(errorMsgs, string("Failed to get the macro expression from: ") + followingWordStart); break;
            }
            string macroExpression = nextWordBuffer;

            listUserDefinedMacros.push_back(XkslUserDefinedMacro(macroName, macroExpression));
            continue;
        }
        else if (XkfxParser::StartWith(nextWordBuffer, instruction_mixer))
        {
            if (!XkfxParser::GetAndCopyNextWord(followingWordStart, nextWordBuffer, wordBufferMaxSize, &nextWordLen, &followingWordStart)) {
                error(errorMsgs, string("Failed to get the mixer name from: ") + followingWordStart); break;
            }
            string mixerName = nextWordBuffer;

            if (mixerName.find_first_of(mixerNameInvalidCharacter) != string::npos) {
                error(errorMsgs, "Invalid mixer name: " + mixerName); break;
            }

            XkEffectMixerObject* mixer = CreateAndAddNewMixer(mixerMap, mixerName, errorMsgs);
            if (mixer == nullptr) {
                error(errorMsgs, "Failed to create a new mixer object"); break;
            }
        }
        else //operation on a mixer (mixerName.instructions)
        {
            //mixer name
            const char* mixerInstructionStart;
            if (!XkfxParser::GetAndCopyNextWord(nextWordBuffer, splitWordBuffer, wordBufferMaxSize, &nextWordLen, &mixerInstructionStart, '.')) {
                error(errorMsgs, string("Failed to get the mixer instruction: ") + nextWordBuffer); break;
            }
            string mixerName = splitWordBuffer;

            //mixer instruction
            if (mixerInstructionStart == nullptr || *mixerInstructionStart != '.') {
                error(errorMsgs, string("mixer dot instruction expected: ") + nextWordBuffer); break;
            }
            mixerInstructionStart++;
            //string mixerInstruction = mixerInstructionStart;

            if (mixerMap.find(mixerName) == mixerMap.end()) {
                error(errorMsgs, "no mixer found with the name: " + mixerName); break;
            }
            XkEffectMixerObject* mixerTarget = mixerMap[mixerName];

            //get the function parameter string
            const char* parameterStringStart;
            int parameterStringLen = 0;
            if (followingWordStart != nullptr) {
                if (!XkfxParser::getFunctionParameterString(followingWordStart, &parameterStringStart, &parameterStringLen)) {
                    error(errorMsgs, string("Failed to find the parameter string from: ") + followingWordStart); break;
                }
            }
            if (parameterStringLen <= 0) functionParameterBuffer[0] = '\0';
            else strncpy_s(functionParameterBuffer, instructionBufferSize, parameterStringStart, parameterStringLen);

            if (XkfxParser::StartWith(mixerInstructionStart, instruction_mixin))
            {
                if (parameterStringLen <= 0) { error(errorMsgs, "mixin: parameters expected"); break; }

                success = MixinShaders(functionParameterBuffer, mixerTarget, callbackRequestDataForShader, mapShaderNameBytecode, mixerMap, listAllocatedBytecodes, listUserDefinedMacros, p_parser, errorMsgs);
                if (!success) { error(errorMsgs, "Mixin failed"); break; }
            }
            else if (XkfxParser::StartWith(mixerInstructionStart, instruction_addComposition))
            {
                if (parameterStringLen <= 0) { error(errorMsgs, "addComposition: parameters expected"); break; }

                success = AddCompositionsToMixer(mixerTarget, functionParameterBuffer, "", callbackRequestDataForShader, mapShaderNameBytecode, mixerMap,
                    listAllocatedBytecodes, listUserDefinedMacros, p_parser, errorMsgs);
                if (!success) { error(errorMsgs, "Failed to add the compositions instruction to the mixer"); break; }
            }
            else if (XkfxParser::StartWith(mixerInstructionStart, instruction_compile))
            {
                success = CompileMixer(mixerTarget->mixer, compiledBytecode, errorMsgs);
                if (!success) { error(errorMsgs, "Failed to compile the effect"); break; }
            }
            else if (XkfxParser::StartWith(mixerInstructionStart, instruction_setStageEntryPoint))
            {
                //ignore the instruction
            }
            else
            {
                error(errorMsgs, string("Unknown mixer instruction: ") + mixerInstructionStart);
                break;
            }
        }

    }

    delete[] instructionToParse;
    delete[] previousPartialInstructionLine;
    delete[] cmdLinesBuffer;
    delete[] nextWordBuffer;
    delete[] splitWordBuffer;
    delete[] functionParameterBuffer;

    //Release allocated data
    {
        for (auto itm = mixerMap.begin(); itm != mixerMap.end(); itm++)
        {
            XkEffectMixerObject* mixerObject = itm->second;
            if (mixerObject->mixer != nullptr) delete mixerObject->mixer;
            delete mixerObject;
        }
            
        for (auto itv = listAllocatedBytecodes.begin(); itv != listAllocatedBytecodes.end(); itv++)
        {
            SpxBytecode* spxBytecode = *itv;
            delete spxBytecode;
        }

        //parser.Finalize();
    }

    if (!success || errorMsgs.size() != 0) return false;
    return true;
}
