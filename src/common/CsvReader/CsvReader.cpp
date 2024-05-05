/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "CsvReader.h"
#include <assert.h>
#include <math.h>

#define DefaultHeaderName "Column"

CCsvReader::CCsvReader(FILE* handle, bool hasHeaders)
	: _initialized(false), _buffer(nullptr),_bufferSize(DefaultBufferSize), _bufferLength(0), _eof(false), _eol(false)
	, _nextFieldStart(0), _nextFieldIndex(0), _supportsMultiline(true), _skipEmptyLines(true)
	, _currentRecordIndex(-1), _handle(handle), _hasHeaders(hasHeaders), _fieldCount(0)
	, _firstRecordInCache(false), _missingFieldFlag(false), _missingFieldAction(ReplaceByEmpty), _parseErrorFlag(false), _defaultParseErrorAction(ThrowException)
	, _trimmingOptions(ValueTrimmingOptions::None)
{
}

CCsvReader::~CCsvReader()
{
	delete [] _buffer;
}

size_t CCsvReader::GetFieldCount()
{
	EnsureInitialize();
	return _fieldCount;
}

const vector<string>& CCsvReader::GetFieldHeaders()
{
	EnsureInitialize();
	assert(_fieldHeaders.size() != 0);	// Field headers must be non null.

	return _fieldHeaders;
}

string CCsvReader::operator[](size_t field)
{
	return ReadField(field, false, false);
}

void CCsvReader::EnsureInitialize()
{
	if (!_initialized)
		ReadNextRecord(true, false);

	assert(_fieldHeaders.size() > 0 || (_fieldHeaders.size() == 0 && _fieldHeaderIndexes.size() == 0));
}

bool CCsvReader::ReadNextRecord()
{
	return ReadNextRecord(false, false);
}


inline std::string trim(const std::string& orig)
{
	string str(orig);
	str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
	str.erase(str.find_last_not_of(' ')+1);         //surfixing spaces
	return str;
}


bool CCsvReader::ReadNextRecord(bool onlyReadHeaders, bool skipToNextLine)
{
	if (_eof)
	{
		if (_firstRecordInCache)
		{
			_firstRecordInCache = false;
			_currentRecordIndex++;

			return true;
		}
		else
			return false;
	}

	if (!_initialized)
	{
		_buffer = new char[_bufferSize];

		// will be replaced if and when headers are read
		_fieldHeaders.clear();

		if (!ReadBuffer())
			return false;

		if (!SkipEmptyAndCommentedLines(_nextFieldStart))
			return false;

		// Keep growing _fields array until the last field has been found
		// and then resize it to its final correct size

		_fieldCount = 0;
		_fields.clear();

		while (!ReadField(_fieldCount, true, false).empty())
		{
			if (_parseErrorFlag)
			{
				_fieldCount = 0;
				_fields.clear();
				_parseErrorFlag = false;
				_nextFieldIndex = 0;
			}
			else
			{
				_fieldCount++;

				if (_fieldCount == _fields.size())
					_fields.resize((_fieldCount + 1) * 2);
			}
		}

		// _fieldCount contains the last field index, but it must contains the field count,
		// so increment by 1
		_fieldCount++;

		if (_fields.size() != _fieldCount)
			_fields.resize(_fieldCount);

		_initialized = true;

		// If headers are present, call ReadNextRecord again
		if (_hasHeaders)
		{
			// Don't count first record as it was the headers
			_currentRecordIndex = -1;

			_firstRecordInCache = false;

			_fieldHeaders.resize(_fields.size());
			_fieldHeaderIndexes.clear();

			for (int i = 0; i < _fields.size(); i++)
			{
				string headerName = _fields[i];
				if (headerName.empty() || trim(headerName).length() == 0)
				{
					char buf[32];
					headerName = DefaultHeaderName;
					headerName.append(_itoa(i, buf, 10));
				}
				_fieldHeaders[i] = headerName;
				_fieldHeaderIndexes.insert({ headerName, i });
			}

			// Proceed to first record
			if (!onlyReadHeaders)
			{
				// Calling again ReadNextRecord() seems to be simpler, 
				// but in fact would probably cause many subtle bugs because a derived class does not expect a recursive behavior
				// so simply do what is needed here and no more.

				if (!SkipEmptyAndCommentedLines(_nextFieldStart))
					return false;

				for_each (_fields.begin(), _fields.end(), [=](string& field) -> void { field.clear(); });
				_nextFieldIndex = 0;
				_eol = false;

				_currentRecordIndex++;
				return true;
			}
		}
		else
		{
			if (onlyReadHeaders)
			{
				_firstRecordInCache = true;
				_currentRecordIndex = -1;
			}
			else
			{
				_firstRecordInCache = false;
				_currentRecordIndex = 0;
			}
		}
	}
	else
	{
		if (skipToNextLine)
			SkipToNextLine(_nextFieldStart);
		else if (_currentRecordIndex > -1 && !_missingFieldFlag)
		{
			// If not already at end of record, move there
			if (!_eol && !_eof)
			{
				if (!_supportsMultiline)
					SkipToNextLine(_nextFieldStart);
				else
				{
					// a dirty trick to handle the case where extra fields are present
					while (!ReadField(_nextFieldIndex, true, true).empty())
					{
					}
				}
			}
		}

		if (!_firstRecordInCache && !SkipEmptyAndCommentedLines(_nextFieldStart))
			return false;

		if (_hasHeaders || !_firstRecordInCache)
			_eol = false;

		// Check to see if the first record is in cache.
		// This can happen when initializing a reader with no headers
		// because one record must be read to get the field count automatically
		if (_firstRecordInCache)
			_firstRecordInCache = false;
		else
		{
			for_each (_fields.begin(), _fields.end(), [=](string& field) -> void { field.clear(); });
			_nextFieldIndex = 0;
		}

		_missingFieldFlag = false;
		_parseErrorFlag = false;
		_currentRecordIndex++;
	}

	return true;
}


