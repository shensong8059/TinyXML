/*
www.sourceforge.net/projects/tinyxml
Original code by Lee Thomason (www.grinninglizard.com)

This software is provided 'as-is', without any express or implied 
warranty. In no event will the authors be held liable for any 
damages arising from the use of this software.

Permission is granted to anyone to use this software for any 
purpose, including commercial applications, and to alter it and 
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must 
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and 
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source 
distribution.
*/

#include "tinyxml.h"

#include <regex>
#include <functional>

//#include <ctype.h>
//#include <stddef.h>


//#define DEBUG_PARSER
#if defined( DEBUG_PARSER )
#	if defined( DEBUG ) && defined( _MSC_VER )
#		include <windows.h>
#		define TIXML_LOG OutputDebugString
#	else
#		define TIXML_LOG printf
#	endif
#endif

using namespace std;

// Note tha "PutString" hardcodes the same list. This
// is less flexible than it appears. Changing the entries
// or order will break putstring.	
TiXmlBase::Entity TiXmlBase::entity[ TiXmlBase::NUM_ENTITY ] = 
{
	{ "&amp;",  5, '&' },
	{ "&lt;",   4, '<' },
	{ "&gt;",   4, '>' },
	{ "&quot;", 6, '\"' },
	{ "&apos;", 6, '\'' }
};

// Bunch of unicode info at:
//		http://www.unicode.org/faq/utf_bom.html
// Including the basic of this table, which determines the #bytes in the
// sequence from the lead byte. 1 placed for invalid sequences --
// although the result will be junk, pass it through as much as possible.
// Beware of the non-characters in UTF-8:	
//				ef bb bf (Microsoft "lead bytes")
//				ef bf be
//				ef bf bf 

constexpr auto TIXML_UTF_LEAD_0 = 0xef;
constexpr auto TIXML_UTF_LEAD_1 = 0xbb;
constexpr auto TIXML_UTF_LEAD_2 = 0xbf;

const int TiXmlBase::utf8ByteTable[256] = 
{
	//	0	1	2	3	4	5	6	7	8	9	a	b	c	d	e	f
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x00
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x10
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x20
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x30
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x40
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x50
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x60
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x70	End of ASCII range
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x80 0x80 to 0xc1 invalid
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0x90 
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0xa0 
		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	// 0xb0 
		1,	1,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	// 0xc0 0xc2 to 0xdf 2 byte
		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	// 0xd0
		3,	3,	3,	3,	3,	3,	3,	3,	3,	3,	3,	3,	3,	3,	3,	3,	// 0xe0 0xe0 to 0xef 3 byte
		4,	4,	4,	4,	4,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1	// 0xf0 0xf0 to 0xf4 4 byte, 0xf5 and higher invalid
};


