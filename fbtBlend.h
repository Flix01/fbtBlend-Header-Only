/*
-------------------------------------------------------------------------------
    This file is part of FBT (File Binary Tables).
    http://gamekit.googlecode.com/

    Copyright (c) 2010 Charlie C & Erwin Coumans.

-------------------------------------------------------------------------------
  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
-------------------------------------------------------------------------------
*/
/*
    Slightly modified by Flix01 (same license) in the following way:
    -> Now fbtBlend.h is header-only (needs FBTBLEND_IMPLEMENTATION before including it in a single .cpp file).
    ->     So now only fbtBlend.h and Blender.h can be used to parse a .blend file.
    -> Updated Blender Version to 279 (64bit, little endian).
    -> Removed all the compilation warning gcc could find.
    -> FBT_HAVE_CONFIG now has its meaning reversed: if you want to include "fbtCnfig.h" you must define it.
    -> Added int parse(const char* path);
    -> Remember to #define FBT_USE_GZ_FILE 1, if you want to support PM_COMPRESSED .blend files (you need to link to zlib: -lz).
    -> Added workaround for people just defining FBT_USE_GZ_FILE, without setting it to 1.
    -> In fbtFile, the method(s) to parse from file are:
       -> Before:
       int parse(const char* path, int mode = PM_UNCOMPRESSED);
       -> Now:
       int parse(const char* path,const char* uncompressedFileDetectorPrefix="BLENDER");
       int parse(const char* path, int mode);
       // This allows auto-detection of the .blend file compression, by just using: parse(const char* path);
     -> Added a public static helper method fbtFile::UTF8_fopen(...) (used internally to to allow UTF8 char paths on Windows).
     -> Minor changes made to class fbtArray<...> and its iterator (but code is still backward-compatible).
     -> Added class fbtString (not used here), as a replacement for std::string.

     -> Added support for zstd decompression through #define FBT_USE_ZSTD (it needs linking to -lzstd).
     -> Updated the "ID enums" (please search: [DNA_ID_enums 1/2] and [DNA_ID_enums 2/2]).
*/
#ifndef _fbtBlend_h_
#define _fbtBlend_h_

//#include "fbtFile.h"
#ifndef _fbtFile_h_
#define _fbtFile_h_

// #include "fbtTypes.h"
#ifndef _fbtTypes_h_
#define _fbtTypes_h_

#ifndef FBT_HAVE_CONFIG
// global config settings
//#define FBT_USE_GZ_FILE 1           // Adds support to PM_COMPRESSED .blend < 3.00 files (needs linking to zlib: -lz)
//#define FBT_USE_ZSTD_FILE 1         // Adds support to PM_COMPRESSED .blend 3.00 files (needs linking to zlib: -lzstd)
#define fbtDEBUG        1           // Traceback detail
//#define FBT_TYPE_LEN_VALIDATE   1   // Write a validation file (use MakeFBT.cmake->ADD_FBT_VALIDATOR to add a self validating build)
// global config settings end
#else
#include "fbtConfig.h"
#endif

// Workaround for people just defining FBT_USE_GZ_FILE, without setting it to 1
#ifdef FBT_USE_GZ_FILE
#   undef FBT_USE_GZ_FILE
#   define FBT_USE_GZ_FILE 1
#endif
// Workaround for people just defining FBT_USE_ZSTD_FILE, without setting it to 1
#ifdef FBT_USE_ZSTD_FILE
#   undef FBT_USE_ZSTD_FILE
#   define FBT_USE_ZSTD_FILE 1
#endif

#include <string.h> //memcmp

#include "Blender.h"	// THIS FILE DEPENDS ON THE BLENDER VERSION (TOGETHER WITH bfBlender.cpp THAT'S AT THE BOTTOM OF THIS FILE)

// global config settings
#ifndef fbtMaxTable
#	if BLENDER_VERSION<306
#   	define fbtMaxTable     5000        // Maximum number of elements in a table
#	else
#   	define fbtMaxTable     6000        // Maximum number of elements in a table
#	endif
#endif
#ifndef fbtMaxID
#   define fbtMaxID        64          // Maximum character array length
#endif
#ifndef fbtMaxMember
#	define fbtMaxMember    256         // Maximum number of members in a struct or class.
#endif
#ifndef fbtDefaultAlloc
#   define fbtDefaultAlloc 2048        // Table default allocation size
#endif
#ifndef FBT_ARRAY_SLOTS
#   if BLENDER_VERSION>279
#       define FBT_ARRAY_SLOTS         3   // Maximum dimensional array, eg: (int m_member[..][..] -> [FBT_ARRAY_SLOTS])
#   else
#       define FBT_ARRAY_SLOTS         2   // Maximum dimensional array, eg: (int m_member[..][..] -> [FBT_ARRAY_SLOTS])
#   endif
#endif
// global config settings end


/** \addtogroup FBT
*  @{
*/

#if (defined(DEBUG) || defined(_DEBUG)) && FBT_USE_DEBUG_ASSERTS == 1
# include <assert.h> // Keep this the only std include in headers
# define FBT_DEBUG 1
# define FBT_ASSERT(x) assert(x)
#else
# define FBT_ASSERT(x) ((void)0)
#endif

#ifdef FBT_USE_COMPILER_CHECKS
#define FBT_ASSERTCOMP(x, n) typedef bool x[(n) ? 1 : -1];
#else
#define FBT_ASSERTCOMP(x, n)
#endif


#define FBT_PLATFORM_WIN32    0
#define FBT_PLATFORM_LINUX    2
#define FBT_PLATFORM_APPLE    3

#if defined (_WIN32)
# define FBT_PLATFORM FBT_PLATFORM_WIN32
#elif defined(__APPLE__)
# define FBT_PLATFORM FBT_PLATFORM_APPLE
#else
# define FBT_PLATFORM FBT_PLATFORM_LINUX
#endif

#define FBT_COMPILER_MSVC    0
#define FBT_COMPILER_GNU     1

#if defined(_MSC_VER)
# define FBT_COMPILER FBT_COMPILER_MSVC
#elif defined(__GNUC__)
# define FBT_COMPILER FBT_COMPILER_GNU
#else
# error unknown compiler
#endif

#define FBT_ENDIAN_LITTLE    0
#define FBT_ENDIAN_BIG       1

#if defined(__sgi)      ||  defined (__sparc)        || \
    defined (__sparc__) ||  defined (__PPC__)        || \
    defined (__ppc__)   ||  defined (__BIG_ENDIAN__)
#define FBT_ENDIAN FBT_ENDIAN_BIG
#else
#define FBT_ENDIAN FBT_ENDIAN_LITTLE
#endif

#if FBT_ENDIAN == FBT_ENDIAN_BIG
# define FBT_ID(a,b,c,d) ( (int)(a)<<24 | (int)(b)<<16 | (c)<<8 | (d) )
# define FBT_ID2(c, d)   ( (c)<<8 | (d) )
#else
# define FBT_ID(a,b,c,d) ( (int)(d)<<24 | (int)(c)<<16 | (b)<<8 | (a) )
# define FBT_ID2(c, d)   ( (d)<<8 | (c) )
#endif

#define FBT_ARCH_32 0
#define FBT_ARCH_64 1

#if defined(__x86_64__)     || defined(_M_X64)      || \
    defined(__powerpc64__)  || defined(__alpha__)   || \
    defined(__ia64__)       || defined(__s390__)    || \
    defined(__s390x__)
#define FBT_ARCH FBT_ARCH_64
#else
#define FBT_ARCH FBT_ARCH_32
#endif


#if FBT_PLATFORM == FBT_PLATFORM_WIN32
# if defined(__MINGW32__) || \
     defined(__CYGWIN__)  || \
     (defined (_MSC_VER) && _MSC_VER < 1300)
#  define FBT_INLINE inline
# else
#  define FBT_INLINE __forceinline
# endif
#else
#  define FBT_INLINE    inline
#endif

// Integer types
typedef long            FBTlong;
typedef unsigned long   FBTulong;
typedef int             FBTint32;
typedef unsigned int    FBTuint32;
typedef short           FBTint16;
typedef unsigned short  FBTuint16;
typedef signed char     FBTint8;
typedef unsigned char   FBTuint8;
typedef unsigned char   FBTubyte;
typedef signed char     FBTbyte;
typedef bool            FBTint1;


#if FBT_COMPILER == FBT_COMPILER_GNU
typedef long long          FBTint64;
typedef unsigned long long FBTuint64;
#else
typedef __int64          FBTint64;
typedef unsigned __int64 FBTuint64;
#endif

// Arch dependent types

#if FBT_ARCH == FBT_ARCH_64
typedef FBTuint64   FBTuintPtr;
typedef FBTint64    FBTintPtr;
#else
typedef FBTuint32   FBTuintPtr;
typedef FBTint32    FBTintPtr;
#endif

typedef FBTuintPtr  FBTsize;


// Type for arrays & tables (Always unsigned & 32bit)
typedef FBTuint32       FBTsizeType;
const   FBTuint32       FBT_NPOS = (FBTuint32) - 1;
typedef FBTuint32       FBThash;
typedef FBTuint16       FBTtype;


FBT_ASSERTCOMP(FBTsizeType_NPOS_VALIDATE, FBT_NPOS == ((FBTuint32) - 1));

#define _FBT_CACHE_LIMIT 999

template <typename T> FBT_INLINE void    fbtSwap(T& a, T& b)                              { T t(a); a = b; b = t; }
template <typename T> FBT_INLINE T       fbtMax(const T& a, const T& b)                   { return a < b ? b : a; }
template <typename T> FBT_INLINE T       fbtMin(const T& a, const T& b)                   { return a < b ? a : b; }
template <typename T> FBT_INLINE T       fbtClamp(const T& v, const T& a, const T& b)     { return v < a ? a : v > b ? b : v; }


#define fbtMalloc(size)             ::malloc(size)
#define fbtCalloc(size, len)        ::calloc(size, len)
#define fbtFree(ptr)                ::free(ptr)
#define fbtMemset                   ::memset
#define fbtMemcpy                   ::memcpy
#define fbtMemmove                  ::memmove
#define fbtMemcmp                   ::memcmp



class fbtDebugger
{
public:
	typedef void (*ReportHook) (FBTuintPtr client, const char* buffer);

	struct Reporter
	{
		FBTuintPtr m_client;
		ReportHook m_hook;
	};

	static bool isDebugger(void);
	static void reportIDE(const char* src, long line, const char* msg, ...);
	static void errorIDE(const char* src, long line, const char* msg, ...);
	static void report(const char* msg, ...);
	static void breakProcess(void);

	static void setReportHook(Reporter& hook);

private:
	static Reporter m_report;

};

#define fbtPrintf fbtDebugger::report
#define fbtTRACE  fbtDebugger::reportIDE
#define fbtERROR  fbtDebugger::errorIDE

typedef enum fbtEndian
{
	FBT_ENDIAN_IS_BIG       = 0,
	FBT_ENDIAN_IS_LITTLE    = 1,
	FBT_ENDIAN_NATIVE,
} fbtEndian;

typedef union fbtEndianTest
{
	FBTbyte  bo[4];
	FBTint32 test;
} fbtEndianTest;



FBT_INLINE fbtEndian fbtGetEndian(void)
{
	fbtEndianTest e;
	e.test = FBT_ENDIAN_IS_LITTLE;
	return static_cast<fbtEndian>(e.bo[0]);
}


FBT_INLINE bool fbtIsEndian(const fbtEndian& endian)
{
	fbtEndianTest e;
	e.test = endian;
	return static_cast<fbtEndian>(e.bo[0]) == endian;
}

FBT_INLINE FBTuint16 fbtSwap16(FBTuint16 in)
{
	return static_cast<FBTuint16>(((in & 0xFF00) >> 8) | ((in & 0x00FF) << 8));
}

FBT_INLINE FBTuint32 fbtSwap32(const FBTuint32& in)
{
	return (((in & 0xFF000000) >> 24) | ((in & 0x00FF0000) >> 8) | ((in & 0x0000FF00) << 8)  | ((in & 0x000000FF) << 24));
}

FBT_INLINE FBTint16 fbtSwap16(FBTint16 in)
{
	return fbtSwap16(static_cast<FBTuint16>(in));
}

FBT_INLINE FBTint32 fbtSwap32(const FBTint32& in)
{
	return fbtSwap32(static_cast<FBTuint32>(in));
}


FBT_INLINE FBTuint64 fbtSwap64(const FBTuint64& in)
{
	FBTuint64 r = 0;
	const FBTubyte* src = reinterpret_cast<const FBTubyte*>(&in);
	FBTubyte* dst = reinterpret_cast<FBTubyte*>(&r);

	dst[0] = src[7];
	dst[1] = src[6];
	dst[2] = src[5];
	dst[3] = src[4];
	dst[4] = src[3];
	dst[5] = src[2];
	dst[6] = src[1];
	dst[7] = src[0];
	return r;
}


FBT_INLINE void fbtSwap16(FBTuint16* sp, FBTsize len)
{
	FBTsizeType i;
	for (i = 0; i < len; ++i)
	{
		*sp = fbtSwap16(*sp);
		++sp;	
	}	
}

FBT_INLINE void fbtSwap32(FBTuint32* ip, FBTsize len)
{	 
	FBTsizeType i;
	for (i = 0; i < len; ++i)
	{
		*ip = fbtSwap32(*ip);
		++ip;
	}
}


FBT_INLINE void fbtSwap64(FBTuint64* dp, FBTsize len)
{	 
	FBTsizeType i;
	for (i = 0; i < len; ++i)
	{
		*dp = fbtSwap64(*dp);
		++dp;
	}
}


FBT_INLINE FBTint64 fbtSwap64(const FBTint64& in)
{
	return fbtSwap64(static_cast<FBTuint64>(in));
}



enum fbtPointerLen
{
	FBT_VOID  = sizeof(void*),
	FBT_VOID4 = FBT_VOID == 4,
	FBT_VOID8 = FBT_VOID == 8,
};


#if FBT_ARCH == FBT_ARCH_64
FBT_ASSERTCOMP(FBT_VOID8_ASSERT, FBT_VOID8);
#else
FBT_ASSERTCOMP(FBT_VOID4_ASSERT, FBT_VOID4);
#endif




class fbtList
{
public:

	struct Link
	{
		Link*   next;
		Link*   prev;
	};


	Link*   first;
	Link*   last;

public:

	fbtList() : first(0), last(0) {}
	~fbtList() { clear(); }

	void clear(void) { first = last = 0; }

	void push_back(void* v)
	{
		Link* link = ((Link*)v);
		if (!link)
			return;

		link->prev = last;
		if (last)
			last->next = link;

		if (!first)
			first = link;

		last = link;
	}
};





template <typename T>
class fbtArrayIterator
{
public:
	typedef typename T::Pointer             Iterator;
	typedef typename T::ReferenceType       ValueType;
	typedef typename T::ConstReferenceType  ConstValueType;

protected:

	mutable Iterator        m_iterator;
	mutable FBTsizeType     m_cur;
	mutable FBTsizeType     m_capacity;

public:


	fbtArrayIterator() : m_iterator(0), m_cur(0), m_capacity(0) {}
	fbtArrayIterator(Iterator begin, FBTsizeType size) : m_iterator(begin), m_cur(0), m_capacity(size) {}
	fbtArrayIterator(T& v) : m_iterator(v.ptr()), m_cur(0), m_capacity(v.size()) { }
    fbtArrayIterator(T& v,FBTsizeType cur) : m_iterator(v.ptr()), m_cur(cur), m_capacity(v.size()) { }


	~fbtArrayIterator() {}

    FBT_INLINE bool           hasMoreElements(void) const { return m_iterator && m_cur < m_capacity; }
	FBT_INLINE ValueType      getNext(void)               { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur++];  }
	FBT_INLINE ConstValueType getNext(void) const         { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur++];  }
	FBT_INLINE void           next(void) const            { FBT_ASSERT(hasMoreElements()); ++m_cur; }
	FBT_INLINE ValueType      peekNext(void)              { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur]; }
	FBT_INLINE ConstValueType peekNext(void) const        { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur]; }

    FBT_INLINE bool operator==(const fbtArrayIterator& rhs) const {return (m_iterator==rhs.m_iterator && m_cur==rhs.m_cur && m_capacity==rhs.m_capacity);}  // default should work
    FBT_INLINE static fbtArrayIterator Find(const fbtArrayIterator& start,const fbtArrayIterator& finish,ConstValueType v) {
        FBT_ASSERT(start.m_iterator==finish.m_iterator);
        FBT_ASSERT(start.m_capacity==finish.m_capacity);
        FBT_ASSERT(start.m_cur<finish.m_cur);
        for (FBTsizeType i = start.m_cur; i < finish.m_cur; i++)   {
            if (start.m_iterator[i] == v) return fbtArrayIterator(start,i);
        }
        return fbtArrayIterator(start,finish.m_cur);
    }

protected:
    fbtArrayIterator(const fbtArrayIterator& rhs,FBTsizeType cur) : m_iterator(rhs.m_iterator), m_cur(cur), m_capacity(rhs.m_capacity) { }


};

template <typename T>
class fbtArray
{
public:
	typedef T*           Pointer;
	typedef const T*     ConstPointer;

	typedef T            ValueType;
	typedef const T      ConstValueType;

	typedef T&           ReferenceType;
	typedef const T&     ConstReferenceType;

	typedef fbtArrayIterator<fbtArray<T> >       Iterator;
	typedef const fbtArrayIterator<fbtArray<T> > ConstIterator;

    //typedef Iterator        iterator;
    //typedef ConstIterator   const_Iterator;


public:
	fbtArray() : m_size(0), m_capacity(0), m_data(0), m_cache(0)  {}

    fbtArray(FBTsizeType size) : m_size(0), m_capacity(0), m_data(0), m_cache(0)  {resize(size);}

    fbtArray(FBTsizeType size,const T& fill) : m_size(0), m_capacity(0), m_data(0), m_cache(0)  {resize(size,fill);}

	fbtArray(const fbtArray<T>& o)
		: m_size(o.size()), m_capacity(0), m_data(0), m_cache(0)
	{
		reserve(m_size);
		copy(m_data, o.m_data, m_size);
	}

	~fbtArray() { clear(); }

	void clear(bool useCache = false)
	{
		if (!useCache)
		{
			if (m_data)
				delete []m_data;
			m_data = 0;
			m_capacity = 0;
			m_size = 0;
			m_cache = 0;
		}
		else
		{
			++m_cache;
			if (m_cache > _FBT_CACHE_LIMIT )
				clear(false);
			else m_size = 0;
		}
	}

	FBTsizeType find(const T& v)
	{
		for (FBTsizeType i = 0; i < m_size; i++)
		{
			if (m_data[i] == v)
				return i;
		}
		return FBT_NPOS;
	}

    FBT_INLINE static Iterator Find(const Iterator& start,const Iterator& finish,const ConstValueType& v) {return Iterator::Find(start,finish,v);}

	FBT_INLINE void push_back(const T& v)
	{
        if (m_size == m_capacity)   {
            const T v_val = v;  // Hehe, v can be a reference to old m_data here!
            reserve(m_size == 0 ? 8 : (m_size * 2));
            m_data[m_size] = v_val;
        }
        else m_data[m_size] = v;
		m_size++;
	}

	FBT_INLINE void pop_back(void)
	{
		m_size--;
		m_data[m_size].~T();
	}

	void erase(const T& v)
	{
		erase(find(v));
	}

	void erase(FBTsizeType pos)
	{
		if (m_size > 0)
		{
			if (pos != FBT_NPOS)
			{
				swap(pos, m_size - 1);
				m_size--;
				m_data[m_size].~T();
			}
		}
	}

	void resize(FBTsizeType nr)
	{
		if (nr < m_size)
		{
			for (FBTsizeType i = m_size; i < nr; i++)
				m_data[i].~T();
		}
		else
		{
			if (nr > m_size)
				reserve(nr);
		}
		m_size = nr;
	}

	void resize(FBTsizeType nr, const T& fill)
	{
		if (nr < m_size)
		{
			for (FBTsizeType i = m_size; i < nr; i++)
				m_data[i].~T();
		}
		else
		{
			if (nr > m_size)
				reserve(nr);
			for (FBTsizeType i = m_size; i < nr; i++)
				m_data[i] = fill;

		}
		m_size = nr;
	}

	void reserve(FBTsizeType nr)
	{

		if (m_capacity < nr)
		{
			T* p = new T[nr];
			if (m_data != 0)
			{
				copy(p, m_data, m_size);
				delete []m_data;
			}
			m_data = p;
			m_capacity = nr;
		}
	}

	void sort(bool (*cmp)(const T& a, const T& b))
	{
		if (m_size > 1 && cmp)
			_sort(cmp, 0, m_size - 1);
	}

	FBT_INLINE T& operator[](FBTsizeType idx)               { FBT_ASSERT(idx >= 0 && idx < m_capacity); return m_data[idx]; }
	FBT_INLINE const T& operator[](FBTsizeType idx) const   { FBT_ASSERT(idx >= 0 && idx < m_capacity); return m_data[idx]; }
	FBT_INLINE T& at(FBTsizeType idx)                       { FBT_ASSERT(idx >= 0 && idx < m_capacity); return m_data[idx]; }
	FBT_INLINE const T& at(FBTsizeType idx) const           { FBT_ASSERT(idx >= 0 && idx < m_capacity); return m_data[idx]; }
	FBT_INLINE T& front(void)                               { FBT_ASSERT(m_size > 0); return m_data[0]; }
	FBT_INLINE T& back(void)                                { FBT_ASSERT(m_size > 0); return m_data[m_size-1]; }

	FBT_INLINE ConstPointer ptr(void) const             { return m_data; }
	FBT_INLINE Pointer      ptr(void)                   { return m_data; }
	FBT_INLINE bool         valid(void) const           { return m_data != 0;}

	FBT_INLINE FBTsizeType capacity(void) const         { return m_capacity; }
	FBT_INLINE FBTsizeType size(void) const             { return m_size; }
	FBT_INLINE bool empty(void) const                   { return m_size == 0;}

    FBT_INLINE Iterator       iterator(void)       { return m_data && m_size > 0 ? Iterator(m_data, m_size) : Iterator(); }
    FBT_INLINE ConstIterator  iterator(void) const { return m_data && m_size > 0 ? ConstIterator(m_data, m_size) : ConstIterator(); }

    FBT_INLINE Iterator       begin(void)       { return m_data && m_size > 0 ? Iterator(m_data, m_size) : Iterator(); }
    FBT_INLINE ConstIterator  begin(void) const { return m_data && m_size > 0 ? ConstIterator(m_data, m_size) : ConstIterator(); }
    FBT_INLINE Iterator       end(void)       { return m_data && m_size > 0 ? Iterator(*this, m_size) : Iterator(); }
    FBT_INLINE ConstIterator  end(void) const { return m_data && m_size > 0 ? ConstIterator(*this, m_size) : ConstIterator(); }


	fbtArray<T> &operator= (const fbtArray<T> &rhs)
	{
		if (this != &rhs)
		{
			clear();
			FBTsizeType os = rhs.size();
			if (os > 0)
			{
				resize(os);
				copy(m_data, rhs.m_data, os);
			}
		}

		return *this;
	}

	FBT_INLINE void copy(Pointer dst, ConstPointer src, FBTsizeType size)
	{
		FBT_ASSERT(size <= m_size);
		for (FBTsizeType i = 0; i < size; i++) dst[i] = src[i];
	}

	FBT_INLINE bool equal(const fbtArray<T> &rhs)
	{
		if (rhs.size() != size()) return false;
		if (empty()) return true;
		return fbtMemcmp(m_data, rhs.m_data, sizeof(T)*m_size) == 0;
	}

    // Not too sure about this
    FBT_INLINE void swap(fbtArray<T> &rhs)  {
        FBTsizeType rhs_size = rhs.m_size; rhs.m_size = m_size; m_size = rhs_size;
        FBTsizeType rhs_cap = rhs.m_capacity; rhs.m_capacity = m_capacity; m_capacity = rhs_cap;
        int rhs_cache = rhs.m_cache; rhs.m_cache = m_cache; m_cache = rhs_cache;    // Waht's m_cache  ?
        Pointer rhs_data = rhs.m_data; rhs.m_data = m_data; m_data = rhs_data;
    }

protected:

	void _sort(bool (*cmp)(const T& a, const T& b), int lo, int hi)
	{
		// btAlignedObjectArray.h

		int i = lo, j = hi;
		T x = m_data[(lo+hi)/2];

		//  partition
		do
		{
			while (cmp(m_data[i], x))
				i++;
			while (cmp(x, m_data[j]))
				j--;
			if (i <= j)
			{
				swap(i, j);
				i++; j--;
			}
		}
		while (i <= j);

		//  recursion
		if (lo < j)
			_sort( cmp, lo, j);
		if (i < hi)
			_sort( cmp, i, hi);
	}


	void swap(FBTsizeType a, FBTsizeType b)
	{
		ValueType t = m_data[a];
		m_data[a] = m_data[b];
		m_data[b] = t;
	}

	FBTsizeType     m_size;
	FBTsizeType     m_capacity;
	Pointer         m_data;
	int             m_cache;
};



template <typename T>
class fbtHashTableIterator
{
public:

	typedef typename T::Pointer        Iterator;
	typedef typename T::Entry&          Pair;
	typedef typename T::ConstEntry&     ConstPair;

	typedef typename T::ReferenceKeyType         KeyType;
	typedef typename T::ReferenceValueType       ValueType;
	typedef typename T::ConstReferenceKeyType    ConstKeyType;
	typedef typename T::ConstReferenceValueType  ConstValueType;

protected:

	mutable Iterator m_iterator;
	mutable FBTsizeType   m_cur;
	const FBTsizeType     m_capacity;


public:
	fbtHashTableIterator() : m_iterator(0), m_cur(0), m_capacity(0)  {}
	fbtHashTableIterator(Iterator begin, FBTsizeType size) : m_iterator(begin), m_cur(0), m_capacity(size) { }
	fbtHashTableIterator(T& v) : m_iterator(v.ptr()), m_cur(0), m_capacity(v.size()) {}

	~fbtHashTableIterator() {}

	FBT_INLINE bool      hasMoreElements(void) const  { return (m_iterator && m_cur < m_capacity); }
	FBT_INLINE Pair      getNext(void)                { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur++];}
	FBT_INLINE ConstPair getNext(void) const          { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur++];}
	FBT_INLINE void      next(void) const             { FBT_ASSERT(hasMoreElements()); ++m_cur; }


	FBT_INLINE Pair      peekNext(void)               { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur]; }
	FBT_INLINE KeyType   peekNextKey(void)            { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur].first;}
	FBT_INLINE ValueType peekNextValue(void)          { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur].second; }

	FBT_INLINE ConstPair      peekNext(void)  const     { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur]; }
	FBT_INLINE ConstKeyType   peekNextKey(void) const   { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur].first;}
	FBT_INLINE ConstValueType peekNextValue(void) const { FBT_ASSERT(hasMoreElements()); return m_iterator[m_cur].second; }
};



// magic numbers from http://www.isthe.com/chongo/tech/comp/fnv/
#define _FBT_INITIAL_FNV  0x9E3779B1
#define _FBT_INITIAL_FNV2 0x9E3779B9
#define _FBT_MULTIPLE_FNV 0x1000193
#define _FBT_TWHASH(key)         \
        key += ~(key << 15);    \
        key ^=  (key >> 10);    \
        key +=  (key << 3);     \
        key ^=  (key >> 6);     \
        key += ~(key << 11);    \
        key ^=  (key >> 16);


class fbtCharHashKey
{
protected:
	char* m_key;
	mutable FBThash m_hash;

public:
	fbtCharHashKey() : m_key(0), m_hash(FBT_NPOS) {}
	fbtCharHashKey(char* k) : m_key(k), m_hash(FBT_NPOS) {hash();}
	fbtCharHashKey(const char* k) : m_key(const_cast<char*>(k)), m_hash(FBT_NPOS) {}
	fbtCharHashKey(const fbtCharHashKey& k) : m_key(k.m_key), m_hash(k.m_hash) { if (m_hash == FBT_NPOS) hash(); }


	FBThash hash(void) const
	{
		if (!m_key) return FBT_NPOS;
		if (m_hash != FBT_NPOS) return m_hash;

		// Fowler / Noll / Vo (FNV) Hash
		m_hash = (FBThash)_FBT_INITIAL_FNV;
		for (int i = 0; m_key[i]; i++)
		{
			m_hash = m_hash ^ (m_key[i]);   // xor  the low 8 bits
			m_hash = m_hash * _FBT_MULTIPLE_FNV;  // multiply by the magic number
		}
		return m_hash;
	}

	FBT_INLINE bool operator== (const fbtCharHashKey& v) const    {return hash() == v.hash();}
	FBT_INLINE bool operator!= (const fbtCharHashKey& v) const    {return hash() != v.hash();}
	FBT_INLINE bool operator== (const FBThash& v) const           {return hash() == v;}
	FBT_INLINE bool operator!= (const FBThash& v) const           {return hash() != v;}
};

class fbtIntHashKey
{
protected:
	FBTint32 m_key;
public:
	fbtIntHashKey() : m_key(0) {}
	fbtIntHashKey(FBTint32 k) : m_key(k) {}
	fbtIntHashKey(const fbtIntHashKey& k) : m_key(k.m_key) { }

	FBT_INLINE FBThash hash(void) const  { return static_cast<FBThash>(m_key) * _FBT_INITIAL_FNV; }

	FBT_INLINE bool operator== (const fbtIntHashKey& v) const {return hash() == v.hash();}
	FBT_INLINE bool operator!= (const fbtIntHashKey& v) const {return hash() != v.hash();}
	FBT_INLINE bool operator== (const FBThash& v) const       {return hash() == v;}
	FBT_INLINE bool operator!= (const FBThash& v) const       {return hash() != v;}
};



//For handing invalid pointers
class fbtSizeHashKey
{
protected:

	FBTsize m_key;
	mutable FBThash m_hash;

public:
	fbtSizeHashKey()
        : m_key(0), m_hash(FBT_NPOS)
	{
	}


	fbtSizeHashKey(const FBTsize& k)
        :   m_key(k), m_hash(FBT_NPOS)
	{
		hash();
	}


	fbtSizeHashKey(const fbtSizeHashKey& k)
        :   m_key(k.m_key), m_hash(FBT_NPOS)
	{
		hash();
	}


	FBT_INLINE FBThash hash(void) const
	{
		if (m_hash != FBT_NPOS)
			return m_hash;

		m_hash = (FBThash)m_key;
		_FBT_TWHASH(m_hash);
		return m_hash;
	}

	FBT_INLINE bool operator== (const fbtSizeHashKey& v) const  { return hash() == v.hash();}
	FBT_INLINE bool operator!= (const fbtSizeHashKey& v) const  { return hash() != v.hash();}
	FBT_INLINE bool operator== (const FBThash& v) const         { return hash() == v;}
	FBT_INLINE bool operator!= (const FBThash& v) const         { return hash() != v;}
};


template<typename T>
class fbtTHashKey
{
protected:
	T* m_key;
	mutable FBThash m_hash;

public:
	fbtTHashKey() : m_key(0), m_hash(FBT_NPOS) { hash(); }
	fbtTHashKey(T* k) : m_key(k), m_hash(FBT_NPOS) { hash(); }
	fbtTHashKey(const fbtTHashKey& k) : m_key(k.m_key), m_hash(k.m_hash) { if (m_hash == FBT_NPOS) hash(); }

	FBT_INLINE T*          key(void)       {return m_key;}
	FBT_INLINE const T*    key(void) const {return m_key;}


	FBT_INLINE FBThash hash(void) const
	{
		if (m_hash != FBT_NPOS)
			return m_hash;

		// Yields the least collisions.
		m_hash = static_cast<FBThash>(reinterpret_cast<FBTuintPtr>(m_key));
		_FBT_TWHASH(m_hash);
		return m_hash;
	}


	FBT_INLINE bool operator== (const fbtTHashKey& v) const  { return hash() == v.hash();}
	FBT_INLINE bool operator!= (const fbtTHashKey& v) const  { return hash() != v.hash();}
	FBT_INLINE bool operator== (const FBThash& v) const      { return hash() == v;}
	FBT_INLINE bool operator!= (const FBThash& v) const      { return hash() != v;}
};

typedef fbtTHashKey<void> fbtPointerHashKey;


template<typename Key, typename Value>
struct fbtHashEntry
{
	Key    first;
	Value  second;

	fbtHashEntry() {}
	fbtHashEntry(const Key& k, const Value& v) : first(k), second(v) {}

	FBT_INLINE bool operator==(const fbtHashEntry& rhs) const
	{
		return first == rhs.first && second == rhs.second;
	}
};

#define _FBT_UTHASHTABLE_HASH(key)      ((key.hash() & (m_capacity - 1)))
#define _FBT_UTHASHTABLE_HKHASH(key)    ((hk & (m_capacity - 1)))
#define _FBT_UTHASHTABLE_FORCE_POW2     1
#define _FBT_UTHASHTABLE_INIT           32
#define _FBT_UTHASHTABLE_EXPANSE  (m_size * 2)


#define _FBT_UTHASHTABLE_STAT       FBT_HASHTABLE_STAT
#define _FBT_UTHASHTABLE_STAT_ALLOC 0


#if _FBT_UTHASHTABLE_FORCE_POW2 == 1
#define _FBT_UTHASHTABLE_POW2(x) \
    --x; x |= x >> 16; x |= x >> 8; x |= x >> 4; \
    x |= x >> 2; x |= x >> 1; ++x;

#define _FBT_UTHASHTABLE_IS_POW2(x) (x && !((x-1) & x))
#endif


#if _FBT_UTHASHTABLE_STAT == 1
#include <typeinfo>
#endif



template < typename Key, typename Value >
class fbtHashTable
{
public:
	typedef fbtHashEntry<Key, Value>        Entry;
	typedef const fbtHashEntry<Key, Value>  ConstEntry;

	typedef Entry*  EntryArray;
	typedef FBTsizeType* IndexArray;


	typedef Key            KeyType;
	typedef Value          ValueType;

	typedef const Key      ConstKeyType;
	typedef const Value    ConstValueType;

	typedef Value&          ReferenceValueType;
	typedef const Value&    ConstReferenceValueType;

	typedef Key&            ReferenceKeyType;
	typedef const Key&      ConstReferenceKeyType;

	typedef EntryArray Pointer;
	typedef const Entry* ConstPointer;


	typedef fbtHashTableIterator<fbtHashTable<Key, Value> > Iterator;
	typedef const fbtHashTableIterator<fbtHashTable<Key, Value> > ConstIterator;

    //typedef Iterator            iterator;
    //typedef ConstIterator       const_iterator;


public:

	fbtHashTable()
        :    m_size(0), m_capacity(0), m_lastPos(FBT_NPOS), m_lastKey(FBT_NPOS),
		     m_iptr(0), m_nptr(0), m_bptr(0), m_cache(0)
	{
	}

	fbtHashTable(FBTsizeType capacity)
        :    m_size(0), m_capacity(0), m_lastPos(FBT_NPOS), m_lastKey(FBT_NPOS),
		     m_iptr(0), m_nptr(0), m_bptr(0), m_cache(0)
	{
	}

	fbtHashTable(const fbtHashTable& rhs)
        :    m_size(0), m_capacity(0), m_lastPos(FBT_NPOS), m_lastKey(FBT_NPOS),
		     m_iptr(0), m_nptr(0), m_bptr(0), m_cache(0)
	{
		doCopy(rhs);
	}

	~fbtHashTable() { clear(); }

	fbtHashTable<Key, Value> &operator = (const fbtHashTable<Key, Value> &rhs)
	{
		if (this != &rhs)
			doCopy(rhs);
		return *this;
	}

	void clear(bool useCache = false)
	{
		if (!useCache)
		{
			m_size = m_capacity = 0;
			m_lastKey = FBT_NPOS;
			m_lastPos = FBT_NPOS;
			m_cache = 0;

			delete [] m_bptr;
			delete [] m_iptr;
			delete [] m_nptr;
			m_bptr = 0; m_iptr = 0; m_nptr = 0;
		}
		else
		{
			++m_cache;
			if (m_cache > _FBT_CACHE_LIMIT)
				clear(false);
			else
			{
				m_size = 0;
				m_lastKey = FBT_NPOS;
				m_lastPos = FBT_NPOS;


				FBTsizeType i;
				for (i = 0; i < m_capacity; ++i)
				{
					m_iptr[i] = FBT_NPOS;
					m_nptr[i] = FBT_NPOS;
				}
			}
		}

	}
	Value&              at(FBTsizeType i)                    { FBT_ASSERT(m_bptr && i >= 0 && i < m_size); return m_bptr[i].second; }
	Value&              operator [](FBTsizeType i)           { FBT_ASSERT(m_bptr && i >= 0 && i < m_size); return m_bptr[i].second; }
	const Value&        at(FBTsizeType i)const               { FBT_ASSERT(m_bptr && i >= 0 && i < m_size); return m_bptr[i].second; }
	const Value&        operator [](FBTsizeType i) const     { FBT_ASSERT(m_bptr && i >= 0 && i < m_size); return m_bptr[i].second; }
	Key&                keyAt(FBTsizeType i)                 { FBT_ASSERT(m_bptr && i >= 0 && i < m_size); return m_bptr[i].first; }
	const Key&          keyAt(FBTsizeType i)const            { FBT_ASSERT(m_bptr && i >= 0 && i < m_size); return m_bptr[i].first; }

	Value* get(const Key& key) const
	{
		if (!m_bptr || m_size == 0)
			return (Value*)0;


		FBThash hr = key.hash();

		if (m_lastKey != hr)
		{
			FBTsizeType i = find(key);
			if (i == FBT_NPOS) return (Value*)0;


			FBT_ASSERT(i >= 0 && i < m_size);

			m_lastKey = hr;
			m_lastPos = i;
		}

		return &m_bptr[m_lastPos].second;
	}


	Value*         operator [](const Key& key)       { return get(key); }
	const Value*   operator [](const Key& key) const { return get(key); }

	FBTsizeType find(const Key& key) const
	{
		if (m_capacity == 0 || m_capacity == FBT_NPOS || m_size == 0)
			return FBT_NPOS;

		FBTsizeType hk = key.hash();
		if (m_lastPos != FBT_NPOS && m_lastKey == hk)
			return m_lastPos;

		FBThash hr = _FBT_UTHASHTABLE_HKHASH(hk);

		FBT_ASSERT(m_bptr && m_iptr && m_nptr);

		FBTsizeType fh = m_iptr[hr];
		while (fh != FBT_NPOS && (key != m_bptr[fh].first))
			fh = m_nptr[fh];


		if (fh != FBT_NPOS)
		{
			m_lastKey = hk;
			m_lastPos = fh;

			FBT_ASSERT(fh >= 0  && fh < m_size);
		}
		return fh;
	}



	void erase(const Key& key) {remove(key);}

	void remove(const Key& key)
	{
		FBThash hash, lhash;
		FBTsizeType index, pindex, findex;

		findex = find(key);
		if (findex == FBT_NPOS || m_capacity == 0 || m_size == 0)
			return;

		m_lastKey = FBT_NPOS;
		m_lastPos = FBT_NPOS;
		FBT_ASSERT(m_bptr && m_iptr && m_nptr);

		hash = _FBT_UTHASHTABLE_HASH(key);

		index  = m_iptr[hash];
		pindex = FBT_NPOS;
		while (index != findex)
		{
			pindex = index;
			index = m_nptr[index];
		}

		if (pindex != FBT_NPOS)
		{
			FBT_ASSERT(m_nptr[pindex] == findex);
			m_nptr[pindex] = m_nptr[findex];
		}
		else
			m_iptr[hash] = m_nptr[findex];

		FBTsizeType lindex = m_size - 1;
		if (lindex == findex)
		{
			--m_size;
			m_bptr[m_size].~Entry();
			return;
		}

		lhash = _FBT_UTHASHTABLE_HASH(m_bptr[lindex].first);
		index  = m_iptr[lhash];
		pindex = FBT_NPOS;
		while (index != lindex)
		{
			pindex = index;
			index = m_nptr[index];
		}

		if (pindex != FBT_NPOS)
		{
			FBT_ASSERT(m_nptr[pindex] == lindex);
			m_nptr[pindex] = m_nptr[lindex];
		}
		else
			m_iptr[lhash] = m_nptr[lindex];

		m_bptr[findex] = m_bptr[lindex];
		m_nptr[findex] = m_iptr[lhash];
		m_iptr[lhash] = findex;

		--m_size;
		m_bptr[m_size].~Entry();
		return;
	}

	bool insert(const Key& key, const Value& val)
	{
		if (find(key) != FBT_NPOS)
			return false;

		if (m_size == m_capacity)
			reserve(m_size == 0 ? _FBT_UTHASHTABLE_INIT : _FBT_UTHASHTABLE_EXPANSE);

		const FBThash hr = _FBT_UTHASHTABLE_HASH(key);

		FBT_ASSERT(m_bptr && m_iptr && m_nptr);
		m_bptr[m_size] = Entry(key, val);
		m_nptr[m_size] = m_iptr[hr];
		m_iptr[hr] = m_size;
		++m_size;
		return true;
	}

	FBT_INLINE Pointer ptr(void)                { return m_bptr; }
	FBT_INLINE ConstPointer ptr(void) const     { return m_bptr; }
	FBT_INLINE bool valid(void) const           { return m_bptr != 0;}


	FBT_INLINE FBTsizeType size(void) const         { return m_size; }
	FBT_INLINE FBTsizeType capacity(void) const     { return capacity; }
	FBT_INLINE bool empty(void) const               { return m_size == 0; }


    Iterator        iterator(void)       { return m_bptr && m_size > 0 ? Iterator(m_bptr, m_size) : Iterator(); }
    ConstIterator   iterator(void) const { return m_bptr && m_size > 0 ? ConstIterator(m_bptr, m_size) : ConstIterator(); }

    Iterator        begin(void)       { return m_bptr && m_size > 0 ? Iterator(m_bptr, m_size) : Iterator(); }
    ConstIterator   begin(void) const { return m_bptr && m_size > 0 ? ConstIterator(m_bptr, m_size) : ConstIterator(); }


	void reserve(FBTsizeType nr)
	{
		if (m_capacity < nr && nr != FBT_NPOS)
			rehash(nr);
	}

#if _FBT_UTHASHTABLE_STAT == 1

	void report(void) const
	{
		if (m_capacity == 0 || m_capacity == FBT_NPOS || m_size == 0)
			return;

		FBT_ASSERT(m_bptr && m_iptr && m_nptr);


		FBTsizeType min_col = m_size, max_col = 0;
		FBTsizeType i, tot = 0, avg = 0;
		for (i = 0; i < m_size; ++i)
		{
			Key& key = m_bptr[i].first;

			FBThash hr = _FBT_UTHASHTABLE_HASH(key);

			FBTsizeType nr = 0;

			FBTsizeType fh = m_iptr[hr];
			while (fh != FBT_NPOS && (key != m_bptr[fh].first))
			{
				fh = m_nptr[fh];
				nr++;
			}

			if (nr < min_col)
				min_col = nr;
			if (nr > max_col)
				max_col = nr;

			tot += nr;
			avg += nr ? 1 : 0;
		}

#if _FBT_UTHASHTABLE_FORCE_POW2 == 1
		fbtPrintf("Results using forced power of 2 expansion.\n\n");
#else
		fbtPrintf("Results using unaltered expansion.\n\n");
#endif
		fbtPrintf("\tTotal number of collisions %i for a table of size %i.\n\t\tusing (%s)\n", tot, m_size, typeid(Key).name());
		fbtPrintf("\tThe minimum number of collisions per key: %i\n", min_col);
		fbtPrintf("\tThe maximum number of collisions per key: %i\n", max_col);

		int favr = (int)(100.f * ((float)avg / (float)m_size));
		fbtPrintf("\tThe average number of key collisions: %i\n\n", favr);

		if (tot == 0)
			fbtPrintf("\nCongratulations lookup is 100%% linear!\n\n");
		else if (favr >  35)
			fbtPrintf("\nImprove your hash function!\n\n");
	}
#endif



private:

	void doCopy(const fbtHashTable<Key, Value> &rhs)
	{
		if (rhs.valid() && !rhs.empty())
		{
			reserve(rhs.m_capacity);

			FBTsizeType i, b;
			m_size     = rhs.m_size;
			m_capacity = rhs.m_capacity;

			b = m_size > 0 ? m_size - 1 : 0;
			for (i = b; i < m_capacity; ++i) m_nptr[i] = m_iptr[i] = FBT_NPOS;

			for (i = 0; i < m_size; ++i)
			{
				m_bptr[i] = rhs.m_bptr[i];
				m_iptr[i] = rhs.m_iptr[i];
				m_nptr[i] = rhs.m_nptr[i];
			}
		}

	}

	template<typename ArrayType>
	void reserveType(ArrayType** old, FBTsizeType nr, bool cpy = false)
	{
		FBTsizeType i;
		ArrayType* nar = new ArrayType[nr];
		if ((*old) != 0)
		{
			if (cpy)
			{
				const ArrayType* oar = (*old);
				for (i = 0; i < m_size; i++) nar[i] = oar[i];
			}
			delete [](*old);
		}
		(*old) = nar;
	}


	void rehash(FBTsizeType nr)
	{
#if _FBT_UTHASHTABLE_FORCE_POW2

		if (!_FBT_UTHASHTABLE_IS_POW2(nr))
		{
			_FBT_UTHASHTABLE_POW2(nr);
		}

#if _FBT_UTHASHTABLE_STAT_ALLOC == 1
		fbtPrintf("Expanding tables: %i\n", nr);
#endif
		FBT_ASSERT(_FBT_UTHASHTABLE_IS_POW2(nr));


#else

#if _FBT_UTHASHTABLE_STAT_ALLOC == 1
		fbtPrintf("Expanding tables: %i\n", nr);
#endif

#endif

		reserveType<Entry>(&m_bptr, nr, true);
		reserveType<FBTsizeType>(&m_iptr, nr);
		reserveType<FBTsizeType>(&m_nptr, nr);

		m_capacity = nr;
		FBT_ASSERT(m_bptr && m_iptr && m_nptr);


		FBTsizeType i, h;
		for (i = 0; i < m_capacity; ++i) { m_iptr[i] = m_nptr[i] = FBT_NPOS; }
		for (i = 0; i < m_size; i++)     { h = _FBT_UTHASHTABLE_HASH(m_bptr[i].first); m_nptr[i] = m_iptr[h]; m_iptr[h] = i;}
	}



	FBTsizeType m_size, m_capacity;
	mutable FBTsizeType m_lastPos;
	mutable FBTsizeType m_lastKey;

	IndexArray m_iptr;
	IndexArray m_nptr;
	EntryArray m_bptr;
	FBTsizeType m_cache;
};




#define fbtCharNEq(a, b, n)  ((a && b) && !strncmp(a, b, n))
#define fbtCharEq(a, b)      ((a && b) && (*a == *b) && !strcmp(a, b))
#define fbtCharEqL(a, b, l)  ((a && b) && (*a == *b) && !memcmp(a, b, l))

FBT_INLINE int fbtStrLen(const char* cp)
{
	int i = 0;
    while (cp[i]) ++i;//cp[i++];
	return i;
}


// For operations on a fixed size character array
template <const FBTuint16 L>
class fbtFixedString
{
public:
	typedef char Pointer[L+1];


public:

	fbtFixedString()
		: m_size(0), m_hash(FBT_NPOS)
	{
		m_buffer[m_size] = 0;
	}

	fbtFixedString(const fbtFixedString& rhs)
		:   m_size(0), m_hash(FBT_NPOS)
	{
		if (rhs.size())
		{
			FBTuint16 i, os = rhs.size();
			const char* cp = rhs.c_str();

			for (i = 0; i < L && i < os; ++i, ++m_size) m_buffer[i] = cp[i];
		}
		m_buffer[m_size] = 0;
	}


	fbtFixedString(const char* rhs)
		:   m_size(0), m_hash(FBT_NPOS)
	{
		if (rhs)
		{
			FBTuint16 i;
			for (i = 0; i < L && rhs[i]; ++i, ++m_size) m_buffer[i] = rhs[i];
		}
		m_buffer[m_size] = 0;
	}



	FBT_INLINE void push_back(char ch)
	{
		if (m_size >= L)
			return;
		m_buffer[m_size++] = ch;
		m_buffer[m_size] = 0;
	}

	void append(const char* str)
	{
		int len = fbtStrLen(str);
		int a = 0;
		while (a < len)
			push_back(str[a++]);
	}


	void append(const fbtFixedString& str)
	{
		int len = str.m_size;
		int a = 0;
		while (a < len)
			push_back(str.m_buffer[a++]);
	}


	fbtFixedString operator +(const fbtFixedString& rhs)
	{
		fbtFixedString lhs = *this;
		lhs.append(rhs);
		return lhs;
	}


	fbtFixedString operator +=(const fbtFixedString& rhs)
	{
		append(rhs);
		return *this;
	}

	fbtFixedString operator +(const char* str)
	{
		fbtFixedString lhs = *this;
		lhs.append(str);
		return lhs;
	}

	fbtFixedString operator +=(const char* str)
	{
		append(str);
		return *this;
	}


	void split(fbtArray<fbtFixedString<L> >& dest, char c, char e = '\0') const
	{
		FBTuint16 i, p = 0, t;
		for (i = 0; i < L && i < m_size; ++i)
		{
			if (m_buffer[i] == c || m_buffer[i] == e)
			{
				fbtFixedString<L> cpy;
				for (t = p; t < i; ++t) cpy.push_back(m_buffer[t]);
				dest.push_back(cpy);
				p = i + 1;
			}
		}

		if (p != i)
		{
			fbtFixedString<L> cpy;
			for (t = p; t < i; ++t) cpy.push_back(m_buffer[t]);
			dest.push_back(cpy);
		}
	}


	void resize(FBTuint16 ns)
	{
		if (ns <= L)
		{
			if (ns < m_size)
				for (FBTuint16 i = ns; i < m_size; i++) m_buffer[i] = 0;
			else
				for (FBTuint16 i = m_size; i < ns; i++) m_buffer[i] = 0;
			m_size = ns;
			m_buffer[m_size] = 0;
		}
	}



	template<const FBTuint16 OL>
	fbtFixedString<L>& operator = (const fbtFixedString<OL>& o)
	{
		if (o.m_size > 0)
		{
			if (m_hash == FBT_NPOS || m_hash != o.m_hash)
			{
				FBTuint16 i;
				m_size = 0;
				m_hash = o.m_hash;
				for (i = 0; (i < L && i < OL) && i < o.m_size; ++i, ++m_size) m_buffer[i] = o.m_buffer[i];
				m_buffer[m_size] = 0;
			}
		}
		return *this;
	}

	FBT_INLINE const char* c_str(void) const                 { return m_buffer; }
	FBT_INLINE char* ptr(void)                               { return m_buffer; }
	FBT_INLINE const char* ptr(void) const                   { return m_buffer; }
    FBT_INLINE char operator [](FBTuint16 i) const           { FBT_ASSERT(i < L); return m_buffer[i]; }
    FBT_INLINE char at(FBTuint16 i) const                    { FBT_ASSERT(i < L); return m_buffer[i]; }
	FBT_INLINE void clear(void)                              { m_buffer[0] = 0; m_size = 0; }
	FBT_INLINE int empty(void) const                         { return m_size == 0; }
	FBT_INLINE int size(void) const                          { return m_size; }
	FBT_INLINE int capacity(void) const                      { return L; }



	FBTsize hash(void) const
	{
		if (m_hash != FBT_NPOS)
			return m_hash;

		if (m_size == 0/* || !m_buffer*/)	// Warning: address of array 'this->m_buffer' will always evaluate to 'true'
			return FBT_NPOS;
		fbtCharHashKey chk(m_buffer);
		m_hash = chk.hash();
		return m_hash;
	}

	FBT_INLINE bool operator == (const fbtFixedString& str) const { return hash() == str.hash(); }
	FBT_INLINE bool operator != (const fbtFixedString& str) const { return !(this->operator ==(str));}

protected:

	Pointer             m_buffer;
	FBTuint16           m_size;
	mutable FBThash     m_hash;
};


// For operations with variable size character array
class fbtString : public fbtArray<char> {
typedef fbtArray<char> base;

public:
FBT_INLINE fbtString(int reservedChars=-1) : base() {if (reservedChars>0) base::reserve(reservedChars+1);base::resize(1);operator[](0)='\0';}
FBT_INLINE fbtString(const fbtString& other) : base() {base::resize(1);operator[](0)='\0';*this = other;}
FBT_INLINE fbtString(const char* str) : base() {
    base::resize(1);operator[](0)='\0';
    *this = str;
}
FBT_INLINE int size() const {
    if (base::size()<1) return 0;
    return base::size()-1;
}
FBT_INLINE int length() const {
    return size();
}
FBT_INLINE bool empty() const {
    return size()==0;
}

FBT_INLINE const char* c_str() const {
    return (internalSize()>0 ? &operator[](0) : NULL);
}
FBT_INLINE int compare(const fbtString& other) const  {
    return (empty() ? -1 : other.empty() ? 1 : strcmp(c_str(),other.c_str()));
}
FBT_INLINE bool operator==(const fbtString& other) const {
    if (internalSize()!=other.internalSize()) return false;
    //for (int i=0,sz=internalSize();i<sz;i++) {if (operator[](i)!=other[i]) return false;}
    //return true;
    return (compare(other)==0);
}
FBT_INLINE bool operator!=(const fbtString& other) const {
    return !operator==(other);
}
FBT_INLINE bool operator<(const fbtString& other) const  {
    return compare(other)<0;
}

FBT_INLINE const fbtString& operator=(const char* other) {
    //printf("operator=(const char* other) START: \"%s\" \"%s\"",this->c_str(),other);
    if (!other) return *this;
    const int len = strlen(other);
    base::resize(len+1);
    for (int i=0;i<len;i++) operator[](i) = other[i];   // strcpy ?
    operator[](len) = '\0';
    //printf("operator=(const char* other) END: \"%s\"\n",this->c_str());
    return *this;
}
FBT_INLINE const fbtString& operator=(const fbtString& other) {
    //printf("operator=(const fbtString& other) START: \"%s\" \"%s\"",this->c_str(),other.c_str());
    //base::operator=(other); // Warning: fbtArray has NO DEFAULT ASSIGNMENT ATM
    // -----------------------
    if (other.size()==0) *this="";
    else operator=(other.c_str());
    // -----------------------
    //printf("operator=(const fbtString& other) END: \"%s\"\n",this->c_str());
    return *this;
}

FBT_INLINE const fbtString& operator+=(const fbtString& other) {
    const int curSize = size();
    if (curSize==0) return operator=(other);
    const int len = other.size();
    base::resize(curSize + len+1);
    for (int i=curSize;i<curSize+len;i++) operator[](i) = other[i-curSize];
    operator[](curSize+len) = '\0';
    return *this;

}
FBT_INLINE const fbtString& operator+=(const char* other) {
    if (!other) return *this;
    const int curSize = size();
    if (curSize==0) return operator=(other);
    const int len = strlen(other);
    base::resize(curSize + len+1);
    for (int i=curSize;i<curSize+len;i++) operator[](i) = other[i-curSize];
    operator[](curSize+len) = '\0';
    return *this;
}
FBT_INLINE const fbtString& operator+=(const char c) {
    const int curSize = internalSize();
    if (curSize==0) {
        resize(2);
        operator[](0) = c;
        operator[](1) = '\0';
    }
    else    {
        base::resize(curSize + 1);
        operator[](curSize-1) = c;
        operator[](curSize) = '\0';
    }
    return *this;
}

static const int npos = -1;


FBT_INLINE int find(const char c,int beg = 0) const  {
    for (int i=beg,sz = size();i<sz;i++)    {
        if (operator[](i) == c) return i;
    }
    return npos;
}
FBT_INLINE int find_first_of(const char c,int beg = 0) const {
    return find(c,beg);
}
FBT_INLINE int find_last_of(const char c,int beg = 0) const  {
    for (int i=size()-1;i>=beg;i--)    {
        if (operator[](i) == c) return i;
    }
    return npos;
}
FBT_INLINE int find(const fbtString& s,int beg = 0) const  {
    int i,j,sz;
    const int ssize = s.size();
    if (ssize == 0 || beg+ssize>=size()) return -1;
    for (i=beg,sz = size()-beg;i<sz;i++)    {
        for (j=0;j<ssize;j++)   {
            if (operator[](i+j) != s.operator [](j)) break;
            if (j==ssize-1) return i;
        }
    }
    return npos;
}
FBT_INLINE int find_first_of(const fbtString& s,int beg = 0) const {
    return find(s,beg);
}
// not tested:
FBT_INLINE int find_last_of(const fbtString& s,int beg = 0) const  {
    int i,j;
    const int ssize = s.size();
    if (ssize == 0 || beg+ssize>=size()) return -1;
    for (i=size()-ssize-1;i>=beg;i--)    {
        for (j=0;j<ssize;j++)   {
            if (operator[](i+j) != s.operator [](j)) break;
            if (j==ssize-1) return i;
        }
    }
    return npos;
}

FBT_INLINE const fbtString substr(int beg,int cnt=-1) const {
    const int sz = size();
    if (beg>=sz) return fbtString("");
    if (cnt==-1) cnt = sz - beg;
    fbtString rv;rv.resize(cnt+1);
    for (int i=0;i<cnt;i++) {
        rv.operator [](i) = this->operator [](beg+i);
    }
    rv.operator [](cnt) = '\0';
    return rv;
}

protected:

private:
FBT_INLINE int internalSize() const {
    return base::size();
}
FBT_INLINE void reserve(int i) {
    return base::reserve(i);
}
FBT_INLINE void resize(int i) {base::resize(i);}
FBT_INLINE void clear() {base::clear();}
FBT_INLINE int findLinearSearch(const char c) {
    for (int i=0,sz=size();i<sz;i++)    {
        if (c == operator[](i)) return i;
    }
    return npos;
}
FBT_INLINE void push_back(const char c) {
    return base::push_back(c);
}

friend void fbtStringNonStdResize(fbtString& s,int size);

//TODO: redefine all the other methods we want to hide here...
};
FBT_INLINE const fbtString operator+(fbtString v1, const fbtString& v2 ) {return v1+=(v2);}



enum FBT_PRIM_TYPE
{
	FBT_PRIM_CHAR,		// 0
	FBT_PRIM_UCHAR,		// 1
	FBT_PRIM_SHORT,		// 2
	FBT_PRIM_USHORT,	// 3
	FBT_PRIM_INT,		// 4
	FBT_PRIM_LONG,		// 5
	FBT_PRIM_ULONG,		// 6
	FBT_PRIM_FLOAT,		// 7
	FBT_PRIM_DOUBLE,	// 8
	FBT_PRIM_VOID,		// 9
	FBT_PRIM_UNKNOWN	// 10
};

FBT_PRIM_TYPE fbtGetPrimType(FBTuint32 typeKey);
FBT_INLINE FBT_PRIM_TYPE fbtGetPrimType(const char* typeName)
{
	return fbtGetPrimType(fbtCharHashKey(typeName).hash());
}
FBT_INLINE bool fbtIsIntType(FBTuint32 typeKey)
{
	FBT_PRIM_TYPE tp = fbtGetPrimType(typeKey);
	return tp < FBT_PRIM_FLOAT;
}
FBT_INLINE bool fbtIsFloatType(FBTuint32 typeKey)
{
	FBT_PRIM_TYPE tp = fbtGetPrimType(typeKey);
	return tp == FBT_PRIM_FLOAT || tp == FBT_PRIM_DOUBLE;
}
FBT_INLINE bool fbtIsNumberType(FBTuint32 typeKey)
{
	FBT_PRIM_TYPE tp = fbtGetPrimType(typeKey);
	return tp != FBT_PRIM_VOID && tp != FBT_PRIM_UNKNOWN;
}



/** @}*/
#endif//_fbtTypes_h_

/** \addtogroup FBT
*  @{
*/

class fbtStream;
class fbtBinTables;

#include <stdio.h> // FILE*, used in the static helper method fbtFile::UTF8_fopen(...)
                   // (unluckily there's no way to forward-declare FILE)

class fbtFile
{
public:

	enum FileMagic
	{
		FM_BIG_ENDIAN       = 'V',
		FM_LITTLE_ENDIAN    = 'v',
		FM_32_BIT           = '_',
		FM_64_BIT           = '-',

		// place custom rules here, note that the
		// full header string must be <= 12 char's
	};


	enum FileStatus
	{
		FS_LINK_FAILED = -7,
		FS_INV_INSERT,
		FS_BAD_ALLOC,
		FS_INV_READ,
		FS_INV_LENGTH,
		FS_INV_HEADER_STR,
		FS_FAILED,
		FS_OK,
	};


	enum ParseMode
	{
		PM_UNCOMPRESSED,
		PM_COMPRESSED,
		PM_READTOMEMORY,

	};

	enum FileHeader
	{
		FH_ENDIAN_SWAP  = (1 << 0),
		FH_CHUNK_64     = (1 << 1),
		FH_VAR_BITS     = (1 << 2),
	};


	struct Chunk32
	{
		FBTuint32       m_code;
		FBTuint32       m_len;
		FBTuint32       m_old;
		FBTuint32       m_typeid;
		FBTuint32       m_nr;
	};

	struct Chunk64
	{
		FBTuint32       m_code;
		FBTuint32       m_len;
		FBTuint64       m_old;
		FBTuint32       m_typeid;
		FBTuint32       m_nr;
	};


	struct Chunk
	{
		FBTuint32       m_code;
		FBTuint32       m_len;
		FBTsize         m_old;
		FBTuint32       m_typeid;
		FBTuint32       m_nr;
	};


	struct MemoryChunk
	{
		enum Flag
		{
			BLK_MODIFIED = (1 << 0),
		};

		MemoryChunk* m_next, *m_prev;
		Chunk        m_chunk;
		void*        m_block;
		void*        m_newBlock;


		FBTuint8     m_flag;
		FBTtype      m_newTypeId;
	};

public:


	fbtFile(const char* uid);
	virtual ~fbtFile();

    int parse(const char* path,const char* uncompressedFileDetectorPrefix="BLENDER");
    int parse(const char* path, int mode/* = PM_UNCOMPRESSED*/);
    int parse(const void* memory, FBTsize sizeInBytes, int mode = PM_UNCOMPRESSED, bool suppressHeaderWarning=false);

	/// Saving in non native endianness is not implemented yet.
	int reflect(const char* path, const int mode = PM_UNCOMPRESSED, const fbtEndian& endian = FBT_ENDIAN_NATIVE);


	const fbtFixedString<12>&   getHeader(void)     const {return m_header;}
	const int&                  getVersion(void)    const {return m_fileVersion;}
	const char*                 getPath(void)       const {return m_curFile; }


	fbtBinTables* getMemoryTable(void)  {return m_memory;}
	fbtBinTables* getFileTable(void)    {return m_file;}


	fbtList& getChunks(void) {return m_chunks;}

    virtual void setIgnoreList(FBTuint32 * /*stripList*/) {}

	bool _setuid(const char* uid);

    // General Programming Staic Helper Methods:
    static FILE* UTF8_fopen(const char* filename, const char* mode);    // Used to allow UTF8 chars on Windows
    static unsigned char* FBT_GetFileContent(const char *filePath,unsigned long* pSizeOut=NULL,const char modes[] = "rb");  // Returns a memory buffer (that user must free using: delete[] mybuffname;) with the content of 'filePath'.


protected:

	int parseMagic(const char* cp);

	void writeStruct(fbtStream* stream, FBTtype index, FBTuint32 code, FBTsize len, void* writeData);
	void writeBuffer(fbtStream* stream, FBTsize len, void* writeData);


	virtual int initializeTables(fbtBinTables* tables) = 0;
    virtual int notifyData(void* /*p*/, const Chunk& /*id*/) {return FS_OK;}
    virtual int writeData(fbtStream* /*stream*/) {return FS_OK;}
	
	virtual void*   getFBT(void) = 0;
	virtual FBTsize getFBTlength(void) = 0;


	// lookup name first 7 of 12
	const char* m_uhid;
	const char* m_aluhid; //alternative header string
	fbtFixedString<12> m_header;


	int m_version, m_fileVersion, m_fileHeader;
	char* m_curFile;

	typedef fbtHashTable<fbtSizeHashKey, MemoryChunk*> ChunkMap;
	fbtList     m_chunks;
	ChunkMap    m_map;
	fbtBinTables* m_memory, *m_file;


    virtual bool skip(const FBTuint32& /*id*/) {return false;}
	void* findPtr(const FBTsize& iptr);
	MemoryChunk* findBlock(const FBTsize& iptr);

    static bool FileStartsWith(const char* path,const char* cmp);    // Used to detect if a .blend file is not compressed

private:


	int parseHeader(fbtStream* stream, bool suppressHeaderWarning=false);
	int parseStreamImpl(fbtStream* stream, bool suppressHeaderWarning=false);

	int compileOffsets(void);
	int link(void);
};

/** @}*/
#endif//_fbtFile_h_




class fbtBlend : public fbtFile
{
public:
	fbtBlend();
	virtual ~fbtBlend();

    // Warning: these fields are NOT updated automatically, but must be MANUALLY integrated (to reflect the content of source/blender/makesdna/DNA_ID_enums.h)
    // It should be OK to use "integrated versions" also for older versions (newer fields are not present in their Blender DNA, and this should be OK). [DNA_ID_enums 1/2]
	fbtList m_scene;
	fbtList m_library;
	fbtList m_object;
	fbtList m_mesh;
	fbtList m_curve;	// In Blender>=3.1 it's obsolated and marked 'legacy'. New .blend files should use m_curves instead (see below)
	fbtList m_mball;    // MataBall
	fbtList m_mat;      // Material
	fbtList m_tex;      // Texture
	fbtList m_image;
	fbtList m_latt;     // Lattice
	fbtList m_lamp;
	fbtList m_camera;
	fbtList m_ipo;      // or FCurves
	fbtList m_key;      // ShapeKey
	fbtList m_world;
	fbtList m_screen;
	fbtList m_script;   // Python   (used in older versions)
	fbtList m_vfont;    // VectorFont
	fbtList m_text;
	fbtList m_speaker;
	fbtList m_sound;
	fbtList m_group;    // Group/Collection
	fbtList m_armature;
	fbtList m_action;
	fbtList m_nodetree;
	fbtList m_brush;
	fbtList m_particle; // ParticleSettings
	fbtList m_gpencil;
	fbtList m_wm;       // WindowManager	
	fbtList m_movieClip;
	fbtList m_mask;
	fbtList m_freeStyleLineStyle;
	fbtList m_palette;
    fbtList m_paintCurve;
	fbtList m_cacheFile;
	fbtList m_workSpace;
	fbtList m_lightProbe;
	fbtList m_hair;		// Removed in Blender 3.1: replaced by m_curves.
	fbtList m_curves;	// Replacement for m_hair (and m_curve) in Blender>=3.1
	fbtList m_pointCloud;
	fbtList m_volume;
	fbtList m_simulation;// GeometryNodeGroups

	Blender::FileGlobal* m_fg;

	int save(const char* path, const int mode = PM_UNCOMPRESSED);
	
	void setIgnoreList(FBTuint32 *stripList) {m_stripList = stripList;}

protected:
	virtual int notifyData(void* p, const Chunk& id);
	virtual int initializeTables(fbtBinTables* tables);
	virtual bool skip(const FBTuint32& id);
	virtual int writeData(fbtStream* stream);

	FBTuint32* m_stripList;

	virtual void*   getFBT(void);
	virtual FBTsize getFBTlength(void);
};


#endif//_fbtBlend_h_




#ifdef FBTBLEND_IMPLEMENTATION
#ifndef bftBlend_imp_
#define bftBlend_imp_

//#include "fbtBlend.h"
//#include "fbtTables.h"
//#include "fbtStreams.h"
#ifndef _fbtFile_imp_
#define _fbtFile_imp_

#define FBT_IN_SOURCE

//#include "fbtFile.h"
//#include "fbtStreams.h"
#ifndef _fbtStreams_h_
#define _fbtStreams_h_

//#include "fbtTypes.h"
/** \addtogroup FBT
*  @{
*/

class fbtStream
{
public:
	enum StreamMode
	{
		SM_READ = 1,
		SM_WRITE = 2,
	};

public:
	fbtStream() {}
	virtual ~fbtStream() {}

    virtual void open(const char* /*path*/, fbtStream::StreamMode /*mode*/) {}
	virtual void clear(void) {};


	virtual bool isOpen(void) const = 0;
	virtual bool eof(void) const = 0;

	virtual FBTsize  read(void* dest, FBTsize nr) const = 0;
	virtual FBTsize  write(const void* src, FBTsize nr) = 0;
    virtual FBTsize  writef(const char* /*fmt*/, ...) {return 0;};

	virtual FBTsize  position(void) const = 0;
	virtual FBTsize  size(void) const = 0;

    virtual FBTsize seek(FBTint32 /*off*/, FBTint32 /*way*/) {return 0;}

protected:
    virtual void reserve(FBTsize /*nr*/) {}
};



class           fbtMemoryStream;
typedef void*   fbtFileHandle;


class fbtFileStream : public fbtStream
{
public:
	fbtFileStream();
	~fbtFileStream();

	void open(const char* path, fbtStream::StreamMode mode);
	void close(void);

	bool isOpen(void)   const {return m_handle != 0;}
	bool eof(void)      const;

	FBTsize  read(void* dest, FBTsize nr) const;
	FBTsize  write(const void* src, FBTsize nr);
	FBTsize  writef(const char* buf, ...);


	FBTsize  position(void) const;
	FBTsize  size(void)     const;
	FBTsize seek(FBTint32 off, FBTint32 way);

	void write(fbtMemoryStream &ms) const;

protected:


	fbtFixedString<272> m_file;
	fbtFileHandle       m_handle;
	int                 m_mode;
	int					m_size;
};




#if FBT_USE_GZ_FILE == 1
class fbtGzStream : public fbtStream
{
public:
	fbtGzStream();
	~fbtGzStream();

	void open(const char* path, fbtStream::StreamMode mode);
	void close(void);

	bool isOpen(void)   const {return m_handle != 0;}
	bool eof(void)      const;

	FBTsize  read(void* dest, FBTsize nr) const;
	FBTsize  write(const void* src, FBTsize nr);
	FBTsize  writef(const char* buf, ...);


	FBTsize  position(void) const;
	FBTsize size(void) const;

	// watch it no size / seek

protected:


	fbtFixedString<272> m_file;
	fbtFileHandle       m_handle;
	int                 m_mode;
};
#endif


class fbtMemoryStream : public fbtStream
{
public:
	fbtMemoryStream();
	~fbtMemoryStream();

	void clear(void);

	void open(fbtStream::StreamMode mode);
	void open(const char* path, fbtStream::StreamMode mode);
	void open(const fbtFileStream& fs, fbtStream::StreamMode mode);
	void open(const void* buffer, FBTsize size, fbtStream::StreamMode mode,bool compressed=false);


	bool     isOpen(void)    const   {return m_buffer != 0;}
	bool     eof(void)       const   {return !m_buffer || m_pos >= m_size;}
	FBTsize  position(void)  const   {return m_pos;}
	FBTsize  size(void)      const   {return m_size;}

	FBTsize  read(void* dest, FBTsize nr) const;
	FBTsize  write(const void* src, FBTsize nr);
	FBTsize  writef(const char* buf, ...);
#if FBT_USE_GZ_FILE == 1
	bool gzipInflate( char* inBuf, int inSize);
#endif
#if FBT_USE_ZSTD_FILE == 1
	bool zstdInflate( char* inBuf, int inSize);
#endif
	void*            ptr(void)          {return m_buffer;}
	const void*      ptr(void) const    {return m_buffer;}

	FBTsize seek(FBTint32 off, FBTint32 way);


	void reserve(FBTsize nr);
	void shrinkToFit(void);
protected:
	friend class fbtFileStream;

	char*            m_buffer;
	mutable FBTsize  m_pos;
	FBTsize          m_size, m_capacity;
	int              m_mode;
};

/** @}*/
#endif//_fbtStreams_h_

//#include "fbtTables.h"
#ifndef _fbtTables_h_
#define _fbtTables_h_

//#include "fbtTypes.h"

/** \addtogroup FBT
*  @{
*/

#define FBT_MAGIC 4

namespace fbtIdNames
{
    const char FBT_SDNA[FBT_MAGIC] = {'S', 'D', 'N', 'A'};
    const char FBT_NAME[FBT_MAGIC] = {'N', 'A', 'M', 'E'}; // Name array
    const char FBT_TYPE[FBT_MAGIC] = {'T', 'Y', 'P', 'E'}; // Type Array
    const char FBT_TLEN[FBT_MAGIC] = {'T', 'L', 'E', 'N'}; // Type length array
    const char FBT_STRC[FBT_MAGIC] = {'S', 'T', 'R', 'C'}; // Struct/Class Array
    const char FBT_OFFS[FBT_MAGIC] = {'O', 'F', 'F', 'S'}; // Offset map (Optional & TODO)
}

FBT_INLINE fbtFixedString<4> fbtByteToString(FBTuint32 i)
{
	union
	{
		char        ids[4];
		FBTuint32   idi;
	} IDU;
	IDU.idi = i;
	fbtFixedString<4> cp;
	cp.push_back(IDU.ids[0]);
	cp.push_back(IDU.ids[1]);
	cp.push_back(IDU.ids[2]);
	cp.push_back(IDU.ids[3]);
	return cp;
}


typedef struct fbtName
{
	char*           m_name;     // note: memory is in the raw table.
	int             m_loc;
	FBTuint32       m_nameId;
	int             m_ptrCount;
	int             m_numSlots, m_isFptr;
	int             m_arraySize;
	int             m_slots[FBT_ARRAY_SLOTS];
} fbtName;

typedef struct fbtType
{
	char*           m_name;     // note: memory is in the raw table.
	FBTuint32       m_typeId;	// fbtCharHashKey(typeName)
	FBTuint32       m_strcId;	
} fbtType;



typedef union fbtKey32
{
	FBTint16 k16[2];
	FBTint32 k32;
} fbtKey32;

typedef union fbtKey64
{
	FBTuint32 k32[2];
	FBTuint64 k64;

} fbtKey64;


class fbtStruct
{
public:
	typedef fbtArray<fbtStruct> Members;
	typedef fbtArray<fbtKey64>  Keys;
	
	enum Flag
	{
		CAN_LINK    = 0,
		MISSING     = (1 << 0),
		MISALIGNED  = (1 << 1),
		SKIP        = (1 << 2),
		NEED_CAST	= (1 << 3)
	};


	fbtStruct()
		:	m_key(),
			m_val(),
			m_off(0),
			m_len(0),
			m_nr(0),
			m_dp(0),
			m_strcId(0),
			m_flag(0),
			m_members(),
			m_link(0)
	{
	}
	~fbtStruct()    {}


	fbtKey32        m_key;		//k[0]: type, k[1]: name
	fbtKey64        m_val;		//key hash value, k[0]: type hash id, k[1]: member(field) base name hash id or 0(struct)
	FBTint32        m_off;		//offset
	FBTint32        m_len;
	FBTint32        m_nr, m_dp; //nr: array index, dp: embeded depth
	FBTint32        m_strcId;
	FBTint32        m_flag;
	Members         m_members;
	fbtStruct*      m_link;		//file/memory table struct link
	Keys            m_keyChain; //parent key hash chain(0: type hash, 1: name hash), size() == m_dp

	FBTsizeType     getUnlinkedMemberCount();
};


class fbtBinTables
{
public:
	typedef fbtName*                Names;  // < fbtMaxTable
	typedef fbtType*                Types;  // < fbtMaxTable
	typedef FBTtype*                TypeL;  // < fbtMaxTable
	typedef FBTtype**               Strcs;  // < fbtMaxTable * fbtMaxMember;


	// Base name trim (*[0-9]) for partial type, name matching
	// Example: M(char m_var[32]) F(char m_var[24])
	//
	//          result = M(char m_var[24] = F(char m_var[24]) then (M(char m_var[24->32]) = 0)
	//
	// (Note: bParse will skip m_var all together because of 'strcmp(Mtype, Ftype) && strcmp(Mname, Fname)')
	//
	typedef fbtArray<FBTuint32>     NameB;
	typedef fbtArray<fbtStruct*>    OffsM;

	typedef fbtHashTable<fbtCharHashKey, fbtType> TypeFinder;

public:

	fbtBinTables();
	fbtBinTables(void* ptr, const FBTsize& len);
	~fbtBinTables();

	bool read(bool swap);
	bool read(const void* ptr, const FBTsize& len, bool swap);

	FBTtype findTypeId(const fbtCharHashKey &cp);

	const char* getStructType(const fbtStruct* strc);
	const char* getStructName(const fbtStruct* strc);
	const char* getOwnerStructName(const fbtStruct* strc);


	Names   m_name;
	Types   m_type;
	TypeL   m_tlen;
	Strcs   m_strc;
	OffsM   m_offs;
	NameB   m_base;

	FBTuint32 m_nameNr;
	FBTuint32 m_typeNr;
	FBTuint32 m_strcNr;

	// It's safe to assume that memory len is FBT_VOID and file len is FH_CHUNK_64 ? 8 : 4
	// Othewise this library will not even compile (no more need for 'sizeof(ListBase) / 2')
	FBTuint8    m_ptr;
	void*       m_otherBlock;
	FBTsize     m_otherLen;


private:

	TypeFinder m_typeFinder;

	void putMember(FBTtype* cp, fbtStruct* off, FBTtype nr, FBTuint32& cof, FBTuint32 depth, fbtStruct::Keys& keys);
	void compile(FBTtype i, FBTtype nr, fbtStruct* off, FBTuint32& cof, FBTuint32 depth, fbtStruct::Keys& keys);
	void compile(void);
	bool sikp(const FBTuint32& type);

};


/** @}*/
#endif//_fbtTables_h_

//#include "fbtPlatformHeaders.h"
#ifndef _fbtPlatformHeaders_h_
#define _fbtPlatformHeaders_h_

#ifndef FBT_IN_SOURCE
#error source include only!
#endif

#if FBT_PLATFORM == FBT_PLATFORM_WIN32
# if FBT_COMPILER == FBT_COMPILER_MSVC
#   define _WIN32_WINNT 0x403
# endif
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN 1
# endif
# include <windows.h>
# include <io.h>
#else
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <memory.h>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#if FBT_COMPILER == FBT_COMPILER_MSVC
#   if _MSC_VER>1310
#       define fbtp_printf(ptr, size, fmt, lst) _vsnprintf_s(ptr, size, fmt, lst)
#   else
#       define fbtp_printf _vsnprintf
#   endif
#else
# define fbtp_printf vsnprintf
#endif

#endif//_fbtPlatformHeaders_h_

// Common Identifiers
const FBTuint32 ENDB = FBT_ID('E', 'N', 'D', 'B');
const FBTuint32 DNA1 = FBT_ID('D', 'N', 'A', '1');
const FBTuint32 DATA = FBT_ID('D', 'A', 'T', 'A');
const FBTuint32 SDNA = FBT_ID('S', 'D', 'N', 'A');


// Compile asserts
FBT_ASSERTCOMP(ChunkLen32, sizeof(fbtFile::Chunk32) == 20);
FBT_ASSERTCOMP(ChunkLen64, sizeof(fbtFile::Chunk64) == 24);

#if FBT_ARCH == FBT_ARCH_32
FBT_ASSERTCOMP(ChunkLenNative, sizeof(fbtFile::Chunk32) == sizeof(fbtFile::Chunk));
#else
FBT_ASSERTCOMP(ChunkLenNative, sizeof(fbtFile::Chunk64) == sizeof(fbtFile::Chunk));
#endif

#define FBT_MALLOC_FAILED   fbtPrintf("Failed to allocate memory!\n");
#define FBT_INVALID_READ    fbtPrintf("Invalid read!\n");
#define FBT_INVALID_LEN     fbtPrintf("Invalid block length!\n");
#define FBT_INVALID_INS     fbtPrintf("Table insertion failed!\n");
#define FBT_LINK_FAILED     fbtPrintf("Linking failed!\n");


struct fbtChunk
{
	enum Size
	{
		BlockSize   = sizeof (fbtFile::Chunk),
		Block32     = sizeof (fbtFile::Chunk32),
		Block64     = sizeof (fbtFile::Chunk64),
	};

	static int read(fbtFile::Chunk* dest, fbtStream* stream, int flags);
	static int write(fbtFile::Chunk* src, fbtStream* stream);
};




fbtFile::fbtFile(const char* uid)
    :   m_uhid(uid), m_aluhid(0), m_version(-1), m_fileVersion(0), m_fileHeader(0),
        m_curFile(0), m_memory(0), m_file(0)
{
}


fbtFile::~fbtFile()
{
	if (m_curFile)
		fbtFree(m_curFile);
	m_curFile = 0;

	MemoryChunk* node = (MemoryChunk*)m_chunks.first, *tnd;
	while (node)
	{
		if (node->m_block)
		{
			//printf("free  m_block: 0x%x\n", node->m_block);fflush(stdout);
			fbtFree(node->m_block);
		}
		if (node->m_newBlock)
		{
			//printf("free m_newBlock: 0x%x\n", node->m_newBlock);fflush(stdout);
			fbtFree(node->m_newBlock);
		}

		tnd  = node;
		node = node->m_next;

		fbtFree(tnd);
	}

	delete m_file;
	delete m_memory;
}

bool fbtFile::FileStartsWith(const char* path,const char* cmp) {
    if (! path || !cmp) return false;
    const size_t numCharsToMatch = strlen(cmp);
    if (numCharsToMatch>255) return false;
    bool match = false;
    FILE* f = UTF8_fopen(path,"rb");
    if (f) {
        if (fseek(f, 0, SEEK_END))  {fclose(f);return match;}
        const long f_size = ftell(f);
        if (f_size<=0 || (size_t)f_size < numCharsToMatch)    {fclose(f);return match;}
        if (fseek(f, 0, SEEK_SET))  {fclose(f);return match;}
        char buff[256]="";
        if (fread(buff,numCharsToMatch,1,f) && strncmp(buff,cmp,numCharsToMatch)==0) match = true;
        fclose(f);
    }
    return match;
}

FILE* fbtFile::UTF8_fopen(const char* filename, const char* mode)   {
    if (!filename || !mode) return NULL;
#	ifdef _WIN32
    const int filenameLen = strlen(filename);
    const int modeLen = strlen(mode);
    wchar_t wfilename[MAX_PATH+1]=L"";
    wchar_t wmode[MAX_PATH+1]=L"";
    FILE* f = NULL;
    int wfilenameLen = 0, wmodeLen = 0;

    if (filenameLen == 0 || modeLen == 0) return NULL;
    wfilenameLen    = MultiByteToWideChar(CP_UTF8, 0, filename, filenameLen,    wfilename,  MAX_PATH);
    wmodeLen        = MultiByteToWideChar(CP_UTF8, 0, mode,     modeLen,        wmode,      MAX_PATH);
    if (wfilenameLen <= MAX_PATH && wmodeLen <= MAX_PATH) {
        wfilename[wfilenameLen] = L'\0';
        wmode[wmodeLen]         = L'\0';
        f = _wfopen(wfilename, wmode);
    }
    else f = fopen(filename, mode); // fallback
    return f;
#	else
    return fopen(filename, mode);
#	endif
}

unsigned char* fbtFile::FBT_GetFileContent(const char *filePath,unsigned long* pSizeOut,const char modes[])   {
    unsigned char* ptr = NULL;
    if (pSizeOut) *pSizeOut=0;
    if (!filePath) return ptr;
    FILE* f;
    if ((f = fbtFile::UTF8_fopen(filePath, modes)) == NULL) return ptr;
    if (fseek(f, 0, SEEK_END))  {fclose(f);return ptr;}
    const long f_size_signed = ftell(f);
    if (f_size_signed == -1)    {fclose(f);return ptr;}
    size_t f_size = (size_t)f_size_signed;
    if (fseek(f, 0, SEEK_SET))  {fclose(f);return ptr;}
    ptr = new unsigned char[f_size];
    if (!ptr) return ptr;
    const size_t f_size_read = f_size>0 ? fread((unsigned char*)ptr, 1, f_size, f) : 0;
    fclose(f);
    if (f_size_read == 0 || f_size_read!=f_size)    {delete[] (ptr);ptr=NULL;}
    else if (pSizeOut) *pSizeOut=f_size;
    return ptr;
}

int fbtFile::parse(const char* path,const char* uncompressedFileDetectorPrefix)    {
    return parse(path,FileStartsWith(path,uncompressedFileDetectorPrefix) ? PM_UNCOMPRESSED : PM_COMPRESSED);
}

int fbtFile::parse(const char* path, int mode)
{
	fbtStream* stream = 0;

#   if ((FBT_USE_ZSTD_FILE==1) && (BLENDER_VERSION>=300))
    if (mode==PM_COMPRESSED)    {
        unsigned long memorySize = 0;
        unsigned char* memory = fbtFile::FBT_GetFileContent(path,&memorySize,"rb");
        int rv = FS_FAILED;
        if (memory) {
            rv = parse((const void*) memory, memorySize, mode, /*bool suppressHeaderWarning*/false);
            delete[] (memory);memory=NULL;memorySize=0;
        }
        if (rv==FS_OK) return rv;
        else {
            fbtPrintf("Warning: fbtFile::parse(\"%s\" has failed with error code %d, when using the zstd decoder.",path,rv);
#           if FBT_USE_GZ_FILE == 1
            fbtPrintf("Trying with the gzip decoder now.");
#           endif
        }
    }
#   endif //((FBT_USE_ZSTD_FILE==1) && (BLENDER_VERSION>=300))

	if (mode == PM_UNCOMPRESSED || mode == PM_COMPRESSED)
	{
#   if FBT_USE_GZ_FILE == 1
		if (mode == PM_COMPRESSED)  {
            stream = new fbtGzStream();
	    }
		else
#   endif  // FBT_USE_GZ_FILE == 1
		{
			stream = new fbtFileStream();
		}
		stream->open(path, fbtStream::SM_READ);
	}
	else
	{
		stream = new fbtMemoryStream();
		stream->open(path, fbtStream::SM_READ);
	}

	if (!stream->isOpen())
	{
		fbtPrintf("File '%s' loading failed\n", path);
		return FS_FAILED;
	}

	if (m_curFile)
		fbtFree(m_curFile);


	FBTsize pl = strlen(path);

	m_curFile = (char*)fbtMalloc(pl + 1);
	if (m_curFile)
	{
		fbtMemcpy(m_curFile, path, pl);
		m_curFile[pl] = 0;
	}

    int result = parseStreamImpl(stream);
	delete stream;
	return result;
}



int fbtFile::parse(const void* memory, FBTsize sizeInBytes, int mode, bool suppressHeaderWarning)
{
	fbtMemoryStream ms;
	ms.open( memory, sizeInBytes, fbtStream::SM_READ, mode==PM_COMPRESSED );

	if (!ms.isOpen())
	{
		fbtPrintf("Memory %p(%i) loading failed\n", memory, sizeInBytes);
		return FS_FAILED;
	}

	return parseStreamImpl(&ms,suppressHeaderWarning);
}



int fbtFile::parseHeader(fbtStream* stream, bool suppressHeaderWarning)
{
	m_header.resize(12);
	stream->read(m_header.ptr(), 12);

	if (!fbtCharNEq(m_header.c_str(), m_uhid, 6) && !fbtCharNEq(m_header.c_str(), m_aluhid, 7))
	{
		if (!suppressHeaderWarning)
			fbtPrintf("Unknown header ID '%s'\n", m_header.c_str());
		return FS_INV_HEADER_STR;
	}

	char* headerMagic = (m_header.ptr() + 7);

	m_fileHeader = 0;
	m_fileVersion = 0;

	if (*(headerMagic++) == FM_64_BIT)
	{
		m_fileHeader |= FH_CHUNK_64;
		if (FBT_VOID4)
			m_fileHeader |= FH_VAR_BITS;
	}
	else if (FBT_VOID8)
		m_fileHeader |= FH_VAR_BITS;


	if (*(headerMagic++) == FM_BIG_ENDIAN)
	{
		if (FBT_ENDIAN_IS_LITTLE)
			m_fileHeader |= FH_ENDIAN_SWAP;
	}
	else if (FBT_ENDIAN_IS_BIG)
		m_fileHeader |= FH_ENDIAN_SWAP;


	m_fileVersion = atoi(headerMagic);

	return FS_OK;
}


int fbtFile::parseStreamImpl(fbtStream* stream, bool suppressHeaderWarning)
{
	int status;

	status = parseHeader(stream,suppressHeaderWarning);
	if (status != FS_OK)
	{
		fbtPrintf("Failed to extract header!\n");
		return status;
	}

	if (!m_memory)
	{
		m_memory = new fbtBinTables();

		status = initializeTables(m_memory);
		if (status != FS_OK)
		{
			fbtPrintf("Failed to initialize builtin tables\n");
			return status;
		}
	}


	if (m_file)
	{
		delete m_file;
		m_file = 0;
	}


	// preallocate table
	m_map.reserve(fbtDefaultAlloc);


	Chunk chunk;


	// Scan chunks

	do
	{
		if ((status = fbtChunk::read(&chunk, stream, m_fileHeader)) <= 0)
		{
			FBT_INVALID_READ;
			return FS_INV_READ;
		}


		if (chunk.m_code == SDNA)
		{
			chunk.m_code = DNA1;
			stream->seek(-status, SEEK_CUR);
			chunk.m_len = stream->size() - stream->position();
		}

		// exit on end byte
		if (chunk.m_code == ENDB)
			break;


		void* curPtr = fbtMalloc(chunk.m_len);
		//printf("alloc curPtr: 0x%x\n", curPtr);fflush(stdout);
		if (!curPtr)
		{
			FBT_MALLOC_FAILED;
			return FS_BAD_ALLOC;
		}

		if (stream->read(curPtr, chunk.m_len) <= 0)
		{
			FBT_INVALID_READ;
			return FS_INV_READ;
		}

		if (chunk.m_code == DNA1)
		{
			m_file = new fbtBinTables(curPtr, chunk.m_len);
			m_file->m_ptr = m_fileHeader & FH_CHUNK_64 ? 8 : 4;


			if (!m_file->read((m_fileHeader & FH_ENDIAN_SWAP) != 0))
			{
				fbtPrintf("Failed to initialize tables\n");
				return FS_INV_READ;
			}

			compileOffsets();

			if ((status = link()) != FS_OK)
			{
				FBT_LINK_FAILED;
				return FS_LINK_FAILED;
			}
			break;
		}
		else
		{
#if FBT_ASSERT_INSERT
			FBTsizeType pos;
			if ((pos = m_map.find(chunk.m_old)) != FBT_NPOS)
			{
				fbtFree(curPtr);
				curPtr = 0;
				int result = fbtMemcmp(&m_map.at(pos)->m_chunk, &chunk, fbtChunk::BlockSize);
				if (result != 0)
				{
					FBT_INVALID_READ;
					return FS_INV_READ;
				}
			}
#else
			if (m_map.find(chunk.m_old) != FBT_NPOS)
			{
				//printf("free  curPtr: 0x%x\n", curPtr);
				fbtFree(curPtr);
				curPtr = 0;
			}
#endif
			else
			{
				MemoryChunk* bin = static_cast<MemoryChunk*>(fbtMalloc(sizeof(MemoryChunk)));
				if (!bin)
				{
					FBT_MALLOC_FAILED;
					return FS_BAD_ALLOC;
				}
				fbtMemset(bin, 0, sizeof(MemoryChunk));
				bin->m_block = curPtr;

				Chunk* cp    = &bin->m_chunk;
				cp->m_code   = chunk.m_code;
				cp->m_len    = chunk.m_len;
				cp->m_nr     = chunk.m_nr;
				cp->m_typeid = chunk.m_typeid;
				cp->m_old    = chunk.m_old;
				m_chunks.push_back(bin);

				if (m_map.insert(bin->m_chunk.m_old, bin) == false)
				{
					FBT_INVALID_INS;
					return FS_INV_INSERT;
				}
			}
		}
	}
	while (!stream->eof());
	return status;
}


class fbtPrimType
{
public:

};

class fbtLinkCompiler
{
public:
	fbtBinTables* m_mp;
	fbtBinTables* m_fp;

	fbtStruct* find(const fbtCharHashKey& kvp);
	fbtStruct* find(fbtStruct* strc, fbtStruct* member, bool isPointer, bool& needCast);
	int        link(void);
};



fbtStruct* fbtLinkCompiler::find(const fbtCharHashKey& kvp)
{
	FBTtype i;
	if ( (i = m_fp->findTypeId(kvp)) != ((FBTtype)-1))
		return m_fp->m_offs.at(i);
	return 0;
}


fbtStruct* fbtLinkCompiler::find(fbtStruct* strc, fbtStruct* member, bool isPointer, bool& needCast)
{
	fbtStruct::Members::Pointer md = strc->m_members.ptr();
	FBTsizeType i, s = strc->m_members.size();

	FBTuint32 k1 = member->m_val.k32[0];

	for (i = 0; i < s; i++)
	{
		fbtStruct* strc2 = &md[i];

		if (strc2->m_nr == member->m_nr && strc2->m_dp ==  member->m_dp)
		{
			if (strc2->m_val.k32[1] == member->m_val.k32[1]) //base name
			{				
				if (!strc2->m_keyChain.equal(member->m_keyChain))
					continue;

				FBTuint32 k2 = strc2->m_val.k32[0];
				if (k1 == k2)
					return strc2;

				if (isPointer)
					continue;

				if (fbtIsIntType(k1) && fbtIsIntType(k2))
					return strc2;

				if (fbtIsNumberType(k1) && fbtIsNumberType(k2))
				{
					needCast = true;
					return strc2;
				}
			}
		}
	}


	return 0;
}

int fbtLinkCompiler::link(void)
{
	fbtBinTables::OffsM::Pointer md = m_mp->m_offs.ptr();
	fbtBinTables::OffsM::Pointer fd = m_fp->m_offs.ptr();

	FBTsizeType i, i2; 
	fbtStruct::Members::Pointer p2;


	for (i = 0; i < m_mp->m_offs.size(); ++i)
	{
		fbtStruct* strc = md[i];
		strc->m_link = find(m_mp->m_type[strc->m_key.k16[0]].m_name);

		if (strc->m_link)
			strc->m_link->m_link = strc;

		p2 = strc->m_members.ptr();

		//fbtPrintf("+%-3d %s\n", i, m_mp->getStructType(strc));
		for (i2 = 0; i2 < strc->m_members.size(); ++i2)
		{
			fbtStruct* member = &strc->m_members[i2];
			//fbtPrintf("  %3d %s %s\n", i2, m_mp->getStructType(strc2), m_mp->getStructName(strc2));

			FBT_ASSERT(!member->m_link);
			member->m_flag |= strc->m_link ? 0 : fbtStruct::MISSING;

			if (!(member->m_flag & fbtStruct::MISSING))
			{
				FBT_ASSERT(member->m_key.k16[1] < m_mp->m_nameNr);
				bool isPointer = m_mp->m_name[member->m_key.k16[1]].m_ptrCount > 0;
				bool needCast = false;
				member->m_link = find(strc->m_link, member, isPointer, needCast);
				if (member->m_link)
				{
					member->m_link->m_link = member;
					if (needCast)
					{
						member->m_flag |= fbtStruct::NEED_CAST;
						member->m_link->m_flag |= fbtStruct::NEED_CAST;
					}
				}
			}
		}

	}

	return fbtFile::FS_OK;
}

void copyValues(FBTbyte* srcPtr, FBTbyte* dstPtr, FBTsize srcElmSize, FBTsize dstElmSize, FBTsize len)
{
	FBTsize i;
	for (i = 0; i < len; i++)
	{
		fbtMemcpy(srcPtr, dstPtr, srcElmSize);
		srcPtr += srcElmSize;
		dstElmSize += dstElmSize;
	}
}

void castValue(FBTsize* srcPtr, FBTsize* dstPtr, FBT_PRIM_TYPE srctp, FBT_PRIM_TYPE dsttp, FBTsize len)
{
#define GET_V(value, current, type, cast, ptr, match) \
	if (current == type) \
	{ \
		value = (*(cast*)ptr); \
		ptr += sizeof(cast); \
		if (++match >= 2) continue; \
	}

#define SET_V(value, current, type, cast, ptr, match) \
	if (current == type) \
	{ \
		(*(cast*)ptr) = (cast)value; \
		ptr += sizeof(cast); \
		if (++match >= 2) continue; \
	}

	double value = 0.0;

	FBTsizeType i;
	for (i = 0; i < len; i++)
	{
		int match = 0;
		GET_V(value, srctp, FBT_PRIM_CHAR,    char,            srcPtr, match);
		SET_V(value, dsttp, FBT_PRIM_CHAR,    char,            dstPtr, match);
		GET_V(value, srctp, FBT_PRIM_SHORT,   short,           srcPtr, match);
		SET_V(value, dsttp, FBT_PRIM_SHORT,   short,           dstPtr, match);
		GET_V(value, srctp, FBT_PRIM_USHORT,  unsigned short,  srcPtr, match);
		SET_V(value, dsttp, FBT_PRIM_USHORT,  unsigned short,  dstPtr, match);
		GET_V(value, srctp, FBT_PRIM_INT,     int,             srcPtr, match);
		SET_V(value, dsttp, FBT_PRIM_INT,     int,             dstPtr, match);
		GET_V(value, srctp, FBT_PRIM_LONG,    int,             srcPtr, match);
		SET_V(value, dsttp, FBT_PRIM_LONG,    int,             dstPtr, match);
		GET_V(value, srctp, FBT_PRIM_FLOAT,   float,           srcPtr, match);
		SET_V(value, dsttp, FBT_PRIM_FLOAT,   float,           dstPtr, match);
		GET_V(value, srctp, FBT_PRIM_DOUBLE,  double,          srcPtr, match);
		SET_V(value, dsttp, FBT_PRIM_DOUBLE,  double,          dstPtr, match);
	}
#undef GET_V
#undef SET_V
}


int fbtFile::link(void)
{
	fbtBinTables::OffsM::Pointer md = m_memory->m_offs.ptr();
	fbtBinTables::OffsM::Pointer fd = m_file->m_offs.ptr();
	FBTsizeType s2, i2, a2, n;
	fbtStruct::Members::Pointer p2;
	FBTsize mlen, malen, total, pi;

	char* dst, *src;
	FBTsize* dstPtr, *srcPtr;

	bool endianSwap = (m_fileHeader & FH_ENDIAN_SWAP) != 0;

	static const FBThash hk = fbtCharHashKey("Link").hash();


	MemoryChunk* node;
	for (node = (MemoryChunk*)m_chunks.first; node; node = node->m_next)
	{
		if (node->m_chunk.m_typeid > m_file->m_strcNr || !( fd[node->m_chunk.m_typeid]->m_link))
			continue;

		fbtStruct* fs, *ms;
		fs = fd[node->m_chunk.m_typeid];
		ms = fs->m_link;

		node->m_newTypeId = ms->m_strcId;

		if (m_memory->m_type[ms->m_key.k16[0]].m_typeId == hk)
		{
			FBTsize totSize = node->m_chunk.m_len;
			node->m_newBlock = fbtMalloc(totSize);
			//printf("alloc1 m_newBlock: 0x%x %d\n", node->m_newBlock, totSize);fflush(stdout);

			if (!node->m_newBlock)
			{
				FBT_MALLOC_FAILED;
				return FS_BAD_ALLOC;
			}

			fbtMemcpy(node->m_newBlock, node->m_block, totSize);
			continue;
		}


		if (skip(m_memory->m_type[ms->m_key.k16[0]].m_typeId))
			continue;


		FBTsize totSize = (node->m_chunk.m_nr * ms->m_len);

		node->m_chunk.m_len = totSize;


		node->m_newBlock = fbtMalloc(totSize);
		//printf("alloc2 m_newBlock: 0x%x %d\n", node->m_newBlock, totSize);fflush(stdout);

		if (!node->m_newBlock)
		{
			FBT_MALLOC_FAILED;
			return FS_BAD_ALLOC;
		}



		// always zero this
		fbtMemset(node->m_newBlock, 0, totSize);
	}



	FBTuint8 mps = m_memory->m_ptr, fps = m_file->m_ptr;

	for (node = (MemoryChunk*)m_chunks.first; node; node = node->m_next)
	{
		if (node->m_newTypeId > m_memory->m_strcNr)
			continue;



		fbtStruct* cs = md[node->m_newTypeId];
		if (m_memory->m_type[cs->m_key.k16[0]].m_typeId == hk)
			continue;

		if (!cs->m_link || skip(m_memory->m_type[cs->m_key.k16[0]].m_typeId) || !node->m_newBlock)
		{
			//printf("free  m_newBlock: 0x%x \n", node->m_newBlock);fflush(stdout);

			fbtFree(node->m_newBlock);
			node->m_newBlock = 0;

			continue;
		}

		s2 = cs->m_members.size();
		p2 = cs->m_members.ptr();

		for (n = 0; n < node->m_chunk.m_nr; ++n)
		{
			dst = static_cast<char*>(node->m_newBlock) + (cs->m_len * n);
			src = static_cast<char*>(node->m_block) + (cs->m_link->m_len * n);


			for (i2 = 0; i2 < s2; ++i2)
			{
				fbtStruct* dstStrc = &p2[i2];
				fbtStruct* srcStrc = dstStrc->m_link;

				// If it's missing we can safely skip this block
				if (!srcStrc)
					continue;


				dstPtr = reinterpret_cast<FBTsize*>(dst + dstStrc->m_off);
				srcPtr = reinterpret_cast<FBTsize*>(src + srcStrc->m_off);



				const fbtName& nameD = m_memory->m_name[dstStrc->m_key.k16[1]];
				const fbtName& nameS = m_file->m_name[srcStrc->m_key.k16[1]];


				if (nameD.m_ptrCount > 0)
				{
					if ((*srcPtr))
					{
						if (nameD.m_ptrCount  > 1)
						{
							MemoryChunk* bin = findBlock((FBTsize)(*srcPtr));
							if (bin)
							{
								if (bin->m_flag & MemoryChunk::BLK_MODIFIED)
									(*dstPtr) = (FBTsize)bin->m_newBlock;
								else
								{
									// take pointer size out of the equation
									total = bin->m_chunk.m_len / fps;


									FBTsize* nptr = (FBTsize*)fbtMalloc(total * mps);
									fbtMemset(nptr, 0, total * mps);

									// always use 32 bit, then offset + 2 for 64 bit (Old pointers are sorted in this mannor)
									FBTuint32* optr = (FBTuint32*)bin->m_block;


									for (pi = 0; pi < total; pi++, optr += (fps == 4 ? 1 : 2))
										nptr[pi] = (FBTsize)findPtr((FBTsize) * optr);

									(*dstPtr) = (FBTsize)(nptr);

									bin->m_chunk.m_len = total * mps;
									bin->m_flag |= MemoryChunk::BLK_MODIFIED;

									fbtFree(bin->m_newBlock);
									bin->m_newBlock = nptr;
								}
							}
							else
							{
								//fbtPrintf("**block not found @ 0x%p)\n", src);
							}
						}
						else
						{
							malen = nameD.m_arraySize > nameS.m_arraySize ? nameS.m_arraySize : nameD.m_arraySize;

							FBTsize* dptr = (FBTsize*)dstPtr;

							// always use 32 bit, then offset + 2 for 64 bit (Old pointers are sorted in this mannor)
							FBTuint32* sptr = (FBTuint32*)srcPtr;


							for (a2 = 0; a2 < malen; ++a2, sptr += (fps == 4 ? 1 : 2))
								dptr[a2] = (FBTsize)findPtr((FBTsize) * sptr);
						}
					}
				}
				else
				{
					FBTsize dstElmSize = dstStrc->m_len / nameD.m_arraySize;
					FBTsize srcElmSize = srcStrc->m_len / nameS.m_arraySize;

					bool needCast = (dstStrc->m_flag & fbtStruct::NEED_CAST) != 0;
					bool needSwap = endianSwap && srcElmSize > 1;

					if (!needCast && !needSwap && srcStrc->m_val.k32[0] == dstStrc->m_val.k32[0]) //same type
					{						
						// Take the minimum length of any array.
						mlen = fbtMin(srcStrc->m_len, dstStrc->m_len);

						fbtMemcpy(dstPtr, srcPtr, mlen);
						continue;
					}

					FBTbyte* dstBPtr = reinterpret_cast<FBTbyte*>(dstPtr);
					FBTbyte* srcBPtr = reinterpret_cast<FBTbyte*>(srcPtr);

					FBT_PRIM_TYPE stp = FBT_PRIM_UNKNOWN, dtp  = FBT_PRIM_UNKNOWN;

					if (needCast || needSwap)
					{
						stp = fbtGetPrimType(srcStrc->m_val.k32[0]);
						dtp = fbtGetPrimType(dstStrc->m_val.k32[0]);

						FBT_ASSERT(fbtIsNumberType(stp) && fbtIsNumberType(dtp) && stp != dtp);
					}

					FBTsize alen = fbtMin(nameS.m_arraySize, nameD.m_arraySize);
					FBTsize elen = fbtMin(srcElmSize, dstElmSize);

					FBTbyte tmpBuf[8] = {0, };
					FBTsize i;
					for (i = 0; i < alen; i++)
					{
						FBTbyte* tmp = srcBPtr;
						if (needSwap)
						{
							tmp = tmpBuf;
							fbtMemcpy(tmpBuf, srcBPtr, srcElmSize);

							if (stp == FBT_PRIM_SHORT || stp == FBT_PRIM_USHORT) 
								fbtSwap16((FBTuint16*)tmpBuf, 1);
							else if (stp >= FBT_PRIM_INT && stp <= FBT_PRIM_FLOAT) 
								fbtSwap32((FBTuint32*)tmpBuf, 1);
							else if (stp == FBT_PRIM_DOUBLE)
								fbtSwap64((FBTuint64*)tmpBuf, 1);
							else
								fbtMemset(tmpBuf, 0, sizeof(tmpBuf)); //unknown type
						}
						
						if (needCast)
							castValue((FBTsize*)tmp, (FBTsize*)dstBPtr, stp, dtp, 1);
						else
							fbtMemcpy(dstBPtr, tmp, elen);

						dstBPtr += dstElmSize;
						srcBPtr += srcElmSize;
					}
				}
			}
		}

		notifyData(node->m_newBlock, node->m_chunk);
	}



	for (node = (MemoryChunk*)m_chunks.first; node; node = node->m_next)
	{
		if (node->m_block)
		{
			fbtFree(node->m_block);
			node->m_block = 0;
		}
	}

	return fbtFile::FS_OK;
}




void* fbtFile::findPtr(const FBTsize& iptr)
{
	FBTsizeType i;
	if ((i = m_map.find(iptr)) != FBT_NPOS)
		return m_map.at(i)->m_newBlock;
	return 0;
}


fbtFile::MemoryChunk* fbtFile::findBlock(const FBTsize& iptr)
{
	FBTsizeType i;
	if ((i = m_map.find(iptr)) != FBT_NPOS)
		return m_map.at(i);
	return 0;
}



int fbtFile::compileOffsets(void)
{
	fbtLinkCompiler lnk;
	lnk.m_mp = m_memory;
	lnk.m_fp = m_file;
	return lnk.link();
}

bool fbtFile::_setuid(const char* uid)
{
	if (!uid || strlen(uid) != 7) return false;

	m_uhid = uid;
	return true;
}

int fbtFile::reflect(const char* path, const int mode, const fbtEndian& /*endian*/)
{
	fbtStream* fs;
	
#if FBT_USE_GZ_FILE == 1
	if (mode == PM_COMPRESSED)
		fs = new fbtGzStream();
	else
#endif
	{
		fs = new fbtFileStream();
	}
	
	fs->open(path, fbtStream::SM_WRITE);


	FBTuint8 cp = FBT_VOID8 ? FM_64_BIT : FM_32_BIT;
	FBTuint8 ce = ((FBTuint8)fbtGetEndian()) == FBT_ENDIAN_IS_BIG ? FM_BIG_ENDIAN : FM_LITTLE_ENDIAN;

//	Commented for now since the rest of the code does not care
//	if (endian != FBT_ENDIAN_NATIVE)
//	{
//		if (endian == FBT_ENDIAN_IS_BIG)
//			ce = FM_BIG_ENDIAN;
//		else
//			ce = FM_LITTLE_ENDIAN;
//	}

	// put magic
	//fs->writef("%s%c%c%i", m_uhid, cp, ce, m_version);
	char header[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	char version[33];
	sprintf(version, "%i",m_version);
	
	strncpy(&header[0], m_uhid, 7); // 7 first bytes of header
	header[7] = cp;					// 8th byte = pointer size
	header[8] = ce;					// 9th byte = endianness
	strncpy(&header[9], version, 3);// last 3 bytes vor 3 version char
	
	fs->write(header, 12);
	
	writeData(fs);

	// write DNA1
	Chunk ch;
	ch.m_code   = DNA1;
	ch.m_len    = getFBTlength();
	ch.m_nr     = 1;
	ch.m_old    = 0;
	ch.m_typeid = 0;
	fs->write(&ch, fbtChunk::BlockSize);
	fs->write(getFBT(), ch.m_len);


	// write ENDB (End Byte | EOF )
	ch.m_code   = ENDB;
	ch.m_len    = 0;
	ch.m_nr     = 0;
	ch.m_old    = 0;
	ch.m_typeid = 0;
	fs->write(&ch, fbtChunk::BlockSize);
	
	delete fs;
	return FS_OK;

}

void fbtFile::writeStruct(fbtStream* stream, FBTtype index, FBTuint32 code, FBTsize len, void* writeData)
{
	Chunk ch;
	ch.m_code   = code;
	ch.m_len    = len;
	ch.m_nr     = 1;
	ch.m_old    = (FBTsize)writeData;
	ch.m_typeid = index;

	fbtChunk::write(&ch, stream);
}

void fbtFile::writeBuffer(fbtStream* stream, FBTsize len, void* writeData)
{
	Chunk ch;
	ch.m_code   = DATA;
	ch.m_len    = len;
	ch.m_nr     = 1;
	ch.m_old    = (FBTsize)writeData;
	ch.m_typeid = m_memory->findTypeId("Link");

	fbtChunk::write(&ch, stream);
}

int fbtChunk::write(fbtFile::Chunk* src, fbtStream* stream)
{	
	int size = 0;
	size += stream->write(src, BlockSize);
	size += stream->write((void*)src->m_old, src->m_len);
	return size;
}

int fbtChunk::read(fbtFile::Chunk* dest, fbtStream* stream, int flags)
{
	int bytesRead = 0;
	bool bitsVary   = (flags & fbtFile::FH_VAR_BITS) != 0;
	bool swapEndian = (flags & fbtFile::FH_ENDIAN_SWAP) != 0;

	fbtFile::Chunk64 c64;
	fbtFile::Chunk32 c32;
	fbtFile::Chunk*  cpy;


	if (FBT_VOID8)
	{

		if (bitsVary)
		{
			fbtFile::Chunk32 src;
			if ((bytesRead = stream->read(&src, Block32)) <= 0)
			{
				FBT_INVALID_READ;
				return fbtFile::FS_INV_READ;
			}

			c64.m_code    = src.m_code;
			c64.m_len     = src.m_len;

			union
			{
				FBTuint64   m_ptr;
				FBTuint32   m_doublePtr[2];
			} ptr;
			ptr.m_doublePtr[0] = src.m_old;
			ptr.m_doublePtr[1] = 0;

			c64.m_old     = ptr.m_ptr;
			c64.m_typeid  = src.m_typeid;
			c64.m_nr      = src.m_nr;
		}
		else
		{
			if ((bytesRead = stream->read(&c64, BlockSize)) <= 0)
			{
				FBT_INVALID_READ;
				return fbtFile::FS_INV_READ;
			}
		}


		if (swapEndian)
		{
			if ((c64.m_code & 0xFFFF) == 0)
				c64.m_code >>= 16;

			c64.m_len    = fbtSwap32(c64.m_len);
			c64.m_nr     = fbtSwap32(c64.m_nr);
			c64.m_typeid = fbtSwap32(c64.m_typeid);
		}



		cpy = (fbtFile::Chunk*)(&c64);
	}
	else
	{

		if (bitsVary)
		{
			fbtFile::Chunk64 src;
			if ((bytesRead = stream->read(&src, Block64)) <= 0)
			{
				FBT_INVALID_READ;
				return fbtFile::FS_INV_READ;
			}

			// copy down

			c32.m_code    = src.m_code;
			c32.m_len     = src.m_len;

			union
			{
				FBTuint64   m_ptr;
				FBTuint32   m_doublePtr[2];
			} ptr;
			ptr.m_doublePtr[0] = 0;
			ptr.m_doublePtr[1] = 0;
			ptr.m_ptr = src.m_old;

			c32.m_old       = ptr.m_doublePtr[0] != 0 ? ptr.m_doublePtr[0] : ptr.m_doublePtr[1];
			c32.m_typeid    = src.m_typeid;
			c32.m_nr        = src.m_nr;
		}
		else
		{
			if ((bytesRead = stream->read(&c32, BlockSize)) <= 0)
			{
				FBT_INVALID_READ;
				return fbtFile::FS_INV_READ;
			}
		}


		if (swapEndian)
		{
			if ((c32.m_code & 0xFFFF) == 0)
				c32.m_code >>= 16;

			c32.m_len    = fbtSwap32(c32.m_len);
			c32.m_nr     = fbtSwap32(c32.m_nr);
			c32.m_typeid = fbtSwap32(c32.m_typeid);
		}



		cpy = (fbtFile::Chunk*)(&c32);
	}

	if (cpy->m_len == FBT_NPOS)
	{
		FBT_INVALID_LEN;
		return fbtFile::FS_INV_LENGTH;
	}

	fbtMemcpy(dest, cpy, BlockSize);
	return bytesRead;
}


// Source File "fbtStreams.cpp"
//#define FBT_IN_SOURCE
//#include "fbtStreams.h"
//#include "fbtPlatformHeaders.h"


#if FBT_USE_GZ_FILE == 1
#include "zlib.h"
#include "zconf.h"
#endif

#if FBT_USE_ZSTD_FILE == 1
#include "zstd.h"
#endif

fbtFileStream::fbtFileStream() 
	:    m_file(), m_handle(0), m_mode(0), m_size(0)
{
}


fbtFileStream::~fbtFileStream()
{
	close();
}



void fbtFileStream::open(const char* p, fbtStream::StreamMode mode)
{
	if (m_handle != 0 && m_file != p)
		fclose((FILE *)m_handle);

	char fm[3] = {0, 0, 0};
	char* mp = &fm[0];
	if (mode & fbtStream::SM_READ)
		*mp++ = 'r';
	else if (mode & fbtStream::SM_WRITE)
		*mp++ = 'w';
	*mp++ = 'b';
	fm[2] = 0;

	m_file = p;
    m_handle = fbtFile::UTF8_fopen(m_file.c_str(), fm);

	if (m_handle && (mode & fbtStream::SM_READ))
	{
		FILE *fp = (FILE*)m_handle;

		position();
		fseek(fp, 0, SEEK_END);
		m_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
	}


}


void fbtFileStream::close(void)
{
	if (m_handle != 0)
	{
		fclose((FILE*)m_handle);
		m_handle = 0;
	}

	m_file.clear();
}


bool fbtFileStream::eof(void) const
{
	if (!m_handle)
		return true;
	return feof((FILE*)m_handle) != 0;
}

FBTsize fbtFileStream::position(void) const
{
	return ftell((FILE*)m_handle);
}


FBTsize fbtFileStream::size(void) const
{
	return m_size;
}

FBTsize fbtFileStream::seek(FBTint32 off, FBTint32 way)
{
	if (!m_handle)
		return 0;

	return fseek((FILE*)m_handle, off, way);
}


FBTsize fbtFileStream::read(void* dest, FBTsize nr) const
{
	if (m_mode == fbtStream::SM_WRITE) 
		return -1;

	if (!dest || !m_handle) 
		return -1;

	return fread(dest, 1, nr, (FILE *)m_handle);
}



FBTsize fbtFileStream::write(const void* src, FBTsize nr)
{
	if (m_mode == fbtStream::SM_READ) return -1;
	if (!src || !m_handle) return -1;

	return fwrite(src, 1, nr, (FILE*)m_handle);
}

void fbtFileStream::write(fbtMemoryStream &ms) const
{
	FILE *fp = (FILE*)m_handle;

	int oldPos = ftell(fp);

	fseek(fp, 0, SEEK_END);
	int len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	ms.reserve(len + 1);
	ms.m_size = read(ms.m_buffer, len);

	fseek(fp, oldPos, SEEK_SET);
}


FBTsize fbtFileStream::writef(const char* fmt, ...)
{
	static char tmp[1024];

	va_list lst;
	va_start(lst, fmt);
	int size = fbtp_printf(tmp, 1024, fmt, lst);
	va_end(lst);

	if (size > 0)
	{
		tmp[size] = 0;
		return write(tmp, size);
	}
	return -1;

}


#if FBT_USE_GZ_FILE == 1

fbtGzStream::fbtGzStream() 
	:    m_file(), m_handle(0), m_mode(0)
{
}


fbtGzStream::~fbtGzStream()
{
	close();
}


void fbtGzStream::open(const char* p, fbtStream::StreamMode mode)
{
	if (m_handle != 0 && m_file != p)
        gzclose((gzFile) m_handle);

	char fm[3] = {0, 0, 0};
	char* mp = &fm[0];
	if (mode & fbtStream::SM_READ)
		*mp++ = 'r';
	else if (mode & fbtStream::SM_WRITE)
		*mp++ = 'w';
	*mp++ = 'b';
	fm[2] = 0;

	m_file = p;
	m_handle = gzopen(m_file.c_str(), fm);
}


void fbtGzStream::close(void)
{
	if (m_handle != 0)
	{
        gzclose((gzFile)m_handle);
		m_handle = 0;
	}

	m_file.clear();
}



FBTsize fbtGzStream::read(void* dest, FBTsize nr) const
{
	if (m_mode == fbtStream::SM_WRITE) return -1;
	if (!dest || !m_handle) 
		return -1;

    return gzread((gzFile)m_handle, dest, nr);
}


FBTsize fbtGzStream::write(const void* src, FBTsize nr)
{
	if (m_mode == fbtStream::SM_READ) return -1;
	if (!src || !m_handle) return -1;
    return gzwrite((gzFile)m_handle,(const voidp) src, nr);
}


bool fbtGzStream::eof(void) const
{
	if (!m_handle)
		return true;
    return gzeof((gzFile)m_handle) != 0;
}

FBTsize fbtGzStream::position(void) const
{
    return gztell((gzFile)m_handle);
}

FBTsize fbtGzStream::size(void) const
{
	return 0;
}


FBTsize fbtGzStream::writef(const char* fmt, ...)
{
	static char tmp[1024];

	va_list lst;
	va_start(lst, fmt);
	int size = fbtp_printf(tmp, 1024, fmt, lst);
	va_end(lst);

	if (size > 0)
	{
		tmp[size] = 0;
		return write(tmp, size);
	}
	return -1;

}

#endif // FBT_USE_GZ_FILE


fbtMemoryStream::fbtMemoryStream()
	:   m_buffer(0), m_pos(0), m_size(0), m_capacity(0), m_mode(0)
{
}


void fbtMemoryStream::open(fbtStream::StreamMode mode)
{
	m_mode = mode;
}

void fbtMemoryStream::open(const char* path, fbtStream::StreamMode mode)
{
	fbtFileStream fs;
	fs.open(path, fbtStream::SM_READ);

	if (fs.isOpen()) 
		open(fs, mode);
}


void fbtMemoryStream::open(const fbtFileStream& fs, fbtStream::StreamMode mode)
{
	if (fs.isOpen())
	{
		fs.write(*this);
		m_mode = mode;
	}
}


void fbtMemoryStream::open(const void* buffer, FBTsize size, fbtStream::StreamMode mode, bool compressed)
{
	if (buffer && size > 0 && size != FBT_NPOS)
	{
		m_mode = mode;
		m_pos  = 0;


		if (!compressed)
		{
			m_size = size;
			reserve(m_size);
			fbtMemcpy(m_buffer, buffer, m_size);

		} else
		{
		    bool result = false;
#           if BLENDER_VERSION >= 300
#               if FBT_USE_ZSTD_FILE == 1
                if (!result) result = zstdInflate((char*)buffer,size);
#               endif
#               if FBT_USE_GZ_FILE == 1
                if (!result) result = gzipInflate((char*)buffer,size);
#               endif
#           else //BLENDER_VERSION    		    
#               if FBT_USE_GZ_FILE == 1
                if (!result) result = gzipInflate((char*)buffer,size);
#               endif
#               if FBT_USE_ZSTD_FILE == 1
                if (!result) result = zstdInflate((char*)buffer,size);
#               endif
#           endif //BLENDER_VERSION
		}

	}
}

#if FBT_USE_GZ_FILE == 1
// this method was adapted from this snippet:
// http://windrealm.org/tutorials/decompress-gzip-stream.php
bool fbtMemoryStream::gzipInflate( char* inBuf, int inSize) {
  // 'inBuf' is the whole compressed stream, of compressed size 'inSize'
  if (m_buffer)
	  delete [] m_buffer;
  m_size = m_capacity = 0;

  reserve(m_capacity+inSize);

  z_stream strm;
  strm.next_in = (Bytef *) inBuf;
  strm.avail_in = inSize ;
  strm.total_out = 0;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;

  bool done = false;

  if (inflateInit2(&strm, (16+MAX_WBITS)) != Z_OK) {
    free( m_buffer );
    return false;
  }

  while (!done) {
    // If our output buffer is too small
    if (strm.total_out >= m_capacity ) {
      // Increase size of output buffer
      reserve(m_capacity+(inSize/2));
    }

    strm.next_out = (Bytef *) (m_buffer + strm.total_out);
    strm.avail_out = m_capacity - strm.total_out;

    // Inflate another chunk.
    int err = inflate (&strm, Z_SYNC_FLUSH);
    if (err == Z_STREAM_END) done = true;
    else if (err != Z_OK)  {
      break;
    }
  }

  if (inflateEnd (&strm) != Z_OK) {
    if (m_buffer)   delete [] m_buffer;
    m_size = m_capacity = 0;
    return false;
  }
  else {
    m_size = strm.total_out;
    shrinkToFit();
    done = true;
  }

  return done;
}
#endif

#if FBT_USE_ZSTD_FILE == 1
bool fbtMemoryStream::zstdInflate(char* inBuf, int inSize) {
    // 'inBuf' is the whole compressed stream, of compressed size 'inSize'  
    if ( inSize == 0 ) {
          m_buffer = inBuf;
          return true;
    }
    if (m_buffer)	  delete [] m_buffer;
    m_size = m_capacity = 0;
    
    const unsigned char* memoryBuffer = (const unsigned char*) inBuf;
    const size_t memoryBufferSize = (size_t) inSize;
  
    // FIll m_size, m_capacity and m_buffer from memoryBuffer and memoryBufferSize.
  
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    const size_t capacityStepMultiplier = 4;   
    
    // input buffer
    const size_t minInputSizeStep = ZSTD_DStreamInSize();
    const size_t inputSizeStep = minInputSizeStep*capacityStepMultiplier;
    ZSTD_inBuffer input = { memoryBuffer, 0, 0 };
    input.size = inputSizeStep<=memoryBufferSize?inputSizeStep:memoryBufferSize;
    
    // output buffer
    const size_t minOutputCapacityStep = ZSTD_DStreamOutSize();
    const size_t outputCapacityStep = minOutputCapacityStep*capacityStepMultiplier;
    reserve(memoryBufferSize*3);   // initial capacity
    ZSTD_outBuffer output = {m_buffer, m_capacity, 0 };
    
    size_t lastRet = 0;
    int failure = 0;
    
    //size_t last_input_pos = 1;    // optional loop lock prevention [line 1/3]
    //size_t num_reallocs = 0, num_decompression_calls = 0;   // optional debug [line 1/4]
    while (input.pos < memoryBufferSize) {
        //if (lastRet==0 && last_input_pos==input.pos) break;    // optional loop lock prevention [line 2/3]
        if (input.size<memoryBufferSize && input.size-input.pos<minInputSizeStep)    {
            input.size+=inputSizeStep;
            if (input.size>memoryBufferSize) input.size=memoryBufferSize;
        }
        size_t const ret = ZSTD_decompressStream(dctx, &output , &input);
        //++num_decompression_calls;   // optional debug [line 2/4]
        if (ZSTD_isError(ret))    {
            fbtPrintf("ZSTD_decompressStream(...) Error: %s\n",ZSTD_getErrorName(ret));
            failure = 1;break;
        }
        else {
            m_size=output.pos;
            if (input.pos < input.size) {
                if (output.size-output.pos<=outputCapacityStep)    {
                    reserve(m_capacity+minOutputCapacityStep+m_capacity/4);     // grow strategy               
                    //++num_reallocs;   // optional debug [line 3/4]
                    output.dst=m_buffer;output.size=m_capacity; // output.pos unchanged           
                }
            }
                
            //fbtPrintf("output: %zu/%zu. input: %zu/%zu.\n",output.pos,output.size,input.pos,input.size);
        }    
        lastRet = ret;
    }
  
    //fbtPrintf("num_reallocs: %zu. num_decompression_calls: %zu.\n",num_reallocs,num_decompression_calls);   // optional debug [line 4/4]
        

    if (lastRet != 0) {
        /* The last return value from ZSTD_decompressStream did not end on a
         * frame, but we reached the end of the file! We assume this is an
         * error, and the input was truncated.
         */
        fbtPrintf("ZSTD_decompressStream(...) Error: EOF before end of stream: %zu\n", lastRet);
        failure = 1;
    }  
    if (failure) {
      if (m_buffer) delete [] m_buffer;
      m_size = m_capacity = 0;    
    }          
    else shrinkToFit();
    
    ZSTD_freeDCtx(dctx);dctx=NULL;  
    return !failure;
}
#endif


fbtMemoryStream::~fbtMemoryStream()
{
	if (m_buffer != 0 )
	{
		delete []m_buffer;
	}
	m_buffer = 0;
	m_size = m_pos = 0;
	m_capacity = 0;
}



void fbtMemoryStream::clear(void)
{
	m_size = m_pos = 0;
	if (m_buffer)
		m_buffer[0] = 0;
}


FBTsize fbtMemoryStream::seek(FBTint32 off, FBTint32 way)
{
	if (way == SEEK_SET)
		m_pos = fbtClamp<FBTsize>(off, 0, m_size);
	else if (way == SEEK_CUR)
		m_pos = fbtClamp<FBTsize>(m_pos + off, 0, m_size);
	else if (way == SEEK_END)
		m_pos = m_size;
	return m_pos;
}



FBTsize fbtMemoryStream::read(void* dest, FBTsize nr) const
{
	if (m_mode == fbtStream::SM_WRITE) return -1;
	if (m_pos > m_size) return 0;
	if (!dest || !m_buffer) return 0;

	if ((m_size - m_pos) < nr) nr = m_size - m_pos;

	char* cp = (char*)dest;
	char* op = (char*)&m_buffer[m_pos];
	fbtMemcpy(cp, op, nr);
	m_pos += nr;
	return nr;
}



FBTsize fbtMemoryStream::write(const void* src, FBTsize nr)
{
	if (m_mode == fbtStream::SM_READ || !src)
		return -1;

	if (m_pos > m_size) return 0;

	if (m_buffer == 0)
		reserve(m_pos + (nr));
	else if (m_pos + nr > m_capacity)
		reserve(m_pos + (nr > 65535 ? nr : nr + 65535));

	char* cp = &m_buffer[m_pos];
	fbtMemcpy(cp, (char*)src, nr);

	m_pos   += nr;
	m_size  += nr;
	return nr;
}



FBTsize fbtMemoryStream::writef(const char* fmt, ...)
{
	static char tmp[1024];

	va_list lst;
	va_start(lst, fmt);
	int size = fbtp_printf(tmp, 1024, fmt, lst);
	va_end(lst);


	if (size > 0)
	{
		tmp[size] = 0;
		return write(tmp, size);
	}

	return -1;

}
void fbtMemoryStream::reserve(FBTsize nr)
{
	if (m_capacity < nr)
	{
		char* buf = new char[nr + 1];
		if (m_buffer != 0)
		{
			fbtMemcpy(buf, m_buffer, m_size);
			delete [] m_buffer;
		}

		m_buffer = buf;
		m_buffer[m_size] = 0;
		m_capacity = nr;
	}
}
void fbtMemoryStream::shrinkToFit()
{
	if (m_capacity > m_size+1)
	{
		char* buf = new char[m_size + 1];
		if (m_buffer != 0)
		{
			fbtMemcpy(buf, m_buffer, m_size);
			delete [] m_buffer;
		}

		m_buffer = buf;
		m_buffer[m_size] = 0;
		m_capacity = m_size;
	}
} 


// Source File "fbtTables.cpp"
//#define FBT_IN_SOURCE
//#include "fbtTables.h"
//#include "fbtPlatformHeaders.h"


FBTsizeType fbtStruct::getUnlinkedMemberCount()
{
	FBTsizeType count = 0;
	for (FBTsizeType i = 0; i < m_members.size(); i++)
		if (!m_members[i].m_link) count++;

	return count;
}


fbtBinTables::fbtBinTables()
	:   m_name(0),
	    m_type(0),
	    m_tlen(0),
	    m_strc(0),
	    m_nameNr(0),
	    m_typeNr(0),
	    m_strcNr(0),
	    m_ptr(FBT_VOID),
	    m_otherBlock(0),
	    m_otherLen(0)
{
}

fbtBinTables::fbtBinTables(void* ptr, const FBTsize& len)
	:   m_name(0),
	    m_type(0),
	    m_tlen(0),
	    m_strc(0),
	    m_nameNr(0),
	    m_typeNr(0),
	    m_strcNr(0),
	    m_ptr(FBT_VOID),
	    m_otherBlock(ptr),
	    m_otherLen(len)
{
}

fbtBinTables::~fbtBinTables()
{
	fbtFree(m_name);
	fbtFree(m_type);
	fbtFree(m_tlen);
	fbtFree(m_strc);
	if (m_otherBlock)
		fbtFree(m_otherBlock);


    OffsM::Iterator it = m_offs.iterator();
	while (it.hasMoreElements())
		delete it.getNext();
}


bool fbtBinTables::read(bool swap)
{
	if (m_otherBlock && m_otherLen != 0)
		return read(m_otherBlock, m_otherLen, swap);
	return false;
}


bool fbtBinTables::read(const void* ptr, const FBTsize& /*len*/, bool swap)
{
	FBTuint32* ip = 0, i, j, k, nl;
	FBTtype* tp = 0;

	char* cp = (char*)ptr;


    //if (!fbtCharNEq(cp,fbtIdNames::FBT_SDNA, FBT_MAGIC))  // -Waddress
    if (!(cp && !strncmp(cp,fbtIdNames::FBT_SDNA, FBT_MAGIC)))
	{
		fbtPrintf("Bin table is missing the start id!\n");
		return false;
	}

	cp += FBT_MAGIC;


    //if (!fbtCharNEq(cp, fbtIdNames::FBT_NAME, FBT_MAGIC))
    if (!(cp && !strncmp(cp, fbtIdNames::FBT_NAME, FBT_MAGIC)))
	{
		fbtPrintf("Bin table is missing the name id!\n");
		return false;
	}

	cp += FBT_MAGIC;


	FBTintPtr opad;

	ip = (FBTuint32*)cp;
	nl = *ip++;
	cp = (char*)ip;

	if (swap) nl = fbtSwap32(nl);


	if (nl > fbtMaxTable)
	{
		fbtPrintf("Max name table size exceeded!\n");
		return false;
	}
	else
	{
		m_name = (Names)fbtMalloc((nl * sizeof(fbtName)) + 1);
	}



	i = 0;
	while (i < nl && i < fbtMaxTable)
	{
        fbtName name = {cp,(int) i, fbtCharHashKey(cp).hash(), 0, 0, 0, 1, {0}};

		fbtFixedString<64> bn;

		// re-lex
		while (*cp)
		{
			switch (*cp)
			{
			default:
				{
					bn.push_back(*cp);
					++cp; break;
				}
			case ')':
			case ']':
				++cp;
				break;
			case '(':   {++cp; name.m_isFptr = 1; break;    }
			case '*':   {++cp; name.m_ptrCount ++; break;   }
			case '[':
				{
					while ((*++cp) != ']')
						name.m_slots[name.m_numSlots] = (name.m_slots[name.m_numSlots] * 10) + ((*cp) - '0');
					name.m_arraySize *= name.m_slots[name.m_numSlots++];
				}
				break;
			}
		}
		++cp;

		//fbtPrintf("%d %d: %s %s %u %u\n", m_nameNr, m_base.size(), name.m_name, bn.c_str(), name.m_nameId, bn.hash());

		m_name[m_nameNr++] = name;
		m_base.push_back(bn.hash());
		++i;
	}

	// read alignment
	opad = (FBTintPtr)cp;
	opad = ((opad + 3) & ~3) - opad;
	while (opad--) cp++;

    //if (!fbtCharNEq(cp, fbtIdNames::FBT_TYPE, FBT_MAGIC))
    if (!(cp && !strncmp(cp, fbtIdNames::FBT_TYPE, FBT_MAGIC)))
	{
		fbtPrintf("Bin table is missing the type id!\n");
		return false;
	}

	cp += FBT_MAGIC;

	ip = (FBTuint32*)cp;
	nl = *ip++;
	cp = (char*)ip;

	if (swap) nl = fbtSwap32(nl);

	if (nl > fbtMaxTable)
	{
		fbtPrintf("Max name table size exceeded!\n");
		return false;
	}
	else
	{
		m_type = (Types)fbtMalloc((nl * sizeof(fbtType) + 1));
		m_tlen = (TypeL)fbtMalloc((nl * sizeof(FBTtype) + 1));
	}

	i = 0;
	while (i < nl)
	{
        fbtType typeData = {cp, fbtCharHashKey(cp).hash(), (FBTuint32)-1};  // Added explicit cast
		m_type[m_typeNr++] = typeData;
		while (*cp) ++cp;
		++cp;
		++i;
	}

	// read alignment
	opad = (FBTintPtr)cp;
	opad = ((opad + 3) & ~3) - opad;
	while (opad--) cp++;

    //if (!fbtCharNEq(cp, fbtIdNames::FBT_TLEN, FBT_MAGIC))
    if (!(cp && !strncmp(cp, fbtIdNames::FBT_TLEN, FBT_MAGIC)))
	{
		fbtPrintf("Bin table is missing the tlen id!\n");
		return false;
	}

	cp += FBT_MAGIC;

	tp = (FBTtype*)cp;

	i = 0;
	while (i < m_typeNr)
	{

		m_tlen[i] = *tp++;
		if (swap)
			m_tlen[i] = fbtSwap16(m_tlen[i]);
		++i;
	}

	// read alignment
	if (m_typeNr & 1) ++tp;

	cp = (char*)tp;

    //if (!fbtCharNEq(cp, fbtIdNames::FBT_STRC, FBT_MAGIC))
    if (!(cp && !strncmp(cp, fbtIdNames::FBT_STRC, FBT_MAGIC)))
	{
		fbtPrintf("Bin table is missing the tlen id!\n");
		return false;
	}

	cp += FBT_MAGIC;


	ip = (FBTuint32*)cp;
	nl = *ip++;
	tp = (FBTtype*)ip;

	if (swap) nl = fbtSwap32(nl);

	if (nl > fbtMaxTable)
	{
		fbtPrintf("Max name table size exceeded!\n");
		return false;
	}
	else
		m_strc = (Strcs)fbtMalloc(nl * fbtMaxMember * sizeof(FBTtype) + 1);


	m_typeFinder.reserve(m_typeNr);


	i = 0;
	while (i < nl)
	{
		m_strc[m_strcNr++] = tp;


		if (swap)
		{
			tp[0] = fbtSwap16(tp[0]);
			tp[1] = fbtSwap16(tp[1]);

			m_type[tp[0]].m_strcId = m_strcNr - 1;

			m_typeFinder.insert(m_type[tp[0]].m_name, m_type[tp[0]]);


			k = tp[1];
			FBT_ASSERT( k < fbtMaxMember );

			j = 0;
			tp += 2;

			while (j < k)
			{
				tp[0] = fbtSwap16(tp[0]);
				tp[1] = fbtSwap16(tp[1]);

				++j;
				tp += 2;
			}
		}
		else
		{
			FBT_ASSERT( tp[1] < fbtMaxMember );
			m_type[tp[0]].m_strcId = m_strcNr - 1;
			m_typeFinder.insert(m_type[tp[0]].m_name, m_type[tp[0]]);

			tp += (2 * tp[1]) + 2;
		}

		++i;
	}

	if (m_strcNr == 0)
	{
		fbtFree(m_name);
		fbtFree(m_type);
		fbtFree(m_tlen);
		fbtFree(m_strc);

		m_name = 0;
		m_type = 0;
		m_tlen = 0;
		m_strc = 0;

		return false;
	}

	compile();
	return true;
}


void fbtBinTables::compile(FBTtype i, FBTtype nr, fbtStruct* off, FBTuint32& cof, FBTuint32 depth, fbtStruct::Keys& keys)
{
	FBTuint32 e, l, a, oof, ol;
	FBTuint16 f = m_strc[0][0];

	if (i > m_strcNr)
	{
		fbtPrintf("Missing recursive type\n");
		return;
	}


	for (a = 0; a < nr; ++a)
	{
		// Only calculate offsets on recursive structs
		// This saves undeded buffers
		FBTtype* strc = m_strc[i];

		oof = cof;
		ol = m_tlen[strc[0]];

		l = strc[1];
		strc += 2;

		for (e = 0; e < l; e++, strc += 2)
		{
			if (strc[0] >= f && m_name[strc[1]].m_ptrCount == 0)
			{
                fbtKey64 k = {{m_type[strc[0]].m_typeId, m_name[strc[1]].m_nameId}};
				keys.push_back(k);

				compile(m_type[strc[0]].m_strcId, m_name[strc[1]].m_arraySize, off, cof, depth+1, keys);

				keys.pop_back();
			}
			else
				putMember(strc, off, a, cof, depth, keys);
		}

		if ((cof - oof) != ol)
			fbtPrintf("Build ==> invalid offset (%i)(%i:%i)\n", a, (cof - oof), ol);
	
	}
}




void fbtBinTables::compile(void)
{
	m_offs.reserve(fbtMaxTable);

	if (!m_strc || m_strcNr <= 0)
	{
		fbtPrintf("Build ==> No structurs.");
		return;
	}

	FBTuint32 i, cof = 0, depth;
	FBTuint16 f = m_strc[0][0], e, memberCount;

	fbtStruct::Keys emptyKeys;
	for (i = 0; i < m_strcNr; i++)
	{
		FBTtype* strc = m_strc[i];

		FBTtype strcType = strc[0];

		depth = 0;
		cof = 0;
		fbtStruct* off = new fbtStruct;
		off->m_key.k16[0] = strcType;
		off->m_key.k16[1] = 0;
		off->m_val.k32[0] = m_type[strcType].m_typeId;
		off->m_val.k32[1] = 0; // no name
		off->m_nr         = 0;
		off->m_dp         = 0;
		off->m_off        = cof;
		off->m_len        = m_tlen[strcType];
		off->m_strcId     = i;
		off->m_link       = 0;
		off->m_flag       = fbtStruct::CAN_LINK;

		m_offs.push_back(off);

		memberCount = strc[1];

		strc += 2;
		off->m_members.reserve(fbtMaxMember);

		for (e = 0; e < memberCount; ++e, strc += 2)
		{
			if (strc[0] >= f && m_name[strc[1]].m_ptrCount == 0) //strc[0]:member_type, strc[1]:member_name
			{
				fbtStruct::Keys keys;
                fbtKey64 k = {{m_type[strc[0]].m_typeId, m_name[strc[1]].m_nameId}};
				keys.push_back(k);
				compile(m_type[strc[0]].m_strcId, m_name[strc[1]].m_arraySize, off, cof, depth+1, keys);				
			}
			else
				putMember(strc, off, 0, cof, 0, emptyKeys);
		}

        if ((int)cof != (int)off->m_len)
		{
			off->m_flag |= fbtStruct::MISALIGNED;
			fbtPrintf("Build ==> invalid offset %s:%i:%i:%i\n", m_type[off->m_key.k16[0]].m_name, i, cof, off->m_len);
		}

	}
}

void fbtBinTables::putMember(FBTtype* cp, fbtStruct* off, FBTtype nr, FBTuint32& cof, FBTuint32 depth, fbtStruct::Keys& keys)
{
	fbtStruct nof;
	nof.m_key.k16[0] = cp[0];
	nof.m_key.k16[1] = cp[1];
	nof.m_val.k32[0] = m_type[cp[0]].m_typeId;
	nof.m_val.k32[1] = m_base[cp[1]];
	nof.m_off        = cof;
	nof.m_strcId     = off->m_strcId;
	nof.m_nr         = nr;
	nof.m_dp         = depth;
	nof.m_link       = 0;
	nof.m_flag       = fbtStruct::CAN_LINK;
	nof.m_len        = (m_name[cp[1]].m_ptrCount ? m_ptr : m_tlen[cp[0]]) * m_name[cp[1]].m_arraySize;
	nof.m_keyChain   = keys;
	off->m_members.push_back(nof);
	cof += nof.m_len;

#ifdef _DEBUG
	//fbtPrintf("%s %s\n", getStructType(off), getStructName(off));
	//fbtPrintf("\t%s %s nr:%d cof:%d depth:%d\n", getStructType(&nof), getStructName(&nof), nr, cof, depth);
#endif
}


FBTtype fbtBinTables::findTypeId(const fbtCharHashKey &cp)
{
	FBTsizeType pos = m_typeFinder.find(cp);
	if (pos != FBT_NPOS)
		return m_typeFinder.at(pos).m_strcId;
	return -1;
}

const char* fbtBinTables::getStructType(const fbtStruct* strc)
{
	
	//return strc ? m_type[strc->m_key.k16[0]].m_name : "";

	FBTuint32 k = strc ? strc->m_key.k16[0] : (FBTuint32)-1;	
	return  (k >= m_typeNr) ? "" : m_type[k].m_name;
}

const char* fbtBinTables::getStructName(const fbtStruct* strc)
{	
	FBTuint32 k = strc ? strc->m_key.k16[1] : (FBTuint32)-1;	
	return  (k >= m_nameNr) ? "" : m_name[k].m_name;
}

const char* fbtBinTables::getOwnerStructName(const fbtStruct* strc)
{
	//cp0 = mp->m_type[mp->m_strc[c->m_strcId][0]].m_name;
	FBTuint32 k = strc ? strc->m_strcId : (FBTuint32)-1;
	return (k >= m_strcNr || *m_strc[k] >= m_typeNr) ? "" : m_type[*m_strc[k]].m_name;
}


// Source File "fbtTypes.cpp"
//#define FBT_IN_SOURCE
//#include "fbtPlatformHeaders.h"


// ----------------------------------------------------------------------------
// Debug Utilities


#ifdef FBT_DEBUG
bool fbtDebugger::isDebugger(void)
{
#if FBT_COMPILER == FBT_COMPILER_MSVC && _WIN32_WINNT >= 0x0400 && _MSC_VER>=1400
    return IsDebuggerPresent() != 0;
#else
	return false;
#endif
}

void fbtDebugger::breakProcess(void)
{
#if FBT_COMPILER == FBT_COMPILER_MSVC
	_asm int 3;
#else
	asm("int $3");
#endif
}

#else//FBT_DEBUG


bool fbtDebugger::isDebugger(void)
{
	return false;
}

void fbtDebugger::breakProcess(void)
{
}

#endif//FBT_DEBUG


#define FBT_DEBUG_BUF_SIZE (1024)
fbtDebugger::Reporter fbtDebugger::m_report = {0, 0};


void fbtDebugger::setReportHook(Reporter& hook)
{
	m_report.m_client = hook.m_client;
	m_report.m_hook   = hook.m_hook;
}


void fbtDebugger::report(const char* fmt, ...)
{
	char ReportBuf[FBT_DEBUG_BUF_SIZE+1];

	va_list lst;
	va_start(lst, fmt);
	int size = fbtp_printf(ReportBuf, FBT_DEBUG_BUF_SIZE, fmt, lst);
	va_end(lst);

	if (size < 0)
	{
		ReportBuf[FBT_DEBUG_BUF_SIZE] = 0;
		size = FBT_DEBUG_BUF_SIZE;
	}

	if (size > 0)
	{
		ReportBuf[size] = 0;

		if (m_report.m_hook)
		{
#if FBT_COMPILER == FBT_COMPILER_MSVC && _WIN32_WINNT >= 0x0400 && _MSC_VER>=1400
			if (IsDebuggerPresent())
				OutputDebugString(ReportBuf);

#endif
			m_report.m_hook(m_report.m_client, ReportBuf);
		}
		else
		{

#if FBT_COMPILER == FBT_COMPILER_MSVC && _WIN32_WINNT >= 0x0400 && _MSC_VER>=1400
			if (IsDebuggerPresent())
				OutputDebugString(ReportBuf);
			else
#endif
				fprintf(stderr, "%s", ReportBuf);
		}
	}

}


void fbtDebugger::reportIDE(const char* src, long line, const char* fmt, ...)
{
	static char ReportBuf[FBT_DEBUG_BUF_SIZE+1];

	va_list lst;
	va_start(lst, fmt);


	int size = fbtp_printf(ReportBuf, FBT_DEBUG_BUF_SIZE, fmt, lst);
	va_end(lst);

	if (size < 0)
	{
		ReportBuf[FBT_DEBUG_BUF_SIZE] = 0;
		size = FBT_DEBUG_BUF_SIZE;
	}

	if (size > 0)
	{
		ReportBuf[size] = 0;
#if FBT_COMPILER == FBT_COMPILER_MSVC
		report("%s(%i): warning: %s", src, line, ReportBuf);
#else
		report("%s:%i: warning: %s", src, line, ReportBuf);
#endif
	}
}


void fbtDebugger::errorIDE(const char* src, long line, const char* fmt, ...)
{
	static char ReportBuf[FBT_DEBUG_BUF_SIZE+1];

	va_list lst;
	va_start(lst, fmt);


	int size = fbtp_printf(ReportBuf, FBT_DEBUG_BUF_SIZE, fmt, lst);
	va_end(lst);

	if (size < 0)
	{
		ReportBuf[FBT_DEBUG_BUF_SIZE] = 0;
		size = FBT_DEBUG_BUF_SIZE;
	}

	if (size > 0)
	{
		ReportBuf[size] = 0;
#if FBT_COMPILER == FBT_COMPILER_MSVC
		report("%s(%i): error: %s", src, line, ReportBuf);
#else
		report("%s:%i: error: %s", src, line, ReportBuf);
#endif
	}
}

FBT_PRIM_TYPE fbtGetPrimType(FBTuint32 typeKey)
{
	static FBTuint32 charT    = fbtCharHashKey("char").hash();
	static FBTuint32 ucharT   = fbtCharHashKey("uchar").hash();
	static FBTuint32 shortT   = fbtCharHashKey("short").hash();
	static FBTuint32 ushortT  = fbtCharHashKey("ushort").hash();
	static FBTuint32 intT     = fbtCharHashKey("int").hash();
	static FBTuint32 longT    = fbtCharHashKey("long").hash();
	static FBTuint32 ulongT   = fbtCharHashKey("ulong").hash();
	static FBTuint32 floatT   = fbtCharHashKey("float").hash();
	static FBTuint32 doubleT  = fbtCharHashKey("double").hash();
	static FBTuint32 voidT    = fbtCharHashKey("void").hash();

	if (typeKey == charT)	return FBT_PRIM_CHAR;
	if (typeKey == ucharT)	return FBT_PRIM_UCHAR;
	if (typeKey == shortT)	return FBT_PRIM_SHORT;
	if (typeKey == ushortT)	return FBT_PRIM_USHORT;
	if (typeKey == intT)	return FBT_PRIM_INT;
	if (typeKey == longT)	return FBT_PRIM_LONG;
	if (typeKey == ulongT)	return FBT_PRIM_ULONG;
	if (typeKey == floatT)	return FBT_PRIM_FLOAT;
	if (typeKey == doubleT)	return FBT_PRIM_DOUBLE;
	if (typeKey == voidT)	return FBT_PRIM_VOID;

	return FBT_PRIM_UNKNOWN;
}


#endif //_fbtFile_imp_



const FBTuint32 GLOB = FBT_ID('G', 'L', 'O', 'B');

struct fbtIdDB
{
	const FBTuint16     m_code;
	fbtList             fbtBlend::*m_ptr;
};

// Warning: these globals are NOT updated automatically, but must be MANUALLY integrated (to reflect the content of source/blender/makesdna/DNA_ID_enums.h)
// It should be OK to use "integrated versions" also for older versions (newer fields are not present in their Blender DNA, and this should be OK). [DNA_ID_enums 2/2]
fbtIdDB fbtData[] =
{
	{ FBT_ID2('S', 'C'), &fbtBlend::m_scene},
	{ FBT_ID2('L', 'I'), &fbtBlend::m_library},
	{ FBT_ID2('O', 'B'), &fbtBlend::m_object},
	{ FBT_ID2('M', 'E'), &fbtBlend::m_mesh},
	{ FBT_ID2('C', 'U'), &fbtBlend::m_curve},	// ID_CV should be used in the future (see #95355).
	{ FBT_ID2('M', 'B'), &fbtBlend::m_mball},
	{ FBT_ID2('M', 'A'), &fbtBlend::m_mat},
	{ FBT_ID2('T', 'E'), &fbtBlend::m_tex},
	{ FBT_ID2('I', 'M'), &fbtBlend::m_image},
	{ FBT_ID2('L', 'T'), &fbtBlend::m_latt},
	{ FBT_ID2('L', 'A'), &fbtBlend::m_lamp},
	{ FBT_ID2('C', 'A'), &fbtBlend::m_camera},
	{ FBT_ID2('I', 'P'), &fbtBlend::m_ipo},
	{ FBT_ID2('K', 'E'), &fbtBlend::m_key},
	{ FBT_ID2('W', 'O'), &fbtBlend::m_world},
#	if BLENDER_VERSION<306	// not sure when the switch was made...
	{ FBT_ID2('S', 'N'), &fbtBlend::m_screen},
#	else
	{ FBT_ID2('S', 'R'), &fbtBlend::m_screen},
#	endif	
	{ FBT_ID2('P', 'Y'), &fbtBlend::m_script},
	{ FBT_ID2('V', 'F'), &fbtBlend::m_vfont},
	{ FBT_ID2('T', 'X'), &fbtBlend::m_text},
	{ FBT_ID2('S', 'K'), &fbtBlend::m_speaker},
	{ FBT_ID2('S', 'O'), &fbtBlend::m_sound},
	{ FBT_ID2('G', 'R'), &fbtBlend::m_group},
	{ FBT_ID2('A', 'R'), &fbtBlend::m_armature},
	{ FBT_ID2('A', 'C'), &fbtBlend::m_action},
	{ FBT_ID2('N', 'T'), &fbtBlend::m_nodetree},
	{ FBT_ID2('B', 'R'), &fbtBlend::m_brush},
	{ FBT_ID2('P', 'A'), &fbtBlend::m_particle},
	{ FBT_ID2('G', 'D'), &fbtBlend::m_gpencil},
	{ FBT_ID2('W', 'M'), &fbtBlend::m_wm},
	{ FBT_ID2('M', 'C'), &fbtBlend::m_movieClip},
	{ FBT_ID2('M', 'S'), &fbtBlend::m_mask},
	{ FBT_ID2('L', 'S'), &fbtBlend::m_freeStyleLineStyle},
	{ FBT_ID2('P', 'L'), &fbtBlend::m_palette},
	{ FBT_ID2('P', 'C'), &fbtBlend::m_paintCurve},
	{ FBT_ID2('C', 'F'), &fbtBlend::m_cacheFile},
	{ FBT_ID2('W', 'S'), &fbtBlend::m_workSpace},
	{ FBT_ID2('L', 'P'), &fbtBlend::m_lightProbe},
	{ FBT_ID2('H', 'A'), &fbtBlend::m_hair},
	{ FBT_ID2('C', 'V'), &fbtBlend::m_curves},
	{ FBT_ID2('P', 'T'), &fbtBlend::m_pointCloud},
	{ FBT_ID2('V', 'O'), &fbtBlend::m_volume},
	{ FBT_ID2('S', 'I'), &fbtBlend::m_simulation},
	{ 0, 0 }
};




extern unsigned char bfBlenderFBT[];
extern int bfBlenderLen;


fbtBlend::fbtBlend()
	:   fbtFile("BLENDER"), m_stripList(0)
{
	m_aluhid = "BLENDEs"; //a stripped blend file
}



fbtBlend::~fbtBlend()
{
}



int fbtBlend::initializeTables(fbtBinTables* tables)
{
	return tables->read(bfBlenderFBT, bfBlenderLen, false) ? FS_OK : FS_FAILED;
}



int fbtBlend::notifyData(void* p, const Chunk& id)
{
	if (id.m_code == GLOB)
	{
		m_fg = (Blender::FileGlobal*)p;
		return FS_OK;
	}

	if ((id.m_code <= 0xFFFF))
	{
		int i = 0;
		while (fbtData[i].m_code != 0)
		{
			if (fbtData[i].m_code == id.m_code)
			{
				(this->*fbtData[i].m_ptr).push_back(p);
				break;
			}
			++i;
		}
	}
	return FS_OK;
}


int fbtBlend::writeData(fbtStream* stream)
{
	fbtBinTables::OffsM::Pointer md = m_memory->m_offs.ptr();


	for (MemoryChunk* node = (MemoryChunk*)m_chunks.first; node; node = node->m_next)
	{
		if (node->m_newTypeId > m_memory->m_strcNr)
			continue;
		if (!node->m_newBlock)
			continue;

		void* wd = node->m_newBlock;

		Chunk ch;
		ch.m_code   = node->m_chunk.m_code;
		ch.m_nr     = node->m_chunk.m_nr;
		ch.m_len    = node->m_chunk.m_len;
		ch.m_typeid = node->m_newTypeId;
		ch.m_old    = (FBTsize)wd;

		stream->write(&ch, sizeof(Chunk));
		stream->write(wd, ch.m_len);
	}

	return FS_OK;
}



bool fbtBlend::skip(const FBTuint32& id)
{

	if (!m_stripList)
		return false;

	int i = 0;
	while (m_stripList[i] != 0)
	{
		if (m_stripList[i++] == id)
			return true;
	}

	return false;
}


void*   fbtBlend::getFBT(void)
{
	return (void*)bfBlenderFBT;
}

FBTsize fbtBlend::getFBTlength(void)
{
	return bfBlenderLen;
}

int fbtBlend::save(const char *path, const int mode)
{
	m_version = m_fileVersion;
	return reflect(path, mode);
}


// bfBlender.cpp: THIS DEPENDS ON THE BLENDER VERSION! =========================================

// Generated using BLENDER-v279
unsigned char bfBlenderFBT[] = {
83,68,78,65,78,65,77,69,194,16,0,0,42,110,101,120,116,0,42,112,114,101,118,0,42,100,97,116,97,0,42,102,105,114,115,116,0,42,108,97,115,116,0,120,0,121,0,122,0,120,109,105,110,0,120,109,97,120,0,121,109,105,110,0,121,109,97,120,0,42,112,111,105,110,116,101,114,0,103,114,111,117,112,0,118,97,108,0,118,97,108,50,0,116,121,112,101,0,115,117,98,116,121,112,101,0,102,108,97,103,0,110,97,109,101,91,54,52,93,0,
115,97,118,101,100,0,100,97,116,97,0,108,101,110,0,116,111,116,97,108,108,101,110,0,42,110,101,119,105,100,0,42,108,105,98,0,110,97,109,101,91,54,54,93,0,116,97,103,0,112,97,100,95,115,49,0,117,115,0,105,99,111,110,95,105,100,0,42,112,114,111,112,101,114,116,105,101,115,0,105,100,0,42,102,105,108,101,100,97,116,97,0,110,97,109,101,91,49,48,50,52,93,0,102,105,108,101,112,97,116,104,91,49,48,50,52,93,0,42,112,
97,114,101,110,116,0,42,112,97,99,107,101,100,102,105,108,101,0,116,101,109,112,95,105,110,100,101,120,0,118,101,114,115,105,111,110,102,105,108,101,0,115,117,98,118,101,114,115,105,111,110,102,105,108,101,0,119,91,50,93,0,104,91,50,93,0,102,108,97,103,91,50,93,0,99,104,97,110,103,101,100,95,116,105,109,101,115,116,97,109,112,91,50,93,0,42,114,101,99,116,91,50,93,0,42,103,112,117,116,101,120,116,117,114,101,91,50,93,0,112,
97,100,91,50,93,0,42,111,98,0,98,108,111,99,107,116,121,112,101,0,97,100,114,99,111,100,101,0,110,97,109,101,91,49,50,56,93,0,42,98,112,0,42,98,101,122,116,0,109,97,120,114,99,116,0,116,111,116,114,99,116,0,118,97,114,116,121,112,101,0,116,111,116,118,101,114,116,0,105,112,111,0,101,120,116,114,97,112,0,114,116,0,98,105,116,109,97,115,107,0,115,108,105,100,101,95,109,105,110,0,115,108,105,100,101,95,109,97,120,0,
99,117,114,118,97,108,0,42,100,114,105,118,101,114,0,99,117,114,118,101,0,99,117,114,0,115,104,111,119,107,101,121,0,109,117,116,101,105,112,111,0,112,97,100,0,112,111,115,0,112,97,100,49,0,114,101,108,97,116,105,118,101,0,116,111,116,101,108,101,109,0,117,105,100,0,118,103,114,111,117,112,91,54,52,93,0,115,108,105,100,101,114,109,105,110,0,115,108,105,100,101,114,109,97,120,0,42,97,100,116,0,42,114,101,102,107,101,121,0,101,
108,101,109,115,116,114,91,51,50,93,0,101,108,101,109,115,105,122,101,0,98,108,111,99,107,0,42,105,112,111,0,42,102,114,111,109,0,116,111,116,107,101,121,0,112,97,100,50,0,99,116,105,109,101,0,117,105,100,103,101,110,0,42,108,105,110,101,0,42,102,111,114,109,97,116,0,98,108,101,110,0,42,110,97,109,101,0,102,108,97,103,115,0,110,108,105,110,101,115,0,108,105,110,101,115,0,42,99,117,114,108,0,42,115,101,108,108,0,99,117,
114,99,0,115,101,108,99,0,42,117,110,100,111,95,98,117,102,0,117,110,100,111,95,112,111,115,0,117,110,100,111,95,108,101,110,0,42,99,111,109,112,105,108,101,100,0,109,116,105,109,101,0,115,105,122,101,0,115,101,101,107,0,102,111,99,117,115,95,100,105,115,116,97,110,99,101,0,102,115,116,111,112,0,102,111,99,97,108,95,108,101,110,103,116,104,0,115,101,110,115,111,114,0,110,117,109,95,98,108,97,100,101,115,0,104,105,103,104,95,113,
117,97,108,105,116,121,0,102,97,99,116,111,114,0,99,111,108,111,114,91,51,93,0,100,105,115,116,97,110,99,101,95,109,97,120,0,97,116,116,101,110,117,97,116,105,111,110,0,115,97,109,112,108,101,115,0,42,100,111,102,0,42,115,115,97,111,0,102,120,95,102,108,97,103,0,112,97,100,91,55,93,0,105,110,116,101,114,111,99,117,108,97,114,95,100,105,115,116,97,110,99,101,0,99,111,110,118,101,114,103,101,110,99,101,95,100,105,115,116,97,
110,99,101,0,99,111,110,118,101,114,103,101,110,99,101,95,109,111,100,101,0,112,105,118,111,116,0,112,111,108,101,95,109,101,114,103,101,95,97,110,103,108,101,95,102,114,111,109,0,112,111,108,101,95,109,101,114,103,101,95,97,110,103,108,101,95,116,111,0,100,116,120,0,112,97,115,115,101,112,97,114,116,97,108,112,104,97,0,99,108,105,112,115,116,97,0,99,108,105,112,101,110,100,0,108,101,110,115,0,111,114,116,104,111,95,115,99,97,108,101,
0,100,114,97,119,115,105,122,101,0,115,101,110,115,111,114,95,120,0,115,101,110,115,111,114,95,121,0,115,104,105,102,116,120,0,115,104,105,102,116,121,0,89,70,95,100,111,102,100,105,115,116,0,42,100,111,102,95,111,98,0,103,112,117,95,100,111,102,0,115,101,110,115,111,114,95,102,105,116,0,115,116,101,114,101,111,0,42,115,99,101,110,101,0,102,114,97,109,101,110,114,0,102,114,97,109,101,115,0,111,102,102,115,101,116,0,115,102,114,97,
0,102,105,101,95,105,109,97,0,99,121,99,108,0,111,107,0,109,117,108,116,105,118,105,101,119,95,101,121,101,0,112,97,115,115,0,109,117,108,116,105,95,105,110,100,101,120,0,118,105,101,119,0,108,97,121,101,114,0,42,97,110,105,109,0,42,99,97,99,104,101,0,97,110,105,109,115,0,42,114,114,0,42,114,101,110,100,101,114,115,91,56,93,0,114,101,110,100,101,114,95,115,108,111,116,0,108,97,115,116,95,114,101,110,100,101,114,95,115,108,
111,116,0,115,111,117,114,99,101,0,108,97,115,116,102,114,97,109,101,0,116,112,97,103,101,102,108,97,103,0,116,111,116,98,105,110,100,0,120,114,101,112,0,121,114,101,112,0,116,119,115,116,97,0,116,119,101,110,100,0,98,105,110,100,99,111,100,101,91,50,93,0,112,97,100,49,91,52,93,0,42,114,101,112,98,105,110,100,0,112,97,99,107,101,100,102,105,108,101,115,0,42,112,114,101,118,105,101,119,0,108,97,115,116,117,112,100,97,116,101,
0,108,97,115,116,117,115,101,100,0,97,110,105,109,115,112,101,101,100,0,103,101,110,95,120,0,103,101,110,95,121,0,103,101,110,95,116,121,112,101,0,103,101,110,95,102,108,97,103,0,103,101,110,95,100,101,112,116,104,0,103,101,110,95,99,111,108,111,114,91,52,93,0,97,115,112,120,0,97,115,112,121,0,99,111,108,111,114,115,112,97,99,101,95,115,101,116,116,105,110,103,115,0,97,108,112,104,97,95,109,111,100,101,0,112,97,100,91,53,93,
0,101,121,101,0,118,105,101,119,115,95,102,111,114,109,97,116,0,118,105,101,119,115,0,42,115,116,101,114,101,111,51,100,95,102,111,114,109,97,116,0,114,101,110,100,101,114,95,115,108,111,116,115,91,56,93,0,116,101,120,99,111,0,109,97,112,116,111,0,109,97,112,116,111,110,101,103,0,98,108,101,110,100,116,121,112,101,0,42,111,98,106,101,99,116,0,42,116,101,120,0,117,118,110,97,109,101,91,54,52,93,0,112,114,111,106,120,0,112,114,
111,106,121,0,112,114,111,106,122,0,109,97,112,112,105,110,103,0,98,114,117,115,104,95,109,97,112,95,109,111,100,101,0,98,114,117,115,104,95,97,110,103,108,101,95,109,111,100,101,0,111,102,115,91,51,93,0,115,105,122,101,91,51,93,0,114,111,116,0,114,97,110,100,111,109,95,97,110,103,108,101,0,116,101,120,102,108,97,103,0,99,111,108,111,114,109,111,100,101,108,0,112,109,97,112,116,111,0,112,109,97,112,116,111,110,101,103,0,110,111,
114,109,97,112,115,112,97,99,101,0,119,104,105,99,104,95,111,117,116,112,117,116,0,114,0,103,0,98,0,107,0,100,101,102,95,118,97,114,0,99,111,108,102,97,99,0,118,97,114,102,97,99,0,110,111,114,102,97,99,0,100,105,115,112,102,97,99,0,119,97,114,112,102,97,99,0,99,111,108,115,112,101,99,102,97,99,0,109,105,114,114,102,97,99,0,97,108,112,104,97,102,97,99,0,100,105,102,102,102,97,99,0,115,112,101,99,102,97,99,0,
101,109,105,116,102,97,99,0,104,97,114,100,102,97,99,0,114,97,121,109,105,114,114,102,97,99,0,116,114,97,110,115,108,102,97,99,0,97,109,98,102,97,99,0,99,111,108,101,109,105,116,102,97,99,0,99,111,108,114,101,102,108,102,97,99,0,99,111,108,116,114,97,110,115,102,97,99,0,100,101,110,115,102,97,99,0,115,99,97,116,116,101,114,102,97,99,0,114,101,102,108,102,97,99,0,116,105,109,101,102,97,99,0,108,101,110,103,116,104,102,
97,99,0,99,108,117,109,112,102,97,99,0,100,97,109,112,102,97,99,0,107,105,110,107,102,97,99,0,107,105,110,107,97,109,112,102,97,99,0,114,111,117,103,104,102,97,99,0,112,97,100,101,110,115,102,97,99,0,103,114,97,118,105,116,121,102,97,99,0,108,105,102,101,102,97,99,0,115,105,122,101,102,97,99,0,105,118,101,108,102,97,99,0,102,105,101,108,100,102,97,99,0,115,104,97,100,111,119,102,97,99,0,122,101,110,117,112,102,97,99,
0,122,101,110,100,111,119,110,102,97,99,0,98,108,101,110,100,102,97,99,0,97,0,116,111,116,0,105,112,111,116,121,112,101,0,105,112,111,116,121,112,101,95,104,117,101,0,99,111,108,111,114,95,109,111,100,101,0,112,97,100,91,49,93,0,100,97,116,97,91,51,50,93,0,42,105,109,97,0,42,99,117,98,101,91,54,93,0,105,109,97,116,91,52,93,91,52,93,0,111,98,105,109,97,116,91,51,93,91,51,93,0,115,116,121,112,101,0,118,105,
101,119,115,99,97,108,101,0,110,111,116,108,97,121,0,99,117,98,101,114,101,115,0,100,101,112,116,104,0,114,101,99,97,108,99,0,108,97,115,116,115,105,122,101,0,102,97,108,108,111,102,102,95,116,121,112,101,0,102,97,108,108,111,102,102,95,115,111,102,116,110,101,115,115,0,114,97,100,105,117,115,0,112,97,100,48,0,99,111,108,111,114,95,115,111,117,114,99,101,0,111,98,95,99,111,108,111,114,95,115,111,117,114,99,101,0,116,111,116,112,
111,105,110,116,115,0,112,115,121,115,0,112,115,121,115,95,99,97,99,104,101,95,115,112,97,99,101,0,111,98,95,99,97,99,104,101,95,115,112,97,99,101,0,118,101,114,116,101,120,95,97,116,116,114,105,98,117,116,101,95,110,97,109,101,91,54,52,93,0,42,112,111,105,110,116,95,116,114,101,101,0,42,112,111,105,110,116,95,100,97,116,97,0,110,111,105,115,101,95,115,105,122,101,0,110,111,105,115,101,95,100,101,112,116,104,0,110,111,105,115,
101,95,105,110,102,108,117,101,110,99,101,0,110,111,105,115,101,95,98,97,115,105,115,0,112,97,100,49,91,51,93,0,110,111,105,115,101,95,102,97,99,0,115,112,101,101,100,95,115,99,97,108,101,0,102,97,108,108,111,102,102,95,115,112,101,101,100,95,115,99,97,108,101,0,42,99,111,98,97,0,42,102,97,108,108,111,102,102,95,99,117,114,118,101,0,114,101,115,111,108,91,51,93,0,105,110,116,101,114,112,95,116,121,112,101,0,102,105,108,101,
95,102,111,114,109,97,116,0,101,120,116,101,110,100,0,115,109,111,107,101,100,95,116,121,112,101,0,104,97,105,114,95,116,121,112,101,0,100,97,116,97,95,116,121,112,101,0,95,112,97,100,0,105,110,116,95,109,117,108,116,105,112,108,105,101,114,0,115,116,105,108,108,95,102,114,97,109,101,0,115,111,117,114,99,101,95,112,97,116,104,91,49,48,50,52,93,0,42,100,97,116,97,115,101,116,0,99,97,99,104,101,100,102,114,97,109,101,0,111,99,
101,97,110,109,111,100,91,54,52,93,0,111,117,116,112,117,116,0,110,111,105,115,101,115,105,122,101,0,116,117,114,98,117,108,0,98,114,105,103,104,116,0,99,111,110,116,114,97,115,116,0,115,97,116,117,114,97,116,105,111,110,0,114,102,97,99,0,103,102,97,99,0,98,102,97,99,0,102,105,108,116,101,114,115,105,122,101,0,109,103,95,72,0,109,103,95,108,97,99,117,110,97,114,105,116,121,0,109,103,95,111,99,116,97,118,101,115,0,109,103,
95,111,102,102,115,101,116,0,109,103,95,103,97,105,110,0,100,105,115,116,95,97,109,111,117,110,116,0,110,115,95,111,117,116,115,99,97,108,101,0,118,110,95,119,49,0,118,110,95,119,50,0,118,110,95,119,51,0,118,110,95,119,52,0,118,110,95,109,101,120,112,0,118,110,95,100,105,115,116,109,0,118,110,95,99,111,108,116,121,112,101,0,110,111,105,115,101,100,101,112,116,104,0,110,111,105,115,101,116,121,112,101,0,110,111,105,115,101,98,97,
115,105,115,0,110,111,105,115,101,98,97,115,105,115,50,0,105,109,97,102,108,97,103,0,99,114,111,112,120,109,105,110,0,99,114,111,112,121,109,105,110,0,99,114,111,112,120,109,97,120,0,99,114,111,112,121,109,97,120,0,116,101,120,102,105,108,116,101,114,0,97,102,109,97,120,0,120,114,101,112,101,97,116,0,121,114,101,112,101,97,116,0,99,104,101,99,107,101,114,100,105,115,116,0,110,97,98,108,97,0,105,117,115,101,114,0,42,110,111,100,
101,116,114,101,101,0,42,101,110,118,0,42,112,100,0,42,118,100,0,42,111,116,0,117,115,101,95,110,111,100,101,115,0,108,111,99,91,51,93,0,114,111,116,91,51,93,0,109,97,116,91,52,93,91,52,93,0,109,105,110,91,51,93,0,109,97,120,91,51,93,0,99,111,98,97,0,98,108,101,110,100,95,99,111,108,111,114,91,51,93,0,98,108,101,110,100,95,102,97,99,116,111,114,0,98,108,101,110,100,95,116,121,112,101,0,112,97,100,91,51,
93,0,109,111,100,101,0,116,111,116,101,120,0,115,104,100,119,114,0,115,104,100,119,103,0,115,104,100,119,98,0,115,104,100,119,112,97,100,0,101,110,101,114,103,121,0,100,105,115,116,0,115,112,111,116,115,105,122,101,0,115,112,111,116,98,108,101,110,100,0,104,97,105,110,116,0,97,116,116,49,0,97,116,116,50,0,99,111,101,102,102,95,99,111,110,115,116,0,99,111,101,102,102,95,108,105,110,0,99,111,101,102,102,95,113,117,97,100,0,99,
111,101,102,102,95,112,97,100,0,42,99,117,114,102,97,108,108,111,102,102,0,98,105,97,115,0,115,111,102,116,0,99,111,109,112,114,101,115,115,116,104,114,101,115,104,0,98,108,101,101,100,98,105,97,115,0,112,97,100,53,0,98,117,102,115,105,122,101,0,115,97,109,112,0,98,117,102,102,101,114,115,0,102,105,108,116,101,114,116,121,112,101,0,98,117,102,102,108,97,103,0,98,117,102,116,121,112,101,0,114,97,121,95,115,97,109,112,0,114,97,
121,95,115,97,109,112,121,0,114,97,121,95,115,97,109,112,122,0,114,97,121,95,115,97,109,112,95,116,121,112,101,0,97,114,101,97,95,115,104,97,112,101,0,97,114,101,97,95,115,105,122,101,0,97,114,101,97,95,115,105,122,101,121,0,97,114,101,97,95,115,105,122,101,122,0,97,100,97,112,116,95,116,104,114,101,115,104,0,114,97,121,95,115,97,109,112,95,109,101,116,104,111,100,0,115,104,97,100,111,119,109,97,112,95,116,121,112,101,0,116,
101,120,97,99,116,0,115,104,97,100,104,97,108,111,115,116,101,112,0,115,117,110,95,101,102,102,101,99,116,95,116,121,112,101,0,115,107,121,98,108,101,110,100,116,121,112,101,0,104,111,114,105,122,111,110,95,98,114,105,103,104,116,110,101,115,115,0,115,112,114,101,97,100,0,115,117,110,95,98,114,105,103,104,116,110,101,115,115,0,115,117,110,95,115,105,122,101,0,98,97,99,107,115,99,97,116,116,101,114,101,100,95,108,105,103,104,116,0,115,117,
110,95,105,110,116,101,110,115,105,116,121,0,97,116,109,95,116,117,114,98,105,100,105,116,121,0,97,116,109,95,105,110,115,99,97,116,116,101,114,105,110,103,95,102,97,99,116,111,114,0,97,116,109,95,101,120,116,105,110,99,116,105,111,110,95,102,97,99,116,111,114,0,97,116,109,95,100,105,115,116,97,110,99,101,95,102,97,99,116,111,114,0,115,107,121,98,108,101,110,100,102,97,99,0,115,107,121,95,101,120,112,111,115,117,114,101,0,115,104,97,
100,111,119,95,102,114,117,115,116,117,109,95,115,105,122,101,0,115,107,121,95,99,111,108,111,114,115,112,97,99,101,0,112,97,100,52,91,50,93,0,42,109,116,101,120,91,49,56,93,0,112,114,95,116,101,120,116,117,114,101,0,112,97,100,54,91,52,93,0,100,101,110,115,105,116,121,0,101,109,105,115,115,105,111,110,0,115,99,97,116,116,101,114,105,110,103,0,114,101,102,108,101,99,116,105,111,110,0,101,109,105,115,115,105,111,110,95,99,111,108,
91,51,93,0,116,114,97,110,115,109,105,115,115,105,111,110,95,99,111,108,91,51,93,0,114,101,102,108,101,99,116,105,111,110,95,99,111,108,91,51,93,0,100,101,110,115,105,116,121,95,115,99,97,108,101,0,100,101,112,116,104,95,99,117,116,111,102,102,0,97,115,121,109,109,101,116,114,121,0,115,116,101,112,115,105,122,101,95,116,121,112,101,0,115,104,97,100,101,102,108,97,103,0,115,104,97,100,101,95,116,121,112,101,0,112,114,101,99,97,99,
104,101,95,114,101,115,111,108,117,116,105,111,110,0,115,116,101,112,115,105,122,101,0,109,115,95,100,105,102,102,0,109,115,95,105,110,116,101,110,115,105,116,121,0,109,115,95,115,112,114,101,97,100,0,97,108,112,104,97,95,98,108,101,110,100,0,102,97,99,101,95,111,114,105,101,110,116,97,116,105,111,110,0,42,117,118,110,97,109,101,0,105,110,100,101,120,0,109,97,116,101,114,105,97,108,95,116,121,112,101,0,115,112,101,99,114,0,115,112,101,
99,103,0,115,112,101,99,98,0,109,105,114,114,0,109,105,114,103,0,109,105,114,98,0,97,109,98,114,0,97,109,98,98,0,97,109,98,103,0,97,109,98,0,101,109,105,116,0,97,110,103,0,115,112,101,99,116,114,97,0,114,97,121,95,109,105,114,114,111,114,0,97,108,112,104,97,0,114,101,102,0,115,112,101,99,0,122,111,102,102,115,0,97,100,100,0,116,114,97,110,115,108,117,99,101,110,99,121,0,118,111,108,0,103,97,109,101,0,102,114,
101,115,110,101,108,95,109,105,114,0,102,114,101,115,110,101,108,95,109,105,114,95,105,0,102,114,101,115,110,101,108,95,116,114,97,0,102,114,101,115,110,101,108,95,116,114,97,95,105,0,102,105,108,116,101,114,0,116,120,95,108,105,109,105,116,0,116,120,95,102,97,108,108,111,102,102,0,114,97,121,95,100,101,112,116,104,0,114,97,121,95,100,101,112,116,104,95,116,114,97,0,104,97,114,0,115,101,101,100,49,0,115,101,101,100,50,0,103,108,111,
115,115,95,109,105,114,0,103,108,111,115,115,95,116,114,97,0,115,97,109,112,95,103,108,111,115,115,95,109,105,114,0,115,97,109,112,95,103,108,111,115,115,95,116,114,97,0,97,100,97,112,116,95,116,104,114,101,115,104,95,109,105,114,0,97,100,97,112,116,95,116,104,114,101,115,104,95,116,114,97,0,97,110,105,115,111,95,103,108,111,115,115,95,109,105,114,0,100,105,115,116,95,109,105,114,0,102,97,100,101,116,111,95,109,105,114,0,115,104,97,
100,101,95,102,108,97,103,0,109,111,100,101,95,108,0,109,111,100,101,50,0,109,111,100,101,50,95,108,0,102,108,97,114,101,99,0,115,116,97,114,99,0,108,105,110,101,99,0,114,105,110,103,99,0,104,97,115,105,122,101,0,102,108,97,114,101,115,105,122,101,0,115,117,98,115,105,122,101,0,102,108,97,114,101,98,111,111,115,116,0,115,116,114,97,110,100,95,115,116,97,0,115,116,114,97,110,100,95,101,110,100,0,115,116,114,97,110,100,95,101,
97,115,101,0,115,116,114,97,110,100,95,115,117,114,102,110,111,114,0,115,116,114,97,110,100,95,109,105,110,0,115,116,114,97,110,100,95,119,105,100,116,104,102,97,100,101,0,115,116,114,97,110,100,95,117,118,110,97,109,101,91,54,52,93,0,115,98,105,97,115,0,108,98,105,97,115,0,115,104,97,100,95,97,108,112,104,97,0,115,101,112,116,101,120,0,114,103,98,115,101,108,0,112,114,95,116,121,112,101,0,112,114,95,108,97,109,112,0,109,108,
95,102,108,97,103,0,109,97,112,102,108,97,103,0,100,105,102,102,95,115,104,97,100,101,114,0,115,112,101,99,95,115,104,97,100,101,114,0,114,111,117,103,104,110,101,115,115,0,114,101,102,114,97,99,0,112,97,114,97,109,91,52,93,0,114,109,115,0,100,97,114,107,110,101,115,115,0,42,114,97,109,112,95,99,111,108,0,42,114,97,109,112,95,115,112,101,99,0,114,97,109,112,105,110,95,99,111,108,0,114,97,109,112,105,110,95,115,112,101,99,
0,114,97,109,112,98,108,101,110,100,95,99,111,108,0,114,97,109,112,98,108,101,110,100,95,115,112,101,99,0,114,97,109,112,95,115,104,111,119,0,112,97,100,51,0,114,97,109,112,102,97,99,95,99,111,108,0,114,97,109,112,102,97,99,95,115,112,101,99,0,42,103,114,111,117,112,0,102,114,105,99,116,105,111,110,0,102,104,0,114,101,102,108,101,99,116,0,102,104,100,105,115,116,0,120,121,102,114,105,99,116,0,100,121,110,97,109,111,100,101,
0,115,115,115,95,114,97,100,105,117,115,91,51,93,0,115,115,115,95,99,111,108,91,51,93,0,115,115,115,95,101,114,114,111,114,0,115,115,115,95,115,99,97,108,101,0,115,115,115,95,105,111,114,0,115,115,115,95,99,111,108,102,97,99,0,115,115,115,95,116,101,120,102,97,99,0,115,115,115,95,102,114,111,110,116,0,115,115,115,95,98,97,99,107,0,115,115,115,95,102,108,97,103,0,115,115,115,95,112,114,101,115,101,116,0,109,97,112,116,111,
95,116,101,120,116,117,114,101,100,0,115,104,97,100,111,119,111,110,108,121,95,102,108,97,103,0,108,105,110,101,95,99,111,108,91,52,93,0,108,105,110,101,95,112,114,105,111,114,105,116,121,0,118,99,111,108,95,97,108,112,104,97,0,112,97,105,110,116,95,97,99,116,105,118,101,95,115,108,111,116,0,112,97,105,110,116,95,99,108,111,110,101,95,115,108,111,116,0,116,111,116,95,115,108,111,116,115,0,112,97,100,52,91,51,93,0,110,109,97,112,
95,116,97,110,103,101,110,116,95,110,97,109,101,115,91,57,93,91,54,52,93,0,110,109,97,112,95,116,97,110,103,101,110,116,95,110,97,109,101,115,95,99,111,117,110,116,0,42,116,101,120,112,97,105,110,116,115,108,111,116,0,103,112,117,109,97,116,101,114,105,97,108,0,42,116,101,109,112,95,112,102,0,42,98,98,0,115,101,108,99,111,108,49,0,115,101,108,99,111,108,50,0,113,117,97,116,91,52,93,0,101,120,112,120,0,101,120,112,121,0,
101,120,112,122,0,114,97,100,0,114,97,100,50,0,115,0,42,109,97,116,0,42,105,109,97,116,0,101,108,101,109,115,0,100,105,115,112,0,42,101,100,105,116,101,108,101,109,115,0,42,42,109,97,116,0,102,108,97,103,50,0,116,111,116,99,111,108,0,119,105,114,101,115,105,122,101,0,114,101,110,100,101,114,115,105,122,101,0,116,104,114,101,115,104,0,42,108,97,115,116,101,108,101,109,0,118,101,99,91,51,93,91,51,93,0,97,108,102,97,0,
119,101,105,103,104,116,0,104,49,0,104,50,0,102,49,0,102,50,0,102,51,0,104,105,100,101,0,101,97,115,105,110,103,0,98,97,99,107,0,97,109,112,108,105,116,117,100,101,0,112,101,114,105,111,100,0,112,97,100,91,52,93,0,118,101,99,91,52,93,0,109,97,116,95,110,114,0,112,110,116,115,117,0,112,110,116,115,118,0,114,101,115,111,108,117,0,114,101,115,111,108,118,0,111,114,100,101,114,117,0,111,114,100,101,114,118,0,102,108,97,
103,117,0,102,108,97,103,118,0,42,107,110,111,116,115,117,0,42,107,110,111,116,115,118,0,116,105,108,116,95,105,110,116,101,114,112,0,114,97,100,105,117,115,95,105,110,116,101,114,112,0,99,104,97,114,105,100,120,0,107,101,114,110,0,119,0,104,0,110,117,114,98,115,0,42,107,101,121,105,110,100,101,120,0,115,104,97,112,101,110,114,0,110,117,114,98,0,42,101,100,105,116,110,117,114,98,0,42,98,101,118,111,98,106,0,42,116,97,112,101,
114,111,98,106,0,42,116,101,120,116,111,110,99,117,114,118,101,0,42,107,101,121,0,100,114,97,119,102,108,97,103,0,116,119,105,115,116,95,109,111,100,101,0,116,119,105,115,116,95,115,109,111,111,116,104,0,115,109,97,108,108,99,97,112,115,95,115,99,97,108,101,0,112,97,116,104,108,101,110,0,98,101,118,114,101,115,111,108,0,119,105,100,116,104,0,101,120,116,49,0,101,120,116,50,0,114,101,115,111,108,117,95,114,101,110,0,114,101,115,111,
108,118,95,114,101,110,0,97,99,116,110,117,0,97,99,116,118,101,114,116,0,115,112,97,99,101,109,111,100,101,0,97,108,105,103,110,95,121,0,115,112,97,99,105,110,103,0,108,105,110,101,100,105,115,116,0,115,104,101,97,114,0,102,115,105,122,101,0,119,111,114,100,115,112,97,99,101,0,117,108,112,111,115,0,117,108,104,101,105,103,104,116,0,120,111,102,0,121,111,102,0,108,105,110,101,119,105,100,116,104,0,115,101,108,115,116,97,114,116,0,
115,101,108,101,110,100,0,108,101,110,95,119,99,104,97,114,0,42,115,116,114,0,42,101,100,105,116,102,111,110,116,0,102,97,109,105,108,121,91,54,52,93,0,42,118,102,111,110,116,0,42,118,102,111,110,116,98,0,42,118,102,111,110,116,105,0,42,118,102,111,110,116,98,105,0,42,116,98,0,116,111,116,98,111,120,0,97,99,116,98,111,120,0,42,115,116,114,105,110,102,111,0,99,117,114,105,110,102,111,0,98,101,118,102,97,99,49,0,98,101,
118,102,97,99,50,0,98,101,118,102,97,99,49,95,109,97,112,112,105,110,103,0,98,101,118,102,97,99,50,95,109,97,112,112,105,110,103,0,112,97,100,50,91,50,93,0,42,109,115,101,108,101,99,116,0,42,109,112,111,108,121,0,42,109,116,112,111,108,121,0,42,109,108,111,111,112,0,42,109,108,111,111,112,117,118,0,42,109,108,111,111,112,99,111,108,0,42,109,102,97,99,101,0,42,109,116,102,97,99,101,0,42,116,102,97,99,101,0,42,109,
118,101,114,116,0,42,109,101,100,103,101,0,42,100,118,101,114,116,0,42,109,99,111,108,0,42,116,101,120,99,111,109,101,115,104,0,42,101,100,105,116,95,98,116,109,101,115,104,0,118,100,97,116,97,0,101,100,97,116,97,0,102,100,97,116,97,0,112,100,97,116,97,0,108,100,97,116,97,0,116,111,116,101,100,103,101,0,116,111,116,102,97,99,101,0,116,111,116,115,101,108,101,99,116,0,116,111,116,112,111,108,121,0,116,111,116,108,111,111,112,
0,97,99,116,95,102,97,99,101,0,115,109,111,111,116,104,114,101,115,104,0,99,100,95,102,108,97,103,0,115,117,98,100,105,118,0,115,117,98,100,105,118,114,0,115,117,98,115,117,114,102,116,121,112,101,0,101,100,105,116,102,108,97,103,0,42,109,114,0,42,116,112,97,103,101,0,117,118,91,52,93,91,50,93,0,99,111,108,91,52,93,0,116,114,97,110,115,112,0,116,105,108,101,0,117,110,119,114,97,112,0,118,49,0,118,50,0,118,51,0,
118,52,0,101,100,99,111,100,101,0,99,114,101,97,115,101,0,98,119,101,105,103,104,116,0,100,101,102,95,110,114,0,42,100,119,0,116,111,116,119,101,105,103,104,116,0,99,111,91,51,93,0,110,111,91,51,93,0,108,111,111,112,115,116,97,114,116,0,118,0,101,0,117,118,91,50,93,0,102,0,105,0,115,91,50,53,53,93,0,115,95,108,101,110,0,116,111,116,100,105,115,112,0,108,101,118,101,108,0,40,42,100,105,115,112,115,41,40,41,0,
42,104,105,100,100,101,110,0,118,91,52,93,0,109,105,100,0,118,91,50,93,0,42,102,97,99,101,115,0,42,99,111,108,102,97,99,101,115,0,42,101,100,103,101,115,0,42,118,101,114,116,115,0,108,101,118,101,108,115,0,108,101,118,101,108,95,99,111,117,110,116,0,99,117,114,114,101,110,116,0,110,101,119,108,118,108,0,101,100,103,101,108,118,108,0,112,105,110,108,118,108,0,114,101,110,100,101,114,108,118,108,0,117,115,101,95,99,111,108,0,
42,101,100,103,101,95,102,108,97,103,115,0,42,101,100,103,101,95,99,114,101,97,115,101,115,0,114,97,100,105,117,115,91,51,93,0,115,116,97,99,107,105,110,100,101,120,0,42,101,114,114,111,114,0,109,111,100,105,102,105,101,114,0,42,116,101,120,116,117,114,101,0,42,109,97,112,95,111,98,106,101,99,116,0,117,118,108,97,121,101,114,95,110,97,109,101,91,54,52,93,0,117,118,108,97,121,101,114,95,116,109,112,0,116,101,120,109,97,112,112,
105,110,103,0,115,117,98,100,105,118,84,121,112,101,0,114,101,110,100,101,114,76,101,118,101,108,115,0,117,115,101,95,111,112,101,110,115,117,98,100,105,118,0,42,101,109,67,97,99,104,101,0,42,109,67,97,99,104,101,0,115,116,114,101,110,103,116,104,0,100,101,102,97,120,105,115,0,112,97,100,91,54,93,0,115,116,97,114,116,0,108,101,110,103,116,104,0,114,97,110,100,111,109,105,122,101,0,115,101,101,100,0,42,111,98,95,97,114,109,0,
42,115,116,97,114,116,95,99,97,112,0,42,101,110,100,95,99,97,112,0,42,99,117,114,118,101,95,111,98,0,42,111,102,102,115,101,116,95,111,98,0,111,102,102,115,101,116,91,51,93,0,115,99,97,108,101,91,51,93,0,109,101,114,103,101,95,100,105,115,116,0,102,105,116,95,116,121,112,101,0,111,102,102,115,101,116,95,116,121,112,101,0,99,111,117,110,116,0,97,120,105,115,0,116,111,108,101,114,97,110,99,101,0,117,118,95,111,102,102,115,
101,116,91,50,93,0,42,109,105,114,114,111,114,95,111,98,0,115,112,108,105,116,95,97,110,103,108,101,0,118,97,108,117,101,0,114,101,115,0,118,97,108,95,102,108,97,103,115,0,108,105,109,95,102,108,97,103,115,0,101,95,102,108,97,103,115,0,109,97,116,0,112,114,111,102,105,108,101,0,98,101,118,101,108,95,97,110,103,108,101,0,100,101,102,103,114,112,95,110,97,109,101,91,54,52,93,0,42,100,111,109,97,105,110,0,42,102,108,111,119,
0,42,99,111,108,108,0,116,105,109,101,0,100,105,114,101,99,116,105,111,110,0,109,105,100,108,101,118,101,108,0,115,112,97,99,101,0,42,112,114,111,106,101,99,116,111,114,115,91,49,48,93,0,42,105,109,97,103,101,0,110,117,109,95,112,114,111,106,101,99,116,111,114,115,0,97,115,112,101,99,116,120,0,97,115,112,101,99,116,121,0,115,99,97,108,101,120,0,115,99,97,108,101,121,0,112,101,114,99,101,110,116,0,105,116,101,114,0,100,101,
108,105,109,105,116,0,115,121,109,109,101,116,114,121,95,97,120,105,115,0,97,110,103,108,101,0,100,101,102,103,114,112,95,102,97,99,116,111,114,0,102,97,99,101,95,99,111,117,110,116,0,102,97,99,0,114,101,112,101,97,116,0,42,111,98,106,101,99,116,99,101,110,116,101,114,0,115,116,97,114,116,120,0,115,116,97,114,116,121,0,104,101,105,103,104,116,0,110,97,114,114,111,119,0,115,112,101,101,100,0,100,97,109,112,0,102,97,108,108,111,
102,102,0,116,105,109,101,111,102,102,115,0,108,105,102,101,116,105,109,101,0,100,101,102,111,114,109,102,108,97,103,0,109,117,108,116,105,0,42,112,114,101,118,67,111,115,0,115,117,98,116,97,114,103,101,116,91,54,52,93,0,112,97,114,101,110,116,105,110,118,91,52,93,91,52,93,0,99,101,110,116,91,51,93,0,42,105,110,100,101,120,97,114,0,116,111,116,105,110,100,101,120,0,102,111,114,99,101,0,42,99,108,111,116,104,79,98,106,101,99,
116,0,42,115,105,109,95,112,97,114,109,115,0,42,99,111,108,108,95,112,97,114,109,115,0,42,112,111,105,110,116,95,99,97,99,104,101,0,112,116,99,97,99,104,101,115,0,42,104,97,105,114,100,97,116,97,0,104,97,105,114,95,103,114,105,100,95,109,105,110,91,51,93,0,104,97,105,114,95,103,114,105,100,95,109,97,120,91,51,93,0,104,97,105,114,95,103,114,105,100,95,114,101,115,91,51,93,0,104,97,105,114,95,103,114,105,100,95,99,101,
108,108,115,105,122,101,0,42,115,111,108,118,101,114,95,114,101,115,117,108,116,0,42,120,0,42,120,110,101,119,0,42,120,111,108,100,0,42,99,117,114,114,101,110,116,95,120,110,101,119,0,42,99,117,114,114,101,110,116,95,120,0,42,99,117,114,114,101,110,116,95,118,0,42,116,114,105,0,109,118,101,114,116,95,110,117,109,0,116,114,105,95,110,117,109,0,116,105,109,101,95,120,0,116,105,109,101,95,120,110,101,119,0,105,115,95,115,116,97,116,
105,99,0,42,98,118,104,116,114,101,101,0,42,118,0,42,100,109,0,99,102,114,97,0,110,117,109,118,101,114,116,115,0,111,112,101,114,97,116,105,111,110,0,115,111,108,118,101,114,0,100,111,117,98,108,101,95,116,104,114,101,115,104,111,108,100,0,118,101,114,116,101,120,0,116,111,116,105,110,102,108,117,101,110,99,101,0,103,114,105,100,115,105,122,101,0,42,98,105,110,100,105,110,102,108,117,101,110,99,101,115,0,42,98,105,110,100,111,102,102,
115,101,116,115,0,42,98,105,110,100,99,97,103,101,99,111,115,0,116,111,116,99,97,103,101,118,101,114,116,0,42,100,121,110,103,114,105,100,0,42,100,121,110,105,110,102,108,117,101,110,99,101,115,0,42,100,121,110,118,101,114,116,115,0,100,121,110,103,114,105,100,115,105,122,101,0,100,121,110,99,101,108,108,109,105,110,91,51,93,0,100,121,110,99,101,108,108,119,105,100,116,104,0,98,105,110,100,109,97,116,91,52,93,91,52,93,0,42,98,105,
110,100,119,101,105,103,104,116,115,0,42,98,105,110,100,99,111,115,0,40,42,98,105,110,100,102,117,110,99,41,40,41,0,42,112,115,121,115,0,42,100,109,95,102,105,110,97,108,0,42,100,109,95,100,101,102,111,114,109,101,100,0,116,111,116,100,109,118,101,114,116,0,116,111,116,100,109,101,100,103,101,0,116,111,116,100,109,102,97,99,101,0,112,111,115,105,116,105,111,110,0,114,97,110,100,111,109,95,112,111,115,105,116,105,111,110,0,42,102,97,
99,101,112,97,0,118,103,114,111,117,112,0,112,114,111,116,101,99,116,0,108,118,108,0,115,99,117,108,112,116,108,118,108,0,116,111,116,108,118,108,0,115,105,109,112,108,101,0,42,102,115,115,0,42,116,97,114,103,101,116,0,42,97,117,120,84,97,114,103,101,116,0,118,103,114,111,117,112,95,110,97,109,101,91,54,52,93,0,107,101,101,112,68,105,115,116,0,115,104,114,105,110,107,84,121,112,101,0,115,104,114,105,110,107,79,112,116,115,0,112,
114,111,106,76,105,109,105,116,0,112,114,111,106,65,120,105,115,0,115,117,98,115,117,114,102,76,101,118,101,108,115,0,42,111,114,105,103,105,110,0,108,105,109,105,116,91,50,93,0,111,102,102,115,101,116,95,102,97,99,0,111,102,102,115,101,116,95,102,97,99,95,118,103,0,111,102,102,115,101,116,95,99,108,97,109,112,0,99,114,101,97,115,101,95,105,110,110,101,114,0,99,114,101,97,115,101,95,111,117,116,101,114,0,99,114,101,97,115,101,95,
114,105,109,0,109,97,116,95,111,102,115,0,109,97,116,95,111,102,115,95,114,105,109,0,42,111,98,95,97,120,105,115,0,115,116,101,112,115,0,114,101,110,100,101,114,95,115,116,101,112,115,0,115,99,114,101,119,95,111,102,115,0,42,111,99,101,97,110,0,42,111,99,101,97,110,99,97,99,104,101,0,114,101,115,111,108,117,116,105,111,110,0,115,112,97,116,105,97,108,95,115,105,122,101,0,119,105,110,100,95,118,101,108,111,99,105,116,121,0,115,
109,97,108,108,101,115,116,95,119,97,118,101,0,119,97,118,101,95,97,108,105,103,110,109,101,110,116,0,119,97,118,101,95,100,105,114,101,99,116,105,111,110,0,119,97,118,101,95,115,99,97,108,101,0,99,104,111,112,95,97,109,111,117,110,116,0,102,111,97,109,95,99,111,118,101,114,97,103,101,0,98,97,107,101,115,116,97,114,116,0,98,97,107,101,101,110,100,0,99,97,99,104,101,112,97,116,104,91,49,48,50,52,93,0,102,111,97,109,108,97,
121,101,114,110,97,109,101,91,54,52,93,0,99,97,99,104,101,100,0,103,101,111,109,101,116,114,121,95,109,111,100,101,0,114,101,102,114,101,115,104,0,114,101,112,101,97,116,95,120,0,114,101,112,101,97,116,95,121,0,102,111,97,109,95,102,97,100,101,0,42,111,98,106,101,99,116,95,102,114,111,109,0,42,111,98,106,101,99,116,95,116,111,0,102,97,108,108,111,102,102,95,114,97,100,105,117,115,0,101,100,105,116,95,102,108,97,103,115,0,100,
101,102,97,117,108,116,95,119,101,105,103,104,116,0,42,99,109,97,112,95,99,117,114,118,101,0,97,100,100,95,116,104,114,101,115,104,111,108,100,0,114,101,109,95,116,104,114,101,115,104,111,108,100,0,109,97,115,107,95,99,111,110,115,116,97,110,116,0,109,97,115,107,95,100,101,102,103,114,112,95,110,97,109,101,91,54,52,93,0,109,97,115,107,95,116,101,120,95,117,115,101,95,99,104,97,110,110,101,108,0,42,109,97,115,107,95,116,101,120,116,
117,114,101,0,42,109,97,115,107,95,116,101,120,95,109,97,112,95,111,98,106,0,109,97,115,107,95,116,101,120,95,109,97,112,112,105,110,103,0,109,97,115,107,95,116,101,120,95,117,118,108,97,121,101,114,95,110,97,109,101,91,54,52,93,0,112,97,100,95,105,49,0,100,101,102,103,114,112,95,110,97,109,101,95,97,91,54,52,93,0,100,101,102,103,114,112,95,110,97,109,101,95,98,91,54,52,93,0,100,101,102,97,117,108,116,95,119,101,105,103,
104,116,95,97,0,100,101,102,97,117,108,116,95,119,101,105,103,104,116,95,98,0,109,105,120,95,109,111,100,101,0,109,105,120,95,115,101,116,0,112,97,100,95,99,49,91,54,93,0,112,114,111,120,105,109,105,116,121,95,109,111,100,101,0,112,114,111,120,105,109,105,116,121,95,102,108,97,103,115,0,42,112,114,111,120,105,109,105,116,121,95,111,98,95,116,97,114,103,101,116,0,109,105,110,95,100,105,115,116,0,109,97,120,95,100,105,115,116,0,42,
99,97,110,118,97,115,0,42,98,114,117,115,104,0,116,104,114,101,115,104,111,108,100,0,115,99,97,108,101,0,104,101,114,109,105,116,101,95,110,117,109,0,98,114,97,110,99,104,95,115,109,111,111,116,104,105,110,103,0,115,121,109,109,101,116,114,121,95,97,120,101,115,0,113,117,97,100,95,109,101,116,104,111,100,0,110,103,111,110,95,109,101,116,104,111,100,0,108,97,109,98,100,97,0,108,97,109,98,100,97,95,98,111,114,100,101,114,0,40,42,
98,105,110,100,95,99,111,111,114,100,115,41,40,41,0,98,105,110,100,95,99,111,111,114,100,115,95,110,117,109,0,115,109,111,111,116,104,95,116,121,112,101,0,114,101,115,116,95,115,111,117,114,99,101,0,40,42,100,101,108,116,97,95,99,97,99,104,101,41,40,41,0,100,101,108,116,97,95,99,97,99,104,101,95,110,117,109,0,112,97,100,50,91,52,93,0,97,120,105,115,95,117,0,97,120,105,115,95,118,0,99,101,110,116,101,114,91,50,93,0,
42,111,98,106,101,99,116,95,115,114,99,0,98,111,110,101,95,115,114,99,91,54,52,93,0,42,111,98,106,101,99,116,95,100,115,116,0,98,111,110,101,95,100,115,116,91,54,52,93,0,116,105,109,101,95,109,111,100,101,0,112,108,97,121,95,109,111,100,101,0,102,111,114,119,97,114,100,95,97,120,105,115,0,117,112,95,97,120,105,115,0,102,108,105,112,95,97,120,105,115,0,105,110,116,101,114,112,0,100,101,102,111,114,109,95,109,111,100,101,0,
102,114,97,109,101,95,115,116,97,114,116,0,102,114,97,109,101,95,115,99,97,108,101,0,101,118,97,108,95,102,114,97,109,101,0,101,118,97,108,95,116,105,109,101,0,101,118,97,108,95,102,97,99,116,111,114,0,97,110,99,104,111,114,95,103,114,112,95,110,97,109,101,91,54,52,93,0,116,111,116,97,108,95,118,101,114,116,115,0,42,118,101,114,116,101,120,99,111,0,42,99,97,99,104,101,95,115,121,115,116,101,109,0,99,114,101,97,115,101,95,
119,101,105,103,104,116,0,42,111,98,95,115,111,117,114,99,101,0,100,97,116,97,95,116,121,112,101,115,0,118,109,97,112,95,109,111,100,101,0,101,109,97,112,95,109,111,100,101,0,108,109,97,112,95,109,111,100,101,0,112,109,97,112,95,109,111,100,101,0,109,97,112,95,109,97,120,95,100,105,115,116,97,110,99,101,0,109,97,112,95,114,97,121,95,114,97,100,105,117,115,0,105,115,108,97,110,100,115,95,112,114,101,99,105,115,105,111,110,0,108,
97,121,101,114,115,95,115,101,108,101,99,116,95,115,114,99,91,52,93,0,108,97,121,101,114,115,95,115,101,108,101,99,116,95,100,115,116,91,52,93,0,109,105,120,95,102,97,99,116,111,114,0,109,105,120,95,108,105,109,105,116,0,112,97,100,95,102,49,0,42,99,97,99,104,101,95,102,105,108,101,0,42,114,101,97,100,101,114,0,111,98,106,101,99,116,95,112,97,116,104,91,49,48,50,52,93,0,114,101,97,100,95,102,108,97,103,0,42,118,101,
114,116,95,105,110,100,115,0,42,118,101,114,116,95,119,101,105,103,104,116,115,0,110,111,114,109,97,108,95,100,105,115,116,0,105,110,102,108,117,101,110,99,101,0,42,98,105,110,100,115,0,110,117,109,98,105,110,100,115,0,110,117,109,112,111,108,121,0,42,108,97,116,116,0,112,110,116,115,119,0,111,112,110,116,115,117,0,111,112,110,116,115,118,0,111,112,110,116,115,119,0,116,121,112,101,117,0,116,121,112,101,118,0,116,121,112,101,119,0,97,
99,116,98,112,0,102,117,0,102,118,0,102,119,0,100,117,0,100,118,0,100,119,0,42,100,101,102,0,42,101,100,105,116,108,97,116,116,0,118,101,99,91,56,93,91,51,93,0,42,115,111,117,114,99,101,0,100,105,115,116,97,110,99,101,0,111,98,104,121,115,116,101,114,101,115,105,115,0,42,115,99,117,108,112,116,0,112,97,114,116,121,112,101,0,112,97,114,49,0,112,97,114,50,0,112,97,114,51,0,112,97,114,115,117,98,115,116,114,91,54,
52,93,0,42,116,114,97,99,107,0,42,112,114,111,120,121,0,42,112,114,111,120,121,95,103,114,111,117,112,0,42,112,114,111,120,121,95,102,114,111,109,0,42,97,99,116,105,111,110,0,42,112,111,115,101,108,105,98,0,42,112,111,115,101,0,42,103,112,100,0,97,118,115,0,42,109,112,97,116,104,0,99,111,110,115,116,114,97,105,110,116,67,104,97,110,110,101,108,115,0,101,102,102,101,99,116,0,100,101,102,98,97,115,101,0,109,111,100,105,102,
105,101,114,115,0,114,101,115,116,111,114,101,95,109,111,100,101,0,42,109,97,116,98,105,116,115,0,97,99,116,99,111,108,0,100,108,111,99,91,51,93,0,111,114,105,103,91,51,93,0,100,115,105,122,101,91,51,93,0,100,115,99,97,108,101,91,51,93,0,100,114,111,116,91,51,93,0,100,113,117,97,116,91,52,93,0,114,111,116,65,120,105,115,91,51,93,0,100,114,111,116,65,120,105,115,91,51,93,0,114,111,116,65,110,103,108,101,0,100,114,
111,116,65,110,103,108,101,0,111,98,109,97,116,91,52,93,91,52,93,0,99,111,110,115,116,105,110,118,91,52,93,91,52,93,0,105,109,97,116,95,114,101,110,91,52,93,91,52,93,0,108,97,121,0,99,111,108,98,105,116,115,0,116,114,97,110,115,102,108,97,103,0,112,114,111,116,101,99,116,102,108,97,103,0,116,114,97,99,107,102,108,97,103,0,117,112,102,108,97,103,0,110,108,97,102,108,97,103,0,115,99,97,102,108,97,103,0,115,99,97,
118,105,115,102,108,97,103,0,100,101,112,115,102,108,97,103,0,108,97,115,116,78,101,101,100,77,97,112,112,105,110,103,0,100,117,112,111,110,0,100,117,112,111,102,102,0,100,117,112,115,116,97,0,100,117,112,101,110,100,0,109,97,115,115,0,100,97,109,112,105,110,103,0,105,110,101,114,116,105,97,0,102,111,114,109,102,97,99,116,111,114,0,114,100,97,109,112,105,110,103,0,109,97,114,103,105,110,0,109,97,120,95,118,101,108,0,109,105,110,95,
118,101,108,0,109,97,120,95,97,110,103,118,101,108,0,109,105,110,95,97,110,103,118,101,108,0,111,98,115,116,97,99,108,101,82,97,100,0,115,116,101,112,95,104,101,105,103,104,116,0,106,117,109,112,95,115,112,101,101,100,0,102,97,108,108,95,115,112,101,101,100,0,109,97,120,95,106,117,109,112,115,0,112,97,100,50,91,51,93,0,99,111,108,95,103,114,111,117,112,0,99,111,108,95,109,97,115,107,0,114,111,116,109,111,100,101,0,98,111,117,
110,100,116,121,112,101,0,99,111,108,108,105,115,105,111,110,95,98,111,117,110,100,116,121,112,101,0,100,116,0,101,109,112,116,121,95,100,114,97,119,116,121,112,101,0,101,109,112,116,121,95,100,114,97,119,115,105,122,101,0,100,117,112,102,97,99,101,115,99,97,0,112,114,111,112,0,115,101,110,115,111,114,115,0,99,111,110,116,114,111,108,108,101,114,115,0,97,99,116,117,97,116,111,114,115,0,115,102,0,97,99,116,100,101,102,0,103,97,109,101,
102,108,97,103,0,103,97,109,101,102,108,97,103,50,0,42,98,115,111,102,116,0,114,101,115,116,114,105,99,116,102,108,97,103,0,115,111,102,116,102,108,97,103,0,97,110,105,115,111,116,114,111,112,105,99,70,114,105,99,116,105,111,110,91,51,93,0,99,111,110,115,116,114,97,105,110,116,115,0,110,108,97,115,116,114,105,112,115,0,104,111,111,107,115,0,112,97,114,116,105,99,108,101,115,121,115,116,101,109,0,42,115,111,102,116,0,42,100,117,112,
95,103,114,111,117,112,0,98,111,100,121,95,116,121,112,101,0,115,104,97,112,101,102,108,97,103,0,42,102,108,117,105,100,115,105,109,83,101,116,116,105,110,103,115,0,42,99,117,114,118,101,95,99,97,99,104,101,0,42,100,101,114,105,118,101,100,68,101,102,111,114,109,0,42,100,101,114,105,118,101,100,70,105,110,97,108,0,108,97,115,116,68,97,116,97,77,97,115,107,0,99,117,115,116,111,109,100,97,116,97,95,109,97,115,107,0,115,116,97,116,
101,0,105,110,105,116,95,115,116,97,116,101,0,103,112,117,108,97,109,112,0,112,99,95,105,100,115,0,42,100,117,112,108,105,108,105,115,116,0,42,114,105,103,105,100,98,111,100,121,95,111,98,106,101,99,116,0,42,114,105,103,105,100,98,111,100,121,95,99,111,110,115,116,114,97,105,110,116,0,105,109,97,95,111,102,115,91,50,93,0,42,105,117,115,101,114,0,108,111,100,108,101,118,101,108,115,0,42,99,117,114,114,101,110,116,108,111,100,0,99,
117,114,105,110,100,101,120,0,97,99,116,105,118,101,0,111,114,99,111,91,51,93,0,110,111,95,100,114,97,119,0,97,110,105,109,97,116,101,100,0,112,101,114,115,105,115,116,101,110,116,95,105,100,91,49,54,93,0,42,112,97,114,116,105,99,108,101,95,115,121,115,116,101,109,0,114,97,110,100,111,109,95,105,100,0,100,101,102,108,101,99,116,0,102,111,114,99,101,102,105,101,108,100,0,115,104,97,112,101,0,116,101,120,95,109,111,100,101,0,107,
105,110,107,0,107,105,110,107,95,97,120,105,115,0,122,100,105,114,0,102,95,115,116,114,101,110,103,116,104,0,102,95,100,97,109,112,0,102,95,102,108,111,119,0,102,95,115,105,122,101,0,102,95,112,111,119,101,114,0,109,97,120,100,105,115,116,0,109,105,110,100,105,115,116,0,102,95,112,111,119,101,114,95,114,0,109,97,120,114,97,100,0,109,105,110,114,97,100,0,112,100,101,102,95,100,97,109,112,0,112,100,101,102,95,114,100,97,109,112,0,
112,100,101,102,95,112,101,114,109,0,112,100,101,102,95,102,114,105,99,116,0,112,100,101,102,95,114,102,114,105,99,116,0,112,100,101,102,95,115,116,105,99,107,110,101,115,115,0,97,98,115,111,114,112,116,105,111,110,0,112,100,101,102,95,115,98,100,97,109,112,0,112,100,101,102,95,115,98,105,102,116,0,112,100,101,102,95,115,98,111,102,116,0,99,108,117,109,112,95,102,97,99,0,99,108,117,109,112,95,112,111,119,0,107,105,110,107,95,102,114,
101,113,0,107,105,110,107,95,115,104,97,112,101,0,107,105,110,107,95,97,109,112,0,102,114,101,101,95,101,110,100,0,116,101,120,95,110,97,98,108,97,0,42,114,110,103,0,102,95,110,111,105,115,101,0,42,102,95,115,111,117,114,99,101,0,119,101,105,103,104,116,91,49,52,93,0,103,108,111,98,97,108,95,103,114,97,118,105,116,121,0,114,116,91,51,93,0,116,111,116,100,97,116,97,0,102,114,97,109,101,0,116,111,116,112,111,105,110,116,0,
42,100,97,116,97,91,56,93,0,42,99,117,114,91,56,93,0,101,120,116,114,97,100,97,116,97,0,115,116,101,112,0,115,105,109,102,114,97,109,101,0,115,116,97,114,116,102,114,97,109,101,0,101,110,100,102,114,97,109,101,0,101,100,105,116,102,114,97,109,101,0,108,97,115,116,95,101,120,97,99,116,0,108,97,115,116,95,118,97,108,105,100,0,99,111,109,112,114,101,115,115,105,111,110,0,112,114,101,118,95,110,97,109,101,91,54,52,93,0,105,
110,102,111,91,54,52,93,0,112,97,116,104,91,49,48,50,52,93,0,42,99,97,99,104,101,100,95,102,114,97,109,101,115,0,109,101,109,95,99,97,99,104,101,0,42,101,100,105,116,0,40,42,102,114,101,101,95,101,100,105,116,41,40,41,0,108,105,110,83,116,105,102,102,0,97,110,103,83,116,105,102,102,0,118,111,108,117,109,101,0,118,105,116,101,114,97,116,105,111,110,115,0,112,105,116,101,114,97,116,105,111,110,115,0,100,105,116,101,114,97,
116,105,111,110,115,0,99,105,116,101,114,97,116,105,111,110,115,0,107,83,82,72,82,95,67,76,0,107,83,75,72,82,95,67,76,0,107,83,83,72,82,95,67,76,0,107,83,82,95,83,80,76,84,95,67,76,0,107,83,75,95,83,80,76,84,95,67,76,0,107,83,83,95,83,80,76,84,95,67,76,0,107,86,67,70,0,107,68,80,0,107,68,71,0,107,76,70,0,107,80,82,0,107,86,67,0,107,68,70,0,107,77,84,0,107,67,72,82,0,107,
75,72,82,0,107,83,72,82,0,107,65,72,82,0,99,111,108,108,105,115,105,111,110,102,108,97,103,115,0,110,117,109,99,108,117,115,116,101,114,105,116,101,114,97,116,105,111,110,115,0,119,101,108,100,105,110,103,0,116,111,116,115,112,114,105,110,103,0,42,98,112,111,105,110,116,0,42,98,115,112,114,105,110,103,0,109,115,103,95,108,111,99,107,0,109,115,103,95,118,97,108,117,101,0,110,111,100,101,109,97,115,115,0,110,97,109,101,100,86,71,
95,77,97,115,115,91,54,52,93,0,103,114,97,118,0,109,101,100,105,97,102,114,105,99,116,0,114,107,108,105,109,105,116,0,112,104,121,115,105,99,115,95,115,112,101,101,100,0,103,111,97,108,115,112,114,105,110,103,0,103,111,97,108,102,114,105,99,116,0,109,105,110,103,111,97,108,0,109,97,120,103,111,97,108,0,100,101,102,103,111,97,108,0,118,101,114,116,103,114,111,117,112,0,110,97,109,101,100,86,71,95,83,111,102,116,103,111,97,108,91,
54,52,93,0,102,117,122,122,121,110,101,115,115,0,105,110,115,112,114,105,110,103,0,105,110,102,114,105,99,116,0,110,97,109,101,100,86,71,95,83,112,114,105,110,103,95,75,91,54,52,93,0,101,102,114,97,0,105,110,116,101,114,118,97,108,0,108,111,99,97,108,0,115,111,108,118,101,114,102,108,97,103,115,0,42,42,107,101,121,115,0,116,111,116,112,111,105,110,116,107,101,121,0,115,101,99,111,110,100,115,112,114,105,110,103,0,99,111,108,98,
97,108,108,0,98,97,108,108,100,97,109,112,0,98,97,108,108,115,116,105,102,102,0,115,98,99,95,109,111,100,101,0,97,101,114,111,101,100,103,101,0,109,105,110,108,111,111,112,115,0,109,97,120,108,111,111,112,115,0,99,104,111,107,101,0,115,111,108,118,101,114,95,73,68,0,112,108,97,115,116,105,99,0,115,112,114,105,110,103,112,114,101,108,111,97,100,0,42,115,99,114,97,116,99,104,0,115,104,101,97,114,115,116,105,102,102,0,105,110,112,
117,115,104,0,42,112,111,105,110,116,99,97,99,104,101,0,42,99,111,108,108,105,115,105,111,110,95,103,114,111,117,112,0,42,101,102,102,101,99,116,111,114,95,119,101,105,103,104,116,115,0,108,99,111,109,91,51,93,0,108,114,111,116,91,51,93,91,51,93,0,108,115,99,97,108,101,91,51,93,91,51,93,0,108,97,115,116,95,102,114,97,109,101,0,118,101,108,91,51,93,0,42,102,109,100,0,116,104,114,101,97,100,115,0,115,104,111,119,95,97,
100,118,97,110,99,101,100,111,112,116,105,111,110,115,0,114,101,115,111,108,117,116,105,111,110,120,121,122,0,112,114,101,118,105,101,119,114,101,115,120,121,122,0,114,101,97,108,115,105,122,101,0,103,117,105,68,105,115,112,108,97,121,77,111,100,101,0,114,101,110,100,101,114,68,105,115,112,108,97,121,77,111,100,101,0,118,105,115,99,111,115,105,116,121,86,97,108,117,101,0,118,105,115,99,111,115,105,116,121,77,111,100,101,0,118,105,115,99,111,115,
105,116,121,69,120,112,111,110,101,110,116,0,103,114,97,118,91,51,93,0,97,110,105,109,83,116,97,114,116,0,97,110,105,109,69,110,100,0,98,97,107,101,83,116,97,114,116,0,98,97,107,101,69,110,100,0,102,114,97,109,101,79,102,102,115,101,116,0,103,115,116,97,114,0,109,97,120,82,101,102,105,110,101,0,105,110,105,86,101,108,120,0,105,110,105,86,101,108,121,0,105,110,105,86,101,108,122,0,115,117,114,102,100,97,116,97,80,97,116,104,
91,49,48,50,52,93,0,98,98,83,116,97,114,116,91,51,93,0,98,98,83,105,122,101,91,51,93,0,116,121,112,101,70,108,97,103,115,0,100,111,109,97,105,110,78,111,118,101,99,103,101,110,0,118,111,108,117,109,101,73,110,105,116,84,121,112,101,0,112,97,114,116,83,108,105,112,86,97,108,117,101,0,103,101,110,101,114,97,116,101,84,114,97,99,101,114,115,0,103,101,110,101,114,97,116,101,80,97,114,116,105,99,108,101,115,0,115,117,114,102,
97,99,101,83,109,111,111,116,104,105,110,103,0,115,117,114,102,97,99,101,83,117,98,100,105,118,115,0,112,97,114,116,105,99,108,101,73,110,102,83,105,122,101,0,112,97,114,116,105,99,108,101,73,110,102,65,108,112,104,97,0,102,97,114,70,105,101,108,100,83,105,122,101,0,42,109,101,115,104,86,101,108,111,99,105,116,105,101,115,0,99,112,115,84,105,109,101,83,116,97,114,116,0,99,112,115,84,105,109,101,69,110,100,0,99,112,115,81,117,97,
108,105,116,121,0,97,116,116,114,97,99,116,102,111,114,99,101,83,116,114,101,110,103,116,104,0,97,116,116,114,97,99,116,102,111,114,99,101,82,97,100,105,117,115,0,118,101,108,111,99,105,116,121,102,111,114,99,101,83,116,114,101,110,103,116,104,0,118,101,108,111,99,105,116,121,102,111,114,99,101,82,97,100,105,117,115,0,108,97,115,116,103,111,111,100,102,114,97,109,101,0,97,110,105,109,82,97,116,101,0,109,105,115,116,121,112,101,0,104,111,
114,114,0,104,111,114,103,0,104,111,114,98,0,122,101,110,114,0,122,101,110,103,0,122,101,110,98,0,101,120,112,111,115,117,114,101,0,101,120,112,0,114,97,110,103,101,0,108,105,110,102,97,99,0,108,111,103,102,97,99,0,103,114,97,118,105,116,121,0,97,99,116,105,118,105,116,121,66,111,120,82,97,100,105,117,115,0,115,107,121,116,121,112,101,0,111,99,99,108,117,115,105,111,110,82,101,115,0,112,104,121,115,105,99,115,69,110,103,105,110,
101,0,116,105,99,114,97,116,101,0,109,97,120,108,111,103,105,99,115,116,101,112,0,112,104,121,115,117,98,115,116,101,112,0,109,97,120,112,104,121,115,116,101,112,0,109,105,115,105,0,109,105,115,116,115,116,97,0,109,105,115,116,100,105,115,116,0,109,105,115,116,104,105,0,115,116,97,114,114,0,115,116,97,114,103,0,115,116,97,114,98,0,115,116,97,114,107,0,115,116,97,114,115,105,122,101,0,115,116,97,114,109,105,110,100,105,115,116,0,115,
116,97,114,100,105,115,116,0,115,116,97,114,99,111,108,110,111,105,115,101,0,100,111,102,115,116,97,0,100,111,102,101,110,100,0,100,111,102,109,105,110,0,100,111,102,109,97,120,0,97,111,100,105,115,116,0,97,111,100,105,115,116,102,97,99,0,97,111,101,110,101,114,103,121,0,97,111,98,105,97,115,0,97,111,109,111,100,101,0,97,111,115,97,109,112,0,97,111,109,105,120,0,97,111,99,111,108,111,114,0,97,111,95,97,100,97,112,116,95,116,
104,114,101,115,104,0,97,111,95,97,100,97,112,116,95,115,112,101,101,100,95,102,97,99,0,97,111,95,97,112,112,114,111,120,95,101,114,114,111,114,0,97,111,95,97,112,112,114,111,120,95,99,111,114,114,101,99,116,105,111,110,0,97,111,95,105,110,100,105,114,101,99,116,95,101,110,101,114,103,121,0,97,111,95,101,110,118,95,101,110,101,114,103,121,0,97,111,95,112,97,100,50,0,97,111,95,105,110,100,105,114,101,99,116,95,98,111,117,110,99,
101,115,0,97,111,95,112,97,100,0,97,111,95,115,97,109,112,95,109,101,116,104,111,100,0,97,111,95,103,97,116,104,101,114,95,109,101,116,104,111,100,0,97,111,95,97,112,112,114,111,120,95,112,97,115,115,101,115,0,42,97,111,115,112,104,101,114,101,0,42,97,111,116,97,98,108,101,115,0,115,101,108,99,111,108,0,115,120,0,115,121,0,42,108,112,70,111,114,109,97,116,0,42,108,112,80,97,114,109,115,0,99,98,70,111,114,109,97,116,0,
99,98,80,97,114,109,115,0,102,99,99,84,121,112,101,0,102,99,99,72,97,110,100,108,101,114,0,100,119,75,101,121,70,114,97,109,101,69,118,101,114,121,0,100,119,81,117,97,108,105,116,121,0,100,119,66,121,116,101,115,80,101,114,83,101,99,111,110,100,0,100,119,70,108,97,103,115,0,100,119,73,110,116,101,114,108,101,97,118,101,69,118,101,114,121,0,97,118,105,99,111,100,101,99,110,97,109,101,91,49,50,56,93,0,42,99,100,80,97,114,
109,115,0,42,112,97,100,0,99,100,83,105,122,101,0,113,116,99,111,100,101,99,110,97,109,101,91,49,50,56,93,0,99,111,100,101,99,84,121,112,101,0,99,111,100,101,99,83,112,97,116,105,97,108,81,117,97,108,105,116,121,0,99,111,100,101,99,0,99,111,100,101,99,70,108,97,103,115,0,99,111,108,111,114,68,101,112,116,104,0,99,111,100,101,99,84,101,109,112,111,114,97,108,81,117,97,108,105,116,121,0,109,105,110,83,112,97,116,105,97,
108,81,117,97,108,105,116,121,0,109,105,110,84,101,109,112,111,114,97,108,81,117,97,108,105,116,121,0,107,101,121,70,114,97,109,101,82,97,116,101,0,98,105,116,82,97,116,101,0,97,117,100,105,111,99,111,100,101,99,84,121,112,101,0,97,117,100,105,111,83,97,109,112,108,101,82,97,116,101,0,97,117,100,105,111,66,105,116,68,101,112,116,104,0,97,117,100,105,111,67,104,97,110,110,101,108,115,0,97,117,100,105,111,67,111,100,101,99,70,108,
97,103,115,0,97,117,100,105,111,66,105,116,82,97,116,101,0,97,117,100,105,111,95,99,111,100,101,99,0,118,105,100,101,111,95,98,105,116,114,97,116,101,0,97,117,100,105,111,95,98,105,116,114,97,116,101,0,97,117,100,105,111,95,109,105,120,114,97,116,101,0,97,117,100,105,111,95,99,104,97,110,110,101,108,115,0,97,117,100,105,111,95,112,97,100,0,97,117,100,105,111,95,118,111,108,117,109,101,0,103,111,112,95,115,105,122,101,0,109,97,
120,95,98,95,102,114,97,109,101,115,0,99,111,110,115,116,97,110,116,95,114,97,116,101,95,102,97,99,116,111,114,0,102,102,109,112,101,103,95,112,114,101,115,101,116,0,114,99,95,109,105,110,95,114,97,116,101,0,114,99,95,109,97,120,95,114,97,116,101,0,114,99,95,98,117,102,102,101,114,95,115,105,122,101,0,109,117,120,95,112,97,99,107,101,116,95,115,105,122,101,0,109,117,120,95,114,97,116,101,0,109,105,120,114,97,116,101,0,109,97,
105,110,0,115,112,101,101,100,95,111,102,95,115,111,117,110,100,0,100,111,112,112,108,101,114,95,102,97,99,116,111,114,0,100,105,115,116,97,110,99,101,95,109,111,100,101,108,0,42,109,97,116,95,111,118,101,114,114,105,100,101,0,42,108,105,103,104,116,95,111,118,101,114,114,105,100,101,0,108,97,121,95,122,109,97,115,107,0,108,97,121,95,101,120,99,108,117,100,101,0,108,97,121,102,108,97,103,0,112,97,115,115,102,108,97,103,0,112,97,115,
115,95,120,111,114,0,112,97,115,115,95,97,108,112,104,97,95,116,104,114,101,115,104,111,108,100,0,42,112,114,111,112,0,102,114,101,101,115,116,121,108,101,67,111,110,102,105,103,0,115,117,102,102,105,120,91,54,52,93,0,118,105,101,119,102,108,97,103,0,100,105,115,112,108,97,121,95,109,111,100,101,0,97,110,97,103,108,121,112,104,95,116,121,112,101,0,105,110,116,101,114,108,97,99,101,95,116,121,112,101,0,105,109,116,121,112,101,0,112,108,
97,110,101,115,0,113,117,97,108,105,116,121,0,99,111,109,112,114,101,115,115,0,101,120,114,95,99,111,100,101,99,0,99,105,110,101,111,110,95,102,108,97,103,0,99,105,110,101,111,110,95,119,104,105,116,101,0,99,105,110,101,111,110,95,98,108,97,99,107,0,99,105,110,101,111,110,95,103,97,109,109,97,0,106,112,50,95,102,108,97,103,0,106,112,50,95,99,111,100,101,99,0,116,105,102,102,95,99,111,100,101,99,0,115,116,101,114,101,111,51,
100,95,102,111,114,109,97,116,0,118,105,101,119,95,115,101,116,116,105,110,103,115,0,100,105,115,112,108,97,121,95,115,101,116,116,105,110,103,115,0,105,109,95,102,111,114,109,97,116,0,99,97,103,101,95,101,120,116,114,117,115,105,111,110,0,112,97,115,115,95,102,105,108,116,101,114,0,110,111,114,109,97,108,95,115,119,105,122,122,108,101,91,51,93,0,110,111,114,109,97,108,95,115,112,97,99,101,0,115,97,118,101,95,109,111,100,101,0,99,97,
103,101,91,54,52,93,0,42,97,118,105,99,111,100,101,99,100,97,116,97,0,42,113,116,99,111,100,101,99,100,97,116,97,0,113,116,99,111,100,101,99,115,101,116,116,105,110,103,115,0,102,102,99,111,100,101,99,100,97,116,97,0,115,117,98,102,114,97,109,101,0,112,115,102,114,97,0,112,101,102,114,97,0,105,109,97,103,101,115,0,102,114,97,109,97,112,116,111,0,102,114,97,109,101,108,101,110,0,98,108,117,114,102,97,99,0,101,100,103,101,
82,0,101,100,103,101,71,0,101,100,103,101,66,0,102,117,108,108,115,99,114,101,101,110,0,120,112,108,97,121,0,121,112,108,97,121,0,102,114,101,113,112,108,97,121,0,97,116,116,114,105,98,0,102,114,97,109,101,95,115,116,101,112,0,115,116,101,114,101,111,109,111,100,101,0,100,105,109,101,110,115,105,111,110,115,112,114,101,115,101,116,0,109,97,120,105,109,115,105,122,101,0,112,97,100,54,0,120,115,99,104,0,121,115,99,104,0,120,112,97,
114,116,115,0,121,112,97,114,116,115,0,116,105,108,101,120,0,116,105,108,101,121,0,115,117,98,105,109,116,121,112,101,0,100,105,115,112,108,97,121,109,111,100,101,0,117,115,101,95,108,111,99,107,95,105,110,116,101,114,102,97,99,101,0,112,97,100,55,0,115,99,101,109,111,100,101,0,114,97,121,116,114,97,99,101,95,111,112,116,105,111,110,115,0,114,97,121,116,114,97,99,101,95,115,116,114,117,99,116,117,114,101,0,111,99,114,101,115,0,112,
97,100,52,0,97,108,112,104,97,109,111,100,101,0,111,115,97,0,102,114,115,95,115,101,99,0,101,100,103,101,105,110,116,0,115,97,102,101,116,121,0,98,111,114,100,101,114,0,100,105,115,112,114,101,99,116,0,108,97,121,101,114,115,0,97,99,116,108,97,121,0,109,98,108,117,114,95,115,97,109,112,108,101,115,0,120,97,115,112,0,121,97,115,112,0,102,114,115,95,115,101,99,95,98,97,115,101,0,103,97,117,115,115,0,99,111,108,111,114,95,
109,103,116,95,102,108,97,103,0,112,111,115,116,103,97,109,109,97,0,112,111,115,116,104,117,101,0,112,111,115,116,115,97,116,0,100,105,116,104,101,114,95,105,110,116,101,110,115,105,116,121,0,98,97,107,101,95,111,115,97,0,98,97,107,101,95,102,105,108,116,101,114,0,98,97,107,101,95,109,111,100,101,0,98,97,107,101,95,102,108,97,103,0,98,97,107,101,95,110,111,114,109,97,108,95,115,112,97,99,101,0,98,97,107,101,95,113,117,97,100,
95,115,112,108,105,116,0,98,97,107,101,95,109,97,120,100,105,115,116,0,98,97,107,101,95,98,105,97,115,100,105,115,116,0,98,97,107,101,95,115,97,109,112,108,101,115,0,98,97,107,101,95,112,97,100,0,98,97,107,101,95,117,115,101,114,95,115,99,97,108,101,0,98,97,107,101,95,112,97,100,49,0,112,105,99,91,49,48,50,52,93,0,115,116,97,109,112,0,115,116,97,109,112,95,102,111,110,116,95,105,100,0,115,116,97,109,112,95,117,100,
97,116,97,91,55,54,56,93,0,102,103,95,115,116,97,109,112,91,52,93,0,98,103,95,115,116,97,109,112,91,52,93,0,115,101,113,95,112,114,101,118,95,116,121,112,101,0,115,101,113,95,114,101,110,100,95,116,121,112,101,0,115,101,113,95,102,108,97,103,0,112,97,100,53,91,53,93,0,115,105,109,112,108,105,102,121,95,102,108,97,103,0,115,105,109,112,108,105,102,121,95,115,117,98,115,117,114,102,0,115,105,109,112,108,105,102,121,95,115,117,
98,115,117,114,102,95,114,101,110,100,101,114,0,115,105,109,112,108,105,102,121,95,115,104,97,100,111,119,115,97,109,112,108,101,115,0,112,97,100,57,0,115,105,109,112,108,105,102,121,95,112,97,114,116,105,99,108,101,115,0,115,105,109,112,108,105,102,121,95,112,97,114,116,105,99,108,101,115,95,114,101,110,100,101,114,0,115,105,109,112,108,105,102,121,95,97,111,115,115,115,0,99,105,110,101,111,110,119,104,105,116,101,0,99,105,110,101,111,110,98,
108,97,99,107,0,99,105,110,101,111,110,103,97,109,109,97,0,106,112,50,95,112,114,101,115,101,116,0,106,112,50,95,100,101,112,116,104,0,114,112,97,100,51,0,100,111,109,101,114,101,115,0,100,111,109,101,109,111,100,101,0,100,111,109,101,97,110,103,108,101,0,100,111,109,101,116,105,108,116,0,100,111,109,101,114,101,115,98,117,102,0,42,100,111,109,101,116,101,120,116,0,108,105,110,101,95,116,104,105,99,107,110,101,115,115,95,109,111,100,101,
0,117,110,105,116,95,108,105,110,101,95,116,104,105,99,107,110,101,115,115,0,101,110,103,105,110,101,91,51,50,93,0,98,97,107,101,0,112,114,101,118,105,101,119,95,115,116,97,114,116,95,114,101,115,111,108,117,116,105,111,110,0,100,101,98,117,103,95,112,97,115,115,95,116,121,112,101,0,97,99,116,118,105,101,119,0,112,97,100,56,91,50,93,0,109,98,108,117,114,95,115,104,117,116,116,101,114,95,99,117,114,118,101,0,110,97,109,101,91,51,
50,93,0,112,97,114,116,105,99,108,101,95,112,101,114,99,0,115,117,98,115,117,114,102,95,109,97,120,0,115,104,97,100,98,117,102,115,97,109,112,108,101,95,109,97,120,0,97,111,95,101,114,114,111,114,0,116,105,108,116,0,114,101,115,98,117,102,0,42,119,97,114,112,116,101,120,116,0,99,111,108,91,51,93,0,99,101,108,108,115,105,122,101,0,99,101,108,108,104,101,105,103,104,116,0,97,103,101,110,116,109,97,120,115,108,111,112,101,0,97,
103,101,110,116,109,97,120,99,108,105,109,98,0,97,103,101,110,116,104,101,105,103,104,116,0,97,103,101,110,116,114,97,100,105,117,115,0,101,100,103,101,109,97,120,108,101,110,0,101,100,103,101,109,97,120,101,114,114,111,114,0,114,101,103,105,111,110,109,105,110,115,105,122,101,0,114,101,103,105,111,110,109,101,114,103,101,115,105,122,101,0,118,101,114,116,115,112,101,114,112,111,108,121,0,100,101,116,97,105,108,115,97,109,112,108,101,100,105,115,116,
0,100,101,116,97,105,108,115,97,109,112,108,101,109,97,120,101,114,114,111,114,0,112,97,114,116,105,116,105,111,110,105,110,103,0,102,114,97,109,105,110,103,0,112,108,97,121,101,114,102,108,97,103,0,114,116,49,0,114,116,50,0,97,97,115,97,109,112,108,101,115,0,100,111,109,101,0,115,116,101,114,101,111,102,108,97,103,0,101,121,101,115,101,112,97,114,97,116,105,111,110,0,114,101,99,97,115,116,68,97,116,97,0,109,97,116,109,111,100,101,
0,101,120,105,116,107,101,121,0,118,115,121,110,99,0,111,98,115,116,97,99,108,101,83,105,109,117,108,97,116,105,111,110,0,114,97,115,116,101,114,95,115,116,111,114,97,103,101,0,108,101,118,101,108,72,101,105,103,104,116,0,100,101,97,99,116,105,118,97,116,105,111,110,116,105,109,101,0,108,105,110,101,97,114,100,101,97,99,116,116,104,114,101,115,104,111,108,100,0,97,110,103,117,108,97,114,100,101,97,99,116,116,104,114,101,115,104,111,108,100,
0,108,111,100,102,108,97,103,0,115,99,101,104,121,115,116,101,114,101,115,105,115,0,42,99,97,109,101,114,97,0,42,112,97,108,101,116,116,101,0,42,99,97,118,105,116,121,95,99,117,114,118,101,0,42,112,97,105,110,116,95,99,117,114,115,111,114,0,112,97,105,110,116,95,99,117,114,115,111,114,95,99,111,108,91,52,93,0,110,117,109,95,105,110,112,117,116,95,115,97,109,112,108,101,115,0,115,121,109,109,101,116,114,121,95,102,108,97,103,115,
0,116,105,108,101,95,111,102,102,115,101,116,91,51,93,0,112,97,105,110,116,0,109,105,115,115,105,110,103,95,100,97,116,97,0,115,101,97,109,95,98,108,101,101,100,0,110,111,114,109,97,108,95,97,110,103,108,101,0,115,99,114,101,101,110,95,103,114,97,98,95,115,105,122,101,91,50,93,0,42,112,97,105,110,116,99,117,114,115,111,114,0,42,115,116,101,110,99,105,108,0,42,99,108,111,110,101,0,115,116,101,110,99,105,108,95,99,111,108,91,
51,93,0,100,105,116,104,101,114,0,105,110,118,101,114,116,0,116,111,116,114,101,107,101,121,0,116,111,116,97,100,100,107,101,121,0,98,114,117,115,104,116,121,112,101,0,98,114,117,115,104,91,55,93,0,101,109,105,116,116,101,114,100,105,115,116,0,115,101,108,101,99,116,109,111,100,101,0,101,100,105,116,116,121,112,101,0,100,114,97,119,95,115,116,101,112,0,102,97,100,101,95,102,114,97,109,101,115,0,42,115,104,97,112,101,95,111,98,106,101,
99,116,0,114,97,100,105,97,108,95,115,121,109,109,91,51,93,0,100,101,116,97,105,108,95,115,105,122,101,0,115,121,109,109,101,116,114,105,122,101,95,100,105,114,101,99,116,105,111,110,0,103,114,97,118,105,116,121,95,102,97,99,116,111,114,0,99,111,110,115,116,97,110,116,95,100,101,116,97,105,108,0,100,101,116,97,105,108,95,112,101,114,99,101,110,116,0,42,103,114,97,118,105,116,121,95,111,98,106,101,99,116,0,42,118,112,97,105,110,116,
95,112,114,101,118,0,42,119,112,97,105,110,116,95,112,114,101,118,0,98,114,117,115,104,91,49,49,93,0,108,111,99,107,95,97,120,105,115,0,42,99,117,115,116,111,109,95,105,112,111,0,109,97,116,91,51,93,91,51,93,0,117,110,112,114,111,106,101,99,116,101,100,95,114,97,100,105,117,115,0,114,103,98,91,51,93,0,115,101,99,111,110,100,97,114,121,95,114,103,98,91,51,93,0,108,97,115,116,95,114,97,107,101,91,50,93,0,108,97,115,
116,95,114,97,107,101,95,97,110,103,108,101,0,108,97,115,116,95,115,116,114,111,107,101,95,118,97,108,105,100,0,97,118,101,114,97,103,101,95,115,116,114,111,107,101,95,97,99,99,117,109,91,51,93,0,97,118,101,114,97,103,101,95,115,116,114,111,107,101,95,99,111,117,110,116,101,114,0,98,114,117,115,104,95,114,111,116,97,116,105,111,110,0,98,114,117,115,104,95,114,111,116,97,116,105,111,110,95,115,101,99,0,97,110,99,104,111,114,101,100,
95,115,105,122,101,0,111,118,101,114,108,97,112,95,102,97,99,116,111,114,0,100,114,97,119,95,105,110,118,101,114,116,101,100,0,115,116,114,111,107,101,95,97,99,116,105,118,101,0,100,114,97,119,95,97,110,99,104,111,114,101,100,0,100,111,95,108,105,110,101,97,114,95,99,111,110,118,101,114,115,105,111,110,0,108,97,115,116,95,108,111,99,97,116,105,111,110,91,51,93,0,108,97,115,116,95,104,105,116,0,97,110,99,104,111,114,101,100,95,105,
110,105,116,105,97,108,95,109,111,117,115,101,91,50,93,0,112,105,120,101,108,95,114,97,100,105,117,115,0,115,105,122,101,95,112,114,101,115,115,117,114,101,95,118,97,108,117,101,0,116,101,120,95,109,111,117,115,101,91,50,93,0,109,97,115,107,95,116,101,120,95,109,111,117,115,101,91,50,93,0,42,99,111,108,111,114,115,112,97,99,101,0,99,117,114,118,101,95,116,121,112,101,0,100,101,112,116,104,95,109,111,100,101,0,115,117,114,102,97,99,
101,95,112,108,97,110,101,0,102,105,116,95,109,101,116,104,111,100,0,101,114,114,111,114,95,116,104,114,101,115,104,111,108,100,0,114,97,100,105,117,115,95,109,105,110,0,114,97,100,105,117,115,95,109,97,120,0,114,97,100,105,117,115,95,116,97,112,101,114,95,115,116,97,114,116,0,114,97,100,105,117,115,95,116,97,112,101,114,95,101,110,100,0,115,117,114,102,97,99,101,95,111,102,102,115,101,116,0,99,111,114,110,101,114,95,97,110,103,108,101,
0,95,112,97,100,49,91,50,93,0,111,118,101,114,104,97,110,103,95,97,120,105,115,0,111,118,101,114,104,97,110,103,95,109,105,110,0,111,118,101,114,104,97,110,103,95,109,97,120,0,116,104,105,99,107,110,101,115,115,95,109,105,110,0,116,104,105,99,107,110,101,115,115,95,109,97,120,0,116,104,105,99,107,110,101,115,115,95,115,97,109,112,108,101,115,0,95,112,97,100,50,91,51,93,0,100,105,115,116,111,114,116,95,109,105,110,0,100,105,115,
116,111,114,116,95,109,97,120,0,115,104,97,114,112,95,109,105,110,0,115,104,97,114,112,95,109,97,120,0,42,118,112,97,105,110,116,0,42,119,112,97,105,110,116,0,42,117,118,115,99,117,108,112,116,0,118,103,114,111,117,112,95,119,101,105,103,104,116,0,100,111,117,98,108,105,109,105,116,0,110,111,114,109,97,108,115,105,122,101,0,97,117,116,111,109,101,114,103,101,0,117,110,119,114,97,112,112,101,114,0,117,118,99,97,108,99,95,102,108,97,
103,0,117,118,95,102,108,97,103,0,117,118,95,115,101,108,101,99,116,109,111,100,101,0,117,118,99,97,108,99,95,109,97,114,103,105,110,0,97,117,116,111,105,107,95,99,104,97,105,110,108,101,110,0,103,112,101,110,99,105,108,95,102,108,97,103,115,0,103,112,101,110,99,105,108,95,115,114,99,0,103,112,101,110,99,105,108,95,118,51,100,95,97,108,105,103,110,0,103,112,101,110,99,105,108,95,118,50,100,95,97,108,105,103,110,0,103,112,101,110,
99,105,108,95,115,101,113,95,97,108,105,103,110,0,103,112,101,110,99,105,108,95,105,109,97,95,97,108,105,103,110,0,103,112,95,115,99,117,108,112,116,0,103,112,95,105,110,116,101,114,112,111,108,97,116,101,0,103,112,95,98,114,117,115,104,101,115,0,105,109,97,112,97,105,110,116,0,112,97,114,116,105,99,108,101,0,112,114,111,112,111,114,116,105,111,110,97,108,95,115,105,122,101,0,115,101,108,101,99,116,95,116,104,114,101,115,104,0,97,117,
116,111,107,101,121,95,109,111,100,101,0,97,117,116,111,107,101,121,95,102,108,97,103,0,107,101,121,102,114,97,109,101,95,116,121,112,101,0,109,117,108,116,105,114,101,115,95,115,117,98,100,105,118,95,116,121,112,101,0,115,107,103,101,110,95,114,101,115,111,108,117,116,105,111,110,0,115,107,103,101,110,95,116,104,114,101,115,104,111,108,100,95,105,110,116,101,114,110,97,108,0,115,107,103,101,110,95,116,104,114,101,115,104,111,108,100,95,101,120,116,
101,114,110,97,108,0,115,107,103,101,110,95,108,101,110,103,116,104,95,114,97,116,105,111,0,115,107,103,101,110,95,108,101,110,103,116,104,95,108,105,109,105,116,0,115,107,103,101,110,95,97,110,103,108,101,95,108,105,109,105,116,0,115,107,103,101,110,95,99,111,114,114,101,108,97,116,105,111,110,95,108,105,109,105,116,0,115,107,103,101,110,95,115,121,109,109,101,116,114,121,95,108,105,109,105,116,0,115,107,103,101,110,95,114,101,116,97,114,103,101,
116,95,97,110,103,108,101,95,119,101,105,103,104,116,0,115,107,103,101,110,95,114,101,116,97,114,103,101,116,95,108,101,110,103,116,104,95,119,101,105,103,104,116,0,115,107,103,101,110,95,114,101,116,97,114,103,101,116,95,100,105,115,116,97,110,99,101,95,119,101,105,103,104,116,0,115,107,103,101,110,95,111,112,116,105,111,110,115,0,115,107,103,101,110,95,112,111,115,116,112,114,111,0,115,107,103,101,110,95,112,111,115,116,112,114,111,95,112,97,115,
115,101,115,0,115,107,103,101,110,95,115,117,98,100,105,118,105,115,105,111,110,115,91,51,93,0,115,107,103,101,110,95,109,117,108,116,105,95,108,101,118,101,108,0,42,115,107,103,101,110,95,116,101,109,112,108,97,116,101,0,98,111,110,101,95,115,107,101,116,99,104,105,110,103,0,98,111,110,101,95,115,107,101,116,99,104,105,110,103,95,99,111,110,118,101,114,116,0,115,107,103,101,110,95,115,117,98,100,105,118,105,115,105,111,110,95,110,117,109,98,
101,114,0,115,107,103,101,110,95,114,101,116,97,114,103,101,116,95,111,112,116,105,111,110,115,0,115,107,103,101,110,95,114,101,116,97,114,103,101,116,95,114,111,108,108,0,115,107,103,101,110,95,115,105,100,101,95,115,116,114,105,110,103,91,56,93,0,115,107,103,101,110,95,110,117,109,95,115,116,114,105,110,103,91,56,93,0,101,100,103,101,95,109,111,100,101,0,101,100,103,101,95,109,111,100,101,95,108,105,118,101,95,117,110,119,114,97,112,0,115,
110,97,112,95,109,111,100,101,0,115,110,97,112,95,110,111,100,101,95,109,111,100,101,0,115,110,97,112,95,117,118,95,109,111,100,101,0,115,110,97,112,95,102,108,97,103,0,115,110,97,112,95,116,97,114,103,101,116,0,112,114,111,112,111,114,116,105,111,110,97,108,0,112,114,111,112,95,109,111,100,101,0,112,114,111,112,111,114,116,105,111,110,97,108,95,111,98,106,101,99,116,115,0,112,114,111,112,111,114,116,105,111,110,97,108,95,109,97,115,107,
0,112,114,111,112,111,114,116,105,111,110,97,108,95,97,99,116,105,111,110,0,112,114,111,112,111,114,116,105,111,110,97,108,95,102,99,117,114,118,101,0,108,111,99,107,95,109,97,114,107,101,114,115,0,112,97,100,52,91,53,93,0,97,117,116,111,95,110,111,114,109,97,108,105,122,101,0,109,117,108,116,105,112,97,105,110,116,0,119,101,105,103,104,116,117,115,101,114,0,118,103,114,111,117,112,115,117,98,115,101,116,0,117,115,101,95,117,118,95,115,
99,117,108,112,116,0,117,118,95,115,99,117,108,112,116,95,115,101,116,116,105,110,103,115,0,117,118,95,115,99,117,108,112,116,95,116,111,111,108,0,117,118,95,114,101,108,97,120,95,109,101,116,104,111,100,0,115,99,117,108,112,116,95,112,97,105,110,116,95,115,101,116,116,105,110,103,115,0,115,99,117,108,112,116,95,112,97,105,110,116,95,117,110,105,102,105,101,100,95,115,105,122,101,0,115,99,117,108,112,116,95,112,97,105,110,116,95,117,110,105,
102,105,101,100,95,117,110,112,114,111,106,101,99,116,101,100,95,114,97,100,105,117,115,0,115,99,117,108,112,116,95,112,97,105,110,116,95,117,110,105,102,105,101,100,95,97,108,112,104,97,0,117,110,105,102,105,101,100,95,112,97,105,110,116,95,115,101,116,116,105,110,103,115,0,99,117,114,118,101,95,112,97,105,110,116,95,115,101,116,116,105,110,103,115,0,115,116,97,116,118,105,115,0,116,111,116,111,98,106,0,116,111,116,108,97,109,112,0,116,111,
116,111,98,106,115,101,108,0,116,111,116,99,117,114,118,101,0,116,111,116,109,101,115,104,0,116,111,116,97,114,109,97,116,117,114,101,0,115,99,97,108,101,95,108,101,110,103,116,104,0,115,121,115,116,101,109,0,115,121,115,116,101,109,95,114,111,116,97,116,105,111,110,0,103,114,97,118,105,116,121,91,51,93,0,113,117,105,99,107,95,99,97,99,104,101,95,115,116,101,112,0,116,105,116,108,101,91,50,93,0,97,99,116,105,111,110,91,50,93,0,
116,105,116,108,101,95,99,101,110,116,101,114,91,50,93,0,97,99,116,105,111,110,95,99,101,110,116,101,114,91,50,93,0,42,119,111,114,108,100,0,42,115,101,116,0,98,97,115,101,0,42,98,97,115,97,99,116,0,42,111,98,101,100,105,116,0,99,117,114,115,111,114,91,51,93,0,116,119,99,101,110,116,91,51,93,0,116,119,109,105,110,91,51,93,0,116,119,109,97,120,91,51,93,0,108,97,121,97,99,116,0,108,97,121,95,117,112,100,97,116,
101,100,0,42,101,100,0,42,116,111,111,108,115,101,116,116,105,110,103,115,0,42,115,116,97,116,115,0,115,97,102,101,95,97,114,101,97,115,0,97,117,100,105,111,0,109,97,114,107,101,114,115,0,116,114,97,110,115,102,111,114,109,95,115,112,97,99,101,115,0,42,115,111,117,110,100,95,115,99,101,110,101,0,42,112,108,97,121,98,97,99,107,95,104,97,110,100,108,101,0,42,115,111,117,110,100,95,115,99,114,117,98,95,104,97,110,100,108,101,0,
42,115,112,101,97,107,101,114,95,104,97,110,100,108,101,115,0,42,102,112,115,95,105,110,102,111,0,42,100,101,112,115,103,114,97,112,104,0,42,112,97,100,49,0,42,116,104,101,68,97,103,0,100,97,103,102,108,97,103,115,0,97,99,116,105,118,101,95,107,101,121,105,110,103,115,101,116,0,107,101,121,105,110,103,115,101,116,115,0,103,109,0,117,110,105,116,0,112,104,121,115,105,99,115,95,115,101,116,116,105,110,103,115,0,42,99,108,105,112,0,
99,117,115,116,111,109,100,97,116,97,95,109,97,115,107,95,109,111,100,97,108,0,115,101,113,117,101,110,99,101,114,95,99,111,108,111,114,115,112,97,99,101,95,115,101,116,116,105,110,103,115,0,42,114,105,103,105,100,98,111,100,121,95,119,111,114,108,100,0,99,117,115,101,114,0,98,108,101,110,100,0,114,111,116,97,116,105,111,110,0,119,105,110,109,97,116,91,52,93,91,52,93,0,118,105,101,119,109,97,116,91,52,93,91,52,93,0,118,105,101,
119,105,110,118,91,52,93,91,52,93,0,112,101,114,115,109,97,116,91,52,93,91,52,93,0,112,101,114,115,105,110,118,91,52,93,91,52,93,0,118,105,101,119,99,97,109,116,101,120,99,111,102,97,99,91,52,93,0,118,105,101,119,109,97,116,111,98,91,52,93,91,52,93,0,112,101,114,115,109,97,116,111,98,91,52,93,91,52,93,0,99,108,105,112,91,54,93,91,52,93,0,99,108,105,112,95,108,111,99,97,108,91,54,93,91,52,93,0,42,99,
108,105,112,98,98,0,42,108,111,99,97,108,118,100,0,42,114,101,110,100,101,114,95,101,110,103,105,110,101,0,42,100,101,112,116,104,115,0,42,103,112,117,111,102,102,115,99,114,101,101,110,0,42,115,109,115,0,42,115,109,111,111,116,104,95,116,105,109,101,114,0,116,119,109,97,116,91,52,93,91,52,93,0,118,105,101,119,113,117,97,116,91,52,93,0,99,97,109,100,120,0,99,97,109,100,121,0,112,105,120,115,105,122,101,0,99,97,109,122,111,
111,109,0,105,115,95,112,101,114,115,112,0,112,101,114,115,112,0,118,105,101,119,108,111,99,107,0,118,105,101,119,108,111,99,107,95,113,117,97,100,0,111,102,115,95,108,111,99,107,91,50,93,0,116,119,100,114,97,119,102,108,97,103,0,114,102,108,97,103,0,108,118,105,101,119,113,117,97,116,91,52,93,0,108,112,101,114,115,112,0,108,118,105,101,119,0,103,114,105,100,118,105,101,119,0,116,119,95,105,100,111,116,91,51,93,0,114,111,116,95,
97,110,103,108,101,0,114,111,116,95,97,120,105,115,91,51,93,0,42,99,111,109,112,111,115,105,116,111,114,0,114,101,103,105,111,110,98,97,115,101,0,115,112,97,99,101,116,121,112,101,0,98,108,111,99,107,115,99,97,108,101,0,98,108,111,99,107,104,97,110,100,108,101,114,91,56,93,0,98,117,110,100,108,101,95,115,105,122,101,0,98,117,110,100,108,101,95,100,114,97,119,116,121,112,101,0,108,97,121,95,112,114,101,118,0,108,97,121,95,117,
115,101,100,0,42,111,98,95,99,101,110,116,114,101,0,114,101,110,100,101,114,95,98,111,114,100,101,114,0,98,103,112,105,99,98,97,115,101,0,42,98,103,112,105,99,0,111,98,95,99,101,110,116,114,101,95,98,111,110,101,91,54,52,93,0,100,114,97,119,116,121,112,101,0,111,98,95,99,101,110,116,114,101,95,99,117,114,115,111,114,0,115,99,101,110,101,108,111,99,107,0,97,114,111,117,110,100,0,103,114,105,100,0,110,101,97,114,0,102,97,
114,0,109,97,116,99,97,112,95,105,99,111,110,0,103,114,105,100,108,105,110,101,115,0,103,114,105,100,115,117,98,100,105,118,0,103,114,105,100,102,108,97,103,0,116,119,116,121,112,101,0,116,119,109,111,100,101,0,116,119,102,108,97,103,0,102,108,97,103,51,0,97,102,116,101,114,100,114,97,119,95,116,114,97,110,115,112,0,97,102,116,101,114,100,114,97,119,95,120,114,97,121,0,97,102,116,101,114,100,114,97,119,95,120,114,97,121,116,114,97,
110,115,112,0,122,98,117,102,0,120,114,97,121,0,112,97,100,51,91,52,93,0,102,120,95,115,101,116,116,105,110,103,115,0,42,112,114,111,112,101,114,116,105,101,115,95,115,116,111,114,97,103,101,0,42,100,101,102,109,97,116,101,114,105,97,108,0,115,116,101,114,101,111,51,100,95,102,108,97,103,0,115,116,101,114,101,111,51,100,95,99,97,109,101,114,97,0,115,116,101,114,101,111,51,100,95,99,111,110,118,101,114,103,101,110,99,101,95,102,97,
99,116,111,114,0,115,116,101,114,101,111,51,100,95,118,111,108,117,109,101,95,97,108,112,104,97,0,115,116,101,114,101,111,51,100,95,99,111,110,118,101,114,103,101,110,99,101,95,97,108,112,104,97,0,112,114,101,118,95,100,114,97,119,116,121,112,101,0,118,101,114,116,0,104,111,114,0,109,97,115,107,0,109,105,110,91,50,93,0,109,97,120,91,50,93,0,109,105,110,122,111,111,109,0,109,97,120,122,111,111,109,0,115,99,114,111,108,108,0,115,
99,114,111,108,108,95,117,105,0,107,101,101,112,116,111,116,0,107,101,101,112,122,111,111,109,0,107,101,101,112,111,102,115,0,97,108,105,103,110,0,119,105,110,120,0,119,105,110,121,0,111,108,100,119,105,110,120,0,111,108,100,119,105,110,121,0,42,116,97,98,95,111,102,102,115,101,116,0,116,97,98,95,110,117,109,0,116,97,98,95,99,117,114,0,114,112,116,95,109,97,115,107,0,118,50,100,0,109,97,105,110,98,0,109,97,105,110,98,111,0,
109,97,105,110,98,117,115,101,114,0,114,101,95,97,108,105,103,110,0,112,114,101,118,105,101,119,0,116,101,120,116,117,114,101,95,99,111,110,116,101,120,116,0,116,101,120,116,117,114,101,95,99,111,110,116,101,120,116,95,112,114,101,118,0,42,112,97,116,104,0,112,97,116,104,102,108,97,103,0,100,97,116,97,105,99,111,110,0,42,112,105,110,105,100,0,42,116,101,120,117,115,101,114,0,116,114,101,101,0,42,116,114,101,101,115,116,111,114,101,0,
115,101,97,114,99,104,95,115,116,114,105,110,103,91,54,52,93,0,115,101,97,114,99,104,95,116,115,101,0,111,117,116,108,105,110,101,118,105,115,0,115,116,111,114,101,102,108,97,103,0,115,101,97,114,99,104,95,102,108,97,103,115,0,42,116,114,101,101,104,97,115,104,0,42,97,100,115,0,103,104,111,115,116,67,117,114,118,101,115,0,97,117,116,111,115,110,97,112,0,99,117,114,115,111,114,84,105,109,101,0,99,117,114,115,111,114,86,97,108,0,
42,97,114,114,97,121,0,99,97,99,104,101,115,0,99,97,99,104,101,95,100,105,115,112,108,97,121,0,114,101,110,100,101,114,95,115,105,122,101,0,99,104,97,110,115,104,111,119,110,0,122,101,98,114,97,0,122,111,111,109,0,111,118,101,114,108,97,121,95,116,121,112,101,0,100,114,97,119,95,102,108,97,103,0,115,99,111,112,101,115,0,112,97,100,50,91,55,93,0,42,109,97,115,107,0,100,114,97,119,95,116,121,112,101,0,111,118,101,114,108,
97,121,95,109,111,100,101,0,112,97,100,51,91,53,93,0,116,105,116,108,101,91,57,54,93,0,100,105,114,91,49,48,57,48,93,0,112,97,100,95,99,49,91,50,93,0,102,105,108,101,91,50,53,54,93,0,114,101,110,97,109,101,102,105,108,101,91,50,53,54,93,0,114,101,110,97,109,101,101,100,105,116,91,50,53,54,93,0,102,105,108,116,101,114,95,103,108,111,98,91,50,53,54,93,0,102,105,108,116,101,114,95,115,101,97,114,99,104,91,54,
52,93,0,102,105,108,116,101,114,95,105,100,0,97,99,116,105,118,101,95,102,105,108,101,0,104,105,103,104,108,105,103,104,116,95,102,105,108,101,0,115,101,108,95,102,105,114,115,116,0,115,101,108,95,108,97,115,116,0,116,104,117,109,98,110,97,105,108,95,115,105,122,101,0,115,111,114,116,0,100,105,115,112,108,97,121,0,114,101,99,117,114,115,105,111,110,95,108,101,118,101,108,0,102,95,102,112,0,102,112,95,115,116,114,91,56,93,0,115,99,
114,111,108,108,95,111,102,102,115,101,116,0,42,112,97,114,97,109,115,0,42,102,105,108,101,115,0,42,102,111,108,100,101,114,115,95,112,114,101,118,0,42,102,111,108,100,101,114,115,95,110,101,120,116,0,42,111,112,0,42,115,109,111,111,116,104,115,99,114,111,108,108,95,116,105,109,101,114,0,42,112,114,101,118,105,101,119,115,95,116,105,109,101,114,0,42,108,97,121,111,117,116,0,114,101,99,101,110,116,110,114,0,98,111,111,107,109,97,114,107,
110,114,0,115,121,115,116,101,109,110,114,0,115,121,115,116,101,109,95,98,111,111,107,109,97,114,107,110,114,0,110,97,109,101,91,50,53,54,93,0,115,97,118,101,0,118,97,108,105,100,0,117,117,105,100,95,97,115,115,101,116,91,52,93,0,117,117,105,100,95,118,97,114,105,97,110,116,91,52,93,0,117,117,105,100,95,114,101,118,105,115,105,111,110,91,52,93,0,42,117,117,105,100,115,0,110,98,114,95,117,117,105,100,115,0,42,99,111,109,109,
101,110,116,0,117,117,105,100,91,52,93,0,115,105,122,101,95,115,116,114,91,49,54,93,0,116,105,109,101,95,115,116,114,91,56,93,0,100,97,116,101,95,115,116,114,91,49,54,93,0,42,100,101,115,99,114,105,112,116,105,111,110,0,114,101,118,105,115,105,111,110,115,0,110,98,114,95,114,101,118,105,115,105,111,110,115,0,97,99,116,95,114,101,118,105,115,105,111,110,0,42,101,110,116,114,121,0,116,121,112,101,102,108,97,103,0,98,108,101,110,
116,121,112,101,0,42,114,101,108,112,97,116,104,0,42,112,111,105,110,0,42,42,116,97,103,115,0,110,98,114,95,116,97,103,115,0,115,116,97,116,117,115,0,118,97,114,105,97,110,116,115,0,110,98,114,95,118,97,114,105,97,110,116,115,0,97,99,116,95,118,97,114,105,97,110,116,0,101,110,116,114,105,101,115,0,110,98,114,95,101,110,116,114,105,101,115,0,110,98,114,95,101,110,116,114,105,101,115,95,102,105,108,116,101,114,101,100,0,101,110,
116,114,121,95,105,100,120,95,115,116,97,114,116,0,101,110,116,114,121,95,105,100,120,95,101,110,100,0,114,111,111,116,91,49,48,50,52,93,0,115,97,109,112,108,101,95,108,105,110,101,95,104,105,115,116,0,99,117,114,115,111,114,91,50,93,0,99,101,110,116,120,0,99,101,110,116,121,0,112,105,110,0,99,117,114,116,105,108,101,0,108,111,99,107,0,100,116,95,117,118,0,115,116,105,99,107,121,0,100,116,95,117,118,115,116,114,101,116,99,104,
0,111,116,104,101,114,95,117,118,95,102,105,108,116,101,114,0,109,97,115,107,95,105,110,102,111,0,42,116,101,120,116,0,116,111,112,0,118,105,101,119,108,105,110,101,115,0,109,101,110,117,110,114,0,108,104,101,105,103,104,116,0,99,119,105,100,116,104,0,108,105,110,101,110,114,115,95,116,111,116,0,108,101,102,116,0,115,104,111,119,108,105,110,101,110,114,115,0,116,97,98,110,117,109,98,101,114,0,115,104,111,119,115,121,110,116,97,120,0,108,
105,110,101,95,104,108,105,103,104,116,0,111,118,101,114,119,114,105,116,101,0,108,105,118,101,95,101,100,105,116,0,112,105,120,95,112,101,114,95,108,105,110,101,0,116,120,116,115,99,114,111,108,108,0,116,120,116,98,97,114,0,119,111,114,100,119,114,97,112,0,100,111,112,108,117,103,105,110,115,0,102,105,110,100,115,116,114,91,50,53,54,93,0,114,101,112,108,97,99,101,115,116,114,91,50,53,54,93,0,109,97,114,103,105,110,95,99,111,108,117,
109,110,0,108,104,101,105,103,104,116,95,100,112,105,0,42,100,114,97,119,99,97,99,104,101,0,115,99,114,111,108,108,95,97,99,99,117,109,91,50,93,0,42,112,121,95,100,114,97,119,0,42,112,121,95,101,118,101,110,116,0,42,112,121,95,98,117,116,116,111,110,0,42,112,121,95,98,114,111,119,115,101,114,99,97,108,108,98,97,99,107,0,42,112,121,95,103,108,111,98,97,108,100,105,99,116,0,108,97,115,116,115,112,97,99,101,0,115,99,114,
105,112,116,110,97,109,101,91,49,48,50,52,93,0,115,99,114,105,112,116,97,114,103,91,50,53,54,93,0,42,115,99,114,105,112,116,0,42,98,117,116,95,114,101,102,115,0,112,97,114,101,110,116,95,107,101,121,0,118,105,101,119,95,99,101,110,116,101,114,91,50,93,0,110,111,100,101,95,110,97,109,101,91,54,52,93,0,42,105,100,0,97,115,112,101,99,116,0,116,114,101,101,112,97,116,104,0,42,101,100,105,116,116,114,101,101,0,116,114,101,
101,95,105,100,110,97,109,101,91,54,52,93,0,116,114,101,101,116,121,112,101,0,116,101,120,102,114,111,109,0,115,104,97,100,101,114,102,114,111,109,0,105,110,115,101,114,116,95,111,102,115,95,100,105,114,0,108,105,110,107,100,114,97,103,0,42,105,111,102,115,100,0,108,101,110,95,97,108,108,111,99,0,99,117,114,115,111,114,0,115,99,114,111,108,108,98,97,99,107,0,104,105,115,116,111,114,121,0,112,114,111,109,112,116,91,50,53,54,93,0,
108,97,110,103,117,97,103,101,91,51,50,93,0,115,101,108,95,115,116,97,114,116,0,115,101,108,95,101,110,100,0,102,105,108,116,101,114,95,116,121,112,101,0,102,105,108,116,101,114,91,54,52,93,0,120,108,111,99,107,111,102,0,121,108,111,99,107,111,102,0,117,115,101,114,0,112,97,116,104,95,108,101,110,103,116,104,0,108,111,99,91,50,93,0,115,116,97,98,109,97,116,91,52,93,91,52,93,0,117,110,105,115,116,97,98,109,97,116,91,52,
93,91,52,93,0,112,111,115,116,112,114,111,99,95,102,108,97,103,0,102,105,108,101,110,97,109,101,91,49,48,50,52,93,0,98,108,102,95,105,100,0,117,105,102,111,110,116,95,105,100,0,114,95,116,111,95,108,0,104,105,110,116,105,110,103,0,112,111,105,110,116,115,0,107,101,114,110,105,110,103,0,119,111,114,100,95,119,114,97,112,0,105,116,97,108,105,99,0,98,111,108,100,0,115,104,97,100,111,119,0,115,104,97,100,120,0,115,104,97,100,
121,0,115,104,97,100,111,119,97,108,112,104,97,0,115,104,97,100,111,119,99,111,108,111,114,0,112,97,110,101,108,116,105,116,108,101,0,103,114,111,117,112,108,97,98,101,108,0,119,105,100,103,101,116,108,97,98,101,108,0,119,105,100,103,101,116,0,112,97,110,101,108,122,111,111,109,0,109,105,110,108,97,98,101,108,99,104,97,114,115,0,109,105,110,119,105,100,103,101,116,99,104,97,114,115,0,99,111,108,117,109,110,115,112,97,99,101,0,116,101,
109,112,108,97,116,101,115,112,97,99,101,0,98,111,120,115,112,97,99,101,0,98,117,116,116,111,110,115,112,97,99,101,120,0,98,117,116,116,111,110,115,112,97,99,101,121,0,112,97,110,101,108,115,112,97,99,101,0,112,97,110,101,108,111,117,116,101,114,0,111,117,116,108,105,110,101,91,52,93,0,105,110,110,101,114,91,52,93,0,105,110,110,101,114,95,115,101,108,91,52,93,0,105,116,101,109,91,52,93,0,116,101,120,116,91,52,93,0,116,101,
120,116,95,115,101,108,91,52,93,0,115,104,97,100,101,100,0,115,104,97,100,101,116,111,112,0,115,104,97,100,101,100,111,119,110,0,97,108,112,104,97,95,99,104,101,99,107,0,105,110,110,101,114,95,97,110,105,109,91,52,93,0,105,110,110,101,114,95,97,110,105,109,95,115,101,108,91,52,93,0,105,110,110,101,114,95,107,101,121,91,52,93,0,105,110,110,101,114,95,107,101,121,95,115,101,108,91,52,93,0,105,110,110,101,114,95,100,114,105,118,
101,110,91,52,93,0,105,110,110,101,114,95,100,114,105,118,101,110,95,115,101,108,91,52,93,0,104,101,97,100,101,114,91,52,93,0,98,97,99,107,91,52,93,0,115,104,111,119,95,104,101,97,100,101,114,0,115,104,111,119,95,98,97,99,107,0,103,114,97,100,105,101,110,116,91,52,93,0,104,105,103,104,95,103,114,97,100,105,101,110,116,91,52,93,0,115,104,111,119,95,103,114,97,100,0,119,99,111,108,95,114,101,103,117,108,97,114,0,119,99,
111,108,95,116,111,111,108,0,119,99,111,108,95,116,101,120,116,0,119,99,111,108,95,114,97,100,105,111,0,119,99,111,108,95,111,112,116,105,111,110,0,119,99,111,108,95,116,111,103,103,108,101,0,119,99,111,108,95,110,117,109,0,119,99,111,108,95,110,117,109,115,108,105,100,101,114,0,119,99,111,108,95,109,101,110,117,0,119,99,111,108,95,112,117,108,108,100,111,119,110,0,119,99,111,108,95,109,101,110,117,95,98,97,99,107,0,119,99,111,108,
95,109,101,110,117,95,105,116,101,109,0,119,99,111,108,95,116,111,111,108,116,105,112,0,119,99,111,108,95,98,111,120,0,119,99,111,108,95,115,99,114,111,108,108,0,119,99,111,108,95,112,114,111,103,114,101,115,115,0,119,99,111,108,95,108,105,115,116,95,105,116,101,109,0,119,99,111,108,95,112,105,101,95,109,101,110,117,0,119,99,111,108,95,115,116,97,116,101,0,112,97,110,101,108,0,119,105,100,103,101,116,95,101,109,98,111,115,115,91,52,
93,0,109,101,110,117,95,115,104,97,100,111,119,95,102,97,99,0,109,101,110,117,95,115,104,97,100,111,119,95,119,105,100,116,104,0,105,99,111,110,102,105,108,101,91,50,53,54,93,0,105,99,111,110,95,97,108,112,104,97,0,120,97,120,105,115,91,52,93,0,121,97,120,105,115,91,52,93,0,122,97,120,105,115,91,52,93,0,116,105,116,108,101,91,52,93,0,116,101,120,116,95,104,105,91,52,93,0,104,101,97,100,101,114,95,116,105,116,108,101,
91,52,93,0,104,101,97,100,101,114,95,116,101,120,116,91,52,93,0,104,101,97,100,101,114,95,116,101,120,116,95,104,105,91,52,93,0,116,97,98,95,97,99,116,105,118,101,91,52,93,0,116,97,98,95,105,110,97,99,116,105,118,101,91,52,93,0,116,97,98,95,98,97,99,107,91,52,93,0,116,97,98,95,111,117,116,108,105,110,101,91,52,93,0,98,117,116,116,111,110,91,52,93,0,98,117,116,116,111,110,95,116,105,116,108,101,91,52,93,0,
98,117,116,116,111,110,95,116,101,120,116,91,52,93,0,98,117,116,116,111,110,95,116,101,120,116,95,104,105,91,52,93,0,108,105,115,116,91,52,93,0,108,105,115,116,95,116,105,116,108,101,91,52,93,0,108,105,115,116,95,116,101,120,116,91,52,93,0,108,105,115,116,95,116,101,120,116,95,104,105,91,52,93,0,112,97,110,101,108,99,111,108,111,114,115,0,103,114,97,100,105,101,110,116,115,0,115,104,97,100,101,49,91,52,93,0,115,104,97,100,
101,50,91,52,93,0,104,105,108,105,116,101,91,52,93,0,103,114,105,100,91,52,93,0,118,105,101,119,95,111,118,101,114,108,97,121,91,52,93,0,119,105,114,101,91,52,93,0,119,105,114,101,95,101,100,105,116,91,52,93,0,115,101,108,101,99,116,91,52,93,0,108,97,109,112,91,52,93,0,115,112,101,97,107,101,114,91,52,93,0,101,109,112,116,121,91,52,93,0,99,97,109,101,114,97,91,52,93,0,97,99,116,105,118,101,91,52,93,0,103,
114,111,117,112,91,52,93,0,103,114,111,117,112,95,97,99,116,105,118,101,91,52,93,0,116,114,97,110,115,102,111,114,109,91,52,93,0,118,101,114,116,101,120,91,52,93,0,118,101,114,116,101,120,95,115,101,108,101,99,116,91,52,93,0,118,101,114,116,101,120,95,98,101,118,101,108,91,52,93,0,118,101,114,116,101,120,95,117,110,114,101,102,101,114,101,110,99,101,100,91,52,93,0,101,100,103,101,91,52,93,0,101,100,103,101,95,115,101,108,101,
99,116,91,52,93,0,101,100,103,101,95,115,101,97,109,91,52,93,0,101,100,103,101,95,115,104,97,114,112,91,52,93,0,101,100,103,101,95,102,97,99,101,115,101,108,91,52,93,0,101,100,103,101,95,99,114,101,97,115,101,91,52,93,0,101,100,103,101,95,98,101,118,101,108,91,52,93,0,102,97,99,101,91,52,93,0,102,97,99,101,95,115,101,108,101,99,116,91,52,93,0,102,97,99,101,95,100,111,116,91,52,93,0,101,120,116,114,97,95,101,
100,103,101,95,108,101,110,91,52,93,0,101,120,116,114,97,95,101,100,103,101,95,97,110,103,108,101,91,52,93,0,101,120,116,114,97,95,102,97,99,101,95,97,110,103,108,101,91,52,93,0,101,120,116,114,97,95,102,97,99,101,95,97,114,101,97,91,52,93,0,110,111,114,109,97,108,91,52,93,0,118,101,114,116,101,120,95,110,111,114,109,97,108,91,52,93,0,108,111,111,112,95,110,111,114,109,97,108,91,52,93,0,98,111,110,101,95,115,111,108,
105,100,91,52,93,0,98,111,110,101,95,112,111,115,101,91,52,93,0,98,111,110,101,95,112,111,115,101,95,97,99,116,105,118,101,91,52,93,0,115,116,114,105,112,91,52,93,0,115,116,114,105,112,95,115,101,108,101,99,116,91,52,93,0,99,102,114,97,109,101,91,52,93,0,116,105,109,101,95,107,101,121,102,114,97,109,101,91,52,93,0,116,105,109,101,95,103,112,95,107,101,121,102,114,97,109,101,91,52,93,0,102,114,101,101,115,116,121,108,101,
95,101,100,103,101,95,109,97,114,107,91,52,93,0,102,114,101,101,115,116,121,108,101,95,102,97,99,101,95,109,97,114,107,91,52,93,0,110,117,114,98,95,117,108,105,110,101,91,52,93,0,110,117,114,98,95,118,108,105,110,101,91,52,93,0,97,99,116,95,115,112,108,105,110,101,91,52,93,0,110,117,114,98,95,115,101,108,95,117,108,105,110,101,91,52,93,0,110,117,114,98,95,115,101,108,95,118,108,105,110,101,91,52,93,0,108,97,115,116,115,
101,108,95,112,111,105,110,116,91,52,93,0,104,97,110,100,108,101,95,102,114,101,101,91,52,93,0,104,97,110,100,108,101,95,97,117,116,111,91,52,93,0,104,97,110,100,108,101,95,118,101,99,116,91,52,93,0,104,97,110,100,108,101,95,97,108,105,103,110,91,52,93,0,104,97,110,100,108,101,95,97,117,116,111,95,99,108,97,109,112,101,100,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,102,114,101,101,91,52,93,0,104,97,110,100,108,
101,95,115,101,108,95,97,117,116,111,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,118,101,99,116,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,97,108,105,103,110,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,97,117,116,111,95,99,108,97,109,112,101,100,91,52,93,0,100,115,95,99,104,97,110,110,101,108,91,52,93,0,100,115,95,115,117,98,99,104,97,110,110,101,108,91,52,93,0,107,101,121,116,121,112,101,95,107,
101,121,102,114,97,109,101,91,52,93,0,107,101,121,116,121,112,101,95,101,120,116,114,101,109,101,91,52,93,0,107,101,121,116,121,112,101,95,98,114,101,97,107,100,111,119,110,91,52,93,0,107,101,121,116,121,112,101,95,106,105,116,116,101,114,91,52,93,0,107,101,121,116,121,112,101,95,107,101,121,102,114,97,109,101,95,115,101,108,101,99,116,91,52,93,0,107,101,121,116,121,112,101,95,101,120,116,114,101,109,101,95,115,101,108,101,99,116,91,52,
93,0,107,101,121,116,121,112,101,95,98,114,101,97,107,100,111,119,110,95,115,101,108,101,99,116,91,52,93,0,107,101,121,116,121,112,101,95,106,105,116,116,101,114,95,115,101,108,101,99,116,91,52,93,0,107,101,121,98,111,114,100,101,114,91,52,93,0,107,101,121,98,111,114,100,101,114,95,115,101,108,101,99,116,91,52,93,0,99,111,110,115,111,108,101,95,111,117,116,112,117,116,91,52,93,0,99,111,110,115,111,108,101,95,105,110,112,117,116,91,
52,93,0,99,111,110,115,111,108,101,95,105,110,102,111,91,52,93,0,99,111,110,115,111,108,101,95,101,114,114,111,114,91,52,93,0,99,111,110,115,111,108,101,95,99,117,114,115,111,114,91,52,93,0,99,111,110,115,111,108,101,95,115,101,108,101,99,116,91,52,93,0,118,101,114,116,101,120,95,115,105,122,101,0,111,117,116,108,105,110,101,95,119,105,100,116,104,0,102,97,99,101,100,111,116,95,115,105,122,101,0,110,111,111,100,108,101,95,99,117,
114,118,105,110,103,0,115,121,110,116,97,120,108,91,52,93,0,115,121,110,116,97,120,115,91,52,93,0,115,121,110,116,97,120,98,91,52,93,0,115,121,110,116,97,120,110,91,52,93,0,115,121,110,116,97,120,118,91,52,93,0,115,121,110,116,97,120,99,91,52,93,0,115,121,110,116,97,120,100,91,52,93,0,115,121,110,116,97,120,114,91,52,93,0,110,111,100,101,99,108,97,115,115,95,111,117,116,112,117,116,91,52,93,0,110,111,100,101,99,108,
97,115,115,95,102,105,108,116,101,114,91,52,93,0,110,111,100,101,99,108,97,115,115,95,118,101,99,116,111,114,91,52,93,0,110,111,100,101,99,108,97,115,115,95,116,101,120,116,117,114,101,91,52,93,0,110,111,100,101,99,108,97,115,115,95,115,104,97,100,101,114,91,52,93,0,110,111,100,101,99,108,97,115,115,95,115,99,114,105,112,116,91,52,93,0,110,111,100,101,99,108,97,115,115,95,112,97,116,116,101,114,110,91,52,93,0,110,111,100,101,
99,108,97,115,115,95,108,97,121,111,117,116,91,52,93,0,109,111,118,105,101,91,52,93,0,109,111,118,105,101,99,108,105,112,91,52,93,0,109,97,115,107,91,52,93,0,105,109,97,103,101,91,52,93,0,115,99,101,110,101,91,52,93,0,97,117,100,105,111,91,52,93,0,101,102,102,101,99,116,91,52,93,0,116,114,97,110,115,105,116,105,111,110,91,52,93,0,109,101,116,97,91,52,93,0,116,101,120,116,95,115,116,114,105,112,91,52,93,0,107,
101,121,102,114,97,109,101,95,115,99,97,108,101,95,102,97,99,0,101,100,105,116,109,101,115,104,95,97,99,116,105,118,101,91,52,93,0,104,97,110,100,108,101,95,118,101,114,116,101,120,91,52,93,0,104,97,110,100,108,101,95,118,101,114,116,101,120,95,115,101,108,101,99,116,91,52,93,0,104,97,110,100,108,101,95,118,101,114,116,101,120,95,115,105,122,101,0,99,108,105,112,112,105,110,103,95,98,111,114,100,101,114,95,51,100,91,52,93,0,109,
97,114,107,101,114,95,111,117,116,108,105,110,101,91,52,93,0,109,97,114,107,101,114,91,52,93,0,97,99,116,95,109,97,114,107,101,114,91,52,93,0,115,101,108,95,109,97,114,107,101,114,91,52,93,0,100,105,115,95,109,97,114,107,101,114,91,52,93,0,108,111,99,107,95,109,97,114,107,101,114,91,52,93,0,98,117,110,100,108,101,95,115,111,108,105,100,91,52,93,0,112,97,116,104,95,98,101,102,111,114,101,91,52,93,0,112,97,116,104,95,
97,102,116,101,114,91,52,93,0,99,97,109,101,114,97,95,112,97,116,104,91,52,93,0,104,112,97,100,91,50,93,0,103,112,95,118,101,114,116,101,120,95,115,105,122,101,0,103,112,95,118,101,114,116,101,120,91,52,93,0,103,112,95,118,101,114,116,101,120,95,115,101,108,101,99,116,91,52,93,0,112,114,101,118,105,101,119,95,98,97,99,107,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,102,97,99,101,91,52,93,0,112,
114,101,118,105,101,119,95,115,116,105,116,99,104,95,101,100,103,101,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,118,101,114,116,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,115,116,105,116,99,104,97,98,108,101,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,117,110,115,116,105,116,99,104,97,98,108,101,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,
97,99,116,105,118,101,91,52,93,0,117,118,95,115,104,97,100,111,119,91,52,93,0,117,118,95,111,116,104,101,114,115,91,52,93,0,109,97,116,99,104,91,52,93,0,115,101,108,101,99,116,101,100,95,104,105,103,104,108,105,103,104,116,91,52,93,0,115,107,105,110,95,114,111,111,116,91,52,93,0,97,110,105,109,95,97,99,116,105,118,101,91,52,93,0,97,110,105,109,95,110,111,110,95,97,99,116,105,118,101,91,52,93,0,110,108,97,95,116,119,
101,97,107,105,110,103,91,52,93,0,110,108,97,95,116,119,101,97,107,100,117,112,108,105,91,52,93,0,110,108,97,95,116,114,97,110,115,105,116,105,111,110,91,52,93,0,110,108,97,95,116,114,97,110,115,105,116,105,111,110,95,115,101,108,91,52,93,0,110,108,97,95,109,101,116,97,91,52,93,0,110,108,97,95,109,101,116,97,95,115,101,108,91,52,93,0,110,108,97,95,115,111,117,110,100,91,52,93,0,110,108,97,95,115,111,117,110,100,95,115,
101,108,91,52,93,0,105,110,102,111,95,115,101,108,101,99,116,101,100,91,52,93,0,105,110,102,111,95,115,101,108,101,99,116,101,100,95,116,101,120,116,91,52,93,0,105,110,102,111,95,101,114,114,111,114,91,52,93,0,105,110,102,111,95,101,114,114,111,114,95,116,101,120,116,91,52,93,0,105,110,102,111,95,119,97,114,110,105,110,103,91,52,93,0,105,110,102,111,95,119,97,114,110,105,110,103,95,116,101,120,116,91,52,93,0,105,110,102,111,95,
105,110,102,111,91,52,93,0,105,110,102,111,95,105,110,102,111,95,116,101,120,116,91,52,93,0,105,110,102,111,95,100,101,98,117,103,91,52,93,0,105,110,102,111,95,100,101,98,117,103,95,116,101,120,116,91,52,93,0,112,97,105,110,116,95,99,117,114,118,101,95,112,105,118,111,116,91,52,93,0,112,97,105,110,116,95,99,117,114,118,101,95,104,97,110,100,108,101,91,52,93,0,109,101,116,97,100,97,116,97,98,103,91,52,93,0,109,101,116,97,
100,97,116,97,116,101,120,116,91,52,93,0,115,111,108,105,100,91,52,93,0,116,117,105,0,116,98,117,116,115,0,116,118,51,100,0,116,102,105,108,101,0,116,105,112,111,0,116,105,110,102,111,0,116,97,99,116,0,116,110,108,97,0,116,115,101,113,0,116,105,109,97,0,116,101,120,116,0,116,111,111,112,115,0,116,116,105,109,101,0,116,110,111,100,101,0,116,108,111,103,105,99,0,116,117,115,101,114,112,114,101,102,0,116,99,111,110,115,111,108,
101,0,116,99,108,105,112,0,116,97,114,109,91,50,48,93,0,97,99,116,105,118,101,95,116,104,101,109,101,95,97,114,101,97,0,109,111,100,117,108,101,91,54,52,93,0,112,97,116,104,91,55,54,56,93,0,115,112,101,99,91,52,93,0,109,111,117,115,101,95,115,112,101,101,100,0,119,97,108,107,95,115,112,101,101,100,0,119,97,108,107,95,115,112,101,101,100,95,102,97,99,116,111,114,0,118,105,101,119,95,104,101,105,103,104,116,0,106,117,109,
112,95,104,101,105,103,104,116,0,116,101,108,101,112,111,114,116,95,116,105,109,101,0,100,117,112,102,108,97,103,0,115,97,118,101,116,105,109,101,0,116,101,109,112,100,105,114,91,55,54,56,93,0,102,111,110,116,100,105,114,91,55,54,56,93,0,114,101,110,100,101,114,100,105,114,91,49,48,50,52,93,0,114,101,110,100,101,114,95,99,97,99,104,101,100,105,114,91,55,54,56,93,0,116,101,120,116,117,100,105,114,91,55,54,56,93,0,112,121,116,
104,111,110,100,105,114,91,55,54,56,93,0,115,111,117,110,100,100,105,114,91,55,54,56,93,0,105,49,56,110,100,105,114,91,55,54,56,93,0,105,109,97,103,101,95,101,100,105,116,111,114,91,49,48,50,52,93,0,97,110,105,109,95,112,108,97,121,101,114,91,49,48,50,52,93,0,97,110,105,109,95,112,108,97,121,101,114,95,112,114,101,115,101,116,0,118,50,100,95,109,105,110,95,103,114,105,100,115,105,122,101,0,116,105,109,101,99,111,100,101,
95,115,116,121,108,101,0,118,101,114,115,105,111,110,115,0,100,98,108,95,99,108,105,99,107,95,116,105,109,101,0,103,97,109,101,102,108,97,103,115,0,119,104,101,101,108,108,105,110,101,115,99,114,111,108,108,0,117,105,102,108,97,103,0,117,105,102,108,97,103,50,0,108,97,110,103,117,97,103,101,0,117,115,101,114,112,114,101,102,0,118,105,101,119,122,111,111,109,0,109,105,120,98,117,102,115,105,122,101,0,97,117,100,105,111,100,101,118,105,99,
101,0,97,117,100,105,111,114,97,116,101,0,97,117,100,105,111,102,111,114,109,97,116,0,97,117,100,105,111,99,104,97,110,110,101,108,115,0,100,112,105,0,117,105,95,115,99,97,108,101,0,117,105,95,108,105,110,101,95,119,105,100,116,104,0,110,111,100,101,95,109,97,114,103,105,110,0,116,114,97,110,115,111,112,116,115,0,109,101,110,117,116,104,114,101,115,104,111,108,100,49,0,109,101,110,117,116,104,114,101,115,104,111,108,100,50,0,97,112,112,
95,116,101,109,112,108,97,116,101,91,54,52,93,0,116,104,101,109,101,115,0,117,105,102,111,110,116,115,0,117,105,115,116,121,108,101,115,0,107,101,121,109,97,112,115,0,117,115,101,114,95,107,101,121,109,97,112,115,0,97,100,100,111,110,115,0,97,117,116,111,101,120,101,99,95,112,97,116,104,115,0,107,101,121,99,111,110,102,105,103,115,116,114,91,54,52,93,0,117,110,100,111,115,116,101,112,115,0,117,110,100,111,109,101,109,111,114,121,0,103,
112,95,109,97,110,104,97,116,116,101,110,100,105,115,116,0,103,112,95,101,117,99,108,105,100,101,97,110,100,105,115,116,0,103,112,95,101,114,97,115,101,114,0,103,112,95,115,101,116,116,105,110,103,115,0,116,98,95,108,101,102,116,109,111,117,115,101,0,116,98,95,114,105,103,104,116,109,111,117,115,101,0,108,105,103,104,116,91,51,93,0,116,119,95,104,111,116,115,112,111,116,0,116,119,95,102,108,97,103,0,116,119,95,104,97,110,100,108,101,115,
105,122,101,0,116,119,95,115,105,122,101,0,116,101,120,116,105,109,101,111,117,116,0,116,101,120,99,111,108,108,101,99,116,114,97,116,101,0,119,109,100,114,97,119,109,101,116,104,111,100,0,100,114,97,103,116,104,114,101,115,104,111,108,100,0,109,101,109,99,97,99,104,101,108,105,109,105,116,0,112,114,101,102,101,116,99,104,102,114,97,109,101,115,0,112,97,100,95,114,111,116,95,97,110,103,108,101,0,102,114,97,109,101,115,101,114,118,101,114,112,
111,114,116,0,111,98,99,101,110,116,101,114,95,100,105,97,0,114,118,105,115,105,122,101,0,114,118,105,98,114,105,103,104,116,0,114,101,99,101,110,116,95,102,105,108,101,115,0,115,109,111,111,116,104,95,118,105,101,119,116,120,0,103,108,114,101,115,108,105,109,105,116,0,99,117,114,115,115,105,122,101,0,99,111,108,111,114,95,112,105,99,107,101,114,95,116,121,112,101,0,105,112,111,95,110,101,119,0,107,101,121,104,97,110,100,108,101,115,95,110,
101,119,0,103,112,117,95,115,101,108,101,99,116,95,109,101,116,104,111,100,0,103,112,117,95,115,101,108,101,99,116,95,112,105,99,107,95,100,101,112,104,0,118,105,101,119,95,102,114,97,109,101,95,116,121,112,101,0,118,105,101,119,95,102,114,97,109,101,95,107,101,121,102,114,97,109,101,115,0,118,105,101,119,95,102,114,97,109,101,95,115,101,99,111,110,100,115,0,115,99,114,99,97,115,116,102,112,115,0,115,99,114,99,97,115,116,119,97,105,116,
0,119,105,100,103,101,116,95,117,110,105,116,0,97,110,105,115,111,116,114,111,112,105,99,95,102,105,108,116,101,114,0,117,115,101,95,49,54,98,105,116,95,116,101,120,116,117,114,101,115,0,117,115,101,95,103,112,117,95,109,105,112,109,97,112,0,110,100,111,102,95,115,101,110,115,105,116,105,118,105,116,121,0,110,100,111,102,95,111,114,98,105,116,95,115,101,110,115,105,116,105,118,105,116,121,0,110,100,111,102,95,100,101,97,100,122,111,110,101,0,
110,100,111,102,95,102,108,97,103,0,111,103,108,95,109,117,108,116,105,115,97,109,112,108,101,115,0,105,109,97,103,101,95,100,114,97,119,95,109,101,116,104,111,100,0,103,108,97,108,112,104,97,99,108,105,112,0,116,101,120,116,95,114,101,110,100,101,114,0,99,111,98,97,95,119,101,105,103,104,116,0,115,99,117,108,112,116,95,112,97,105,110,116,95,111,118,101,114,108,97,121,95,99,111,108,91,51,93,0,103,112,101,110,99,105,108,95,110,101,119,
95,108,97,121,101,114,95,99,111,108,91,52,93,0,116,119,101,97,107,95,116,104,114,101,115,104,111,108,100,0,110,97,118,105,103,97,116,105,111,110,95,109,111,100,101,0,97,117,116,104,111,114,91,56,48,93,0,102,111,110,116,95,112,97,116,104,95,117,105,91,49,48,50,52,93,0,102,111,110,116,95,112,97,116,104,95,117,105,95,109,111,110,111,91,49,48,50,52,93,0,99,111,109,112,117,116,101,95,100,101,118,105,99,101,95,116,121,112,101,0,
99,111,109,112,117,116,101,95,100,101,118,105,99,101,95,105,100,0,102,99,117,95,105,110,97,99,116,105,118,101,95,97,108,112,104,97,0,112,105,120,101,108,115,105,122,101,0,118,105,114,116,117,97,108,95,112,105,120,101,108,0,112,105,101,95,105,110,116,101,114,97,99,116,105,111,110,95,116,121,112,101,0,112,105,101,95,105,110,105,116,105,97,108,95,116,105,109,101,111,117,116,0,112,105,101,95,97,110,105,109,97,116,105,111,110,95,116,105,109,101,
111,117,116,0,112,105,101,95,109,101,110,117,95,99,111,110,102,105,114,109,0,112,105,101,95,109,101,110,117,95,114,97,100,105,117,115,0,112,105,101,95,109,101,110,117,95,116,104,114,101,115,104,111,108,100,0,119,97,108,107,95,110,97,118,105,103,97,116,105,111,110,0,111,112,101,110,115,117,98,100,105,118,95,99,111,109,112,117,116,101,95,116,121,112,101,0,112,97,100,53,91,54,93,0,118,101,114,116,98,97,115,101,0,101,100,103,101,98,97,115,
101,0,97,114,101,97,98,97,115,101,0,42,110,101,119,115,99,101,110,101,0,119,105,110,105,100,0,114,101,100,114,97,119,115,95,102,108,97,103,0,116,101,109,112,0,100,111,95,100,114,97,119,0,100,111,95,114,101,102,114,101,115,104,0,100,111,95,100,114,97,119,95,103,101,115,116,117,114,101,0,100,111,95,100,114,97,119,95,112,97,105,110,116,99,117,114,115,111,114,0,100,111,95,100,114,97,119,95,100,114,97,103,0,115,119,97,112,0,115,107,
105,112,95,104,97,110,100,108,105,110,103,0,115,99,114,117,98,98,105,110,103,0,109,97,105,110,119,105,110,0,115,117,98,119,105,110,97,99,116,105,118,101,0,42,97,110,105,109,116,105,109,101,114,0,42,99,111,110,116,101,120,116,0,42,110,101,119,118,0,118,101,99,0,42,118,49,0,42,118,50,0,42,116,121,112,101,0,112,97,110,101,108,110,97,109,101,91,54,52,93,0,116,97,98,110,97,109,101,91,54,52,93,0,100,114,97,119,110,97,109,
101,91,54,52,93,0,111,102,115,120,0,111,102,115,121,0,115,105,122,101,120,0,115,105,122,101,121,0,108,97,98,101,108,111,102,115,0,114,117,110,116,105,109,101,95,102,108,97,103,0,99,111,110,116,114,111,108,0,115,110,97,112,0,115,111,114,116,111,114,100,101,114,0,42,112,97,110,101,108,116,97,98,0,42,97,99,116,105,118,101,100,97,116,97,0,105,100,110,97,109,101,91,54,52,93,0,108,105,115,116,95,105,100,91,54,52,93,0,108,97,
121,111,117,116,95,116,121,112,101,0,108,105,115,116,95,115,99,114,111,108,108,0,108,105,115,116,95,103,114,105,112,0,108,105,115,116,95,108,97,115,116,95,108,101,110,0,108,105,115,116,95,108,97,115,116,95,97,99,116,105,118,101,105,0,102,105,108,116,101,114,95,98,121,110,97,109,101,91,54,52,93,0,102,105,108,116,101,114,95,102,108,97,103,0,102,105,108,116,101,114,95,115,111,114,116,95,102,108,97,103,0,42,100,121,110,95,100,97,116,97,
0,112,114,101,118,105,101,119,95,105,100,91,54,52,93,0,42,118,51,0,42,118,52,0,42,102,117,108,108,0,98,117,116,115,112,97,99,101,116,121,112,101,0,104,101,97,100,101,114,116,121,112,101,0,114,101,103,105,111,110,95,97,99,116,105,118,101,95,119,105,110,0,115,112,97,99,101,100,97,116,97,0,104,97,110,100,108,101,114,115,0,97,99,116,105,111,110,122,111,110,101,115,0,119,105,110,114,99,116,0,100,114,97,119,114,99,116,0,115,119,
105,110,105,100,0,114,101,103,105,111,110,116,121,112,101,0,97,108,105,103,110,109,101,110,116,0,100,111,95,100,114,97,119,95,111,118,101,114,108,97,121,0,111,118,101,114,108,97,112,0,102,108,97,103,102,117,108,108,115,99,114,101,101,110,0,117,105,98,108,111,99,107,115,0,112,97,110,101,108,115,0,112,97,110,101,108,115,95,99,97,116,101,103,111,114,121,95,97,99,116,105,118,101,0,117,105,95,108,105,115,116,115,0,117,105,95,112,114,101,118,
105,101,119,115,0,112,97,110,101,108,115,95,99,97,116,101,103,111,114,121,0,42,114,101,103,105,111,110,116,105,109,101,114,0,42,104,101,97,100,101,114,115,116,114,0,42,114,101,103,105,111,110,100,97,116,97,0,115,117,98,118,115,116,114,91,52,93,0,115,117,98,118,101,114,115,105,111,110,0,109,105,110,118,101,114,115,105,111,110,0,109,105,110,115,117,98,118,101,114,115,105,111,110,0,42,99,117,114,115,99,114,101,101,110,0,42,99,117,114,115,
99,101,110,101,0,102,105,108,101,102,108,97,103,115,0,103,108,111,98,97,108,102,0,98,117,105,108,100,95,99,111,109,109,105,116,95,116,105,109,101,115,116,97,109,112,0,98,117,105,108,100,95,104,97,115,104,91,49,54,93,0,111,114,105,103,95,119,105,100,116,104,0,111,114,105,103,95,104,101,105,103,104,116,0,98,111,116,116,111,109,0,114,105,103,104,116,0,120,111,102,115,0,121,111,102,115,0,108,105,102,116,91,51,93,0,103,97,109,109,97,
91,51,93,0,103,97,105,110,91,51,93,0,100,105,114,91,55,54,56,93,0,116,99,0,98,117,105,108,100,95,115,105,122,101,95,102,108,97,103,115,0,98,117,105,108,100,95,116,99,95,102,108,97,103,115,0,98,117,105,108,100,95,102,108,97,103,115,0,115,116,111,114,97,103,101,0,100,111,110,101,0,115,116,97,114,116,115,116,105,108,108,0,101,110,100,115,116,105,108,108,0,42,115,116,114,105,112,100,97,116,97,0,42,99,114,111,112,0,42,116,
114,97,110,115,102,111,114,109,0,42,99,111,108,111,114,95,98,97,108,97,110,99,101,0,42,116,109,112,0,115,116,97,114,116,111,102,115,0,101,110,100,111,102,115,0,109,97,99,104,105,110,101,0,115,116,97,114,116,100,105,115,112,0,101,110,100,100,105,115,112,0,115,97,116,0,109,117,108,0,104,97,110,100,115,105,122,101,0,97,110,105,109,95,112,114,101,115,101,101,107,0,115,116,114,101,97,109,105,110,100,101,120,0,109,117,108,116,105,99,97,
109,95,115,111,117,114,99,101,0,99,108,105,112,95,102,108,97,103,0,42,115,116,114,105,112,0,42,115,99,101,110,101,95,99,97,109,101,114,97,0,101,102,102,101,99,116,95,102,97,100,101,114,0,115,112,101,101,100,95,102,97,100,101,114,0,42,115,101,113,49,0,42,115,101,113,50,0,42,115,101,113,51,0,115,101,113,98,97,115,101,0,42,115,111,117,110,100,0,42,115,99,101,110,101,95,115,111,117,110,100,0,112,105,116,99,104,0,112,97,110,
0,115,116,114,111,98,101,0,42,101,102,102,101,99,116,100,97,116,97,0,97,110,105,109,95,115,116,97,114,116,111,102,115,0,97,110,105,109,95,101,110,100,111,102,115,0,98,108,101,110,100,95,109,111,100,101,0,98,108,101,110,100,95,111,112,97,99,105,116,121,0,42,111,108,100,98,97,115,101,112,0,42,112,97,114,115,101,113,0,100,105,115,112,95,114,97,110,103,101,91,50,93,0,42,115,101,113,98,97,115,101,112,0,109,101,116,97,115,116,97,
99,107,0,42,97,99,116,95,115,101,113,0,97,99,116,95,105,109,97,103,101,100,105,114,91,49,48,50,52,93,0,97,99,116,95,115,111,117,110,100,100,105,114,91,49,48,50,52,93,0,112,114,111,120,121,95,100,105,114,91,49,48,50,52,93,0,111,118,101,114,95,111,102,115,0,111,118,101,114,95,99,102,114,97,0,111,118,101,114,95,102,108,97,103,0,112,114,111,120,121,95,115,116,111,114,97,103,101,0,111,118,101,114,95,98,111,114,100,101,114,
0,101,100,103,101,87,105,100,116,104,0,102,111,114,119,97,114,100,0,119,105,112,101,116,121,112,101,0,102,77,105,110,105,0,102,67,108,97,109,112,0,102,66,111,111,115,116,0,100,68,105,115,116,0,100,81,117,97,108,105,116,121,0,98,78,111,67,111,109,112,0,83,99,97,108,101,120,73,110,105,0,83,99,97,108,101,121,73,110,105,0,120,73,110,105,0,121,73,110,105,0,114,111,116,73,110,105,0,105,110,116,101,114,112,111,108,97,116,105,111,
110,0,117,110,105,102,111,114,109,95,115,99,97,108,101,0,42,102,114,97,109,101,77,97,112,0,103,108,111,98,97,108,83,112,101,101,100,0,108,97,115,116,86,97,108,105,100,70,114,97,109,101,0,115,105,122,101,95,120,0,115,105,122,101,95,121,0,116,101,120,116,91,53,49,50,93,0,116,101,120,116,95,115,105,122,101,0,99,111,108,111,114,91,52,93,0,115,104,97,100,111,119,95,99,111,108,111,114,91,52,93,0,119,114,97,112,95,119,105,100,
116,104,0,109,97,115,107,95,105,110,112,117,116,95,116,121,112,101,0,109,97,115,107,95,116,105,109,101,0,42,109,97,115,107,95,115,101,113,117,101,110,99,101,0,42,109,97,115,107,95,105,100,0,99,111,108,111,114,95,98,97,108,97,110,99,101,0,99,111,108,111,114,95,109,117,108,116,105,112,108,121,0,99,117,114,118,101,95,109,97,112,112,105,110,103,0,119,104,105,116,101,95,118,97,108,117,101,91,51,93,0,107,101,121,0,103,97,109,109,97,
0,105,110,116,101,110,115,105,116,121,0,97,100,97,112,116,97,116,105,111,110,0,99,111,114,114,101,99,116,105,111,110,0,42,114,101,102,101,114,101,110,99,101,95,105,98,117,102,0,42,122,101,98,114,97,95,105,98,117,102,0,42,119,97,118,101,102,111,114,109,95,105,98,117,102,0,42,115,101,112,95,119,97,118,101,102,111,114,109,95,105,98,117,102,0,42,118,101,99,116,111,114,95,105,98,117,102,0,42,104,105,115,116,111,103,114,97,109,95,105,
98,117,102,0,98,117,116,116,121,112,101,0,117,115,101,114,106,105,116,0,115,116,97,0,101,110,100,0,116,111,116,112,97,114,116,0,110,111,114,109,102,97,99,0,111,98,102,97,99,0,114,97,110,100,102,97,99,0,116,101,120,102,97,99,0,114,97,110,100,108,105,102,101,0,102,111,114,99,101,91,51,93,0,118,101,99,116,115,105,122,101,0,109,97,120,108,101,110,0,100,101,102,118,101,99,91,51,93,0,109,117,108,116,91,52,93,0,108,105,102,
101,91,52,93,0,99,104,105,108,100,91,52,93,0,109,97,116,91,52,93,0,116,101,120,109,97,112,0,99,117,114,109,117,108,116,0,115,116,97,116,105,99,115,116,101,112,0,111,109,97,116,0,116,105,109,101,116,101,120,0,115,112,101,101,100,116,101,120,0,102,108,97,103,50,110,101,103,0,118,101,114,116,103,114,111,117,112,95,118,0,118,103,114,111,117,112,110,97,109,101,91,54,52,93,0,118,103,114,111,117,112,110,97,109,101,95,118,91,54,52,
93,0,42,107,101,121,115,0,109,105,110,102,97,99,0,110,114,0,117,115,101,100,0,117,115,101,100,101,108,101,109,0,114,101,115,101,116,100,105,115,116,0,108,97,115,116,118,97,108,0,112,114,111,112,110,97,109,101,91,54,52,93,0,109,97,116,110,97,109,101,91,54,52,93,0,42,109,97,0,113,117,97,108,0,113,117,97,108,50,0,116,97,114,103,101,116,78,97,109,101,91,54,52,93,0,116,111,103,103,108,101,78,97,109,101,91,54,52,93,0,
118,97,108,117,101,91,54,52,93,0,109,97,120,118,97,108,117,101,91,54,52,93,0,100,101,108,97,121,0,100,117,114,97,116,105,111,110,0,109,97,116,101,114,105,97,108,78,97,109,101,91,54,52,93,0,100,97,109,112,116,105,109,101,114,0,97,120,105,115,102,108,97,103,0,112,111,115,101,99,104,97,110,110,101,108,91,54,52,93,0,99,111,110,115,116,114,97,105,110,116,91,54,52,93,0,42,102,114,111,109,79,98,106,101,99,116,0,115,117,98,
106,101,99,116,91,54,52,93,0,98,111,100,121,91,54,52,93,0,111,116,121,112,101,0,112,117,108,115,101,0,102,114,101,113,0,116,111,116,108,105,110,107,115,0,42,42,108,105,110,107,115,0,116,97,112,0,106,111,121,105,110,100,101,120,0,97,120,105,115,95,115,105,110,103,108,101,0,97,120,105,115,102,0,98,117,116,116,111,110,0,104,97,116,0,104,97,116,102,0,112,114,101,99,105,115,105,111,110,0,115,116,114,91,49,50,56,93,0,42,109,
121,110,101,119,0,105,110,112,117,116,115,0,116,111,116,115,108,105,110,107,115,0,42,42,115,108,105,110,107,115,0,118,97,108,111,0,115,116,97,116,101,95,109,97,115,107,0,42,97,99,116,0,102,114,97,109,101,80,114,111,112,91,54,52,93,0,98,108,101,110,100,105,110,0,112,114,105,111,114,105,116,121,0,101,110,100,95,114,101,115,101,116,0,115,116,114,105,100,101,97,120,105,115,0,115,116,114,105,100,101,108,101,110,103,116,104,0,108,97,121,
101,114,95,119,101,105,103,104,116,0,109,105,110,95,103,97,105,110,0,109,97,120,95,103,97,105,110,0,114,101,102,101,114,101,110,99,101,95,100,105,115,116,97,110,99,101,0,109,97,120,95,100,105,115,116,97,110,99,101,0,114,111,108,108,111,102,102,95,102,97,99,116,111,114,0,99,111,110,101,95,105,110,110,101,114,95,97,110,103,108,101,0,99,111,110,101,95,111,117,116,101,114,95,97,110,103,108,101,0,99,111,110,101,95,111,117,116,101,114,95,
103,97,105,110,0,115,110,100,110,114,0,112,97,100,51,91,50,93,0,115,111,117,110,100,51,68,0,112,97,100,54,91,49,93,0,42,109,101,0,108,105,110,86,101,108,111,99,105,116,121,91,51,93,0,97,110,103,86,101,108,111,99,105,116,121,91,51,93,0,108,111,99,97,108,102,108,97,103,0,100,121,110,95,111,112,101,114,97,116,105,111,110,0,102,111,114,99,101,108,111,99,91,51,93,0,102,111,114,99,101,114,111,116,91,51,93,0,108,105,110,
101,97,114,118,101,108,111,99,105,116,121,91,51,93,0,97,110,103,117,108,97,114,118,101,108,111,99,105,116,121,91,51,93,0,42,114,101,102,101,114,101,110,99,101,0,109,105,110,0,109,97,120,0,114,111,116,100,97,109,112,0,109,105,110,108,111,99,91,51,93,0,109,97,120,108,111,99,91,51,93,0,109,105,110,114,111,116,91,51,93,0,109,97,120,114,111,116,91,51,93,0,109,97,116,112,114,111,112,91,54,52,93,0,98,117,116,115,116,97,0,
98,117,116,101,110,100,0,100,105,115,116,114,105,98,117,116,105,111,110,0,105,110,116,95,97,114,103,95,49,0,105,110,116,95,97,114,103,95,50,0,102,108,111,97,116,95,97,114,103,95,49,0,102,108,111,97,116,95,97,114,103,95,50,0,116,111,80,114,111,112,78,97,109,101,91,54,52,93,0,42,116,111,79,98,106,101,99,116,0,98,111,100,121,84,121,112,101,0,102,105,108,101,110,97,109,101,91,54,52,93,0,108,111,97,100,97,110,105,110,97,
109,101,91,54,52,93,0,105,110,116,95,97,114,103,0,102,108,111,97,116,95,97,114,103,0,42,115,117,98,116,97,114,103,101,116,0,102,97,99,105,110,103,97,120,105,115,0,118,101,108,111,99,105,116,121,0,97,99,99,101,108,101,114,97,116,105,111,110,0,116,117,114,110,115,112,101,101,100,0,117,112,100,97,116,101,84,105,109,101,0,42,110,97,118,109,101,115,104,0,111,98,106,101,99,116,95,97,120,105,115,91,50,93,0,116,104,114,101,115,104,
111,108,100,91,50,93,0,115,101,110,115,105,116,105,118,105,116,121,91,50,93,0,108,105,109,105,116,95,120,91,50,93,0,108,105,109,105,116,95,121,91,50,93,0,103,111,0,42,104,97,110,100,108,101,0,42,110,101,119,112,97,99,107,101,100,102,105,108,101,0,42,119,97,118,101,102,111,114,109,0,42,115,112,105,110,108,111,99,107,0,42,108,97,109,112,114,101,110,0,103,111,98,106,101,99,116,0,100,117,112,108,105,95,111,102,115,91,51,93,0,
99,104,105,108,100,98,97,115,101,0,114,111,108,108,0,104,101,97,100,91,51,93,0,116,97,105,108,91,51,93,0,98,111,110,101,95,109,97,116,91,51,93,91,51,93,0,97,114,109,95,104,101,97,100,91,51,93,0,97,114,109,95,116,97,105,108,91,51,93,0,97,114,109,95,109,97,116,91,52,93,91,52,93,0,97,114,109,95,114,111,108,108,0,120,119,105,100,116,104,0,122,119,105,100,116,104,0,101,97,115,101,49,0,101,97,115,101,50,0,114,
97,100,95,104,101,97,100,0,114,97,100,95,116,97,105,108,0,114,111,108,108,49,0,114,111,108,108,50,0,99,117,114,118,101,73,110,88,0,99,117,114,118,101,73,110,89,0,99,117,114,118,101,79,117,116,88,0,99,117,114,118,101,79,117,116,89,0,115,99,97,108,101,73,110,0,115,99,97,108,101,79,117,116,0,115,101,103,109,101,110,116,115,0,98,111,110,101,98,97,115,101,0,99,104,97,105,110,98,97,115,101,0,42,101,100,98,111,0,42,97,
99,116,95,98,111,110,101,0,42,97,99,116,95,101,100,98,111,110,101,0,42,115,107,101,116,99,104,0,103,101,118,101,114,116,100,101,102,111,114,109,101,114,0,108,97,121,101,114,95,117,115,101,100,0,108,97,121,101,114,95,112,114,111,116,101,99,116,101,100,0,103,104,111,115,116,101,112,0,103,104,111,115,116,115,105,122,101,0,103,104,111,115,116,116,121,112,101,0,112,97,116,104,115,105,122,101,0,103,104,111,115,116,115,102,0,103,104,111,115,116,
101,102,0,112,97,116,104,115,102,0,112,97,116,104,101,102,0,112,97,116,104,98,99,0,112,97,116,104,97,99,0,42,112,111,105,110,116,115,0,115,116,97,114,116,95,102,114,97,109,101,0,101,110,100,95,102,114,97,109,101,0,108,105,110,101,95,116,104,105,99,107,110,101,115,115,0,103,104,111,115,116,95,115,102,0,103,104,111,115,116,95,101,102,0,103,104,111,115,116,95,98,99,0,103,104,111,115,116,95,97,99,0,103,104,111,115,116,95,116,121,
112,101,0,103,104,111,115,116,95,115,116,101,112,0,103,104,111,115,116,95,102,108,97,103,0,112,97,116,104,95,116,121,112,101,0,112,97,116,104,95,115,116,101,112,0,112,97,116,104,95,118,105,101,119,102,108,97,103,0,112,97,116,104,95,98,97,107,101,102,108,97,103,0,112,97,116,104,95,115,102,0,112,97,116,104,95,101,102,0,112,97,116,104,95,98,99,0,112,97,116,104,95,97,99,0,105,107,102,108,97,103,0,97,103,114,112,95,105,110,100,
101,120,0,99,111,110,115,116,102,108,97,103,0,115,101,108,101,99,116,102,108,97,103,0,98,98,111,110,101,102,108,97,103,0,112,97,100,48,91,52,93,0,42,98,111,110,101,0,42,99,104,105,108,100,0,105,107,116,114,101,101,0,115,105,107,116,114,101,101,0,42,99,117,115,116,111,109,0,42,99,117,115,116,111,109,95,116,120,0,99,117,115,116,111,109,95,115,99,97,108,101,0,101,117,108,91,51,93,0,99,104,97,110,95,109,97,116,91,52,93,
91,52,93,0,112,111,115,101,95,109,97,116,91,52,93,91,52,93,0,112,111,115,101,95,104,101,97,100,91,51,93,0,112,111,115,101,95,116,97,105,108,91,51,93,0,108,105,109,105,116,109,105,110,91,51,93,0,108,105,109,105,116,109,97,120,91,51,93,0,115,116,105,102,102,110,101,115,115,91,51,93,0,105,107,115,116,114,101,116,99,104,0,105,107,114,111,116,119,101,105,103,104,116,0,105,107,108,105,110,119,101,105,103,104,116,0,42,98,98,111,
110,101,95,112,114,101,118,0,42,98,98,111,110,101,95,110,101,120,116,0,42,116,101,109,112,0,99,104,97,110,98,97,115,101,0,42,99,104,97,110,104,97,115,104,0,112,114,111,120,121,95,108,97,121,101,114,0,115,116,114,105,100,101,95,111,102,102,115,101,116,91,51,93,0,99,121,99,108,105,99,95,111,102,102,115,101,116,91,51,93,0,97,103,114,111,117,112,115,0,97,99,116,105,118,101,95,103,114,111,117,112,0,105,107,115,111,108,118,101,114,
0,42,105,107,100,97,116,97,0,42,105,107,112,97,114,97,109,0,112,114,111,120,121,95,97,99,116,95,98,111,110,101,91,54,52,93,0,110,117,109,105,116,101,114,0,110,117,109,115,116,101,112,0,109,105,110,115,116,101,112,0,109,97,120,115,116,101,112,0,102,101,101,100,98,97,99,107,0,109,97,120,118,101,108,0,100,97,109,112,109,97,120,0,100,97,109,112,101,112,115,0,99,104,97,110,110,101,108,115,0,99,117,115,116,111,109,67,111,108,0,
99,115,0,99,117,114,118,101,115,0,103,114,111,117,112,115,0,97,99,116,105,118,101,95,109,97,114,107,101,114,0,105,100,114,111,111,116,0,42,102,105,108,116,101,114,95,103,114,112,0,115,101,97,114,99,104,115,116,114,91,54,52,93,0,102,105,108,116,101,114,102,108,97,103,0,114,101,110,97,109,101,73,110,100,101,120,0,97,100,115,0,116,105,109,101,115,108,105,100,101,0,42,103,114,112,0,110,97,109,101,91,51,48,93,0,111,119,110,115,112,
97,99,101,0,116,97,114,115,112,97,99,101,0,101,110,102,111,114,99,101,0,104,101,97,100,116,97,105,108,0,108,105,110,95,101,114,114,111,114,0,114,111,116,95,101,114,114,111,114,0,42,116,97,114,0,109,97,116,114,105,120,91,52,93,91,52,93,0,114,111,116,79,114,100,101,114,0,116,97,114,110,117,109,0,116,97,114,103,101,116,115,0,105,116,101,114,97,116,105,111,110,115,0,114,111,111,116,98,111,110,101,0,109,97,120,95,114,111,111,116,
98,111,110,101,0,42,112,111,108,101,116,97,114,0,112,111,108,101,115,117,98,116,97,114,103,101,116,91,54,52,93,0,112,111,108,101,97,110,103,108,101,0,111,114,105,101,110,116,119,101,105,103,104,116,0,103,114,97,98,116,97,114,103,101,116,91,51,93,0,110,117,109,112,111,105,110,116,115,0,99,104,97,105,110,108,101,110,0,120,122,83,99,97,108,101,77,111,100,101,0,98,117,108,103,101,0,98,117,108,103,101,95,109,105,110,0,98,117,108,103,
101,95,109,97,120,0,98,117,108,103,101,95,115,109,111,111,116,104,0,114,101,115,101,114,118,101,100,49,0,114,101,115,101,114,118,101,100,50,0,109,105,110,109,97,120,102,108,97,103,0,115,116,117,99,107,0,99,97,99,104,101,91,51,93,0,108,111,99,107,102,108,97,103,0,102,111,108,108,111,119,102,108,97,103,0,118,111,108,109,111,100,101,0,112,108,97,110,101,0,111,114,103,108,101,110,103,116,104,0,112,105,118,88,0,112,105,118,89,0,112,
105,118,90,0,97,120,88,0,97,120,89,0,97,120,90,0,109,105,110,76,105,109,105,116,91,54,93,0,109,97,120,76,105,109,105,116,91,54,93,0,101,120,116,114,97,70,122,0,105,110,118,109,97,116,91,52,93,91,52,93,0,102,114,111,109,0,116,111,0,109,97,112,91,51,93,0,101,120,112,111,0,102,114,111,109,95,109,105,110,91,51,93,0,102,114,111,109,95,109,97,120,91,51,93,0,116,111,95,109,105,110,91,51,93,0,116,111,95,109,97,
120,91,51,93,0,102,114,111,109,95,109,105,110,95,114,111,116,91,51,93,0,102,114,111,109,95,109,97,120,95,114,111,116,91,51,93,0,116,111,95,109,105,110,95,114,111,116,91,51,93,0,116,111,95,109,97,120,95,114,111,116,91,51,93,0,102,114,111,109,95,109,105,110,95,115,99,97,108,101,91,51,93,0,102,114,111,109,95,109,97,120,95,115,99,97,108,101,91,51,93,0,116,111,95,109,105,110,95,115,99,97,108,101,91,51,93,0,116,111,95,
109,97,120,95,115,99,97,108,101,91,51,93,0,114,111,116,65,120,105,115,0,122,109,105,110,0,122,109,97,120,0,112,114,111,106,65,120,105,115,83,112,97,99,101,0,116,114,97,99,107,91,54,52,93,0,102,114,97,109,101,95,109,101,116,104,111,100,0,111,98,106,101,99,116,91,54,52,93,0,42,100,101,112,116,104,95,111,98,0,99,104,97,110,110,101,108,91,51,50,93,0,110,111,95,114,111,116,95,97,120,105,115,0,115,116,114,105,100,101,95,
97,120,105,115,0,99,117,114,109,111,100,0,97,99,116,115,116,97,114,116,0,97,99,116,101,110,100,0,97,99,116,111,102,102,115,0,115,116,114,105,100,101,108,101,110,0,98,108,101,110,100,111,117,116,0,115,116,114,105,100,101,99,104,97,110,110,101,108,91,51,50,93,0,111,102,102,115,95,98,111,110,101,91,51,50,93,0,104,97,115,105,110,112,117,116,0,104,97,115,111,117,116,112,117,116,0,100,97,116,97,116,121,112,101,0,115,111,99,107,101,
116,116,121,112,101,0,105,115,95,99,111,112,121,0,101,120,116,101,114,110,97,108,0,42,110,101,119,95,115,111,99,107,0,105,100,101,110,116,105,102,105,101,114,91,54,52,93,0,42,115,116,111,114,97,103,101,0,108,105,109,105,116,0,105,110,95,111,117,116,0,42,116,121,112,101,105,110,102,111,0,108,111,99,120,0,108,111,99,121,0,42,100,101,102,97,117,108,116,95,118,97,108,117,101,0,115,116,97,99,107,95,105,110,100,101,120,0,115,116,97,
99,107,95,116,121,112,101,0,111,119,110,95,105,110,100,101,120,0,116,111,95,105,110,100,101,120,0,42,103,114,111,117,112,115,111,99,107,0,42,108,105,110,107,0,110,115,0,42,110,101,119,95,110,111,100,101,0,108,97,115,116,121,0,111,117,116,112,117,116,115,0,42,111,114,105,103,105,110,97,108,0,105,110,116,101,114,110,97,108,95,108,105,110,107,115,0,109,105,110,105,119,105,100,116,104,0,111,102,102,115,101,116,120,0,111,102,102,115,101,116,
121,0,97,110,105,109,95,105,110,105,116,95,108,111,99,120,0,97,110,105,109,95,111,102,115,120,0,117,112,100,97,116,101,0,108,97,98,101,108,91,54,52,93,0,99,117,115,116,111,109,49,0,99,117,115,116,111,109,50,0,99,117,115,116,111,109,51,0,99,117,115,116,111,109,52,0,110,101,101,100,95,101,120,101,99,0,101,120,101,99,0,42,116,104,114,101,97,100,100,97,116,97,0,116,111,116,114,0,98,117,116,114,0,112,114,118,114,0,112,114,
101,118,105,101,119,95,120,115,105,122,101,0,112,114,101,118,105,101,119,95,121,115,105,122,101,0,42,98,108,111,99,107,0,104,97,115,104,95,101,110,116,114,121,0,42,114,101,99,116,0,120,115,105,122,101,0,121,115,105,122,101,0,42,102,114,111,109,110,111,100,101,0,42,116,111,110,111,100,101,0,42,102,114,111,109,115,111,99,107,0,42,116,111,115,111,99,107,0,42,105,110,116,101,114,102,97,99,101,95,116,121,112,101,0,110,111,100,101,115,0,
108,105,110,107,115,0,105,110,105,116,0,99,117,114,95,105,110,100,101,120,0,105,115,95,117,112,100,97,116,105,110,103,0,110,111,100,101,116,121,112,101,0,101,100,105,116,95,113,117,97,108,105,116,121,0,114,101,110,100,101,114,95,113,117,97,108,105,116,121,0,99,104,117,110,107,115,105,122,101,0,118,105,101,119,101,114,95,98,111,114,100,101,114,0,42,112,114,101,118,105,101,119,115,0,97,99,116,105,118,101,95,118,105,101,119,101,114,95,107,101,
121,0,42,101,120,101,99,100,97,116,97,0,40,42,112,114,111,103,114,101,115,115,41,40,41,0,40,42,115,116,97,116,115,95,100,114,97,119,41,40,41,0,40,42,116,101,115,116,95,98,114,101,97,107,41,40,41,0,40,42,117,112,100,97,116,101,95,100,114,97,119,41,40,41,0,42,116,98,104,0,42,112,114,104,0,42,115,100,104,0,42,117,100,104,0,42,100,117,112,108,105,108,111,99,107,0,118,97,108,117,101,91,51,93,0,118,97,108,117,101,
91,52,93,0,118,97,108,117,101,91,49,48,50,52,93,0,108,97,98,101,108,95,115,105,122,101,0,99,121,99,108,105,99,0,109,111,118,105,101,0,103,97,105,110,0,108,105,102,116,0,109,97,115,116,101,114,0,115,104,97,100,111,119,115,0,109,105,100,116,111,110,101,115,0,104,105,103,104,108,105,103,104,116,115,0,115,116,97,114,116,109,105,100,116,111,110,101,115,0,101,110,100,109,105,100,116,111,110,101,115,0,102,108,97,112,115,0,114,111,117,
110,100,105,110,103,0,99,97,116,97,100,105,111,112,116,114,105,99,0,108,101,110,115,115,104,105,102,116,0,112,97,115,115,95,105,110,100,101,120,0,112,97,115,115,95,110,97,109,101,91,54,52,93,0,109,97,120,115,112,101,101,100,0,109,105,110,115,112,101,101,100,0,99,117,114,118,101,100,0,112,101,114,99,101,110,116,120,0,112,101,114,99,101,110,116,121,0,98,111,107,101,104,0,105,109,97,103,101,95,105,110,95,119,105,100,116,104,0,105,109,
97,103,101,95,105,110,95,104,101,105,103,104,116,0,99,101,110,116,101,114,95,120,0,99,101,110,116,101,114,95,121,0,115,112,105,110,0,119,114,97,112,0,115,105,103,109,97,95,99,111,108,111,114,0,115,105,103,109,97,95,115,112,97,99,101,0,104,117,101,0,98,97,115,101,95,112,97,116,104,91,49,48,50,52,93,0,102,111,114,109,97,116,0,97,99,116,105,118,101,95,105,110,112,117,116,0,117,115,101,95,114,101,110,100,101,114,95,102,111,114,
109,97,116,0,117,115,101,95,110,111,100,101,95,102,111,114,109,97,116,0,108,97,121,101,114,91,51,48,93,0,116,49,0,116,50,0,116,51,0,102,115,116,114,101,110,103,116,104,0,102,97,108,112,104,97,0,107,101,121,91,52,93,0,97,108,103,111,114,105,116,104,109,0,99,104,97,110,110,101,108,0,120,49,0,120,50,0,121,49,0,121,50,0,102,97,99,95,120,49,0,102,97,99,95,120,50,0,102,97,99,95,121,49,0,102,97,99,95,121,50,
0,99,111,108,110,97,109,101,91,54,52,93,0,98,107,116,121,112,101,0,112,97,100,95,99,49,0,103,97,109,99,111,0,110,111,95,122,98,117,102,0,109,97,120,98,108,117,114,0,98,116,104,114,101,115,104,0,42,100,105,99,116,0,42,110,111,100,101,0,115,116,97,114,95,52,53,0,115,116,114,101,97,107,115,0,99,111,108,109,111,100,0,109,105,120,0,102,97,100,101,0,97,110,103,108,101,95,111,102,115,0,109,0,99,0,106,105,116,0,112,
114,111,106,0,102,105,116,0,115,108,111,112,101,91,51,93,0,112,111,119,101,114,91,51,93,0,111,102,102,115,101,116,95,98,97,115,105,115,0,95,112,97,100,91,52,93,0,108,105,109,99,104,97,110,0,117,110,115,112,105,108,108,0,108,105,109,115,99,97,108,101,0,117,115,112,105,108,108,114,0,117,115,112,105,108,108,103,0,117,115,112,105,108,108,98,0,116,101,120,95,109,97,112,112,105,110,103,0,99,111,108,111,114,95,109,97,112,112,105,110,
103,0,115,107,121,95,109,111,100,101,108,0,115,117,110,95,100,105,114,101,99,116,105,111,110,91,51,93,0,116,117,114,98,105,100,105,116,121,0,103,114,111,117,110,100,95,97,108,98,101,100,111,0,99,111,108,111,114,95,115,112,97,99,101,0,112,114,111,106,101,99,116,105,111,110,0,112,114,111,106,101,99,116,105,111,110,95,98,108,101,110,100,0,101,120,116,101,110,115,105,111,110,0,111,102,102,115,101,116,95,102,114,101,113,0,115,113,117,97,115,
104,95,102,114,101,113,0,115,113,117,97,115,104,0,103,114,97,100,105,101,110,116,95,116,121,112,101,0,99,111,108,111,114,105,110,103,0,109,117,115,103,114,97,118,101,95,116,121,112,101,0,119,97,118,101,95,116,121,112,101,0,119,97,118,101,95,112,114,111,102,105,108,101,0,99,111,110,118,101,114,116,95,102,114,111,109,0,99,111,110,118,101,114,116,95,116,111,0,112,111,105,110,116,95,115,111,117,114,99,101,0,112,97,114,116,105,99,108,101,95,
115,121,115,116,101,109,0,112,100,0,99,97,99,104,101,100,95,114,101,115,111,108,117,116,105,111,110,0,116,114,97,99,107,105,110,103,95,111,98,106,101,99,116,91,54,52,93,0,115,99,114,101,101,110,95,98,97,108,97,110,99,101,0,100,101,115,112,105,108,108,95,102,97,99,116,111,114,0,100,101,115,112,105,108,108,95,98,97,108,97,110,99,101,0,101,100,103,101,95,107,101,114,110,101,108,95,114,97,100,105,117,115,0,101,100,103,101,95,107,101,
114,110,101,108,95,116,111,108,101,114,97,110,99,101,0,99,108,105,112,95,98,108,97,99,107,0,99,108,105,112,95,119,104,105,116,101,0,100,105,108,97,116,101,95,100,105,115,116,97,110,99,101,0,102,101,97,116,104,101,114,95,100,105,115,116,97,110,99,101,0,102,101,97,116,104,101,114,95,102,97,108,108,111,102,102,0,98,108,117,114,95,112,114,101,0,98,108,117,114,95,112,111,115,116,0,116,114,97,99,107,95,110,97,109,101,91,54,52,93,0,
119,114,97,112,95,97,120,105,115,0,112,108,97,110,101,95,116,114,97,99,107,95,110,97,109,101,91,54,52,93,0,109,111,116,105,111,110,95,98,108,117,114,95,115,97,109,112,108,101,115,0,109,111,116,105,111,110,95,98,108,117,114,95,115,104,117,116,116,101,114,0,98,121,116,101,99,111,100,101,95,104,97,115,104,91,54,52,93,0,42,98,121,116,101,99,111,100,101,0,100,105,114,101,99,116,105,111,110,95,116,121,112,101,0,117,118,95,109,97,112,
91,54,52,93,0,115,111,117,114,99,101,91,50,93,0,114,97,121,95,108,101,110,103,116,104,0,115,104,111,114,116,121,0,109,105,110,116,97,98,108,101,0,109,97,120,116,97,98,108,101,0,101,120,116,95,105,110,91,50,93,0,101,120,116,95,111,117,116,91,50,93,0,42,99,117,114,118,101,0,42,116,97,98,108,101,0,42,112,114,101,109,117,108,116,97,98,108,101,0,112,114,101,109,117,108,95,101,120,116,95,105,110,91,50,93,0,112,114,101,109,
117,108,95,101,120,116,95,111,117,116,91,50,93,0,112,114,101,115,101,116,0,99,104,97,110,103,101,100,95,116,105,109,101,115,116,97,109,112,0,99,117,114,114,0,99,108,105,112,114,0,99,109,91,52,93,0,98,108,97,99,107,91,51,93,0,119,104,105,116,101,91,51,93,0,98,119,109,117,108,91,51,93,0,115,97,109,112,108,101,91,51,93,0,120,95,114,101,115,111,108,117,116,105,111,110,0,100,97,116,97,95,108,117,109,97,91,50,53,54,93,
0,100,97,116,97,95,114,91,50,53,54,93,0,100,97,116,97,95,103,91,50,53,54,93,0,100,97,116,97,95,98,91,50,53,54,93,0,100,97,116,97,95,97,91,50,53,54,93,0,99,111,91,50,93,91,50,93,0,115,97,109,112,108,101,95,102,117,108,108,0,115,97,109,112,108,101,95,108,105,110,101,115,0,97,99,99,117,114,97,99,121,0,119,97,118,101,102,114,109,95,109,111,100,101,0,119,97,118,101,102,114,109,95,97,108,112,104,97,0,119,
97,118,101,102,114,109,95,121,102,97,99,0,119,97,118,101,102,114,109,95,104,101,105,103,104,116,0,118,101,99,115,99,111,112,101,95,97,108,112,104,97,0,118,101,99,115,99,111,112,101,95,104,101,105,103,104,116,0,109,105,110,109,97,120,91,51,93,91,50,93,0,104,105,115,116,0,42,119,97,118,101,102,111,114,109,95,49,0,42,119,97,118,101,102,111,114,109,95,50,0,42,119,97,118,101,102,111,114,109,95,51,0,42,118,101,99,115,99,111,112,
101,0,119,97,118,101,102,111,114,109,95,116,111,116,0,108,111,111,107,91,54,52,93,0,118,105,101,119,95,116,114,97,110,115,102,111,114,109,91,54,52,93,0,42,99,117,114,118,101,95,109,97,112,112,105,110,103,0,42,112,97,100,50,0,100,105,115,112,108,97,121,95,100,101,118,105,99,101,91,54,52,93,0,111,102,102,115,101,116,91,50,93,0,99,108,111,110,101,0,109,116,101,120,0,109,97,115,107,95,109,116,101,120,0,42,116,111,103,103,108,
101,95,98,114,117,115,104,0,42,105,99,111,110,95,105,109,98,117,102,0,42,103,114,97,100,105,101,110,116,0,42,112,97,105,110,116,95,99,117,114,118,101,0,105,99,111,110,95,102,105,108,101,112,97,116,104,91,49,48,50,52,93,0,110,111,114,109,97,108,95,119,101,105,103,104,116,0,114,97,107,101,95,102,97,99,116,111,114,0,111,98,95,109,111,100,101,0,109,97,115,107,95,112,114,101,115,115,117,114,101,0,106,105,116,116,101,114,0,106,105,
116,116,101,114,95,97,98,115,111,108,117,116,101,0,111,118,101,114,108,97,121,95,102,108,97,103,115,0,115,109,111,111,116,104,95,115,116,114,111,107,101,95,114,97,100,105,117,115,0,115,109,111,111,116,104,95,115,116,114,111,107,101,95,102,97,99,116,111,114,0,114,97,116,101,0,115,99,117,108,112,116,95,112,108,97,110,101,0,112,108,97,110,101,95,111,102,102,115,101,116,0,103,114,97,100,105,101,110,116,95,115,112,97,99,105,110,103,0,103,114,
97,100,105,101,110,116,95,115,116,114,111,107,101,95,109,111,100,101,0,103,114,97,100,105,101,110,116,95,102,105,108,108,95,109,111,100,101,0,115,99,117,108,112,116,95,116,111,111,108,0,118,101,114,116,101,120,112,97,105,110,116,95,116,111,111,108,0,105,109,97,103,101,112,97,105,110,116,95,116,111,111,108,0,109,97,115,107,95,116,111,111,108,0,97,117,116,111,115,109,111,111,116,104,95,102,97,99,116,111,114,0,99,114,101,97,115,101,95,112,105,
110,99,104,95,102,97,99,116,111,114,0,112,108,97,110,101,95,116,114,105,109,0,116,101,120,116,117,114,101,95,115,97,109,112,108,101,95,98,105,97,115,0,116,101,120,116,117,114,101,95,111,118,101,114,108,97,121,95,97,108,112,104,97,0,109,97,115,107,95,111,118,101,114,108,97,121,95,97,108,112,104,97,0,99,117,114,115,111,114,95,111,118,101,114,108,97,121,95,97,108,112,104,97,0,115,104,97,114,112,95,116,104,114,101,115,104,111,108,100,0,
98,108,117,114,95,107,101,114,110,101,108,95,114,97,100,105,117,115,0,98,108,117,114,95,109,111,100,101,0,102,105,108,108,95,116,104,114,101,115,104,111,108,100,0,97,100,100,95,99,111,108,91,51,93,0,115,117,98,95,99,111,108,91,51,93,0,115,116,101,110,99,105,108,95,112,111,115,91,50,93,0,115,116,101,110,99,105,108,95,100,105,109,101,110,115,105,111,110,91,50,93,0,109,97,115,107,95,115,116,101,110,99,105,108,95,112,111,115,91,50,
93,0,109,97,115,107,95,115,116,101,110,99,105,108,95,100,105,109,101,110,115,105,111,110,91,50,93,0,99,111,108,111,114,115,0,97,99,116,105,118,101,95,99,111,108,111,114,0,98,101,122,0,112,114,101,115,115,117,114,101,0,116,111,116,95,112,111,105,110,116,115,0,97,100,100,95,105,110,100,101,120,0,97,99,116,105,118,101,95,114,110,100,0,97,99,116,105,118,101,95,99,108,111,110,101,0,97,99,116,105,118,101,95,109,97,115,107,0,42,108,
97,121,101,114,115,0,116,121,112,101,109,97,112,91,52,50,93,0,116,111,116,108,97,121,101,114,0,109,97,120,108,97,121,101,114,0,116,111,116,115,105,122,101,0,42,112,111,111,108,0,42,101,120,116,101,114,110,97,108,0,119,111,114,108,100,95,99,111,91,51,93,0,114,111,116,91,52,93,0,97,118,101,91,51,93,0,42,103,114,111,117,110,100,0,119,97,110,100,101,114,91,51,93,0,114,101,115,116,95,108,101,110,103,116,104,0,112,97,114,116,
105,99,108,101,95,105,110,100,101,120,91,50,93,0,100,101,108,101,116,101,95,102,108,97,103,0,110,117,109,0,112,97,114,101,110,116,0,112,97,91,52,93,0,119,91,52,93,0,102,117,118,91,52,93,0,102,111,102,102,115,101,116,0,112,114,101,118,95,115,116,97,116,101,0,42,104,97,105,114,0,42,98,111,105,100,0,100,105,101,116,105,109,101,0,110,117,109,95,100,109,99,97,99,104,101,0,115,112,104,100,101,110,115,105,116,121,0,104,97,105,
114,95,105,110,100,101,120,0,97,108,105,118,101,0,115,112,114,105,110,103,95,107,0,112,108,97,115,116,105,99,105,116,121,95,99,111,110,115,116,97,110,116,0,121,105,101,108,100,95,114,97,116,105,111,0,112,108,97,115,116,105,99,105,116,121,95,98,97,108,97,110,99,101,0,121,105,101,108,100,95,98,97,108,97,110,99,101,0,118,105,115,99,111,115,105,116,121,95,111,109,101,103,97,0,118,105,115,99,111,115,105,116,121,95,98,101,116,97,0,115,
116,105,102,102,110,101,115,115,95,107,0,115,116,105,102,102,110,101,115,115,95,107,110,101,97,114,0,114,101,115,116,95,100,101,110,115,105,116,121,0,98,117,111,121,97,110,99,121,0,115,112,114,105,110,103,95,102,114,97,109,101,115,0,42,98,111,105,100,115,0,42,102,108,117,105,100,0,100,105,115,116,114,0,112,104,121,115,116,121,112,101,0,97,118,101,109,111,100,101,0,114,101,97,99,116,101,118,101,110,116,0,100,114,97,119,0,100,114,97,119,
95,97,115,0,100,114,97,119,95,115,105,122,101,0,99,104,105,108,100,116,121,112,101,0,114,101,110,95,97,115,0,115,117,98,102,114,97,109,101,115,0,100,114,97,119,95,99,111,108,0,114,101,110,95,115,116,101,112,0,104,97,105,114,95,115,116,101,112,0,107,101,121,115,95,115,116,101,112,0,97,100,97,112,116,95,97,110,103,108,101,0,97,100,97,112,116,95,112,105,120,0,105,110,116,101,103,114,97,116,111,114,0,114,111,116,102,114,111,109,0,
98,98,95,97,108,105,103,110,0,98,98,95,117,118,95,115,112,108,105,116,0,98,98,95,97,110,105,109,0,98,98,95,115,112,108,105,116,95,111,102,102,115,101,116,0,98,98,95,116,105,108,116,0,98,98,95,114,97,110,100,95,116,105,108,116,0,98,98,95,111,102,102,115,101,116,91,50,93,0,98,98,95,115,105,122,101,91,50,93,0,98,98,95,118,101,108,95,104,101,97,100,0,98,98,95,118,101,108,95,116,97,105,108,0,99,111,108,111,114,95,
118,101,99,95,109,97,120,0,115,105,109,112,108,105,102,121,95,114,101,102,115,105,122,101,0,115,105,109,112,108,105,102,121,95,114,97,116,101,0,115,105,109,112,108,105,102,121,95,116,114,97,110,115,105,116,105,111,110,0,115,105,109,112,108,105,102,121,95,118,105,101,119,112,111,114,116,0,116,105,109,101,116,119,101,97,107,0,99,111,117,114,97,110,116,95,116,97,114,103,101,116,0,106,105,116,102,97,99,0,101,102,102,95,104,97,105,114,0,103,114,
105,100,95,114,97,110,100,0,112,115,95,111,102,102,115,101,116,91,49,93,0,103,114,105,100,95,114,101,115,0,101,102,102,101,99,116,111,114,95,97,109,111,117,110,116,0,116,105,109,101,95,102,108,97,103,0,116,105,109,101,95,112,97,100,91,51,93,0,112,97,114,116,102,97,99,0,116,97,110,102,97,99,0,116,97,110,112,104,97,115,101,0,114,101,97,99,116,102,97,99,0,111,98,95,118,101,108,91,51,93,0,97,118,101,102,97,99,0,112,104,
97,115,101,102,97,99,0,114,97,110,100,114,111,116,102,97,99,0,114,97,110,100,112,104,97,115,101,102,97,99,0,114,97,110,100,115,105,122,101,0,97,99,99,91,51,93,0,100,114,97,103,102,97,99,0,98,114,111,119,110,102,97,99,0,114,97,110,100,108,101,110,103,116,104,0,99,104,105,108,100,95,102,108,97,103,0,99,104,105,108,100,95,110,98,114,0,114,101,110,95,99,104,105,108,100,95,110,98,114,0,112,97,114,101,110,116,115,0,99,104,
105,108,100,115,105,122,101,0,99,104,105,108,100,114,97,110,100,115,105,122,101,0,99,104,105,108,100,114,97,100,0,99,104,105,108,100,102,108,97,116,0,99,108,117,109,112,112,111,119,0,107,105,110,107,95,102,108,97,116,0,107,105,110,107,95,97,109,112,95,99,108,117,109,112,0,107,105,110,107,95,101,120,116,114,97,95,115,116,101,112,115,0,107,105,110,107,95,97,120,105,115,95,114,97,110,100,111,109,0,107,105,110,107,95,97,109,112,95,114,97,
110,100,111,109,0,114,111,117,103,104,49,0,114,111,117,103,104,49,95,115,105,122,101,0,114,111,117,103,104,50,0,114,111,117,103,104,50,95,115,105,122,101,0,114,111,117,103,104,50,95,116,104,114,101,115,0,114,111,117,103,104,95,101,110,100,0,114,111,117,103,104,95,101,110,100,95,115,104,97,112,101,0,99,108,101,110,103,116,104,0,99,108,101,110,103,116,104,95,116,104,114,101,115,0,112,97,114,116,105,110,103,95,102,97,99,0,112,97,114,116,
105,110,103,95,109,105,110,0,112,97,114,116,105,110,103,95,109,97,120,0,98,114,97,110,99,104,95,116,104,114,101,115,0,100,114,97,119,95,108,105,110,101,91,50,93,0,112,97,116,104,95,115,116,97,114,116,0,112,97,116,104,95,101,110,100,0,116,114,97,105,108,95,99,111,117,110,116,0,107,101,121,101,100,95,108,111,111,112,115,0,42,99,108,117,109,112,99,117,114,118,101,0,42,114,111,117,103,104,99,117,114,118,101,0,99,108,117,109,112,95,
110,111,105,115,101,95,115,105,122,101,0,98,101,110,100,105,110,103,95,114,97,110,100,111,109,0,100,117,112,108,105,119,101,105,103,104,116,115,0,42,101,102,102,95,103,114,111,117,112,0,42,100,117,112,95,111,98,0,42,98,98,95,111,98,0,42,112,100,50,0,117,115,101,95,109,111,100,105,102,105,101,114,95,115,116,97,99,107,0,112,97,100,53,91,51,93,0,42,112,97,114,116,0,42,112,97,114,116,105,99,108,101,115,0,42,42,112,97,116,104,
99,97,99,104,101,0,42,42,99,104,105,108,100,99,97,99,104,101,0,112,97,116,104,99,97,99,104,101,98,117,102,115,0,99,104,105,108,100,99,97,99,104,101,98,117,102,115,0,42,99,108,109,100,0,42,104,97,105,114,95,105,110,95,100,109,0,42,104,97,105,114,95,111,117,116,95,100,109,0,42,116,97,114,103,101,116,95,111,98,0,42,108,97,116,116,105,99,101,95,100,101,102,111,114,109,95,100,97,116,97,0,116,114,101,101,95,102,114,97,109,
101,0,98,118,104,116,114,101,101,95,102,114,97,109,101,0,99,104,105,108,100,95,115,101,101,100,0,116,111,116,117,110,101,120,105,115,116,0,116,111,116,99,104,105,108,100,0,116,111,116,99,97,99,104,101,100,0,116,111,116,99,104,105,108,100,99,97,99,104,101,0,116,97,114,103,101,116,95,112,115,121,115,0,116,111,116,107,101,121,101,100,0,98,97,107,101,115,112,97,99,101,0,98,98,95,117,118,110,97,109,101,91,51,93,91,54,52,93,0,118,
103,114,111,117,112,91,49,50,93,0,118,103,95,110,101,103,0,114,116,51,0,42,114,101,110,100,101,114,100,97,116,97,0,42,101,102,102,101,99,116,111,114,115,0,42,102,108,117,105,100,95,115,112,114,105,110,103,115,0,116,111,116,95,102,108,117,105,100,115,112,114,105,110,103,115,0,97,108,108,111,99,95,102,108,117,105,100,115,112,114,105,110,103,115,0,42,116,114,101,101,0,42,112,100,100,0,100,116,95,102,114,97,99,0,108,97,116,116,105,99,
101,95,115,116,114,101,110,103,116,104,0,67,100,105,115,0,67,118,105,0,115,116,114,117,99,116,117,114,97,108,0,98,101,110,100,105,110,103,0,109,97,120,95,98,101,110,100,0,109,97,120,95,115,116,114,117,99,116,0,109,97,120,95,115,104,101,97,114,0,109,97,120,95,115,101,119,105,110,103,0,97,118,103,95,115,112,114,105,110,103,95,108,101,110,0,116,105,109,101,115,99,97,108,101,0,116,105,109,101,95,115,99,97,108,101,0,101,102,102,95,
102,111,114,99,101,95,115,99,97,108,101,0,101,102,102,95,119,105,110,100,95,115,99,97,108,101,0,115,105,109,95,116,105,109,101,95,111,108,100,0,118,101,108,111,99,105,116,121,95,115,109,111,111,116,104,0,100,101,110,115,105,116,121,95,116,97,114,103,101,116,0,100,101,110,115,105,116,121,95,115,116,114,101,110,103,116,104,0,99,111,108,108,105,100,101,114,95,102,114,105,99,116,105,111,110,0,118,101,108,95,100,97,109,112,105,110,103,0,115,104,
114,105,110,107,95,109,105,110,0,115,104,114,105,110,107,95,109,97,120,0,98,101,110,100,105,110,103,95,100,97,109,112,105,110,103,0,118,111,120,101,108,95,99,101,108,108,95,115,105,122,101,0,115,116,101,112,115,80,101,114,70,114,97,109,101,0,112,114,101,114,111,108,108,0,109,97,120,115,112,114,105,110,103,108,101,110,0,115,111,108,118,101,114,95,116,121,112,101,0,118,103,114,111,117,112,95,98,101,110,100,0,118,103,114,111,117,112,95,109,97,
115,115,0,118,103,114,111,117,112,95,115,116,114,117,99,116,0,118,103,114,111,117,112,95,115,104,114,105,110,107,0,115,104,97,112,101,107,101,121,95,114,101,115,116,0,112,114,101,115,101,116,115,0,114,101,115,101,116,0,42,99,111,108,108,105,115,105,111,110,95,108,105,115,116,0,101,112,115,105,108,111,110,0,115,101,108,102,95,102,114,105,99,116,105,111,110,0,115,101,108,102,101,112,115,105,108,111,110,0,114,101,112,101,108,95,102,111,114,99,101,
0,100,105,115,116,97,110,99,101,95,114,101,112,101,108,0,115,101,108,102,95,108,111,111,112,95,99,111,117,110,116,0,108,111,111,112,95,99,111,117,110,116,0,118,103,114,111,117,112,95,115,101,108,102,99,111,108,0,116,104,105,99,107,110,101,115,115,0,100,114,97,119,95,115,109,111,111,116,104,102,97,99,0,100,114,97,119,95,115,109,111,111,116,104,108,118,108,0,115,117,98,108,101,118,101,108,0,100,114,97,119,95,115,101,110,115,105,116,105,118,
105,116,121,0,100,114,97,119,95,115,116,114,101,110,103,116,104,0,100,114,97,119,95,106,105,116,116,101,114,0,100,114,97,119,95,97,110,103,108,101,0,100,114,97,119,95,97,110,103,108,101,95,102,97,99,116,111,114,0,100,114,97,119,95,114,97,110,100,111,109,95,112,114,101,115,115,0,100,114,97,119,95,114,97,110,100,111,109,95,115,117,98,0,42,99,117,114,95,115,101,110,115,105,116,105,118,105,116,121,0,42,99,117,114,95,115,116,114,101,110,
103,116,104,0,42,99,117,114,95,106,105,116,116,101,114,0,102,105,108,108,91,52,93,0,42,116,114,105,97,110,103,108,101,115,0,116,111,116,95,116,114,105,97,110,103,108,101,115,0,105,110,105,116,116,105,109,101,0,99,111,108,111,114,110,97,109,101,91,49,50,56,93,0,42,112,97,108,99,111,108,111,114,0,116,109,112,95,108,97,121,101,114,105,110,102,111,91,49,50,56,93,0,115,116,114,111,107,101,115,0,102,114,97,109,101,110,117,109,0,107,
101,121,95,116,121,112,101,0,42,97,99,116,102,114,97,109,101,0,103,115,116,101,112,0,103,115,116,101,112,95,110,101,120,116,0,103,99,111,108,111,114,95,112,114,101,118,91,51,93,0,103,99,111,108,111,114,95,110,101,120,116,91,51,93,0,105,110,102,111,91,49,50,56,93,0,105,110,118,101,114,115,101,91,52,93,91,52,93,0,116,105,110,116,99,111,108,111,114,91,52,93,0,111,112,97,99,105,116,121,0,115,98,117,102,102,101,114,95,115,105,
122,101,0,115,98,117,102,102,101,114,95,115,102,108,97,103,0,42,115,98,117,102,102,101,114,0,115,99,111,108,111,114,91,52,93,0,115,102,108,97,103,0,112,97,108,101,116,116,101,115,0,108,105,115,116,0,112,114,105,110,116,108,101,118,101,108,0,115,116,111,114,101,108,101,118,101,108,0,42,114,101,112,111,114,116,116,105,109,101,114,0,42,119,105,110,100,114,97,119,97,98,108,101,0,42,119,105,110,97,99,116,105,118,101,0,119,105,110,100,111,
119,115,0,105,110,105,116,105,97,108,105,122,101,100,0,102,105,108,101,95,115,97,118,101,100,0,111,112,95,117,110,100,111,95,100,101,112,116,104,0,111,112,101,114,97,116,111,114,115,0,113,117,101,117,101,0,114,101,112,111,114,116,115,0,106,111,98,115,0,112,97,105,110,116,99,117,114,115,111,114,115,0,100,114,97,103,115,0,107,101,121,99,111,110,102,105,103,115,0,42,100,101,102,97,117,108,116,99,111,110,102,0,42,97,100,100,111,110,99,111,
110,102,0,42,117,115,101,114,99,111,110,102,0,116,105,109,101,114,115,0,42,97,117,116,111,115,97,118,101,116,105,109,101,114,0,105,115,95,105,110,116,101,114,102,97,99,101,95,108,111,99,107,101,100,0,112,97,114,91,55,93,0,42,103,104,111,115,116,119,105,110,0,42,115,99,114,101,101,110,0,42,110,101,119,115,99,114,101,101,110,0,115,99,114,101,101,110,110,97,109,101,91,54,52,93,0,112,111,115,120,0,112,111,115,121,0,119,105,110,100,
111,119,115,116,97,116,101,0,109,111,110,105,116,111,114,0,108,97,115,116,99,117,114,115,111,114,0,109,111,100,97,108,99,117,114,115,111,114,0,103,114,97,98,99,117,114,115,111,114,0,97,100,100,109,111,117,115,101,109,111,118,101,0,109,117,108,116,105,115,97,109,112,108,101,115,0,108,111,99,107,95,112,105,101,95,101,118,101,110,116,0,108,97,115,116,95,112,105,101,95,101,118,101,110,116,0,42,101,118,101,110,116,115,116,97,116,101,0,42,99,
117,114,115,119,105,110,0,42,116,119,101,97,107,0,42,105,109,101,95,100,97,116,97,0,100,114,97,119,109,101,116,104,111,100,0,100,114,97,119,102,97,105,108,0,100,114,97,119,100,97,116,97,0,109,111,100,97,108,104,97,110,100,108,101,114,115,0,115,117,98,119,105,110,100,111,119,115,0,103,101,115,116,117,114,101,0,112,114,111,112,118,97,108,117,101,95,115,116,114,91,54,52,93,0,112,114,111,112,118,97,108,117,101,0,115,104,105,102,116,0,
99,116,114,108,0,97,108,116,0,111,115,107,101,121,0,107,101,121,109,111,100,105,102,105,101,114,0,109,97,112,116,121,112,101,0,42,112,116,114,0,42,114,101,109,111,118,101,95,105,116,101,109,0,42,97,100,100,95,105,116,101,109,0,105,116,101,109,115,0,100,105,102,102,95,105,116,101,109,115,0,115,112,97,99,101,105,100,0,114,101,103,105,111,110,105,100,0,107,109,105,95,105,100,0,40,42,112,111,108,108,41,40,41,0,42,109,111,100,97,108,
95,105,116,101,109,115,0,98,97,115,101,110,97,109,101,91,54,52,93,0,97,99,116,107,101,121,109,97,112,0,42,99,117,115,116,111,109,100,97,116,97,0,42,112,121,95,105,110,115,116,97,110,99,101,0,42,114,101,112,111,114,116,115,0,109,97,99,114,111,0,42,111,112,109,0,42,99,111,101,102,102,105,99,105,101,110,116,115,0,97,114,114,97,121,115,105,122,101,0,112,111,108,121,95,111,114,100,101,114,0,112,104,97,115,101,95,109,117,108,116,
105,112,108,105,101,114,0,112,104,97,115,101,95,111,102,102,115,101,116,0,118,97,108,117,101,95,111,102,102,115,101,116,0,109,105,100,118,97,108,0,98,101,102,111,114,101,95,109,111,100,101,0,97,102,116,101,114,95,109,111,100,101,0,98,101,102,111,114,101,95,99,121,99,108,101,115,0,97,102,116,101,114,95,99,121,99,108,101,115,0,114,101,99,116,0,112,104,97,115,101,0,109,111,100,105,102,105,99,97,116,105,111,110,0,115,116,101,112,95,115,
105,122,101,0,42,114,110,97,95,112,97,116,104,0,112,99,104,97,110,95,110,97,109,101,91,54,52,93,0,116,114,97,110,115,67,104,97,110,0,105,100,116,121,112,101,0,116,97,114,103,101,116,115,91,56,93,0,110,117,109,95,116,97,114,103,101,116,115,0,118,97,114,105,97,98,108,101,115,0,101,120,112,114,101,115,115,105,111,110,91,50,53,54,93,0,42,101,120,112,114,95,99,111,109,112,0,118,101,99,91,50,93,0,42,102,112,116,0,97,114,
114,97,121,95,105,110,100,101,120,0,112,114,101,118,95,110,111,114,109,95,102,97,99,116,111,114,0,112,114,101,118,95,111,102,102,115,101,116,0,102,114,111,109,91,49,50,56,93,0,116,111,91,49,50,56,93,0,109,97,112,112,105,110,103,115,0,115,116,114,105,112,115,0,42,114,101,109,97,112,0,102,99,117,114,118,101,115,0,115,116,114,105,112,95,116,105,109,101,0,98,108,101,110,100,109,111,100,101,0,101,120,116,101,110,100,109,111,100,101,0,
42,115,112,101,97,107,101,114,95,104,97,110,100,108,101,0,103,114,111,117,112,91,54,52,93,0,103,114,111,117,112,109,111,100,101,0,107,101,121,105,110,103,102,108,97,103,0,107,101,121,105,110,103,111,118,101,114,114,105,100,101,0,112,97,116,104,115,0,100,101,115,99,114,105,112,116,105,111,110,91,50,52,48,93,0,116,121,112,101,105,110,102,111,91,54,52,93,0,97,99,116,105,118,101,95,112,97,116,104,0,42,116,109,112,97,99,116,0,110,108,
97,95,116,114,97,99,107,115,0,42,97,99,116,95,116,114,97,99,107,0,42,97,99,116,115,116,114,105,112,0,100,114,105,118,101,114,115,0,111,118,101,114,114,105,100,101,115,0,97,99,116,95,98,108,101,110,100,109,111,100,101,0,97,99,116,95,101,120,116,101,110,100,109,111,100,101,0,97,99,116,95,105,110,102,108,117,101,110,99,101,0,114,117,108,101,0,111,112,116,105,111,110,115,0,102,101,97,114,95,102,97,99,116,111,114,0,115,105,103,110,
97,108,95,105,100,0,108,111,111,107,95,97,104,101,97,100,0,111,108,111,99,91,51,93,0,113,117,101,117,101,95,115,105,122,101,0,119,97,110,100,101,114,0,102,108,101,101,95,100,105,115,116,97,110,99,101,0,104,101,97,108,116,104,0,115,116,97,116,101,95,105,100,0,114,117,108,101,115,0,99,111,110,100,105,116,105,111,110,115,0,97,99,116,105,111,110,115,0,114,117,108,101,115,101,116,95,116,121,112,101,0,114,117,108,101,95,102,117,122,122,
105,110,101,115,115,0,108,97,115,116,95,115,116,97,116,101,95,105,100,0,108,97,110,100,105,110,103,95,115,109,111,111,116,104,110,101,115,115,0,98,97,110,107,105,110,103,0,97,103,103,114,101,115,115,105,111,110,0,97,105,114,95,109,105,110,95,115,112,101,101,100,0,97,105,114,95,109,97,120,95,115,112,101,101,100,0,97,105,114,95,109,97,120,95,97,99,99,0,97,105,114,95,109,97,120,95,97,118,101,0,97,105,114,95,112,101,114,115,111,110,
97,108,95,115,112,97,99,101,0,108,97,110,100,95,106,117,109,112,95,115,112,101,101,100,0,108,97,110,100,95,109,97,120,95,115,112,101,101,100,0,108,97,110,100,95,109,97,120,95,97,99,99,0,108,97,110,100,95,109,97,120,95,97,118,101,0,108,97,110,100,95,112,101,114,115,111,110,97,108,95,115,112,97,99,101,0,108,97,110,100,95,115,116,105,99,107,95,102,111,114,99,101,0,115,116,97,116,101,115,0,42,115,109,100,0,42,102,108,117,105,
100,95,109,117,116,101,120,0,42,102,108,117,105,100,95,103,114,111,117,112,0,42,99,111,108,108,95,103,114,111,117,112,0,42,119,116,0,42,116,101,120,95,119,116,0,42,116,101,120,95,115,104,97,100,111,119,0,42,116,101,120,95,102,108,97,109,101,0,42,115,104,97,100,111,119,0,112,48,91,51,93,0,112,49,91,51,93,0,100,112,48,91,51,93,0,99,101,108,108,95,115,105,122,101,91,51,93,0,103,108,111,98,97,108,95,115,105,122,101,91,
51,93,0,112,114,101,118,95,108,111,99,91,51,93,0,115,104,105,102,116,91,51,93,0,115,104,105,102,116,95,102,91,51,93,0,111,98,106,95,115,104,105,102,116,95,102,91,51,93,0,102,108,117,105,100,109,97,116,91,52,93,91,52,93,0,102,108,117,105,100,109,97,116,95,119,116,91,52,93,91,52,93,0,98,97,115,101,95,114,101,115,91,51,93,0,114,101,115,95,109,105,110,91,51,93,0,114,101,115,95,109,97,120,91,51,93,0,114,101,115,
91,51,93,0,116,111,116,97,108,95,99,101,108,108,115,0,100,120,0,97,100,97,112,116,95,109,97,114,103,105,110,0,97,100,97,112,116,95,114,101,115,0,97,100,97,112,116,95,116,104,114,101,115,104,111,108,100,0,98,101,116,97,0,97,109,112,108,105,102,121,0,109,97,120,114,101,115,0,118,105,101,119,115,101,116,116,105,110,103,115,0,110,111,105,115,101,0,100,105,115,115,95,112,101,114,99,101,110,116,0,100,105,115,115,95,115,112,101,101,100,
0,114,101,115,95,119,116,91,51,93,0,100,120,95,119,116,0,99,97,99,104,101,95,99,111,109,112,0,99,97,99,104,101,95,104,105,103,104,95,99,111,109,112,0,111,112,101,110,118,100,98,95,99,111,109,112,0,99,97,99,104,101,95,102,105,108,101,95,102,111,114,109,97,116,0,100,97,116,97,95,100,101,112,116,104,0,42,112,111,105,110,116,95,99,97,99,104,101,91,50,93,0,112,116,99,97,99,104,101,115,91,50,93,0,98,111,114,100,101,114,
95,99,111,108,108,105,115,105,111,110,115,0,118,111,114,116,105,99,105,116,121,0,97,99,116,105,118,101,95,102,105,101,108,100,115,0,97,99,116,105,118,101,95,99,111,108,111,114,91,51,93,0,104,105,103,104,114,101,115,95,115,97,109,112,108,105,110,103,0,98,117,114,110,105,110,103,95,114,97,116,101,0,102,108,97,109,101,95,115,109,111,107,101,0,102,108,97,109,101,95,118,111,114,116,105,99,105,116,121,0,102,108,97,109,101,95,105,103,110,105,
116,105,111,110,0,102,108,97,109,101,95,109,97,120,95,116,101,109,112,0,102,108,97,109,101,95,115,109,111,107,101,95,99,111,108,111,114,91,51,93,0,115,108,105,99,101,95,109,101,116,104,111,100,0,97,120,105,115,95,115,108,105,99,101,95,109,101,116,104,111,100,0,115,108,105,99,101,95,97,120,105,115,0,100,114,97,119,95,118,101,108,111,99,105,116,121,0,115,108,105,99,101,95,112,101,114,95,118,111,120,101,108,0,115,108,105,99,101,95,100,
101,112,116,104,0,100,105,115,112,108,97,121,95,116,104,105,99,107,110,101,115,115,0,118,101,99,116,111,114,95,115,99,97,108,101,0,118,101,99,116,111,114,95,100,114,97,119,95,116,121,112,101,0,117,115,101,95,99,111,98,97,0,99,111,98,97,95,102,105,101,108,100,0,42,110,111,105,115,101,95,116,101,120,116,117,114,101,0,42,118,101,114,116,115,95,111,108,100,0,118,101,108,95,109,117,108,116,105,0,118,101,108,95,110,111,114,109,97,108,0,
118,101,108,95,114,97,110,100,111,109,0,102,117,101,108,95,97,109,111,117,110,116,0,118,111,108,117,109,101,95,100,101,110,115,105,116,121,0,115,117,114,102,97,99,101,95,100,105,115,116,97,110,99,101,0,112,97,114,116,105,99,108,101,95,115,105,122,101,0,116,101,120,116,117,114,101,95,115,105,122,101,0,116,101,120,116,117,114,101,95,111,102,102,115,101,116,0,118,103,114,111,117,112,95,100,101,110,115,105,116,121,0,116,101,120,116,117,114,101,95,
116,121,112,101,0,118,111,108,117,109,101,95,109,97,120,0,118,111,108,117,109,101,95,109,105,110,0,100,105,115,116,97,110,99,101,95,114,101,102,101,114,101,110,99,101,0,99,111,110,101,95,97,110,103,108,101,95,111,117,116,101,114,0,99,111,110,101,95,97,110,103,108,101,95,105,110,110,101,114,0,99,111,110,101,95,118,111,108,117,109,101,95,111,117,116,101,114,0,114,101,110,100,101,114,95,102,108,97,103,0,98,117,105,108,100,95,115,105,122,101,
95,102,108,97,103,0,98,117,105,108,100,95,116,99,95,102,108,97,103,0,108,97,115,116,115,105,122,101,91,50,93,0,116,114,97,99,107,105,110,103,0,42,116,114,97,99,107,105,110,103,95,99,111,110,116,101,120,116,0,112,114,111,120,121,0,102,114,97,109,101,95,111,102,102,115,101,116,0,117,115,101,95,116,114,97,99,107,95,109,97,115,107,0,116,114,97,99,107,95,112,114,101,118,105,101,119,95,104,101,105,103,104,116,0,102,114,97,109,101,95,
119,105,100,116,104,0,102,114,97,109,101,95,104,101,105,103,104,116,0,117,110,100,105,115,116,95,109,97,114,107,101,114,0,42,116,114,97,99,107,95,115,101,97,114,99,104,0,42,116,114,97,99,107,95,112,114,101,118,105,101,119,0,116,114,97,99,107,95,112,111,115,91,50,93,0,116,114,97,99,107,95,100,105,115,97,98,108,101,100,0,116,114,97,99,107,95,108,111,99,107,101,100,0,42,109,97,114,107,101,114,0,115,108,105,100,101,95,115,99,97,
108,101,91,50,93,0,101,114,114,111,114,0,42,105,110,116,114,105,110,115,105,99,115,0,100,105,115,116,111,114,116,105,111,110,95,109,111,100,101,108,0,115,101,110,115,111,114,95,119,105,100,116,104,0,112,105,120,101,108,95,97,115,112,101,99,116,0,102,111,99,97,108,0,117,110,105,116,115,0,112,114,105,110,99,105,112,97,108,91,50,93,0,107,49,0,107,50,0,107,51,0,100,105,118,105,115,105,111,110,95,107,49,0,100,105,118,105,115,105,111,
110,95,107,50,0,112,111,115,91,50,93,0,112,97,116,116,101,114,110,95,99,111,114,110,101,114,115,91,52,93,91,50,93,0,115,101,97,114,99,104,95,109,105,110,91,50,93,0,115,101,97,114,99,104,95,109,97,120,91,50,93,0,112,97,116,95,109,105,110,91,50,93,0,112,97,116,95,109,97,120,91,50,93,0,109,97,114,107,101,114,115,110,114,0,108,97,115,116,95,109,97,114,107,101,114,0,42,109,97,114,107,101,114,115,0,98,117,110,100,108,
101,95,112,111,115,91,51,93,0,112,97,116,95,102,108,97,103,0,115,101,97,114,99,104,95,102,108,97,103,0,102,114,97,109,101,115,95,108,105,109,105,116,0,112,97,116,116,101,114,110,95,109,97,116,99,104,0,109,111,116,105,111,110,95,109,111,100,101,108,0,97,108,103,111,114,105,116,104,109,95,102,108,97,103,0,109,105,110,105,109,117,109,95,99,111,114,114,101,108,97,116,105,111,110,0,119,101,105,103,104,116,95,115,116,97,98,0,99,111,114,
110,101,114,115,91,52,93,91,50,93,0,42,42,112,111,105,110,116,95,116,114,97,99,107,115,0,112,111,105,110,116,95,116,114,97,99,107,115,110,114,0,105,109,97,103,101,95,111,112,97,99,105,116,121,0,100,101,102,97,117,108,116,95,109,111,116,105,111,110,95,109,111,100,101,108,0,100,101,102,97,117,108,116,95,97,108,103,111,114,105,116,104,109,95,102,108,97,103,0,100,101,102,97,117,108,116,95,109,105,110,105,109,117,109,95,99,111,114,114,101,
108,97,116,105,111,110,0,100,101,102,97,117,108,116,95,112,97,116,116,101,114,110,95,115,105,122,101,0,100,101,102,97,117,108,116,95,115,101,97,114,99,104,95,115,105,122,101,0,100,101,102,97,117,108,116,95,102,114,97,109,101,115,95,108,105,109,105,116,0,100,101,102,97,117,108,116,95,109,97,114,103,105,110,0,100,101,102,97,117,108,116,95,112,97,116,116,101,114,110,95,109,97,116,99,104,0,100,101,102,97,117,108,116,95,102,108,97,103,0,109,
111,116,105,111,110,95,102,108,97,103,0,107,101,121,102,114,97,109,101,49,0,107,101,121,102,114,97,109,101,50,0,114,101,99,111,110,115,116,114,117,99,116,105,111,110,95,102,108,97,103,0,114,101,102,105,110,101,95,99,97,109,101,114,97,95,105,110,116,114,105,110,115,105,99,115,0,99,108,101,97,110,95,102,114,97,109,101,115,0,99,108,101,97,110,95,97,99,116,105,111,110,0,99,108,101,97,110,95,101,114,114,111,114,0,111,98,106,101,99,116,
95,100,105,115,116,97,110,99,101,0,116,111,116,95,116,114,97,99,107,0,97,99,116,95,116,114,97,99,107,0,116,111,116,95,114,111,116,95,116,114,97,99,107,0,97,99,116,95,114,111,116,95,116,114,97,99,107,0,109,97,120,115,99,97,108,101,0,42,114,111,116,95,116,114,97,99,107,0,97,110,99,104,111,114,95,102,114,97,109,101,0,116,97,114,103,101,116,95,112,111,115,91,50,93,0,116,97,114,103,101,116,95,114,111,116,0,108,111,99,105,
110,102,0,115,99,97,108,101,105,110,102,0,114,111,116,105,110,102,0,108,97,115,116,95,99,97,109,101,114,97,0,99,97,109,110,114,0,42,99,97,109,101,114,97,115,0,116,114,97,99,107,115,0,112,108,97,110,101,95,116,114,97,99,107,115,0,114,101,99,111,110,115,116,114,117,99,116,105,111,110,0,109,101,115,115,97,103,101,91,50,53,54,93,0,116,111,116,95,115,101,103,109,101,110,116,0,42,115,101,103,109,101,110,116,115,0,109,97,120,95,
115,101,103,109,101,110,116,0,116,111,116,97,108,95,102,114,97,109,101,115,0,99,111,118,101,114,97,103,101,0,115,111,114,116,95,109,101,116,104,111,100,0,99,111,118,101,114,97,103,101,95,115,101,103,109,101,110,116,115,0,116,111,116,95,99,104,97,110,110,101,108,0,115,101,116,116,105,110,103,115,0,99,97,109,101,114,97,0,115,116,97,98,105,108,105,122,97,116,105,111,110,0,42,97,99,116,95,112,108,97,110,101,95,116,114,97,99,107,0,111,
98,106,101,99,116,115,0,111,98,106,101,99,116,110,114,0,116,111,116,95,111,98,106,101,99,116,0,100,111,112,101,115,104,101,101,116,0,42,98,114,117,115,104,95,103,114,111,117,112,0,99,117,114,114,101,110,116,95,102,114,97,109,101,0,100,105,115,112,95,116,121,112,101,0,105,109,97,103,101,95,102,105,108,101,102,111,114,109,97,116,0,101,102,102,101,99,116,95,117,105,0,112,114,101,118,105,101,119,95,105,100,0,105,110,105,116,95,99,111,108,
111,114,95,116,121,112,101,0,112,97,100,95,115,0,105,109,97,103,101,95,114,101,115,111,108,117,116,105,111,110,0,115,117,98,115,116,101,112,115,0,105,110,105,116,95,99,111,108,111,114,91,52,93,0,42,105,110,105,116,95,116,101,120,116,117,114,101,0,105,110,105,116,95,108,97,121,101,114,110,97,109,101,91,54,52,93,0,100,114,121,95,115,112,101,101,100,0,99,111,108,111,114,95,100,114,121,95,116,104,114,101,115,104,111,108,100,0,100,101,112,
116,104,95,99,108,97,109,112,0,100,105,115,112,95,102,97,99,116,111,114,0,115,112,114,101,97,100,95,115,112,101,101,100,0,99,111,108,111,114,95,115,112,114,101,97,100,95,115,112,101,101,100,0,115,104,114,105,110,107,95,115,112,101,101,100,0,100,114,105,112,95,118,101,108,0,100,114,105,112,95,97,99,99,0,105,110,102,108,117,101,110,99,101,95,115,99,97,108,101,0,114,97,100,105,117,115,95,115,99,97,108,101,0,119,97,118,101,95,100,97,
109,112,105,110,103,0,119,97,118,101,95,115,112,101,101,100,0,119,97,118,101,95,116,105,109,101,115,99,97,108,101,0,119,97,118,101,95,115,112,114,105,110,103,0,119,97,118,101,95,115,109,111,111,116,104,110,101,115,115,0,105,109,97,103,101,95,111,117,116,112,117,116,95,112,97,116,104,91,49,48,50,52,93,0,111,117,116,112,117,116,95,110,97,109,101,91,54,52,93,0,111,117,116,112,117,116,95,110,97,109,101,50,91,54,52,93,0,42,112,109,
100,0,115,117,114,102,97,99,101,115,0,97,99,116,105,118,101,95,115,117,114,0,101,114,114,111,114,91,54,52,93,0,99,111,108,108,105,115,105,111,110,0,119,101,116,110,101,115,115,0,112,97,114,116,105,99,108,101,95,114,97,100,105,117,115,0,112,97,114,116,105,99,108,101,95,115,109,111,111,116,104,0,112,97,105,110,116,95,100,105,115,116,97,110,99,101,0,42,112,97,105,110,116,95,114,97,109,112,0,42,118,101,108,95,114,97,109,112,0,112,
114,111,120,105,109,105,116,121,95,102,97,108,108,111,102,102,0,114,97,121,95,100,105,114,0,119,97,118,101,95,102,97,99,116,111,114,0,119,97,118,101,95,99,108,97,109,112,0,109,97,120,95,118,101,108,111,99,105,116,121,0,115,109,117,100,103,101,95,115,116,114,101,110,103,116,104,0,109,97,115,107,108,97,121,101,114,115,0,109,97,115,107,108,97,121,95,97,99,116,0,109,97,115,107,108,97,121,95,116,111,116,0,105,100,95,116,121,112,101,0,
112,97,114,101,110,116,91,54,52,93,0,115,117,98,95,112,97,114,101,110,116,91,54,52,93,0,112,97,114,101,110,116,95,111,114,105,103,91,50,93,0,112,97,114,101,110,116,95,99,111,114,110,101,114,115,95,111,114,105,103,91,52,93,91,50,93,0,117,0,98,101,122,116,0,116,111,116,95,117,119,0,42,117,119,0,111,102,102,115,101,116,95,109,111,100,101,0,119,101,105,103,104,116,95,105,110,116,101,114,112,0,116,111,116,95,112,111,105,110,116,
0,42,112,111,105,110,116,115,95,100,101,102,111,114,109,0,116,111,116,95,118,101,114,116,0,115,112,108,105,110,101,115,0,115,112,108,105,110,101,115,95,115,104,97,112,101,115,0,42,97,99,116,95,115,112,108,105,110,101,0,42,97,99,116,95,112,111,105,110,116,0,98,108,101,110,100,95,102,108,97,103,0,42,42,111,98,106,101,99,116,115,0,42,99,111,110,115,116,114,97,105,110,116,115,0,108,116,105,109,101,0,110,117,109,98,111,100,105,101,115,
0,115,116,101,112,115,95,112,101,114,95,115,101,99,111,110,100,0,110,117,109,95,115,111,108,118,101,114,95,105,116,101,114,97,116,105,111,110,115,0,42,112,104,121,115,105,99,115,95,119,111,114,108,100,0,42,112,104,121,115,105,99,115,95,111,98,106,101,99,116,0,42,112,104,121,115,105,99,115,95,115,104,97,112,101,0,99,111,108,95,103,114,111,117,112,115,0,109,101,115,104,95,115,111,117,114,99,101,0,114,101,115,116,105,116,117,116,105,111,110,
0,108,105,110,95,100,97,109,112,105,110,103,0,97,110,103,95,100,97,109,112,105,110,103,0,108,105,110,95,115,108,101,101,112,95,116,104,114,101,115,104,0,97,110,103,95,115,108,101,101,112,95,116,104,114,101,115,104,0,111,114,110,91,52,93,0,112,111,115,91,51,93,0,42,111,98,49,0,42,111,98,50,0,98,114,101,97,107,105,110,103,95,116,104,114,101,115,104,111,108,100,0,108,105,109,105,116,95,108,105,110,95,120,95,108,111,119,101,114,0,
108,105,109,105,116,95,108,105,110,95,120,95,117,112,112,101,114,0,108,105,109,105,116,95,108,105,110,95,121,95,108,111,119,101,114,0,108,105,109,105,116,95,108,105,110,95,121,95,117,112,112,101,114,0,108,105,109,105,116,95,108,105,110,95,122,95,108,111,119,101,114,0,108,105,109,105,116,95,108,105,110,95,122,95,117,112,112,101,114,0,108,105,109,105,116,95,97,110,103,95,120,95,108,111,119,101,114,0,108,105,109,105,116,95,97,110,103,95,120,95,
117,112,112,101,114,0,108,105,109,105,116,95,97,110,103,95,121,95,108,111,119,101,114,0,108,105,109,105,116,95,97,110,103,95,121,95,117,112,112,101,114,0,108,105,109,105,116,95,97,110,103,95,122,95,108,111,119,101,114,0,108,105,109,105,116,95,97,110,103,95,122,95,117,112,112,101,114,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,120,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,121,0,115,112,114,105,
110,103,95,115,116,105,102,102,110,101,115,115,95,122,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,97,110,103,95,120,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,97,110,103,95,121,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,97,110,103,95,122,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,120,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,121,0,115,112,
114,105,110,103,95,100,97,109,112,105,110,103,95,122,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,97,110,103,95,120,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,97,110,103,95,121,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,97,110,103,95,122,0,109,111,116,111,114,95,108,105,110,95,116,97,114,103,101,116,95,118,101,108,111,99,105,116,121,0,109,111,116,111,114,95,97,110,103,95,116,97,114,103,101,116,
95,118,101,108,111,99,105,116,121,0,109,111,116,111,114,95,108,105,110,95,109,97,120,95,105,109,112,117,108,115,101,0,109,111,116,111,114,95,97,110,103,95,109,97,120,95,105,109,112,117,108,115,101,0,42,112,104,121,115,105,99,115,95,99,111,110,115,116,114,97,105,110,116,0,115,101,108,101,99,116,105,111,110,0,113,105,0,113,105,95,115,116,97,114,116,0,113,105,95,101,110,100,0,101,100,103,101,95,116,121,112,101,115,0,101,120,99,108,117,100,
101,95,101,100,103,101,95,116,121,112,101,115,0,42,108,105,110,101,115,116,121,108,101,0,105,115,95,100,105,115,112,108,97,121,101,100,0,109,111,100,117,108,101,115,0,114,97,121,99,97,115,116,105,110,103,95,97,108,103,111,114,105,116,104,109,0,115,112,104,101,114,101,95,114,97,100,105,117,115,0,100,107,114,95,101,112,115,105,108,111,110,0,99,114,101,97,115,101,95,97,110,103,108,101,0,108,105,110,101,115,101,116,115,0,42,99,111,108,111,114,
95,114,97,109,112,0,118,97,108,117,101,95,109,105,110,0,118,97,108,117,101,95,109,97,120,0,114,97,110,103,101,95,109,105,110,0,114,97,110,103,101,95,109,97,120,0,109,105,110,95,99,117,114,118,97,116,117,114,101,0,109,97,120,95,99,117,114,118,97,116,117,114,101,0,109,105,110,95,116,104,105,99,107,110,101,115,115,0,109,97,120,95,116,104,105,99,107,110,101,115,115,0,109,105,110,95,97,110,103,108,101,0,109,97,120,95,97,110,103,108,
101,0,109,97,116,95,97,116,116,114,0,115,97,109,112,108,105,110,103,0,119,97,118,101,108,101,110,103,116,104,0,111,99,116,97,118,101,115,0,102,114,101,113,117,101,110,99,121,0,98,97,99,107,98,111,110,101,95,108,101,110,103,116,104,0,116,105,112,95,108,101,110,103,116,104,0,114,111,117,110,100,115,0,114,97,110,100,111,109,95,114,97,100,105,117,115,0,114,97,110,100,111,109,95,99,101,110,116,101,114,0,114,97,110,100,111,109,95,98,97,
99,107,98,111,110,101,0,115,99,97,108,101,95,120,0,115,99,97,108,101,95,121,0,112,105,118,111,116,95,117,0,112,105,118,111,116,95,120,0,112,105,118,111,116,95,121,0,111,114,105,101,110,116,97,116,105,111,110,0,116,104,105,99,107,110,101,115,115,95,112,111,115,105,116,105,111,110,0,116,104,105,99,107,110,101,115,115,95,114,97,116,105,111,0,99,97,112,115,0,99,104,97,105,110,105,110,103,0,115,112,108,105,116,95,108,101,110,103,116,104,
0,109,105,110,95,108,101,110,103,116,104,0,109,97,120,95,108,101,110,103,116,104,0,99,104,97,105,110,95,99,111,117,110,116,0,115,112,108,105,116,95,100,97,115,104,49,0,115,112,108,105,116,95,103,97,112,49,0,115,112,108,105,116,95,100,97,115,104,50,0,115,112,108,105,116,95,103,97,112,50,0,115,112,108,105,116,95,100,97,115,104,51,0,115,112,108,105,116,95,103,97,112,51,0,115,111,114,116,95,107,101,121,0,105,110,116,101,103,114,97,
116,105,111,110,95,116,121,112,101,0,116,101,120,115,116,101,112,0,100,97,115,104,49,0,103,97,112,49,0,100,97,115,104,50,0,103,97,112,50,0,100,97,115,104,51,0,103,97,112,51,0,99,111,108,111,114,95,109,111,100,105,102,105,101,114,115,0,97,108,112,104,97,95,109,111,100,105,102,105,101,114,115,0,116,104,105,99,107,110,101,115,115,95,109,111,100,105,102,105,101,114,115,0,103,101,111,109,101,116,114,121,95,109,111,100,105,102,105,101,114,
115,0,112,97,116,104,91,52,48,57,54,93,0,42,104,97,110,100,108,101,95,109,117,116,101,120,0,111,98,106,101,99,116,95,112,97,116,104,115,0,105,115,95,115,101,113,117,101,110,99,101,0,111,118,101,114,114,105,100,101,95,102,114,97,109,101,0,0,84,89,80,69,194,2,0,0,99,104,97,114,0,117,99,104,97,114,0,115,104,111,114,116,0,117,115,104,111,114,116,0,105,110,116,0,108,111,110,103,0,117,108,111,110,103,0,102,108,111,97,116,
0,100,111,117,98,108,101,0,105,110,116,54,52,95,116,0,117,105,110,116,54,52,95,116,0,118,111,105,100,0,76,105,110,107,0,76,105,110,107,68,97,116,97,0,76,105,115,116,66,97,115,101,0,118,101,99,50,115,0,118,101,99,50,102,0,118,101,99,51,102,0,114,99,116,105,0,114,99,116,102,0,73,68,80,114,111,112,101,114,116,121,68,97,116,97,0,73,68,80,114,111,112,101,114,116,121,0,73,68,0,76,105,98,114,97,114,121,0,70,105,
108,101,68,97,116,97,0,80,97,99,107,101,100,70,105,108,101,0,80,114,101,118,105,101,119,73,109,97,103,101,0,71,80,85,84,101,120,116,117,114,101,0,73,112,111,68,114,105,118,101,114,0,79,98,106,101,99,116,0,73,112,111,67,117,114,118,101,0,66,80,111,105,110,116,0,66,101,122,84,114,105,112,108,101,0,73,112,111,0,75,101,121,66,108,111,99,107,0,75,101,121,0,65,110,105,109,68,97,116,97,0,84,101,120,116,76,105,110,101,0,
84,101,120,116,0,71,80,85,68,79,70,83,101,116,116,105,110,103,115,0,71,80,85,83,83,65,79,83,101,116,116,105,110,103,115,0,71,80,85,70,88,83,101,116,116,105,110,103,115,0,67,97,109,101,114,97,83,116,101,114,101,111,83,101,116,116,105,110,103,115,0,67,97,109,101,114,97,0,73,109,97,103,101,85,115,101,114,0,83,99,101,110,101,0,73,109,97,103,101,65,110,105,109,0,97,110,105,109,0,73,109,97,103,101,86,105,101,119,0,73,
109,97,103,101,80,97,99,107,101,100,70,105,108,101,0,82,101,110,100,101,114,83,108,111,116,0,73,109,97,103,101,0,77,111,118,105,101,67,97,99,104,101,0,82,101,110,100,101,114,82,101,115,117,108,116,0,67,111,108,111,114,77,97,110,97,103,101,100,67,111,108,111,114,115,112,97,99,101,83,101,116,116,105,110,103,115,0,83,116,101,114,101,111,51,100,70,111,114,109,97,116,0,77,84,101,120,0,84,101,120,0,67,66,68,97,116,97,0,67,111,
108,111,114,66,97,110,100,0,69,110,118,77,97,112,0,73,109,66,117,102,0,80,111,105,110,116,68,101,110,115,105,116,121,0,67,117,114,118,101,77,97,112,112,105,110,103,0,86,111,120,101,108,68,97,116,97,0,79,99,101,97,110,84,101,120,0,98,78,111,100,101,84,114,101,101,0,84,101,120,77,97,112,112,105,110,103,0,67,111,108,111,114,77,97,112,112,105,110,103,0,76,97,109,112,0,86,111,108,117,109,101,83,101,116,116,105,110,103,115,0,
71,97,109,101,83,101,116,116,105,110,103,115,0,84,101,120,80,97,105,110,116,83,108,111,116,0,77,97,116,101,114,105,97,108,0,71,114,111,117,112,0,86,70,111,110,116,0,86,70,111,110,116,68,97,116,97,0,77,101,116,97,69,108,101,109,0,66,111,117,110,100,66,111,120,0,77,101,116,97,66,97,108,108,0,78,117,114,98,0,67,104,97,114,73,110,102,111,0,84,101,120,116,66,111,120,0,69,100,105,116,78,117,114,98,0,71,72,97,115,104,
0,67,117,114,118,101,0,69,100,105,116,70,111,110,116,0,77,101,115,104,0,77,83,101,108,101,99,116,0,77,80,111,108,121,0,77,84,101,120,80,111,108,121,0,77,76,111,111,112,0,77,76,111,111,112,85,86,0,77,76,111,111,112,67,111,108,0,77,70,97,99,101,0,77,84,70,97,99,101,0,84,70,97,99,101,0,77,86,101,114,116,0,77,69,100,103,101,0,77,68,101,102,111,114,109,86,101,114,116,0,77,67,111,108,0,66,77,69,100,105,
116,77,101,115,104,0,67,117,115,116,111,109,68,97,116,97,0,77,117,108,116,105,114,101,115,0,77,68,101,102,111,114,109,87,101,105,103,104,116,0,77,70,108,111,97,116,80,114,111,112,101,114,116,121,0,77,73,110,116,80,114,111,112,101,114,116,121,0,77,83,116,114,105,110,103,80,114,111,112,101,114,116,121,0,79,114,105,103,83,112,97,99,101,70,97,99,101,0,79,114,105,103,83,112,97,99,101,76,111,111,112,0,77,68,105,115,112,115,0,77,
117,108,116,105,114,101,115,67,111,108,0,77,117,108,116,105,114,101,115,67,111,108,70,97,99,101,0,77,117,108,116,105,114,101,115,70,97,99,101,0,77,117,108,116,105,114,101,115,69,100,103,101,0,77,117,108,116,105,114,101,115,76,101,118,101,108,0,77,82,101,99,97,115,116,0,71,114,105,100,80,97,105,110,116,77,97,115,107,0,77,86,101,114,116,83,107,105,110,0,70,114,101,101,115,116,121,108,101,69,100,103,101,0,70,114,101,101,115,116,121,
108,101,70,97,99,101,0,77,111,100,105,102,105,101,114,68,97,116,97,0,77,97,112,112,105,110,103,73,110,102,111,77,111,100,105,102,105,101,114,68,97,116,97,0,83,117,98,115,117,114,102,77,111,100,105,102,105,101,114,68,97,116,97,0,76,97,116,116,105,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,67,117,114,118,101,77,111,100,105,102,105,101,114,68,97,116,97,0,66,117,105,108,100,77,111,100,105,102,105,101,114,68,97,116,97,0,
77,97,115,107,77,111,100,105,102,105,101,114,68,97,116,97,0,65,114,114,97,121,77,111,100,105,102,105,101,114,68,97,116,97,0,77,105,114,114,111,114,77,111,100,105,102,105,101,114,68,97,116,97,0,69,100,103,101,83,112,108,105,116,77,111,100,105,102,105,101,114,68,97,116,97,0,66,101,118,101,108,77,111,100,105,102,105,101,114,68,97,116,97,0,83,109,111,107,101,77,111,100,105,102,105,101,114,68,97,116,97,0,83,109,111,107,101,68,111,109,
97,105,110,83,101,116,116,105,110,103,115,0,83,109,111,107,101,70,108,111,119,83,101,116,116,105,110,103,115,0,83,109,111,107,101,67,111,108,108,83,101,116,116,105,110,103,115,0,68,105,115,112,108,97,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,85,86,80,114,111,106,101,99,116,77,111,100,105,102,105,101,114,68,97,116,97,0,68,101,99,105,109,97,116,101,77,111,100,105,102,105,101,114,68,97,116,97,0,83,109,111,111,116,104,77,111,
100,105,102,105,101,114,68,97,116,97,0,67,97,115,116,77,111,100,105,102,105,101,114,68,97,116,97,0,87,97,118,101,77,111,100,105,102,105,101,114,68,97,116,97,0,65,114,109,97,116,117,114,101,77,111,100,105,102,105,101,114,68,97,116,97,0,72,111,111,107,77,111,100,105,102,105,101,114,68,97,116,97,0,83,111,102,116,98,111,100,121,77,111,100,105,102,105,101,114,68,97,116,97,0,67,108,111,116,104,77,111,100,105,102,105,101,114,68,97,116,
97,0,67,108,111,116,104,0,67,108,111,116,104,83,105,109,83,101,116,116,105,110,103,115,0,67,108,111,116,104,67,111,108,108,83,101,116,116,105,110,103,115,0,80,111,105,110,116,67,97,99,104,101,0,67,108,111,116,104,72,97,105,114,68,97,116,97,0,67,108,111,116,104,83,111,108,118,101,114,82,101,115,117,108,116,0,67,111,108,108,105,115,105,111,110,77,111,100,105,102,105,101,114,68,97,116,97,0,77,86,101,114,116,84,114,105,0,66,86,72,
84,114,101,101,0,83,117,114,102,97,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,68,101,114,105,118,101,100,77,101,115,104,0,66,86,72,84,114,101,101,70,114,111,109,77,101,115,104,0,66,111,111,108,101,97,110,77,111,100,105,102,105,101,114,68,97,116,97,0,77,68,101,102,73,110,102,108,117,101,110,99,101,0,77,68,101,102,67,101,108,108,0,77,101,115,104,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,80,
97,114,116,105,99,108,101,83,121,115,116,101,109,77,111,100,105,102,105,101,114,68,97,116,97,0,80,97,114,116,105,99,108,101,83,121,115,116,101,109,0,80,97,114,116,105,99,108,101,73,110,115,116,97,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,69,120,112,108,111,100,101,77,111,100,105,102,105,101,114,68,97,116,97,0,77,117,108,116,105,114,101,115,77,111,100,105,102,105,101,114,68,97,116,97,0,70,108,117,105,100,115,105,109,77,
111,100,105,102,105,101,114,68,97,116,97,0,70,108,117,105,100,115,105,109,83,101,116,116,105,110,103,115,0,83,104,114,105,110,107,119,114,97,112,77,111,100,105,102,105,101,114,68,97,116,97,0,83,105,109,112,108,101,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,83,104,97,112,101,75,101,121,77,111,100,105,102,105,101,114,68,97,116,97,0,83,111,108,105,100,105,102,121,77,111,100,105,102,105,101,114,68,97,116,97,0,83,
99,114,101,119,77,111,100,105,102,105,101,114,68,97,116,97,0,79,99,101,97,110,77,111,100,105,102,105,101,114,68,97,116,97,0,79,99,101,97,110,0,79,99,101,97,110,67,97,99,104,101,0,87,97,114,112,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,86,71,69,100,105,116,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,86,71,77,105,120,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,
105,103,104,116,86,71,80,114,111,120,105,109,105,116,121,77,111,100,105,102,105,101,114,68,97,116,97,0,68,121,110,97,109,105,99,80,97,105,110,116,77,111,100,105,102,105,101,114,68,97,116,97,0,68,121,110,97,109,105,99,80,97,105,110,116,67,97,110,118,97,115,83,101,116,116,105,110,103,115,0,68,121,110,97,109,105,99,80,97,105,110,116,66,114,117,115,104,83,101,116,116,105,110,103,115,0,82,101,109,101,115,104,77,111,100,105,102,105,101,114,
68,97,116,97,0,83,107,105,110,77,111,100,105,102,105,101,114,68,97,116,97,0,84,114,105,97,110,103,117,108,97,116,101,77,111,100,105,102,105,101,114,68,97,116,97,0,76,97,112,108,97,99,105,97,110,83,109,111,111,116,104,77,111,100,105,102,105,101,114,68,97,116,97,0,67,111,114,114,101,99,116,105,118,101,83,109,111,111,116,104,77,111,100,105,102,105,101,114,68,97,116,97,0,85,86,87,97,114,112,77,111,100,105,102,105,101,114,68,97,116,
97,0,77,101,115,104,67,97,99,104,101,77,111,100,105,102,105,101,114,68,97,116,97,0,76,97,112,108,97,99,105,97,110,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,87,105,114,101,102,114,97,109,101,77,111,100,105,102,105,101,114,68,97,116,97,0,68,97,116,97,84,114,97,110,115,102,101,114,77,111,100,105,102,105,101,114,68,97,116,97,0,78,111,114,109,97,108,69,100,105,116,77,111,100,105,102,105,101,114,68,97,116,
97,0,77,101,115,104,83,101,113,67,97,99,104,101,77,111,100,105,102,105,101,114,68,97,116,97,0,67,97,99,104,101,70,105,108,101,0,67,97,99,104,101,82,101,97,100,101,114,0,83,68,101,102,66,105,110,100,0,83,68,101,102,86,101,114,116,0,83,117,114,102,97,99,101,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,69,100,105,116,76,97,116,116,0,76,97,116,116,105,99,101,0,98,68,101,102,111,114,109,71,114,111,
117,112,0,76,111,100,76,101,118,101,108,0,83,99,117,108,112,116,83,101,115,115,105,111,110,0,98,65,99,116,105,111,110,0,98,80,111,115,101,0,98,71,80,100,97,116,97,0,98,65,110,105,109,86,105,122,83,101,116,116,105,110,103,115,0,98,77,111,116,105,111,110,80,97,116,104,0,66,117,108,108,101,116,83,111,102,116,66,111,100,121,0,80,97,114,116,68,101,102,108,101,99,116,0,83,111,102,116,66,111,100,121,0,67,117,114,118,101,67,97,
99,104,101,0,82,105,103,105,100,66,111,100,121,79,98,0,82,105,103,105,100,66,111,100,121,67,111,110,0,79,98,72,111,111,107,0,68,117,112,108,105,79,98,106,101,99,116,0,82,78,71,0,69,102,102,101,99,116,111,114,87,101,105,103,104,116,115,0,80,84,67,97,99,104,101,69,120,116,114,97,0,80,84,67,97,99,104,101,77,101,109,0,80,84,67,97,99,104,101,69,100,105,116,0,83,66,86,101,114,116,101,120,0,66,111,100,121,80,111,105,
110,116,0,66,111,100,121,83,112,114,105,110,103,0,83,66,83,99,114,97,116,99,104,0,70,108,117,105,100,86,101,114,116,101,120,86,101,108,111,99,105,116,121,0,87,111,114,108,100,0,66,97,115,101,0,65,118,105,67,111,100,101,99,68,97,116,97,0,81,117,105,99,107,116,105,109,101,67,111,100,101,99,68,97,116,97,0,81,117,105,99,107,116,105,109,101,67,111,100,101,99,83,101,116,116,105,110,103,115,0,70,70,77,112,101,103,67,111,100,101,
99,68,97,116,97,0,65,117,100,105,111,68,97,116,97,0,83,99,101,110,101,82,101,110,100,101,114,76,97,121,101,114,0,70,114,101,101,115,116,121,108,101,67,111,110,102,105,103,0,83,99,101,110,101,82,101,110,100,101,114,86,105,101,119,0,73,109,97,103,101,70,111,114,109,97,116,68,97,116,97,0,67,111,108,111,114,77,97,110,97,103,101,100,86,105,101,119,83,101,116,116,105,110,103,115,0,67,111,108,111,114,77,97,110,97,103,101,100,68,105,
115,112,108,97,121,83,101,116,116,105,110,103,115,0,66,97,107,101,68,97,116,97,0,82,101,110,100,101,114,68,97,116,97,0,82,101,110,100,101,114,80,114,111,102,105,108,101,0,71,97,109,101,68,111,109,101,0,71,97,109,101,70,114,97,109,105,110,103,0,82,101,99,97,115,116,68,97,116,97,0,71,97,109,101,68,97,116,97,0,84,105,109,101,77,97,114,107,101,114,0,80,97,105,110,116,0,66,114,117,115,104,0,80,97,108,101,116,116,101,0,
73,109,97,103,101,80,97,105,110,116,83,101,116,116,105,110,103,115,0,80,97,114,116,105,99,108,101,66,114,117,115,104,68,97,116,97,0,80,97,114,116,105,99,108,101,69,100,105,116,83,101,116,116,105,110,103,115,0,83,99,117,108,112,116,0,85,118,83,99,117,108,112,116,0,86,80,97,105,110,116,0,71,80,95,69,100,105,116,66,114,117,115,104,95,68,97,116,97,0,71,80,95,66,114,117,115,104,69,100,105,116,95,83,101,116,116,105,110,103,115,
0,71,80,95,73,110,116,101,114,112,111,108,97,116,101,95,83,101,116,116,105,110,103,115,0,84,114,97,110,115,102,111,114,109,79,114,105,101,110,116,97,116,105,111,110,0,85,110,105,102,105,101,100,80,97,105,110,116,83,101,116,116,105,110,103,115,0,67,111,108,111,114,83,112,97,99,101,0,67,117,114,118,101,80,97,105,110,116,83,101,116,116,105,110,103,115,0,77,101,115,104,83,116,97,116,86,105,115,0,84,111,111,108,83,101,116,116,105,110,103,
115,0,98,83,116,97,116,115,0,85,110,105,116,83,101,116,116,105,110,103,115,0,80,104,121,115,105,99,115,83,101,116,116,105,110,103,115,0,68,105,115,112,108,97,121,83,97,102,101,65,114,101,97,115,0,69,100,105,116,105,110,103,0,83,99,101,110,101,83,116,97,116,115,0,68,101,112,115,103,114,97,112,104,0,68,97,103,70,111,114,101,115,116,0,77,111,118,105,101,67,108,105,112,0,82,105,103,105,100,66,111,100,121,87,111,114,108,100,0,66,
71,112,105,99,0,77,111,118,105,101,67,108,105,112,85,115,101,114,0,82,101,103,105,111,110,86,105,101,119,51,68,0,82,101,110,100,101,114,69,110,103,105,110,101,0,86,105,101,119,68,101,112,116,104,115,0,83,109,111,111,116,104,86,105,101,119,51,68,83,116,111,114,101,0,119,109,84,105,109,101,114,0,71,80,85,70,88,0,86,105,101,119,51,68,0,83,112,97,99,101,76,105,110,107,0,86,105,101,119,50,68,0,83,109,111,111,116,104,86,105,
101,119,50,68,83,116,111,114,101,0,83,112,97,99,101,73,110,102,111,0,83,112,97,99,101,66,117,116,115,0,83,112,97,99,101,79,111,112,115,0,66,76,73,95,109,101,109,112,111,111,108,0,84,114,101,101,83,116,111,114,101,69,108,101,109,0,83,112,97,99,101,73,112,111,0,98,68,111,112,101,83,104,101,101,116,0,83,112,97,99,101,78,108,97,0,83,112,97,99,101,84,105,109,101,67,97,99,104,101,0,83,112,97,99,101,84,105,109,101,0,
83,112,97,99,101,83,101,113,0,83,101,113,117,101,110,99,101,114,83,99,111,112,101,115,0,77,97,115,107,83,112,97,99,101,73,110,102,111,0,77,97,115,107,0,70,105,108,101,83,101,108,101,99,116,80,97,114,97,109,115,0,83,112,97,99,101,70,105,108,101,0,70,105,108,101,76,105,115,116,0,119,109,79,112,101,114,97,116,111,114,0,70,105,108,101,76,97,121,111,117,116,0,70,83,77,101,110,117,69,110,116,114,121,0,65,115,115,101,116,85,
85,73,68,0,65,115,115,101,116,85,85,73,68,76,105,115,116,0,70,105,108,101,68,105,114,69,110,116,114,121,82,101,118,105,115,105,111,110,0,70,105,108,101,68,105,114,69,110,116,114,121,86,97,114,105,97,110,116,0,70,105,108,101,68,105,114,69,110,116,114,121,0,70,105,108,101,68,105,114,69,110,116,114,121,65,114,114,0,83,112,97,99,101,73,109,97,103,101,0,83,99,111,112,101,115,0,72,105,115,116,111,103,114,97,109,0,83,112,97,99,
101,84,101,120,116,0,83,99,114,105,112,116,0,83,112,97,99,101,83,99,114,105,112,116,0,98,78,111,100,101,84,114,101,101,80,97,116,104,0,98,78,111,100,101,73,110,115,116,97,110,99,101,75,101,121,0,83,112,97,99,101,78,111,100,101,0,78,111,100,101,73,110,115,101,114,116,79,102,115,68,97,116,97,0,83,112,97,99,101,76,111,103,105,99,0,67,111,110,115,111,108,101,76,105,110,101,0,83,112,97,99,101,67,111,110,115,111,108,101,0,
83,112,97,99,101,85,115,101,114,80,114,101,102,0,83,112,97,99,101,67,108,105,112,0,77,111,118,105,101,67,108,105,112,83,99,111,112,101,115,0,117,105,70,111,110,116,0,117,105,70,111,110,116,83,116,121,108,101,0,117,105,83,116,121,108,101,0,117,105,87,105,100,103,101,116,67,111,108,111,114,115,0,117,105,87,105,100,103,101,116,83,116,97,116,101,67,111,108,111,114,115,0,117,105,80,97,110,101,108,67,111,108,111,114,115,0,117,105,71,114,
97,100,105,101,110,116,67,111,108,111,114,115,0,84,104,101,109,101,85,73,0,84,104,101,109,101,83,112,97,99,101,0,84,104,101,109,101,87,105,114,101,67,111,108,111,114,0,98,84,104,101,109,101,0,98,65,100,100,111,110,0,98,80,97,116,104,67,111,109,112,97,114,101,0,83,111,108,105,100,76,105,103,104,116,0,87,97,108,107,78,97,118,105,103,97,116,105,111,110,0,85,115,101,114,68,101,102,0,98,83,99,114,101,101,110,0,83,99,114,86,
101,114,116,0,83,99,114,69,100,103,101,0,80,97,110,101,108,0,80,97,110,101,108,84,121,112,101,0,117,105,76,97,121,111,117,116,0,80,97,110,101,108,67,97,116,101,103,111,114,121,83,116,97,99,107,0,117,105,76,105,115,116,0,117,105,76,105,115,116,84,121,112,101,0,117,105,76,105,115,116,68,121,110,0,117,105,80,114,101,118,105,101,119,0,83,99,114,65,114,101,97,0,83,112,97,99,101,84,121,112,101,0,65,82,101,103,105,111,110,0,
65,82,101,103,105,111,110,84,121,112,101,0,70,105,108,101,71,108,111,98,97,108,0,83,116,114,105,112,65,110,105,109,0,83,116,114,105,112,69,108,101,109,0,83,116,114,105,112,67,114,111,112,0,83,116,114,105,112,84,114,97,110,115,102,111,114,109,0,83,116,114,105,112,67,111,108,111,114,66,97,108,97,110,99,101,0,83,116,114,105,112,80,114,111,120,121,0,83,116,114,105,112,0,83,101,113,117,101,110,99,101,0,98,83,111,117,110,100,0,77,
101,116,97,83,116,97,99,107,0,87,105,112,101,86,97,114,115,0,71,108,111,119,86,97,114,115,0,84,114,97,110,115,102,111,114,109,86,97,114,115,0,83,111,108,105,100,67,111,108,111,114,86,97,114,115,0,83,112,101,101,100,67,111,110,116,114,111,108,86,97,114,115,0,71,97,117,115,115,105,97,110,66,108,117,114,86,97,114,115,0,84,101,120,116,86,97,114,115,0,83,101,113,117,101,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,
67,111,108,111,114,66,97,108,97,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,67,117,114,118,101,115,77,111,100,105,102,105,101,114,68,97,116,97,0,72,117,101,67,111,114,114,101,99,116,77,111,100,105,102,105,101,114,68,97,116,97,0,66,114,105,103,104,116,67,111,110,116,114,97,115,116,77,111,100,105,102,105,101,114,68,97,116,97,0,83,101,113,117,101,110,99,101,114,77,97,115,107,77,111,100,105,102,105,101,114,68,97,116,97,0,
87,104,105,116,101,66,97,108,97,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,83,101,113,117,101,110,99,101,114,84,111,110,101,109,97,112,77,111,100,105,102,105,101,114,68,97,116,97,0,69,102,102,101,99,116,0,66,117,105,108,100,69,102,102,0,80,97,114,116,69,102,102,0,80,97,114,116,105,99,108,101,0,87,97,118,101,69,102,102,0,84,114,101,101,83,116,111,114,101,0,98,80,114,111,112,101,114,116,121,0,98,78,101,97,114,
83,101,110,115,111,114,0,98,77,111,117,115,101,83,101,110,115,111,114,0,98,84,111,117,99,104,83,101,110,115,111,114,0,98,75,101,121,98,111,97,114,100,83,101,110,115,111,114,0,98,80,114,111,112,101,114,116,121,83,101,110,115,111,114,0,98,65,99,116,117,97,116,111,114,83,101,110,115,111,114,0,98,68,101,108,97,121,83,101,110,115,111,114,0,98,67,111,108,108,105,115,105,111,110,83,101,110,115,111,114,0,98,82,97,100,97,114,83,101,110,
115,111,114,0,98,82,97,110,100,111,109,83,101,110,115,111,114,0,98,82,97,121,83,101,110,115,111,114,0,98,65,114,109,97,116,117,114,101,83,101,110,115,111,114,0,98,77,101,115,115,97,103,101,83,101,110,115,111,114,0,98,83,101,110,115,111,114,0,98,67,111,110,116,114,111,108,108,101,114,0,98,74,111,121,115,116,105,99,107,83,101,110,115,111,114,0,98,69,120,112,114,101,115,115,105,111,110,67,111,110,116,0,98,80,121,116,104,111,110,67,
111,110,116,0,98,65,99,116,117,97,116,111,114,0,98,65,100,100,79,98,106,101,99,116,65,99,116,117,97,116,111,114,0,98,65,99,116,105,111,110,65,99,116,117,97,116,111,114,0,83,111,117,110,100,51,68,0,98,83,111,117,110,100,65,99,116,117,97,116,111,114,0,98,69,100,105,116,79,98,106,101,99,116,65,99,116,117,97,116,111,114,0,98,83,99,101,110,101,65,99,116,117,97,116,111,114,0,98,80,114,111,112,101,114,116,121,65,99,116,117,
97,116,111,114,0,98,79,98,106,101,99,116,65,99,116,117,97,116,111,114,0,98,73,112,111,65,99,116,117,97,116,111,114,0,98,67,97,109,101,114,97,65,99,116,117,97,116,111,114,0,98,67,111,110,115,116,114,97,105,110,116,65,99,116,117,97,116,111,114,0,98,71,114,111,117,112,65,99,116,117,97,116,111,114,0,98,82,97,110,100,111,109,65,99,116,117,97,116,111,114,0,98,77,101,115,115,97,103,101,65,99,116,117,97,116,111,114,0,98,71,
97,109,101,65,99,116,117,97,116,111,114,0,98,86,105,115,105,98,105,108,105,116,121,65,99,116,117,97,116,111,114,0,98,84,119,111,68,70,105,108,116,101,114,65,99,116,117,97,116,111,114,0,98,80,97,114,101,110,116,65,99,116,117,97,116,111,114,0,98,83,116,97,116,101,65,99,116,117,97,116,111,114,0,98,65,114,109,97,116,117,114,101,65,99,116,117,97,116,111,114,0,98,83,116,101,101,114,105,110,103,65,99,116,117,97,116,111,114,0,98,
77,111,117,115,101,65,99,116,117,97,116,111,114,0,71,114,111,117,112,79,98,106,101,99,116,0,66,111,110,101,0,98,65,114,109,97,116,117,114,101,0,69,100,105,116,66,111,110,101,0,98,77,111,116,105,111,110,80,97,116,104,86,101,114,116,0,98,80,111,115,101,67,104,97,110,110,101,108,0,98,73,75,80,97,114,97,109,0,98,73,116,97,115,99,0,98,65,99,116,105,111,110,71,114,111,117,112,0,83,112,97,99,101,65,99,116,105,111,110,0,
98,65,99,116,105,111,110,67,104,97,110,110,101,108,0,98,67,111,110,115,116,114,97,105,110,116,67,104,97,110,110,101,108,0,98,67,111,110,115,116,114,97,105,110,116,0,98,67,111,110,115,116,114,97,105,110,116,84,97,114,103,101,116,0,98,80,121,116,104,111,110,67,111,110,115,116,114,97,105,110,116,0,98,75,105,110,101,109,97,116,105,99,67,111,110,115,116,114,97,105,110,116,0,98,83,112,108,105,110,101,73,75,67,111,110,115,116,114,97,105,
110,116,0,98,84,114,97,99,107,84,111,67,111,110,115,116,114,97,105,110,116,0,98,82,111,116,97,116,101,76,105,107,101,67,111,110,115,116,114,97,105,110,116,0,98,76,111,99,97,116,101,76,105,107,101,67,111,110,115,116,114,97,105,110,116,0,98,83,105,122,101,76,105,107,101,67,111,110,115,116,114,97,105,110,116,0,98,83,97,109,101,86,111,108,117,109,101,67,111,110,115,116,114,97,105,110,116,0,98,84,114,97,110,115,76,105,107,101,67,111,
110,115,116,114,97,105,110,116,0,98,77,105,110,77,97,120,67,111,110,115,116,114,97,105,110,116,0,98,65,99,116,105,111,110,67,111,110,115,116,114,97,105,110,116,0,98,76,111,99,107,84,114,97,99,107,67,111,110,115,116,114,97,105,110,116,0,98,68,97,109,112,84,114,97,99,107,67,111,110,115,116,114,97,105,110,116,0,98,70,111,108,108,111,119,80,97,116,104,67,111,110,115,116,114,97,105,110,116,0,98,83,116,114,101,116,99,104,84,111,67,
111,110,115,116,114,97,105,110,116,0,98,82,105,103,105,100,66,111,100,121,74,111,105,110,116,67,111,110,115,116,114,97,105,110,116,0,98,67,108,97,109,112,84,111,67,111,110,115,116,114,97,105,110,116,0,98,67,104,105,108,100,79,102,67,111,110,115,116,114,97,105,110,116,0,98,84,114,97,110,115,102,111,114,109,67,111,110,115,116,114,97,105,110,116,0,98,80,105,118,111,116,67,111,110,115,116,114,97,105,110,116,0,98,76,111,99,76,105,109,105,
116,67,111,110,115,116,114,97,105,110,116,0,98,82,111,116,76,105,109,105,116,67,111,110,115,116,114,97,105,110,116,0,98,83,105,122,101,76,105,109,105,116,67,111,110,115,116,114,97,105,110,116,0,98,68,105,115,116,76,105,109,105,116,67,111,110,115,116,114,97,105,110,116,0,98,83,104,114,105,110,107,119,114,97,112,67,111,110,115,116,114,97,105,110,116,0,98,70,111,108,108,111,119,84,114,97,99,107,67,111,110,115,116,114,97,105,110,116,0,98,
67,97,109,101,114,97,83,111,108,118,101,114,67,111,110,115,116,114,97,105,110,116,0,98,79,98,106,101,99,116,83,111,108,118,101,114,67,111,110,115,116,114,97,105,110,116,0,98,84,114,97,110,115,102,111,114,109,67,97,99,104,101,67,111,110,115,116,114,97,105,110,116,0,98,65,99,116,105,111,110,77,111,100,105,102,105,101,114,0,98,65,99,116,105,111,110,83,116,114,105,112,0,98,78,111,100,101,83,116,97,99,107,0,98,78,111,100,101,83,111,
99,107,101,116,0,98,78,111,100,101,83,111,99,107,101,116,84,121,112,101,0,98,78,111,100,101,76,105,110,107,0,98,78,111,100,101,0,98,78,111,100,101,84,121,112,101,0,117,105,66,108,111,99,107,0,98,78,111,100,101,73,110,115,116,97,110,99,101,72,97,115,104,69,110,116,114,121,0,98,78,111,100,101,80,114,101,118,105,101,119,0,98,78,111,100,101,84,114,101,101,84,121,112,101,0,83,116,114,117,99,116,82,78,65,0,98,78,111,100,101,
73,110,115,116,97,110,99,101,72,97,115,104,0,98,78,111,100,101,84,114,101,101,69,120,101,99,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,73,110,116,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,70,108,111,97,116,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,66,111,111,108,101,97,110,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,86,101,99,116,111,114,0,98,78,111,100,
101,83,111,99,107,101,116,86,97,108,117,101,82,71,66,65,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,83,116,114,105,110,103,0,78,111,100,101,70,114,97,109,101,0,78,111,100,101,73,109,97,103,101,65,110,105,109,0,67,111,108,111,114,67,111,114,114,101,99,116,105,111,110,68,97,116,97,0,78,111,100,101,67,111,108,111,114,67,111,114,114,101,99,116,105,111,110,0,78,111,100,101,66,111,107,101,104,73,109,97,103,101,0,78,
111,100,101,66,111,120,77,97,115,107,0,78,111,100,101,69,108,108,105,112,115,101,77,97,115,107,0,78,111,100,101,73,109,97,103,101,76,97,121,101,114,0,78,111,100,101,66,108,117,114,68,97,116,97,0,78,111,100,101,68,66,108,117,114,68,97,116,97,0,78,111,100,101,66,105,108,97,116,101,114,97,108,66,108,117,114,68,97,116,97,0,78,111,100,101,72,117,101,83,97,116,0,78,111,100,101,73,109,97,103,101,70,105,108,101,0,78,111,100,101,
73,109,97,103,101,77,117,108,116,105,70,105,108,101,0,78,111,100,101,73,109,97,103,101,77,117,108,116,105,70,105,108,101,83,111,99,107,101,116,0,78,111,100,101,67,104,114,111,109,97,0,78,111,100,101,84,119,111,88,89,115,0,78,111,100,101,84,119,111,70,108,111,97,116,115,0,78,111,100,101,71,101,111,109,101,116,114,121,0,78,111,100,101,86,101,114,116,101,120,67,111,108,0,78,111,100,101,68,101,102,111,99,117,115,0,78,111,100,101,83,
99,114,105,112,116,68,105,99,116,0,78,111,100,101,71,108,97,114,101,0,78,111,100,101,84,111,110,101,109,97,112,0,78,111,100,101,76,101,110,115,68,105,115,116,0,78,111,100,101,67,111,108,111,114,66,97,108,97,110,99,101,0,78,111,100,101,67,111,108,111,114,115,112,105,108,108,0,78,111,100,101,68,105,108,97,116,101,69,114,111,100,101,0,78,111,100,101,77,97,115,107,0,78,111,100,101,84,101,120,66,97,115,101,0,78,111,100,101,84,101,
120,83,107,121,0,78,111,100,101,84,101,120,73,109,97,103,101,0,78,111,100,101,84,101,120,67,104,101,99,107,101,114,0,78,111,100,101,84,101,120,66,114,105,99,107,0,78,111,100,101,84,101,120,69,110,118,105,114,111,110,109,101,110,116,0,78,111,100,101,84,101,120,71,114,97,100,105,101,110,116,0,78,111,100,101,84,101,120,78,111,105,115,101,0,78,111,100,101,84,101,120,86,111,114,111,110,111,105,0,78,111,100,101,84,101,120,77,117,115,103,
114,97,118,101,0,78,111,100,101,84,101,120,87,97,118,101,0,78,111,100,101,84,101,120,77,97,103,105,99,0,78,111,100,101,83,104,97,100,101,114,65,116,116,114,105,98,117,116,101,0,78,111,100,101,83,104,97,100,101,114,86,101,99,116,84,114,97,110,115,102,111,114,109,0,78,111,100,101,83,104,97,100,101,114,84,101,120,80,111,105,110,116,68,101,110,115,105,116,121,0,84,101,120,78,111,100,101,79,117,116,112,117,116,0,78,111,100,101,75,101,
121,105,110,103,83,99,114,101,101,110,68,97,116,97,0,78,111,100,101,75,101,121,105,110,103,68,97,116,97,0,78,111,100,101,84,114,97,99,107,80,111,115,68,97,116,97,0,78,111,100,101,84,114,97,110,115,108,97,116,101,68,97,116,97,0,78,111,100,101,80,108,97,110,101,84,114,97,99,107,68,101,102,111,114,109,68,97,116,97,0,78,111,100,101,83,104,97,100,101,114,83,99,114,105,112,116,0,78,111,100,101,83,104,97,100,101,114,84,97,110,
103,101,110,116,0,78,111,100,101,83,104,97,100,101,114,78,111,114,109,97,108,77,97,112,0,78,111,100,101,83,104,97,100,101,114,85,86,77,97,112,0,78,111,100,101,83,117,110,66,101,97,109,115,0,67,117,114,118,101,77,97,112,80,111,105,110,116,0,67,117,114,118,101,77,97,112,0,66,114,117,115,104,67,108,111,110,101,0,80,97,105,110,116,67,117,114,118,101,0,80,97,108,101,116,116,101,67,111,108,111,114,0,80,97,105,110,116,67,117,114,
118,101,80,111,105,110,116,0,67,117,115,116,111,109,68,97,116,97,76,97,121,101,114,0,67,117,115,116,111,109,68,97,116,97,69,120,116,101,114,110,97,108,0,72,97,105,114,75,101,121,0,80,97,114,116,105,99,108,101,75,101,121,0,66,111,105,100,80,97,114,116,105,99,108,101,0,66,111,105,100,68,97,116,97,0,80,97,114,116,105,99,108,101,83,112,114,105,110,103,0,67,104,105,108,100,80,97,114,116,105,99,108,101,0,80,97,114,116,105,99,
108,101,84,97,114,103,101,116,0,80,97,114,116,105,99,108,101,68,117,112,108,105,87,101,105,103,104,116,0,80,97,114,116,105,99,108,101,68,97,116,97,0,83,80,72,70,108,117,105,100,83,101,116,116,105,110,103,115,0,80,97,114,116,105,99,108,101,83,101,116,116,105,110,103,115,0,66,111,105,100,83,101,116,116,105,110,103,115,0,80,97,114,116,105,99,108,101,67,97,99,104,101,75,101,121,0,76,97,116,116,105,99,101,68,101,102,111,114,109,68,
97,116,97,0,80,97,114,116,105,99,108,101,82,101,110,100,101,114,68,97,116,97,0,75,68,84,114,101,101,0,80,97,114,116,105,99,108,101,68,114,97,119,68,97,116,97,0,76,105,110,107,78,111,100,101,0,98,71,80,68,115,112,111,105,110,116,0,98,71,80,68,116,114,105,97,110,103,108,101,0,98,71,80,68,98,114,117,115,104,0,98,71,80,68,112,97,108,101,116,116,101,99,111,108,111,114,0,98,71,80,68,112,97,108,101,116,116,101,0,98,
71,80,68,115,116,114,111,107,101,0,98,71,80,68,102,114,97,109,101,0,98,71,80,68,108,97,121,101,114,0,82,101,112,111,114,116,76,105,115,116,0,119,109,87,105,110,100,111,119,77,97,110,97,103,101,114,0,119,109,87,105,110,100,111,119,0,119,109,75,101,121,67,111,110,102,105,103,0,119,109,69,118,101,110,116,0,119,109,83,117,98,87,105,110,100,111,119,0,119,109,71,101,115,116,117,114,101,0,119,109,73,77,69,68,97,116,97,0,119,109,
75,101,121,77,97,112,73,116,101,109,0,80,111,105,110,116,101,114,82,78,65,0,119,109,75,101,121,77,97,112,68,105,102,102,73,116,101,109,0,119,109,75,101,121,77,97,112,0,119,109,79,112,101,114,97,116,111,114,84,121,112,101,0,70,77,111,100,105,102,105,101,114,0,70,77,111,100,95,71,101,110,101,114,97,116,111,114,0,70,77,111,100,95,70,117,110,99,116,105,111,110,71,101,110,101,114,97,116,111,114,0,70,67,77,95,69,110,118,101,108,
111,112,101,68,97,116,97,0,70,77,111,100,95,69,110,118,101,108,111,112,101,0,70,77,111,100,95,67,121,99,108,101,115,0,70,77,111,100,95,80,121,116,104,111,110,0,70,77,111,100,95,76,105,109,105,116,115,0,70,77,111,100,95,78,111,105,115,101,0,70,77,111,100,95,83,116,101,112,112,101,100,0,68,114,105,118,101,114,84,97,114,103,101,116,0,68,114,105,118,101,114,86,97,114,0,67,104,97,110,110,101,108,68,114,105,118,101,114,0,70,
80,111,105,110,116,0,70,67,117,114,118,101,0,65,110,105,109,77,97,112,80,97,105,114,0,65,110,105,109,77,97,112,112,101,114,0,78,108,97,83,116,114,105,112,0,78,108,97,84,114,97,99,107,0,75,83,95,80,97,116,104,0,75,101,121,105,110,103,83,101,116,0,65,110,105,109,79,118,101,114,114,105,100,101,0,73,100,65,100,116,84,101,109,112,108,97,116,101,0,66,111,105,100,82,117,108,101,0,66,111,105,100,82,117,108,101,71,111,97,108,
65,118,111,105,100,0,66,111,105,100,82,117,108,101,65,118,111,105,100,67,111,108,108,105,115,105,111,110,0,66,111,105,100,82,117,108,101,70,111,108,108,111,119,76,101,97,100,101,114,0,66,111,105,100,82,117,108,101,65,118,101,114,97,103,101,83,112,101,101,100,0,66,111,105,100,82,117,108,101,70,105,103,104,116,0,66,111,105,100,83,116,97,116,101,0,70,76,85,73,68,95,51,68,0,87,84,85,82,66,85,76,69,78,67,69,0,83,112,101,97,
107,101,114,0,77,111,118,105,101,67,108,105,112,80,114,111,120,121,0,77,111,118,105,101,67,108,105,112,67,97,99,104,101,0,77,111,118,105,101,84,114,97,99,107,105,110,103,0,77,111,118,105,101,84,114,97,99,107,105,110,103,77,97,114,107,101,114,0,77,111,118,105,101,84,114,97,99,107,105,110,103,84,114,97,99,107,0,77,111,118,105,101,82,101,99,111,110,115,116,114,117,99,116,101,100,67,97,109,101,114,97,0,77,111,118,105,101,84,114,97,
99,107,105,110,103,67,97,109,101,114,97,0,77,111,118,105,101,84,114,97,99,107,105,110,103,80,108,97,110,101,77,97,114,107,101,114,0,77,111,118,105,101,84,114,97,99,107,105,110,103,80,108,97,110,101,84,114,97,99,107,0,77,111,118,105,101,84,114,97,99,107,105,110,103,83,101,116,116,105,110,103,115,0,77,111,118,105,101,84,114,97,99,107,105,110,103,83,116,97,98,105,108,105,122,97,116,105,111,110,0,77,111,118,105,101,84,114,97,99,107,
105,110,103,82,101,99,111,110,115,116,114,117,99,116,105,111,110,0,77,111,118,105,101,84,114,97,99,107,105,110,103,79,98,106,101,99,116,0,77,111,118,105,101,84,114,97,99,107,105,110,103,83,116,97,116,115,0,77,111,118,105,101,84,114,97,99,107,105,110,103,68,111,112,101,115,104,101,101,116,67,104,97,110,110,101,108,0,77,111,118,105,101,84,114,97,99,107,105,110,103,68,111,112,101,115,104,101,101,116,67,111,118,101,114,97,103,101,83,101,103,
109,101,110,116,0,77,111,118,105,101,84,114,97,99,107,105,110,103,68,111,112,101,115,104,101,101,116,0,68,121,110,97,109,105,99,80,97,105,110,116,83,117,114,102,97,99,101,0,80,97,105,110,116,83,117,114,102,97,99,101,68,97,116,97,0,77,97,115,107,80,97,114,101,110,116,0,77,97,115,107,83,112,108,105,110,101,80,111,105,110,116,85,87,0,77,97,115,107,83,112,108,105,110,101,80,111,105,110,116,0,77,97,115,107,83,112,108,105,110,101,
0,77,97,115,107,76,97,121,101,114,83,104,97,112,101,0,77,97,115,107,76,97,121,101,114,0,70,114,101,101,115,116,121,108,101,76,105,110,101,83,101,116,0,70,114,101,101,115,116,121,108,101,76,105,110,101,83,116,121,108,101,0,70,114,101,101,115,116,121,108,101,77,111,100,117,108,101,67,111,110,102,105,103,0,76,105,110,101,83,116,121,108,101,77,111,100,105,102,105,101,114,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,
102,105,101,114,95,65,108,111,110,103,83,116,114,111,107,101,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,65,108,111,110,103,83,116,114,111,107,101,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,65,108,111,110,103,83,116,114,111,107,101,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,
99,101,70,114,111,109,67,97,109,101,114,97,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,67,97,109,101,114,97,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,67,97,109,101,114,97,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,
105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,79,98,106,101,99,116,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,79,98,106,101,99,116,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,79,98,106,101,99,116,0,76,105,110,101,83,116,121,108,101,
67,111,108,111,114,77,111,100,105,102,105,101,114,95,67,117,114,118,97,116,117,114,101,95,51,68,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,67,117,114,118,97,116,117,114,101,95,51,68,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,67,117,114,118,97,116,117,114,101,95,51,68,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,
105,102,105,101,114,95,78,111,105,115,101,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,78,111,105,115,101,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,78,111,105,115,101,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,67,114,101,97,115,101,65,110,103,108,101,0,76,105,110,101,83,116,121,108,101,65,108,
112,104,97,77,111,100,105,102,105,101,114,95,67,114,101,97,115,101,65,110,103,108,101,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,67,114,101,97,115,101,65,110,103,108,101,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,84,97,110,103,101,110,116,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,84,97,110,
103,101,110,116,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,84,97,110,103,101,110,116,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,77,97,116,101,114,105,97,108,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,77,97,116,101,114,105,97,108,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,
115,115,77,111,100,105,102,105,101,114,95,77,97,116,101,114,105,97,108,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,83,97,109,112,108,105,110,103,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,66,101,122,105,101,114,67,117,114,118,101,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,83,
105,110,117,115,68,105,115,112,108,97,99,101,109,101,110,116,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,83,112,97,116,105,97,108,78,111,105,115,101,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,80,101,114,108,105,110,78,111,105,115,101,49,68,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,
114,95,80,101,114,108,105,110,78,111,105,115,101,50,68,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,66,97,99,107,98,111,110,101,83,116,114,101,116,99,104,101,114,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,84,105,112,82,101,109,111,118,101,114,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,
101,114,95,80,111,108,121,103,111,110,97,108,105,122,97,116,105,111,110,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,71,117,105,100,105,110,103,76,105,110,101,115,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,66,108,117,101,112,114,105,110,116,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,
95,50,68,79,102,102,115,101,116,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,50,68,84,114,97,110,115,102,111,114,109,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,83,105,109,112,108,105,102,105,99,97,116,105,111,110,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,67,97,108,108,
105,103,114,97,112,104,121,0,65,108,101,109,98,105,99,79,98,106,101,99,116,80,97,116,104,0,65,98,99,65,114,99,104,105,118,101,72,97,110,100,108,101,0,0,84,76,69,78,1,0,1,0,2,0,2,0,4,0,4,0,4,0,4,0,8,0,8,0,8,0,0,0,16,0,24,0,16,0,4,0,8,0,12,0,16,0,16,0,32,0,128,0,120,0,152,8,0,0,16,0,64,0,0,0,144,0,160,5,112,0,36,0,72,0,160,0,184,0,224,0,
104,0,40,0,208,0,24,0,32,0,24,0,24,0,248,0,40,0,96,21,24,0,0,0,80,4,24,4,64,0,200,7,0,0,0,0,64,0,8,0,64,1,160,1,24,0,8,3,200,0,0,0,168,0,128,1,64,4,80,0,184,1,144,0,56,3,24,2,88,0,16,0,24,0,248,5,160,0,144,4,0,0,104,0,104,0,248,0,88,0,8,0,16,0,32,0,0,0,16,2,0,0,136,5,8,0,12,0,16,0,8,0,12,0,4,0,20,0,48,0,
64,0,20,0,12,0,16,0,4,0,0,0,208,0,208,1,8,0,4,0,4,0,0,1,32,0,8,0,24,0,16,0,64,0,24,0,12,0,64,0,4,0,16,0,16,0,4,0,4,0,112,0,200,0,144,0,192,0,192,0,128,0,192,0,192,0,136,0,120,0,208,0,144,0,240,2,184,0,32,0,24,1,40,1,200,0,184,0,200,0,64,1,200,0,104,1,112,0,224,0,0,0,192,0,64,0,40,5,0,0,0,0,200,0,0,0,0,0,152,0,
0,0,0,0,128,0,8,0,8,0,104,1,152,0,144,2,136,0,192,0,120,0,128,0,216,4,208,0,200,0,112,0,216,0,152,0,16,5,0,0,0,0,48,1,104,1,160,1,104,1,136,0,104,0,112,0,128,0,120,0,128,0,192,0,216,0,144,1,152,4,208,0,200,0,8,1,216,0,136,4,176,4,0,0,32,0,16,0,208,0,16,0,24,1,88,0,40,0,0,0,200,0,216,0,200,0,48,0,40,0,120,0,160,0,224,1,0,0,96,0,
152,0,0,1,192,0,0,0,80,0,32,0,176,0,0,0,16,0,0,0,0,0,0,0,12,0,32,2,40,0,184,0,152,0,64,0,88,0,32,0,192,0,56,0,160,0,0,1,160,0,64,0,88,5,80,17,64,0,24,0,16,0,56,0,192,0,96,0,64,0,24,8,144,0,128,0,16,0,176,0,112,0,64,0,96,0,8,0,112,0,24,0,120,0,152,0,0,0,32,0,40,0,128,3,32,0,8,0,24,0,32,0,80,12,0,0,0,0,0,0,
112,9,88,0,112,0,8,0,136,3,0,0,0,0,0,0,0,0,0,0,168,1,56,0,160,0,0,0,64,0,16,1,80,1,0,0,16,0,8,1,112,0,232,0,24,0,224,0,64,1,48,0,16,0,168,0,20,9,112,0,0,0,168,0,0,0,24,1,48,0,16,0,104,0,72,0,128,0,32,4,88,41,144,20,40,20,168,2,168,5,64,0,104,0,4,0,152,1,0,0,72,0,40,0,136,1,104,0,152,1,136,0,24,4,32,0,232,0,32,0,
32,0,16,0,16,0,144,3,248,2,16,0,128,55,88,0,24,3,56,0,32,0,48,47,240,0,32,0,40,0,16,1,0,0,0,0,80,0,200,0,0,0,0,0,88,0,160,0,0,0,128,1,0,0,64,4,24,0,8,1,16,0,8,0,44,0,24,4,136,3,120,1,216,4,40,0,12,0,24,0,32,0,16,0,24,0,8,0,56,2,112,0,160,0,240,1,240,1,120,0,112,0,128,0,144,0,24,0,32,0,136,1,0,0,64,0,16,0,96,0,
80,0,136,0,80,0,136,0,200,0,72,0,8,0,136,0,76,0,72,0,204,0,136,0,136,0,128,0,136,0,92,0,128,0,80,0,112,0,16,0,168,0,32,0,72,0,128,0,24,0,144,0,112,0,148,0,32,0,128,0,88,0,88,0,208,0,140,0,4,0,24,0,16,0,8,0,160,0,48,0,44,0,40,0,104,1,0,1,0,0,16,0,88,2,4,0,40,0,120,0,88,1,120,0,56,0,120,0,160,0,112,0,184,0,40,0,88,0,80,0,
80,0,80,0,8,0,72,0,104,0,104,0,80,0,80,0,24,0,104,0,104,0,16,0,144,0,224,0,88,0,28,0,28,0,28,0,88,0,24,0,160,0,16,0,152,0,16,4,72,0,168,0,48,0,96,1,0,0,56,0,216,1,0,0,0,0,8,0,24,0,0,0,0,0,0,0,0,0,16,0,16,0,4,0,24,0,16,0,8,4,4,0,16,0,24,0,104,0,20,0,24,0,24,0,68,0,40,0,28,0,12,0,12,0,8,5,16,5,40,5,
44,0,24,0,8,0,128,0,64,0,32,0,16,0,32,0,32,0,8,0,80,0,20,0,8,0,8,0,200,3,224,3,8,4,200,3,216,3,0,4,208,3,200,3,208,3,208,3,208,3,208,3,64,0,16,0,208,4,64,0,64,0,48,0,128,0,8,0,136,0,80,4,72,0,68,0,64,0,12,0,12,0,72,0,24,0,136,0,32,0,76,0,104,0,0,4,36,0,56,0,56,0,20,0,16,0,64,0,40,0,32,0,200,0,68,0,88,3,104,0,
0,0,0,0,0,0,0,0,0,0,0,0,28,0,12,0,144,0,120,0,104,0,64,1,40,0,136,1,40,0,96,1,32,1,168,0,0,0,0,0,0,0,0,0,184,0,0,0,32,0,136,0,0,0,112,0,24,0,24,0,16,0,24,0,8,0,16,0,24,0,20,0,20,0,88,0,24,3,40,1,16,0,112,0,0,1,40,0,208,0,104,0,112,0,224,1,32,0,128,0,56,0,80,0,64,0,104,0,72,0,64,0,128,0,0,0,0,0,184,0,
8,3,0,0,96,1,64,0,208,0,72,0,56,0,40,0,128,0,72,0,72,0,24,0,152,0,0,1,112,0,32,0,48,0,24,6,0,0,184,0,12,0,16,1,224,0,40,0,144,0,128,0,208,1,32,0,96,0,104,0,112,0,120,0,112,0,120,0,128,0,120,0,128,0,136,0,120,0,120,0,128,0,120,0,120,0,112,0,112,0,120,0,128,0,104,0,112,0,120,0,112,0,112,0,120,0,104,0,104,0,112,0,112,0,120,0,120,0,104,0,
104,0,104,0,104,0,120,0,112,0,128,0,104,0,112,0,16,16,0,0,83,84,82,67,109,2,0,0,12,0,2,0,12,0,0,0,12,0,1,0,13,0,3,0,13,0,0,0,13,0,1,0,11,0,2,0,14,0,2,0,11,0,3,0,11,0,4,0,15,0,2,0,2,0,5,0,2,0,6,0,16,0,2,0,7,0,5,0,7,0,6,0,17,0,3,0,7,0,5,0,7,0,6,0,7,0,7,0,18,0,4,0,4,0,8,0,4,0,9,0,
4,0,10,0,4,0,11,0,19,0,4,0,7,0,8,0,7,0,9,0,7,0,10,0,7,0,11,0,20,0,4,0,11,0,12,0,14,0,13,0,4,0,14,0,4,0,15,0,21,0,10,0,21,0,0,0,21,0,1,0,0,0,16,0,0,0,17,0,2,0,18,0,0,0,19,0,4,0,20,0,20,0,21,0,4,0,22,0,4,0,23,0,22,0,11,0,11,0,0,0,11,0,1,0,22,0,24,0,23,0,25,0,0,0,26,0,2,0,18,0,
2,0,27,0,2,0,28,0,4,0,29,0,4,0,30,0,21,0,31,0,23,0,9,0,22,0,32,0,24,0,33,0,0,0,34,0,0,0,35,0,23,0,36,0,25,0,37,0,4,0,38,0,2,0,39,0,2,0,40,0,26,0,9,0,4,0,41,0,4,0,42,0,2,0,43,0,2,0,44,0,4,0,45,0,27,0,46,0,4,0,30,0,2,0,27,0,0,0,47,0,28,0,6,0,29,0,48,0,2,0,49,0,2,0,50,0,2,0,16,0,
2,0,18,0,0,0,51,0,30,0,21,0,30,0,0,0,30,0,1,0,31,0,52,0,32,0,53,0,19,0,54,0,19,0,55,0,2,0,49,0,2,0,50,0,2,0,56,0,2,0,57,0,2,0,58,0,2,0,59,0,2,0,18,0,2,0,60,0,7,0,10,0,7,0,11,0,4,0,61,0,7,0,62,0,7,0,63,0,7,0,64,0,28,0,65,0,33,0,7,0,22,0,32,0,14,0,66,0,19,0,67,0,2,0,49,0,2,0,68,0,
2,0,69,0,2,0,70,0,34,0,15,0,34,0,0,0,34,0,1,0,7,0,71,0,7,0,64,0,2,0,16,0,2,0,72,0,2,0,73,0,2,0,18,0,4,0,74,0,4,0,75,0,11,0,2,0,0,0,19,0,0,0,76,0,7,0,77,0,7,0,78,0,35,0,15,0,22,0,32,0,36,0,79,0,34,0,80,0,0,0,81,0,4,0,82,0,4,0,70,0,14,0,83,0,33,0,84,0,22,0,85,0,4,0,86,0,2,0,18,0,
0,0,16,0,0,0,87,0,7,0,88,0,4,0,89,0,37,0,6,0,37,0,0,0,37,0,1,0,0,0,90,0,0,0,91,0,4,0,22,0,4,0,92,0,38,0,14,0,22,0,32,0,0,0,93,0,4,0,94,0,4,0,95,0,14,0,96,0,37,0,97,0,37,0,98,0,4,0,99,0,4,0,100,0,0,0,101,0,4,0,102,0,4,0,103,0,11,0,104,0,8,0,105,0,25,0,3,0,4,0,106,0,4,0,107,0,11,0,2,0,
39,0,6,0,7,0,108,0,7,0,109,0,7,0,110,0,7,0,111,0,4,0,112,0,4,0,113,0,40,0,6,0,7,0,114,0,7,0,115,0,7,0,116,0,7,0,117,0,4,0,118,0,4,0,70,0,41,0,4,0,39,0,119,0,40,0,120,0,0,0,121,0,0,0,122,0,42,0,8,0,7,0,123,0,7,0,124,0,2,0,125,0,2,0,126,0,2,0,18,0,2,0,70,0,7,0,127,0,7,0,128,0,43,0,22,0,22,0,32,0,
36,0,79,0,0,0,16,0,0,0,129,0,2,0,18,0,7,0,130,0,7,0,131,0,7,0,132,0,7,0,133,0,7,0,134,0,7,0,135,0,7,0,136,0,7,0,137,0,7,0,138,0,7,0,139,0,7,0,140,0,33,0,84,0,29,0,141,0,39,0,142,0,0,0,143,0,0,0,122,0,42,0,144,0,44,0,15,0,45,0,145,0,4,0,146,0,4,0,147,0,4,0,148,0,4,0,149,0,0,0,150,0,0,0,151,0,0,0,152,0,
0,0,153,0,2,0,154,0,2,0,70,0,2,0,155,0,2,0,156,0,2,0,157,0,2,0,18,0,46,0,3,0,46,0,0,0,46,0,1,0,47,0,158,0,48,0,4,0,48,0,0,0,48,0,1,0,0,0,19,0,0,0,35,0,49,0,4,0,49,0,0,0,49,0,1,0,25,0,37,0,0,0,35,0,50,0,1,0,0,0,19,0,51,0,45,0,22,0,32,0,0,0,34,0,52,0,159,0,27,0,46,0,14,0,160,0,53,0,161,0,
53,0,162,0,2,0,163,0,2,0,164,0,4,0,18,0,2,0,165,0,2,0,16,0,4,0,166,0,2,0,167,0,2,0,168,0,2,0,169,0,2,0,170,0,2,0,171,0,2,0,172,0,4,0,173,0,0,0,174,0,4,0,175,0,25,0,37,0,14,0,176,0,26,0,177,0,7,0,178,0,4,0,179,0,2,0,180,0,2,0,152,0,4,0,181,0,4,0,182,0,0,0,183,0,0,0,184,0,2,0,185,0,7,0,186,0,7,0,187,0,
7,0,188,0,54,0,189,0,0,0,190,0,0,0,191,0,0,0,192,0,0,0,193,0,14,0,194,0,55,0,195,0,50,0,196,0,56,0,69,0,2,0,197,0,2,0,198,0,2,0,199,0,2,0,200,0,29,0,201,0,57,0,202,0,0,0,203,0,0,0,204,0,0,0,205,0,0,0,206,0,0,0,207,0,0,0,208,0,0,0,209,0,0,0,47,0,7,0,210,0,7,0,211,0,7,0,212,0,7,0,213,0,2,0,214,0,2,0,215,0,
2,0,216,0,2,0,217,0,2,0,218,0,2,0,219,0,7,0,220,0,7,0,221,0,7,0,222,0,7,0,223,0,7,0,224,0,7,0,60,0,7,0,225,0,7,0,226,0,7,0,227,0,7,0,228,0,7,0,229,0,7,0,230,0,7,0,231,0,7,0,232,0,7,0,233,0,7,0,234,0,7,0,235,0,7,0,236,0,7,0,237,0,7,0,238,0,7,0,239,0,7,0,240,0,7,0,241,0,7,0,242,0,7,0,243,0,7,0,244,0,
7,0,245,0,7,0,246,0,7,0,247,0,7,0,248,0,7,0,249,0,7,0,250,0,7,0,251,0,7,0,252,0,7,0,253,0,7,0,254,0,7,0,255,0,7,0,0,1,7,0,1,1,7,0,2,1,4,0,87,0,7,0,3,1,7,0,4,1,7,0,5,1,7,0,6,1,58,0,6,0,7,0,220,0,7,0,221,0,7,0,222,0,7,0,7,1,7,0,71,0,4,0,67,0,59,0,7,0,2,0,8,1,2,0,67,0,0,0,9,1,
0,0,10,1,0,0,11,1,0,0,12,1,58,0,13,1,60,0,17,0,29,0,201,0,51,0,14,1,61,0,15,1,7,0,16,1,7,0,17,1,2,0,16,0,2,0,18,1,7,0,131,0,7,0,132,0,7,0,19,1,4,0,20,1,2,0,21,1,2,0,22,1,4,0,152,0,4,0,166,0,2,0,23,1,2,0,24,1,62,0,27,0,2,0,18,0,2,0,25,1,7,0,26,1,7,0,27,1,2,0,165,0,2,0,28,1,2,0,29,1,
2,0,30,1,4,0,31,1,29,0,201,0,4,0,32,1,2,0,33,1,2,0,34,1,0,0,35,1,11,0,36,1,7,0,37,1,7,0,38,1,2,0,39,1,2,0,40,1,2,0,41,1,2,0,42,1,7,0,43,1,7,0,44,1,7,0,45,1,7,0,87,0,59,0,46,1,63,0,47,1,64,0,16,0,4,0,48,1,4,0,49,1,2,0,50,1,2,0,18,0,2,0,51,1,2,0,52,1,2,0,53,1,2,0,54,1,4,0,55,1,
29,0,201,0,7,0,56,1,4,0,57,1,0,0,58,1,7,0,59,1,4,0,60,1,4,0,152,0,65,0,4,0,29,0,201,0,0,0,61,1,4,0,62,1,4,0,70,0,57,0,63,0,22,0,32,0,36,0,79,0,7,0,63,1,7,0,64,1,7,0,65,1,7,0,66,1,7,0,67,1,7,0,68,1,7,0,69,1,7,0,70,1,7,0,71,1,7,0,87,0,7,0,72,1,7,0,73,1,7,0,74,1,7,0,75,1,7,0,76,1,
7,0,77,1,7,0,78,1,7,0,79,1,7,0,80,1,7,0,81,1,7,0,82,1,7,0,83,1,2,0,84,1,2,0,85,1,2,0,86,1,2,0,87,1,2,0,88,1,2,0,89,1,2,0,90,1,2,0,18,0,2,0,16,0,2,0,18,1,7,0,91,1,7,0,92,1,7,0,93,1,7,0,94,1,4,0,95,1,4,0,96,1,2,0,97,1,2,0,98,1,2,0,51,1,2,0,150,0,4,0,22,0,4,0,147,0,4,0,148,0,
4,0,149,0,7,0,99,1,7,0,100,1,7,0,72,0,44,0,101,1,66,0,102,1,33,0,84,0,51,0,14,1,59,0,46,1,60,0,103,1,26,0,177,0,62,0,104,1,64,0,105,1,65,0,106,1,0,0,107,1,0,0,122,0,67,0,13,0,7,0,108,1,7,0,109,1,7,0,211,0,4,0,18,0,0,0,204,0,0,0,205,0,0,0,206,0,0,0,207,0,4,0,16,0,7,0,110,1,7,0,111,1,7,0,112,1,29,0,48,0,
68,0,9,0,59,0,113,1,7,0,65,1,7,0,66,1,7,0,67,1,4,0,18,0,7,0,114,1,7,0,115,1,4,0,116,1,4,0,117,1,69,0,79,0,22,0,32,0,36,0,79,0,2,0,16,0,2,0,18,0,4,0,118,1,2,0,215,0,2,0,119,1,7,0,220,0,7,0,221,0,7,0,222,0,7,0,223,0,7,0,120,1,7,0,121,1,7,0,122,1,7,0,123,1,7,0,124,1,7,0,125,1,7,0,126,1,7,0,127,1,
7,0,128,1,7,0,129,1,7,0,130,1,7,0,131,1,7,0,132,1,7,0,133,1,7,0,134,1,63,0,135,1,2,0,25,1,2,0,87,0,7,0,131,0,7,0,132,0,7,0,136,1,7,0,137,1,7,0,138,1,7,0,139,1,7,0,140,1,2,0,141,1,2,0,142,1,2,0,143,1,2,0,144,1,0,0,145,1,0,0,146,1,2,0,147,1,2,0,148,1,2,0,149,1,2,0,150,1,2,0,151,1,7,0,152,1,7,0,153,1,
7,0,154,1,7,0,155,1,2,0,156,1,2,0,157,1,2,0,158,1,2,0,159,1,2,0,160,1,2,0,161,1,7,0,162,1,7,0,163,1,7,0,164,1,7,0,165,1,7,0,166,1,7,0,167,1,7,0,168,1,7,0,169,1,7,0,170,1,7,0,171,1,7,0,172,1,7,0,173,1,7,0,174,1,2,0,175,1,0,0,176,1,33,0,84,0,56,0,177,1,2,0,178,1,2,0,107,1,0,0,179,1,26,0,177,0,66,0,102,1,
70,0,18,0,7,0,180,1,7,0,181,1,7,0,182,1,7,0,183,1,7,0,184,1,7,0,185,1,7,0,186,1,7,0,187,1,7,0,188,1,7,0,189,1,2,0,190,1,2,0,191,1,2,0,192,1,2,0,193,1,7,0,194,1,7,0,195,1,7,0,196,1,7,0,197,1,71,0,4,0,4,0,18,0,4,0,198,1,4,0,199,1,4,0,72,0,72,0,4,0,51,0,14,1,0,0,200,1,4,0,201,1,4,0,70,0,73,0,140,0,
22,0,32,0,36,0,79,0,2,0,202,1,2,0,18,0,7,0,220,0,7,0,221,0,7,0,222,0,7,0,203,1,7,0,204,1,7,0,205,1,7,0,206,1,7,0,207,1,7,0,208,1,7,0,209,1,7,0,210,1,7,0,211,1,7,0,212,1,7,0,213,1,7,0,214,1,7,0,215,1,7,0,216,1,7,0,217,1,7,0,218,1,7,0,219,1,7,0,220,1,7,0,221,1,7,0,222,1,70,0,223,1,71,0,224,1,7,0,225,1,
7,0,226,1,7,0,227,1,7,0,228,1,7,0,229,1,7,0,230,1,7,0,231,1,2,0,232,1,2,0,233,1,2,0,234,1,0,0,235,1,0,0,236,1,7,0,237,1,7,0,238,1,2,0,239,1,2,0,240,1,7,0,241,1,7,0,242,1,7,0,243,1,7,0,244,1,2,0,245,1,2,0,246,1,4,0,118,1,4,0,247,1,4,0,248,1,4,0,249,1,2,0,250,1,2,0,251,1,2,0,252,1,2,0,253,1,7,0,254,1,
7,0,255,1,7,0,0,2,7,0,1,2,7,0,2,2,7,0,3,2,7,0,4,2,7,0,5,2,7,0,6,2,7,0,7,2,0,0,8,2,7,0,9,2,7,0,10,2,7,0,11,2,4,0,12,2,0,0,13,2,0,0,158,1,0,0,14,2,0,0,107,1,2,0,15,2,2,0,178,1,2,0,16,2,0,0,17,2,0,0,70,0,2,0,18,2,2,0,19,2,7,0,20,2,7,0,21,2,7,0,22,2,7,0,23,2,7,0,24,2,
2,0,197,0,2,0,198,0,59,0,25,2,59,0,26,2,0,0,27,2,0,0,28,2,0,0,29,2,0,0,30,2,2,0,31,2,2,0,32,2,7,0,33,2,7,0,34,2,56,0,177,1,66,0,102,1,33,0,84,0,74,0,35,2,26,0,177,0,7,0,36,2,7,0,37,2,7,0,38,2,7,0,39,2,7,0,40,2,2,0,41,2,2,0,87,0,7,0,42,2,7,0,43,2,7,0,44,2,7,0,45,2,7,0,46,2,7,0,47,2,
7,0,48,2,7,0,49,2,7,0,50,2,2,0,51,2,2,0,52,2,4,0,53,2,2,0,54,2,2,0,201,1,7,0,55,2,2,0,56,2,2,0,57,2,2,0,58,2,2,0,59,2,2,0,60,2,2,0,61,2,0,0,62,2,4,0,63,2,4,0,140,1,72,0,64,2,14,0,65,2,75,0,5,0,22,0,32,0,0,0,34,0,76,0,2,0,25,0,37,0,25,0,66,2,77,0,20,0,77,0,0,0,77,0,1,0,78,0,67,2,
2,0,16,0,2,0,18,0,2,0,68,2,2,0,69,2,7,0,5,0,7,0,6,0,7,0,7,0,7,0,70,2,7,0,71,2,7,0,72,2,7,0,73,2,7,0,74,2,7,0,75,2,7,0,76,2,7,0,22,0,7,0,77,2,7,0,78,2,79,0,19,0,22,0,32,0,36,0,79,0,14,0,79,2,14,0,80,2,14,0,81,2,33,0,84,0,73,0,82,2,0,0,18,0,0,0,83,2,2,0,84,2,2,0,214,0,2,0,70,0,
7,0,108,1,7,0,211,0,7,0,109,1,7,0,85,2,7,0,86,2,7,0,87,2,77,0,88,2,32,0,16,0,7,0,89,2,7,0,90,2,7,0,91,2,7,0,27,1,0,0,58,0,0,0,92,2,0,0,93,2,0,0,94,2,0,0,95,2,0,0,96,2,0,0,97,2,0,0,98,2,7,0,99,2,7,0,100,2,7,0,101,2,0,0,102,2,31,0,7,0,7,0,103,2,7,0,90,2,7,0,91,2,2,0,94,2,2,0,97,2,
7,0,27,1,7,0,70,0,80,0,22,0,80,0,0,0,80,0,1,0,2,0,16,0,2,0,104,2,2,0,97,2,2,0,18,0,4,0,105,2,4,0,106,2,2,0,47,0,2,0,107,2,2,0,108,2,2,0,109,2,2,0,110,2,2,0,111,2,2,0,112,2,7,0,113,2,7,0,114,2,31,0,52,0,32,0,53,0,2,0,115,2,2,0,116,2,4,0,117,2,81,0,5,0,2,0,118,2,2,0,104,2,0,0,18,0,0,0,70,0,
2,0,87,0,82,0,4,0,7,0,5,0,7,0,6,0,7,0,119,2,7,0,120,2,83,0,4,0,14,0,121,2,84,0,122,2,4,0,123,2,0,0,102,2,85,0,70,0,22,0,32,0,36,0,79,0,78,0,67,2,14,0,124,2,83,0,125,2,29,0,126,2,29,0,127,2,29,0,128,2,33,0,84,0,35,0,129,2,73,0,82,2,7,0,108,1,7,0,211,0,7,0,109,1,2,0,16,0,2,0,214,0,2,0,130,2,2,0,131,2,
7,0,132,2,7,0,133,2,4,0,134,2,2,0,135,2,2,0,84,2,4,0,18,0,7,0,136,2,7,0,137,2,7,0,138,2,2,0,107,2,2,0,108,2,2,0,139,2,2,0,140,2,4,0,141,2,4,0,142,2,0,0,102,2,2,0,96,0,0,0,143,2,0,0,144,2,7,0,145,2,7,0,146,2,7,0,147,2,7,0,148,2,7,0,149,2,7,0,150,2,7,0,151,2,7,0,152,2,7,0,153,2,7,0,154,2,4,0,71,0,
4,0,155,2,4,0,156,2,4,0,157,2,4,0,22,0,0,0,158,2,86,0,159,2,0,0,160,2,75,0,161,2,75,0,162,2,75,0,163,2,75,0,164,2,82,0,165,2,4,0,166,2,4,0,167,2,81,0,168,2,81,0,169,2,7,0,88,0,7,0,170,2,7,0,171,2,0,0,172,2,0,0,173,2,0,0,174,2,87,0,49,0,22,0,32,0,36,0,79,0,78,0,67,2,33,0,84,0,35,0,129,2,73,0,82,2,88,0,175,2,
89,0,176,2,90,0,177,2,91,0,178,2,92,0,179,2,93,0,180,2,94,0,181,2,95,0,182,2,96,0,183,2,97,0,184,2,98,0,185,2,99,0,186,2,100,0,187,2,87,0,188,2,101,0,189,2,102,0,190,2,102,0,191,2,102,0,192,2,102,0,193,2,102,0,194,2,4,0,57,0,4,0,195,2,4,0,196,2,4,0,197,2,4,0,198,2,4,0,199,2,4,0,200,2,7,0,108,1,7,0,211,0,7,0,109,1,4,0,130,2,
2,0,214,0,2,0,18,0,7,0,201,2,4,0,87,0,0,0,202,2,0,0,70,0,0,0,203,2,0,0,204,2,0,0,205,2,0,0,206,2,2,0,84,2,103,0,207,2,96,0,8,0,11,0,208,2,7,0,209,2,4,0,210,2,0,0,18,0,0,0,211,2,2,0,118,1,2,0,212,2,2,0,213,2,94,0,7,0,4,0,214,2,4,0,215,2,4,0,216,2,4,0,217,2,2,0,104,2,0,0,218,2,0,0,18,0,98,0,5,0,
4,0,214,2,4,0,215,2,0,0,219,2,0,0,220,2,2,0,18,0,104,0,2,0,4,0,221,2,7,0,91,2,99,0,3,0,104,0,222,2,4,0,223,2,4,0,18,0,97,0,4,0,7,0,224,2,2,0,225,2,0,0,18,0,0,0,220,2,100,0,4,0,0,0,7,1,0,0,220,0,0,0,221,0,0,0,222,0,89,0,5,0,4,0,226,2,4,0,199,2,2,0,104,2,0,0,18,0,0,0,70,0,91,0,2,0,4,0,227,2,
4,0,228,2,90,0,6,0,51,0,208,2,0,0,18,0,0,0,211,2,2,0,118,1,2,0,212,2,2,0,70,0,92,0,2,0,7,0,229,2,4,0,18,0,93,0,4,0,0,0,220,0,0,0,221,0,0,0,222,0,0,0,7,1,88,0,2,0,4,0,201,1,4,0,16,0,95,0,7,0,7,0,209,2,51,0,208,2,0,0,18,0,0,0,211,2,2,0,118,1,2,0,212,2,2,0,213,2,105,0,1,0,7,0,230,2,106,0,1,0,
4,0,231,2,107,0,2,0,0,0,232,2,0,0,233,2,108,0,1,0,7,0,209,2,109,0,1,0,7,0,229,2,110,0,4,0,4,0,234,2,4,0,235,2,7,0,236,2,4,0,237,2,111,0,4,0,7,0,7,1,7,0,220,0,7,0,221,0,7,0,222,0,112,0,1,0,111,0,210,2,113,0,5,0,4,0,238,2,4,0,239,2,0,0,18,0,0,0,104,2,0,0,47,0,114,0,2,0,4,0,240,2,4,0,239,2,115,0,10,0,
115,0,0,0,115,0,1,0,113,0,241,2,112,0,242,2,114,0,243,2,4,0,57,0,4,0,196,2,4,0,195,2,4,0,70,0,97,0,244,2,103,0,14,0,14,0,245,2,97,0,244,2,0,0,246,2,0,0,247,2,0,0,248,2,0,0,249,2,0,0,250,2,0,0,251,2,0,0,252,2,0,0,18,0,102,0,190,2,102,0,192,2,2,0,253,2,0,0,254,2,116,0,1,0,4,0,231,2,117,0,3,0,7,0,2,0,4,0,235,2,
4,0,70,0,118,0,2,0,7,0,255,2,4,0,18,0,119,0,2,0,0,0,18,0,0,0,117,1,120,0,2,0,0,0,18,0,0,0,117,1,121,0,9,0,121,0,0,0,121,0,1,0,4,0,16,0,4,0,118,1,4,0,0,3,4,0,70,0,0,0,19,0,45,0,145,0,0,0,1,3,122,0,6,0,121,0,2,3,57,0,3,3,29,0,4,3,0,0,5,3,4,0,6,3,4,0,7,3,123,0,9,0,121,0,2,3,2,0,8,3,
2,0,245,2,2,0,9,3,2,0,94,0,2,0,10,3,2,0,117,1,11,0,11,3,11,0,12,3,124,0,5,0,121,0,2,3,29,0,201,0,0,0,19,0,7,0,13,3,0,0,102,2,125,0,5,0,121,0,2,3,29,0,201,0,0,0,19,0,2,0,14,3,0,0,15,3,126,0,6,0,121,0,2,3,7,0,16,3,7,0,17,3,2,0,18,0,2,0,18,3,4,0,19,3,127,0,5,0,121,0,2,3,29,0,20,3,0,0,76,0,
4,0,118,1,4,0,18,0,128,0,13,0,121,0,2,3,29,0,21,3,29,0,22,3,29,0,23,3,29,0,24,3,7,0,25,3,7,0,26,3,7,0,17,3,7,0,27,3,4,0,28,3,4,0,29,3,4,0,94,0,4,0,30,3,129,0,6,0,121,0,2,3,2,0,31,3,2,0,18,0,7,0,32,3,7,0,33,3,29,0,34,3,130,0,3,0,121,0,2,3,7,0,35,3,4,0,94,0,131,0,13,0,121,0,2,3,7,0,36,3,
4,0,37,3,2,0,94,0,2,0,38,3,2,0,39,3,2,0,40,3,2,0,41,3,2,0,70,0,4,0,87,0,7,0,42,3,7,0,43,3,0,0,44,3,132,0,6,0,121,0,2,3,133,0,45,3,134,0,46,3,135,0,47,3,7,0,48,3,4,0,16,0,136,0,11,0,121,0,2,3,57,0,3,3,29,0,4,3,0,0,5,3,4,0,6,3,4,0,7,3,7,0,13,3,4,0,49,3,0,0,44,3,7,0,50,3,4,0,51,3,
137,0,12,0,121,0,2,3,29,0,52,3,51,0,53,3,4,0,94,0,4,0,54,3,7,0,55,3,7,0,56,3,7,0,57,3,7,0,58,3,0,0,5,3,4,0,6,3,4,0,70,0,138,0,11,0,121,0,2,3,7,0,59,3,2,0,60,3,0,0,61,3,0,0,62,3,7,0,63,3,0,0,44,3,7,0,64,3,2,0,18,0,2,0,118,1,4,0,65,3,139,0,5,0,121,0,2,3,7,0,66,3,0,0,44,3,2,0,18,0,
2,0,67,3,140,0,8,0,121,0,2,3,29,0,201,0,7,0,66,3,7,0,27,1,7,0,106,0,0,0,44,3,2,0,18,0,2,0,16,0,141,0,21,0,121,0,2,3,57,0,3,3,29,0,4,3,0,0,5,3,4,0,6,3,4,0,7,3,29,0,68,3,0,0,44,3,2,0,18,0,2,0,70,0,7,0,69,3,7,0,70,3,7,0,71,3,7,0,136,2,7,0,72,3,7,0,73,3,7,0,74,3,7,0,75,3,7,0,76,3,
7,0,77,3,7,0,72,0,142,0,7,0,121,0,2,3,2,0,78,3,2,0,79,3,4,0,87,0,29,0,201,0,7,0,80,3,0,0,44,3,143,0,14,0,121,0,2,3,29,0,201,0,0,0,81,3,0,0,18,0,0,0,25,1,0,0,15,3,7,0,82,3,7,0,83,3,7,0,75,3,63,0,135,1,4,0,84,3,4,0,85,3,7,0,86,3,0,0,19,0,144,0,1,0,121,0,2,3,145,0,13,0,121,0,2,3,45,0,145,0,
146,0,87,3,147,0,88,3,148,0,89,3,149,0,90,3,14,0,91,3,150,0,92,3,7,0,93,3,7,0,94,3,4,0,95,3,7,0,96,3,151,0,97,3,152,0,15,0,121,0,2,3,97,0,98,3,97,0,99,3,97,0,100,3,97,0,101,3,97,0,102,3,97,0,103,3,153,0,104,3,4,0,105,3,4,0,106,3,7,0,107,3,7,0,108,3,0,0,109,3,0,0,122,0,154,0,110,3,155,0,7,0,121,0,2,3,97,0,98,3,
97,0,111,3,156,0,112,3,157,0,110,3,4,0,113,3,4,0,114,3,158,0,6,0,121,0,2,3,29,0,201,0,0,0,115,3,0,0,116,3,0,0,47,0,7,0,117,3,159,0,2,0,4,0,118,3,7,0,91,2,160,0,2,0,4,0,148,0,4,0,119,3,161,0,22,0,121,0,2,3,29,0,201,0,0,0,44,3,2,0,120,3,2,0,18,0,2,0,47,0,159,0,121,3,4,0,122,3,7,0,123,3,4,0,57,0,4,0,124,3,
160,0,125,3,159,0,126,3,4,0,127,3,4,0,128,3,4,0,119,3,7,0,129,3,7,0,130,3,7,0,131,3,7,0,132,3,7,0,133,3,11,0,134,3,162,0,9,0,121,0,2,3,163,0,135,3,156,0,136,3,156,0,137,3,4,0,138,3,4,0,139,3,4,0,140,3,2,0,18,0,2,0,70,0,164,0,8,0,121,0,2,3,29,0,48,0,2,0,32,1,2,0,18,0,2,0,31,3,2,0,70,0,7,0,141,3,7,0,142,3,
165,0,6,0,121,0,2,3,4,0,143,3,2,0,18,0,2,0,144,3,7,0,145,3,0,0,203,0,166,0,8,0,121,0,2,3,0,0,146,3,0,0,147,3,0,0,251,2,0,0,148,3,0,0,149,3,0,0,94,0,0,0,47,0,167,0,3,0,121,0,2,3,168,0,150,3,149,0,90,3,169,0,12,0,121,0,2,3,29,0,151,3,29,0,152,3,0,0,153,3,7,0,154,3,2,0,155,3,0,0,156,3,0,0,72,0,7,0,157,3,
0,0,158,3,0,0,159,3,0,0,47,0,170,0,9,0,121,0,2,3,29,0,160,3,0,0,153,3,7,0,114,0,7,0,161,3,0,0,118,1,0,0,31,3,0,0,18,0,0,0,70,0,171,0,1,0,121,0,2,3,172,0,13,0,121,0,2,3,0,0,44,3,7,0,148,0,7,0,162,3,7,0,163,3,7,0,164,3,7,0,70,0,7,0,165,3,7,0,166,3,7,0,167,3,4,0,18,0,2,0,168,3,2,0,169,3,173,0,11,0,
121,0,2,3,29,0,170,3,4,0,171,3,4,0,172,3,4,0,60,3,7,0,173,3,7,0,63,3,7,0,27,3,2,0,18,0,0,0,31,3,0,0,191,0,174,0,29,0,121,0,2,3,175,0,174,3,176,0,175,3,4,0,176,3,4,0,177,3,7,0,178,3,7,0,74,3,7,0,179,3,7,0,22,1,7,0,180,3,7,0,181,3,7,0,182,3,7,0,183,3,7,0,184,3,7,0,48,3,4,0,185,3,4,0,186,3,0,0,187,3,
0,0,188,3,0,0,189,3,0,0,190,3,0,0,18,0,0,0,191,3,2,0,192,3,2,0,193,3,4,0,19,3,7,0,106,0,7,0,194,3,4,0,70,0,177,0,15,0,121,0,2,3,57,0,3,3,29,0,4,3,0,0,5,3,4,0,6,3,4,0,7,3,29,0,195,3,29,0,196,3,63,0,135,1,0,0,44,3,7,0,13,3,7,0,197,3,0,0,18,0,0,0,25,1,0,0,15,3,178,0,16,0,121,0,2,3,0,0,44,3,
2,0,198,3,2,0,25,1,7,0,199,3,63,0,200,3,7,0,201,3,7,0,202,3,7,0,203,3,0,0,204,3,4,0,205,3,57,0,206,3,29,0,207,3,4,0,208,3,0,0,209,3,4,0,210,3,179,0,16,0,121,0,2,3,0,0,211,3,0,0,212,3,7,0,213,3,7,0,214,3,0,0,215,3,0,0,216,3,0,0,217,3,7,0,203,3,0,0,204,3,4,0,205,3,57,0,206,3,29,0,207,3,4,0,208,3,0,0,209,3,
4,0,210,3,180,0,16,0,121,0,2,3,0,0,44,3,4,0,218,3,4,0,219,3,29,0,220,3,7,0,203,3,0,0,204,3,4,0,205,3,57,0,206,3,29,0,207,3,4,0,208,3,0,0,209,3,7,0,221,3,7,0,222,3,2,0,25,1,2,0,28,0,181,0,5,0,121,0,2,3,182,0,223,3,183,0,224,3,4,0,16,0,4,0,70,0,184,0,8,0,121,0,2,3,7,0,225,3,7,0,226,3,7,0,227,3,0,0,22,1,
0,0,18,0,0,0,118,1,0,0,70,0,185,0,5,0,121,0,2,3,7,0,228,3,0,0,18,0,0,0,229,3,0,0,47,0,186,0,5,0,121,0,2,3,4,0,18,0,4,0,230,3,4,0,231,3,4,0,70,0,187,0,7,0,121,0,2,3,7,0,232,3,7,0,233,3,7,0,72,0,0,0,44,3,2,0,18,0,2,0,67,3,188,0,13,0,121,0,2,3,7,0,234,3,4,0,235,3,7,0,232,3,2,0,67,3,2,0,18,0,
0,0,236,3,0,0,237,3,0,0,47,0,0,0,44,3,7,0,238,3,4,0,239,3,0,0,240,3,189,0,11,0,121,0,2,3,0,0,241,3,0,0,242,3,0,0,15,3,7,0,243,3,29,0,244,3,0,0,245,3,29,0,246,3,0,0,247,3,0,0,153,3,0,0,5,3,190,0,18,0,121,0,2,3,0,0,18,0,0,0,16,0,0,0,248,3,0,0,249,3,0,0,250,3,0,0,251,3,0,0,252,3,0,0,253,3,7,0,114,0,
0,0,254,3,0,0,122,0,7,0,255,3,7,0,0,4,7,0,1,4,7,0,2,4,7,0,3,4,0,0,35,0,191,0,8,0,121,0,2,3,0,0,4,4,4,0,5,4,4,0,67,3,7,0,6,4,11,0,7,4,2,0,18,0,2,0,117,1,192,0,9,0,121,0,2,3,0,0,44,3,7,0,148,0,7,0,162,3,7,0,163,3,7,0,8,4,2,0,18,0,2,0,168,3,2,0,47,0,193,0,17,0,121,0,2,3,29,0,9,4,
4,0,10,4,4,0,11,4,4,0,12,4,4,0,13,4,4,0,14,4,7,0,15,4,7,0,16,4,7,0,17,4,4,0,210,3,4,0,18,4,4,0,19,4,4,0,215,3,7,0,20,4,0,0,44,3,4,0,94,0,194,0,11,0,121,0,2,3,0,0,44,3,29,0,151,3,2,0,118,1,2,0,18,0,2,0,215,3,0,0,47,0,7,0,20,4,7,0,21,4,7,0,25,3,7,0,22,4,195,0,6,0,121,0,2,3,196,0,23,4,
197,0,24,4,0,0,25,4,0,0,26,4,0,0,122,0,198,0,6,0,4,0,27,4,4,0,114,3,4,0,118,1,7,0,28,4,7,0,29,4,7,0,30,4,199,0,3,0,198,0,31,4,4,0,32,4,0,0,102,2,200,0,8,0,121,0,2,3,29,0,151,3,199,0,244,2,7,0,75,3,4,0,114,3,4,0,33,4,4,0,94,0,7,0,110,1,201,0,3,0,202,0,34,4,4,0,123,2,0,0,102,2,202,0,27,0,22,0,32,0,
36,0,79,0,2,0,105,2,2,0,106,2,2,0,35,4,2,0,18,0,2,0,36,4,2,0,37,4,2,0,38,4,2,0,87,0,0,0,39,4,0,0,40,4,0,0,41,4,0,0,32,2,4,0,42,4,7,0,43,4,7,0,44,4,7,0,45,4,7,0,46,4,7,0,47,4,7,0,48,4,31,0,49,4,33,0,84,0,35,0,129,2,99,0,186,2,0,0,76,0,201,0,50,4,203,0,5,0,203,0,0,0,203,0,1,0,0,0,19,0,
0,0,18,0,0,0,122,0,78,0,3,0,7,0,51,4,4,0,18,0,4,0,70,0,204,0,7,0,204,0,0,0,204,0,1,0,29,0,52,4,4,0,94,0,7,0,53,4,7,0,70,0,4,0,54,4,29,0,139,0,22,0,32,0,36,0,79,0,205,0,55,4,2,0,16,0,2,0,56,4,4,0,57,4,4,0,58,4,4,0,59,4,0,0,60,4,29,0,36,0,29,0,61,4,29,0,62,4,29,0,63,4,29,0,64,4,33,0,84,0,
78,0,67,2,206,0,65,4,206,0,66,4,207,0,67,4,11,0,2,0,208,0,68,4,209,0,69,4,210,0,70,4,14,0,71,4,14,0,72,4,14,0,73,4,14,0,74,4,4,0,118,1,4,0,75,4,73,0,82,2,0,0,76,4,4,0,84,2,4,0,77,4,7,0,108,1,7,0,78,4,7,0,79,4,7,0,211,0,7,0,80,4,7,0,81,4,7,0,109,1,7,0,82,4,7,0,70,2,7,0,83,4,7,0,84,4,7,0,85,4,
7,0,86,4,7,0,87,4,7,0,88,4,7,0,82,3,7,0,89,4,7,0,16,1,7,0,90,4,4,0,91,4,2,0,18,0,2,0,92,4,2,0,93,4,2,0,94,4,2,0,95,4,2,0,96,4,2,0,97,4,2,0,98,4,0,0,99,4,0,0,100,4,0,0,101,4,0,0,70,0,4,0,102,4,4,0,103,4,4,0,104,4,4,0,105,4,7,0,106,4,7,0,107,4,7,0,108,4,7,0,109,4,7,0,110,4,7,0,111,4,
7,0,112,4,7,0,113,4,7,0,114,4,7,0,115,4,7,0,116,4,7,0,117,4,7,0,118,4,7,0,119,4,0,0,120,4,0,0,121,4,2,0,122,4,2,0,123,4,2,0,124,4,0,0,125,4,0,0,126,4,2,0,129,0,0,0,127,4,0,0,128,4,7,0,129,4,7,0,130,4,14,0,131,4,14,0,132,4,14,0,133,4,14,0,134,4,7,0,135,4,2,0,201,1,2,0,136,4,7,0,210,2,4,0,137,4,4,0,138,4,
211,0,139,4,0,0,140,4,0,0,23,1,2,0,141,4,7,0,142,4,14,0,143,4,14,0,144,4,14,0,145,4,14,0,146,4,212,0,104,1,213,0,147,4,74,0,148,4,0,0,149,4,0,0,150,4,2,0,123,2,7,0,201,2,168,0,151,4,214,0,152,4,156,0,153,4,156,0,154,4,10,0,155,4,10,0,156,4,4,0,157,4,4,0,158,4,14,0,159,4,14,0,160,4,14,0,161,4,215,0,162,4,216,0,163,4,7,0,164,4,
44,0,165,4,14,0,166,4,204,0,167,4,26,0,177,0,217,0,14,0,217,0,0,0,217,0,1,0,29,0,36,0,7,0,82,3,7,0,110,1,7,0,83,3,7,0,75,3,0,0,19,0,4,0,84,3,4,0,85,3,4,0,168,4,2,0,16,0,2,0,169,4,7,0,86,3,218,0,13,0,218,0,0,0,218,0,1,0,29,0,48,0,7,0,110,1,7,0,170,4,7,0,229,2,2,0,16,0,0,0,171,4,0,0,172,4,4,0,173,4,
163,0,174,4,4,0,175,4,4,0,70,0,212,0,41,0,4,0,18,0,2,0,176,4,2,0,177,4,2,0,75,3,2,0,178,4,2,0,179,4,2,0,180,4,2,0,181,4,2,0,182,4,7,0,183,4,7,0,184,4,7,0,185,4,7,0,186,4,7,0,187,4,7,0,188,4,7,0,189,4,7,0,190,4,7,0,191,4,7,0,192,4,7,0,193,4,7,0,194,4,7,0,195,4,7,0,196,4,7,0,197,4,7,0,198,4,7,0,199,4,
7,0,200,4,7,0,201,4,7,0,202,4,7,0,203,4,7,0,204,4,7,0,205,4,7,0,206,4,7,0,207,4,7,0,208,4,7,0,209,4,57,0,202,0,219,0,210,4,7,0,211,4,4,0,19,3,29,0,212,4,220,0,6,0,74,0,35,2,7,0,213,4,7,0,214,4,2,0,18,0,2,0,215,4,4,0,70,0,221,0,5,0,221,0,0,0,221,0,1,0,4,0,16,0,4,0,216,4,11,0,2,0,222,0,9,0,222,0,0,0,
222,0,1,0,4,0,217,4,4,0,218,4,4,0,10,4,4,0,18,0,11,0,219,4,11,0,220,4,14,0,221,4,149,0,23,0,149,0,0,0,149,0,1,0,4,0,18,0,4,0,222,4,4,0,223,4,4,0,224,4,4,0,225,4,4,0,226,4,4,0,227,4,4,0,228,4,4,0,70,0,4,0,218,4,4,0,201,1,2,0,229,4,2,0,60,0,0,0,19,0,0,0,230,4,0,0,231,4,0,0,232,4,0,0,233,4,14,0,234,4,
223,0,235,4,11,0,236,4,224,0,1,0,7,0,103,2,211,0,30,0,4,0,18,0,7,0,237,4,7,0,238,4,7,0,239,4,4,0,240,4,4,0,241,4,4,0,242,4,4,0,243,4,7,0,244,4,7,0,245,4,7,0,246,4,7,0,247,4,7,0,248,4,7,0,249,4,7,0,250,4,7,0,251,4,7,0,252,4,7,0,253,4,7,0,254,4,7,0,255,4,7,0,0,5,7,0,1,5,7,0,2,5,7,0,3,5,7,0,4,5,
7,0,5,5,4,0,6,5,4,0,7,5,7,0,8,5,7,0,111,4,213,0,55,0,4,0,218,4,4,0,9,5,225,0,10,5,226,0,11,5,0,0,70,0,0,0,12,5,2,0,13,5,7,0,14,5,0,0,15,5,7,0,16,5,7,0,17,5,7,0,18,5,7,0,19,5,7,0,20,5,7,0,21,5,7,0,22,5,7,0,23,5,7,0,24,5,2,0,25,5,0,0,26,5,2,0,27,5,7,0,28,5,7,0,29,5,0,0,30,5,
4,0,149,0,4,0,31,5,4,0,32,5,2,0,33,5,2,0,34,5,224,0,35,5,4,0,36,5,4,0,86,0,7,0,37,5,7,0,38,5,7,0,39,5,7,0,40,5,2,0,41,5,2,0,42,5,2,0,43,5,2,0,44,5,2,0,45,5,2,0,46,5,2,0,47,5,2,0,48,5,227,0,49,5,7,0,50,5,7,0,51,5,149,0,52,5,14,0,91,3,74,0,53,5,220,0,54,5,7,0,55,5,7,0,56,5,7,0,57,5,
4,0,58,5,228,0,1,0,7,0,59,5,168,0,52,0,167,0,60,5,4,0,61,5,4,0,72,0,2,0,16,0,2,0,62,5,2,0,63,5,2,0,64,5,7,0,65,5,2,0,66,5,2,0,67,5,7,0,68,5,2,0,69,5,2,0,70,5,7,0,71,5,7,0,72,5,7,0,73,5,4,0,74,5,4,0,75,5,4,0,76,5,4,0,87,0,7,0,77,5,4,0,78,5,7,0,79,5,7,0,80,5,7,0,81,5,0,0,82,5,
7,0,83,5,7,0,84,5,33,0,84,0,2,0,85,5,0,0,86,5,0,0,87,5,7,0,88,5,4,0,89,5,7,0,90,5,7,0,91,5,4,0,92,5,4,0,18,0,7,0,93,5,7,0,94,5,7,0,95,5,228,0,96,5,4,0,57,0,7,0,97,5,7,0,98,5,7,0,99,5,7,0,100,5,7,0,101,5,7,0,102,5,7,0,103,5,4,0,104,5,7,0,105,5,229,0,77,0,22,0,32,0,36,0,79,0,2,0,215,0,
2,0,119,1,2,0,158,1,2,0,106,5,7,0,107,5,7,0,108,5,7,0,109,5,7,0,110,5,7,0,111,5,7,0,112,5,7,0,209,1,7,0,211,1,7,0,210,1,7,0,113,5,7,0,114,5,7,0,115,5,7,0,116,5,7,0,117,5,7,0,118,5,7,0,119,5,2,0,120,5,2,0,118,1,2,0,121,5,2,0,122,5,2,0,123,5,2,0,124,5,2,0,125,5,2,0,126,5,7,0,127,5,7,0,128,5,7,0,129,5,
7,0,130,5,7,0,131,5,7,0,132,5,7,0,133,5,7,0,134,5,7,0,135,5,7,0,136,5,7,0,137,5,7,0,138,5,2,0,139,5,2,0,140,5,2,0,141,5,2,0,142,5,7,0,143,5,7,0,144,5,7,0,145,5,7,0,146,5,2,0,147,5,2,0,148,5,2,0,149,5,2,0,150,5,7,0,151,5,7,0,152,5,7,0,153,5,7,0,154,5,7,0,155,5,7,0,156,5,7,0,157,5,2,0,158,5,2,0,159,5,
2,0,160,5,2,0,161,5,2,0,162,5,2,0,18,0,7,0,163,5,7,0,164,5,33,0,84,0,56,0,177,1,2,0,178,1,2,0,107,1,2,0,47,0,26,0,177,0,66,0,102,1,14,0,65,2,230,0,8,0,230,0,0,0,230,0,1,0,4,0,91,4,4,0,165,5,4,0,18,0,2,0,166,5,2,0,167,5,29,0,201,0,231,0,13,0,11,0,168,5,11,0,169,5,4,0,170,5,4,0,171,5,4,0,172,5,4,0,173,5,
4,0,174,5,4,0,175,5,4,0,176,5,4,0,177,5,4,0,178,5,4,0,70,0,0,0,179,5,232,0,5,0,11,0,180,5,11,0,181,5,4,0,182,5,4,0,87,0,0,0,183,5,233,0,17,0,4,0,184,5,4,0,185,5,4,0,186,5,4,0,187,5,4,0,188,5,4,0,189,5,4,0,190,5,4,0,191,5,4,0,192,5,4,0,193,5,4,0,194,5,4,0,195,5,2,0,196,5,2,0,197,5,4,0,198,5,4,0,199,5,
4,0,72,0,234,0,21,0,4,0,16,0,4,0,186,5,4,0,200,5,4,0,201,5,4,0,202,5,4,0,203,5,4,0,204,5,4,0,205,5,7,0,206,5,4,0,207,5,4,0,208,5,4,0,94,0,4,0,209,5,4,0,210,5,4,0,211,5,4,0,212,5,4,0,213,5,4,0,214,5,4,0,215,5,4,0,72,0,21,0,31,0,235,0,9,0,4,0,216,5,7,0,217,5,7,0,218,5,7,0,219,5,4,0,220,5,2,0,18,0,
2,0,70,0,7,0,239,4,7,0,87,0,236,0,15,0,236,0,0,0,236,0,1,0,0,0,19,0,73,0,221,5,74,0,222,5,4,0,91,4,4,0,223,5,4,0,224,5,4,0,225,5,4,0,226,5,4,0,227,5,4,0,118,0,7,0,228,5,21,0,229,5,237,0,230,5,238,0,7,0,238,0,0,0,238,0,1,0,0,0,19,0,0,0,231,5,4,0,232,5,4,0,47,0,0,0,240,3,55,0,5,0,2,0,18,0,0,0,233,5,
0,0,234,5,0,0,235,5,0,0,117,1,239,0,19,0,0,0,236,5,0,0,22,1,0,0,237,5,0,0,18,0,0,0,238,5,0,0,239,5,0,0,240,5,0,0,241,5,2,0,242,5,2,0,243,5,7,0,244,5,0,0,245,5,0,0,246,5,0,0,247,5,0,0,102,2,0,0,193,0,55,0,248,5,240,0,249,5,241,0,250,5,242,0,13,0,239,0,251,5,0,0,35,0,2,0,136,2,2,0,71,3,2,0,111,4,2,0,18,0,
7,0,252,5,4,0,253,5,0,0,254,5,0,0,255,5,0,0,0,6,0,0,117,1,0,0,1,6,243,0,128,0,239,0,251,5,231,0,2,6,232,0,3,6,233,0,4,6,234,0,5,6,4,0,113,3,4,0,149,0,4,0,31,5,7,0,6,6,4,0,7,6,4,0,8,6,4,0,9,6,4,0,10,6,2,0,18,0,2,0,61,5,7,0,11,6,7,0,12,6,7,0,13,6,7,0,14,6,7,0,15,6,2,0,16,6,2,0,17,6,
2,0,18,6,2,0,19,6,2,0,22,1,2,0,20,6,4,0,21,6,2,0,22,6,2,0,23,6,2,0,144,1,2,0,106,0,2,0,24,6,2,0,25,6,4,0,26,6,4,0,27,6,2,0,28,6,2,0,29,6,4,0,30,6,4,0,31,6,2,0,237,5,2,0,236,5,2,0,32,6,2,0,238,5,2,0,33,6,0,0,34,6,0,0,35,6,4,0,36,6,4,0,118,1,4,0,37,6,2,0,38,6,2,0,72,0,2,0,39,6,
2,0,40,6,2,0,41,6,2,0,42,6,2,0,43,6,2,0,44,6,19,0,45,6,19,0,46,6,18,0,47,6,14,0,48,6,2,0,49,6,2,0,50,6,7,0,51,6,7,0,52,6,7,0,53,6,7,0,54,6,4,0,55,6,7,0,56,6,7,0,57,6,7,0,58,6,7,0,59,6,2,0,60,6,2,0,61,6,2,0,62,6,2,0,63,6,2,0,64,6,2,0,65,6,7,0,66,6,7,0,67,6,2,0,68,6,2,0,69,6,
7,0,70,6,7,0,71,6,0,0,72,6,4,0,73,6,2,0,74,6,2,0,32,2,0,0,75,6,7,0,76,6,7,0,77,6,0,0,78,6,0,0,79,6,0,0,80,6,0,0,81,6,4,0,82,6,2,0,83,6,2,0,84,6,2,0,85,6,2,0,86,6,7,0,87,6,7,0,88,6,7,0,89,6,2,0,90,6,2,0,91,6,7,0,92,6,2,0,93,6,2,0,94,6,4,0,95,6,2,0,96,6,2,0,97,6,2,0,98,6,
2,0,99,6,7,0,100,6,7,0,87,0,38,0,101,6,4,0,102,6,7,0,103,6,0,0,104,6,242,0,105,6,4,0,106,6,2,0,107,6,2,0,70,0,14,0,194,0,2,0,108,6,2,0,193,0,2,0,109,6,63,0,110,6,244,0,9,0,244,0,0,0,244,0,1,0,0,0,111,6,2,0,112,6,2,0,113,6,2,0,114,6,2,0,72,0,7,0,115,6,7,0,87,0,245,0,7,0,2,0,37,3,2,0,118,1,2,0,63,3,
2,0,116,6,7,0,117,6,7,0,87,0,38,0,118,6,246,0,5,0,7,0,119,6,0,0,16,0,0,0,72,0,0,0,87,0,0,0,32,2,247,0,16,0,7,0,120,6,7,0,121,6,7,0,122,6,7,0,123,6,7,0,124,6,7,0,125,6,7,0,126,6,7,0,127,6,7,0,128,6,7,0,129,6,4,0,130,6,7,0,131,6,7,0,132,6,0,0,133,6,0,0,72,0,2,0,87,0,248,0,39,0,246,0,134,6,2,0,135,6,
2,0,17,6,2,0,18,6,2,0,19,6,2,0,22,1,2,0,20,6,2,0,136,6,2,0,137,6,2,0,138,6,2,0,61,2,245,0,139,6,2,0,140,6,2,0,22,6,7,0,141,6,247,0,142,6,7,0,118,5,7,0,119,5,4,0,18,0,2,0,118,1,2,0,143,6,2,0,121,5,2,0,122,5,2,0,144,6,2,0,145,6,2,0,123,5,2,0,124,5,2,0,125,5,2,0,126,5,2,0,146,6,2,0,147,6,7,0,148,6,
7,0,149,6,7,0,150,6,7,0,151,6,2,0,152,6,2,0,87,0,4,0,153,6,4,0,140,1,249,0,6,0,249,0,0,0,249,0,1,0,4,0,217,4,0,0,19,0,4,0,18,0,29,0,154,6,250,0,10,0,251,0,224,3,252,0,155,6,63,0,156,6,11,0,157,6,0,0,158,6,4,0,94,0,4,0,159,6,4,0,160,6,7,0,161,6,4,0,87,0,253,0,13,0,250,0,162,6,2,0,18,0,2,0,163,6,2,0,164,6,
2,0,165,6,2,0,166,6,4,0,118,1,11,0,167,6,51,0,168,6,51,0,169,6,51,0,223,3,7,0,170,6,7,0,171,6,254,0,6,0,2,0,106,0,2,0,222,4,2,0,172,6,2,0,30,3,4,0,18,0,7,0,13,3,255,0,15,0,2,0,18,0,2,0,173,6,2,0,174,6,2,0,175,6,254,0,176,6,11,0,167,6,7,0,177,6,7,0,60,0,4,0,178,6,4,0,179,6,4,0,180,6,4,0,181,6,45,0,145,0,
29,0,201,0,29,0,182,6,0,1,10,0,250,0,162,6,4,0,94,0,4,0,183,6,7,0,184,6,4,0,185,6,7,0,186,6,7,0,187,6,7,0,188,6,7,0,70,0,29,0,189,6,1,1,1,0,250,0,162,6,2,1,7,0,250,0,162,6,2,0,18,0,2,0,70,0,4,0,8,1,4,0,190,6,99,0,191,6,11,0,167,6,3,1,3,0,2,0,106,0,2,0,18,0,7,0,13,3,4,1,6,0,3,1,192,6,11,0,167,6,
4,0,175,6,4,0,18,0,4,0,193,6,7,0,217,1,5,1,7,0,2,0,18,0,0,0,16,0,0,0,98,2,7,0,99,2,7,0,100,2,7,0,101,2,63,0,194,6,6,1,5,0,6,1,0,0,6,1,1,0,0,0,19,0,7,0,195,6,4,0,70,0,7,1,28,0,4,0,106,0,7,0,196,6,7,0,217,1,7,0,91,2,7,0,197,6,7,0,198,6,4,0,18,0,7,0,199,6,7,0,200,6,4,0,201,6,7,0,202,6,
4,0,203,6,7,0,204,6,7,0,205,6,4,0,206,6,7,0,207,6,0,0,208,6,0,0,209,6,0,0,210,6,0,0,211,6,7,0,212,6,4,0,213,6,7,0,214,6,7,0,215,6,7,0,216,6,7,0,217,6,7,0,218,6,8,1,219,6,9,1,13,0,0,0,220,6,0,0,18,0,0,0,221,6,0,0,222,6,0,0,223,6,0,0,70,0,2,0,224,6,7,0,225,6,7,0,226,6,7,0,227,6,7,0,228,6,7,0,229,6,
7,0,230,6,10,1,13,0,0,0,16,0,0,0,231,6,0,0,232,6,7,0,233,6,7,0,234,6,7,0,235,6,7,0,236,6,0,0,237,6,0,0,238,6,7,0,239,6,7,0,240,6,7,0,241,6,7,0,242,6,11,1,87,0,2,1,243,6,2,1,244,6,0,1,55,4,1,1,245,6,7,0,246,6,7,0,247,6,7,0,248,6,2,0,249,6,2,0,178,6,0,0,250,6,0,0,251,6,0,0,252,6,0,0,253,6,7,0,254,6,
2,0,255,6,0,0,0,7,0,0,1,7,0,0,2,7,0,0,3,7,0,0,4,7,0,0,5,7,4,1,6,7,5,1,7,7,14,0,8,7,253,0,9,7,255,0,10,7,7,0,11,7,7,0,12,7,2,0,13,7,2,0,14,7,0,0,15,7,0,0,16,7,2,0,17,7,7,0,18,7,7,0,19,7,7,0,20,7,7,0,21,7,7,0,22,7,7,0,23,7,7,0,24,7,7,0,25,7,7,0,26,7,7,0,27,7,2,0,28,7,
0,0,29,7,0,0,30,7,0,0,31,7,0,0,32,7,29,0,33,7,0,0,34,7,0,0,35,7,0,0,36,7,0,0,37,7,0,0,38,7,0,0,39,7,0,0,40,7,0,0,41,7,0,0,42,7,0,0,43,7,0,0,44,7,0,0,45,7,2,0,46,7,2,0,47,7,2,0,48,7,2,0,49,7,0,0,50,7,0,0,51,7,0,0,52,7,0,0,53,7,0,0,54,7,0,0,55,7,0,0,56,7,0,0,57,7,0,0,58,7,
0,0,59,7,4,0,60,7,4,0,61,7,4,0,62,7,4,0,63,7,2,0,64,7,2,0,140,1,4,0,65,7,7,0,66,7,7,0,67,7,7,1,68,7,9,1,69,7,10,1,70,7,12,1,8,0,4,0,71,7,4,0,72,7,4,0,73,7,4,0,74,7,4,0,75,7,4,0,76,7,4,0,57,0,4,0,196,2,13,1,4,0,7,0,77,7,0,0,78,7,0,0,79,7,2,0,18,0,14,1,4,0,7,0,80,7,4,0,18,0,
4,0,81,7,4,0,60,0,15,1,4,0,7,0,82,7,7,0,83,7,7,0,84,7,7,0,85,7,45,0,52,0,22,0,32,0,36,0,79,0,29,0,154,6,229,0,86,7,45,0,87,7,14,0,88,7,230,0,89,7,29,0,90,7,7,0,91,7,7,0,92,7,7,0,93,7,7,0,94,7,4,0,91,4,4,0,95,7,4,0,96,7,2,0,18,0,0,0,107,1,0,0,12,1,66,0,102,1,16,1,97,7,11,1,98,7,17,1,99,7,
15,1,100,7,243,0,220,0,235,0,101,7,14,0,102,7,14,0,103,7,11,0,104,7,11,0,105,7,11,0,106,7,11,0,107,7,11,0,108,7,18,1,109,7,11,0,110,7,19,1,111,7,2,0,112,7,2,0,32,2,4,0,113,7,14,0,114,7,246,0,134,6,248,0,115,7,13,1,116,7,208,0,68,4,14,1,117,7,20,1,118,7,10,0,156,4,10,0,119,7,240,0,249,5,241,0,250,5,54,0,120,7,21,1,121,7,26,0,177,0,
22,1,15,0,22,1,0,0,22,1,1,0,51,0,14,1,44,0,101,1,20,1,118,7,23,1,122,7,7,0,152,2,7,0,153,2,7,0,106,0,7,0,123,7,7,0,124,7,2,0,156,0,2,0,18,0,2,0,165,0,0,0,15,3,24,1,42,0,7,0,125,7,7,0,126,7,7,0,127,7,7,0,128,7,7,0,129,7,7,0,130,7,7,0,131,7,7,0,132,7,7,0,133,7,7,0,134,7,78,0,135,7,24,1,136,7,25,1,137,7,
26,1,138,7,11,0,139,7,27,1,140,7,28,1,141,7,7,0,142,7,7,0,143,7,7,0,125,1,7,0,144,7,7,0,145,7,7,0,146,7,7,0,210,0,7,0,147,7,0,0,148,7,0,0,149,7,0,0,156,0,0,0,150,7,0,0,151,7,0,0,117,1,7,0,152,7,2,0,153,7,2,0,154,7,7,0,155,7,2,0,156,7,2,0,157,7,7,0,158,7,7,0,159,7,7,0,160,7,7,0,161,7,29,1,162,7,30,1,65,0,
31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,7,0,143,7,7,0,125,1,7,0,167,7,0,0,168,7,0,0,117,1,4,0,169,7,4,0,170,7,2,0,149,7,2,0,156,0,29,0,154,6,29,0,171,7,19,0,172,7,14,0,173,7,22,1,174,7,30,1,136,7,0,0,175,7,4,0,91,4,4,0,95,7,2,0,176,7,2,0,177,7,2,0,178,7,2,0,179,7,2,0,18,0,2,0,83,2,
7,0,133,0,7,0,180,7,7,0,181,7,7,0,182,7,7,0,210,0,7,0,91,7,2,0,183,7,2,0,184,7,2,0,185,7,0,0,186,7,0,0,187,7,0,0,188,7,0,0,189,7,2,0,190,7,14,0,191,7,14,0,192,7,14,0,193,7,0,0,194,7,0,0,211,2,0,0,195,7,0,0,153,0,0,0,196,7,41,0,197,7,11,0,198,7,73,0,199,7,208,0,68,4,2,0,200,7,0,0,201,7,0,0,40,6,7,0,202,7,
7,0,203,7,7,0,204,7,2,0,205,7,2,0,72,0,7,0,87,0,32,1,26,0,19,0,8,1,19,0,67,0,18,0,206,7,18,0,207,7,18,0,208,7,7,0,209,7,7,0,210,7,7,0,211,7,7,0,212,7,2,0,213,7,2,0,214,7,2,0,215,7,2,0,216,7,2,0,217,7,2,0,18,0,2,0,218,7,2,0,219,7,2,0,220,7,2,0,221,7,2,0,222,7,2,0,179,7,7,0,223,7,4,0,224,7,4,0,225,7,
33,1,140,7,28,1,141,7,31,1,6,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,34,1,8,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,0,0,226,7,0,0,122,0,35,1,22,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,32,1,227,7,2,0,228,7,2,0,229,7,2,0,230,7,2,0,231,7,
2,0,218,7,2,0,232,7,2,0,233,7,2,0,234,7,0,0,18,0,0,0,122,0,11,0,235,7,4,0,236,7,4,0,237,7,22,0,238,7,11,0,239,7,36,1,16,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,32,1,227,7,14,0,240,7,37,1,241,7,0,0,242,7,38,1,243,7,2,0,18,0,2,0,244,7,2,0,245,7,2,0,246,7,11,0,247,7,39,1,16,0,31,1,0,0,
31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,32,1,227,7,40,1,248,7,14,0,249,7,2,0,118,1,2,0,250,7,4,0,18,0,7,0,251,7,7,0,252,7,4,0,179,7,4,0,70,0,41,1,11,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,2,0,250,7,2,0,18,0,4,0,70,0,40,1,248,7,32,1,227,7,42,1,3,0,42,1,0,0,42,1,1,0,
7,0,253,7,43,1,9,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,32,1,227,7,14,0,254,7,4,0,255,7,4,0,18,0,44,1,23,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,32,1,227,7,7,0,152,2,7,0,153,2,2,0,228,7,2,0,0,8,2,0,1,8,2,0,2,8,4,0,18,0,7,0,3,8,4,0,156,0,4,0,4,8,4,0,5,8,
4,0,70,0,208,0,68,4,45,1,6,8,0,0,153,0,0,0,7,8,46,1,5,0,47,1,8,8,0,0,5,8,0,0,9,8,0,0,10,8,0,0,11,8,48,1,23,0,0,0,12,8,0,0,13,8,0,0,14,8,0,0,15,8,0,0,16,8,0,0,17,8,0,0,18,8,0,0,19,8,4,0,20,8,4,0,21,8,4,0,22,8,4,0,23,8,4,0,24,8,2,0,25,8,2,0,70,0,2,0,16,0,2,0,18,0,2,0,26,8,
2,0,27,8,4,0,229,1,2,0,28,8,2,0,29,8,0,0,30,8,49,1,17,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,4,0,31,8,48,1,32,8,50,1,33,8,14,0,34,8,14,0,35,8,51,1,36,8,28,1,37,8,28,1,38,8,52,1,39,8,2,0,40,8,2,0,41,8,2,0,42,8,2,0,43,8,53,1,6,0,53,1,0,0,0,0,235,7,0,0,44,8,2,0,45,8,2,0,46,8,2,0,47,0,
54,1,3,0,4,0,47,8,4,0,48,8,4,0,49,8,55,1,3,0,54,1,50,8,4,0,51,8,4,0,70,0,56,1,10,0,56,1,0,0,56,1,1,0,0,0,52,8,11,0,181,5,4,0,53,8,10,0,106,0,9,0,48,3,0,0,54,8,0,0,55,8,0,0,56,8,57,1,8,0,57,1,0,0,57,1,1,0,4,0,53,8,0,0,93,0,0,0,57,8,14,0,58,8,4,0,59,8,4,0,60,8,58,1,18,0,58,1,0,0,
58,1,1,0,4,0,53,8,0,0,93,0,0,0,57,8,56,1,61,8,4,0,62,8,4,0,63,8,0,0,64,8,11,0,65,8,61,0,53,3,0,0,66,8,4,0,67,8,2,0,68,8,2,0,94,0,14,0,69,8,4,0,70,8,4,0,71,8,59,1,6,0,14,0,72,8,4,0,73,8,4,0,74,8,4,0,75,8,4,0,76,8,0,0,77,8,60,1,28,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,4,0,18,0,
51,0,53,3,44,0,101,1,61,1,6,8,62,1,78,8,208,0,68,4,7,0,79,8,7,0,152,2,7,0,153,2,7,0,3,8,7,0,80,8,7,0,81,8,0,0,118,1,0,0,82,8,2,0,70,0,2,0,83,8,2,0,84,8,0,0,85,8,0,0,86,8,0,0,87,8,0,0,179,7,4,0,88,8,4,0,87,0,46,1,89,8,63,1,33,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,
38,0,90,8,4,0,91,8,4,0,92,8,2,0,94,0,2,0,93,8,2,0,94,8,0,0,95,8,0,0,96,8,4,0,97,8,4,0,98,8,4,0,99,8,2,0,100,8,2,0,101,8,2,0,102,8,2,0,103,8,7,0,104,8,18,0,105,8,18,0,106,8,4,0,107,8,4,0,108,8,0,0,109,8,0,0,110,8,2,0,111,8,2,0,112,8,0,0,102,2,11,0,113,8,7,0,114,8,64,1,10,0,22,0,32,0,11,0,115,8,
11,0,116,8,11,0,117,8,11,0,118,8,11,0,119,8,4,0,94,0,4,0,120,8,0,0,121,8,0,0,122,8,65,1,10,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,64,1,123,8,2,0,94,0,2,0,93,8,4,0,72,0,11,0,124,8,66,1,7,0,66,1,0,0,66,1,1,0,66,0,102,1,67,1,125,8,4,0,70,0,7,0,126,8,0,0,127,8,68,1,31,0,31,1,0,0,31,1,1,0,
14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,32,1,227,7,22,0,128,8,22,0,85,0,2,0,18,0,2,0,72,0,7,0,129,8,7,0,87,0,7,0,152,2,7,0,153,2,7,0,3,8,7,0,79,8,14,0,130,8,66,0,102,1,66,0,131,8,0,0,132,8,4,0,133,8,4,0,32,2,2,0,134,8,2,0,135,8,2,0,23,1,0,0,136,8,0,0,40,6,14,0,137,8,69,1,138,8,208,0,68,4,70,1,10,0,
31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,2,0,18,0,2,0,98,4,4,0,70,0,208,0,68,4,71,1,7,0,71,1,0,0,71,1,1,0,4,0,139,8,4,0,22,0,0,0,90,0,4,0,140,8,4,0,16,0,72,1,14,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,4,0,94,8,4,0,70,0,14,0,141,8,14,0,142,8,0,0,143,8,
0,0,144,8,4,0,145,8,4,0,146,8,73,1,7,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,0,0,117,1,0,0,147,8,0,0,148,8,74,1,29,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,152,2,7,0,153,2,7,0,149,8,7,0,150,8,7,0,3,8,23,1,151,8,20,1,118,7,75,1,6,8,4,0,18,0,2,0,118,1,2,0,156,0,4,0,152,8,7,0,153,8,7,0,226,3,
7,0,63,3,4,0,70,0,7,0,154,8,7,0,155,8,4,0,156,8,2,0,1,7,2,0,87,0,4,0,179,7,4,0,40,6,7,0,79,8,46,1,89,8,76,1,7,0,76,1,0,0,76,1,1,0,0,0,157,8,2,0,158,8,2,0,159,8,2,0,160,8,2,0,161,8,77,1,13,0,2,0,159,8,2,0,162,8,2,0,163,8,0,0,164,8,0,0,191,0,2,0,165,8,2,0,166,8,2,0,167,8,2,0,168,8,2,0,169,8,
2,0,218,7,7,0,170,8,7,0,171,8,78,1,18,0,78,1,0,0,78,1,1,0,0,0,19,0,77,1,172,8,77,1,173,8,77,1,174,8,77,1,175,8,7,0,176,8,2,0,177,8,2,0,178,8,2,0,179,8,2,0,180,8,2,0,181,8,2,0,182,8,2,0,183,8,2,0,184,8,2,0,185,8,2,0,70,0,79,1,10,0,0,0,186,8,0,0,187,8,0,0,188,8,0,0,189,8,0,0,190,8,0,0,191,8,2,0,192,8,
2,0,193,8,2,0,194,8,2,0,195,8,80,1,8,0,0,0,196,8,0,0,197,8,0,0,198,8,0,0,199,8,0,0,200,8,0,0,201,8,7,0,123,7,7,0,70,0,81,1,5,0,0,0,202,8,0,0,203,8,2,0,204,8,2,0,205,8,4,0,70,0,82,1,4,0,0,0,206,8,0,0,207,8,4,0,208,8,4,0,87,0,83,1,29,0,79,1,209,8,79,1,210,8,79,1,211,8,79,1,212,8,79,1,213,8,79,1,214,8,
79,1,215,8,79,1,216,8,79,1,217,8,79,1,218,8,79,1,219,8,79,1,220,8,79,1,221,8,79,1,222,8,79,1,223,8,79,1,224,8,79,1,225,8,79,1,226,8,80,1,227,8,81,1,228,8,0,0,229,8,7,0,230,8,2,0,231,8,2,0,117,1,0,0,232,8,7,0,233,8,0,0,234,8,0,0,235,8,0,0,236,8,84,1,189,0,0,0,203,8,0,0,237,8,0,0,190,8,0,0,238,8,0,0,202,8,0,0,239,8,
0,0,240,8,0,0,241,8,0,0,242,8,0,0,243,8,0,0,244,8,0,0,245,8,0,0,246,8,0,0,247,8,0,0,248,8,0,0,249,8,0,0,250,8,0,0,251,8,0,0,252,8,0,0,253,8,81,1,254,8,82,1,255,8,0,0,0,9,0,0,1,9,0,0,2,9,0,0,3,9,0,0,4,9,0,0,5,9,0,0,6,9,0,0,7,9,0,0,8,9,0,0,9,9,0,0,10,9,0,0,11,9,0,0,12,9,0,0,13,9,
0,0,14,9,0,0,15,9,0,0,16,9,0,0,17,9,0,0,18,9,0,0,19,9,0,0,20,9,0,0,21,9,0,0,22,9,0,0,23,9,0,0,24,9,0,0,25,9,0,0,26,9,0,0,27,9,0,0,28,9,0,0,29,9,0,0,30,9,0,0,31,9,0,0,32,9,0,0,33,9,0,0,34,9,0,0,35,9,0,0,36,9,0,0,37,9,0,0,38,9,0,0,39,9,0,0,40,9,0,0,41,9,0,0,42,9,0,0,43,9,
0,0,44,9,0,0,45,9,0,0,46,9,0,0,47,9,0,0,48,9,0,0,49,9,0,0,50,9,0,0,51,9,0,0,52,9,0,0,53,9,0,0,54,9,0,0,55,9,0,0,56,9,0,0,57,9,0,0,58,9,0,0,59,9,0,0,60,9,0,0,61,9,0,0,62,9,0,0,63,9,0,0,64,9,0,0,65,9,0,0,66,9,0,0,67,9,0,0,68,9,0,0,69,9,0,0,70,9,0,0,71,9,0,0,72,9,0,0,73,9,
0,0,74,9,0,0,75,9,0,0,76,9,0,0,77,9,0,0,78,9,0,0,79,9,0,0,80,9,0,0,81,9,0,0,82,9,0,0,83,9,0,0,84,9,0,0,85,9,0,0,86,9,0,0,87,9,0,0,88,9,0,0,89,9,0,0,90,9,0,0,91,9,0,0,92,9,0,0,93,9,0,0,94,9,0,0,95,9,0,0,96,9,0,0,97,9,0,0,98,9,0,0,99,9,0,0,100,9,0,0,101,9,0,0,102,9,0,0,103,9,
0,0,104,9,0,0,105,9,0,0,106,9,0,0,107,9,0,0,108,9,0,0,109,9,0,0,110,9,7,0,111,9,0,0,112,9,0,0,113,9,0,0,114,9,0,0,115,9,0,0,116,9,0,0,117,9,0,0,118,9,0,0,119,9,0,0,120,9,0,0,121,9,0,0,122,9,0,0,123,9,0,0,124,9,0,0,125,9,0,0,126,9,0,0,127,9,0,0,128,9,0,0,129,9,0,0,130,9,0,0,131,9,0,0,132,9,0,0,133,9,
0,0,134,9,0,0,135,9,0,0,136,9,0,0,137,9,0,0,138,9,0,0,139,9,0,0,140,9,0,0,141,9,0,0,142,9,0,0,143,9,0,0,144,9,0,0,145,9,0,0,146,9,0,0,147,9,0,0,148,9,0,0,149,9,0,0,150,9,0,0,151,9,0,0,152,9,0,0,153,9,0,0,154,9,0,0,155,9,0,0,156,9,0,0,157,9,0,0,158,9,0,0,159,9,0,0,160,9,0,0,161,9,0,0,162,9,0,0,163,9,
0,0,164,9,0,0,165,9,0,0,166,9,85,1,5,0,0,0,167,9,0,0,7,9,0,0,12,9,2,0,18,0,2,0,70,0,86,1,24,0,86,1,0,0,86,1,1,0,0,0,111,6,83,1,168,9,84,1,169,9,84,1,170,9,84,1,171,9,84,1,172,9,84,1,173,9,84,1,174,9,84,1,175,9,84,1,176,9,84,1,177,9,84,1,178,9,84,1,179,9,84,1,180,9,84,1,181,9,84,1,182,9,84,1,183,9,84,1,184,9,
84,1,185,9,85,1,186,9,4,0,187,9,4,0,70,0,87,1,4,0,87,1,0,0,87,1,1,0,0,0,188,9,21,0,229,5,88,1,5,0,88,1,0,0,88,1,1,0,0,0,189,9,0,0,18,0,0,0,122,0,89,1,5,0,4,0,18,0,4,0,70,0,7,0,210,2,7,0,190,9,7,0,103,2,90,1,8,0,7,0,191,9,7,0,192,9,7,0,193,9,7,0,194,9,7,0,195,9,7,0,196,9,2,0,18,0,2,0,117,1,
91,1,127,0,4,0,39,0,4,0,40,0,4,0,18,0,4,0,197,9,4,0,198,9,0,0,199,9,0,0,200,9,0,0,201,9,0,0,202,9,0,0,203,9,0,0,204,9,0,0,205,9,0,0,206,9,0,0,207,9,0,0,208,9,4,0,209,9,2,0,210,9,2,0,211,9,2,0,212,9,2,0,213,9,2,0,214,9,2,0,215,9,4,0,216,9,4,0,217,9,4,0,218,9,2,0,219,9,2,0,220,9,4,0,221,9,4,0,222,9,
4,0,223,9,4,0,224,9,4,0,225,9,4,0,141,8,4,0,226,9,7,0,227,9,4,0,228,9,0,0,229,9,0,0,87,0,2,0,230,9,2,0,231,9,2,0,232,9,0,0,233,9,14,0,234,9,14,0,235,9,14,0,236,9,14,0,237,9,14,0,238,9,14,0,239,9,14,0,240,9,0,0,241,9,2,0,242,9,2,0,243,9,2,0,244,9,2,0,245,9,2,0,246,9,2,0,247,9,2,0,248,9,2,0,249,9,89,1,250,9,
2,0,251,9,2,0,252,9,2,0,253,9,2,0,254,9,2,0,255,9,2,0,0,10,2,0,1,10,2,0,2,10,4,0,3,10,4,0,4,10,7,0,5,10,2,0,6,10,2,0,7,10,2,0,8,10,2,0,9,10,2,0,10,10,2,0,11,10,2,0,12,10,2,0,13,10,2,0,14,10,0,0,15,10,0,0,16,10,0,0,17,10,0,0,18,10,0,0,40,6,0,0,19,10,4,0,20,10,7,0,21,10,2,0,22,10,2,0,23,10,
2,0,24,10,2,0,25,10,2,0,26,10,2,0,27,10,7,0,28,10,7,0,29,10,7,0,30,10,4,0,31,10,2,0,32,10,2,0,33,10,7,0,34,10,2,0,13,7,2,0,14,7,2,0,35,10,2,0,86,6,59,0,36,10,7,0,37,10,7,0,38,10,2,0,39,10,0,0,40,10,0,0,70,0,0,0,41,10,0,0,42,10,0,0,43,10,4,0,44,10,4,0,45,10,7,0,46,10,7,0,47,10,4,0,48,10,2,0,49,10,
2,0,50,10,2,0,51,10,2,0,52,10,2,0,53,10,2,0,54,10,90,1,55,10,2,0,56,10,0,0,57,10,92,1,24,0,22,0,32,0,14,0,58,10,14,0,59,10,14,0,60,10,14,0,163,7,45,0,145,0,45,0,61,10,2,0,62,10,2,0,63,10,0,0,64,10,0,0,157,4,0,0,65,10,0,0,66,10,0,0,67,10,0,0,68,10,0,0,69,10,0,0,70,10,0,0,71,10,0,0,72,10,0,0,15,3,2,0,73,10,
2,0,74,10,28,1,75,10,11,0,76,10,93,1,6,0,93,1,0,0,93,1,1,0,93,1,77,10,15,0,78,10,2,0,18,0,2,0,206,2,94,1,7,0,94,1,0,0,94,1,1,0,93,1,79,10,93,1,80,10,2,0,46,6,2,0,18,0,4,0,70,0,95,1,20,0,95,1,0,0,95,1,1,0,96,1,81,10,97,1,39,8,0,0,82,10,0,0,83,10,0,0,84,10,4,0,85,10,4,0,86,10,4,0,87,10,4,0,88,10,
2,0,89,10,2,0,70,0,2,0,18,0,2,0,90,10,2,0,91,10,2,0,92,10,4,0,93,10,95,1,94,10,11,0,95,10,98,1,3,0,98,1,0,0,98,1,1,0,0,0,96,10,99,1,15,0,99,1,0,0,99,1,1,0,100,1,81,10,0,0,97,10,4,0,98,10,4,0,18,0,4,0,99,10,4,0,100,10,4,0,101,10,4,0,102,10,0,0,103,10,4,0,104,10,4,0,105,10,21,0,31,0,101,1,106,10,102,1,5,0,
102,1,0,0,102,1,1,0,0,0,107,10,2,0,71,3,2,0,42,1,103,1,23,0,103,1,0,0,103,1,1,0,93,1,79,10,93,1,80,10,93,1,108,10,93,1,109,10,92,1,110,10,18,0,55,0,0,0,164,7,0,0,111,10,2,0,219,7,2,0,220,7,2,0,112,10,2,0,66,10,2,0,18,0,2,0,113,10,0,0,64,10,0,0,70,0,104,1,81,10,14,0,114,10,14,0,163,7,14,0,115,10,14,0,116,10,105,1,31,0,
105,1,0,0,105,1,1,0,32,1,227,7,18,0,117,10,18,0,118,10,2,0,219,7,2,0,220,7,2,0,119,10,2,0,120,10,2,0,121,10,2,0,18,0,7,0,148,2,2,0,87,10,2,0,88,10,2,0,65,10,2,0,122,10,2,0,70,10,2,0,123,10,2,0,124,10,2,0,70,0,106,1,81,10,14,0,125,10,14,0,126,10,14,0,127,10,14,0,128,10,14,0,129,10,14,0,115,10,14,0,130,10,28,1,131,10,0,0,132,10,
11,0,133,10,107,1,12,0,0,0,134,10,2,0,135,10,2,0,136,10,2,0,137,10,0,0,15,3,92,1,138,10,45,0,139,10,4,0,140,10,4,0,141,10,10,0,142,10,0,0,143,10,0,0,157,8,108,1,3,0,108,1,0,0,108,1,1,0,47,0,158,0,109,1,3,0,0,0,44,8,4,0,144,10,4,0,145,10,110,1,4,0,4,0,91,8,4,0,146,10,4,0,97,8,4,0,147,10,111,1,2,0,4,0,148,10,4,0,149,10,
112,1,5,0,7,0,150,10,7,0,151,10,7,0,152,10,4,0,18,0,4,0,70,0,113,1,10,0,0,0,153,10,0,0,15,8,47,0,158,0,2,0,154,10,2,0,238,5,2,0,155,10,2,0,156,10,2,0,157,10,0,0,158,10,0,0,191,0,114,1,13,0,114,1,0,0,114,1,1,0,4,0,29,0,4,0,159,10,4,0,160,10,4,0,161,10,109,1,162,10,0,0,153,10,113,1,62,4,110,1,163,10,111,1,164,10,112,1,165,10,
54,0,189,0,115,1,55,0,115,1,0,0,115,1,1,0,11,0,166,10,11,0,25,0,0,0,19,0,4,0,18,0,4,0,16,0,4,0,22,0,4,0,16,3,4,0,167,10,4,0,168,10,4,0,160,10,4,0,161,10,4,0,169,10,4,0,22,1,4,0,170,10,4,0,171,10,7,0,172,10,7,0,173,10,7,0,174,10,2,0,175,10,2,0,176,10,4,0,177,10,4,0,178,10,114,1,179,10,33,0,84,0,45,0,145,0,29,0,180,10,
20,1,118,7,47,1,8,8,14,0,160,0,7,0,181,10,7,0,182,10,115,1,183,10,115,1,184,10,115,1,185,10,14,0,186,10,116,1,187,10,11,0,188,10,7,0,239,4,7,0,189,10,7,0,190,10,7,0,191,10,11,0,192,10,4,0,193,10,4,0,194,10,4,0,195,10,7,0,196,10,4,0,149,0,0,0,190,0,0,0,47,0,0,0,193,0,55,0,195,0,21,0,229,5,14,0,74,4,117,1,5,0,117,1,0,0,117,1,1,0,
14,0,197,10,115,1,198,10,4,0,199,10,16,1,12,0,14,0,200,10,14,0,186,10,14,0,201,10,115,1,202,10,0,0,203,10,0,0,204,10,0,0,205,10,4,0,206,10,4,0,207,10,4,0,208,10,4,0,209,10,19,0,210,10,118,1,4,0,7,0,211,10,7,0,63,3,2,0,212,10,2,0,213,10,119,1,6,0,7,0,214,10,7,0,215,10,7,0,216,10,7,0,217,10,4,0,218,10,4,0,219,10,120,1,8,0,7,0,220,10,
7,0,221,10,7,0,222,10,7,0,223,10,7,0,224,10,4,0,59,3,4,0,225,10,4,0,226,10,121,1,2,0,7,0,119,6,7,0,70,0,122,1,5,0,7,0,227,10,7,0,228,10,4,0,94,0,4,0,17,3,4,0,229,10,123,1,2,0,7,0,230,10,7,0,231,10,124,1,10,0,0,0,232,10,4,0,233,10,7,0,234,10,7,0,235,10,7,0,153,8,7,0,236,10,0,0,18,0,0,0,218,7,0,0,144,2,0,0,191,0,
125,1,9,0,125,1,0,0,125,1,1,0,4,0,16,0,4,0,18,0,0,0,19,0,4,0,237,10,4,0,238,10,115,1,239,10,47,1,240,10,126,1,3,0,125,1,2,3,112,1,241,10,7,0,242,10,127,1,2,0,125,1,2,3,63,0,243,10,128,1,2,0,125,1,2,3,63,0,243,10,129,1,3,0,125,1,2,3,7,0,65,1,7,0,66,1,130,1,1,0,125,1,2,3,131,1,3,0,125,1,2,3,7,0,244,10,7,0,70,0,
132,1,9,0,125,1,2,3,7,0,245,10,7,0,148,0,7,0,246,10,7,0,247,10,7,0,66,1,7,0,248,10,7,0,249,10,4,0,16,0,45,1,6,0,61,0,250,10,61,0,251,10,61,0,252,10,61,0,253,10,61,0,254,10,61,0,255,10,133,1,6,0,133,1,0,0,133,1,1,0,2,0,16,0,2,0,18,0,2,0,0,11,2,0,60,0,134,1,8,0,134,1,0,0,134,1,1,0,2,0,16,0,2,0,18,0,2,0,0,11,
2,0,60,0,7,0,22,0,7,0,149,0,135,1,45,0,135,1,0,0,135,1,1,0,2,0,16,0,2,0,18,0,2,0,0,11,2,0,18,1,2,0,25,5,2,0,1,11,7,0,2,11,7,0,3,11,7,0,77,3,4,0,4,11,4,0,86,0,4,0,19,3,7,0,5,11,7,0,6,11,7,0,7,11,7,0,8,11,7,0,9,11,7,0,10,11,7,0,74,3,7,0,100,1,7,0,11,11,7,0,12,11,7,0,70,0,7,0,13,11,
7,0,14,11,7,0,15,11,2,0,16,11,2,0,17,11,2,0,18,11,2,0,19,11,2,0,20,11,2,0,21,11,2,0,22,11,2,0,23,11,2,0,83,2,2,0,24,11,2,0,80,2,2,0,25,11,0,0,26,11,0,0,27,11,7,0,16,1,136,1,28,11,74,0,35,2,137,1,16,0,137,1,0,0,137,1,1,0,2,0,16,0,2,0,18,0,2,0,0,11,2,0,18,1,7,0,69,3,7,0,70,3,7,0,71,3,7,0,136,2,
7,0,72,3,7,0,73,3,7,0,29,11,7,0,74,3,7,0,76,3,7,0,77,3,38,1,5,0,2,0,16,0,2,0,30,11,2,0,18,0,2,0,31,11,22,0,128,8,138,1,3,0,4,0,74,0,4,0,32,11,38,1,2,0,139,1,7,0,139,1,0,0,139,1,1,0,0,0,19,0,2,0,16,0,2,0,18,0,4,0,21,0,11,0,65,8,140,1,5,0,0,0,19,0,7,0,125,1,7,0,33,11,4,0,34,11,4,0,70,0,
141,1,6,0,2,0,16,0,2,0,18,0,2,0,72,0,2,0,118,1,0,0,35,11,0,0,36,11,142,1,4,0,0,0,19,0,73,0,37,11,7,0,125,1,7,0,70,0,143,1,6,0,2,0,245,10,2,0,38,11,2,0,16,0,2,0,39,11,0,0,40,11,0,0,41,11,144,1,5,0,4,0,16,0,4,0,70,0,0,0,19,0,0,0,42,11,0,0,43,11,145,1,3,0,4,0,16,0,4,0,70,0,0,0,19,0,146,1,4,0,
2,0,44,11,2,0,45,11,2,0,18,0,2,0,70,0,147,1,6,0,0,0,19,0,0,0,46,11,2,0,47,11,2,0,74,3,2,0,118,1,2,0,87,0,148,1,5,0,0,0,19,0,7,0,63,3,7,0,115,5,2,0,18,0,2,0,31,3,149,1,3,0,0,0,19,0,4,0,19,3,4,0,44,11,150,1,7,0,0,0,19,0,7,0,115,5,0,0,35,11,0,0,36,11,2,0,118,1,2,0,72,0,4,0,48,11,151,1,4,0,
0,0,49,11,0,0,50,11,4,0,16,0,7,0,36,3,152,1,3,0,29,0,51,11,0,0,52,11,0,0,53,11,153,1,18,0,153,1,0,0,153,1,1,0,2,0,16,0,2,0,54,11,2,0,18,0,2,0,55,11,2,0,56,11,2,0,57,11,2,0,72,0,2,0,87,0,0,0,19,0,11,0,2,0,154,1,58,11,29,0,48,0,2,0,172,6,2,0,235,2,2,0,59,11,2,0,70,0,155,1,11,0,0,0,19,0,0,0,16,0,
0,0,60,11,2,0,18,0,2,0,31,3,2,0,61,11,4,0,62,11,4,0,63,11,4,0,64,11,4,0,65,11,4,0,66,11,156,1,1,0,0,0,67,11,157,1,4,0,38,0,90,8,0,0,188,9,4,0,118,1,4,0,18,0,154,1,18,0,154,1,0,0,154,1,1,0,154,1,68,11,2,0,16,0,2,0,18,0,2,0,69,11,2,0,57,11,2,0,54,11,2,0,70,11,2,0,87,0,2,0,32,2,0,0,19,0,11,0,2,0,
158,1,58,11,153,1,71,11,2,0,14,0,2,0,72,11,4,0,73,11,159,1,3,0,4,0,48,3,4,0,70,0,29,0,48,0,160,1,15,0,206,0,74,11,2,0,16,0,2,0,18,0,7,0,2,11,7,0,3,11,0,0,19,0,0,0,75,11,2,0,76,11,2,0,77,11,2,0,157,0,2,0,78,11,2,0,79,11,2,0,195,10,7,0,80,11,7,0,81,11,161,1,8,0,7,0,82,11,7,0,83,11,7,0,84,11,7,0,85,11,
7,0,86,11,7,0,87,11,7,0,88,11,7,0,89,11,162,1,13,0,2,0,18,0,2,0,90,11,4,0,72,0,4,0,87,0,2,0,91,11,7,0,239,4,7,0,189,10,116,1,187,10,161,1,92,11,2,0,16,0,2,0,40,6,2,0,140,1,2,0,93,11,163,1,14,0,4,0,48,3,2,0,16,0,2,0,18,0,29,0,48,0,87,0,94,11,0,0,19,0,7,0,95,11,7,0,96,11,7,0,106,4,2,0,97,11,2,0,98,11,
2,0,96,4,2,0,95,4,4,0,70,0,164,1,5,0,2,0,16,0,2,0,72,0,4,0,70,0,45,0,145,0,29,0,154,6,165,1,5,0,4,0,70,0,4,0,16,0,0,0,19,0,0,0,42,11,29,0,48,0,166,1,13,0,2,0,18,0,2,0,16,0,2,0,54,11,2,0,107,4,7,0,99,11,7,0,100,11,7,0,117,1,7,0,42,1,7,0,78,4,7,0,82,4,7,0,101,11,7,0,102,11,29,0,103,11,167,1,10,0,
2,0,18,0,2,0,16,0,7,0,2,11,7,0,3,11,0,0,19,0,0,0,75,11,2,0,72,0,2,0,87,0,2,0,32,2,2,0,40,6,168,1,8,0,29,0,48,0,7,0,71,3,7,0,104,11,7,0,105,11,7,0,107,4,2,0,72,0,2,0,31,3,7,0,87,0,169,1,12,0,2,0,16,0,2,0,118,1,2,0,18,0,2,0,74,3,2,0,48,3,2,0,106,11,4,0,70,0,7,0,107,11,7,0,108,11,7,0,109,11,
7,0,110,11,0,0,111,11,170,1,9,0,2,0,18,0,2,0,16,0,4,0,2,11,4,0,3,11,0,0,19,0,2,0,117,1,2,0,67,0,2,0,112,11,2,0,113,11,171,1,7,0,4,0,19,3,4,0,114,11,4,0,115,11,4,0,116,11,7,0,117,11,7,0,118,11,0,0,35,11,172,1,7,0,0,0,119,11,29,0,120,11,0,0,52,11,2,0,121,11,2,0,72,0,4,0,87,0,0,0,53,11,173,1,6,0,2,0,18,0,
2,0,16,0,4,0,2,11,4,0,3,11,0,0,122,11,0,0,123,11,174,1,1,0,4,0,18,0,175,1,6,0,0,0,102,2,2,0,16,0,2,0,18,0,4,0,124,11,7,0,125,11,38,0,90,8,176,1,4,0,0,0,47,0,2,0,18,0,4,0,16,0,29,0,48,0,177,1,2,0,4,0,16,0,4,0,208,7,178,1,8,0,0,0,49,11,0,0,50,11,4,0,16,0,7,0,91,2,7,0,30,4,7,0,70,0,29,0,151,3,
29,0,126,11,179,1,11,0,0,0,191,0,0,0,18,0,2,0,127,11,4,0,16,0,7,0,125,1,7,0,128,11,7,0,129,11,7,0,130,11,4,0,131,11,29,0,151,3,29,0,132,11,180,1,7,0,2,0,16,0,2,0,18,0,4,0,133,11,7,0,134,11,7,0,135,11,7,0,136,11,7,0,137,11,158,1,10,0,158,1,0,0,158,1,1,0,158,1,68,11,2,0,16,0,2,0,18,0,2,0,54,11,2,0,138,11,0,0,19,0,
11,0,2,0,29,0,48,0,116,1,18,0,22,0,32,0,0,0,34,0,25,0,37,0,11,0,139,11,25,0,140,11,33,0,84,0,7,0,239,4,7,0,117,0,7,0,189,10,7,0,82,11,7,0,83,11,7,0,53,4,4,0,94,0,4,0,70,0,11,0,159,0,11,0,141,11,11,0,105,7,11,0,142,11,181,1,6,0,181,1,0,0,181,1,1,0,29,0,48,0,11,0,143,11,2,0,23,1,0,0,15,3,74,0,5,0,22,0,32,0,
14,0,144,11,26,0,177,0,4,0,157,0,7,0,145,11,182,1,36,0,182,1,0,0,182,1,1,0,21,0,229,5,182,1,36,0,14,0,146,11,0,0,19,0,7,0,147,11,7,0,148,11,7,0,149,11,7,0,150,11,4,0,18,0,7,0,151,11,7,0,152,11,7,0,153,11,7,0,154,11,7,0,125,1,7,0,91,2,7,0,155,11,7,0,17,3,7,0,156,11,7,0,157,11,7,0,158,11,7,0,159,11,7,0,160,11,7,0,161,11,
7,0,162,11,7,0,163,11,7,0,164,11,7,0,165,11,7,0,166,11,7,0,167,11,7,0,168,11,7,0,211,0,4,0,157,0,2,0,169,11,2,0,72,0,183,1,27,0,22,0,32,0,36,0,79,0,14,0,170,11,14,0,171,11,14,0,172,11,182,1,173,11,184,1,174,11,11,0,175,11,4,0,18,0,4,0,176,7,4,0,176,11,4,0,70,0,2,0,78,3,2,0,236,7,4,0,177,11,4,0,157,0,4,0,178,11,2,0,179,11,
2,0,180,11,2,0,181,11,2,0,182,11,4,0,183,11,4,0,184,11,4,0,185,11,4,0,186,11,4,0,187,11,4,0,188,11,185,1,2,0,7,0,224,2,4,0,18,0,210,0,7,0,185,1,189,11,4,0,17,3,4,0,190,11,4,0,191,11,7,0,115,0,4,0,192,11,4,0,18,0,209,0,16,0,4,0,193,11,4,0,194,11,4,0,195,11,4,0,196,11,2,0,197,11,2,0,198,11,2,0,199,11,2,0,23,1,2,0,200,11,
2,0,201,11,2,0,202,11,2,0,203,11,4,0,204,11,4,0,205,11,4,0,206,11,4,0,207,11,186,1,54,0,186,1,0,0,186,1,1,0,21,0,229,5,14,0,143,4,0,0,19,0,2,0,18,0,2,0,208,11,2,0,94,4,2,0,209,11,0,0,210,11,0,0,211,11,0,0,130,2,0,0,212,11,0,0,213,11,182,1,214,11,186,1,36,0,186,1,215,11,14,0,216,11,14,0,217,11,210,0,70,4,29,0,218,11,186,1,219,11,
7,0,220,11,0,0,174,0,7,0,108,1,7,0,211,0,7,0,221,11,7,0,70,2,7,0,84,4,7,0,86,4,2,0,124,4,2,0,70,0,7,0,222,11,7,0,223,11,7,0,89,4,7,0,224,11,7,0,225,11,7,0,226,11,7,0,227,11,7,0,228,11,7,0,229,11,7,0,230,11,7,0,231,11,7,0,161,11,7,0,162,11,7,0,163,11,7,0,164,11,7,0,165,11,7,0,166,11,7,0,167,11,7,0,168,11,186,1,232,11,
186,1,233,11,11,0,234,11,207,0,16,0,14,0,235,11,84,0,236,11,2,0,18,0,2,0,70,0,4,0,237,11,4,0,72,0,7,0,88,0,7,0,238,11,7,0,239,11,14,0,240,11,4,0,241,11,4,0,242,11,11,0,243,11,11,0,244,11,209,0,69,4,0,0,245,11,187,1,1,0,4,0,242,11,188,1,12,0,4,0,242,11,7,0,66,11,2,0,246,11,2,0,247,11,7,0,248,11,7,0,249,11,2,0,116,3,2,0,18,0,
7,0,250,11,7,0,251,11,7,0,252,11,7,0,253,11,189,1,7,0,189,1,0,0,189,1,1,0,14,0,254,11,4,0,18,0,4,0,255,11,0,0,19,0,85,1,0,12,206,0,9,0,22,0,32,0,14,0,1,12,14,0,235,11,14,0,2,12,14,0,102,7,4,0,18,0,4,0,3,12,4,0,4,12,4,0,70,0,40,1,8,0,22,0,52,4,14,0,235,11,74,0,5,12,0,0,6,12,4,0,7,12,4,0,18,0,4,0,8,12,
4,0,70,0,190,1,13,0,31,1,0,0,31,1,1,0,14,0,163,7,4,0,164,7,7,0,165,7,2,0,166,7,32,1,227,7,206,0,65,4,40,1,9,12,0,0,118,1,0,0,250,7,2,0,18,0,7,0,10,12,191,1,8,0,191,1,0,0,191,1,1,0,189,1,11,12,33,0,84,0,14,0,71,4,4,0,18,0,0,0,19,0,4,0,64,10,192,1,5,0,192,1,0,0,192,1,1,0,33,0,84,0,2,0,18,0,0,0,12,12,
193,1,14,0,193,1,0,0,193,1,1,0,11,0,2,0,2,0,16,0,2,0,18,0,0,0,13,12,0,0,14,12,0,0,19,0,2,0,70,0,7,0,15,12,7,0,16,12,33,0,84,0,7,0,17,12,7,0,18,12,194,1,9,0,194,1,0,0,194,1,1,0,29,0,19,12,0,0,81,3,7,0,20,12,2,0,51,3,2,0,18,0,2,0,16,0,2,0,21,12,195,1,7,0,38,0,90,8,21,0,229,5,4,0,18,0,4,0,22,12,
14,0,23,12,29,0,19,12,0,0,81,3,196,1,15,0,29,0,19,12,2,0,24,12,2,0,18,0,2,0,25,12,2,0,26,12,0,0,81,3,29,0,27,12,0,0,28,12,7,0,29,12,7,0,91,2,7,0,30,12,7,0,31,12,2,0,16,0,2,0,118,1,7,0,125,1,197,1,10,0,29,0,19,12,7,0,189,11,2,0,32,12,2,0,33,12,2,0,18,0,2,0,34,12,7,0,35,12,7,0,36,12,7,0,37,12,7,0,38,12,
198,1,6,0,29,0,19,12,4,0,39,12,4,0,40,12,4,0,94,0,4,0,70,0,0,0,81,3,199,1,4,0,29,0,19,12,4,0,18,0,4,0,39,12,0,0,81,3,200,1,4,0,29,0,19,12,4,0,18,0,4,0,39,12,0,0,81,3,201,1,4,0,29,0,19,12,4,0,18,0,4,0,39,12,0,0,81,3,202,1,2,0,4,0,18,0,7,0,239,4,203,1,2,0,29,0,19,12,0,0,81,3,204,1,10,0,29,0,19,12,
4,0,41,12,7,0,148,0,4,0,18,0,2,0,86,8,2,0,42,12,2,0,72,0,2,0,87,0,7,0,43,12,0,0,81,3,205,1,10,0,29,0,19,12,2,0,16,0,2,0,33,5,4,0,16,3,4,0,3,11,7,0,104,11,7,0,105,11,4,0,18,0,206,0,74,11,0,0,81,3,206,1,4,0,29,0,19,12,4,0,95,4,4,0,44,12,0,0,81,3,207,1,4,0,29,0,19,12,4,0,95,4,4,0,70,0,0,0,81,3,
208,1,6,0,29,0,19,12,7,0,148,0,7,0,162,3,4,0,45,12,2,0,95,4,2,0,96,4,209,1,10,0,29,0,19,12,4,0,18,0,4,0,46,12,4,0,47,12,7,0,48,12,7,0,35,12,7,0,36,12,7,0,37,12,7,0,38,12,0,0,81,3,210,1,16,0,29,0,19,12,29,0,215,11,4,0,16,0,7,0,49,12,7,0,50,12,7,0,51,12,7,0,52,12,7,0,53,12,7,0,54,12,7,0,55,12,7,0,56,12,
7,0,57,12,2,0,18,0,2,0,70,0,2,0,72,0,2,0,87,0,211,1,3,0,29,0,19,12,4,0,18,0,4,0,83,2,212,1,5,0,29,0,19,12,4,0,18,0,4,0,70,0,7,0,58,12,0,0,81,3,213,1,18,0,29,0,19,12,0,0,81,3,2,0,59,12,2,0,60,12,0,0,61,12,0,0,62,12,7,0,63,12,7,0,64,12,7,0,65,12,7,0,66,12,7,0,67,12,7,0,68,12,7,0,69,12,7,0,70,12,
7,0,71,12,7,0,72,12,7,0,73,12,7,0,74,12,214,1,5,0,29,0,19,12,0,0,81,3,7,0,25,3,2,0,75,12,2,0,18,0,215,1,8,0,7,0,8,0,7,0,9,0,7,0,10,0,7,0,11,0,7,0,76,12,7,0,77,12,2,0,18,0,2,0,83,2,216,1,8,0,7,0,8,0,7,0,9,0,7,0,10,0,7,0,11,0,7,0,76,12,7,0,77,12,2,0,18,0,2,0,83,2,217,1,8,0,7,0,8,0,
7,0,9,0,7,0,10,0,7,0,11,0,7,0,76,12,7,0,77,12,2,0,18,0,2,0,83,2,218,1,7,0,29,0,19,12,0,0,81,3,7,0,125,1,7,0,137,1,2,0,18,0,2,0,118,1,4,0,70,0,219,1,7,0,29,0,151,3,7,0,125,1,2,0,155,3,0,0,158,3,0,0,78,12,7,0,157,3,0,0,102,2,220,1,7,0,20,1,118,7,0,0,79,12,4,0,18,0,4,0,80,12,0,0,81,12,29,0,154,6,
29,0,82,12,221,1,3,0,20,1,118,7,4,0,18,0,4,0,70,0,222,1,6,0,20,1,118,7,4,0,18,0,4,0,70,0,0,0,81,12,7,0,58,12,29,0,154,6,223,1,3,0,196,0,23,4,197,0,24,4,0,0,25,4,224,1,10,0,224,1,0,0,224,1,1,0,2,0,16,0,2,0,18,0,0,0,83,12,7,0,63,1,7,0,64,1,2,0,254,11,2,0,84,12,29,0,48,0,225,1,22,0,225,1,0,0,225,1,1,0,
2,0,18,0,2,0,118,1,2,0,85,12,2,0,86,12,33,0,84,0,206,0,74,11,29,0,201,0,7,0,16,3,7,0,3,11,7,0,87,12,7,0,88,12,7,0,89,12,7,0,90,12,7,0,67,3,7,0,226,3,7,0,76,11,7,0,91,12,0,0,92,12,0,0,93,12,14,0,74,4,226,1,11,0,7,0,103,2,7,0,104,11,7,0,105,11,11,0,2,0,2,0,94,12,2,0,95,12,2,0,96,12,2,0,97,12,2,0,98,12,
2,0,99,12,2,0,47,0,227,1,25,0,227,1,0,0,227,1,1,0,227,1,100,12,21,0,229,5,0,0,101,12,0,0,19,0,11,0,102,12,2,0,16,0,2,0,18,0,2,0,103,12,2,0,104,12,228,1,105,12,0,0,96,10,7,0,106,12,7,0,107,12,11,0,108,12,2,0,109,12,2,0,110,12,4,0,70,0,11,0,159,0,4,0,111,12,4,0,112,12,227,1,113,12,229,1,114,12,226,1,115,12,230,1,49,0,230,1,0,0,
230,1,1,0,230,1,116,12,21,0,229,5,231,1,105,12,0,0,96,10,0,0,19,0,4,0,18,0,2,0,16,0,2,0,70,0,2,0,159,10,2,0,235,2,2,0,117,12,2,0,93,8,2,0,109,12,2,0,30,11,7,0,115,0,14,0,69,11,14,0,118,12,230,1,36,0,22,0,128,8,11,0,102,12,230,1,119,12,14,0,120,12,7,0,106,12,7,0,107,12,7,0,136,2,7,0,71,3,7,0,121,12,7,0,122,12,7,0,123,12,
7,0,124,12,7,0,125,12,4,0,126,12,0,0,127,12,2,0,128,12,2,0,129,12,7,0,130,12,7,0,131,12,2,0,132,12,2,0,133,12,11,0,134,12,19,0,135,12,19,0,136,12,19,0,137,12,2,0,138,12,2,0,139,12,4,0,87,0,232,1,140,12,67,1,1,0,4,0,36,3,233,1,3,0,67,1,245,10,2,0,27,0,2,0,70,0,234,1,5,0,233,1,141,12,0,0,142,12,2,0,143,12,2,0,144,12,4,0,70,0,
229,1,8,0,229,1,0,0,229,1,1,0,230,1,145,12,230,1,146,12,227,1,147,12,227,1,148,12,4,0,18,0,4,0,70,0,66,0,37,0,22,0,32,0,36,0,79,0,235,1,105,12,0,0,96,10,236,1,149,12,208,0,68,4,7,0,126,8,14,0,150,12,14,0,151,12,4,0,16,0,4,0,152,12,4,0,153,12,4,0,18,0,4,0,126,12,2,0,154,12,2,0,159,10,4,0,87,0,4,0,155,12,2,0,156,12,2,0,157,12,
4,0,158,12,19,0,159,12,14,0,69,11,14,0,118,12,237,1,160,12,67,1,161,12,4,0,70,0,238,1,162,12,11,0,163,12,11,0,164,12,4,0,165,12,11,0,166,12,11,0,167,12,11,0,168,12,11,0,169,12,11,0,170,12,11,0,171,12,239,1,4,0,4,0,17,0,4,0,36,3,4,0,104,11,4,0,105,11,240,1,4,0,4,0,17,0,7,0,36,3,7,0,104,11,7,0,105,11,241,1,2,0,0,0,36,3,0,0,117,1,
242,1,4,0,4,0,17,0,7,0,172,12,7,0,104,11,7,0,105,11,243,1,1,0,7,0,173,12,244,1,3,0,4,0,17,0,4,0,70,0,0,0,174,12,245,1,2,0,2,0,18,0,2,0,175,12,246,1,6,0,4,0,147,0,4,0,149,0,4,0,30,11,0,0,176,12,0,0,177,12,2,0,70,0,247,1,6,0,7,0,67,1,7,0,66,1,7,0,246,10,7,0,178,12,7,0,179,12,4,0,70,0,248,1,6,0,247,1,180,12,
247,1,181,12,247,1,182,12,247,1,183,12,7,0,184,12,7,0,185,12,249,1,5,0,7,0,63,3,4,0,186,12,7,0,187,12,7,0,188,12,7,0,189,12,250,1,6,0,7,0,5,0,7,0,6,0,7,0,124,7,7,0,71,3,7,0,136,2,4,0,70,0,251,1,6,0,7,0,5,0,7,0,6,0,7,0,124,7,7,0,71,3,7,0,136,2,4,0,70,0,252,1,2,0,4,0,190,12,0,0,191,12,253,1,16,0,2,0,87,10,
2,0,88,10,2,0,118,0,2,0,192,12,2,0,193,12,2,0,73,0,2,0,129,8,2,0,194,12,7,0,66,3,7,0,195,12,7,0,196,12,2,0,144,1,0,0,197,12,0,0,246,10,4,0,198,12,4,0,199,12,254,1,9,0,7,0,200,12,7,0,201,12,7,0,53,4,7,0,63,3,7,0,202,12,7,0,3,8,2,0,60,3,0,0,203,12,0,0,70,0,255,1,4,0,7,0,204,12,7,0,205,12,2,0,60,3,2,0,70,0,
0,2,3,0,7,0,206,12,7,0,172,10,7,0,14,0,1,2,4,0,0,0,34,0,239,0,251,5,4,0,149,0,4,0,31,5,2,2,6,0,0,0,207,12,239,0,208,12,4,0,149,0,4,0,31,5,4,0,209,12,4,0,70,0,3,2,7,0,2,0,210,12,2,0,211,12,4,0,72,0,0,0,232,4,239,0,208,12,0,0,212,12,0,0,174,2,4,2,9,0,7,0,213,12,7,0,214,12,7,0,215,12,7,0,148,2,7,0,216,12,
7,0,217,12,7,0,218,12,2,0,219,12,2,0,220,12,5,2,8,0,2,0,221,12,2,0,222,12,2,0,223,12,2,0,224,12,7,0,225,12,7,0,226,12,7,0,227,12,7,0,228,12,6,2,2,0,7,0,5,0,7,0,6,0,7,2,2,0,0,0,203,0,0,0,229,12,8,2,1,0,0,0,19,0,9,2,12,0,0,0,230,12,0,0,231,12,0,0,232,7,0,0,232,12,2,0,118,0,2,0,233,12,7,0,109,0,7,0,234,12,
7,0,235,12,7,0,226,3,7,0,124,7,7,0,22,4,10,2,2,0,11,0,236,12,11,0,237,12,11,2,14,0,0,0,238,5,0,0,16,0,0,0,60,3,0,0,63,3,0,0,231,12,0,0,106,0,0,0,238,12,0,0,239,12,7,0,240,12,7,0,241,12,7,0,225,3,7,0,242,12,7,0,243,12,7,0,22,4,12,2,8,0,7,0,245,10,7,0,148,0,7,0,246,10,7,0,230,2,7,0,244,12,7,0,7,1,7,0,245,12,
4,0,16,0,13,2,4,0,2,0,246,12,2,0,247,12,2,0,248,12,2,0,70,0,14,2,8,0,7,0,249,12,7,0,25,3,7,0,250,12,7,0,251,12,0,0,252,12,7,0,150,10,7,0,151,10,7,0,152,10,15,2,6,0,2,0,253,12,2,0,254,12,7,0,255,12,7,0,0,13,7,0,1,13,7,0,2,13,16,2,2,0,0,0,75,3,0,0,122,0,17,2,2,0,4,0,230,10,4,0,231,10,18,2,2,0,67,0,3,13,
68,0,4,13,19,2,5,0,18,2,88,7,4,0,5,13,7,0,6,13,7,0,7,13,7,0,8,13,20,2,8,0,18,2,88,7,44,0,101,1,4,0,9,13,4,0,10,13,7,0,11,13,4,0,225,10,4,0,12,13,4,0,70,0,21,2,1,0,18,2,88,7,22,2,5,0,18,2,88,7,4,0,13,13,4,0,14,13,7,0,148,0,7,0,15,13,23,2,6,0,18,2,88,7,44,0,101,1,4,0,9,13,4,0,10,13,4,0,225,10,
4,0,70,0,24,2,3,0,18,2,88,7,4,0,16,13,4,0,70,0,25,2,1,0,18,2,88,7,26,2,3,0,18,2,88,7,4,0,17,13,4,0,70,0,27,2,3,0,18,2,88,7,4,0,18,13,4,0,70,0,28,2,3,0,18,2,88,7,4,0,19,13,4,0,20,13,29,2,3,0,18,2,88,7,4,0,22,1,4,0,70,0,30,2,1,0,0,0,19,0,31,2,4,0,4,0,16,0,4,0,21,13,4,0,22,13,4,0,70,0,
32,2,14,0,18,2,88,7,2,0,23,13,2,0,70,0,4,0,24,13,7,0,27,1,4,0,176,3,2,0,51,3,2,0,225,10,2,0,29,1,2,0,30,1,0,0,35,1,62,0,25,13,4,0,26,13,4,0,87,0,33,2,1,0,0,0,19,0,34,2,1,0,0,0,27,13,35,2,12,0,7,0,28,13,7,0,29,13,7,0,30,13,4,0,31,13,7,0,32,13,7,0,33,13,7,0,34,13,4,0,35,13,4,0,36,13,4,0,37,13,
4,0,38,13,4,0,39,13,36,2,2,0,0,0,27,13,0,0,40,13,37,2,3,0,0,0,41,13,0,0,73,0,0,0,15,3,38,2,6,0,0,0,27,13,0,0,42,13,0,0,18,0,0,0,43,13,0,0,47,0,7,0,44,13,39,2,5,0,4,0,118,1,4,0,18,0,0,0,35,0,0,0,45,13,0,0,46,13,40,2,3,0,4,0,47,13,4,0,31,3,0,0,48,13,41,2,2,0,4,0,51,3,0,0,48,13,42,2,1,0,
0,0,48,13,43,2,2,0,7,0,49,13,7,0,50,13,44,2,4,0,7,0,5,0,7,0,6,0,2,0,18,0,2,0,51,13,45,2,12,0,2,0,218,4,2,0,18,0,7,0,115,5,7,0,52,13,7,0,53,13,7,0,54,13,7,0,55,13,44,2,56,13,44,2,57,13,44,2,58,13,7,0,59,13,7,0,60,13,63,0,11,0,4,0,18,0,4,0,67,0,4,0,61,13,4,0,62,13,19,0,63,13,19,0,64,13,45,2,65,13,
7,0,66,13,7,0,67,13,7,0,68,13,7,0,69,13,62,1,13,0,4,0,254,11,4,0,70,13,7,0,71,13,7,0,72,13,7,0,73,13,7,0,74,13,7,0,75,13,7,0,9,0,7,0,11,0,2,0,118,1,2,0,18,0,4,0,71,3,7,0,76,13,61,1,18,0,4,0,152,0,4,0,77,13,4,0,78,13,7,0,79,13,4,0,80,13,7,0,81,13,7,0,82,13,4,0,83,13,7,0,84,13,4,0,85,13,7,0,86,13,
62,1,87,13,7,0,88,13,7,0,89,13,7,0,90,13,7,0,91,13,4,0,92,13,4,0,70,0,240,0,8,0,4,0,18,0,4,0,70,0,0,0,93,13,0,0,94,13,7,0,113,5,7,0,246,10,63,0,95,13,11,0,96,13,241,0,1,0,0,0,97,13,54,0,1,0,0,0,19,0,46,2,4,0,51,0,53,3,7,0,98,13,7,0,217,1,7,0,70,0,251,0,57,0,22,0,32,0,46,2,99,13,63,0,56,13,56,0,100,13,
56,0,101,13,251,0,102,13,61,0,103,13,26,0,177,0,59,0,104,13,47,2,105,13,0,0,106,13,7,0,107,13,7,0,108,13,2,0,123,7,2,0,109,13,7,0,91,2,4,0,106,0,4,0,18,0,4,0,110,13,7,0,111,13,4,0,112,13,4,0,113,13,4,0,145,2,4,0,114,13,7,0,115,13,7,0,116,13,7,0,197,6,7,0,217,1,7,0,198,6,4,0,117,13,7,0,118,13,4,0,119,13,4,0,120,13,4,0,121,13,
0,0,122,13,0,0,123,13,0,0,124,13,0,0,125,13,7,0,126,13,7,0,127,13,7,0,128,13,7,0,71,3,7,0,129,13,4,0,130,13,4,0,131,13,4,0,132,13,7,0,196,6,7,0,133,13,4,0,134,13,4,0,135,13,7,0,136,13,7,0,137,13,7,0,138,13,7,0,139,13,7,0,140,13,7,0,141,13,7,0,142,13,48,2,4,0,48,2,0,0,48,2,1,0,7,0,197,6,7,0,36,3,252,0,4,0,22,0,32,0,
14,0,143,13,4,0,144,13,4,0,70,0,49,2,2,0,32,0,145,13,7,0,146,13,47,2,4,0,22,0,32,0,49,2,189,11,4,0,147,13,4,0,148,13,50,2,10,0,4,0,16,0,4,0,148,0,4,0,18,0,4,0,169,4,4,0,149,13,4,0,150,13,4,0,151,13,4,0,75,0,0,0,19,0,11,0,2,0,51,2,1,0,0,0,157,8,102,0,8,0,50,2,152,13,4,0,153,13,4,0,210,3,4,0,154,13,4,0,155,13,
4,0,156,13,37,1,157,13,51,2,158,13,52,2,6,0,7,0,224,2,7,0,48,3,7,0,91,2,2,0,206,2,2,0,70,0,7,0,159,13,53,2,5,0,7,0,224,2,7,0,59,5,7,0,160,13,7,0,161,13,7,0,48,3,54,2,5,0,29,0,162,13,55,2,21,0,7,0,80,7,7,0,163,13,7,0,60,0,56,2,3,0,7,0,164,13,4,0,165,13,4,0,166,13,57,2,7,0,4,0,167,13,4,0,168,13,4,0,169,13,
7,0,170,13,7,0,171,13,7,0,172,13,7,0,60,0,58,2,8,0,58,2,0,0,58,2,1,0,29,0,48,0,4,0,32,1,2,0,18,0,2,0,118,1,7,0,48,3,7,0,45,11,59,2,7,0,59,2,0,0,59,2,1,0,29,0,48,0,2,0,30,3,2,0,18,0,2,0,201,1,2,0,60,0,60,2,19,0,53,2,157,4,53,2,173,13,52,2,174,13,53,2,28,11,54,2,175,13,4,0,86,0,7,0,48,3,7,0,77,3,
7,0,176,13,4,0,167,13,4,0,177,13,7,0,171,13,7,0,172,13,7,0,106,0,7,0,178,13,4,0,70,0,4,0,179,13,2,0,18,0,2,0,180,13,61,2,17,0,7,0,27,1,7,0,181,13,7,0,164,13,7,0,182,13,7,0,183,13,7,0,184,13,7,0,185,13,7,0,186,13,7,0,187,13,7,0,188,13,7,0,189,13,7,0,190,13,7,0,191,13,4,0,18,0,4,0,192,13,2,0,116,3,2,0,117,1,62,2,143,0,
22,0,32,0,36,0,79,0,63,2,193,13,61,2,194,13,220,0,54,5,74,0,53,5,4,0,18,0,4,0,60,0,2,0,16,0,2,0,59,12,2,0,195,13,2,0,158,1,2,0,196,13,2,0,124,4,2,0,197,13,2,0,198,13,4,0,199,13,4,0,72,0,2,0,200,13,2,0,201,13,2,0,202,13,2,0,87,0,2,0,203,13,2,0,204,13,2,0,205,13,2,0,180,6,2,0,206,13,2,0,207,13,2,0,208,13,2,0,209,13,
2,0,210,13,2,0,80,2,2,0,21,11,2,0,225,10,2,0,211,13,2,0,212,13,2,0,180,4,2,0,181,4,2,0,213,13,2,0,214,13,2,0,215,13,2,0,216,13,7,0,217,13,7,0,218,13,7,0,219,13,7,0,220,13,7,0,221,13,7,0,222,13,7,0,223,13,2,0,82,6,2,0,224,13,7,0,225,13,7,0,226,13,7,0,227,13,7,0,2,11,7,0,3,11,7,0,77,3,7,0,9,11,7,0,228,13,7,0,229,13,
7,0,230,13,7,0,231,13,7,0,232,13,7,0,233,13,4,0,4,11,4,0,1,11,4,0,234,13,4,0,235,13,2,0,236,13,2,0,237,13,7,0,5,11,7,0,6,11,7,0,7,11,7,0,238,13,7,0,239,13,7,0,240,13,7,0,241,13,7,0,242,13,7,0,243,13,7,0,244,13,7,0,245,13,7,0,246,13,7,0,106,4,7,0,106,0,7,0,247,13,7,0,248,13,7,0,249,13,7,0,250,13,7,0,249,0,7,0,251,13,
4,0,252,13,4,0,32,2,4,0,253,13,4,0,254,13,7,0,255,13,7,0,0,14,7,0,1,14,7,0,2,14,7,0,3,14,7,0,248,0,7,0,4,14,7,0,207,4,7,0,205,4,7,0,206,4,7,0,5,14,7,0,6,14,4,0,7,14,4,0,40,6,7,0,8,14,7,0,9,14,7,0,10,14,7,0,11,14,7,0,12,14,7,0,13,14,7,0,14,14,7,0,15,14,7,0,16,14,7,0,17,14,7,0,18,14,7,0,19,14,
7,0,20,14,7,0,21,14,7,0,22,14,7,0,23,14,7,0,24,14,7,0,25,14,4,0,26,14,4,0,27,14,63,0,28,14,63,0,29,14,7,0,30,14,7,0,31,14,56,0,177,1,74,0,148,4,14,0,32,14,74,0,33,14,29,0,34,14,29,0,35,14,33,0,84,0,212,0,104,1,212,0,36,14,2,0,37,14,2,0,38,14,163,0,51,0,163,0,0,0,163,0,1,0,62,2,39,14,60,2,40,14,57,2,215,11,223,0,235,4,
11,0,236,4,64,2,41,14,64,2,42,14,14,0,43,14,14,0,44,14,145,0,45,14,156,0,46,14,156,0,47,14,29,0,48,14,65,2,49,14,29,0,36,0,14,0,23,12,0,0,19,0,7,0,16,1,7,0,113,3,7,0,50,14,7,0,51,14,4,0,19,3,4,0,52,14,4,0,18,0,4,0,4,11,4,0,53,14,4,0,54,14,4,0,55,14,4,0,56,14,2,0,23,1,2,0,57,14,2,0,58,14,2,0,59,14,0,0,60,14,
2,0,61,14,2,0,62,14,2,0,63,14,66,2,64,14,149,0,52,5,14,0,91,3,14,0,65,14,56,2,66,14,4,0,67,14,4,0,68,14,67,2,69,14,154,0,110,3,68,2,70,14,7,0,71,14,7,0,72,14,147,0,48,0,69,2,159,0,7,0,22,5,7,0,73,14,7,0,74,14,7,0,80,7,7,0,127,4,7,0,106,4,7,0,75,14,7,0,147,2,7,0,76,14,7,0,77,14,7,0,78,14,7,0,79,14,7,0,80,14,
7,0,81,14,7,0,82,14,7,0,83,14,7,0,23,5,7,0,84,14,7,0,85,14,7,0,86,14,7,0,24,5,7,0,20,5,7,0,21,5,7,0,87,14,7,0,88,14,7,0,89,14,7,0,90,14,7,0,91,14,7,0,92,14,7,0,93,14,7,0,94,14,7,0,95,14,4,0,70,0,4,0,96,14,4,0,94,0,4,0,97,14,4,0,98,14,2,0,99,14,2,0,100,14,2,0,101,14,2,0,102,14,2,0,103,14,2,0,104,14,
2,0,105,14,2,0,106,14,0,0,213,11,220,0,54,5,148,0,15,0,69,2,107,14,7,0,108,14,7,0,109,14,7,0,36,2,7,0,107,4,7,0,110,14,7,0,111,14,7,0,112,14,4,0,94,0,2,0,113,14,2,0,114,14,4,0,70,0,74,0,35,2,2,0,115,14,2,0,121,4,70,2,7,0,7,0,5,0,7,0,6,0,7,0,7,0,7,0,146,13,7,0,13,3,7,0,48,3,4,0,18,0,71,2,3,0,4,0,214,2,
4,0,215,2,4,0,216,2,72,2,18,0,72,2,0,0,72,2,1,0,0,0,231,4,2,0,116,14,2,0,18,0,7,0,117,14,2,0,118,14,2,0,119,14,7,0,120,14,7,0,121,14,7,0,122,14,7,0,123,14,7,0,124,14,7,0,125,14,7,0,126,14,63,0,127,14,63,0,128,14,63,0,129,14,73,2,7,0,73,2,0,0,73,2,1,0,0,0,231,4,7,0,234,10,7,0,130,14,2,0,18,0,0,0,15,3,74,2,6,0,
74,2,0,0,74,2,1,0,14,0,143,13,0,0,231,4,2,0,18,0,0,0,15,3,75,2,13,0,75,2,0,0,75,2,1,0,70,2,189,11,71,2,131,14,4,0,31,1,4,0,132,14,2,0,116,14,2,0,18,0,2,0,47,0,8,0,133,14,0,0,134,14,73,2,135,14,0,0,136,14,76,2,6,0,76,2,0,0,76,2,1,0,14,0,137,14,4,0,138,14,2,0,18,0,2,0,139,14,77,2,20,0,77,2,0,0,77,2,1,0,
14,0,147,0,76,2,140,14,2,0,18,0,2,0,116,14,2,0,141,14,2,0,142,14,7,0,143,14,7,0,144,14,7,0,234,10,7,0,130,14,0,0,145,14,29,0,36,0,7,0,146,14,0,0,60,4,2,0,56,4,2,0,70,0,7,0,147,14,7,0,148,14,208,0,11,0,22,0,32,0,36,0,79,0,14,0,48,6,4,0,18,0,2,0,149,14,2,0,150,14,11,0,151,14,7,0,152,14,0,0,15,3,2,0,153,14,14,0,154,14,
78,2,6,0,14,0,155,14,4,0,156,14,4,0,157,14,4,0,18,0,4,0,70,0,28,1,158,14,79,2,21,0,22,0,32,0,80,2,159,14,80,2,160,14,14,0,161,14,4,0,162,14,2,0,163,14,2,0,164,14,14,0,165,14,14,0,166,14,78,2,167,14,14,0,168,14,14,0,169,14,14,0,170,14,14,0,171,14,81,2,172,14,81,2,173,14,81,2,174,14,14,0,175,14,28,1,176,14,0,0,177,14,0,0,178,14,80,2,36,0,
80,2,0,0,80,2,1,0,11,0,179,14,92,1,180,14,92,1,181,14,0,0,182,14,2,0,183,14,2,0,184,14,2,0,87,10,2,0,88,10,2,0,185,14,2,0,186,14,2,0,169,4,2,0,140,8,2,0,187,14,2,0,188,14,2,0,189,14,2,0,190,14,2,0,191,14,2,0,117,1,4,0,62,10,2,0,192,14,2,0,193,14,82,2,194,14,83,2,195,14,84,2,196,14,85,2,197,14,4,0,198,14,4,0,199,14,14,0,200,14,
14,0,166,14,14,0,115,10,14,0,201,14,14,0,202,14,14,0,203,14,55,0,195,0,86,2,18,0,86,2,0,0,86,2,1,0,0,0,96,10,21,0,31,0,0,0,204,14,2,0,205,14,2,0,16,0,2,0,14,0,2,0,206,14,2,0,207,14,2,0,208,14,2,0,209,14,2,0,210,14,2,0,18,0,2,0,211,14,2,0,32,0,2,0,70,0,87,2,212,14,88,2,4,0,88,2,0,0,88,2,1,0,86,2,213,14,86,2,214,14,
89,2,11,0,89,2,0,0,89,2,1,0,14,0,215,14,14,0,216,14,0,0,96,10,2,0,217,14,2,0,218,14,2,0,18,0,2,0,219,14,4,0,220,14,11,0,221,14,81,2,7,0,81,2,0,0,81,2,1,0,0,0,96,10,0,0,222,14,14,0,237,9,4,0,223,14,4,0,18,0,51,1,14,0,51,1,0,0,51,1,1,0,0,0,96,10,21,0,31,0,90,2,81,10,11,0,224,14,11,0,225,14,87,2,212,14,78,2,226,14,
14,0,227,14,51,1,228,14,97,1,39,8,2,0,18,0,2,0,117,1,91,2,11,0,91,2,0,0,91,2,1,0,11,0,2,0,0,0,19,0,2,0,16,0,2,0,18,0,7,0,30,4,7,0,149,0,7,0,31,5,7,0,76,11,7,0,91,12,92,2,5,0,7,0,229,14,4,0,230,14,4,0,231,14,4,0,118,1,4,0,18,0,93,2,6,0,7,0,100,2,7,0,232,14,7,0,233,14,7,0,234,14,4,0,16,0,4,0,18,0,
94,2,5,0,7,0,104,11,7,0,105,11,7,0,48,3,2,0,94,2,2,0,95,2,95,2,5,0,94,2,2,0,4,0,57,0,7,0,235,14,7,0,104,11,7,0,105,11,96,2,4,0,2,0,236,14,2,0,237,14,2,0,238,14,2,0,239,14,97,2,2,0,38,0,123,8,21,0,229,5,98,2,3,0,19,0,240,14,4,0,18,0,4,0,70,0,99,2,6,0,7,0,106,0,7,0,13,3,7,0,241,14,7,0,148,0,2,0,22,1,
2,0,242,14,100,2,5,0,7,0,243,14,7,0,148,0,7,0,190,11,7,0,191,11,4,0,18,0,101,2,6,0,22,0,128,8,0,0,244,14,0,0,245,14,2,0,246,14,2,0,18,0,4,0,247,14,102,2,8,0,102,2,0,0,102,2,1,0,0,0,19,0,101,2,248,14,0,0,249,14,0,0,16,0,2,0,18,0,7,0,64,0,103,2,7,0,14,0,250,14,0,0,251,14,11,0,252,14,7,0,64,0,7,0,30,4,4,0,16,0,
4,0,18,0,104,2,3,0,7,0,253,14,4,0,18,0,4,0,70,0,105,2,17,0,105,2,0,0,105,2,1,0,189,1,11,12,103,2,65,0,14,0,74,4,32,0,53,0,104,2,254,14,4,0,57,0,7,0,64,0,2,0,18,0,2,0,51,1,4,0,255,14,0,0,244,14,4,0,11,1,7,0,115,0,7,0,0,15,7,0,1,15,106,2,2,0,0,0,2,15,0,0,3,15,107,2,4,0,107,2,0,0,107,2,1,0,206,0,151,3,
14,0,4,15,108,2,25,0,108,2,0,0,108,2,1,0,14,0,5,15,206,0,74,11,107,2,6,15,14,0,7,15,14,0,74,4,0,0,19,0,7,0,30,4,7,0,8,15,7,0,16,3,7,0,3,11,7,0,87,12,7,0,88,12,7,0,67,3,7,0,226,3,7,0,76,11,7,0,91,12,2,0,9,15,2,0,10,15,2,0,72,0,2,0,16,0,11,0,11,15,4,0,18,0,4,0,87,0,109,2,6,0,109,2,0,0,109,2,1,0,
14,0,5,15,4,0,18,0,4,0,201,1,0,0,19,0,110,2,11,0,110,2,0,0,110,2,1,0,22,0,128,8,0,0,12,15,4,0,247,14,2,0,13,15,2,0,18,0,0,0,244,14,4,0,255,14,2,0,14,15,2,0,15,15,111,2,12,0,111,2,0,0,111,2,1,0,14,0,16,15,0,0,96,10,0,0,19,0,0,0,17,15,0,0,18,15,4,0,19,15,2,0,18,0,2,0,14,15,2,0,15,15,0,0,15,3,112,2,5,0,
112,2,0,0,112,2,1,0,0,0,244,14,4,0,255,14,7,0,36,3,36,0,13,0,206,0,65,4,206,0,20,15,107,2,6,15,14,0,21,15,109,2,22,15,108,2,23,15,14,0,24,15,14,0,25,15,4,0,18,0,4,0,23,1,2,0,26,15,2,0,27,15,7,0,28,15,113,2,2,0,22,0,32,0,36,0,79,0,114,2,5,0,114,2,0,0,114,2,1,0,4,0,16,0,4,0,18,0,0,0,111,6,115,2,6,0,114,2,29,15,
29,0,48,0,4,0,30,15,7,0,31,15,4,0,32,15,4,0,254,11,116,2,3,0,114,2,29,15,4,0,30,15,7,0,33,15,117,2,8,0,114,2,29,15,29,0,48,0,7,0,108,1,7,0,34,15,7,0,113,3,7,0,53,4,4,0,30,15,4,0,35,15,118,2,5,0,114,2,29,15,7,0,36,15,7,0,235,2,7,0,73,3,7,0,60,0,119,2,3,0,114,2,29,15,7,0,53,4,7,0,37,15,55,2,4,0,7,0,38,15,
7,0,248,13,2,0,39,15,2,0,118,1,120,2,14,0,120,2,0,0,120,2,1,0,14,0,40,15,14,0,41,15,14,0,42,15,0,0,111,6,4,0,32,0,4,0,18,0,4,0,43,15,7,0,44,15,4,0,32,15,4,0,254,11,7,0,239,4,7,0,75,3,63,2,23,0,4,0,30,15,4,0,45,15,7,0,46,15,7,0,71,3,7,0,47,15,7,0,189,10,7,0,38,15,7,0,48,15,7,0,13,3,7,0,79,13,7,0,115,5,
7,0,49,15,7,0,50,15,7,0,51,15,7,0,52,15,7,0,53,15,7,0,54,15,7,0,55,15,7,0,56,15,7,0,57,15,7,0,58,15,7,0,59,15,14,0,60,15,133,0,81,0,132,0,61,15,121,2,194,13,11,0,62,15,74,0,63,15,74,0,33,14,74,0,64,15,122,2,65,15,27,0,202,0,27,0,66,15,27,0,67,15,27,0,68,15,7,0,69,15,7,0,70,15,7,0,71,15,7,0,72,15,7,0,73,15,7,0,74,15,
7,0,75,15,4,0,76,15,7,0,77,15,7,0,78,15,7,0,16,1,7,0,88,4,7,0,79,15,7,0,80,15,4,0,81,15,4,0,82,15,4,0,83,15,4,0,84,15,4,0,85,15,7,0,86,15,7,0,226,3,4,0,87,15,4,0,88,15,7,0,89,15,7,0,217,1,7,0,90,15,4,0,91,15,4,0,92,15,4,0,94,0,4,0,93,15,2,0,94,15,2,0,95,15,4,0,96,15,7,0,13,3,4,0,97,15,7,0,98,15,
4,0,99,15,4,0,100,15,4,0,101,15,0,0,102,15,0,0,103,15,0,0,47,0,149,0,104,15,14,0,105,15,220,0,54,5,4,0,106,15,7,0,83,14,7,0,107,15,4,0,108,15,7,0,109,15,4,0,110,15,7,0,111,15,7,0,112,15,7,0,113,15,7,0,114,15,7,0,115,15,7,0,116,15,0,0,117,15,0,0,118,15,0,0,119,15,0,0,120,15,7,0,121,15,7,0,122,15,7,0,123,15,59,0,46,1,7,0,124,15,
0,0,125,15,0,0,126,15,0,0,127,15,0,0,87,0,134,0,26,0,132,0,61,15,156,0,112,3,163,0,135,3,57,0,128,15,7,0,129,15,4,0,114,3,7,0,130,15,7,0,131,15,7,0,132,15,7,0,180,1,7,0,115,0,7,0,133,15,7,0,64,10,7,0,134,15,7,0,135,15,7,0,136,15,4,0,204,13,7,0,137,15,7,0,138,15,4,0,70,0,0,0,5,3,2,0,139,15,2,0,16,0,2,0,165,0,2,0,140,15,
4,0,94,0,135,0,6,0,132,0,61,15,156,0,112,3,7,0,129,15,4,0,114,3,2,0,16,0,2,0,70,0,123,2,15,0,22,0,32,0,36,0,79,0,116,1,187,10,7,0,141,15,7,0,142,15,7,0,116,0,7,0,143,15,7,0,117,0,7,0,144,15,7,0,145,15,7,0,146,15,7,0,239,4,7,0,189,10,2,0,18,0,2,0,42,1,23,1,3,0,4,0,146,0,2,0,0,8,2,0,147,15,124,2,5,0,0,0,153,10,
2,0,154,10,2,0,238,5,2,0,148,15,2,0,149,15,20,1,19,0,22,0,32,0,36,0,79,0,0,0,34,0,4,0,165,0,4,0,166,0,4,0,150,15,7,0,187,0,7,0,188,0,47,0,158,0,125,2,159,0,208,0,68,4,126,2,151,15,11,0,152,15,124,2,153,15,4,0,18,0,4,0,22,0,4,0,190,11,4,0,154,15,54,0,189,0,75,1,15,0,2,0,152,0,2,0,155,15,4,0,156,15,4,0,157,15,4,0,158,15,
127,2,159,15,61,0,160,15,61,0,161,15,7,0,162,15,2,0,163,15,2,0,164,15,4,0,146,0,128,2,61,4,127,2,165,15,7,0,166,15,129,2,3,0,4,0,146,0,7,0,167,15,7,0,110,1,130,2,14,0,11,0,168,15,2,0,169,15,2,0,70,0,7,0,170,15,7,0,171,15,7,0,172,15,2,0,173,15,2,0,72,0,7,0,174,15,7,0,175,15,7,0,176,15,7,0,177,15,7,0,178,15,7,0,179,15,127,2,6,0,
7,0,180,15,7,0,181,15,7,0,182,15,7,0,183,15,4,0,146,0,4,0,18,0,128,2,26,0,128,2,0,0,128,2,1,0,0,0,19,0,7,0,184,15,7,0,185,15,7,0,182,15,7,0,183,15,7,0,98,13,4,0,186,15,4,0,187,15,127,2,188,15,7,0,189,15,7,0,167,15,4,0,18,0,4,0,190,15,4,0,191,15,7,0,115,0,2,0,192,15,2,0,111,4,2,0,193,15,2,0,194,15,4,0,195,15,7,0,196,15,
208,0,68,4,7,0,91,2,7,0,197,15,131,2,3,0,7,0,198,15,4,0,146,0,4,0,18,0,132,2,12,0,132,2,0,0,132,2,1,0,0,0,19,0,128,2,199,15,4,0,200,15,4,0,70,0,131,2,188,15,4,0,186,15,4,0,18,0,51,0,53,3,7,0,201,15,4,0,187,15,133,2,24,0,4,0,18,0,2,0,202,15,2,0,203,15,7,0,204,15,2,0,205,15,2,0,206,15,2,0,207,15,2,0,208,15,2,0,209,15,
2,0,210,15,7,0,199,3,2,0,211,15,2,0,73,3,4,0,212,15,4,0,213,15,4,0,214,15,2,0,215,15,2,0,87,0,7,0,125,1,4,0,216,15,4,0,217,15,7,0,218,15,7,0,219,15,4,0,32,2,134,2,16,0,4,0,18,0,4,0,220,15,4,0,221,15,4,0,222,15,4,0,223,15,7,0,224,15,128,2,225,15,4,0,226,15,7,0,227,15,7,0,228,15,7,0,226,3,7,0,229,15,7,0,230,15,7,0,231,15,
4,0,229,1,4,0,152,0,135,2,5,0,4,0,18,0,7,0,167,15,4,0,232,15,4,0,233,15,129,2,234,15,136,2,10,0,136,2,0,0,136,2,1,0,0,0,19,0,4,0,18,0,7,0,226,3,14,0,235,15,14,0,236,15,135,2,237,15,4,0,212,15,4,0,213,15,137,2,1,0,0,0,238,15,138,2,9,0,138,2,0,0,138,2,1,0,128,2,61,4,4,0,70,0,0,0,19,0,4,0,239,15,4,0,240,15,4,0,241,15,
4,0,242,15,139,2,6,0,139,2,0,0,139,2,1,0,4,0,243,15,4,0,190,11,4,0,191,11,4,0,70,0,140,2,7,0,4,0,152,0,2,0,244,15,2,0,18,0,14,0,245,15,14,0,254,11,4,0,246,15,4,0,70,0,126,2,13,0,133,2,247,15,130,2,248,15,14,0,235,15,14,0,236,15,135,2,237,15,134,2,249,15,128,2,22,15,132,2,250,15,14,0,251,15,4,0,252,15,4,0,253,15,137,2,99,7,140,2,254,15,
141,2,50,0,141,2,0,0,141,2,1,0,182,0,223,3,142,2,2,0,74,0,255,15,220,0,54,5,149,0,52,5,14,0,91,3,4,0,0,16,0,0,19,0,2,0,208,12,2,0,16,0,2,0,1,16,2,0,2,16,2,0,3,16,2,0,4,16,2,0,5,16,2,0,6,16,4,0,94,0,4,0,72,4,4,0,7,16,4,0,8,16,4,0,190,11,4,0,191,11,4,0,70,0,7,0,9,16,57,0,10,16,0,0,11,16,4,0,12,16,
4,0,96,15,7,0,13,16,7,0,14,16,7,0,15,16,7,0,16,16,7,0,17,16,7,0,18,16,7,0,19,16,7,0,20,16,7,0,21,16,7,0,22,16,7,0,23,16,7,0,24,16,7,0,25,16,7,0,26,16,7,0,27,16,4,0,87,0,0,0,5,3,0,0,28,16,0,0,29,16,0,0,30,16,182,0,7,0,181,0,31,16,156,0,112,3,14,0,32,16,2,0,33,16,2,0,94,0,4,0,70,0,0,0,34,16,183,0,24,0,
181,0,31,16,156,0,112,3,163,0,135,3,73,0,77,2,4,0,94,0,4,0,35,16,7,0,220,0,7,0,221,0,7,0,222,0,7,0,217,1,7,0,36,16,7,0,37,16,7,0,38,16,7,0,39,16,59,0,40,16,59,0,41,16,2,0,42,16,2,0,19,13,2,0,43,16,2,0,70,0,7,0,44,16,7,0,45,16,7,0,46,16,7,0,47,16,47,1,9,0,22,0,32,0,36,0,79,0,14,0,48,16,4,0,49,16,4,0,50,16,
4,0,149,0,4,0,31,5,4,0,18,0,4,0,70,0,143,2,7,0,4,0,51,16,4,0,16,0,22,0,128,8,0,0,52,16,0,0,53,16,7,0,54,16,7,0,55,16,144,2,3,0,7,0,56,16,7,0,119,2,4,0,18,0,145,2,5,0,32,0,57,16,4,0,70,0,4,0,58,16,144,2,59,16,143,2,168,13,146,2,9,0,146,2,0,0,146,2,1,0,2,0,18,0,0,0,60,16,0,0,61,16,4,0,62,16,145,2,189,11,
143,2,168,13,145,2,63,16,147,2,7,0,147,2,0,0,147,2,1,0,7,0,2,0,4,0,64,16,4,0,217,4,0,0,18,0,0,0,122,0,148,2,14,0,148,2,0,0,148,2,1,0,0,0,19,0,14,0,65,16,14,0,66,16,146,2,67,16,145,2,68,16,7,0,217,1,0,0,123,7,0,0,69,16,0,0,75,3,0,0,122,0,0,0,18,0,0,0,140,4,21,1,14,0,220,0,54,5,74,0,35,2,29,0,70,16,74,0,71,16,
4,0,70,0,7,0,72,16,149,0,52,5,14,0,91,3,4,0,73,16,2,0,74,16,2,0,75,16,4,0,18,0,7,0,83,14,11,0,76,16,215,0,19,0,11,0,77,16,11,0,78,16,2,0,16,0,2,0,178,4,4,0,18,0,4,0,79,16,2,0,80,16,2,0,70,0,7,0,106,4,7,0,36,2,7,0,81,16,7,0,111,4,7,0,82,16,7,0,83,16,7,0,84,16,7,0,85,16,7,0,86,16,7,0,87,16,7,0,72,0,
216,0,36,0,29,0,88,16,29,0,89,16,2,0,16,0,2,0,75,16,4,0,18,0,7,0,90,16,7,0,70,0,7,0,91,16,7,0,92,16,7,0,93,16,7,0,94,16,7,0,95,16,7,0,96,16,7,0,97,16,7,0,98,16,7,0,99,16,7,0,100,16,7,0,101,16,7,0,102,16,7,0,103,16,7,0,104,16,7,0,105,16,7,0,106,16,7,0,107,16,7,0,108,16,7,0,109,16,7,0,110,16,7,0,111,16,7,0,112,16,
7,0,113,16,7,0,114,16,7,0,115,16,7,0,116,16,7,0,117,16,7,0,118,16,11,0,119,16,149,2,14,0,149,2,0,0,149,2,1,0,0,0,19,0,4,0,94,0,4,0,120,16,2,0,121,16,2,0,72,0,4,0,122,16,4,0,123,16,4,0,124,16,4,0,125,16,4,0,87,0,74,0,35,2,150,2,126,16,151,2,5,0,151,2,0,0,151,2,1,0,38,0,123,8,2,0,127,16,2,0,117,1,237,0,8,0,14,0,128,16,
4,0,118,1,4,0,129,16,4,0,94,0,7,0,130,16,7,0,131,16,7,0,132,16,14,0,133,16,152,2,7,0,152,2,0,0,152,2,1,0,0,0,19,0,4,0,16,0,7,0,30,4,4,0,94,0,4,0,123,7,153,2,2,0,152,2,2,3,59,0,134,16,154,2,4,0,152,2,2,3,63,0,56,13,4,0,94,0,4,0,70,0,155,2,6,0,152,2,2,3,63,0,56,13,4,0,94,0,7,0,135,16,7,0,136,16,4,0,70,0,
156,2,4,0,152,2,2,3,59,0,134,16,7,0,137,16,7,0,138,16,157,2,6,0,152,2,2,3,63,0,56,13,4,0,94,0,7,0,137,16,7,0,138,16,4,0,70,0,158,2,8,0,152,2,2,3,63,0,56,13,4,0,94,0,7,0,137,16,7,0,138,16,7,0,135,16,7,0,136,16,4,0,70,0,159,2,5,0,152,2,2,3,29,0,151,3,59,0,134,16,7,0,137,16,7,0,138,16,160,2,7,0,152,2,2,3,29,0,151,3,
63,0,56,13,4,0,94,0,7,0,137,16,7,0,138,16,4,0,70,0,161,2,9,0,152,2,2,3,29,0,151,3,63,0,56,13,4,0,94,0,7,0,137,16,7,0,138,16,7,0,135,16,7,0,136,16,4,0,70,0,162,2,6,0,152,2,2,3,7,0,139,16,7,0,140,16,59,0,134,16,7,0,137,16,7,0,138,16,163,2,6,0,152,2,2,3,63,0,56,13,4,0,94,0,7,0,139,16,7,0,140,16,4,0,70,0,164,2,8,0,
152,2,2,3,63,0,56,13,4,0,94,0,4,0,70,0,7,0,139,16,7,0,140,16,7,0,141,16,7,0,142,16,165,2,6,0,152,2,2,3,59,0,134,16,7,0,101,2,7,0,100,2,4,0,19,3,4,0,70,0,166,2,6,0,152,2,2,3,63,0,56,13,4,0,94,0,7,0,101,2,7,0,100,2,4,0,19,3,167,2,5,0,152,2,2,3,7,0,101,2,7,0,100,2,4,0,94,0,4,0,19,3,168,2,4,0,152,2,2,3,
59,0,134,16,7,0,143,16,7,0,144,16,169,2,6,0,152,2,2,3,63,0,56,13,4,0,94,0,7,0,143,16,7,0,144,16,4,0,70,0,170,2,8,0,152,2,2,3,63,0,56,13,4,0,94,0,4,0,70,0,7,0,143,16,7,0,144,16,7,0,141,16,7,0,142,16,171,2,2,0,152,2,2,3,59,0,134,16,172,2,4,0,152,2,2,3,63,0,56,13,4,0,94,0,4,0,70,0,173,2,6,0,152,2,2,3,63,0,56,13,
4,0,94,0,7,0,141,16,7,0,142,16,4,0,70,0,174,2,4,0,152,2,2,3,59,0,134,16,4,0,94,0,4,0,145,16,175,2,4,0,152,2,2,3,63,0,56,13,4,0,94,0,4,0,145,16,176,2,6,0,152,2,2,3,63,0,56,13,4,0,94,0,7,0,135,16,7,0,136,16,4,0,145,16,177,2,3,0,152,2,2,3,7,0,146,16,4,0,70,0,178,2,3,0,152,2,2,3,7,0,167,15,4,0,70,0,179,2,5,0,
152,2,2,3,7,0,147,16,7,0,100,2,7,0,241,14,4,0,70,0,180,2,5,0,152,2,2,3,7,0,100,2,7,0,226,3,4,0,148,16,4,0,94,0,181,2,7,0,152,2,2,3,7,0,149,16,7,0,100,2,7,0,63,3,4,0,148,16,4,0,19,3,4,0,72,0,182,2,7,0,152,2,2,3,7,0,149,16,7,0,100,2,7,0,63,3,4,0,148,16,4,0,19,3,4,0,72,0,183,2,3,0,152,2,2,3,7,0,150,16,
4,0,70,0,184,2,3,0,152,2,2,3,7,0,151,16,4,0,70,0,185,2,3,0,152,2,2,3,7,0,167,15,4,0,70,0,186,2,3,0,152,2,2,3,7,0,148,0,4,0,70,0,187,2,7,0,152,2,2,3,4,0,94,0,4,0,152,16,7,0,150,16,4,0,153,16,4,0,154,16,4,0,155,16,188,2,5,0,152,2,2,3,7,0,16,3,7,0,3,11,7,0,5,0,7,0,6,0,189,2,9,0,152,2,2,3,4,0,126,0,
7,0,156,16,7,0,157,16,7,0,63,3,7,0,158,16,7,0,159,16,7,0,160,16,4,0,70,0,190,2,3,0,152,2,2,3,7,0,32,3,4,0,70,0,191,2,5,0,152,2,2,3,7,0,141,16,7,0,142,16,7,0,161,16,4,0,70,0,150,2,45,0,22,0,32,0,36,0,79,0,7,0,220,0,7,0,221,0,7,0,222,0,7,0,217,1,7,0,116,14,4,0,162,16,7,0,163,16,4,0,18,0,4,0,164,16,4,0,165,16,
4,0,152,16,7,0,166,16,7,0,143,16,7,0,144,16,7,0,167,16,7,0,168,16,4,0,169,16,2,0,170,16,2,0,171,16,2,0,172,16,2,0,173,16,2,0,174,16,2,0,175,16,4,0,176,16,4,0,177,16,7,0,178,16,2,0,158,1,2,0,178,1,2,0,107,1,2,0,117,1,2,0,179,16,2,0,180,16,2,0,181,16,2,0,182,16,2,0,183,16,2,0,184,16,4,0,228,8,56,0,177,1,66,0,102,1,14,0,185,16,
14,0,186,16,14,0,187,16,14,0,188,16,192,2,3,0,192,2,0,0,192,2,1,0,0,0,189,16,196,0,14,0,22,0,32,0,36,0,79,0,193,2,139,11,11,0,190,16,14,0,191,16,0,0,35,0,0,0,192,16,0,0,250,3,0,0,251,3,0,0,193,16,7,0,226,3,7,0,217,4,2,0,18,0,2,0,5,8};

int bfBlenderLen=sizeof(bfBlenderFBT);
//===============================================================================================

#endif //bftBlend_imp_
#endif //FBTBLEND_IMPLEMENTATION