// Moves to the specified record index.
bool CCsvReader::MoveTo(size_t record)
{
	if (record < _currentRecordIndex)
		return false;

	// Get number of record to read
	long offset = record - _currentRecordIndex;

	while (offset > 0)
	{
		if (!ReadNextRecord())
			return false;

		offset--;
	}

	return true;
}

// Parses a new line delimiter.
bool CCsvReader::ParseNewLine(size_t& pos)
{
	assert(pos <= _bufferLength);

	// Check if already at the end of the buffer
	if (pos == _bufferLength)
	{
		pos = 0;

		if (!ReadBuffer())
			return false;
	}

	char c = _buffer[pos];

	// Treat \r as new line only if it's not the delimiter

	if (c == '\r')
	{
		pos++;

		// Skip following \n (if there is one)

		if (pos < _bufferLength)
		{
			if (_buffer[pos] == '\n')
				pos++;
		}
		else
		{
			if (ReadBuffer())
			{
				if (_buffer[0] == '\n')
					pos = 1;
				else
					pos = 0;
			}
		}

		if (pos >= _bufferLength)
		{
			ReadBuffer();
			pos = 0;
		}

		return true;
	}
	else if (c == '\n')
	{
		pos++;

		if (pos >= _bufferLength)
		{
			ReadBuffer();
			pos = 0;
		}

		return true;
	}

	return false;
}

// Determines whether the character at the specified position is a new line delimiter.
bool CCsvReader::IsNewLine(size_t pos)
{
	assert(pos < _bufferLength);

	char c = _buffer[pos];
	return (c == '\n') || (c == '\r');
}

#if _DEBUG
int g_i;
#endif

// Fills the buffer with data from the reader.
bool CCsvReader::ReadBuffer()
{
	if (_eof)
		return false;

	_bufferLength = fread(_buffer, 1, _bufferSize, _handle);
	
//#if _DEBUG
//	ATLTRACE2(atlTraceDBProvider, 0, L"CCsvReader::ReadBuffer() read %d bytes, g_i=%d\n", _bufferLength, g_i++);
//	if (g_i == 2117)
//	{
//		const char* p = strstr(_buffer, "2016/03/");
//		_bufferLength = p - _buffer;
//		_eof = true;
//	}
//#endif

	if (_bufferLength > 0)
		return true;
	else
	{
		_eof = true;
		delete [] _buffer;
		_buffer = nullptr;

		return false;
	}
}

