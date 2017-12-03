//
// Copyright (C) 2016 Google, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of Google, Inc., nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include "hlslTokenStream.h"

namespace glslang {

	void HlslTokenStream::pushTokenBuffer(const HlslToken& tok)
	{
		assert(tokenBufferPos == tokenBuffer.size());  //we cannot push if we're tracking back to a previous position

		tokenBuffer.push_back(tok);
		tokenBufferPos++;
	}

	HlslToken HlslTokenStream::popTokenBuffer()
	{
		// Back up
		assert(tokenBufferPos > 0);

		tokenBufferPos--;
		return tokenBuffer[tokenBufferPos];
	}

    bool HlslTokenStream::advanceUntilFirstTokenFromList(const TVector<EHlslTokenClass>& tokList, bool jumpOverBlocks)
    {
        if (tokenBufferPos == -1) advanceToken();

        for (unsigned int i = 0; i<tokList.size(); ++i)
            if (token.tokenClass == tokList[i]) return true;

        while (true)
        {
            if (jumpOverBlocks)
            {
                switch (token.tokenClass)
                {
                case EHTokLeftBracket:
                    advanceToken();
                    if (!advanceUntilEndOfBlock(EHTokRightBracket)) return false;
                    break;

                case EHTokLeftBrace:
                    advanceToken();
                    if (!advanceUntilEndOfBlock(EHTokRightBrace)) return false;
                    break;

                case EHTokLeftParen:
                    advanceToken();
                    if (!advanceUntilEndOfBlock(EHTokRightParen)) return false;
                    break;
                }
            }

            for (unsigned int i = 0; i<tokList.size(); ++i)
                if (token.tokenClass == tokList[i]) return true;

            advanceToken();
            if (token.tokenClass == EHTokNone) return false;
        }
    }

    bool HlslTokenStream::advanceUntilToken(EHlslTokenClass tok, bool jumpOverBlocks)
    {
        TVector<EHlslTokenClass> listTokens;
        listTokens.push_back(tok);

        return advanceUntilFirstTokenFromList(listTokens, jumpOverBlocks);
    }

    bool HlslTokenStream::advanceUntilEndOfTokenList()
    {
        while (token.tokenClass != EHTokNone)
        {
            advanceToken();
        }
        return true;
    }

    //Advance the token until we reach the end of the block
    bool HlslTokenStream::advanceUntilEndOfBlock(EHlslTokenClass endOfBlockToken)
    {
        while (true)
        {
            if (token.tokenClass == endOfBlockToken)
            {
                advanceToken();
                return true;
            }

            switch (token.tokenClass)
            {
            case EHTokNone:
                return false;

            case EHTokLeftBracket:
                advanceToken();
                if (!advanceUntilEndOfBlock(EHTokRightBracket)) return false;
                break;

            case EHTokLeftBrace:
                advanceToken();
                if (!advanceUntilEndOfBlock(EHTokRightBrace)) return false;
                break;

            case EHTokLeftParen:
                advanceToken();
                if (!advanceUntilEndOfBlock(EHTokRightParen)) return false;
                break;

            default:
                advanceToken();
                break;
            }
        }

        return false;
    }

    bool HlslTokenStream::importListParsedToken(HlslToken* expressionTokensList, int countTokens)
    {
        if (tokenBufferPos == tokenBuffer.size())
        {
            pushTokenBuffer(token);
            tokenBufferPos = (int)(tokenBuffer.size()) - 1;
        }

        for (int i = 0; i < countTokens; ++i)
        {
            tokenBuffer.push_back(*expressionTokensList++);
        }

        return true;
    }

    bool HlslTokenStream::getListPreviouslyParsedToken(HlslToken tokenStart, HlslToken tokenEnd, TVector<HlslToken>& listTokens)
    {
        if (token.IsEqualsToToken(tokenStart)) return true;

        //find the first token in our list of accepted token
        int tokenIndex = -1;
        for (int i = 0; i < tokenBufferPos; ++i)
        {
            if (tokenBuffer[i].IsEqualsToToken(tokenStart))
            {
                tokenIndex = i;
                break;
            }
        }
        if (tokenIndex == -1) return false;

        HlslToken curToken = tokenBuffer[tokenIndex++];
        while (true)
        {
            if (curToken.IsEqualsToToken(tokenEnd)) return true;

            listTokens.push_back(curToken);

            if (tokenIndex == tokenBuffer.size()) return true;
            curToken = tokenBuffer[tokenIndex++];
        }

        return true;
    }

    int HlslTokenStream::getTokenCurrentIndex()
    {
        return tokenBufferPos;
    }

    HlslToken HlslTokenStream::getTokenAtIndex(int index)
    {
        if (index < 0 || index >= (int)tokenBuffer.size()){
            HlslToken invalidToken;
            invalidToken.tokenClass = EHTokNone;
            return invalidToken;
        }

        return tokenBuffer[index];
    }

    TString HlslTokenStream::convertTokenToString(const HlslToken& token)
    {
        return scanner.convertTokenToString(token);
    }