void TiXmlBase::ConvertUTF32ToUTF8( unsigned long input, char* output, int* length )
{
	const unsigned long BYTE_MASK = 0xBF;
	const unsigned long BYTE_MARK = 0x80;
	const unsigned long FIRST_BYTE_MARK[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

	if (input < 0x80) 
		*length = 1;
	else if ( input < 0x800 )
		*length = 2;
	else if ( input < 0x10000 )
		*length = 3;
	else if ( input < 0x200000 )
		*length = 4;
	else
		{ *length = 0; return; }	// This code won't covert this correctly anyway.

	output += *length;

	// Scary scary fall throughs.
	switch (*length) 
	{
		case 4:
			--output; 
			*output = (char)((input | BYTE_MARK) & BYTE_MASK); 
			input >>= 6;
		case 3:
			--output; 
			*output = (char)((input | BYTE_MARK) & BYTE_MASK); 
			input >>= 6;
		case 2:
			--output; 
			*output = (char)((input | BYTE_MARK) & BYTE_MASK); 
			input >>= 6;
		case 1:
			--output; 
			*output = (char)(input | FIRST_BYTE_MARK[*length]);
	}
}


/*static*/ int TiXmlBase::IsAlpha( unsigned char anyByte, TiXmlEncoding /*encoding*/ )
{
	// This will only work for low-ascii, everything else is assumed to be a valid
	// letter. I'm not sure this is the best approach, but it is quite tricky trying
	// to figure out alhabetical vs. not across encoding. So take a very 
	// conservative approach.

//	if ( encoding == TIXML_ENCODING_UTF8 )
//	{
		if ( anyByte < 127 )
			return isalpha( anyByte );
		else
			return 1;	// What else to do? The unicode set is huge...get the english ones right.
//	}
//	else
//	{
//		return isalpha( anyByte );
//	}
}


/*static*/ int TiXmlBase::IsAlphaNum( unsigned char anyByte, TiXmlEncoding /*encoding*/ )
{
	// This will only work for low-ascii, everything else is assumed to be a valid
	// letter. I'm not sure this is the best approach, but it is quite tricky trying
	// to figure out alhabetical vs. not across encoding. So take a very 
	// conservative approach.

//	if ( encoding == TIXML_ENCODING_UTF8 )
//	{
		if ( anyByte < 127 )
			return isalnum( anyByte );
		else
			return 1;	// What else to do? The unicode set is huge...get the english ones right.
//	}
//	else
//	{
//		return isalnum( anyByte );
//	}
}


class TiXmlParsingData
{
	friend class TiXmlDocument;
  public:
	void Stamp( string::const_iterator first, std::string::const_iterator last, TiXmlEncoding encoding );

	const TiXmlCursor& Cursor() const	{ return cursor; }

  private:
	// Only used by the document!
	TiXmlParsingData(std::string::const_iterator first, std::string::const_iterator last, int _tabsize, int row, int col )
	{
		assert( first!=last );
		stamp = first;
		tabsize = _tabsize;
		cursor.row = row;
		cursor.col = col;
	}

	TiXmlCursor		cursor;
	string::const_iterator		stamp;
	int				tabsize;
};


void TiXmlParsingData::Stamp( string::const_iterator first, std::string::const_iterator last, TiXmlEncoding encoding )
{
	assert( first!=last );

	// Do nothing if the tabsize is 0.
	if ( tabsize < 1 )
	{
		return;
	}

	// Get the current row, column.
	int row = cursor.row;
	int col = cursor.col;
	auto p = stamp;
	assert( p!=last );

	while ( p < first )
	{
		// Treat p as unsigned, so we have a happy compiler.
		auto pU = p;

		// Code contributed by Fletcher Dunn: (modified by lee)
		switch (*pU) {
			case 0:
				// We *should* never get here, but in case we do, don't
				// advance past the terminating null character, ever
				return;

			case '\r':
				// bump down to the next line
				++row;
				col = 0;				
				// Eat the character
				++p;

				// Check for \r\n sequence, and treat this as a single character
				if (*p == '\n') {
					++p;
				}
				break;

			case '\n':
				// bump down to the next line
				++row;
				col = 0;

				// Eat the character
				++p;

				// Check for \n\r sequence, and treat this as a single
				// character.  (Yes, this bizarre thing does occur still
				// on some arcane platforms...)
				if (*p == '\r') {
					++p;
				}
				break;

			case '\t':
				// Eat the character
				++p;

				// Skip to next tab stop
				col = (col / tabsize + 1) * tabsize;
				break;

			case TIXML_UTF_LEAD_0:
				if ( encoding == TIXML_ENCODING_UTF8 )
				{
					if ( *(p+1) && *(p+2) )
					{
						// In these cases, don't advance the column. These are
						// 0-width spaces.
						if ( *(pU+1)==TIXML_UTF_LEAD_1 && *(pU+2)==TIXML_UTF_LEAD_2 )
							p += 3;	
						else if ( *(pU+1)==0xbf && *(pU+2)==0xbe )
							p += 3;	
						else if ( *(pU+1)==0xbf && *(pU+2)==0xbf )
							p += 3;	
						else
							{ p +=3; ++col; }	// A normal character.
					}
				}
				else
				{
					++p;
					++col;
				}
				break;

			default:
				if ( encoding == TIXML_ENCODING_UTF8 )
				{
					// Eat the 1 to 4 byte utf8 character.
					int step = TiXmlBase::utf8ByteTable[*(p)];
					if ( step == 0 )
						step = 1;		// Error case from bad encoding, but handle gracefully.
					p += step;

					// Just advance one column, of course.
					++col;
				}
				else
				{
					++p;
					++col;
				}
				break;
		}
	}
	cursor.row = row;
	cursor.col = col;
	assert( cursor.row >= -1 );
	assert( cursor.col >= -1 );
	stamp = p;
	assert( stamp!=last );
}


std::string::const_iterator TiXmlBase::SkipWhiteSpace( std::string::const_iterator first, std::string::const_iterator last)
{
	smatch m;
	regex re("^[\\s]*");
	auto res = regex_search(first, last, m, re);
	if (!res)
		return last;
	first = m[0].second;
	//while (first!=last && IsWhiteSpace(*first))
	//	++first;
	return first;
}

#ifdef TIXML_USE_STL
/*static*/ bool TiXmlBase::StreamWhiteSpace( std::istream * in, TIXML_STRING * tag )
{
	for( ;; )
	{
		if ( !in->good() ) return false;

		int c = in->peek();
		// At this scope, we can't get to a document. So fail silently.
		if ( !IsWhiteSpace( c ) || c <= 0 )
			return true;

		*tag += (char) in->get();
	}
}

/*static*/ bool TiXmlBase::StreamTo( std::istream * in, int character, TIXML_STRING * tag )
{
	//assert( character > 0 && character < 128 );	// else it won't work in utf-8
	while ( in->good() )
	{
		int c = in->peek();
		if ( c == character )
			return true;
		if ( c <= 0 )		// Silent failure: can't get document at this scope
			return false;

		in->get();
		*tag += (char) c;
	}
	return false;
}
#endif

// One of TinyXML's more performance demanding functions. Try to keep the memory overhead down. The
// "assign" optimization removes over 10% of the execution time.
//
std::string::const_iterator TiXmlBase::ReadName( std::string::const_iterator first, std::string::const_iterator last, TIXML_STRING * name, TiXmlEncoding encoding )
{
	// Oddly, not supported on some compilers,
	//name->clear();
	// So use this:
	*name = "";
	assert( first!=last );

	// Names start with letters or underscores.
	// Of course, in unicode, tinyxml has no idea what a letter *is*. The
	// algorithm is generous.
	//
	// After that, they can be letters, underscores, numbers,
	// hyphens, or colons. (Colons are valid only for namespaces,
	// but tinyxml can't tell namespaces from names.)
	if (    first!=last
		 && ( IsAlpha( (unsigned char) *first, encoding ) || *first == '_' ) )
	{
		auto start = first;
		while(		first !=last
				&&	(		IsAlphaNum( (unsigned char ) *first, encoding ) 
						 || *first == '_'
						 || *first == '-'
						 || *first == '.'
						 || *first == ':' ) )
		{
			//(*name) += *first; // expensive
			++first;
		}
		if ( first-start > 0 ) {
			name->assign( start, first );
		}
		return first;
	}
	return last;
}

string::const_iterator TiXmlBase::GetEntity( std::string::const_iterator first, std::string::const_iterator last, char* value, int* length, TiXmlEncoding encoding )
{
	// Presume an entity, and pull it out.
    TIXML_STRING ent;
	int i;
	*length = 0;

	if ( *(first+1) && *(first+1) == '#' && *(first+2) )
	{
		unsigned long ucs = 0;
		ptrdiff_t delta = 0;
		unsigned mult = 1;

		if ( *(first+2) == 'x' )
		{
			// Hexadecimal.
			if ( !*(first+3) ) return last;

			auto q = first+3;
			q = find( q,last, ';' );

			if ( q==last ) return last;

			delta = q-first;
			--q;

			while ( *q != 'x' )
			{
				if ( *q >= '0' && *q <= '9' )
					ucs += mult * (*q - '0');
				else if ( *q >= 'a' && *q <= 'f' )
					ucs += mult * (*q - 'a' + 10);
				else if ( *q >= 'A' && *q <= 'F' )
					ucs += mult * (*q - 'A' + 10 );
				else 
					return last;
				mult *= 16;
				--q;
			}
		}
		else
		{
			// Decimal.
			if ( !*(first+2) ) return last;

			auto q = first+2;
			q = find(q, last, ';' );

			if (q == last) return last;

			delta = q-first;
			--q;

			while ( *q != '#' )
			{
				if ( *q >= '0' && *q <= '9' )
					ucs += mult * (*q - '0');
				else 
					return last;
				mult *= 10;
				--q;
			}
		}
		if ( encoding == TIXML_ENCODING_UTF8 )
		{
			// convert the UCS to UTF-8
			ConvertUTF32ToUTF8( ucs, value, length );
		}
		else
		{
			*value = (char)ucs;
			*length = 1;
		}
		return first + delta + 1;
	}

	// Now try to match it.
	for( i=0; i<NUM_ENTITY; ++i )
	{
		if ( equal( entity[i].str, entity[i].str+ entity[i].strLength, first ) )
		{
			assert( strlen( entity[i].str ) == entity[i].strLength );
			*value = entity[i].chr;
			*length = 1;
			return ( first + entity[i].strLength );
		}
	}

	// So it wasn't an entity, its unrecognized, or something like that.
	*value = *first;	// Don't put back the last one, since we return it!
	//*length = 1;	// Leave unrecognized entities - this doesn't really work.
					// Just writes strange XML.
	return first+1;
}


bool TiXmlBase::StringEqual( std::string::const_iterator first, std::string::const_iterator last, const std::string & tag, bool ignoreCase )
{
	function<bool(char,char)> comp[] = 
	{
		equal_to(),
		[](char c1,char c2) {return std::tolower(c1) == tolower(c2); }
	};
	auto res = equal(first, first+std::min(int(last-first),(int)tag.size()), tag.begin(), tag.end(),comp[ignoreCase]);
	return res;
}

std::string::const_iterator TiXmlBase::ReadText(std::string::const_iterator first, std::string::const_iterator last,
	TIXML_STRING * text,bool trimWhiteSpace,const char* endTag,	bool caseInsensitive, TiXmlEncoding encoding )
{
    *text = "";
	if (    !trimWhiteSpace			// certain tags always keep whitespace
		 || !condenseWhiteSpace )	// if true, whitespace is always kept
	{
		// Keep all the white space.
		while (first !=last
				&& !StringEqual(first,last , endTag, caseInsensitive )
			  )
		{
			int len;
			char cArr[4] = { 0, 0, 0, 0 };
			first = GetChar( first,last , cArr, &len, encoding );
			text->append( cArr, len );
		}
	}
	else
	{
		bool whitespace = false;

		// Remove leading white space:
		first = SkipWhiteSpace( first,last );
		while (	   first!=last
				&& !StringEqual( first,last , endTag, caseInsensitive ) )
		{
			if ( *first == '\r' || *first == '\n' )
			{
				whitespace = true;
				++first;
			}
			else if ( IsWhiteSpace( *first ) )
			{
				whitespace = true;
				++first;
			}
			else
			{
				// If we've found whitespace, add it before the
				// new character. Any whitespace just becomes a space.
				if ( whitespace )
				{
					(*text) += ' ';
					whitespace = false;
				}
				int len;
				char cArr[4] = { 0, 0, 0, 0 };
				first = GetChar( first,last , cArr, &len, encoding );
				if ( len == 1 )
					(*text) += cArr[0];	// more efficient
				else
					text->append( cArr, len );
			}
		}
	}
	if ( first!=last )
		first += strlen( endTag );
	return ( first!=last) ? first : last;
}

#ifdef TIXML_USE_STL

void TiXmlDocument::StreamIn( std::istream * in, TIXML_STRING * tag )
{
	// The basic issue with a document is that we don't know what we're
	// streaming. Read something presumed to be a tag (and hope), then
	// identify it, and call the appropriate stream method on the tag.
	//
	// This "pre-streaming" will never read the closing ">" so the
	// sub-tag can orient itself.

	if ( !StreamTo( in, '<', tag ) ) 
	{
		string str;
		SetError( TIXML_ERROR_PARSING_EMPTY, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN );
		return;
	}

	while ( in->good() )
	{
		int tagIndex = (int) tag->length();
		while ( in->good() && in->peek() != '>' )
		{
			int c = in->get();
			if ( c <= 0 )
			{
				string str;
				SetError( TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN );
				break;
			}
			(*tag) += (char) c;
		}

		if ( in->good() )
		{
			// We now have something we presume to be a node of 
			// some sort. Identify it, and call the node to
			// continue streaming.
			TiXmlNode* node = Identify( tag->begin() + tagIndex,tag->end() , TIXML_DEFAULT_ENCODING );

			if ( node )
			{
				node->StreamIn( in, tag );
				bool isElement = node->ToElement() != 0;
				delete node;
				node = 0;

				// If this is the root element, we're done. Parsing will be
				// done by the >> operator.
				if ( isElement )
				{
					return;
				}
			}
			else
			{
				string str;
				SetError( TIXML_ERROR, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN );
				return;
			}
		}
	}
	// We should have returned sooner.
	string str;
	SetError( TIXML_ERROR, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN );
}

#endif

std::string::const_iterator TiXmlDocument::Parse(std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* prevData, TiXmlEncoding encoding )
{
	ClearError();

	// Parse away, at the document level. Since a document
	// contains nothing but other tags, most of what happens
	// here is skipping white space.
	if ( first==last )
	{
		SetError( TIXML_ERROR_DOCUMENT_EMPTY, first,last , 0, TIXML_ENCODING_UNKNOWN );
		return last;
	}

	// Note that, for a document, this needs to come
	// before the while space skip, so that parsing
	// starts from the pointer we are given.
	location.Clear();
	if ( prevData )
	{
		location.row = prevData->cursor.row;
		location.col = prevData->cursor.col;
	}
	else
	{
		location.row = 0;
		location.col = 0;
	}
	TiXmlParsingData data(first,last, TabSize(), location.row, location.col );
	location = data.Cursor();

	if ( encoding == TIXML_ENCODING_UNKNOWN )
	{
		// Check for the Microsoft UTF-8 lead bytes.
		string utfBom = {(char)0xef, (char)0xbb, (char)0xbf };
		auto itPar = mismatch(first, last,utfBom.begin(),utfBom.end());
		if (itPar.second ==utfBom.end())
		{
			encoding = TIXML_ENCODING_UTF8;
			useMicrosoftBOM = true;
			first= itPar.first;
		}
	}
	first = SkipWhiteSpace(first,last);
	if ( first==last )
	{
		SetError( TIXML_ERROR_DOCUMENT_EMPTY, first,last , 0, TIXML_ENCODING_UNKNOWN );
		return last;
	}

	while ( first!=last)
	{
		TiXmlNode* node = Identify( first,last , encoding );
		if ( node )
		{
			first = node->Parse( first,last, &data, encoding );
			LinkEndChild( node );
		}
		else
		{
			break;
		}

		// Did we get encoding info?
		if (    encoding == TIXML_ENCODING_UNKNOWN
			 && node->ToDeclaration() )
		{
			TiXmlDeclaration* dec = node->ToDeclaration();
			auto enc = dec->Encoding();

			if (!enc.empty())
				encoding = TIXML_ENCODING_UTF8;
			else if ( StringEqual( first,last , "UTF-8", true ) )
				encoding = TIXML_ENCODING_UTF8;
			else if ( StringEqual( first,last , "UTF8", true ) )
				encoding = TIXML_ENCODING_UTF8;	// incorrect, but be nice
			else 
				encoding = TIXML_ENCODING_LEGACY;
		}

		first = SkipWhiteSpace( first,last  );
	}

	// Was this empty?
	if ( !firstChild ) {
		SetError( TIXML_ERROR_DOCUMENT_EMPTY, first,last , 0, encoding );
		return last;
	}

	// All is well.
	return first;
}

void TiXmlDocument::SetError( int err, std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* data, TiXmlEncoding encoding )
{	
	// The first error in a chain is more accurate - don't set again!
	if ( error )
		return;

	assert( err > 0 && err < TIXML_ERROR_STRING_COUNT );
	error   = true;
	errorId = err;
	errorDesc = errorString[ errorId ];

	errorLocation.Clear();
	if ( first!=last && data )
	{
		data->Stamp( first,last , encoding  );
		errorLocation = data->Cursor();
	}
}


TiXmlNode* TiXmlNode::Identify( std::string::const_iterator first, std::string::const_iterator last, TiXmlEncoding encoding )
{
	TiXmlNode* returnNode = 0;

	first = SkipWhiteSpace( first,last);
	if( first==last || *first != '<' )
	{
		return 0;
	}

	first = SkipWhiteSpace(first, last);

	if ( first==last )
	{
		return 0;
	}

	// What is this thing? 
	// - Elements start with a letter or underscore, but xml is reserved.
	// - Comments: <!--
	// - Decleration: <?xml
	// - Everthing else is unknown to tinyxml.
	//

	const char* xmlHeader = { "<?xml" };
	const char* commentHeader = { "<!--" };
	const char* dtdHeader = { "<!" };
	const char* cdataHeader = { "<![CDATA[" };

	if ( StringEqual( first,last , xmlHeader, true ) )
	{
		#ifdef DEBUG_PARSER
			TIXML_LOG( "XML parsing Declaration\n" );
		#endif
		returnNode = new TiXmlDeclaration();
	}
	else if ( StringEqual( first,last , commentHeader, false ) )
	{
		#ifdef DEBUG_PARSER
			TIXML_LOG( "XML parsing Comment\n" );
		#endif
		returnNode = new TiXmlComment();
	}
	else if ( StringEqual( first,last , cdataHeader, false ) )
	{
		#ifdef DEBUG_PARSER
			TIXML_LOG( "XML parsing CDATA\n" );
		#endif
		TiXmlText* text = new TiXmlText( "" );
		text->SetCDATA( true );
		returnNode = text;
	}
	else if ( StringEqual( first,last , dtdHeader, false ) )
	{
		#ifdef DEBUG_PARSER
			TIXML_LOG( "XML parsing Unknown(1)\n" );
		#endif
		returnNode = new TiXmlUnknown();
	}
	else if (    IsAlpha( *(first+1), encoding )
			  || *(first+1) == '_' )
	{
		#ifdef DEBUG_PARSER
			TIXML_LOG( "XML parsing Element\n" );
		#endif
		returnNode = new TiXmlElement( "" );
	}
	else
	{
		#ifdef DEBUG_PARSER
			TIXML_LOG( "XML parsing Unknown(2)\n" );
		#endif
		returnNode = new TiXmlUnknown();
	}

	if ( returnNode )
	{
		// Set the parent, so it can report errors
		returnNode->parent = this;
	}
	return returnNode;
}

#ifdef TIXML_USE_STL

void TiXmlElement::StreamIn (std::istream * in, TIXML_STRING * tag)
{
	// We're called with some amount of pre-parsing. That is, some of "this"
	// element is in "tag". Go ahead and stream to the closing ">"
	while( in->good() )
	{
		int c = in->get();
		if ( c <= 0 )
		{
			TiXmlDocument* document = GetDocument();
			if (document)
			{
				string str;
				document->SetError(TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN);
			}
			return;
		}
		(*tag) += (char) c ;
		
		if ( c == '>' )
			break;
	}

	if ( tag->length() < 3 ) return;

	// Okay...if we are a "/>" tag, then we're done. We've read a complete tag.
	// If not, identify and stream.

	if (    tag->at( tag->length() - 1 ) == '>' 
		 && tag->at( tag->length() - 2 ) == '/' )
	{
		// All good!
		return;
	}
	else if ( tag->at( tag->length() - 1 ) == '>' )
	{
		// There is more. Could be:
		//		text
		//		cdata text (which looks like another node)
		//		closing tag
		//		another node.
		for ( ;; )
		{
			StreamWhiteSpace( in, tag );

			// Do we have text?
			if ( in->good() && in->peek() != '<' ) 
			{
				// Yep, text.
				TiXmlText text( "" );
				text.StreamIn( in, tag );

				// What follows text is a closing tag or another node.
				// Go around again and figure it out.
				continue;
			}

			// We now have either a closing tag...or another node.
			// We should be at a "<", regardless.
			if ( !in->good() ) return;
			assert( in->peek() == '<' );
			int tagIndex = (int) tag->length();

			bool closingTag = false;
			bool firstCharFound = false;

			for( ;; )
			{
				if ( !in->good() )
					return;

				int c = in->peek();
				if ( c <= 0 )
				{
					TiXmlDocument* document = GetDocument();
					if (document)
					{
						string str;
						document->SetError(TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN);
					}
					return;
				}
				
				if ( c == '>' )
					break;

				*tag += (char) c;
				in->get();

				// Early out if we find the CDATA id.
				if ( c == '[' && tag->size() >= 9 )
				{
					size_t len = tag->size();
					const char* start = tag->c_str() + len - 9;
					if ( strcmp( start, "<![CDATA[" ) == 0 ) {
						assert( !closingTag );
						break;
					}
				}

				if ( !firstCharFound && c != '<' && !IsWhiteSpace( c ) )
				{
					firstCharFound = true;
					if ( c == '/' )
						closingTag = true;
				}
			}
			// If it was a closing tag, then read in the closing '>' to clean up the input stream.
			// If it was not, the streaming will be done by the tag.
			if ( closingTag )
			{
				if ( !in->good() )
					return;

				int c = in->get();
				if ( c <= 0 )
				{
					TiXmlDocument* document = GetDocument();
					if (document)
					{
						string str;
						document->SetError(TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN);
					}
					return;
				}
				assert( c == '>' );
				*tag += (char) c;

				// We are done, once we've found our closing tag.
				return;
			}
			else
			{
				// If not a closing tag, id it, and stream.
				auto tagloc = tag->begin() + tagIndex;
				TiXmlNode* node = Identify( tagloc, tag->end(), TIXML_DEFAULT_ENCODING );
				if ( !node )
					return;
				node->StreamIn( in, tag );
				delete node;
				node = 0;

				// No return: go around from the beginning: text, closing tag, or node.
			}
		}
	}
}
#endif

std::string::const_iterator TiXmlElement::Parse(std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* data, TiXmlEncoding encoding )
{
	first = SkipWhiteSpace( first, last );
	TiXmlDocument* document = GetDocument();

	if ( first==last )
	{
		if ( document ) document->SetError( TIXML_ERROR_PARSING_ELEMENT, first,last , 0, encoding );
		return last;
	}

	if ( data )
	{
		data->Stamp( first, last, encoding  );
		location = data->Cursor();
	}

	if ( *first != '<' )
	{
		if ( document ) document->SetError( TIXML_ERROR_PARSING_ELEMENT, first, last, data, encoding );
		return last;
	}

	first = SkipWhiteSpace( first+1,  last);

	// Read the name.
	auto pErr = first;

    first = ReadName( first, last, &value, encoding );
	if ( first ==last )
	{
		if ( document )	document->SetError( TIXML_ERROR_FAILED_TO_READ_ELEMENT_NAME, pErr,last , data, encoding );
		return last;
	}

    TIXML_STRING endTag ("</");
	endTag += value;

	// Check for and read attributes. Also look for an empty
	// tag or an end tag.
	while ( first !=last)
	{
		pErr = first;
		first = SkipWhiteSpace( first, last );
		if ( first==last )
		{
			if ( document ) document->SetError( TIXML_ERROR_READING_ATTRIBUTES, pErr,last , data, encoding );
			return last;
		}
		if ( *first == '/' )
		{
			++first;
			// Empty tag.
			if ( *first  != '>' )
			{
				if ( document ) document->SetError( TIXML_ERROR_PARSING_EMPTY, first, last, data, encoding );		
				return last;
			}
			return (first+1);
		}
		else if ( *first == '>' )
		{
			// Done with attributes (if there were any.)
			// Read the value -- which can include other
			// elements -- read the end tag, and return.
			++first;
			first = ReadValue( first, last, data, encoding );		// Note this is an Element method, and will set the error if one happens.
			if (first==last ) {
				// We were looking for the end tag, but found nothing.
				// Fix for [ 1663758 ] Failure to report error on bad XML
				if ( document ) document->SetError( TIXML_ERROR_READING_END_TAG, first,last , data, encoding );
				return last;
			}

			// We should find the end tag now
			// note that:
			// </foo > and
			// </foo> 
			// are both valid end tags.
			if ( StringEqual( first,last , endTag.c_str(), false ) )
			{
				first += endTag.length();
				first = SkipWhiteSpace( first, last );
				if ( first !=last && *first == '>' ) {
					++first;
					return first;
				}
				if ( document ) document->SetError( TIXML_ERROR_READING_END_TAG, first,last , data, encoding );
				return last;
			}
			else
			{
				if ( document ) document->SetError( TIXML_ERROR_READING_END_TAG, first,last , data, encoding );
				return last;
			}
		}
		else
		{
			// Try to read an attribute:
			TiXmlAttribute* attrib = new TiXmlAttribute();
			if ( !attrib )
			{
				return last;
			}

			attrib->SetDocument( document );
			pErr = first;
			first = attrib->Parse( first,last, data, encoding );

			if ( first==last )
			{
				if ( document ) document->SetError( TIXML_ERROR_PARSING_ELEMENT, pErr,last , data, encoding );
				delete attrib;
				return last;
			}

			// Handle the strange case of double attributes:
			TiXmlAttribute* node = attributeSet.Find( attrib->NameTStr() );
			if ( node )
			{
				if ( document ) document->SetError( TIXML_ERROR_PARSING_ELEMENT, pErr,last , data, encoding );
				delete attrib;
				return last;
			}

			attributeSet.Add( attrib );
		}
	}
	return first;
}


std::string::const_iterator TiXmlElement::ReadValue( std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* data, TiXmlEncoding encoding )
{
	TiXmlDocument* document = GetDocument();

	// Read in text and elements in any order.
	auto pWithWhiteSpace = first;
	first = SkipWhiteSpace( first,  last);

	while ( first!=last )
	{
		if ( *first != '<' )
		{
			// Take what we have, make a text element.
			TiXmlText* textNode = new TiXmlText( "" );

			if ( !textNode )
			{
			    return last;
			}

			if ( TiXmlBase::IsWhiteSpaceCondensed() )
			{
				first = textNode->Parse( first,last, data, encoding );
			}
			else
			{
				// Special case: we want to keep the white space
				// so that leading spaces aren't removed.
				first = textNode->Parse( pWithWhiteSpace,last, data, encoding );
			}

			if ( !textNode->Blank() )
				LinkEndChild( textNode );
			else
				delete textNode;
		} 
		else 
		{
			// We hit a '<'
			// Have we hit a new element or an end tag? This could also be
			// a TiXmlText in the "CDATA" style.
			if ( StringEqual( first,last , "</", false ) )
			{
				return first;
			}
			else
			{
				TiXmlNode* node = Identify( first, last, encoding );
				if ( node )
				{
					first = node->Parse( first,last, data, encoding );
					LinkEndChild( node );
				}				
				else
				{
					return last;
				}
			}
		}
		pWithWhiteSpace = first;
		first = SkipWhiteSpace( first,  last);
	}

	if ( first==last )
	{
		if ( document ) document->SetError( TIXML_ERROR_READING_ELEMENT_VALUE, first,last , 0, encoding );
	}	
	return first;
}


#ifdef TIXML_USE_STL
void TiXmlUnknown::StreamIn( std::istream * in, TIXML_STRING * tag )
{
	while ( in->good() )
	{
		int c = in->get();	
		if ( c <= 0 )
		{
			TiXmlDocument* document = GetDocument();
			if (document)
			{
				string str;
				document->SetError(TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN);
			}
			return;
		}
		(*tag) += (char) c;

		if ( c == '>' )
		{
			// All is well.
			return;		
		}
	}
}
#endif


std::string::const_iterator TiXmlUnknown::Parse(std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* data, TiXmlEncoding encoding)
{
	TiXmlDocument* document = GetDocument();
	first = SkipWhiteSpace( first,last);

	if ( data )
	{
		data->Stamp( first,last, encoding );
		location = data->Cursor();
	}
	if ( first==last|| *first != '<' )
	{
		if ( document ) document->SetError( TIXML_ERROR_PARSING_UNKNOWN, first,last, data, encoding );
		return last;
	}
	++first;
    value = "";

	while ( first!=last&& *first != '>' )
	{
		value += *first;
		++first;
	}

	if ( first==last)
	{
		if ( document )	
			document->SetError( TIXML_ERROR_PARSING_UNKNOWN, first,last, 0, encoding );
	}
	if ( first!=last && *first == '>' )
		return first+1;
	return first;
}

#ifdef TIXML_USE_STL
void TiXmlComment::StreamIn( std::istream * in, TIXML_STRING * tag )
{
	while ( in->good() )
	{
		int c = in->get();	
		if ( c <= 0 )
		{
			TiXmlDocument* document = GetDocument();
			if (document)
			{
				string str;
				document->SetError(TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN);
			}
			return;
		}

		(*tag) += (char) c;

		if ( c == '>' 
			 && tag->at( tag->length() - 2 ) == '-'
			 && tag->at( tag->length() - 3 ) == '-' )
		{
			// All is well.
			return;		
		}
	}
}
#endif


string::const_iterator TiXmlComment::Parse( std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* data, TiXmlEncoding encoding )
{
	TiXmlDocument* document = GetDocument();
	value = "";
	regex re("(?:^|[\\s]*)<!--([\\S\\s]*?)-->");
	smatch m;
	auto res = regex_search(first, last, m, re);
	if (!res)
		return last;
	value = m[1].str();
	first = m[0].second;
	return first;
}


std::string::const_iterator TiXmlAttribute::Parse(std::string::const_iterator first, std::string::const_iterator last,
 	TiXmlParsingData* data, TiXmlEncoding encoding)
{
	first = SkipWhiteSpace( first, last );
	if ( first==last ) return last;

	if ( data )
	{
		data->Stamp( first, last, encoding  );
		location = data->Cursor();
	}
	// Read the name, the '=' and the value.
	auto pErr = first;
	first = ReadName( first,last , &name, encoding );
	if ( first==last)
	{
		if ( document ) document->SetError( TIXML_ERROR_READING_ATTRIBUTES, pErr,last , data, encoding );
		return last;
	}
	first = SkipWhiteSpace( first, last );
	if (first == last || *first != '=' )
	{
		if ( document ) document->SetError( TIXML_ERROR_READING_ATTRIBUTES, first, last, data, encoding );
		return last;
	}

	++first;	// skip '='
	first = SkipWhiteSpace( first, last );
	if (first == last)
	{
		if ( document ) document->SetError( TIXML_ERROR_READING_ATTRIBUTES, first, last, data, encoding );
		return last;
	}
	
	const char* end;
	const char SINGLE_QUOTE = '\'';
	const char DOUBLE_QUOTE = '\"';

	if ( *first == SINGLE_QUOTE )
	{
		++first;
		end = "\'";		// single quote in string
		first = ReadText( first, last, &value, false, end, false, encoding );
	}
	else if ( *first == DOUBLE_QUOTE )
	{
		++first;
		end = "\"";		// double quote in string
		first = ReadText(first,last, &value, false, end, false, encoding );
	}
	else
	{
		// All attribute values should be in single or double quotes.
		// But this is such a common error that the parser will try
		// its best, even without them.
		value = "";
		while (first != last											// existence
				&& !IsWhiteSpace( *first )								// whitespace
				&& *first != '/' && *first != '>' )							// tag end
		{
			if ( *first == SINGLE_QUOTE || *first == DOUBLE_QUOTE ) {
				// [ 1451649 ] Attribute values with trailing quotes not handled correctly
				// We did not have an opening quote but seem to have a 
				// closing one. Give up and throw an error.
				if ( document ) document->SetError( TIXML_ERROR_READING_ATTRIBUTES, first,last , data, encoding );
				return last;
			}
			value += *first;
			++first;
		}
	}
	return first;
}

#ifdef TIXML_USE_STL
void TiXmlText::StreamIn( std::istream * in, TIXML_STRING * tag )
{
	while ( in->good() )
	{
		int c = in->peek();	
		if ( !cdata && (c == '<' ) ) 
		{
			return;
		}
		if ( c <= 0 )
		{
			TiXmlDocument* document = GetDocument();
			if (document)
			{
				string str;
				document->SetError(TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN);
			}
			return;
		}

		(*tag) += (char) c;
		in->get();	// "commits" the peek made above

		if ( cdata && c == '>' && tag->size() >= 3 ) {
			size_t len = tag->size();
			if ( (*tag)[len-2] == ']' && (*tag)[len-3] == ']' ) {
				// terminator of cdata.
				return;
			}
		}    
	}
}
#endif

string::const_iterator TiXmlText::Parse( std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* data, TiXmlEncoding encoding )
{
	value = "";
	TiXmlDocument* document = GetDocument();

	if ( data )
	{
		data->Stamp( first,last , encoding  );
		location = data->Cursor();
	}

	const char* const startTag = "<!\\[CDATA\\[";
	const char* const endTag   = "\\]\\]>";

	if ( cdata || StringEqual(first ,last , startTag, false ) )
	{
		cdata = true;

		if ( !StringEqual(first, last, startTag, false ) )
		{
			if ( document )
				document->SetError( TIXML_ERROR_PARSING_CDATA, first, last, data, encoding );
			return last;
		}
		first += strlen( startTag );

		// Keep all the white space, ignore the encoding, etc.
		while (	   first != last
				&& !StringEqual(first, last, endTag, false )
			  )
		{
			value += *first;
			++first;
		}

		TIXML_STRING dummy; 
		first = ReadText(first, last, &dummy, false, endTag, false, encoding );
		return first;
	}
	else
	{
		bool ignoreWhite = true;

		const char* end = "<";
		first = ReadText(first, last, &value, ignoreWhite, end, false, encoding );
		if ( first!= last)
			return first-1;	// don't truncate the '<'
		return last;
	}
}

#ifdef TIXML_USE_STL
void TiXmlDeclaration::StreamIn( std::istream * in, TIXML_STRING * tag )
{
	while ( in->good() )
	{
		int c = in->get();
		if ( c <= 0 )
		{
			TiXmlDocument* document = GetDocument();
			if (document)
			{
				string str;
				document->SetError(TIXML_ERROR_EMBEDDED_NULL, str.begin(),str.end() , 0, TIXML_ENCODING_UNKNOWN);
			}
			return;
		}
		(*tag) += (char) c;

		if ( c == '>' )
		{
			// All is well.
			return;
		}
	}
}
#endif

string::const_iterator TiXmlDeclaration::Parse(std::string::const_iterator first, std::string::const_iterator last, TiXmlParsingData* data, TiXmlEncoding _encoding )
{
	first = SkipWhiteSpace( first, last);
	// Find the beginning, find the end, and look for
	// the stuff in-between.
	TiXmlDocument* document = GetDocument();
	if (first==last || !StringEqual(first, last, "<?xml", true ) )
	{
		if ( document ) document->SetError( TIXML_ERROR_PARSING_DECLARATION, first, last, 0, _encoding );
		return last;
	}
	if ( data )
	{
		data->Stamp( first, last, _encoding  );
		location = data->Cursor();
	}
	regex re(
		"(?:^|[\\s]*)<\\?xml"
		"(?:[\\s]+(version)[\\s]*=[\\s]*(?:(?:\"([^\"]*)\")|(?:\'([^\']*)\')))"
		"(?:[\\s]+(encoding)[\\s]*=[\\s]*(?:(?:\"([^\"]*)\")|(?:\'([^\']*)\')))?"
		"(?:[\\s]+(standalone)[\\s]*=[\\s]*(?:(?:\"[^\"]*\")|(?:\'([^\']*)\')))?"
		"[\\s]*\\??>",regex::icase
	);
	smatch m;
	auto res = regex_search(first, last, m, re);
	if (!res)
		return last;
	first =m[0].second;

	version = "";
	encoding = "";
	standalone = "";
	if (m[1].matched)
	{
		if (m[2].matched)
			version = m[2].str();
		else if (m[3].matched)
			version = m[3].str();
	}
	if (m[1].matched)
	{
		if (m[2].matched)
			version = m[2].str();
		else if (m[3].matched)
			version = m[3].str();
	}
	if (m[4].matched)
	{
		if (m[5].matched)
			encoding = m[5].str();
		else if (m[6].matched)
			encoding = m[6].str();
	}
	if (m[7].matched)
	{
		if (m[8].matched)
			encoding = m[8].str();
		else if (m[9].matched)
			encoding = m[9].str();
	}
	return first;
}

bool TiXmlText::Blank() const
{
	for ( unsigned i=0; i<value.length(); i++ )
		if ( !IsWhiteSpace( value[i] ) )
			return false;
	return true;
}