string CCsvReader::ReadField(size_t field, bool initializing, bool discardValue)
{
	if (!initializing)
	{
		if (field < 0 || field >= _fieldCount)
			throw exception("FieldIndexOutOfRange");

		if (_currentRecordIndex < 0)
			throw exception("NoCurrentRecord");

		if (!_fields[field].empty())
			return _fields[field];
		else if (_missingFieldFlag)
			return HandleMissingField("", field, _nextFieldStart);
	}

	size_t index = _nextFieldIndex;

	while (index < field + 1)
	{
		// Handle case where stated start of field is past buffer
		// This can occur because _nextFieldStart is simply 1 + last char position of previous field
		if (_nextFieldStart == _bufferLength)
		{
			_nextFieldStart = 0;

			// Possible EOF will be handled later (see Handle_EOF1)
			ReadBuffer();
		}

		string value;

		if (_missingFieldFlag)
		{
			value = HandleMissingField(value, index, _nextFieldStart);
		}
		else if (_nextFieldStart == _bufferLength)
		{
			// Handle_EOF1: Handle EOF here

			// If current field is the requested field, then the value of the field is "" as in "f1,f2,f3,(\s*)"
			// otherwise, the CSV is malformed

			if (index == field)
			{
				if (!discardValue)
				{
					value.clear();
					if (_fields.size() < (index + 1))
						_fields.resize(index + 1);
					_fields[index] = value;
				}

				_missingFieldFlag = true;
			}
			else
			{
				value = HandleMissingField(value, index, _nextFieldStart);
			}
		}
		else
		{
			// Trim spaces at start
			if ((_trimmingOptions & ValueTrimmingOptions::UnquotedOnly) != 0)
				SkipWhiteSpaces(_nextFieldStart);

			if (_eof)
			{
				value.clear();
				if (_fields.size() < (field + 1))
					_fields.resize(field + 1);
				_fields[field] = value;

				if (field < _fieldCount)
					_missingFieldFlag = true;
			}
			else if (_buffer[_nextFieldStart] != DefaultQuote)
			{
				// Non-quoted field
				int start = _nextFieldStart;
				int pos = _nextFieldStart;

				for (; ; )
				{
					while (pos < _bufferLength)
					{
						char c = _buffer[pos];

						if (c == DefaultDelimiter)
						{
							_nextFieldStart = pos + 1;

							break;
						}
						else if (c == '\r' || c == '\n')
						{
							_nextFieldStart = pos;
							_eol = true;

							break;
						}
						else
							pos++;
					}

					if (pos < _bufferLength)
						break;
					else
					{
						if (!discardValue)
							value.append(_buffer, start, pos - start);

						start = 0;
						pos = 0;
						_nextFieldStart = 0;

						if (!ReadBuffer())
							break;
					}
				}

				if (!discardValue)
				{
					if ((_trimmingOptions & ValueTrimmingOptions::UnquotedOnly) == 0)
					{
						if (!_eof && pos > start)
							value.append(_buffer, start, pos - start);
					}
					else
					{
						if (!_eof && pos > start)
						{
							// Do the trimming
							pos--;
							while (pos > -1 && IsWhiteSpace(_buffer[pos]))
								pos--;
							pos++;

							if (pos > 0)
								value.append(_buffer, start, pos - start);
						}
						else
							pos = -1;

						// If pos <= 0, that means the trimming went past buffer start,
						// and the concatenated value needs to be trimmed too.
						if (pos <= 0)
						{
							pos = (value.length() - 1);

							// Do the trimming
							while (pos > -1 && IsWhiteSpace(value[pos]))
								pos--;

							pos++;

							if (pos > 0 && pos != value.length())
								value = value.substr(0, pos);
						}
					}
				}

				if (_eol || _eof)
				{
					_eol = ParseNewLine(_nextFieldStart);

					// Reaching a new line is ok as long as the parser is initializing or it is the last field
					if (!initializing && index != _fieldCount - 1)
					{
						value = HandleMissingField(value, index, _nextFieldStart);
					}
				}

				if (!discardValue)
				{
					if (_fields.size() < (index + 1))
						_fields.resize(index + 1);
					_fields[index] = value;
				}
			}
			else
			{
				// Quoted field

				// Skip quote
				size_t start = _nextFieldStart + 1;
				size_t pos = start;

				bool quoted = true;
				bool escaped = false;

				if ((_trimmingOptions & ValueTrimmingOptions::QuotedOnly) != 0)
				{
					SkipWhiteSpaces(start);
					pos = start;
				}

				for (;;)
				{
					while (pos < _bufferLength)
					{
						char c = _buffer[pos];

						if (escaped)
						{
							escaped = false;
							start = pos;
						}
						// IF current char is escape AND (escape and quote are different OR next char is a quote)
						else 
						{
							char nextc = getc(_handle);
							ungetc(nextc, _handle);

							if (c == DefaultEscape && (DefaultEscape != DefaultQuote || (pos + 1 < _bufferLength && _buffer[pos + 1] == DefaultQuote) || (pos + 1 == _bufferLength && nextc == DefaultQuote)))
							{
								if (!discardValue)
									value.append(_buffer, start, pos - start);

								escaped = true;
							}
							else if (c == DefaultQuote)
							{
								quoted = false;
								break;
							}
						}
						pos++;
					}

					if (!quoted)
						break;
					else
					{
						if (!discardValue && !escaped)
							value.append(_buffer, start, pos - start);

						start = 0;
						pos = 0;
						_nextFieldStart = 0;

						if (!ReadBuffer())
						{
							HandleParseError(exception("MalformedCsv"), _nextFieldStart);
							return "";
						}
					}
				}

				if (!_eof)
				{
					// Append remaining parsed buffer content
					if (!discardValue && pos > start)
						value.append(_buffer, start, pos - start);

					if (!discardValue && value.empty() && (_trimmingOptions & ValueTrimmingOptions::QuotedOnly) != 0)
					{
						int newLength = value.length();
						while (newLength > 0 && IsWhiteSpace(value[newLength - 1]))
							newLength--;

						if (newLength < value.length())
							value = value.substr(0, newLength);
					}

					// Skip quote
					_nextFieldStart = pos + 1;

					// Skip whitespaces between the quote and the delimiter/eol
					SkipWhiteSpaces(_nextFieldStart);

					// Skip delimiter
					bool delimiterSkipped;
					if (_nextFieldStart < _bufferLength && _buffer[_nextFieldStart] == DefaultDelimiter)
					{
						_nextFieldStart++;
						delimiterSkipped = true;
					}
					else
					{
						delimiterSkipped = false;
					}

					// Skip new line delimiter if initializing or last field
					// (if the next field is missing, it will be caught when parsed)
					if (!_eof && !delimiterSkipped && (initializing || index == _fieldCount - 1))
						_eol = ParseNewLine(_nextFieldStart);

					// If no delimiter is present after the quoted field and it is not the last field, then it is a parsing error
					if (!delimiterSkipped && !_eof && !(_eol || IsNewLine(_nextFieldStart)))
						HandleParseError(exception("MalformedCsv"), _nextFieldStart);
				}

				if (!discardValue)
				{
					if (_fields.size() < (index + 1))
						_fields.resize(index + 1);
					_fields[index] = value;
				}
			}
		}

		_nextFieldIndex = max(index + 1, _nextFieldIndex);

		if (index == field)
		{
			// If initializing, return null to signify the last field has been reached

			if (initializing)
			{
				if (_eol || _eof)
					return "";
				else
					return value;
			}
			else
				return value;
		}

		index++;
	}

	// Getting here is bad ...
	HandleParseError(exception("MalformedCsv"), _nextFieldStart);
	return "";
}

