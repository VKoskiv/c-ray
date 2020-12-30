//
//  test_textbuffer.h
//  C-ray
//
//  Created by Valtteri on 24.6.2020.
//  Copyright © 2020 Valtteri Koskivuori. All rights reserved.
//

#include <string.h>
#include "../src/utils/textbuffer.h"
#include "../src/utils/string.h"

#define MULTILINE "This is a\nMultiline\nstring!\n"

bool textbuffer_new(void) {
	char *string = MULTILINE;
	
	test_assert(!newTextBuffer(NULL));
	
	textBuffer *empty = newTextBuffer("");
	test_assert(empty->amountOf.lines == 0);
	test_assert(empty->buflen == 0);
	freeTextBuffer(empty);
	
	textBuffer *original = newTextBuffer(string);
	test_assert(original->amountOf.lines == 3);
	test_assert(original->buflen == strlen(string));
	return true;
}

bool textbuffer_gotoline(void) {
	char *string = MULTILINE;
	textBuffer *original = newTextBuffer(string);
	
	test_assert(stringEquals(goToLine(original, 0), "This is a"));
	
	test_assert(original->current.line == 0);
	
	test_assert(stringEquals(goToLine(original, 1), "Multiline"));
	
	test_assert(original->current.line == 1);
	
	test_assert(stringEquals(goToLine(original, 2), "string!"));
	
	test_assert(original->current.line == 2);
	
	test_assert(!goToLine(original, 3));
	
	test_assert(original->current.line == 2);
	
	test_assert(!goToLine(original, -1));
	
	test_assert(original->current.line == 2);
	return true;
}

bool textbuffer_peekline(void) {
	char *string = MULTILINE;
	textBuffer *original = newTextBuffer(string);
	
	test_assert(stringEquals(peekLine(original, 0), "This is a"));
	
	test_assert(original->current.line == 0);
	
	test_assert(stringEquals(peekLine(original, 1), "Multiline"));
	
	test_assert(original->current.line == 0);
	
	test_assert(stringEquals(peekLine(original, 2), "string!"));
	
	test_assert(original->current.line == 0);
	
	test_assert(!peekLine(original, 3));
	
	test_assert(!peekLine(original, -1));
	
	return true;
}

bool textbuffer_nextline(void) {
	char *string = MULTILINE;
	textBuffer *original = newTextBuffer(string);
	
	test_assert(stringEquals(nextLine(original), "Multiline"));
	
	test_assert(stringEquals(nextLine(original), "string!"));
	
	test_assert(!nextLine(original));
	
	return true;
}

bool textbuffer_previousline(void) {
	char *string = MULTILINE;
	textBuffer *original = newTextBuffer(string);
	
	lastLine(original);
	
	test_assert(stringEquals(previousLine(original), "Multiline"));
	
	test_assert(stringEquals(previousLine(original), "This is a"));
	
	test_assert(stringEquals(nextLine(original), "Multiline"));
	
	test_assert(stringEquals(previousLine(original), "This is a"));
	
	test_assert(!previousLine(original));
	
	return true;
}

bool textbuffer_peeknextline(void) {
	char *string = MULTILINE;
	textBuffer *original = newTextBuffer(string);
	
	test_assert(stringEquals(peekNextLine(original), "Multiline"));
	
	nextLine(original);
	
	test_assert(stringEquals(peekNextLine(original), "string!"));
	
	return true;
}

bool textbuffer_firstline(void) {
	char *string = MULTILINE;
	textBuffer *original = newTextBuffer(string);
	
	lastLine(original);
	
	test_assert(stringEquals(firstLine(original), "This is a"));
	
	return true;
}

bool textbuffer_currentline(void) {
	char *string = MULTILINE;
	textBuffer *original = newTextBuffer(string);
	
	test_assert(stringEquals(currentLine(original), "This is a"));
	
	test_assert(stringEquals(nextLine(original), "Multiline"));
	
	test_assert(stringEquals(currentLine(original), "Multiline"));
	
	test_assert(stringEquals(nextLine(original), "string!"));
	
	test_assert(stringEquals(currentLine(original), "string!"));
	
	test_assert(stringEquals(firstLine(original), "This is a"));
	
	test_assert(stringEquals(currentLine(original), "This is a"));
	
	return true;
}

bool textbuffer_lastline(void) {
	char *string = MULTILINE;
	
	textBuffer *original = newTextBuffer(string);
	test_assert(stringEquals(lastLine(original), "string!"));
	
	return true;
}

bool textbuffer_textview(void) {
	char *string = MULTILINE;
	
	textBuffer *original = newTextBuffer(string);
	
	test_assert(original->amountOf.lines == 3);
	
	textBuffer *view = newTextView(original, 0, 1);
	test_assert(stringEquals(currentLine(view), "This is a"));
	test_assert(view->amountOf.lines == 1);
	freeTextBuffer(view);
	
	view = newTextView(original, 1, 1);
	test_assert(stringEquals(currentLine(view), "Multiline"));
	test_assert(view->amountOf.lines == 1);
	freeTextBuffer(view);
	
	view = newTextView(original, 2, 1);
	test_assert(stringEquals(currentLine(view), "string!"));
	test_assert(view->amountOf.lines == 1);
	freeTextBuffer(view);
	
	freeTextBuffer(original);
	
	return true;
}

bool textbuffer_tokenizer(void) {
	char *rawText = MULTILINE;
	textBuffer *file = newTextBuffer(rawText);
	
	lineBuffer *line = newLineBuffer();
	
	fillLineBuffer(line, firstLine(file), ' ');
	char *currentToken = firstToken(line);
	test_assert(stringEquals(currentToken, "This"));
	currentToken = nextToken(line);
	test_assert(stringEquals(currentToken, "is"));
	currentToken = nextToken(line);
	test_assert(stringEquals(currentToken, "a"));
	currentToken = firstToken(line);
	test_assert(stringEquals(currentToken, "This"));
	currentToken = lastToken(line);
	test_assert(stringEquals(currentToken, "a"));
	
	currentToken = nextToken(line);
	test_assert(!currentToken);
	
	fillLineBuffer(line, nextLine(file), ' ');
	currentToken = firstToken(line);
	test_assert(stringEquals(currentToken, "Multiline"));
	
	fillLineBuffer(line, nextLine(file), ' ');
	currentToken = lastToken(line);
	test_assert(stringEquals(currentToken, "string!"));
	
	currentToken = firstToken(line);
	test_assert(stringEquals(currentToken, "string!"));
	
	currentToken = nextToken(line);
	test_assert(!currentToken);
	
	destroyLineBuffer(line);
	freeTextBuffer(file);
	
	return true;
}
