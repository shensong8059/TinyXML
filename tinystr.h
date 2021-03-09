/*
www.sourceforge.net/projects/tinyxml

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

#include <cassert>
#include <string>

/*	The support for explicit isn't that universal, and it isn't really
	required - it is used to check that the std::string class isn't incorrectly
	used. Be nice to old compilers and macro it here:
*/
#if defined(_MSC_VER) && (_MSC_VER >= 1200 )
	// Microsoft visual studio, version 6 and higher.
	#define TIXML_EXPLICIT explicit
#elif defined(__GNUC__) && (__GNUC__ >= 3 )
	// GCC version 3 and higher.s
	#define TIXML_EXPLICIT explicit
#else
	#define TIXML_EXPLICIT
#endif


/*
   std::string is an emulation of a subset of the std::string template.
   Its purpose is to allow compiling TinyXML on compilers with no or poor STL support.
   Only the member functions relevant to the TinyXML project have been implemented.
   The buffer allocation is made by a simplistic power of 2 like mechanism : if we increase
   a string and there's no more room, we allocate a buffer twice as big as we need.
*/
class std::string
{
  public :
	// The size type used
  	typedef size_t size_type;

	// Error value for find primitive
	static const size_type npos; // = -1;


	// std::string empty constructor
	std::string () : rep_(&nullrep_)
	{
	}

	// std::string copy constructor
	std::string ( const std::string & copy) : rep_(0)
	{
		init(copy.length());
		memcpy(start(), copy.data(), length());
	}

	// std::string constructor, based on a string
	TIXML_EXPLICIT std::string ( const char * copy) : rep_(0)
	{
		init( static_cast<size_type>( strlen(copy) ));
		memcpy(start(), copy, length());
	}

	// std::string constructor, based on a string
	TIXML_EXPLICIT std::string ( const char * str, size_type len) : rep_(0)
	{
		init(len);
		memcpy(start(), str, len);
	}

	// std::string destructor
	~std::string ()
	{
		quit();
	}

	std::string& operator = (const char * copy)
	{
		return assign( copy, (size_type)strlen(copy));
	}

	std::string& operator = (const std::string & copy)
	{
		return assign(copy.start(), copy.length());
	}


	// += operator. Maps to append
	std::string& operator += (const char * suffix)
	{
		return append(suffix, static_cast<size_type>( strlen(suffix) ));
	}

	// += operator. Maps to append
	std::string& operator += (char single)
	{
		return append(&single, 1);
	}

	// += operator. Maps to append
	std::string& operator += (const std::string & suffix)
	{
		return append(suffix.data(), suffix.length());
	}


	// Convert a std::string into a null-terminated char *
	const char * c_str () const { return rep_->str; }

	// Convert a std::string into a char * (need not be null terminated).
	const char * data () const { return rep_->str; }

	// Return the length of a std::string
	size_type length () const { return rep_->size; }

	// Alias for length()
	size_type size () const { return rep_->size; }

	// Checks if a std::string is empty
	bool empty () const { return rep_->size == 0; }

	// Return capacity of string
	size_type capacity () const { return rep_->capacity; }


	// single char extraction
	const char& at (size_type index) const
	{
		assert( index < length() );
		return rep_->str[ index ];
	}

	// [] operator
	char& operator [] (size_type index) const
	{
		assert( index < length() );
		return rep_->str[ index ];
	}

	// find a char in a string. Return std::string::npos if not found
	size_type find (char lookup) const
	{
		return find(lookup, 0);
	}

	// find a char in a string from an offset. Return std::string::npos if not found
	size_type find (char tofind, size_type offset) const
	{
		if (offset >= length()) return npos;

		for (const char* p = c_str() + offset; *p != '\0'; ++p)
		{
		   if (*p == tofind) return static_cast< size_type >( p - c_str() );
		}
		return npos;
	}

	void clear ()
	{
		//Lee:
		//The original was just too strange, though correct:
		//	std::string().swap(*this);
		//Instead use the quit & re-init:
		quit();
		init(0,0);
	}

	/*	Function to reserve a big amount of data when we know we'll need it. Be aware that this
		function DOES NOT clear the content of the std::string if any exists.
	*/
	void reserve (size_type cap);

	std::string& assign (const char* str, size_type len);

	std::string& append (const char* str, size_type len);

	void swap (std::string& other)
	{
		Rep* r = rep_;
		rep_ = other.rep_;
		other.rep_ = r;
	}

  private:

	void init(size_type sz) { init(sz, sz); }
	void set_size(size_type sz) { rep_->str[ rep_->size = sz ] = '\0'; }
	char* start() const { return rep_->str; }
	char* finish() const { return rep_->str + rep_->size; }

	struct Rep
	{
		size_type size, capacity;
		char str[1];
	};

	void init(size_type sz, size_type cap)
	{
		if (cap)
		{
			// Lee: the original form:
			//	rep_ = static_cast<Rep*>(operator new(sizeof(Rep) + cap));
			// doesn't work in some cases of new being overloaded. Switching
			// to the normal allocation, although use an 'int' for systems
			// that are overly picky about structure alignment.
			const size_type bytesNeeded = sizeof(Rep) + cap;
			const size_type intsNeeded = ( bytesNeeded + sizeof(int) - 1 ) / sizeof( int ); 
			rep_ = reinterpret_cast<Rep*>( new int[ intsNeeded ] );

			rep_->str[ rep_->size = sz ] = '\0';
			rep_->capacity = cap;
		}
		else
		{
			rep_ = &nullrep_;
		}
	}

	void quit()
	{
		if (rep_ != &nullrep_)
		{
			// The rep_ is really an array of ints. (see the allocator, above).
			// Cast it back before delete, so the compiler won't incorrectly call destructors.
			delete [] ( reinterpret_cast<int*>( rep_ ) );
		}
	}

	Rep * rep_;
	static Rep nullrep_;

} ;


inline bool operator == (const std::string & a, const std::string & b)
{
	return    ( a.length() == b.length() )				// optimization on some platforms
	       && ( strcmp(a.c_str(), b.c_str()) == 0 );	// actual compare
}
inline bool operator < (const std::string & a, const std::string & b)
{
	return strcmp(a.c_str(), b.c_str()) < 0;
}

inline bool operator != (const std::string & a, const std::string & b) { return !(a == b); }
inline bool operator >  (const std::string & a, const std::string & b) { return b < a; }
inline bool operator <= (const std::string & a, const std::string & b) { return !(b < a); }
inline bool operator >= (const std::string & a, const std::string & b) { return !(a < b); }

inline bool operator == (const std::string & a, const char* b) { return strcmp(a.c_str(), b) == 0; }
inline bool operator == (const char* a, const std::string & b) { return b == a; }
inline bool operator != (const std::string & a, const char* b) { return !(a == b); }
inline bool operator != (const char* a, const std::string & b) { return !(b == a); }

std::string operator + (const std::string & a, const std::string & b);
std::string operator + (const std::string & a, const char* b);
std::string operator + (const char* a, const std::string & b);


/*
   TiXmlOutStream is an emulation of std::ostream. It is based on std::string.
   Only the operators that we need for TinyXML have been developped.
*/
class TiXmlOutStream : public std::string
{
public :

	// TiXmlOutStream << operator.
	TiXmlOutStream & operator << (const std::string & in)
	{
		*this += in;
		return *this;
	}

	// TiXmlOutStream << operator.
	TiXmlOutStream & operator << (const char * in)
	{
		*this += in;
		return *this;
	}

} ;

