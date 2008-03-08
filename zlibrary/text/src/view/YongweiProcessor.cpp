/*
 * Copyright (C) 2004-2008 Geometer Plus <contact@geometerplus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <algorithm>

#include <linebreak.h>

#include <ZLUnicodeUtil.h>

#include <ZLTextParagraph.h>

#include "ZLTextParagraphCursor.h"
#include "ZLTextWord.h"

bool ZLTextParagraphCursor::YongweiProcessor::ourIndexIsInitialised = false;

ZLTextParagraphCursor::YongweiProcessor::YongweiProcessor(const std::string &language, const ZLTextParagraph &paragraph, const std::vector<ZLTextMark> &marks, int paragraphNumber, ZLTextElementVector &elements) : Processor(paragraph, marks, paragraphNumber, elements), myLanguage(language), myBreaksTable(0) {
	if (!ourIndexIsInitialised) {
		init_linebreak_prop_index();
		ourIndexIsInitialised = true;
	}
}

ZLTextParagraphCursor::YongweiProcessor::~YongweiProcessor() {
	if (myBreaksTable != 0) {
		delete[] myBreaksTable;
	}
}

void ZLTextParagraphCursor::YongweiProcessor::processTextEntry(const ZLTextEntry &textEntry) {
	const size_t dataLength = textEntry.dataLength();
	if (dataLength != 0) {
		if ((myBreaksTable != 0) && (myBreaksTableLength < dataLength)) {
			delete[] myBreaksTable;
			myBreaksTable = 0;
		}
		if (myBreaksTable == 0) {
			myBreaksTableLength = dataLength;
			myBreaksTable = new char[dataLength];
		}
		const char *start = textEntry.data();
		const char *end = start + dataLength;
		set_linebreaks_utf8((const utf8_t*)start, dataLength, myLanguage.c_str(), myBreaksTable);
		ZLUnicodeUtil::Ucs2Char ch;
		enum { NO_SPACE, SPACE, NON_BREAKABLE_SPACE } spaceState = NO_SPACE;
		int charLength = 0;
		int index = 0;
		const char *wordStart = start;
		for (const char *ptr = start; ptr < end; ptr += charLength, index += charLength) {
			charLength = ZLUnicodeUtil::firstChar(ch, ptr);
			if (ZLUnicodeUtil::isSpace(ch)) {
				if ((spaceState == NO_SPACE) && (ptr != wordStart)) {
					addWord(wordStart, myOffset + (wordStart - textEntry.data()), ptr - wordStart);
				}
				spaceState = SPACE;
			} else if (ZLUnicodeUtil::isNBSpace(ch)) {
				if (spaceState == NO_SPACE) {
					if (ptr != wordStart) {
						addWord(wordStart, myOffset + (wordStart - textEntry.data()), ptr - wordStart);
					}
					spaceState = NON_BREAKABLE_SPACE;
				}
			} else {
				switch (spaceState) {
					case SPACE:
						myElements.push_back(ZLTextElementPool::Pool.HSpaceElement);
						wordStart = ptr;
						break;
					case NON_BREAKABLE_SPACE:
						myElements.push_back(ZLTextElementPool::Pool.NBHSpaceElement);
						wordStart = ptr;
						break;
					case NO_SPACE:
						if ((index > 0) && (myBreaksTable[index - 1] != LINEBREAK_NOBREAK) && (ptr != wordStart)) {
							addWord(wordStart, myOffset + (wordStart - textEntry.data()), ptr - wordStart);
							wordStart = ptr;
						}
						break;
				}
				spaceState = NO_SPACE;
			}
		}
		switch (spaceState) {
			case SPACE:
				myElements.push_back(ZLTextElementPool::Pool.HSpaceElement);
				break;
			case NON_BREAKABLE_SPACE:
				myElements.push_back(ZLTextElementPool::Pool.NBHSpaceElement);
				break;
			case NO_SPACE:
				addWord(wordStart, myOffset + (wordStart - textEntry.data()), end - wordStart);
				break;
		}
		myOffset += dataLength;
	}
}