/// Skips empty and commented lines.
/// If the end of the buffer is reached, its content be discarded and filled again from the reader.
bool CCsvReader::SkipEmptyAndCommentedLines(size_t& pos)
{
	if (pos < _bufferLength)
		DoSkipEmptyAndCommentedLines(pos);

	while (pos >= _bufferLength && !_eof)
	{
		if (ReadBuffer())
		{
			pos = 0;
			DoSkipEmptyAndCommentedLines(pos);
		}
		else
			return false;
	}

	return !_eof;
}

void CCsvReader::DoSkipEmptyAndCommentedLines(size_t& pos)
{
	while (pos < _bufferLength)
	{
		if (_buffer[pos] == DefaultComment)
		{
			pos++;
			SkipToNextLine(pos);
		}
		else if (_skipEmptyLines && ParseNewLine(pos))
			continue;
		else
			break;
	}
}

/// Skips whitespace characters.
bool CCsvReader::SkipWhiteSpaces(size_t& pos)
{
	for (;;)
	{
		while (pos < _bufferLength && IsWhiteSpace(_buffer[pos]))
			pos++;

		if (pos < _bufferLength)
			break;
		else
		{
			pos = 0;

			if (!ReadBuffer())
				return false;
		}
	}

	return true;
}

/// Skips ahead to the next NewLine character.
/// If the end of the buffer is reached, its content be discarded and filled again from the reader.
bool CCsvReader::SkipToNextLine(size_t& pos)
{
	// ((pos = 0) == 0) is a little trick to reset position inline
	while ((pos < _bufferLength || (ReadBuffer() && ((pos = 0) == 0))) && !ParseNewLine(pos))
		pos++;

	return !_eof;
}

// Handles a parsing error.
void CCsvReader::HandleParseError(exception error, size_t& pos)
{
	_parseErrorFlag = true;

	switch (_defaultParseErrorAction)
	{
		case ThrowException:
			throw error;

		case AdvanceToNextLine:
			// already at EOL when fields are missing, so don't skip to next line in that case
			if (!_missingFieldFlag && pos >= 0)
				SkipToNextLine(pos);
			break;

		default:
			throw exception("NotSupported");
	}
}

// Handles a missing field error.
string CCsvReader::HandleMissingField(const string& value, size_t fieldIndex, size_t& currentPosition)
{
	if (fieldIndex < 0 || fieldIndex >= _fieldCount)
		throw exception("ArgumentOutOfRange");

	_missingFieldFlag = true;

	for (int i = fieldIndex + 1; i < _fieldCount; i++)
		_fields[i] = "";

	if (!value.empty())
		return value;

	switch (_missingFieldAction)
	{
		case ParseError:
			HandleParseError(exception("MissingFieldCsv"), currentPosition);
			return value;

		case ReplaceByEmpty:
			return "";

		default:
			throw exception("NotSupported");
	}
}
