/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <exception>
using std::map;
using std::string;
using std::vector;
using std::exception;

// Fast CSV Reader
class CCsvReader
{
public:
	enum ParseErrorAction
	{ ThrowException, AdvanceToNextLine };

	enum MissingFieldAction
	{ ParseError, ReplaceByEmpty };

	enum ValueTrimmingOptions
	{
		None = 0,
		UnquotedOnly = 1,
		QuotedOnly = 2,
		All = UnquotedOnly | QuotedOnly
	};

protected:
	enum
	{
		DefaultBufferSize = 0x8000,
		DefaultDelimiter = ',',
		DefaultQuote = '"',
		DefaultEscape = '"',
		DefaultComment = '#',
	};

	bool _initialized;

	size_t _bufferSize;

	char* _buffer;	// Contains the read buffer.

	size_t _bufferLength;	// Contains the current read buffer length.

	bool _eof;	// Indicates if the end of the file has been reached.

	bool _eol;	// Indicates if the last read operation reached an EOL character.

	bool _firstRecordInCache;	// Indicates if the first record is in cache.
								// This can happen when initializing a reader with no headers
								// because one record must be read to get the field count automatically

	bool _missingFieldFlag;		// Indicates if one or more field are missing for the current record.
								// Resets after each successful record read.

	bool _parseErrorFlag;		// Indicates if a parse error occured for the current record.
								// Resets after each successful record read.
	
	int _currentRecordIndex;

	size_t _nextFieldStart;	// Contains the starting position of the next unread field.

	size_t _nextFieldIndex;	// Contains the index of the next unread field.

	FILE* _handle;
	
	bool _hasHeaders;	// Indicates if field names are located on the first non commented line.

	bool _supportsMultiline;	// Indicates if the reader supports multiline.

	bool _skipEmptyLines;	// Indicates if the reader will skip empty lines.

	size_t _fieldCount;	// Contains the maximum number of fields to retrieve for each record.

	vector<string> _fieldHeaders;	// Contains the field headers.

	map<string, int> _fieldHeaderIndexes;	// Dictionary of field indexes by header. The key is the field name and the value is its index.

	vector<string> _fields;	// Contains the array of the field values for the current record.

	// Ensures that the reader is initialized.
	void EnsureInitialize();

	// Reads the next record.
	bool ReadNextRecord(bool onlyReadHeaders, bool skipToNextLine);

	// Indicates whether the specified Unicode character is categorized as white space.
	inline bool IsWhiteSpace(char c) { return (c == ' ' || c == '\t'); }

	// Parses a new line delimiter.
	bool ParseNewLine(size_t& pos);

	// Determines whether the character at the specified position is a new line delimiter.
	bool IsNewLine(size_t pos);

	// Fills the buffer with data from the reader.
	bool ReadBuffer();

	// Reads the field at the specified index.
	// Any unread fields with an inferior index will also be read as part of the required parsing.
	string ReadField(size_t field, bool initializing, bool discardValue);

	// Skips empty and commented lines.
	// If the end of the buffer is reached, its content be discarded and filled again from the reader.
	bool SkipEmptyAndCommentedLines(size_t& pos);

	void DoSkipEmptyAndCommentedLines(size_t& pos);

	// Skips whitespace characters.
	bool SkipWhiteSpaces(size_t& pos);

	// Skips ahead to the next NewLine character.
	// If the end of the buffer is reached, its content be discarded and filled again from the reader.
	bool SkipToNextLine(size_t& pos);

	// Handles a parsing error.
	void HandleParseError(exception error, size_t& pos);

	// Handles a missing field error.
	string HandleMissingField(const string& value, size_t fieldIndex, size_t& currentPosition);

	ParseErrorAction _defaultParseErrorAction;

	MissingFieldAction _missingFieldAction;

	ValueTrimmingOptions _trimmingOptions;		// Determines which values should be trimmed.

public:
	CCsvReader(FILE* handle, bool hasHeaders);

	~CCsvReader();

	size_t GetFieldCount();

	// Gets the field headers.
	const vector<string>& GetFieldHeaders();

	// Gets the field at the specified index.
	string operator[](size_t);

	// Reads the next record.
	bool ReadNextRecord();

	// Moves to the specified record index.
	bool MoveTo(size_t record);
};