    bool HlslTokenStream::convertTokenToString(const HlslToken& token, TString& str)
    {
        TString word = scanner.convertTokenToString(token);
        if (word.size() == 0) return false;
        
        str = word;
        return true;
    }

    bool HlslTokenStream::recedeToTokenIndex(int index)
    {
        if (index < 0) index = 0;
        if (tokenBufferPos <= index) return true;
        
        if (tokenBufferPos == tokenBuffer.size())
        {
            //save current token at the end of buffer, so that we can push it back
            pushTokenBuffer(token);
        }
        tokenBufferPos = index;
        token = tokenBuffer[tokenBufferPos];
        return true;
    }

	bool HlslTokenStream::recedeToToken(HlslToken tok)
	{
		if (token.IsEqualsToToken(tok)) return true;

		//find the token in our list of accepted token
		int tokenIndex = -1;
		for (int i = 0; i < tokenBufferPos; ++i)
		{
			if (tokenBuffer[i].IsEqualsToToken(tok))
			{
				tokenIndex = i;
				break;
			}
		}
		if (tokenIndex == -1) return false;

		if (tokenBufferPos == tokenBuffer.size())
		{
			//save current token at the end of buffer, so that we can push it back
			pushTokenBuffer(token);
		}
		tokenBufferPos = tokenIndex;
		token = tokenBuffer[tokenBufferPos];
		return true;
	}

    void HlslTokenStream::CopyTokenBufferInto(TVector<HlslToken>& fileTokenList)
    {
        fileTokenList = tokenBuffer;
    }

    bool HlslTokenStream::CopyTokenBufferInto(TVector<HlslToken>& tokenList, int indexStart, int indexEnd)
    {
        if (indexStart < 0 || indexEnd >= (int)tokenBuffer.size() || indexStart > indexEnd) return false;

        for (int k = indexStart; k <= indexEnd; k++) tokenList.push_back(tokenBuffer[k]);
        return true;
    }

    bool HlslTokenStream::pushTokenStream(const TVector<HlslToken>* tokens)
    {
        // save current state
        currentTokenStack.push_back(token);

        // set up new token stream
        tokenStreamStack.push_back(tokens);

        // start position at first token:
        token = (*tokens)[0];
        tokenPosition.push_back(0);

        return true;
    }

    // Undo pushTokenStream(), see above
    bool HlslTokenStream::popTokenStream()
    {
        tokenStreamStack.pop_back();
        tokenPosition.pop_back();
        token = currentTokenStack.back();
        currentTokenStack.pop_back();

        return true;
    }

    bool HlslTokenStream::getListTokensForExpression(const TString& expression, TVector<HlslToken>& listTokens)
    {
        scanner.tokenizeExpression(expression, listTokens);
        return true;
    }

    bool HlslTokenStream::insertListOfTokensAtCurrentPosition(const TVector<HlslToken>& listTokens)
    {
        int countTokens = (int)listTokens.size();
        if (countTokens == 0) return true;

        int insertionIndex = tokenBufferPos + 1;
        if (tokenBufferPos == tokenBuffer.size())
        {
            pushTokenBuffer(token);
        }

        tokenBuffer.insert(tokenBuffer.begin() + insertionIndex, listTokens.begin(), listTokens.begin() + countTokens);

        return true;
    }

	// Load 'token' with the next token in the stream of tokens.
	void HlslTokenStream::advanceToken()
	{
		//Save the current token if need, or increase the buffer position
		if (tokenBufferPos == tokenBuffer.size())
		{
			pushTokenBuffer(token);	
		}
		else
		{
			tokenBufferPos++;
		}

		//Get the next token
		if (tokenBufferPos == tokenBuffer.size())
		{
			scanner.tokenize(token);
		}
		else
		{
			token = tokenBuffer[tokenBufferPos];
		}
	}

	bool HlslTokenStream::recedeToken()
	{
		if (tokenBufferPos == 0) return false;

		if (tokenBufferPos == tokenBuffer.size())
		{
			//save current token at the end of buffer, so that we can push it back
			pushTokenBuffer(token);
			tokenBufferPos--;
		}

		token = popTokenBuffer();

		return true;
	}

	// Return the current token class.
	EHlslTokenClass HlslTokenStream::peek() const
	{
		return token.tokenClass;
	}

	// Return true, without advancing to the next token, if the current token is
	// the expected (passed in) token class.
	bool HlslTokenStream::peekTokenClass(EHlslTokenClass tokenClass) const
	{
		return peek() == tokenClass;
	}

	// Return true and advance to the next token if the current token is the
	// expected (passed in) token class.
	bool HlslTokenStream::acceptTokenClass(EHlslTokenClass tokenClass)
	{
		if (peekTokenClass(tokenClass)) {
			advanceToken();
			return true;
		}

		return false;
	}

    bool HlslTokenStream::acceptIdentifierTokenClass(TString& identiferName)
    {
        if (peek() != EHTokIdentifier) return false;
        if (token.string == nullptr) return false;

        identiferName = *token.string;
        return acceptTokenClass(EHTokIdentifier);
    }

} // end namespace glslang
