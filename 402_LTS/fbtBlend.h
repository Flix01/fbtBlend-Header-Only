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
    defined(__s390x__)	    || defined(__aarch64__)
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
	fbtList m_gpencilLegacy;
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
	fbtList m_gpencil;

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
	{ FBT_ID2('G', 'D'), &fbtBlend::m_gpencilLegacy},
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
	{ FBT_ID2('G', 'P'), &fbtBlend::m_gpencil},
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

// Generated using BLENDER-v402
unsigned char bfBlenderFBT[] = {
83,68,78,65,78,65,77,69,80,20,0,0,42,102,105,114,115,116,0,42,108,97,115,116,0,42,100,101,115,99,114,105,112,116,105,111,110,0,114,110,97,95,115,117,98,116,121,112,101,0,95,112,97,100,91,52,93,0,42,105,100,101,110,116,105,102,105,101,114,0,42,110,97,109,101,0,118,97,108,117,101,0,105,99,111,110,0,98,97,115,101,0,42,100,101,102,97,117,108,116,95,97,114,114,97,121,0,100,101,102,97,117,108,116,95,97,114,114,97,121,
95,108,101,110,0,109,105,110,0,109,97,120,0,115,111,102,116,95,109,105,110,0,115,111,102,116,95,109,97,120,0,115,116,101,112,0,100,101,102,97,117,108,116,95,118,97,108,117,101,0,101,110,117,109,95,105,116,101,109,115,95,110,117,109,0,42,101,110,117,109,95,105,116,101,109,115,0,95,112,97,100,91,51,93,0,112,114,101,99,105,115,105,111,110,0,42,100,101,102,97,117,108,116,95,118,97,108,117,101,0,105,100,95,116,121,112,101,0,95,112,
97,100,91,54,93,0,42,112,111,105,110,116,101,114,0,103,114,111,117,112,0,118,97,108,0,118,97,108,50,0,42,110,101,120,116,0,42,112,114,101,118,0,116,121,112,101,0,115,117,98,116,121,112,101,0,102,108,97,103,0,110,97,109,101,91,54,52,93,0,95,112,97,100,48,91,52,93,0,100,97,116,97,0,108,101,110,0,116,111,116,97,108,108,101,110,0,42,117,105,95,100,97,116,97,0,111,112,101,114,97,116,105,111,110,0,116,97,103,0,95,
112,97,100,48,91,50,93,0,42,115,117,98,105,116,101,109,95,114,101,102,101,114,101,110,99,101,95,110,97,109,101,0,42,115,117,98,105,116,101,109,95,108,111,99,97,108,95,110,97,109,101,0,115,117,98,105,116,101,109,95,114,101,102,101,114,101,110,99,101,95,105,110,100,101,120,0,115,117,98,105,116,101,109,95,108,111,99,97,108,95,105,110,100,101,120,0,42,115,117,98,105,116,101,109,95,114,101,102,101,114,101,110,99,101,95,105,100,0,42,115,
117,98,105,116,101,109,95,108,111,99,97,108,95,105,100,0,42,114,110,97,95,112,97,116,104,0,111,112,101,114,97,116,105,111,110,115,0,95,112,97,100,91,50,93,0,114,110,97,95,112,114,111,112,95,116,121,112,101,0,42,114,101,102,101,114,101,110,99,101,0,112,114,111,112,101,114,116,105,101,115,0,42,104,105,101,114,97,114,99,104,121,95,114,111,111,116,0,42,115,116,111,114,97,103,101,0,42,114,117,110,116,105,109,101,0,95,112,97,100,95,
49,91,52,93,0,115,116,97,116,117,115,0,115,107,105,112,112,101,100,95,114,101,102,99,111,117,110,116,101,100,0,115,107,105,112,112,101,100,95,100,105,114,101,99,116,0,115,107,105,112,112,101,100,95,105,110,100,105,114,101,99,116,0,114,101,109,97,112,0,42,100,101,112,115,103,114,97,112,104,0,42,95,112,97,100,0,42,110,101,119,105,100,0,42,108,105,98,0,42,97,115,115,101,116,95,100,97,116,97,0,110,97,109,101,91,54,54,93,0,117,
115,0,105,99,111,110,95,105,100,0,114,101,99,97,108,99,0,114,101,99,97,108,99,95,117,112,95,116,111,95,117,110,100,111,95,112,117,115,104,0,114,101,99,97,108,99,95,97,102,116,101,114,95,117,110,100,111,95,112,117,115,104,0,115,101,115,115,105,111,110,95,117,105,100,0,42,112,114,111,112,101,114,116,105,101,115,0,42,111,118,101,114,114,105,100,101,95,108,105,98,114,97,114,121,0,42,111,114,105,103,95,105,100,0,42,112,121,95,105,110,
115,116,97,110,99,101,0,42,108,105,98,114,97,114,121,95,119,101,97,107,95,114,101,102,101,114,101,110,99,101,0,114,117,110,116,105,109,101,0,42,110,97,109,101,95,109,97,112,0,42,102,105,108,101,100,97,116,97,0,102,105,108,101,112,97,116,104,95,97,98,115,91,49,48,50,52,93,0,42,112,97,114,101,110,116,0,116,101,109,112,95,105,110,100,101,120,0,118,101,114,115,105,111,110,102,105,108,101,0,115,117,98,118,101,114,115,105,111,110,102,
105,108,101,0,105,100,0,110,97,109,101,91,49,48,50,52,93,0,42,112,97,99,107,101,100,102,105,108,101,0,108,105,98,114,97,114,121,95,102,105,108,101,112,97,116,104,91,49,48,50,52,93,0,108,105,98,114,97,114,121,95,105,100,95,110,97,109,101,91,54,54,93,0,119,91,50,93,0,104,91,50,93,0,102,108,97,103,91,50,93,0,99,104,97,110,103,101,100,95,116,105,109,101,115,116,97,109,112,91,50,93,0,42,114,101,99,116,91,50,93,
0,99,111,91,51,93,0,42,112,111,105,110,116,115,0,108,101,110,103,116,104,0,115,116,97,114,116,95,102,114,97,109,101,0,101,110,100,95,102,114,97,109,101,0,99,111,108,111,114,91,51,93,0,99,111,108,111,114,95,112,111,115,116,91,51,93,0,108,105,110,101,95,116,104,105,99,107,110,101,115,115,0,95,112,97,100,50,91,52,93,0,42,112,111,105,110,116,115,95,118,98,111,0,42,98,97,116,99,104,95,108,105,110,101,0,42,98,97,116,99,
104,95,112,111,105,110,116,115,0,112,97,116,104,95,116,121,112,101,0,112,97,116,104,95,115,116,101,112,0,112,97,116,104,95,114,97,110,103,101,0,112,97,116,104,95,118,105,101,119,102,108,97,103,0,112,97,116,104,95,98,97,107,101,102,108,97,103,0,112,97,116,104,95,115,102,0,112,97,116,104,95,101,102,0,112,97,116,104,95,98,99,0,112,97,116,104,95,97,99,0,112,111,105,110,116,91,51,93,0,112,108,97,110,101,95,110,111,114,109,97,
108,91,51,93,0,112,108,97,110,101,95,111,102,102,115,101,116,0,100,101,112,116,104,95,115,99,97,108,101,0,100,101,102,111,114,109,95,100,117,97,108,95,113,117,97,116,0,98,98,111,110,101,95,115,101,103,109,101,110,116,115,0,98,98,111,110,101,95,97,114,99,95,108,101,110,103,116,104,95,114,101,99,105,112,114,111,99,97,108,0,95,112,97,100,49,91,52,93,0,42,98,98,111,110,101,95,114,101,115,116,95,109,97,116,115,0,42,98,98,111,
110,101,95,112,111,115,101,95,109,97,116,115,0,42,98,98,111,110,101,95,100,101,102,111,114,109,95,109,97,116,115,0,42,98,98,111,110,101,95,100,117,97,108,95,113,117,97,116,115,0,42,98,98,111,110,101,95,115,101,103,109,101,110,116,95,98,111,117,110,100,97,114,105,101,115,0,42,112,114,111,112,0,99,111,110,115,116,114,97,105,110,116,115,0,105,107,102,108,97,103,0,112,114,111,116,101,99,116,102,108,97,103,0,97,103,114,112,95,105,110,
100,101,120,0,99,111,110,115,116,102,108,97,103,0,115,101,108,101,99,116,102,108,97,103,0,100,114,97,119,102,108,97,103,0,98,98,111,110,101,102,108,97,103,0,42,98,111,110,101,0,42,99,104,105,108,100,0,105,107,116,114,101,101,0,115,105,107,116,114,101,101,0,42,109,112,97,116,104,0,42,99,117,115,116,111,109,0,42,99,117,115,116,111,109,95,116,120,0,99,117,115,116,111,109,95,115,99,97,108,101,0,99,117,115,116,111,109,95,115,99,
97,108,101,95,120,121,122,91,51,93,0,99,117,115,116,111,109,95,116,114,97,110,115,108,97,116,105,111,110,91,51,93,0,99,117,115,116,111,109,95,114,111,116,97,116,105,111,110,95,101,117,108,101,114,91,51,93,0,99,117,115,116,111,109,95,115,104,97,112,101,95,119,105,114,101,95,119,105,100,116,104,0,108,111,99,91,51,93,0,115,105,122,101,91,51,93,0,101,117,108,91,51,93,0,113,117,97,116,91,52,93,0,114,111,116,65,120,105,115,91,
51,93,0,114,111,116,65,110,103,108,101,0,114,111,116,109,111,100,101,0,99,104,97,110,95,109,97,116,91,52,93,91,52,93,0,112,111,115,101,95,109,97,116,91,52,93,91,52,93,0,100,105,115,112,95,109,97,116,91,52,93,91,52,93,0,100,105,115,112,95,116,97,105,108,95,109,97,116,91,52,93,91,52,93,0,99,111,110,115,116,105,110,118,91,52,93,91,52,93,0,112,111,115,101,95,104,101,97,100,91,51,93,0,112,111,115,101,95,116,97,
105,108,91,51,93,0,108,105,109,105,116,109,105,110,91,51,93,0,108,105,109,105,116,109,97,120,91,51,93,0,115,116,105,102,102,110,101,115,115,91,51,93,0,105,107,115,116,114,101,116,99,104,0,105,107,114,111,116,119,101,105,103,104,116,0,105,107,108,105,110,119,101,105,103,104,116,0,114,111,108,108,49,0,114,111,108,108,50,0,99,117,114,118,101,73,110,88,0,99,117,114,118,101,73,110,89,0,99,117,114,118,101,79,117,116,88,0,99,117,114,
118,101,79,117,116,89,0,101,97,115,101,49,0,101,97,115,101,50,0,115,99,97,108,101,73,110,0,115,99,97,108,101,95,105,110,95,121,0,115,99,97,108,101,79,117,116,0,115,99,97,108,101,95,111,117,116,95,121,0,115,99,97,108,101,95,105,110,91,51,93,0,115,99,97,108,101,95,111,117,116,91,51,93,0,42,98,98,111,110,101,95,112,114,101,118,0,42,98,98,111,110,101,95,110,101,120,116,0,42,116,101,109,112,0,42,100,114,97,119,95,
100,97,116,97,0,42,111,114,105,103,95,112,99,104,97,110,0,99,111,108,111,114,0,99,104,97,110,98,97,115,101,0,42,99,104,97,110,104,97,115,104,0,42,42,99,104,97,110,95,97,114,114,97,121,0,99,116,105,109,101,0,115,116,114,105,100,101,95,111,102,102,115,101,116,91,51,93,0,99,121,99,108,105,99,95,111,102,102,115,101,116,91,51,93,0,97,103,114,111,117,112,115,0,97,99,116,105,118,101,95,103,114,111,117,112,0,105,107,115,111,
108,118,101,114,0,42,105,107,100,97,116,97,0,42,105,107,112,97,114,97,109,0,97,118,115,0,110,117,109,105,116,101,114,0,110,117,109,115,116,101,112,0,109,105,110,115,116,101,112,0,109,97,120,115,116,101,112,0,115,111,108,118,101,114,0,102,101,101,100,98,97,99,107,0,109,97,120,118,101,108,0,100,97,109,112,109,97,120,0,100,97,109,112,101,112,115,0,99,104,97,110,110,101,108,115,0,99,117,115,116,111,109,67,111,108,0,99,115,0,42,
42,108,97,121,101,114,95,97,114,114,97,121,0,108,97,121,101,114,95,97,114,114,97,121,95,110,117,109,0,108,97,121,101,114,95,97,99,116,105,118,101,95,105,110,100,101,120,0,42,42,98,105,110,100,105,110,103,95,97,114,114,97,121,0,98,105,110,100,105,110,103,95,97,114,114,97,121,95,110,117,109,0,108,97,115,116,95,98,105,110,100,105,110,103,95,104,97,110,100,108,101,0,99,117,114,118,101,115,0,103,114,111,117,112,115,0,109,97,114,107,
101,114,115,0,97,99,116,105,118,101,95,109,97,114,107,101,114,0,105,100,114,111,111,116,0,102,114,97,109,101,95,115,116,97,114,116,0,102,114,97,109,101,95,101,110,100,0,42,112,114,101,118,105,101,119,0,42,115,111,117,114,99,101,0,42,102,105,108,116,101,114,95,103,114,112,0,115,101,97,114,99,104,115,116,114,91,54,52,93,0,102,105,108,116,101,114,102,108,97,103,0,102,105,108,116,101,114,102,108,97,103,50,0,114,101,110,97,109,101,73,
110,100,101,120,0,95,112,97,100,48,91,55,93,0,114,101,103,105,111,110,98,97,115,101,0,115,112,97,99,101,116,121,112,101,0,108,105,110,107,95,102,108,97,103,0,95,112,97,100,48,91,54,93,0,118,50,100,0,42,97,99,116,105,111,110,0,97,100,115,0,116,105,109,101,115,108,105,100,101,0,109,111,100,101,0,109,111,100,101,95,112,114,101,118,0,97,117,116,111,115,110,97,112,0,99,97,99,104,101,95,100,105,115,112,108,97,121,0,95,112,
97,100,49,91,54,93,0,42,103,114,112,0,42,105,112,111,0,99,111,110,115,116,114,97,105,110,116,67,104,97,110,110,101,108,115,0,116,101,109,112,0,105,110,102,108,117,101,110,99,101,0,108,97,121,101,114,95,102,108,97,103,115,0,108,97,121,101,114,95,109,105,120,95,109,111,100,101,0,42,42,115,116,114,105,112,95,97,114,114,97,121,0,115,116,114,105,112,95,97,114,114,97,121,95,110,117,109,0,105,100,116,121,112,101,0,104,97,110,100,108,
101,0,115,116,114,105,112,95,116,121,112,101,0,95,112,97,100,48,91,51,93,0,102,114,97,109,101,95,111,102,102,115,101,116,0,115,116,114,105,112,0,42,42,99,104,97,110,110,101,108,98,97,103,115,95,97,114,114,97,121,0,99,104,97,110,110,101,108,98,97,103,115,95,97,114,114,97,121,95,110,117,109,0,98,105,110,100,105,110,103,95,104,97,110,100,108,101,0,102,99,117,114,118,101,95,97,114,114,97,121,95,110,117,109,0,42,42,102,99,117,
114,118,101,95,97,114,114,97,121,0,42,99,117,114,118,101,0,42,100,97,116,97,0,117,105,95,101,120,112,97,110,100,95,102,108,97,103,0,115,102,114,97,0,101,102,114,97,0,98,108,101,110,100,105,110,0,98,108,101,110,100,111,117,116,0,42,99,111,101,102,102,105,99,105,101,110,116,115,0,97,114,114,97,121,115,105,122,101,0,112,111,108,121,95,111,114,100,101,114,0,97,109,112,108,105,116,117,100,101,0,112,104,97,115,101,95,109,117,108,116,
105,112,108,105,101,114,0,112,104,97,115,101,95,111,102,102,115,101,116,0,118,97,108,117,101,95,111,102,102,115,101,116,0,116,105,109,101,0,102,49,0,102,50,0,116,111,116,118,101,114,116,0,109,105,100,118,97,108,0,98,101,102,111,114,101,95,109,111,100,101,0,97,102,116,101,114,95,109,111,100,101,0,98,101,102,111,114,101,95,99,121,99,108,101,115,0,97,102,116,101,114,95,99,121,99,108,101,115,0,42,115,99,114,105,112,116,0,114,101,99,
116,0,115,105,122,101,0,115,116,114,101,110,103,116,104,0,112,104,97,115,101,0,111,102,102,115,101,116,0,100,101,112,116,104,0,109,111,100,105,102,105,99,97,116,105,111,110,0,115,116,101,112,95,115,105,122,101,0,42,105,100,0,112,99,104,97,110,95,110,97,109,101,91,54,52,93,0,116,114,97,110,115,67,104,97,110,0,114,111,116,97,116,105,111,110,95,109,111,100,101,0,95,112,97,100,91,53,93,0,111,112,116,105,111,110,115,0,99,111,110,
116,101,120,116,95,112,114,111,112,101,114,116,121,0,102,97,108,108,98,97,99,107,95,118,97,108,117,101,0,116,97,114,103,101,116,115,91,56,93,0,110,117,109,95,116,97,114,103,101,116,115,0,99,117,114,118,97,108,0,118,97,114,105,97,98,108,101,115,0,101,120,112,114,101,115,115,105,111,110,91,50,53,54,93,0,42,101,120,112,114,95,99,111,109,112,0,42,101,120,112,114,95,115,105,109,112,108,101,0,118,101,99,91,50,93,0,42,100,114,105,
118,101,114,0,109,111,100,105,102,105,101,114,115,0,42,98,101,122,116,0,42,102,112,116,0,97,99,116,105,118,101,95,107,101,121,102,114,97,109,101,95,105,110,100,101,120,0,101,120,116,101,110,100,0,97,117,116,111,95,115,109,111,111,116,104,105,110,103,0,97,114,114,97,121,95,105,110,100,101,120,0,99,111,108,111,114,95,109,111,100,101,0,112,114,101,118,95,110,111,114,109,95,102,97,99,116,111,114,0,112,114,101,118,95,111,102,102,115,101,116,
0,115,116,114,105,112,115,0,42,97,99,116,0,102,99,117,114,118,101,115,0,115,116,114,105,112,95,116,105,109,101,0,115,116,97,114,116,0,101,110,100,0,97,99,116,115,116,97,114,116,0,97,99,116,101,110,100,0,114,101,112,101,97,116,0,115,99,97,108,101,0,98,108,101,110,100,109,111,100,101,0,101,120,116,101,110,100,109,111,100,101,0,95,112,97,100,49,91,50,93,0,42,115,112,101,97,107,101,114,95,104,97,110,100,108,101,0,42,111,114,
105,103,95,115,116,114,105,112,0,42,95,112,97,100,51,0,105,110,100,101,120,0,103,114,111,117,112,91,54,52,93,0,103,114,111,117,112,109,111,100,101,0,107,101,121,105,110,103,102,108,97,103,0,107,101,121,105,110,103,111,118,101,114,114,105,100,101,0,112,97,116,104,115,0,105,100,110,97,109,101,91,54,52,93,0,100,101,115,99,114,105,112,116,105,111,110,91,49,48,50,52,93,0,116,121,112,101,105,110,102,111,91,54,52,93,0,97,99,116,105,
118,101,95,112,97,116,104,0,98,105,110,100,105,110,103,95,110,97,109,101,91,54,54,93,0,42,116,109,112,97,99,116,0,110,108,97,95,116,114,97,99,107,115,0,42,97,99,116,95,116,114,97,99,107,0,42,97,99,116,115,116,114,105,112,0,100,114,105,118,101,114,115,0,111,118,101,114,114,105,100,101,115,0,42,42,100,114,105,118,101,114,95,97,114,114,97,121,0,97,99,116,95,98,108,101,110,100,109,111,100,101,0,97,99,116,95,101,120,116,101,
110,100,109,111,100,101,0,97,99,116,95,105,110,102,108,117,101,110,99,101,0,42,97,100,116,0,112,97,108,101,116,116,101,95,105,110,100,101,120,0,99,117,115,116,111,109,0,99,111,108,108,101,99,116,105,111,110,115,0,99,104,105,108,100,98,97,115,101,0,114,111,108,108,0,104,101,97,100,91,51,93,0,116,97,105,108,91,51,93,0,98,111,110,101,95,109,97,116,91,51,93,91,51,93,0,105,110,104,101,114,105,116,95,115,99,97,108,101,95,109,
111,100,101,0,97,114,109,95,104,101,97,100,91,51,93,0,97,114,109,95,116,97,105,108,91,51,93,0,97,114,109,95,109,97,116,91,52,93,91,52,93,0,97,114,109,95,114,111,108,108,0,100,105,115,116,0,119,101,105,103,104,116,0,120,119,105,100,116,104,0,122,119,105,100,116,104,0,114,97,100,95,104,101,97,100,0,114,97,100,95,116,97,105,108,0,108,97,121,101,114,0,115,101,103,109,101,110,116,115,0,98,98,111,110,101,95,109,97,112,112,
105,110,103,95,109,111,100,101,0,95,112,97,100,50,91,55,93,0,98,98,111,110,101,95,112,114,101,118,95,116,121,112,101,0,98,98,111,110,101,95,110,101,120,116,95,116,121,112,101,0,98,98,111,110,101,95,102,108,97,103,0,98,98,111,110,101,95,112,114,101,118,95,102,108,97,103,0,98,98,111,110,101,95,110,101,120,116,95,102,108,97,103,0,97,99,116,105,118,101,95,99,111,108,108,101,99,116,105,111,110,95,105,110,100,101,120,0,42,97,99,
116,105,118,101,95,99,111,108,108,101,99,116,105,111,110,0,98,111,110,101,98,97,115,101,0,42,98,111,110,101,104,97,115,104,0,42,95,112,97,100,49,0,42,101,100,98,111,0,42,97,99,116,95,98,111,110,101,0,42,97,99,116,95,101,100,98,111,110,101,0,110,101,101,100,115,95,102,108,117,115,104,95,116,111,95,105,100,0,100,114,97,119,116,121,112,101,0,100,101,102,111,114,109,102,108,97,103,0,112,97,116,104,102,108,97,103,0,42,42,99,
111,108,108,101,99,116,105,111,110,95,97,114,114,97,121,0,99,111,108,108,101,99,116,105,111,110,95,97,114,114,97,121,95,110,117,109,0,99,111,108,108,101,99,116,105,111,110,95,114,111,111,116,95,99,111,117,110,116,0,97,99,116,105,118,101,95,99,111,108,108,101,99,116,105,111,110,95,110,97,109,101,91,54,52,93,0,108,97,121,101,114,95,117,115,101,100,0,108,97,121,101,114,95,112,114,111,116,101,99,116,101,100,0,97,120,101,115,95,112,111,
115,105,116,105,111,110,0,98,111,110,101,115,0,102,108,97,103,115,0,99,104,105,108,100,95,105,110,100,101,120,0,99,104,105,108,100,95,99,111,117,110,116,0,42,98,99,111,108,108,0,42,108,111,99,97,108,95,116,121,112,101,95,105,110,102,111,0,99,97,116,97,108,111,103,95,105,100,0,99,97,116,97,108,111,103,95,115,105,109,112,108,101,95,110,97,109,101,91,54,52,93,0,42,97,117,116,104,111,114,0,42,99,111,112,121,114,105,103,104,116,
0,42,108,105,99,101,110,115,101,0,116,97,103,115,0,97,99,116,105,118,101,95,116,97,103,0,116,111,116,95,116,97,103,115,0,99,117,115,116,111,109,95,108,105,98,114,97,114,121,95,105,110,100,101,120,0,97,115,115,101,116,95,108,105,98,114,97,114,121,95,116,121,112,101,0,42,97,115,115,101,116,95,108,105,98,114,97,114,121,95,105,100,101,110,116,105,102,105,101,114,0,42,114,101,108,97,116,105,118,101,95,97,115,115,101,116,95,105,100,101,
110,116,105,102,105,101,114,0,42,112,97,116,104,0,110,97,109,101,91,51,50,93,0,114,117,108,101,0,42,111,98,0,102,101,97,114,95,102,97,99,116,111,114,0,115,105,103,110,97,108,95,105,100,0,108,111,111,107,95,97,104,101,97,100,0,111,108,111,99,91,51,93,0,99,102,114,97,0,100,105,115,116,97,110,99,101,0,113,117,101,117,101,95,115,105,122,101,0,119,97,110,100,101,114,0,108,101,118,101,108,0,115,112,101,101,100,0,102,108,101,
101,95,100,105,115,116,97,110,99,101,0,104,101,97,108,116,104,0,97,99,99,91,51,93,0,115,116,97,116,101,95,105,100,0,114,117,108,101,115,0,99,111,110,100,105,116,105,111,110,115,0,97,99,116,105,111,110,115,0,114,117,108,101,115,101,116,95,116,121,112,101,0,114,117,108,101,95,102,117,122,122,105,110,101,115,115,0,118,111,108,117,109,101,0,102,97,108,108,111,102,102,0,108,97,115,116,95,115,116,97,116,101,95,105,100,0,108,97,110,100,
105,110,103,95,115,109,111,111,116,104,110,101,115,115,0,104,101,105,103,104,116,0,98,97,110,107,105,110,103,0,112,105,116,99,104,0,97,103,103,114,101,115,115,105,111,110,0,97,99,99,117,114,97,99,121,0,114,97,110,103,101,0,97,105,114,95,109,105,110,95,115,112,101,101,100,0,97,105,114,95,109,97,120,95,115,112,101,101,100,0,97,105,114,95,109,97,120,95,97,99,99,0,97,105,114,95,109,97,120,95,97,118,101,0,97,105,114,95,112,101,
114,115,111,110,97,108,95,115,112,97,99,101,0,108,97,110,100,95,106,117,109,112,95,115,112,101,101,100,0,108,97,110,100,95,109,97,120,95,115,112,101,101,100,0,108,97,110,100,95,109,97,120,95,97,99,99,0,108,97,110,100,95,109,97,120,95,97,118,101,0,108,97,110,100,95,112,101,114,115,111,110,97,108,95,115,112,97,99,101,0,108,97,110,100,95,115,116,105,99,107,95,102,111,114,99,101,0,115,116,97,116,101,115,0,42,105,109,97,103,101,
0,111,102,102,115,101,116,91,50,93,0,97,108,112,104,97,0,100,114,97,119,95,115,109,111,111,116,104,102,97,99,0,102,105,108,108,95,102,97,99,116,111,114,0,100,114,97,119,95,115,116,114,101,110,103,116,104,0,100,114,97,119,95,106,105,116,116,101,114,0,100,114,97,119,95,97,110,103,108,101,0,100,114,97,119,95,97,110,103,108,101,95,102,97,99,116,111,114,0,100,114,97,119,95,114,97,110,100,111,109,95,112,114,101,115,115,0,100,114,97,
119,95,114,97,110,100,111,109,95,115,116,114,101,110,103,116,104,0,100,114,97,119,95,115,109,111,111,116,104,108,118,108,0,100,114,97,119,95,115,117,98,100,105,118,105,100,101,0,102,105,108,108,95,108,97,121,101,114,95,109,111,100,101,0,102,105,108,108,95,100,105,114,101,99,116,105,111,110,0,102,105,108,108,95,116,104,114,101,115,104,111,108,100,0,95,112,97,100,50,91,50,93,0,99,97,112,115,95,116,121,112,101,0,102,108,97,103,50,0,102,
105,108,108,95,115,105,109,112,108,121,108,118,108,0,102,105,108,108,95,100,114,97,119,95,109,111,100,101,0,102,105,108,108,95,101,120,116,101,110,100,95,109,111,100,101,0,105,110,112,117,116,95,115,97,109,112,108,101,115,0,117,118,95,114,97,110,100,111,109,0,98,114,117,115,104,95,116,121,112,101,0,101,114,97,115,101,114,95,109,111,100,101,0,97,99,116,105,118,101,95,115,109,111,111,116,104,0,101,114,97,95,115,116,114,101,110,103,116,104,95,
102,0,101,114,97,95,116,104,105,99,107,110,101,115,115,95,102,0,103,114,97,100,105,101,110,116,95,102,0,103,114,97,100,105,101,110,116,95,115,91,50,93,0,115,105,109,112,108,105,102,121,95,102,0,118,101,114,116,101,120,95,102,97,99,116,111,114,0,118,101,114,116,101,120,95,109,111,100,101,0,115,99,117,108,112,116,95,102,108,97,103,0,115,99,117,108,112,116,95,109,111,100,101,95,102,108,97,103,0,112,114,101,115,101,116,95,116,121,112,101,
0,98,114,117,115,104,95,100,114,97,119,95,109,111,100,101,0,114,97,110,100,111,109,95,104,117,101,0,114,97,110,100,111,109,95,115,97,116,117,114,97,116,105,111,110,0,114,97,110,100,111,109,95,118,97,108,117,101,0,102,105,108,108,95,101,120,116,101,110,100,95,102,97,99,0,100,105,108,97,116,101,95,112,105,120,101,108,115,0,42,99,117,114,118,101,95,115,101,110,115,105,116,105,118,105,116,121,0,42,99,117,114,118,101,95,115,116,114,101,110,
103,116,104,0,42,99,117,114,118,101,95,106,105,116,116,101,114,0,42,99,117,114,118,101,95,114,97,110,100,95,112,114,101,115,115,117,114,101,0,42,99,117,114,118,101,95,114,97,110,100,95,115,116,114,101,110,103,116,104,0,42,99,117,114,118,101,95,114,97,110,100,95,117,118,0,42,99,117,114,118,101,95,114,97,110,100,95,104,117,101,0,42,99,117,114,118,101,95,114,97,110,100,95,115,97,116,117,114,97,116,105,111,110,0,42,99,117,114,118,101,
95,114,97,110,100,95,118,97,108,117,101,0,111,117,116,108,105,110,101,95,102,97,99,0,42,109,97,116,101,114,105,97,108,0,42,109,97,116,101,114,105,97,108,95,97,108,116,0,97,100,100,95,97,109,111,117,110,116,0,112,111,105,110,116,115,95,112,101,114,95,99,117,114,118,101,0,109,105,110,105,109,117,109,95,108,101,110,103,116,104,0,99,117,114,118,101,95,108,101,110,103,116,104,0,109,105,110,105,109,117,109,95,100,105,115,116,97,110,99,101,
0,99,117,114,118,101,95,114,97,100,105,117,115,0,100,101,110,115,105,116,121,95,97,100,100,95,97,116,116,101,109,112,116,115,0,100,101,110,115,105,116,121,95,109,111,100,101,0,95,112,97,100,91,55,93,0,42,99,117,114,118,101,95,112,97,114,97,109,101,116,101,114,95,102,97,108,108,111,102,102,0,99,108,111,110,101,0,109,116,101,120,0,109,97,115,107,95,109,116,101,120,0,42,116,111,103,103,108,101,95,98,114,117,115,104,0,42,105,99,111,
110,95,105,109,98,117,102,0,42,103,114,97,100,105,101,110,116,0,42,112,97,105,110,116,95,99,117,114,118,101,0,105,99,111,110,95,102,105,108,101,112,97,116,104,91,49,48,50,52,93,0,110,111,114,109,97,108,95,119,101,105,103,104,116,0,114,97,107,101,95,102,97,99,116,111,114,0,98,108,101,110,100,0,111,98,95,109,111,100,101,0,115,97,109,112,108,105,110,103,95,102,108,97,103,0,109,97,115,107,95,112,114,101,115,115,117,114,101,0,106,
105,116,116,101,114,0,106,105,116,116,101,114,95,97,98,115,111,108,117,116,101,0,111,118,101,114,108,97,121,95,102,108,97,103,115,0,115,112,97,99,105,110,103,0,115,109,111,111,116,104,95,115,116,114,111,107,101,95,114,97,100,105,117,115,0,115,109,111,111,116,104,95,115,116,114,111,107,101,95,102,97,99,116,111,114,0,114,97,116,101,0,114,103,98,91,51,93,0,104,97,114,100,110,101,115,115,0,102,108,111,119,0,119,101,116,95,109,105,120,0,
119,101,116,95,112,101,114,115,105,115,116,101,110,99,101,0,100,101,110,115,105,116,121,0,112,97,105,110,116,95,102,108,97,103,115,0,116,105,112,95,114,111,117,110,100,110,101,115,115,0,116,105,112,95,115,99,97,108,101,95,120,0,115,101,99,111,110,100,97,114,121,95,114,103,98,91,51,93,0,100,97,115,104,95,114,97,116,105,111,0,100,97,115,104,95,115,97,109,112,108,101,115,0,115,99,117,108,112,116,95,112,108,97,110,101,0,103,114,97,100,
105,101,110,116,95,115,112,97,99,105,110,103,0,103,114,97,100,105,101,110,116,95,115,116,114,111,107,101,95,109,111,100,101,0,103,114,97,100,105,101,110,116,95,102,105,108,108,95,109,111,100,101,0,95,112,97,100,48,0,102,97,108,108,111,102,102,95,115,104,97,112,101,0,102,97,108,108,111,102,102,95,97,110,103,108,101,0,115,99,117,108,112,116,95,116,111,111,108,0,118,101,114,116,101,120,112,97,105,110,116,95,116,111,111,108,0,119,101,105,103,
104,116,112,97,105,110,116,95,116,111,111,108,0,105,109,97,103,101,112,97,105,110,116,95,116,111,111,108,0,109,97,115,107,95,116,111,111,108,0,103,112,101,110,99,105,108,95,116,111,111,108,0,103,112,101,110,99,105,108,95,118,101,114,116,101,120,95,116,111,111,108,0,103,112,101,110,99,105,108,95,115,99,117,108,112,116,95,116,111,111,108,0,103,112,101,110,99,105,108,95,119,101,105,103,104,116,95,116,111,111,108,0,99,117,114,118,101,115,95,115,
99,117,108,112,116,95,116,111,111,108,0,97,117,116,111,115,109,111,111,116,104,95,102,97,99,116,111,114,0,116,105,108,116,95,115,116,114,101,110,103,116,104,95,102,97,99,116,111,114,0,116,111,112,111,108,111,103,121,95,114,97,107,101,95,102,97,99,116,111,114,0,99,114,101,97,115,101,95,112,105,110,99,104,95,102,97,99,116,111,114,0,110,111,114,109,97,108,95,114,97,100,105,117,115,95,102,97,99,116,111,114,0,97,114,101,97,95,114,97,100,
105,117,115,95,102,97,99,116,111,114,0,119,101,116,95,112,97,105,110,116,95,114,97,100,105,117,115,95,102,97,99,116,111,114,0,112,108,97,110,101,95,116,114,105,109,0,116,101,120,116,117,114,101,95,115,97,109,112,108,101,95,98,105,97,115,0,99,117,114,118,101,95,112,114,101,115,101,116,0,100,105,115,99,111,110,110,101,99,116,101,100,95,100,105,115,116,97,110,99,101,95,109,97,120,0,100,101,102,111,114,109,95,116,97,114,103,101,116,0,97,
117,116,111,109,97,115,107,105,110,103,95,102,108,97,103,115,0,97,117,116,111,109,97,115,107,105,110,103,95,98,111,117,110,100,97,114,121,95,101,100,103,101,115,95,112,114,111,112,97,103,97,116,105,111,110,95,115,116,101,112,115,0,97,117,116,111,109,97,115,107,105,110,103,95,115,116,97,114,116,95,110,111,114,109,97,108,95,108,105,109,105,116,0,97,117,116,111,109,97,115,107,105,110,103,95,115,116,97,114,116,95,110,111,114,109,97,108,95,102,97,
108,108,111,102,102,0,97,117,116,111,109,97,115,107,105,110,103,95,118,105,101,119,95,110,111,114,109,97,108,95,108,105,109,105,116,0,97,117,116,111,109,97,115,107,105,110,103,95,118,105,101,119,95,110,111,114,109,97,108,95,102,97,108,108,111,102,102,0,101,108,97,115,116,105,99,95,100,101,102,111,114,109,95,116,121,112,101,0,101,108,97,115,116,105,99,95,100,101,102,111,114,109,95,118,111,108,117,109,101,95,112,114,101,115,101,114,118,97,116,105,
111,110,0,115,110,97,107,101,95,104,111,111,107,95,100,101,102,111,114,109,95,116,121,112,101,0,112,111,115,101,95,100,101,102,111,114,109,95,116,121,112,101,0,112,111,115,101,95,111,102,102,115,101,116,0,112,111,115,101,95,115,109,111,111,116,104,95,105,116,101,114,97,116,105,111,110,115,0,112,111,115,101,95,105,107,95,115,101,103,109,101,110,116,115,0,112,111,115,101,95,111,114,105,103,105,110,95,116,121,112,101,0,98,111,117,110,100,97,114,121,
95,100,101,102,111,114,109,95,116,121,112,101,0,98,111,117,110,100,97,114,121,95,102,97,108,108,111,102,102,95,116,121,112,101,0,98,111,117,110,100,97,114,121,95,111,102,102,115,101,116,0,99,108,111,116,104,95,100,101,102,111,114,109,95,116,121,112,101,0,99,108,111,116,104,95,102,111,114,99,101,95,102,97,108,108,111,102,102,95,116,121,112,101,0,99,108,111,116,104,95,115,105,109,117,108,97,116,105,111,110,95,97,114,101,97,95,116,121,112,101,
0,99,108,111,116,104,95,109,97,115,115,0,99,108,111,116,104,95,100,97,109,112,105,110,103,0,99,108,111,116,104,95,115,105,109,95,108,105,109,105,116,0,99,108,111,116,104,95,115,105,109,95,102,97,108,108,111,102,102,0,99,108,111,116,104,95,99,111,110,115,116,114,97,105,110,116,95,115,111,102,116,98,111,100,121,95,115,116,114,101,110,103,116,104,0,115,109,111,111,116,104,95,100,101,102,111,114,109,95,116,121,112,101,0,115,117,114,102,97,99,
101,95,115,109,111,111,116,104,95,115,104,97,112,101,95,112,114,101,115,101,114,118,97,116,105,111,110,0,115,117,114,102,97,99,101,95,115,109,111,111,116,104,95,99,117,114,114,101,110,116,95,118,101,114,116,101,120,0,115,117,114,102,97,99,101,95,115,109,111,111,116,104,95,105,116,101,114,97,116,105,111,110,115,0,109,117,108,116,105,112,108,97,110,101,95,115,99,114,97,112,101,95,97,110,103,108,101,0,115,109,101,97,114,95,100,101,102,111,114,109,
95,116,121,112,101,0,115,108,105,100,101,95,100,101,102,111,114,109,95,116,121,112,101,0,116,101,120,116,117,114,101,95,111,118,101,114,108,97,121,95,97,108,112,104,97,0,109,97,115,107,95,111,118,101,114,108,97,121,95,97,108,112,104,97,0,99,117,114,115,111,114,95,111,118,101,114,108,97,121,95,97,108,112,104,97,0,117,110,112,114,111,106,101,99,116,101,100,95,114,97,100,105,117,115,0,115,104,97,114,112,95,116,104,114,101,115,104,111,108,100,
0,98,108,117,114,95,107,101,114,110,101,108,95,114,97,100,105,117,115,0,98,108,117,114,95,109,111,100,101,0,97,100,100,95,99,111,108,91,52,93,0,115,117,98,95,99,111,108,91,52,93,0,115,116,101,110,99,105,108,95,112,111,115,91,50,93,0,115,116,101,110,99,105,108,95,100,105,109,101,110,115,105,111,110,91,50,93,0,109,97,115,107,95,115,116,101,110,99,105,108,95,112,111,115,91,50,93,0,109,97,115,107,95,115,116,101,110,99,105,108,
95,100,105,109,101,110,115,105,111,110,91,50,93,0,42,103,112,101,110,99,105,108,95,115,101,116,116,105,110,103,115,0,42,99,117,114,118,101,115,95,115,99,117,108,112,116,95,115,101,116,116,105,110,103,115,0,97,117,116,111,109,97,115,107,105,110,103,95,99,97,118,105,116,121,95,98,108,117,114,95,115,116,101,112,115,0,97,117,116,111,109,97,115,107,105,110,103,95,99,97,118,105,116,121,95,102,97,99,116,111,114,0,42,97,117,116,111,109,97,115,
107,105,110,103,95,99,97,118,105,116,121,95,99,117,114,118,101,0,104,0,115,0,118,0,99,111,108,111,114,115,0,97,99,116,105,118,101,95,99,111,108,111,114,0,98,101,122,0,112,114,101,115,115,117,114,101,0,116,111,116,95,112,111,105,110,116,115,0,97,100,100,95,105,110,100,101,120,0,112,97,116,104,91,52,48,57,54,93,0,102,105,108,101,112,97,116,104,91,49,48,50,52,93,0,95,112,97,100,0,111,98,106,101,99,116,95,112,97,116,104,
115,0,108,97,121,101,114,115,0,105,115,95,115,101,113,117,101,110,99,101,0,102,111,114,119,97,114,100,95,97,120,105,115,0,117,112,95,97,120,105,115,0,111,118,101,114,114,105,100,101,95,102,114,97,109,101,0,102,114,97,109,101,0,117,115,101,95,114,101,110,100,101,114,95,112,114,111,99,101,100,117,114,97,108,0,95,112,97,100,49,91,51,93,0,117,115,101,95,112,114,101,102,101,116,99,104,0,112,114,101,102,101,116,99,104,95,99,97,99,104,
101,95,115,105,122,101,0,97,99,116,105,118,101,95,108,97,121,101,114,0,95,112,97,100,50,91,51,93,0,118,101,108,111,99,105,116,121,95,117,110,105,116,0,118,101,108,111,99,105,116,121,95,110,97,109,101,91,54,52,93,0,42,104,97,110,100,108,101,0,104,97,110,100,108,101,95,102,105,108,101,112,97,116,104,91,49,48,50,52,93,0,42,104,97,110,100,108,101,95,114,101,97,100,101,114,115,0,105,110,116,101,114,111,99,117,108,97,114,95,100,
105,115,116,97,110,99,101,0,99,111,110,118,101,114,103,101,110,99,101,95,100,105,115,116,97,110,99,101,0,99,111,110,118,101,114,103,101,110,99,101,95,109,111,100,101,0,112,105,118,111,116,0,112,111,108,101,95,109,101,114,103,101,95,97,110,103,108,101,95,102,114,111,109,0,112,111,108,101,95,109,101,114,103,101,95,97,110,103,108,101,95,116,111,0,42,105,109,97,0,105,117,115,101,114,0,42,99,108,105,112,0,99,117,115,101,114,0,114,111,116,
97,116,105,111,110,0,115,111,117,114,99,101,0,42,102,111,99,117,115,95,111,98,106,101,99,116,0,102,111,99,117,115,95,115,117,98,116,97,114,103,101,116,91,54,52,93,0,102,111,99,117,115,95,100,105,115,116,97,110,99,101,0,97,112,101,114,116,117,114,101,95,102,115,116,111,112,0,97,112,101,114,116,117,114,101,95,114,111,116,97,116,105,111,110,0,97,112,101,114,116,117,114,101,95,114,97,116,105,111,0,97,112,101,114,116,117,114,101,95,98,
108,97,100,101,115,0,100,114,119,95,99,111,114,110,101,114,115,91,50,93,91,52,93,91,50,93,0,100,114,119,95,116,114,105,97,91,50,93,91,50,93,0,100,114,119,95,100,101,112,116,104,91,50,93,0,100,114,119,95,102,111,99,117,115,109,97,116,91,52,93,91,52,93,0,100,114,119,95,110,111,114,109,97,108,109,97,116,91,52,93,91,52,93,0,100,116,120,0,112,97,115,115,101,112,97,114,116,97,108,112,104,97,0,99,108,105,112,115,116,97,
0,99,108,105,112,101,110,100,0,108,101,110,115,0,111,114,116,104,111,95,115,99,97,108,101,0,100,114,97,119,115,105,122,101,0,115,101,110,115,111,114,95,120,0,115,101,110,115,111,114,95,121,0,115,104,105,102,116,120,0,115,104,105,102,116,121,0,89,70,95,100,111,102,100,105,115,116,0,115,101,110,115,111,114,95,102,105,116,0,112,97,110,111,114,97,109,97,95,116,121,112,101,0,102,105,115,104,101,121,101,95,102,111,118,0,102,105,115,104,101,
121,101,95,108,101,110,115,0,108,97,116,105,116,117,100,101,95,109,105,110,0,108,97,116,105,116,117,100,101,95,109,97,120,0,108,111,110,103,105,116,117,100,101,95,109,105,110,0,108,111,110,103,105,116,117,100,101,95,109,97,120,0,102,105,115,104,101,121,101,95,112,111,108,121,110,111,109,105,97,108,95,107,48,0,102,105,115,104,101,121,101,95,112,111,108,121,110,111,109,105,97,108,95,107,49,0,102,105,115,104,101,121,101,95,112,111,108,121,110,111,
109,105,97,108,95,107,50,0,102,105,115,104,101,121,101,95,112,111,108,121,110,111,109,105,97,108,95,107,51,0,102,105,115,104,101,121,101,95,112,111,108,121,110,111,109,105,97,108,95,107,52,0,42,100,111,102,95,111,98,0,103,112,117,95,100,111,102,0,100,111,102,0,98,103,95,105,109,97,103,101,115,0,115,116,101,114,101,111,0,42,99,97,99,104,101,0,109,105,110,103,111,97,108,0,67,100,105,115,0,67,118,105,0,103,114,97,118,105,116,121,
91,51,93,0,100,116,0,109,97,115,115,0,115,116,114,117,99,116,117,114,97,108,0,115,104,101,97,114,0,98,101,110,100,105,110,103,0,109,97,120,95,98,101,110,100,0,109,97,120,95,115,116,114,117,99,116,0,109,97,120,95,115,104,101,97,114,0,109,97,120,95,115,101,119,105,110,103,0,97,118,103,95,115,112,114,105,110,103,95,108,101,110,0,116,105,109,101,115,99,97,108,101,0,116,105,109,101,95,115,99,97,108,101,0,109,97,120,103,111,97,
108,0,101,102,102,95,102,111,114,99,101,95,115,99,97,108,101,0,101,102,102,95,119,105,110,100,95,115,99,97,108,101,0,115,105,109,95,116,105,109,101,95,111,108,100,0,100,101,102,103,111,97,108,0,103,111,97,108,115,112,114,105,110,103,0,103,111,97,108,102,114,105,99,116,0,118,101,108,111,99,105,116,121,95,115,109,111,111,116,104,0,100,101,110,115,105,116,121,95,116,97,114,103,101,116,0,100,101,110,115,105,116,121,95,115,116,114,101,110,103,
116,104,0,99,111,108,108,105,100,101,114,95,102,114,105,99,116,105,111,110,0,118,101,108,95,100,97,109,112,105,110,103,0,115,104,114,105,110,107,95,109,105,110,0,115,104,114,105,110,107,95,109,97,120,0,117,110,105,102,111,114,109,95,112,114,101,115,115,117,114,101,95,102,111,114,99,101,0,116,97,114,103,101,116,95,118,111,108,117,109,101,0,112,114,101,115,115,117,114,101,95,102,97,99,116,111,114,0,102,108,117,105,100,95,100,101,110,115,105,116,
121,0,118,103,114,111,117,112,95,112,114,101,115,115,117,114,101,0,95,112,97,100,55,91,54,93,0,98,101,110,100,105,110,103,95,100,97,109,112,105,110,103,0,118,111,120,101,108,95,99,101,108,108,95,115,105,122,101,0,115,116,101,112,115,80,101,114,70,114,97,109,101,0,112,114,101,114,111,108,108,0,109,97,120,115,112,114,105,110,103,108,101,110,0,115,111,108,118,101,114,95,116,121,112,101,0,118,103,114,111,117,112,95,98,101,110,100,0,118,103,
114,111,117,112,95,109,97,115,115,0,118,103,114,111,117,112,95,115,116,114,117,99,116,0,118,103,114,111,117,112,95,115,104,114,105,110,107,0,115,104,97,112,101,107,101,121,95,114,101,115,116,0,112,114,101,115,101,116,115,0,114,101,115,101,116,0,42,101,102,102,101,99,116,111,114,95,119,101,105,103,104,116,115,0,98,101,110,100,105,110,103,95,109,111,100,101,108,0,118,103,114,111,117,112,95,115,104,101,97,114,0,116,101,110,115,105,111,110,0,99,
111,109,112,114,101,115,115,105,111,110,0,109,97,120,95,116,101,110,115,105,111,110,0,109,97,120,95,99,111,109,112,114,101,115,115,105,111,110,0,116,101,110,115,105,111,110,95,100,97,109,112,0,99,111,109,112,114,101,115,115,105,111,110,95,100,97,109,112,0,115,104,101,97,114,95,100,97,109,112,0,105,110,116,101,114,110,97,108,95,115,112,114,105,110,103,95,109,97,120,95,108,101,110,103,116,104,0,105,110,116,101,114,110,97,108,95,115,112,114,105,
110,103,95,109,97,120,95,100,105,118,101,114,115,105,111,110,0,118,103,114,111,117,112,95,105,110,116,101,114,110,0,105,110,116,101,114,110,97,108,95,116,101,110,115,105,111,110,0,105,110,116,101,114,110,97,108,95,99,111,109,112,114,101,115,115,105,111,110,0,109,97,120,95,105,110,116,101,114,110,97,108,95,116,101,110,115,105,111,110,0,109,97,120,95,105,110,116,101,114,110,97,108,95,99,111,109,112,114,101,115,115,105,111,110,0,42,99,111,108,108,
105,115,105,111,110,95,108,105,115,116,0,101,112,115,105,108,111,110,0,115,101,108,102,95,102,114,105,99,116,105,111,110,0,102,114,105,99,116,105,111,110,0,100,97,109,112,105,110,103,0,115,101,108,102,101,112,115,105,108,111,110,0,114,101,112,101,108,95,102,111,114,99,101,0,100,105,115,116,97,110,99,101,95,114,101,112,101,108,0,115,101,108,102,95,108,111,111,112,95,99,111,117,110,116,0,108,111,111,112,95,99,111,117,110,116,0,42,103,114,111,
117,112,0,118,103,114,111,117,112,95,115,101,108,102,99,111,108,0,118,103,114,111,117,112,95,111,98,106,99,111,108,0,99,108,97,109,112,0,115,101,108,102,95,99,108,97,109,112,0,108,105,110,107,95,115,116,97,116,101,0,108,105,103,104,116,95,108,105,110,107,105,110,103,0,42,99,111,108,108,101,99,116,105,111,110,0,102,104,95,105,100,110,97,109,101,91,54,52,93,0,42,101,120,112,111,114,116,95,112,114,111,112,101,114,116,105,101,115,0,111,
98,106,101,99,116,95,99,97,99,104,101,0,111,98,106,101,99,116,95,99,97,99,104,101,95,105,110,115,116,97,110,99,101,100,0,112,97,114,101,110,116,115,0,42,103,111,98,106,101,99,116,95,104,97,115,104,0,42,111,119,110,101,114,95,105,100,0,103,111,98,106,101,99,116,0,99,104,105,108,100,114,101,110,0,97,99,116,105,118,101,95,101,120,112,111,114,116,101,114,95,105,110,100,101,120,0,101,120,112,111,114,116,101,114,115,0,100,117,112,108,
105,95,111,102,115,91,51,93,0,99,111,108,111,114,95,116,97,103,0,108,105,110,101,97,114,116,95,117,115,97,103,101,0,108,105,110,101,97,114,116,95,102,108,97,103,115,0,108,105,110,101,97,114,116,95,105,110,116,101,114,115,101,99,116,105,111,110,95,109,97,115,107,0,108,105,110,101,97,114,116,95,105,110,116,101,114,115,101,99,116,105,111,110,95,112,114,105,111,114,105,116,121,0,42,118,105,101,119,95,108,97,121,101,114,0,120,0,121,0,115,
104,111,114,116,121,0,116,111,116,112,111,105,110,116,0,109,105,110,116,97,98,108,101,0,109,97,120,116,97,98,108,101,0,101,120,116,95,105,110,91,50,93,0,101,120,116,95,111,117,116,91,50,93,0,42,116,97,98,108,101,0,42,112,114,101,109,117,108,116,97,98,108,101,0,112,114,101,109,117,108,95,101,120,116,95,105,110,91,50,93,0,112,114,101,109,117,108,95,101,120,116,95,111,117,116,91,50,93,0,100,101,102,97,117,108,116,95,104,97,110,
100,108,101,95,116,121,112,101,0,99,117,114,0,112,114,101,115,101,116,0,99,104,97,110,103,101,100,95,116,105,109,101,115,116,97,109,112,0,99,117,114,114,0,99,108,105,112,114,0,99,109,91,52,93,0,98,108,97,99,107,91,51,93,0,119,104,105,116,101,91,51,93,0,98,119,109,117,108,91,51,93,0,115,97,109,112,108,101,91,51,93,0,116,111,110,101,0,120,95,114,101,115,111,108,117,116,105,111,110,0,100,97,116,97,95,108,117,109,97,91,
50,53,54,93,0,100,97,116,97,95,114,91,50,53,54,93,0,100,97,116,97,95,103,91,50,53,54,93,0,100,97,116,97,95,98,91,50,53,54,93,0,100,97,116,97,95,97,91,50,53,54,93,0,120,109,97,120,0,121,109,97,120,0,99,111,91,50,93,91,50,93,0,111,107,0,115,97,109,112,108,101,95,102,117,108,108,0,115,97,109,112,108,101,95,108,105,110,101,115,0,119,97,118,101,102,114,109,95,109,111,100,101,0,118,101,99,115,99,111,112,
101,95,109,111,100,101,0,119,97,118,101,102,114,109,95,104,101,105,103,104,116,0,118,101,99,115,99,111,112,101,95,104,101,105,103,104,116,0,119,97,118,101,102,111,114,109,95,116,111,116,0,119,97,118,101,102,114,109,95,97,108,112,104,97,0,119,97,118,101,102,114,109,95,121,102,97,99,0,118,101,99,115,99,111,112,101,95,97,108,112,104,97,0,109,105,110,109,97,120,91,51,93,91,50,93,0,104,105,115,116,0,42,119,97,118,101,102,111,114,109,
95,49,0,42,119,97,118,101,102,111,114,109,95,50,0,42,119,97,118,101,102,111,114,109,95,51,0,42,118,101,99,115,99,111,112,101,0,42,118,101,99,115,99,111,112,101,95,114,103,98,0,108,111,111,107,91,54,52,93,0,118,105,101,119,95,116,114,97,110,115,102,111,114,109,91,54,52,93,0,101,120,112,111,115,117,114,101,0,103,97,109,109,97,0,42,99,117,114,118,101,95,109,97,112,112,105,110,103,0,42,95,112,97,100,50,0,100,105,115,112,
108,97,121,95,100,101,118,105,99,101,91,54,52,93,0,110,97,109,101,91,51,48,93,0,111,119,110,115,112,97,99,101,0,116,97,114,115,112,97,99,101,0,42,115,112,97,99,101,95,111,98,106,101,99,116,0,115,112,97,99,101,95,115,117,98,116,97,114,103,101,116,91,54,52,93,0,101,110,102,111,114,99,101,0,104,101,97,100,116,97,105,108,0,108,105,110,95,101,114,114,111,114,0,114,111,116,95,101,114,114,111,114,0,42,116,97,114,0,115,117,
98,116,97,114,103,101,116,91,54,52,93,0,109,97,116,114,105,120,91,52,93,91,52,93,0,115,112,97,99,101,0,114,111,116,79,114,100,101,114,0,42,116,101,120,116,0,116,97,114,110,117,109,0,116,97,114,103,101,116,115,0,105,116,101,114,97,116,105,111,110,115,0,114,111,111,116,98,111,110,101,0,109,97,120,95,114,111,111,116,98,111,110,101,0,42,112,111,108,101,116,97,114,0,112,111,108,101,115,117,98,116,97,114,103,101,116,91,54,52,93,
0,112,111,108,101,97,110,103,108,101,0,111,114,105,101,110,116,119,101,105,103,104,116,0,103,114,97,98,116,97,114,103,101,116,91,51,93,0,110,117,109,112,111,105,110,116,115,0,99,104,97,105,110,108,101,110,0,120,122,83,99,97,108,101,77,111,100,101,0,121,83,99,97,108,101,77,111,100,101,0,98,117,108,103,101,0,98,117,108,103,101,95,109,105,110,0,98,117,108,103,101,95,109,97,120,0,98,117,108,103,101,95,115,109,111,111,116,104,0,114,
101,115,101,114,118,101,100,49,0,114,101,115,101,114,118,101,100,50,0,101,117,108,101,114,95,111,114,100,101,114,0,109,105,120,95,109,111,100,101,0,112,111,119,101,114,0,109,105,110,109,97,120,102,108,97,103,0,108,111,99,97,108,0,101,118,97,108,95,116,105,109,101,0,116,114,97,99,107,102,108,97,103,0,108,111,99,107,102,108,97,103,0,111,102,102,115,101,116,95,102,97,99,0,102,111,108,108,111,119,102,108,97,103,0,117,112,102,108,97,103,
0,118,111,108,109,111,100,101,0,112,108,97,110,101,0,111,114,103,108,101,110,103,116,104,0,112,105,118,88,0,112,105,118,89,0,112,105,118,90,0,97,120,88,0,97,120,89,0,97,120,90,0,109,105,110,76,105,109,105,116,91,54,93,0,109,97,120,76,105,109,105,116,91,54,93,0,101,120,116,114,97,70,122,0,105,110,118,109,97,116,91,52,93,91,52,93,0,102,114,111,109,0,116,111,0,109,97,112,91,51,93,0,101,120,112,111,0,102,114,111,
109,95,114,111,116,97,116,105,111,110,95,109,111,100,101,0,116,111,95,101,117,108,101,114,95,111,114,100,101,114,0,109,105,120,95,109,111,100,101,95,108,111,99,0,109,105,120,95,109,111,100,101,95,114,111,116,0,109,105,120,95,109,111,100,101,95,115,99,97,108,101,0,102,114,111,109,95,109,105,110,91,51,93,0,102,114,111,109,95,109,97,120,91,51,93,0,116,111,95,109,105,110,91,51,93,0,116,111,95,109,97,120,91,51,93,0,102,114,111,109,
95,109,105,110,95,114,111,116,91,51,93,0,102,114,111,109,95,109,97,120,95,114,111,116,91,51,93,0,116,111,95,109,105,110,95,114,111,116,91,51,93,0,116,111,95,109,97,120,95,114,111,116,91,51,93,0,102,114,111,109,95,109,105,110,95,115,99,97,108,101,91,51,93,0,102,114,111,109,95,109,97,120,95,115,99,97,108,101,91,51,93,0,116,111,95,109,105,110,95,115,99,97,108,101,91,51,93,0,116,111,95,109,97,120,95,115,99,97,108,101,
91,51,93,0,111,102,102,115,101,116,91,51,93,0,114,111,116,65,120,105,115,0,120,109,105,110,0,121,109,105,110,0,122,109,105,110,0,122,109,97,120,0,115,111,102,116,0,42,116,97,114,103,101,116,0,115,104,114,105,110,107,84,121,112,101,0,112,114,111,106,65,120,105,115,0,112,114,111,106,65,120,105,115,83,112,97,99,101,0,112,114,111,106,76,105,109,105,116,0,115,104,114,105,110,107,77,111,100,101,0,116,114,97,99,107,65,120,105,115,0,
116,114,97,99,107,91,54,52,93,0,102,114,97,109,101,95,109,101,116,104,111,100,0,111,98,106,101,99,116,91,54,52,93,0,42,99,97,109,101,114,97,0,42,100,101,112,116,104,95,111,98,0,42,99,97,99,104,101,95,102,105,108,101,0,111,98,106,101,99,116,95,112,97,116,104,91,49,48,50,52,93,0,42,114,101,97,100,101,114,0,114,101,97,100,101,114,95,111,98,106,101,99,116,95,112,97,116,104,91,49,48,50,52,93,0,118,101,99,91,51,
93,91,51,93,0,97,108,102,97,0,114,97,100,105,117,115,0,105,112,111,0,104,49,0,104,50,0,102,51,0,104,105,100,101,0,101,97,115,105,110,103,0,98,97,99,107,0,112,101,114,105,111,100,0,97,117,116,111,95,104,97,110,100,108,101,95,116,121,112,101,0,118,101,99,91,52,93,0,95,112,97,100,49,91,49,93,0,109,97,116,95,110,114,0,112,110,116,115,117,0,112,110,116,115,118,0,114,101,115,111,108,117,0,114,101,115,111,108,118,0,
111,114,100,101,114,117,0,111,114,100,101,114,118,0,102,108,97,103,117,0,102,108,97,103,118,0,42,107,110,111,116,115,117,0,42,107,110,111,116,115,118,0,42,98,112,0,116,105,108,116,95,105,110,116,101,114,112,0,114,97,100,105,117,115,95,105,110,116,101,114,112,0,99,104,97,114,105,100,120,0,107,101,114,110,0,95,112,97,100,91,49,93,0,119,0,110,117,114,98,0,42,101,100,105,116,110,117,114,98,0,42,98,101,118,111,98,106,0,42,116,
97,112,101,114,111,98,106,0,42,116,101,120,116,111,110,99,117,114,118,101,0,42,107,101,121,0,42,42,109,97,116,0,42,98,101,118,101,108,95,112,114,111,102,105,108,101,0,116,101,120,102,108,97,103,0,116,119,105,115,116,95,109,111,100,101,0,116,119,105,115,116,95,115,109,111,111,116,104,0,115,109,97,108,108,99,97,112,115,95,115,99,97,108,101,0,112,97,116,104,108,101,110,0,98,101,118,114,101,115,111,108,0,116,111,116,99,111,108,0,119,
105,100,116,104,0,101,120,116,49,0,101,120,116,50,0,114,101,115,111,108,117,95,114,101,110,0,114,101,115,111,108,118,95,114,101,110,0,97,99,116,110,117,0,97,99,116,118,101,114,116,0,111,118,101,114,102,108,111,119,0,115,112,97,99,101,109,111,100,101,0,97,108,105,103,110,95,121,0,98,101,118,101,108,95,109,111,100,101,0,116,97,112,101,114,95,114,97,100,105,117,115,95,109,111,100,101,0,108,105,110,101,115,0,108,105,110,101,100,105,115,
116,0,102,115,105,122,101,0,119,111,114,100,115,112,97,99,101,0,117,108,112,111,115,0,117,108,104,101,105,103,104,116,0,120,111,102,0,121,111,102,0,108,105,110,101,119,105,100,116,104,0,112,111,115,0,115,101,108,115,116,97,114,116,0,115,101,108,101,110,100,0,108,101,110,95,119,99,104,97,114,0,42,115,116,114,0,42,101,100,105,116,102,111,110,116,0,102,97,109,105,108,121,91,54,52,93,0,42,118,102,111,110,116,0,42,118,102,111,110,116,
98,0,42,118,102,111,110,116,105,0,42,118,102,111,110,116,98,105,0,42,116,98,0,116,111,116,98,111,120,0,97,99,116,98,111,120,0,42,115,116,114,105,110,102,111,0,99,117,114,105,110,102,111,0,98,101,118,102,97,99,49,0,98,101,118,102,97,99,50,0,98,101,118,102,97,99,49,95,109,97,112,112,105,110,103,0,98,101,118,102,97,99,50,95,109,97,112,112,105,110,103,0,95,112,97,100,50,91,54,93,0,102,115,105,122,101,95,114,101,97,
108,116,105,109,101,0,42,99,117,114,118,101,95,101,118,97,108,0,101,100,105,116,95,100,97,116,97,95,102,114,111,109,95,111,114,105,103,105,110,97,108,0,95,112,97,100,51,91,55,93,0,42,98,97,116,99,104,95,99,97,99,104,101,0,104,49,95,108,111,99,91,50,93,0,104,50,95,108,111,99,91,50,93,0,42,112,114,111,102,105,108,101,0,112,97,116,104,95,108,101,110,0,115,101,103,109,101,110,116,115,95,108,101,110,0,42,115,101,103,109,
101,110,116,115,0,118,105,101,119,95,114,101,99,116,0,99,108,105,112,95,114,101,99,116,0,42,99,117,114,118,101,95,111,102,102,115,101,116,115,0,112,111,105,110,116,95,100,97,116,97,0,99,117,114,118,101,95,100,97,116,97,0,112,111,105,110,116,95,115,105,122,101,0,99,117,114,118,101,95,115,105,122,101,0,118,101,114,116,101,120,95,103,114,111,117,112,95,110,97,109,101,115,0,118,101,114,116,101,120,95,103,114,111,117,112,95,97,99,116,105,
118,101,95,105,110,100,101,120,0,103,101,111,109,101,116,114,121,0,97,116,116,114,105,98,117,116,101,115,95,97,99,116,105,118,101,95,105,110,100,101,120,0,115,121,109,109,101,116,114,121,0,115,101,108,101,99,116,105,111,110,95,100,111,109,97,105,110,0,42,115,117,114,102,97,99,101,0,42,115,117,114,102,97,99,101,95,117,118,95,109,97,112,0,97,99,116,105,118,101,0,97,99,116,105,118,101,95,114,110,100,0,97,99,116,105,118,101,95,99,108,
111,110,101,0,97,99,116,105,118,101,95,109,97,115,107,0,117,105,100,0,110,97,109,101,91,54,56,93,0,42,97,110,111,110,121,109,111,117,115,95,105,100,0,42,115,104,97,114,105,110,103,95,105,110,102,111,0,102,105,108,101,110,97,109,101,91,49,48,50,52,93,0,42,108,97,121,101,114,115,0,116,121,112,101,109,97,112,91,53,51,93,0,116,111,116,108,97,121,101,114,0,109,97,120,108,97,121,101,114,0,116,111,116,115,105,122,101,0,42,112,
111,111,108,0,42,101,120,116,101,114,110,97,108,0,118,109,97,115,107,0,101,109,97,115,107,0,102,109,97,115,107,0,112,109,97,115,107,0,108,109,97,115,107,0,42,99,97,110,118,97,115,0,42,98,114,117,115,104,95,103,114,111,117,112,0,42,112,111,105,110,116,99,97,99,104,101,0,112,116,99,97,99,104,101,115,0,99,117,114,114,101,110,116,95,102,114,97,109,101,0,102,111,114,109,97,116,0,100,105,115,112,95,116,121,112,101,0,105,109,97,
103,101,95,102,105,108,101,102,111,114,109,97,116,0,101,102,102,101,99,116,95,117,105,0,105,110,105,116,95,99,111,108,111,114,95,116,121,112,101,0,101,102,102,101,99,116,0,105,109,97,103,101,95,114,101,115,111,108,117,116,105,111,110,0,115,117,98,115,116,101,112,115,0,105,110,105,116,95,99,111,108,111,114,91,52,93,0,42,105,110,105,116,95,116,101,120,116,117,114,101,0,105,110,105,116,95,108,97,121,101,114,110,97,109,101,91,54,56,93,0,
100,114,121,95,115,112,101,101,100,0,100,105,115,115,95,115,112,101,101,100,0,99,111,108,111,114,95,100,114,121,95,116,104,114,101,115,104,111,108,100,0,100,101,112,116,104,95,99,108,97,109,112,0,100,105,115,112,95,102,97,99,116,111,114,0,115,112,114,101,97,100,95,115,112,101,101,100,0,99,111,108,111,114,95,115,112,114,101,97,100,95,115,112,101,101,100,0,115,104,114,105,110,107,95,115,112,101,101,100,0,100,114,105,112,95,118,101,108,0,100,
114,105,112,95,97,99,99,0,105,110,102,108,117,101,110,99,101,95,115,99,97,108,101,0,114,97,100,105,117,115,95,115,99,97,108,101,0,119,97,118,101,95,100,97,109,112,105,110,103,0,119,97,118,101,95,115,112,101,101,100,0,119,97,118,101,95,116,105,109,101,115,99,97,108,101,0,119,97,118,101,95,115,112,114,105,110,103,0,119,97,118,101,95,115,109,111,111,116,104,110,101,115,115,0,117,118,108,97,121,101,114,95,110,97,109,101,91,54,56,93,
0,105,109,97,103,101,95,111,117,116,112,117,116,95,112,97,116,104,91,49,48,50,52,93,0,111,117,116,112,117,116,95,110,97,109,101,91,54,56,93,0,111,117,116,112,117,116,95,110,97,109,101,50,91,54,56,93,0,42,112,109,100,0,115,117,114,102,97,99,101,115,0,97,99,116,105,118,101,95,115,117,114,0,101,114,114,111,114,91,54,52,93,0,42,112,115,121,115,0,99,111,108,108,105,115,105,111,110,0,114,0,103,0,98,0,119,101,116,110,101,
115,115,0,112,97,114,116,105,99,108,101,95,114,97,100,105,117,115,0,112,97,114,116,105,99,108,101,95,115,109,111,111,116,104,0,112,97,105,110,116,95,100,105,115,116,97,110,99,101,0,42,112,97,105,110,116,95,114,97,109,112,0,42,118,101,108,95,114,97,109,112,0,112,114,111,120,105,109,105,116,121,95,102,97,108,108,111,102,102,0,119,97,118,101,95,116,121,112,101,0,114,97,121,95,100,105,114,0,119,97,118,101,95,102,97,99,116,111,114,0,
119,97,118,101,95,99,108,97,109,112,0,109,97,120,95,118,101,108,111,99,105,116,121,0,115,109,117,100,103,101,95,115,116,114,101,110,103,116,104,0,98,117,116,116,121,112,101,0,115,116,121,112,101,0,118,101,114,116,103,114,111,117,112,0,117,115,101,114,106,105,116,0,115,116,97,0,108,105,102,101,116,105,109,101,0,116,111,116,112,97,114,116,0,116,111,116,107,101,121,0,115,101,101,100,0,110,111,114,109,102,97,99,0,111,98,102,97,99,0,114,
97,110,100,102,97,99,0,116,101,120,102,97,99,0,114,97,110,100,108,105,102,101,0,102,111,114,99,101,91,51,93,0,100,97,109,112,0,110,97,98,108,97,0,118,101,99,116,115,105,122,101,0,109,97,120,108,101,110,0,100,101,102,118,101,99,91,51,93,0,109,117,108,116,91,52,93,0,108,105,102,101,91,52,93,0,99,104,105,108,100,91,52,93,0,109,97,116,91,52,93,0,116,101,120,109,97,112,0,99,117,114,109,117,108,116,0,115,116,97,116,
105,99,115,116,101,112,0,111,109,97,116,0,116,105,109,101,116,101,120,0,115,112,101,101,100,116,101,120,0,102,108,97,103,50,110,101,103,0,100,105,115,112,0,118,101,114,116,103,114,111,117,112,95,118,0,118,103,114,111,117,112,110,97,109,101,91,54,52,93,0,118,103,114,111,117,112,110,97,109,101,95,118,91,54,52,93,0,105,109,97,116,91,52,93,91,52,93,0,42,107,101,121,115,0,115,116,97,114,116,120,0,115,116,97,114,116,121,0,110,97,
114,114,111,119,0,109,105,110,102,97,99,0,116,105,109,101,111,102,102,115,0,115,117,98,118,115,116,114,91,52,93,0,115,117,98,118,101,114,115,105,111,110,0,109,105,110,118,101,114,115,105,111,110,0,109,105,110,115,117,98,118,101,114,115,105,111,110,0,42,99,117,114,115,99,114,101,101,110,0,42,99,117,114,115,99,101,110,101,0,42,99,117,114,95,118,105,101,119,95,108,97,121,101,114,0,102,105,108,101,102,108,97,103,115,0,103,108,111,98,97,
108,102,0,98,117,105,108,100,95,99,111,109,109,105,116,95,116,105,109,101,115,116,97,109,112,0,98,117,105,108,100,95,104,97,115,104,91,49,54,93,0,42,102,109,100,0,42,102,108,117,105,100,0,42,102,108,117,105,100,95,111,108,100,0,42,102,108,117,105,100,95,109,117,116,101,120,0,42,102,108,117,105,100,95,103,114,111,117,112,0,42,102,111,114,99,101,95,103,114,111,117,112,0,42,101,102,102,101,99,116,111,114,95,103,114,111,117,112,0,42,
116,101,120,95,100,101,110,115,105,116,121,0,42,116,101,120,95,99,111,108,111,114,0,42,116,101,120,95,119,116,0,42,116,101,120,95,115,104,97,100,111,119,0,42,116,101,120,95,102,108,97,109,101,0,42,116,101,120,95,102,108,97,109,101,95,99,111,98,97,0,42,116,101,120,95,99,111,98,97,0,42,116,101,120,95,102,105,101,108,100,0,42,116,101,120,95,118,101,108,111,99,105,116,121,95,120,0,42,116,101,120,95,118,101,108,111,99,105,116,121,
95,121,0,42,116,101,120,95,118,101,108,111,99,105,116,121,95,122,0,42,116,101,120,95,102,108,97,103,115,0,42,116,101,120,95,114,97,110,103,101,95,102,105,101,108,100,0,42,103,117,105,100,105,110,103,95,112,97,114,101,110,116,0,112,48,91,51,93,0,112,49,91,51,93,0,100,112,48,91,51,93,0,99,101,108,108,95,115,105,122,101,91,51,93,0,103,108,111,98,97,108,95,115,105,122,101,91,51,93,0,112,114,101,118,95,108,111,99,91,51,
93,0,115,104,105,102,116,91,51,93,0,115,104,105,102,116,95,102,91,51,93,0,111,98,106,95,115,104,105,102,116,95,102,91,51,93,0,111,98,109,97,116,91,52,93,91,52,93,0,102,108,117,105,100,109,97,116,91,52,93,91,52,93,0,102,108,117,105,100,109,97,116,95,119,116,91,52,93,91,52,93,0,98,97,115,101,95,114,101,115,91,51,93,0,114,101,115,95,109,105,110,91,51,93,0,114,101,115,95,109,97,120,91,51,93,0,114,101,115,91,
51,93,0,116,111,116,97,108,95,99,101,108,108,115,0,100,120,0,98,111,117,110,100,97,114,121,95,119,105,100,116,104,0,103,114,97,118,105,116,121,95,102,105,110,97,108,91,51,93,0,97,100,97,112,116,95,109,97,114,103,105,110,0,97,100,97,112,116,95,114,101,115,0,97,100,97,112,116,95,116,104,114,101,115,104,111,108,100,0,109,97,120,114,101,115,0,115,111,108,118,101,114,95,114,101,115,0,98,111,114,100,101,114,95,99,111,108,108,105,115,
105,111,110,115,0,97,99,116,105,118,101,95,102,105,101,108,100,115,0,98,101,116,97,0,118,111,114,116,105,99,105,116,121,0,97,99,116,105,118,101,95,99,111,108,111,114,91,51,93,0,104,105,103,104,114,101,115,95,115,97,109,112,108,105,110,103,0,98,117,114,110,105,110,103,95,114,97,116,101,0,102,108,97,109,101,95,115,109,111,107,101,0,102,108,97,109,101,95,118,111,114,116,105,99,105,116,121,0,102,108,97,109,101,95,105,103,110,105,116,105,
111,110,0,102,108,97,109,101,95,109,97,120,95,116,101,109,112,0,102,108,97,109,101,95,115,109,111,107,101,95,99,111,108,111,114,91,51,93,0,110,111,105,115,101,95,115,116,114,101,110,103,116,104,0,110,111,105,115,101,95,112,111,115,95,115,99,97,108,101,0,110,111,105,115,101,95,116,105,109,101,95,97,110,105,109,0,114,101,115,95,110,111,105,115,101,91,51,93,0,110,111,105,115,101,95,115,99,97,108,101,0,95,112,97,100,51,91,52,93,0,
112,97,114,116,105,99,108,101,95,114,97,110,100,111,109,110,101,115,115,0,112,97,114,116,105,99,108,101,95,110,117,109,98,101,114,0,112,97,114,116,105,99,108,101,95,109,105,110,105,109,117,109,0,112,97,114,116,105,99,108,101,95,109,97,120,105,109,117,109,0,112,97,114,116,105,99,108,101,95,98,97,110,100,95,119,105,100,116,104,0,102,114,97,99,116,105,111,110,115,95,116,104,114,101,115,104,111,108,100,0,102,114,97,99,116,105,111,110,115,95,
100,105,115,116,97,110,99,101,0,102,108,105,112,95,114,97,116,105,111,0,115,121,115,95,112,97,114,116,105,99,108,101,95,109,97,120,105,109,117,109,0,115,105,109,117,108,97,116,105,111,110,95,109,101,116,104,111,100,0,95,112,97,100,52,91,54,93,0,118,105,115,99,111,115,105,116,121,95,118,97,108,117,101,0,95,112,97,100,53,91,52,93,0,115,117,114,102,97,99,101,95,116,101,110,115,105,111,110,0,118,105,115,99,111,115,105,116,121,95,98,
97,115,101,0,118,105,115,99,111,115,105,116,121,95,101,120,112,111,110,101,110,116,0,109,101,115,104,95,99,111,110,99,97,118,101,95,117,112,112,101,114,0,109,101,115,104,95,99,111,110,99,97,118,101,95,108,111,119,101,114,0,109,101,115,104,95,112,97,114,116,105,99,108,101,95,114,97,100,105,117,115,0,109,101,115,104,95,115,109,111,111,116,104,101,110,95,112,111,115,0,109,101,115,104,95,115,109,111,111,116,104,101,110,95,110,101,103,0,109,101,
115,104,95,115,99,97,108,101,0,109,101,115,104,95,103,101,110,101,114,97,116,111,114,0,95,112,97,100,54,91,50,93,0,112,97,114,116,105,99,108,101,95,116,121,112,101,0,112,97,114,116,105,99,108,101,95,115,99,97,108,101,0,115,110,100,112,97,114,116,105,99,108,101,95,116,97,117,95,109,105,110,95,119,99,0,115,110,100,112,97,114,116,105,99,108,101,95,116,97,117,95,109,97,120,95,119,99,0,115,110,100,112,97,114,116,105,99,108,101,95,
116,97,117,95,109,105,110,95,116,97,0,115,110,100,112,97,114,116,105,99,108,101,95,116,97,117,95,109,97,120,95,116,97,0,115,110,100,112,97,114,116,105,99,108,101,95,116,97,117,95,109,105,110,95,107,0,115,110,100,112,97,114,116,105,99,108,101,95,116,97,117,95,109,97,120,95,107,0,115,110,100,112,97,114,116,105,99,108,101,95,107,95,119,99,0,115,110,100,112,97,114,116,105,99,108,101,95,107,95,116,97,0,115,110,100,112,97,114,116,105,
99,108,101,95,107,95,98,0,115,110,100,112,97,114,116,105,99,108,101,95,107,95,100,0,115,110,100,112,97,114,116,105,99,108,101,95,108,95,109,105,110,0,115,110,100,112,97,114,116,105,99,108,101,95,108,95,109,97,120,0,115,110,100,112,97,114,116,105,99,108,101,95,112,111,116,101,110,116,105,97,108,95,114,97,100,105,117,115,0,115,110,100,112,97,114,116,105,99,108,101,95,117,112,100,97,116,101,95,114,97,100,105,117,115,0,115,110,100,112,97,
114,116,105,99,108,101,95,98,111,117,110,100,97,114,121,0,115,110,100,112,97,114,116,105,99,108,101,95,99,111,109,98,105,110,101,100,95,101,120,112,111,114,116,0,103,117,105,100,105,110,103,95,97,108,112,104,97,0,103,117,105,100,105,110,103,95,98,101,116,97,0,103,117,105,100,105,110,103,95,118,101,108,95,102,97,99,116,111,114,0,103,117,105,100,101,95,114,101,115,91,51,93,0,103,117,105,100,105,110,103,95,115,111,117,114,99,101,0,95,112,
97,100,56,91,50,93,0,99,97,99,104,101,95,102,114,97,109,101,95,115,116,97,114,116,0,99,97,99,104,101,95,102,114,97,109,101,95,101,110,100,0,99,97,99,104,101,95,102,114,97,109,101,95,112,97,117,115,101,95,100,97,116,97,0,99,97,99,104,101,95,102,114,97,109,101,95,112,97,117,115,101,95,110,111,105,115,101,0,99,97,99,104,101,95,102,114,97,109,101,95,112,97,117,115,101,95,109,101,115,104,0,99,97,99,104,101,95,102,114,97,
109,101,95,112,97,117,115,101,95,112,97,114,116,105,99,108,101,115,0,99,97,99,104,101,95,102,114,97,109,101,95,112,97,117,115,101,95,103,117,105,100,105,110,103,0,99,97,99,104,101,95,102,114,97,109,101,95,111,102,102,115,101,116,0,99,97,99,104,101,95,102,108,97,103,0,99,97,99,104,101,95,109,101,115,104,95,102,111,114,109,97,116,0,99,97,99,104,101,95,100,97,116,97,95,102,111,114,109,97,116,0,99,97,99,104,101,95,112,97,114,
116,105,99,108,101,95,102,111,114,109,97,116,0,99,97,99,104,101,95,110,111,105,115,101,95,102,111,114,109,97,116,0,99,97,99,104,101,95,100,105,114,101,99,116,111,114,121,91,49,48,50,52,93,0,99,97,99,104,101,95,116,121,112,101,0,99,97,99,104,101,95,105,100,91,52,93,0,95,112,97,100,57,91,50,93,0,116,105,109,101,95,116,111,116,97,108,0,116,105,109,101,95,112,101,114,95,102,114,97,109,101,0,102,114,97,109,101,95,108,101,
110,103,116,104,0,99,102,108,95,99,111,110,100,105,116,105,111,110,0,116,105,109,101,115,116,101,112,115,95,109,105,110,105,109,117,109,0,116,105,109,101,115,116,101,112,115,95,109,97,120,105,109,117,109,0,115,108,105,99,101,95,112,101,114,95,118,111,120,101,108,0,115,108,105,99,101,95,100,101,112,116,104,0,100,105,115,112,108,97,121,95,116,104,105,99,107,110,101,115,115,0,103,114,105,100,95,115,99,97,108,101,0,42,99,111,98,97,0,118,101,
99,116,111,114,95,115,99,97,108,101,0,103,114,105,100,108,105,110,101,115,95,108,111,119,101,114,95,98,111,117,110,100,0,103,114,105,100,108,105,110,101,115,95,117,112,112,101,114,95,98,111,117,110,100,0,103,114,105,100,108,105,110,101,115,95,114,97,110,103,101,95,99,111,108,111,114,91,52,93,0,97,120,105,115,95,115,108,105,99,101,95,109,101,116,104,111,100,0,115,108,105,99,101,95,97,120,105,115,0,115,104,111,119,95,103,114,105,100,108,105,
110,101,115,0,100,114,97,119,95,118,101,108,111,99,105,116,121,0,118,101,99,116,111,114,95,100,114,97,119,95,116,121,112,101,0,118,101,99,116,111,114,95,102,105,101,108,100,0,118,101,99,116,111,114,95,115,99,97,108,101,95,119,105,116,104,95,109,97,103,110,105,116,117,100,101,0,118,101,99,116,111,114,95,100,114,97,119,95,109,97,99,95,99,111,109,112,111,110,101,110,116,115,0,117,115,101,95,99,111,98,97,0,99,111,98,97,95,102,105,101,
108,100,0,105,110,116,101,114,112,95,109,101,116,104,111,100,0,103,114,105,100,108,105,110,101,115,95,99,111,108,111,114,95,102,105,101,108,100,0,103,114,105,100,108,105,110,101,115,95,99,101,108,108,95,102,105,108,116,101,114,0,95,112,97,100,49,48,91,51,93,0,118,101,108,111,99,105,116,121,95,115,99,97,108,101,0,111,112,101,110,118,100,98,95,99,111,109,112,114,101,115,115,105,111,110,0,99,108,105,112,112,105,110,103,0,111,112,101,110,118,
100,98,95,100,97,116,97,95,100,101,112,116,104,0,95,112,97,100,49,49,91,55,93,0,118,105,101,119,115,101,116,116,105,110,103,115,0,95,112,97,100,49,50,91,52,93,0,42,112,111,105,110,116,95,99,97,99,104,101,91,50,93,0,112,116,99,97,99,104,101,115,91,50,93,0,99,97,99,104,101,95,99,111,109,112,0,99,97,99,104,101,95,104,105,103,104,95,99,111,109,112,0,99,97,99,104,101,95,102,105,108,101,95,102,111,114,109,97,116,0,
95,112,97,100,49,51,91,55,93,0,42,109,101,115,104,0,42,110,111,105,115,101,95,116,101,120,116,117,114,101,0,42,118,101,114,116,115,95,111,108,100,0,110,117,109,118,101,114,116,115,0,118,101,108,95,109,117,108,116,105,0,118,101,108,95,110,111,114,109,97,108,0,118,101,108,95,114,97,110,100,111,109,0,118,101,108,95,99,111,111,114,100,91,51,93,0,102,117,101,108,95,97,109,111,117,110,116,0,116,101,109,112,101,114,97,116,117,114,101,0,
118,111,108,117,109,101,95,100,101,110,115,105,116,121,0,115,117,114,102,97,99,101,95,100,105,115,116,97,110,99,101,0,112,97,114,116,105,99,108,101,95,115,105,122,101,0,115,117,98,102,114,97,109,101,115,0,116,101,120,116,117,114,101,95,115,105,122,101,0,116,101,120,116,117,114,101,95,111,102,102,115,101,116,0,118,103,114,111,117,112,95,100,101,110,115,105,116,121,0,98,101,104,97,118,105,111,114,0,116,101,120,116,117,114,101,95,116,121,112,101,
0,95,112,97,100,52,91,51,93,0,103,117,105,100,105,110,103,95,109,111,100,101,0,115,101,108,101,99,116,105,111,110,0,113,105,0,113,105,95,115,116,97,114,116,0,113,105,95,101,110,100,0,101,100,103,101,95,116,121,112,101,115,0,101,120,99,108,117,100,101,95,101,100,103,101,95,116,121,112,101,115,0,42,108,105,110,101,115,116,121,108,101,0,105,115,95,100,105,115,112,108,97,121,101,100,0,109,111,100,117,108,101,115,0,114,97,121,99,97,115,
116,105,110,103,95,97,108,103,111,114,105,116,104,109,0,115,112,104,101,114,101,95,114,97,100,105,117,115,0,100,107,114,95,101,112,115,105,108,111,110,0,99,114,101,97,115,101,95,97,110,103,108,101,0,108,105,110,101,115,101,116,115,0,122,0,99,111,108,111,114,91,52,93,0,42,112,116,95,111,114,105,103,0,105,100,120,95,111,114,105,103,0,117,118,95,102,97,99,0,117,118,95,114,111,116,0,117,118,95,102,105,108,108,91,50,93,0,118,101,114,
116,95,99,111,108,111,114,91,52,93,0,118,101,114,116,115,91,51,93,0,105,110,102,111,91,54,52,93,0,102,105,108,108,91,52,93,0,98,101,122,116,0,112,111,105,110,116,95,105,110,100,101,120,0,42,99,117,114,118,101,95,112,111,105,110,116,115,0,116,111,116,95,99,117,114,118,101,95,112,111,105,110,116,115,0,116,109,112,95,108,97,121,101,114,105,110,102,111,91,49,50,56,93,0,109,117,108,116,105,95,102,114,97,109,101,95,102,97,108,108,
111,102,102,0,115,116,114,111,107,101,95,115,116,97,114,116,0,102,105,108,108,95,115,116,97,114,116,0,118,101,114,116,101,120,95,115,116,97,114,116,0,99,117,114,118,101,95,115,116,97,114,116,0,42,103,112,115,95,111,114,105,103,0,42,116,114,105,97,110,103,108,101,115,0,116,111,116,112,111,105,110,116,115,0,116,111,116,95,116,114,105,97,110,103,108,101,115,0,116,104,105,99,107,110,101,115,115,0,105,110,105,116,116,105,109,101,0,99,111,108,
111,114,110,97,109,101,91,49,50,56,93,0,99,97,112,115,91,50,93,0,102,105,108,108,95,111,112,97,99,105,116,121,95,102,97,99,0,98,111,117,110,100,98,111,120,95,109,105,110,91,51,93,0,98,111,117,110,100,98,111,120,95,109,97,120,91,51,93,0,117,118,95,114,111,116,97,116,105,111,110,0,117,118,95,116,114,97,110,115,108,97,116,105,111,110,91,50,93,0,117,118,95,115,99,97,108,101,0,115,101,108,101,99,116,95,105,110,100,101,120,
0,95,112,97,100,52,91,52,93,0,42,100,118,101,114,116,0,118,101,114,116,95,99,111,108,111,114,95,102,105,108,108,91,52,93,0,42,101,100,105,116,99,117,114,118,101,0,42,95,112,97,100,53,0,102,114,97,109,101,105,100,0,111,110,105,111,110,95,105,100,0,42,103,112,102,95,111,114,105,103,0,115,116,114,111,107,101,115,0,102,114,97,109,101,110,117,109,0,107,101,121,95,116,121,112,101,0,110,97,109,101,91,49,50,56,93,0,115,111,114,
116,95,105,110,100,101,120,0,42,103,112,108,95,111,114,105,103,0,102,114,97,109,101,115,0,42,97,99,116,102,114,97,109,101,0,111,110,105,111,110,95,102,108,97,103,0,105,110,102,111,91,49,50,56,93,0,112,97,115,115,95,105,110,100,101,120,0,105,110,118,101,114,115,101,91,52,93,91,52,93,0,112,97,114,115,117,98,115,116,114,91,54,52,93,0,112,97,114,116,121,112,101,0,108,105,110,101,95,99,104,97,110,103,101,0,116,105,110,116,99,
111,108,111,114,91,52,93,0,111,112,97,99,105,116,121,0,118,105,101,119,108,97,121,101,114,110,97,109,101,91,54,52,93,0,98,108,101,110,100,95,109,111,100,101,0,118,101,114,116,101,120,95,112,97,105,110,116,95,111,112,97,99,105,116,121,0,103,115,116,101,112,0,103,115,116,101,112,95,110,101,120,116,0,103,99,111,108,111,114,95,112,114,101,118,91,51,93,0,103,99,111,108,111,114,95,110,101,120,116,91,51,93,0,109,97,115,107,95,108,97,
121,101,114,115,0,97,99,116,95,109,97,115,107,0,108,111,99,97,116,105,111,110,91,51,93,0,114,111,116,97,116,105,111,110,91,51,93,0,115,99,97,108,101,91,51,93,0,108,97,121,101,114,95,109,97,116,91,52,93,91,52,93,0,108,97,121,101,114,95,105,110,118,109,97,116,91,52,93,91,52,93,0,42,115,98,117,102,102,101,114,0,42,115,98,117,102,102,101,114,95,112,111,115,105,116,105,111,110,95,98,117,102,0,42,115,98,117,102,102,101,
114,95,99,111,108,111,114,95,98,117,102,0,42,115,98,117,102,102,101,114,95,98,97,116,99,104,0,42,115,98,117,102,102,101,114,95,103,112,115,0,112,108,97,121,105,110,103,0,109,97,116,105,100,0,115,98,117,102,102,101,114,95,115,102,108,97,103,0,115,98,117,102,102,101,114,95,117,115,101,100,0,115,98,117,102,102,101,114,95,115,105,122,101,0,97,114,114,111,119,95,115,116,97,114,116,91,56,93,0,97,114,114,111,119,95,101,110,100,91,56,
93,0,97,114,114,111,119,95,115,116,97,114,116,95,115,116,121,108,101,0,97,114,114,111,119,95,101,110,100,95,115,116,121,108,101,0,116,111,116,95,99,112,95,112,111,105,110,116,115,0,42,99,112,95,112,111,105,110,116,115,0,42,115,98,117,102,102,101,114,95,98,114,117,115,104,0,42,103,112,101,110,99,105,108,95,99,97,99,104,101,0,42,108,105,110,101,97,114,116,95,99,97,99,104,101,0,42,117,112,100,97,116,101,95,99,97,99,104,101,0,
115,99,97,108,101,91,50,93,0,99,117,114,118,101,95,101,100,105,116,95,114,101,115,111,108,117,116,105,111,110,0,99,117,114,118,101,95,101,100,105,116,95,116,104,114,101,115,104,111,108,100,0,99,117,114,118,101,95,101,100,105,116,95,99,111,114,110,101,114,95,97,110,103,108,101,0,112,97,108,101,116,116,101,115,0,112,105,120,102,97,99,116,111,114,0,108,105,110,101,95,99,111,108,111,114,91,52,93,0,111,110,105,111,110,95,102,97,99,116,111,
114,0,111,110,105,111,110,95,109,111,100,101,0,122,100,101,112,116,104,95,111,102,102,115,101,116,0,116,111,116,102,114,97,109,101,0,116,111,116,115,116,114,111,107,101,0,100,114,97,119,95,109,111,100,101,0,111,110,105,111,110,95,107,101,121,116,121,112,101,0,115,101,108,101,99,116,95,108,97,115,116,95,105,110,100,101,120,0,103,114,105,100,0,42,101,114,114,111,114,0,109,111,100,105,102,105,101,114,0,108,97,121,101,114,110,97,109,101,91,54,
52,93,0,109,97,116,101,114,105,97,108,110,97,109,101,91,54,52,93,0,118,103,110,97,109,101,91,54,52,93,0,102,97,99,116,111,114,0,102,97,99,116,111,114,95,115,116,114,101,110,103,116,104,0,102,97,99,116,111,114,95,116,104,105,99,107,110,101,115,115,0,102,97,99,116,111,114,95,117,118,115,0,110,111,105,115,101,95,111,102,102,115,101,116,0,110,111,105,115,101,95,109,111,100,101,0,108,97,121,101,114,95,112,97,115,115,0,42,99,117,
114,118,101,95,105,110,116,101,110,115,105,116,121,0,116,104,105,99,107,110,101,115,115,95,102,97,99,0,42,99,117,114,118,101,95,116,104,105,99,107,110,101,115,115,0,42,103,112,109,100,0,115,101,103,95,115,116,97,114,116,0,115,101,103,95,101,110,100,0,115,101,103,95,109,111,100,101,0,115,101,103,95,114,101,112,101,97,116,0,102,114,97,109,101,95,115,99,97,108,101,0,115,101,103,109,101,110,116,95,97,99,116,105,118,101,95,105,110,100,101,
120,0,104,115,118,91,51,93,0,109,111,100,105,102,121,95,99,111,108,111,114,0,104,97,114,100,101,110,101,115,115,0,42,111,98,106,101,99,116,0,115,97,109,112,108,101,95,108,101,110,103,116,104,0,115,117,98,100,105,118,0,42,111,117,116,108,105,110,101,95,109,97,116,101,114,105,97,108,0,99,111,117,110,116,0,114,110,100,95,111,102,102,115,101,116,91,51,93,0,114,110,100,95,114,111,116,91,51,93,0,114,110,100,95,115,99,97,108,101,91,
51,93,0,109,97,116,95,114,112,108,0,115,116,97,114,116,95,100,101,108,97,121,0,116,114,97,110,115,105,116,105,111,110,0,116,105,109,101,95,97,108,105,103,110,109,101,110,116,0,115,112,101,101,100,95,102,97,99,0,115,112,101,101,100,95,109,97,120,103,97,112,0,116,105,109,101,95,109,111,100,101,0,112,101,114,99,101,110,116,97,103,101,95,102,97,99,0,102,97,100,101,95,102,97,99,0,116,97,114,103,101,116,95,118,103,110,97,109,101,91,
54,52,93,0,102,97,100,101,95,111,112,97,99,105,116,121,95,115,116,114,101,110,103,116,104,0,102,97,100,101,95,116,104,105,99,107,110,101,115,115,95,115,116,114,101,110,103,116,104,0,42,99,97,99,104,101,95,100,97,116,97,0,115,116,97,114,116,95,102,97,99,0,101,110,100,95,102,97,99,0,114,97,110,100,95,115,116,97,114,116,95,102,97,99,0,114,97,110,100,95,101,110,100,95,102,97,99,0,114,97,110,100,95,111,102,102,115,101,116,0,
111,118,101,114,115,104,111,111,116,95,102,97,99,0,112,111,105,110,116,95,100,101,110,115,105,116,121,0,115,101,103,109,101,110,116,95,105,110,102,108,117,101,110,99,101,0,109,97,120,95,97,110,103,108,101,0,42,100,109,100,0,100,97,115,104,0,103,97,112,0,100,97,115,104,95,111,102,102,115,101,116,0,102,97,108,108,111,102,102,95,116,121,112,101,0,112,97,114,101,110,116,105,110,118,91,52,93,91,52,93,0,99,101,110,116,91,51,93,0,102,
111,114,99,101,0,42,99,117,114,102,97,108,108,111,102,102,0,114,111,116,91,51,93,0,115,116,114,111,107,101,95,115,116,101,112,0,115,116,114,111,107,101,95,115,116,97,114,116,95,111,102,102,115,101,116,0,109,117,108,116,105,0,40,42,118,101,114,116,95,99,111,111,114,100,115,95,112,114,101,118,41,40,41,0,100,117,112,108,105,99,97,116,105,111,110,115,0,102,97,100,105,110,103,95,99,101,110,116,101,114,0,102,97,100,105,110,103,95,116,104,
105,99,107,110,101,115,115,0,102,97,100,105,110,103,95,111,112,97,99,105,116,121,0,42,99,111,108,111,114,98,97,110,100,0,117,118,95,111,102,102,115,101,116,0,102,105,108,108,95,114,111,116,97,116,105,111,110,0,102,105,108,108,95,111,102,102,115,101,116,91,50,93,0,102,105,108,108,95,115,99,97,108,101,0,102,105,116,95,109,101,116,104,111,100,0,97,108,105,103,110,109,101,110,116,95,114,111,116,97,116,105,111,110,0,109,105,110,95,119,101,
105,103,104,116,0,100,105,115,116,95,115,116,97,114,116,0,100,105,115,116,95,101,110,100,0,97,120,105,115,0,97,110,103,108,101,0,108,105,110,101,95,116,121,112,101,115,0,115,111,117,114,99,101,95,116,121,112,101,0,117,115,101,95,109,117,108,116,105,112,108,101,95,108,101,118,101,108,115,0,108,101,118,101,108,95,115,116,97,114,116,0,108,101,118,101,108,95,101,110,100,0,42,115,111,117,114,99,101,95,99,97,109,101,114,97,0,42,108,105,103,
104,116,95,99,111,110,116,111,117,114,95,111,98,106,101,99,116,0,42,115,111,117,114,99,101,95,111,98,106,101,99,116,0,42,115,111,117,114,99,101,95,99,111,108,108,101,99,116,105,111,110,0,42,116,97,114,103,101,116,95,109,97,116,101,114,105,97,108,0,116,97,114,103,101,116,95,108,97,121,101,114,91,54,52,93,0,115,111,117,114,99,101,95,118,101,114,116,101,120,95,103,114,111,117,112,91,54,52,93,0,111,118,101,114,115,99,97,110,0,115,
104,97,100,111,119,95,99,97,109,101,114,97,95,102,111,118,0,115,104,97,100,111,119,95,99,97,109,101,114,97,95,115,105,122,101,0,115,104,97,100,111,119,95,99,97,109,101,114,97,95,110,101,97,114,0,115,104,97,100,111,119,95,99,97,109,101,114,97,95,102,97,114,0,116,114,97,110,115,112,97,114,101,110,99,121,95,102,108,97,103,115,0,116,114,97,110,115,112,97,114,101,110,99,121,95,109,97,115,107,0,105,110,116,101,114,115,101,99,116,105,
111,110,95,109,97,115,107,0,115,104,97,100,111,119,95,115,101,108,101,99,116,105,111,110,0,115,105,108,104,111,117,101,116,116,101,95,115,101,108,101,99,116,105,111,110,0,99,114,101,97,115,101,95,116,104,114,101,115,104,111,108,100,0,97,110,103,108,101,95,115,112,108,105,116,116,105,110,103,95,116,104,114,101,115,104,111,108,100,0,99,104,97,105,110,95,115,109,111,111,116,104,95,116,111,108,101,114,97,110,99,101,0,99,104,97,105,110,105,110,103,
95,105,109,97,103,101,95,116,104,114,101,115,104,111,108,100,0,99,97,108,99,117,108,97,116,105,111,110,95,102,108,97,103,115,0,115,116,114,111,107,101,95,100,101,112,116,104,95,111,102,102,115,101,116,0,108,101,118,101,108,95,115,116,97,114,116,95,111,118,101,114,114,105,100,101,0,108,101,118,101,108,95,101,110,100,95,111,118,101,114,114,105,100,101,0,101,100,103,101,95,116,121,112,101,115,95,111,118,101,114,114,105,100,101,0,115,104,97,100,111,
119,95,115,101,108,101,99,116,105,111,110,95,111,118,101,114,114,105,100,101,0,115,104,97,100,111,119,95,117,115,101,95,115,105,108,104,111,117,101,116,116,101,95,111,118,101,114,114,105,100,101,0,42,108,97,95,100,97,116,97,95,112,116,114,0,42,97,117,120,95,116,97,114,103,101,116,0,107,101,101,112,95,100,105,115,116,0,115,104,114,105,110,107,95,116,121,112,101,0,115,104,114,105,110,107,95,111,112,116,115,0,115,104,114,105,110,107,95,109,111,
100,101,0,112,114,111,106,95,108,105,109,105,116,0,112,114,111,106,95,97,120,105,115,0,115,117,98,115,117,114,102,95,108,101,118,101,108,115,0,115,109,111,111,116,104,95,102,97,99,116,111,114,0,115,109,111,111,116,104,95,115,116,101,112,0,115,107,105,112,0,115,112,114,101,97,100,0,102,115,116,111,112,0,102,111,99,97,108,95,108,101,110,103,116,104,0,115,101,110,115,111,114,0,114,97,116,105,111,0,110,117,109,95,98,108,97,100,101,115,0,
104,105,103,104,95,113,117,97,108,105,116,121,0,42,105,100,95,114,101,102,101,114,101,110,99,101,0,100,114,97,119,105,110,103,95,105,110,100,101,120,0,42,118,97,108,117,101,115,0,42,108,97,121,101,114,95,110,97,109,101,0,102,114,97,109,101,115,95,115,116,111,114,97,103,101,0,109,97,115,107,115,0,97,99,116,105,118,101,95,109,97,115,107,95,105,110,100,101,120,0,42,112,97,114,115,117,98,115,116,114,0,116,114,97,110,115,108,97,116,105,
111,110,91,51,93,0,42,118,105,101,119,108,97,121,101,114,110,97,109,101,0,102,105,108,116,101,114,0,110,117,109,95,102,114,97,109,101,115,95,98,101,102,111,114,101,0,110,117,109,95,102,114,97,109,101,115,95,97,102,116,101,114,0,99,111,108,111,114,95,98,101,102,111,114,101,91,51,93,0,99,111,108,111,114,95,97,102,116,101,114,91,51,93,0,42,42,100,114,97,119,105,110,103,95,97,114,114,97,121,0,100,114,97,119,105,110,103,95,97,114,
114,97,121,95,115,105,122,101,0,42,114,111,111,116,95,103,114,111,117,112,95,112,116,114,0,108,97,121,101,114,115,95,100,97,116,97,0,42,97,99,116,105,118,101,95,110,111,100,101,0,42,42,109,97,116,101,114,105,97,108,95,97,114,114,97,121,0,109,97,116,101,114,105,97,108,95,97,114,114,97,121,95,115,105,122,101,0,95,112,97,100,51,91,50,93,0,111,110,105,111,110,95,115,107,105,110,110,105,110,103,95,115,101,116,116,105,110,103,115,0,
42,115,99,101,110,101,0,102,114,97,109,101,110,114,0,99,121,99,108,0,109,117,108,116,105,118,105,101,119,95,101,121,101,0,112,97,115,115,0,116,105,108,101,0,109,117,108,116,105,95,105,110,100,101,120,0,118,105,101,119,0,42,97,110,105,109,0,116,105,108,101,95,110,117,109,98,101,114,0,42,114,101,110,100,101,114,0,116,105,108,101,97,114,114,97,121,95,108,97,121,101,114,0,116,105,108,101,97,114,114,97,121,95,111,102,102,115,101,116,91,
50,93,0,116,105,108,101,97,114,114,97,121,95,115,105,122,101,91,50,93,0,103,101,110,95,120,0,103,101,110,95,121,0,103,101,110,95,116,121,112,101,0,103,101,110,95,102,108,97,103,0,103,101,110,95,100,101,112,116,104,0,103,101,110,95,99,111,108,111,114,91,52,93,0,108,97,98,101,108,91,54,52,93,0,42,99,97,99,104,101,95,109,117,116,101,120,0,42,112,97,114,116,105,97,108,95,117,112,100,97,116,101,95,114,101,103,105,115,116,101,
114,0,42,112,97,114,116,105,97,108,95,117,112,100,97,116,101,95,117,115,101,114,0,98,97,99,107,100,114,111,112,95,111,102,102,115,101,116,91,50,93,0,100,114,97,119,100,97,116,97,0,42,103,112,117,116,101,120,116,117,114,101,91,51,93,91,50,93,0,97,110,105,109,115,0,42,114,114,0,114,101,110,100,101,114,115,108,111,116,115,0,114,101,110,100,101,114,95,115,108,111,116,0,108,97,115,116,95,114,101,110,100,101,114,95,115,108,111,116,0,
108,97,115,116,102,114,97,109,101,0,103,112,117,102,114,97,109,101,110,114,0,103,112,117,102,108,97,103,0,103,112,117,95,112,97,115,115,0,103,112,117,95,108,97,121,101,114,0,103,112,117,95,118,105,101,119,0,115,101,97,109,95,109,97,114,103,105,110,0,112,97,99,107,101,100,102,105,108,101,115,0,108,97,115,116,117,115,101,100,0,97,115,112,120,0,97,115,112,121,0,99,111,108,111,114,115,112,97,99,101,95,115,101,116,116,105,110,103,115,0,
97,108,112,104,97,95,109,111,100,101,0,101,121,101,0,118,105,101,119,115,95,102,111,114,109,97,116,0,97,99,116,105,118,101,95,116,105,108,101,95,105,110,100,101,120,0,116,105,108,101,115,0,118,105,101,119,115,0,42,115,116,101,114,101,111,51,100,95,102,111,114,109,97,116,0,98,108,111,99,107,116,121,112,101,0,97,100,114,99,111,100,101,0,109,97,120,114,99,116,0,116,111,116,114,99,116,0,118,97,114,116,121,112,101,0,101,120,116,114,97,
112,0,98,105,116,109,97,115,107,0,115,108,105,100,101,95,109,105,110,0,115,108,105,100,101,95,109,97,120,0,99,117,114,118,101,0,115,104,111,119,107,101,121,0,109,117,116,101,105,112,111,0,114,101,108,97,116,105,118,101,0,116,111,116,101,108,101,109,0,118,103,114,111,117,112,91,54,52,93,0,115,108,105,100,101,114,109,105,110,0,115,108,105,100,101,114,109,97,120,0,42,114,101,102,107,101,121,0,101,108,101,109,115,116,114,91,51,50,93,0,
101,108,101,109,115,105,122,101,0,98,108,111,99,107,0,42,102,114,111,109,0,95,112,97,100,50,0,117,105,100,103,101,110,0,112,110,116,115,119,0,111,112,110,116,115,117,0,111,112,110,116,115,118,0,111,112,110,116,115,119,0,116,121,112,101,117,0,116,121,112,101,118,0,116,121,112,101,119,0,97,99,116,98,112,0,102,117,0,102,118,0,102,119,0,100,117,0,100,118,0,100,119,0,42,100,101,102,0,42,101,100,105,116,108,97,116,116,0,42,98,
97,115,101,95,111,114,105,103,0,108,97,121,0,102,108,97,103,95,102,114,111,109,95,99,111,108,108,101,99,116,105,111,110,0,102,108,97,103,95,108,101,103,97,99,121,0,108,111,99,97,108,95,118,105,101,119,95,98,105,116,115,0,108,111,99,97,108,95,99,111,108,108,101,99,116,105,111,110,115,95,98,105,116,115,0,42,101,110,103,105,110,101,95,116,121,112,101,0,40,42,102,114,101,101,41,40,41,0,114,117,110,116,105,109,101,95,102,108,97,103,
0,108,97,121,101,114,95,99,111,108,108,101,99,116,105,111,110,115,0,114,101,110,100,101,114,95,112,97,115,115,101,115,0,111,98,106,101,99,116,95,98,97,115,101,115,0,42,115,116,97,116,115,0,42,98,97,115,97,99,116,0,108,97,121,102,108,97,103,0,112,97,115,115,102,108,97,103,0,112,97,115,115,95,97,108,112,104,97,95,116,104,114,101,115,104,111,108,100,0,99,114,121,112,116,111,109,97,116,116,101,95,102,108,97,103,0,99,114,121,112,
116,111,109,97,116,116,101,95,108,101,118,101,108,115,0,115,97,109,112,108,101,115,0,42,109,97,116,95,111,118,101,114,114,105,100,101,0,42,119,111,114,108,100,95,111,118,101,114,114,105,100,101,0,42,105,100,95,112,114,111,112,101,114,116,105,101,115,0,102,114,101,101,115,116,121,108,101,95,99,111,110,102,105,103,0,101,101,118,101,101,0,97,111,118,115,0,42,97,99,116,105,118,101,95,97,111,118,0,108,105,103,104,116,103,114,111,117,112,115,0,
42,97,99,116,105,118,101,95,108,105,103,104,116,103,114,111,117,112,0,42,42,111,98,106,101,99,116,95,98,97,115,101,115,95,97,114,114,97,121,0,42,111,98,106,101,99,116,95,98,97,115,101,115,95,104,97,115,104,0,101,110,101,114,103,121,95,110,101,119,0,115,112,111,116,115,105,122,101,0,115,112,111,116,98,108,101,110,100,0,97,114,101,97,95,115,104,97,112,101,0,95,112,97,100,49,0,97,114,101,97,95,115,105,122,101,0,97,114,101,97,
95,115,105,122,101,121,0,97,114,101,97,95,115,105,122,101,122,0,97,114,101,97,95,115,112,114,101,97,100,0,115,117,110,95,97,110,103,108,101,0,115,104,100,119,114,0,115,104,100,119,103,0,115,104,100,119,98,0,112,114,95,116,101,120,116,117,114,101,0,117,115,101,95,110,111,100,101,115,0,98,105,97,115,0,99,97,115,99,97,100,101,95,109,97,120,95,100,105,115,116,0,99,97,115,99,97,100,101,95,101,120,112,111,110,101,110,116,0,99,97,
115,99,97,100,101,95,102,97,100,101,0,99,97,115,99,97,100,101,95,99,111,117,110,116,0,99,111,110,116,97,99,116,95,100,105,115,116,0,99,111,110,116,97,99,116,95,98,105,97,115,0,99,111,110,116,97,99,116,95,116,104,105,99,107,110,101,115,115,0,100,105,102,102,95,102,97,99,0,115,112,101,99,95,102,97,99,0,116,114,97,110,115,109,105,115,115,105,111,110,95,102,97,99,0,118,111,108,117,109,101,95,102,97,99,0,97,116,116,95,100,
105,115,116,0,115,104,97,100,111,119,95,102,105,108,116,101,114,95,114,97,100,105,117,115,0,115,104,97,100,111,119,95,109,97,120,105,109,117,109,95,114,101,115,111,108,117,116,105,111,110,0,115,104,97,100,111,119,95,106,105,116,116,101,114,95,111,118,101,114,98,108,117,114,0,42,110,111,100,101,116,114,101,101,0,101,110,101,114,103,121,0,97,116,116,101,110,117,97,116,105,111,110,95,116,121,112,101,0,112,97,114,97,108,108,97,120,95,116,121,112,
101,0,103,114,105,100,95,102,108,97,103,0,100,105,115,116,105,110,102,0,100,105,115,116,112,97,114,0,118,105,115,95,98,105,97,115,0,118,105,115,95,98,108,101,101,100,98,105,97,115,0,118,105,115,95,98,108,117,114,0,105,110,116,101,110,115,105,116,121,0,103,114,105,100,95,114,101,115,111,108,117,116,105,111,110,95,120,0,103,114,105,100,95,114,101,115,111,108,117,116,105,111,110,95,121,0,103,114,105,100,95,114,101,115,111,108,117,116,105,111,
110,95,122,0,103,114,105,100,95,98,97,107,101,95,115,97,109,112,108,101,115,0,103,114,105,100,95,115,117,114,102,97,99,101,95,98,105,97,115,0,103,114,105,100,95,101,115,99,97,112,101,95,98,105,97,115,0,103,114,105,100,95,110,111,114,109,97,108,95,98,105,97,115,0,103,114,105,100,95,118,105,101,119,95,98,105,97,115,0,103,114,105,100,95,102,97,99,105,110,103,95,98,105,97,115,0,103,114,105,100,95,118,97,108,105,100,105,116,121,95,
116,104,114,101,115,104,111,108,100,0,103,114,105,100,95,100,105,108,97,116,105,111,110,95,116,104,114,101,115,104,111,108,100,0,103,114,105,100,95,100,105,108,97,116,105,111,110,95,114,97,100,105,117,115,0,103,114,105,100,95,99,108,97,109,112,95,100,105,114,101,99,116,0,103,114,105,100,95,99,108,97,109,112,95,105,110,100,105,114,101,99,116,0,103,114,105,100,95,115,117,114,102,101,108,95,100,101,110,115,105,116,121,0,42,118,105,115,105,98,105,
108,105,116,121,95,103,114,112,0,100,97,116,97,95,100,105,115,112,108,97,121,95,115,105,122,101,0,112,111,115,105,116,105,111,110,91,51,93,0,97,116,116,101,110,117,97,116,105,111,110,95,102,97,99,0,97,116,116,101,110,117,97,116,105,111,110,109,97,116,91,52,93,91,52,93,0,112,97,114,97,108,108,97,120,109,97,116,91,52,93,91,52,93,0,109,97,116,91,52,93,91,52,93,0,114,101,115,111,108,117,116,105,111,110,91,51,93,0,99,111,
114,110,101,114,91,51,93,0,97,116,116,101,110,117,97,116,105,111,110,95,115,99,97,108,101,0,105,110,99,114,101,109,101,110,116,95,120,91,51,93,0,97,116,116,101,110,117,97,116,105,111,110,95,98,105,97,115,0,105,110,99,114,101,109,101,110,116,95,121,91,51,93,0,108,101,118,101,108,95,98,105,97,115,0,105,110,99,114,101,109,101,110,116,95,122,91,51,93,0,95,112,97,100,52,0,118,105,115,105,98,105,108,105,116,121,95,98,105,97,115,
0,118,105,115,105,98,105,108,105,116,121,95,98,108,101,101,100,0,118,105,115,105,98,105,108,105,116,121,95,114,97,110,103,101,0,95,112,97,100,53,0,42,116,101,120,0,116,101,120,95,115,105,122,101,91,51,93,0,100,97,116,97,95,116,121,112,101,0,99,111,109,112,111,110,101,110,116,115,0,118,101,114,115,105,111,110,0,99,117,98,101,95,108,101,110,0,103,114,105,100,95,108,101,110,0,109,105,112,115,95,108,101,110,0,118,105,115,95,114,101,
115,0,114,101,102,95,114,101,115,0,95,112,97,100,91,52,93,91,50,93,0,103,114,105,100,95,116,120,0,99,117,98,101,95,116,120,0,42,99,117,98,101,95,109,105,112,115,0,42,99,117,98,101,95,100,97,116,97,0,42,103,114,105,100,95,100,97,116,97,0,40,42,76,48,41,40,41,0,40,42,76,49,95,97,41,40,41,0,40,42,76,49,95,98,41,40,41,0,40,42,76,49,95,99,41,40,41,0,42,118,97,108,105,100,105,116,121,0,40,42,
118,105,114,116,117,97,108,95,111,102,102,115,101,116,41,40,41,0,42,76,48,0,42,76,49,95,97,0,42,76,49,95,98,0,42,76,49,95,99,0,100,97,116,97,95,108,97,121,111,117,116,0,98,108,111,99,107,95,108,101,110,0,98,108,111,99,107,95,115,105,122,101,0,42,98,108,111,99,107,95,105,110,102,111,115,0,98,97,107,105,110,103,0,105,114,114,97,100,105,97,110,99,101,0,118,105,115,105,98,105,108,105,116,121,0,99,111,110,110,101,
99,116,105,118,105,116,121,0,115,117,114,102,101,108,115,95,108,101,110,0,42,115,117,114,102,101,108,115,0,115,104,97,114,101,100,0,100,105,114,116,121,0,42,103,114,105,100,95,115,116,97,116,105,99,95,99,97,99,104,101,0,42,99,111,108,111,114,95,114,97,109,112,0,118,97,108,117,101,95,109,105,110,0,118,97,108,117,101,95,109,97,120,0,114,97,110,103,101,95,109,105,110,0,114,97,110,103,101,95,109,97,120,0,109,105,110,95,99,117,114,
118,97,116,117,114,101,0,109,97,120,95,99,117,114,118,97,116,117,114,101,0,109,105,110,95,116,104,105,99,107,110,101,115,115,0,109,97,120,95,116,104,105,99,107,110,101,115,115,0,109,105,110,95,97,110,103,108,101,0,109,97,116,95,97,116,116,114,0,115,97,109,112,108,105,110,103,0,101,114,114,111,114,0,119,97,118,101,108,101,110,103,116,104,0,111,99,116,97,118,101,115,0,102,114,101,113,117,101,110,99,121,0,98,97,99,107,98,111,110,101,
95,108,101,110,103,116,104,0,116,105,112,95,108,101,110,103,116,104,0,114,111,117,110,100,115,0,114,97,110,100,111,109,95,114,97,100,105,117,115,0,114,97,110,100,111,109,95,99,101,110,116,101,114,0,114,97,110,100,111,109,95,98,97,99,107,98,111,110,101,0,115,99,97,108,101,95,120,0,115,99,97,108,101,95,121,0,112,105,118,111,116,95,117,0,112,105,118,111,116,95,120,0,112,105,118,111,116,95,121,0,116,111,108,101,114,97,110,99,101,0,
111,114,105,101,110,116,97,116,105,111,110,0,116,104,105,99,107,110,101,115,115,95,112,111,115,105,116,105,111,110,0,116,104,105,99,107,110,101,115,115,95,114,97,116,105,111,0,99,97,112,115,0,99,104,97,105,110,105,110,103,0,115,112,108,105,116,95,108,101,110,103,116,104,0,109,105,110,95,108,101,110,103,116,104,0,109,97,120,95,108,101,110,103,116,104,0,99,104,97,105,110,95,99,111,117,110,116,0,115,112,108,105,116,95,100,97,115,104,49,0,
115,112,108,105,116,95,103,97,112,49,0,115,112,108,105,116,95,100,97,115,104,50,0,115,112,108,105,116,95,103,97,112,50,0,115,112,108,105,116,95,100,97,115,104,51,0,115,112,108,105,116,95,103,97,112,51,0,115,111,114,116,95,107,101,121,0,105,110,116,101,103,114,97,116,105,111,110,95,116,121,112,101,0,116,101,120,115,116,101,112,0,116,101,120,97,99,116,0,100,97,115,104,49,0,103,97,112,49,0,100,97,115,104,50,0,103,97,112,50,0,
100,97,115,104,51,0,103,97,112,51,0,112,97,110,101,108,0,42,109,116,101,120,91,49,56,93,0,99,111,108,111,114,95,109,111,100,105,102,105,101,114,115,0,97,108,112,104,97,95,109,111,100,105,102,105,101,114,115,0,116,104,105,99,107,110,101,115,115,95,109,111,100,105,102,105,101,114,115,0,103,101,111,109,101,116,114,121,95,109,111,100,105,102,105,101,114,115,0,109,97,115,107,108,97,121,101,114,115,0,109,97,115,107,108,97,121,95,97,99,116,
0,109,97,115,107,108,97,121,95,116,111,116,0,112,97,114,101,110,116,91,54,52,93,0,115,117,98,95,112,97,114,101,110,116,91,54,52,93,0,112,97,114,101,110,116,95,111,114,105,103,91,50,93,0,112,97,114,101,110,116,95,99,111,114,110,101,114,115,95,111,114,105,103,91,52,93,91,50,93,0,117,0,116,111,116,95,117,119,0,42,117,119,0,112,97,114,101,110,116,0,111,102,102,115,101,116,95,109,111,100,101,0,119,101,105,103,104,116,95,105,
110,116,101,114,112,0,116,111,116,95,112,111,105,110,116,0,42,112,111,105,110,116,115,95,100,101,102,111,114,109,0,116,111,116,95,118,101,114,116,0,115,112,108,105,110,101,115,0,115,112,108,105,110,101,115,95,115,104,97,112,101,115,0,42,97,99,116,95,115,112,108,105,110,101,0,42,97,99,116,95,112,111,105,110,116,0,98,108,101,110,100,95,102,108,97,103,0,114,101,115,116,114,105,99,116,102,108,97,103,0,42,105,109,97,103,101,95,117,115,101,
114,0,42,117,118,110,97,109,101,0,42,97,116,116,114,105,98,117,116,101,95,110,97,109,101,0,118,97,108,105,100,0,105,110,116,101,114,112,0,42,115,105,109,97,0,115,116,114,111,107,101,95,114,103,98,97,91,52,93,0,102,105,108,108,95,114,103,98,97,91,52,93,0,109,105,120,95,114,103,98,97,91,52,93,0,115,116,114,111,107,101,95,115,116,121,108,101,0,102,105,108,108,95,115,116,121,108,101,0,109,105,120,95,102,97,99,116,111,114,0,
103,114,97,100,105,101,110,116,95,97,110,103,108,101,0,103,114,97,100,105,101,110,116,95,114,97,100,105,117,115,0,103,114,97,100,105,101,110,116,95,115,99,97,108,101,91,50,93,0,103,114,97,100,105,101,110,116,95,115,104,105,102,116,91,50,93,0,116,101,120,116,117,114,101,95,97,110,103,108,101,0,116,101,120,116,117,114,101,95,115,99,97,108,101,91,50,93,0,116,101,120,116,117,114,101,95,111,102,102,115,101,116,91,50,93,0,116,101,120,116,
117,114,101,95,111,112,97,99,105,116,121,0,116,101,120,116,117,114,101,95,112,105,120,115,105,122,101,0,103,114,97,100,105,101,110,116,95,116,121,112,101,0,109,105,120,95,115,116,114,111,107,101,95,102,97,99,116,111,114,0,97,108,105,103,110,109,101,110,116,95,109,111,100,101,0,109,97,116,95,111,99,99,108,117,115,105,111,110,0,105,110,116,101,114,115,101,99,116,105,111,110,95,112,114,105,111,114,105,116,121,0,115,117,114,102,97,99,101,95,114,
101,110,100,101,114,95,109,101,116,104,111,100,0,97,0,115,112,101,99,114,0,115,112,101,99,103,0,115,112,101,99,98,0,114,97,121,95,109,105,114,114,111,114,0,115,112,101,99,0,103,108,111,115,115,95,109,105,114,0,114,111,117,103,104,110,101,115,115,0,109,101,116,97,108,108,105,99,0,112,114,95,116,121,112,101,0,112,114,95,102,108,97,103,0,108,105,110,101,95,99,111,108,91,52,93,0,108,105,110,101,95,112,114,105,111,114,105,116,121,0,
118,99,111,108,95,97,108,112,104,97,0,112,97,105,110,116,95,97,99,116,105,118,101,95,115,108,111,116,0,112,97,105,110,116,95,99,108,111,110,101,95,115,108,111,116,0,116,111,116,95,115,108,111,116,115,0,100,105,115,112,108,97,99,101,109,101,110,116,95,109,101,116,104,111,100,0,116,104,105,99,107,110,101,115,115,95,109,111,100,101,0,97,108,112,104,97,95,116,104,114,101,115,104,111,108,100,0,114,101,102,114,97,99,116,95,100,101,112,116,104,
0,98,108,101,110,100,95,109,101,116,104,111,100,0,98,108,101,110,100,95,115,104,97,100,111,119,0,118,111,108,117,109,101,95,105,110,116,101,114,115,101,99,116,105,111,110,95,109,101,116,104,111,100,0,105,110,102,108,97,116,101,95,98,111,117,110,100,115,0,42,116,101,120,112,97,105,110,116,115,108,111,116,0,103,112,117,109,97,116,101,114,105,97,108,0,42,103,112,95,115,116,121,108,101,0,108,105,110,101,97,114,116,0,116,111,116,101,100,103,101,
0,116,111,116,112,111,108,121,0,116,111,116,108,111,111,112,0,42,112,111,108,121,95,111,102,102,115,101,116,95,105,110,100,105,99,101,115,0,118,100,97,116,97,0,101,100,97,116,97,0,112,100,97,116,97,0,108,100,97,116,97,0,42,109,115,101,108,101,99,116,0,116,111,116,115,101,108,101,99,116,0,97,99,116,95,102,97,99,101,0,42,116,101,120,99,111,109,101,115,104,0,101,100,105,116,102,108,97,103,0,115,109,111,111,116,104,114,101,115,104,
0,114,101,109,101,115,104,95,118,111,120,101,108,95,115,105,122,101,0,114,101,109,101,115,104,95,118,111,120,101,108,95,97,100,97,112,116,105,118,105,116,121,0,102,97,99,101,95,115,101,116,115,95,99,111,108,111,114,95,115,101,101,100,0,102,97,99,101,95,115,101,116,115,95,99,111,108,111,114,95,100,101,102,97,117,108,116,0,42,97,99,116,105,118,101,95,99,111,108,111,114,95,97,116,116,114,105,98,117,116,101,0,42,100,101,102,97,117,108,116,
95,99,111,108,111,114,95,97,116,116,114,105,98,117,116,101,0,114,101,109,101,115,104,95,109,111,100,101,0,99,100,95,102,108,97,103,0,115,117,98,100,105,118,114,0,115,117,98,115,117,114,102,116,121,112,101,0,42,109,112,111,108,121,0,42,109,108,111,111,112,0,42,109,118,101,114,116,0,42,109,101,100,103,101,0,42,109,116,102,97,99,101,0,42,116,102,97,99,101,0,42,109,99,111,108,0,42,109,102,97,99,101,0,102,100,97,116,97,0,116,
111,116,102,97,99,101,0,42,116,112,97,103,101,0,117,118,91,52,93,91,50,93,0,99,111,108,91,52,93,0,116,114,97,110,115,112,0,117,110,119,114,97,112,0,102,0,105,0,115,91,50,53,53,93,0,115,95,108,101,110,0,100,101,102,95,110,114,0,42,100,119,0,116,111,116,119,101,105,103,104,116,0,114,97,100,105,117,115,91,51,93,0,116,111,116,100,105,115,112,0,40,42,100,105,115,112,115,41,40,41,0,42,104,105,100,100,101,110,0,118,
49,0,118,50,0,99,114,101,97,115,101,0,98,119,101,105,103,104,116,0,108,111,111,112,115,116,97,114,116,0,117,118,91,50,93,0,101,0,118,51,0,118,52,0,101,100,99,111,100,101,0,42,98,98,0,101,120,112,120,0,101,120,112,121,0,101,120,112,122,0,114,97,100,0,114,97,100,50,0,42,109,97,116,0,42,105,109,97,116,0,101,108,101,109,115,0,42,101,100,105,116,101,108,101,109,115,0,119,105,114,101,115,105,122,101,0,114,101,110,100,
101,114,115,105,122,101,0,116,104,114,101,115,104,0,42,108,97,115,116,101,108,101,109,0,101,120,101,99,117,116,105,111,110,95,116,105,109,101,0,108,97,121,111,117,116,95,112,97,110,101,108,95,111,112,101,110,95,102,108,97,103,0,112,101,114,115,105,115,116,101,110,116,95,117,105,100,0,42,116,101,120,116,117,114,101,0,42,109,97,112,95,111,98,106,101,99,116,0,109,97,112,95,98,111,110,101,91,54,52,93,0,117,118,108,97,121,101,114,95,116,
109,112,0,116,101,120,109,97,112,112,105,110,103,0,115,117,98,100,105,118,84,121,112,101,0,108,101,118,101,108,115,0,114,101,110,100,101,114,76,101,118,101,108,115,0,117,118,95,115,109,111,111,116,104,0,113,117,97,108,105,116,121,0,98,111,117,110,100,97,114,121,95,115,109,111,111,116,104,0,42,101,109,67,97,99,104,101,0,42,109,67,97,99,104,101,0,100,101,102,97,120,105,115,0,114,97,110,100,111,109,105,122,101,0,42,111,98,95,97,114,
109,0,116,104,114,101,115,104,111,108,100,0,42,115,116,97,114,116,95,99,97,112,0,42,101,110,100,95,99,97,112,0,42,99,117,114,118,101,95,111,98,0,42,111,102,102,115,101,116,95,111,98,0,109,101,114,103,101,95,100,105,115,116,0,102,105,116,95,116,121,112,101,0,111,102,102,115,101,116,95,116,121,112,101,0,117,118,95,111,102,102,115,101,116,91,50,93,0,98,105,115,101,99,116,95,116,104,114,101,115,104,111,108,100,0,117,115,101,95,99,
111,114,114,101,99,116,95,111,114,100,101,114,95,111,110,95,109,101,114,103,101,0,117,118,95,111,102,102,115,101,116,95,99,111,112,121,91,50,93,0,42,109,105,114,114,111,114,95,111,98,0,115,112,108,105,116,95,97,110,103,108,101,0,114,101,115,0,118,97,108,95,102,108,97,103,115,0,112,114,111,102,105,108,101,95,116,121,112,101,0,108,105,109,95,102,108,97,103,115,0,101,95,102,108,97,103,115,0,109,97,116,0,101,100,103,101,95,102,108,97,
103,115,0,102,97,99,101,95,115,116,114,95,109,111,100,101,0,109,105,116,101,114,95,105,110,110,101,114,0,109,105,116,101,114,95,111,117,116,101,114,0,118,109,101,115,104,95,109,101,116,104,111,100,0,97,102,102,101,99,116,95,116,121,112,101,0,112,114,111,102,105,108,101,0,98,101,118,101,108,95,97,110,103,108,101,0,100,101,102,103,114,112,95,110,97,109,101,91,54,52,93,0,42,99,117,115,116,111,109,95,112,114,111,102,105,108,101,0,42,100,
111,109,97,105,110,0,42,102,108,111,119,0,42,101,102,102,101,99,116,111,114,0,100,105,114,101,99,116,105,111,110,0,109,105,100,108,101,118,101,108,0,42,112,114,111,106,101,99,116,111,114,115,91,49,48,93,0,110,117,109,95,112,114,111,106,101,99,116,111,114,115,0,97,115,112,101,99,116,120,0,97,115,112,101,99,116,121,0,115,99,97,108,101,120,0,115,99,97,108,101,121,0,112,101,114,99,101,110,116,0,105,116,101,114,0,100,101,108,105,109,
105,116,0,115,121,109,109,101,116,114,121,95,97,120,105,115,0,100,101,102,103,114,112,95,102,97,99,116,111,114,0,102,97,99,101,95,99,111,117,110,116,0,102,97,99,0,42,111,98,106,101,99,116,99,101,110,116,101,114,0,42,95,112,97,100,52,0,42,105,110,100,101,120,97,114,0,116,111,116,105,110,100,101,120,0,42,99,108,111,116,104,79,98,106,101,99,116,0,42,115,105,109,95,112,97,114,109,115,0,42,99,111,108,108,95,112,97,114,109,115,
0,42,112,111,105,110,116,95,99,97,99,104,101,0,42,104,97,105,114,100,97,116,97,0,104,97,105,114,95,103,114,105,100,95,109,105,110,91,51,93,0,104,97,105,114,95,103,114,105,100,95,109,97,120,91,51,93,0,104,97,105,114,95,103,114,105,100,95,114,101,115,91,51,93,0,104,97,105,114,95,103,114,105,100,95,99,101,108,108,115,105,122,101,0,42,115,111,108,118,101,114,95,114,101,115,117,108,116,0,40,42,120,41,40,41,0,40,42,120,110,
101,119,41,40,41,0,40,42,120,111,108,100,41,40,41,0,40,42,99,117,114,114,101,110,116,95,120,110,101,119,41,40,41,0,40,42,99,117,114,114,101,110,116,95,120,41,40,41,0,40,42,99,117,114,114,101,110,116,95,118,41,40,41,0,40,42,118,101,114,116,95,116,114,105,115,41,40,41,0,109,118,101,114,116,95,110,117,109,0,116,114,105,95,110,117,109,0,116,105,109,101,95,120,0,116,105,109,101,95,120,110,101,119,0,105,115,95,115,116,97,
116,105,99,0,42,98,118,104,116,114,101,101,0,40,42,118,101,114,116,95,112,111,115,105,116,105,111,110,115,95,112,114,101,118,41,40,41,0,40,42,118,101,114,116,95,118,101,108,111,99,105,116,105,101,115,41,40,41,0,99,102,114,97,95,112,114,101,118,0,118,101,114,116,115,95,110,117,109,0,100,111,117,98,108,101,95,116,104,114,101,115,104,111,108,100,0,109,97,116,101,114,105,97,108,95,109,111,100,101,0,98,109,95,102,108,97,103,0,118,101,
114,116,101,120,0,116,111,116,105,110,102,108,117,101,110,99,101,0,103,114,105,100,115,105,122,101,0,42,98,105,110,100,105,110,102,108,117,101,110,99,101,115,0,42,98,105,110,100,111,102,102,115,101,116,115,0,42,98,105,110,100,99,97,103,101,99,111,115,0,116,111,116,99,97,103,101,118,101,114,116,0,42,100,121,110,103,114,105,100,0,42,100,121,110,105,110,102,108,117,101,110,99,101,115,0,42,100,121,110,118,101,114,116,115,0,100,121,110,103,114,
105,100,115,105,122,101,0,100,121,110,99,101,108,108,109,105,110,91,51,93,0,100,121,110,99,101,108,108,119,105,100,116,104,0,98,105,110,100,109,97,116,91,52,93,91,52,93,0,42,98,105,110,100,119,101,105,103,104,116,115,0,42,98,105,110,100,99,111,115,0,40,42,98,105,110,100,102,117,110,99,41,40,41,0,42,109,101,115,104,95,102,105,110,97,108,0,42,109,101,115,104,95,111,114,105,103,105,110,97,108,0,116,111,116,100,109,118,101,114,116,
0,116,111,116,100,109,101,100,103,101,0,116,111,116,100,109,102,97,99,101,0,112,115,121,115,0,112,111,115,105,116,105,111,110,0,114,97,110,100,111,109,95,112,111,115,105,116,105,111,110,0,114,97,110,100,111,109,95,114,111,116,97,116,105,111,110,0,112,97,114,116,105,99,108,101,95,97,109,111,117,110,116,0,112,97,114,116,105,99,108,101,95,111,102,102,115,101,116,0,105,110,100,101,120,95,108,97,121,101,114,95,110,97,109,101,91,54,56,93,0,
118,97,108,117,101,95,108,97,121,101,114,95,110,97,109,101,91,54,56,93,0,42,102,97,99,101,112,97,0,118,103,114,111,117,112,0,112,114,111,116,101,99,116,0,117,118,110,97,109,101,91,54,56,93,0,108,118,108,0,115,99,117,108,112,116,108,118,108,0,114,101,110,100,101,114,108,118,108,0,116,111,116,108,118,108,0,115,105,109,112,108,101,0,42,102,115,115,0,42,97,117,120,84,97,114,103,101,116,0,118,103,114,111,117,112,95,110,97,109,101,
91,54,52,93,0,107,101,101,112,68,105,115,116,0,115,104,114,105,110,107,79,112,116,115,0,115,117,98,115,117,114,102,76,101,118,101,108,115,0,42,111,114,105,103,105,110,0,108,105,109,105,116,91,50,93,0,100,101,102,111,114,109,95,97,120,105,115,0,115,104,101,108,108,95,100,101,102,103,114,112,95,110,97,109,101,91,54,52,93,0,114,105,109,95,100,101,102,103,114,112,95,110,97,109,101,91,54,52,93,0,111,102,102,115,101,116,95,102,97,99,
95,118,103,0,111,102,102,115,101,116,95,99,108,97,109,112,0,110,111,110,109,97,110,105,102,111,108,100,95,111,102,102,115,101,116,95,109,111,100,101,0,110,111,110,109,97,110,105,102,111,108,100,95,98,111,117,110,100,97,114,121,95,109,111,100,101,0,99,114,101,97,115,101,95,105,110,110,101,114,0,99,114,101,97,115,101,95,111,117,116,101,114,0,99,114,101,97,115,101,95,114,105,109,0,109,97,116,95,111,102,115,0,109,97,116,95,111,102,115,95,
114,105,109,0,109,101,114,103,101,95,116,111,108,101,114,97,110,99,101,0,98,101,118,101,108,95,99,111,110,118,101,120,0,42,111,98,95,97,120,105,115,0,115,116,101,112,115,0,114,101,110,100,101,114,95,115,116,101,112,115,0,115,99,114,101,119,95,111,102,115,0,42,111,99,101,97,110,0,42,111,99,101,97,110,99,97,99,104,101,0,114,101,115,111,108,117,116,105,111,110,0,118,105,101,119,112,111,114,116,95,114,101,115,111,108,117,116,105,111,110,
0,115,112,97,116,105,97,108,95,115,105,122,101,0,119,105,110,100,95,118,101,108,111,99,105,116,121,0,115,109,97,108,108,101,115,116,95,119,97,118,101,0,119,97,118,101,95,97,108,105,103,110,109,101,110,116,0,119,97,118,101,95,100,105,114,101,99,116,105,111,110,0,119,97,118,101,95,115,99,97,108,101,0,99,104,111,112,95,97,109,111,117,110,116,0,102,111,97,109,95,99,111,118,101,114,97,103,101,0,115,112,101,99,116,114,117,109,0,102,101,
116,99,104,95,106,111,110,115,119,97,112,0,115,104,97,114,112,101,110,95,112,101,97,107,95,106,111,110,115,119,97,112,0,98,97,107,101,115,116,97,114,116,0,98,97,107,101,101,110,100,0,99,97,99,104,101,112,97,116,104,91,49,48,50,52,93,0,102,111,97,109,108,97,121,101,114,110,97,109,101,91,54,56,93,0,115,112,114,97,121,108,97,121,101,114,110,97,109,101,91,54,56,93,0,99,97,99,104,101,100,0,103,101,111,109,101,116,114,121,95,
109,111,100,101,0,114,101,112,101,97,116,95,120,0,114,101,112,101,97,116,95,121,0,102,111,97,109,95,102,97,100,101,0,42,111,98,106,101,99,116,95,102,114,111,109,0,42,111,98,106,101,99,116,95,116,111,0,98,111,110,101,95,102,114,111,109,91,54,52,93,0,98,111,110,101,95,116,111,91,54,52,93,0,102,97,108,108,111,102,102,95,114,97,100,105,117,115,0,101,100,105,116,95,102,108,97,103,115,0,100,101,102,97,117,108,116,95,119,101,105,
103,104,116,0,42,99,109,97,112,95,99,117,114,118,101,0,97,100,100,95,116,104,114,101,115,104,111,108,100,0,114,101,109,95,116,104,114,101,115,104,111,108,100,0,109,97,115,107,95,99,111,110,115,116,97,110,116,0,109,97,115,107,95,100,101,102,103,114,112,95,110,97,109,101,91,54,52,93,0,109,97,115,107,95,116,101,120,95,117,115,101,95,99,104,97,110,110,101,108,0,42,109,97,115,107,95,116,101,120,116,117,114,101,0,42,109,97,115,107,95,
116,101,120,95,109,97,112,95,111,98,106,0,109,97,115,107,95,116,101,120,95,109,97,112,95,98,111,110,101,91,54,52,93,0,109,97,115,107,95,116,101,120,95,109,97,112,112,105,110,103,0,109,97,115,107,95,116,101,120,95,117,118,108,97,121,101,114,95,110,97,109,101,91,54,56,93,0,100,101,102,103,114,112,95,110,97,109,101,95,97,91,54,52,93,0,100,101,102,103,114,112,95,110,97,109,101,95,98,91,54,52,93,0,100,101,102,97,117,108,116,
95,119,101,105,103,104,116,95,97,0,100,101,102,97,117,108,116,95,119,101,105,103,104,116,95,98,0,109,105,120,95,115,101,116,0,112,114,111,120,105,109,105,116,121,95,109,111,100,101,0,112,114,111,120,105,109,105,116,121,95,102,108,97,103,115,0,42,112,114,111,120,105,109,105,116,121,95,111,98,95,116,97,114,103,101,116,0,109,105,110,95,100,105,115,116,0,109,97,120,95,100,105,115,116,0,42,98,114,117,115,104,0,104,101,114,109,105,116,101,95,
110,117,109,0,118,111,120,101,108,95,115,105,122,101,0,97,100,97,112,116,105,118,105,116,121,0,98,114,97,110,99,104,95,115,109,111,111,116,104,105,110,103,0,115,121,109,109,101,116,114,121,95,97,120,101,115,0,113,117,97,100,95,109,101,116,104,111,100,0,110,103,111,110,95,109,101,116,104,111,100,0,109,105,110,95,118,101,114,116,105,99,101,115,0,108,97,109,98,100,97,0,108,97,109,98,100,97,95,98,111,114,100,101,114,0,40,42,100,101,108,
116,97,115,41,40,41,0,100,101,108,116,97,115,95,110,117,109,0,115,109,111,111,116,104,95,116,121,112,101,0,114,101,115,116,95,115,111,117,114,99,101,0,40,42,98,105,110,100,95,99,111,111,114,100,115,41,40,41,0,98,105,110,100,95,99,111,111,114,100,115,95,110,117,109,0,100,101,108,116,97,95,99,97,99,104,101,0,97,120,105,115,95,117,0,97,120,105,115,95,118,0,99,101,110,116,101,114,91,50,93,0,42,111,98,106,101,99,116,95,115,
114,99,0,98,111,110,101,95,115,114,99,91,54,52,93,0,42,111,98,106,101,99,116,95,100,115,116,0,98,111,110,101,95,100,115,116,91,54,52,93,0,112,108,97,121,95,109,111,100,101,0,102,108,105,112,95,97,120,105,115,0,100,101,102,111,114,109,95,109,111,100,101,0,101,118,97,108,95,102,114,97,109,101,0,101,118,97,108,95,102,97,99,116,111,114,0,97,110,99,104,111,114,95,103,114,112,95,110,97,109,101,91,54,52,93,0,116,111,116,97,
108,95,118,101,114,116,115,0,42,118,101,114,116,101,120,99,111,0,42,99,97,99,104,101,95,115,121,115,116,101,109,0,99,114,101,97,115,101,95,119,101,105,103,104,116,0,42,111,98,95,115,111,117,114,99,101,0,100,97,116,97,95,116,121,112,101,115,0,118,109,97,112,95,109,111,100,101,0,101,109,97,112,95,109,111,100,101,0,108,109,97,112,95,109,111,100,101,0,112,109,97,112,95,109,111,100,101,0,109,97,112,95,109,97,120,95,100,105,115,116,
97,110,99,101,0,109,97,112,95,114,97,121,95,114,97,100,105,117,115,0,105,115,108,97,110,100,115,95,112,114,101,99,105,115,105,111,110,0,108,97,121,101,114,115,95,115,101,108,101,99,116,95,115,114,99,91,53,93,0,108,97,121,101,114,115,95,115,101,108,101,99,116,95,100,115,116,91,53,93,0,109,105,120,95,108,105,109,105,116,0,114,101,97,100,95,102,108,97,103,0,42,118,101,114,116,95,105,110,100,115,0,42,118,101,114,116,95,119,101,105,
103,104,116,115,0,110,111,114,109,97,108,95,100,105,115,116,0,42,98,105,110,100,115,0,110,117,109,98,105,110,100,115,0,118,101,114,116,101,120,95,105,100,120,0,42,118,101,114,116,115,0,110,117,109,95,109,101,115,104,95,118,101,114,116,115,0,116,97,114,103,101,116,95,118,101,114,116,115,95,110,117,109,0,110,117,109,112,111,108,121,0,42,105,100,95,110,97,109,101,0,42,108,105,98,95,110,97,109,101,0,98,97,107,101,95,109,111,100,101,0,
42,100,105,114,101,99,116,111,114,121,0,100,97,116,97,95,98,108,111,99,107,115,95,110,117,109,0,97,99,116,105,118,101,95,100,97,116,97,95,98,108,111,99,107,0,42,100,97,116,97,95,98,108,111,99,107,115,0,42,110,111,100,101,95,103,114,111,117,112,0,115,101,116,116,105,110,103,115,0,42,115,105,109,117,108,97,116,105,111,110,95,98,97,107,101,95,100,105,114,101,99,116,111,114,121,0,98,97,107,101,115,95,110,117,109,0,42,98,97,107,
101,115,0,112,97,110,101,108,115,95,110,117,109,0,42,112,97,110,101,108,115,0,114,101,115,111,108,117,116,105,111,110,95,109,111,100,101,0,118,111,120,101,108,95,97,109,111,117,110,116,0,105,110,116,101,114,105,111,114,95,98,97,110,100,95,119,105,100,116,104,0,42,116,101,120,116,117,114,101,95,109,97,112,95,111,98,106,101,99,116,0,116,101,120,116,117,114,101,95,109,97,112,95,109,111,100,101,0,116,101,120,116,117,114,101,95,109,105,100,95,
108,101,118,101,108,91,51,93,0,116,101,120,116,117,114,101,95,115,97,109,112,108,101,95,114,97,100,105,117,115,0,103,114,105,100,95,110,97,109,101,91,54,52,93,0,108,97,121,101,114,95,110,97,109,101,91,54,52,93,0,109,97,116,101,114,105,97,108,95,112,97,115,115,0,118,101,114,116,101,120,95,103,114,111,117,112,95,110,97,109,101,91,54,52,93,0,42,99,117,115,116,111,109,95,99,117,114,118,101,0,99,111,108,111,114,95,102,97,99,116,
111,114,0,104,97,114,100,110,101,115,115,95,102,97,99,116,111,114,0,95,112,97,100,91,56,93,0,116,105,110,116,95,109,111,100,101,0,115,116,114,111,107,101,95,108,111,99,91,51,93,0,115,116,114,111,107,101,95,114,111,116,91,51,93,0,115,116,114,111,107,101,95,115,99,97,108,101,91,51,93,0,42,115,101,103,109,101,110,116,115,95,97,114,114,97,121,0,115,101,103,109,101,110,116,115,95,110,117,109,0,109,97,115,107,95,115,119,105,116,99,
104,101,115,0,109,97,116,101,114,105,97,108,95,109,97,115,107,95,98,105,116,115,0,42,115,104,97,114,101,100,95,99,97,99,104,101,0,115,101,103,109,101,110,116,95,115,116,97,114,116,0,115,101,103,109,101,110,116,95,101,110,100,0,115,101,103,109,101,110,116,95,109,111,100,101,0,115,101,103,109,101,110,116,95,114,101,112,101,97,116,0,114,101,110,100,101,114,95,115,105,122,101,0,114,101,110,100,101,114,95,102,108,97,103,0,100,105,114,91,55,
54,56,93,0,116,99,0,98,117,105,108,100,95,115,105,122,101,95,102,108,97,103,0,98,117,105,108,100,95,116,99,95,102,108,97,103,0,117,115,101,114,0,42,103,112,117,116,101,120,116,117,114,101,91,51,93,0,103,112,117,116,101,120,116,117,114,101,115,0,108,97,115,116,115,105,122,101,91,50,93,0,42,103,112,100,0,116,114,97,99,107,105,110,103,0,42,116,114,97,99,107,105,110,103,95,99,111,110,116,101,120,116,0,112,114,111,120,121,0,117,
115,101,95,116,114,97,99,107,95,109,97,115,107,0,116,114,97,99,107,95,112,114,101,118,105,101,119,95,104,101,105,103,104,116,0,102,114,97,109,101,95,119,105,100,116,104,0,102,114,97,109,101,95,104,101,105,103,104,116,0,117,110,100,105,115,116,95,109,97,114,107,101,114,0,42,116,114,97,99,107,95,115,101,97,114,99,104,0,42,116,114,97,99,107,95,112,114,101,118,105,101,119,0,116,114,97,99,107,95,112,111,115,91,50,93,0,116,114,97,99,
107,95,100,105,115,97,98,108,101,100,0,116,114,97,99,107,95,108,111,99,107,101,100,0,115,99,101,110,101,95,102,114,97,109,101,110,114,0,42,116,114,97,99,107,0,42,109,97,114,107,101,114,0,115,108,105,100,101,95,115,99,97,108,101,91,50,93,0,99,104,97,110,110,101,108,91,51,50,93,0,110,111,105,115,101,115,105,122,101,0,116,117,114,98,117,108,0,110,111,95,114,111,116,95,97,120,105,115,0,115,116,114,105,100,101,95,97,120,105,115,
0,99,117,114,109,111,100,0,97,99,116,111,102,102,115,0,115,116,114,105,100,101,108,101,110,0,115,116,114,105,100,101,99,104,97,110,110,101,108,91,51,50,93,0,111,102,102,115,95,98,111,110,101,91,51,50,93,0,105,116,101,109,95,116,121,112,101,0,105,116,101,109,0,42,115,111,99,107,101,116,95,116,121,112,101,0,97,116,116,114,105,98,117,116,101,95,100,111,109,97,105,110,0,100,101,102,97,117,108,116,95,105,110,112,117,116,0,42,100,101,
102,97,117,108,116,95,97,116,116,114,105,98,117,116,101,95,110,97,109,101,0,42,115,111,99,107,101,116,95,100,97,116,97,0,42,42,105,116,101,109,115,95,97,114,114,97,121,0,105,116,101,109,115,95,110,117,109,0,105,100,101,110,116,105,102,105,101,114,0,114,111,111,116,95,112,97,110,101,108,0,97,99,116,105,118,101,95,105,110,100,101,120,0,110,101,120,116,95,117,105,100,0,104,97,115,105,110,112,117,116,0,104,97,115,111,117,116,112,117,116,
0,100,97,116,97,116,121,112,101,0,115,111,99,107,101,116,116,121,112,101,0,105,115,95,99,111,112,121,0,101,120,116,101,114,110,97,108,0,105,100,101,110,116,105,102,105,101,114,91,54,52,93,0,108,105,109,105,116,0,105,110,95,111,117,116,0,42,116,121,112,101,105,110,102,111,0,115,116,97,99,107,95,105,110,100,101,120,0,100,105,115,112,108,97,121,95,115,104,97,112,101,0,115,104,111,114,116,95,108,97,98,101,108,91,54,52,93,0,100,101,
115,99,114,105,112,116,105,111,110,91,54,52,93,0,111,119,110,95,105,110,100,101,120,0,116,111,95,105,110,100,101,120,0,42,108,105,110,107,0,110,115,0,105,110,112,117,116,115,0,111,117,116,112,117,116,115,0,117,105,95,111,114,100,101,114,0,99,117,115,116,111,109,49,0,99,117,115,116,111,109,50,0,99,117,115,116,111,109,51,0,99,117,115,116,111,109,52,0,108,111,99,120,0,108,111,99,121,0,111,102,102,115,101,116,120,0,111,102,102,115,
101,116,121,0,110,117,109,95,112,97,110,101,108,95,115,116,97,116,101,115,0,42,112,97,110,101,108,95,115,116,97,116,101,115,95,97,114,114,97,121,0,42,102,114,111,109,110,111,100,101,0,42,116,111,110,111,100,101,0,42,102,114,111,109,115,111,99,107,0,42,116,111,115,111,99,107,0,109,117,108,116,105,95,105,110,112,117,116,95,115,111,99,107,101,116,95,105,110,100,101,120,0,110,111,100,101,95,105,100,0,105,100,95,105,110,95,110,111,100,101,
0,112,97,116,104,0,118,105,101,119,95,99,101,110,116,101,114,91,50,93,0,110,111,100,101,115,0,108,105,110,107,115,0,99,117,114,95,105,110,100,101,120,0,99,104,117,110,107,115,105,122,101,0,101,120,101,99,117,116,105,111,110,95,109,111,100,101,0,118,105,101,119,101,114,95,98,111,114,100,101,114,0,116,114,101,101,95,105,110,116,101,114,102,97,99,101,0,42,112,114,101,118,105,101,119,115,0,97,99,116,105,118,101,95,118,105,101,119,101,114,
95,107,101,121,0,110,101,115,116,101,100,95,110,111,100,101,95,114,101,102,115,95,110,117,109,0,42,110,101,115,116,101,100,95,110,111,100,101,95,114,101,102,115,0,42,103,101,111,109,101,116,114,121,95,110,111,100,101,95,97,115,115,101,116,95,116,114,97,105,116,115,0,118,97,108,117,101,91,51,93,0,118,97,108,117,101,95,101,117,108,101,114,91,51,93,0,118,97,108,117,101,91,52,93,0,118,97,108,117,101,91,49,48,50,52,93,0,42,118,97,
108,117,101,0,108,97,98,101,108,95,115,105,122,101,0,110,114,0,99,121,99,108,105,99,0,109,111,118,105,101,0,115,97,116,117,114,97,116,105,111,110,0,99,111,110,116,114,97,115,116,0,103,97,105,110,0,108,105,102,116,0,109,97,115,116,101,114,0,115,104,97,100,111,119,115,0,109,105,100,116,111,110,101,115,0,104,105,103,104,108,105,103,104,116,115,0,115,116,97,114,116,109,105,100,116,111,110,101,115,0,101,110,100,109,105,100,116,111,110,101,
115,0,102,108,97,112,115,0,114,111,117,110,100,105,110,103,0,99,97,116,97,100,105,111,112,116,114,105,99,0,108,101,110,115,115,104,105,102,116,0,112,97,115,115,95,110,97,109,101,91,54,52,93,0,115,105,122,101,120,0,115,105,122,101,121,0,109,97,120,115,112,101,101,100,0,109,105,110,115,112,101,101,100,0,97,115,112,101,99,116,0,99,117,114,118,101,100,0,112,101,114,99,101,110,116,120,0,112,101,114,99,101,110,116,121,0,102,105,108,116,
101,114,116,121,112,101,0,98,111,107,101,104,0,105,109,97,103,101,95,105,110,95,119,105,100,116,104,0,105,109,97,103,101,95,105,110,95,104,101,105,103,104,116,0,99,101,110,116,101,114,95,120,0,99,101,110,116,101,114,95,121,0,115,112,105,110,0,122,111,111,109,0,115,105,103,109,97,95,99,111,108,111,114,0,115,105,103,109,97,95,115,112,97,99,101,0,118,97,114,105,97,116,105,111,110,0,117,110,105,102,111,114,109,105,116,121,0,115,104,97,
114,112,110,101,115,115,0,101,99,99,101,110,116,114,105,99,105,116,121,0,104,105,103,104,95,112,114,101,99,105,115,105,111,110,0,99,111,110,116,114,97,115,116,95,108,105,109,105,116,0,99,111,114,110,101,114,95,114,111,117,110,100,105,110,103,0,104,117,101,0,115,97,116,0,105,109,95,102,111,114,109,97,116,0,98,97,115,101,95,112,97,116,104,91,49,48,50,52,93,0,97,99,116,105,118,101,95,105,110,112,117,116,0,117,115,101,95,114,101,110,
100,101,114,95,102,111,114,109,97,116,0,117,115,101,95,110,111,100,101,95,102,111,114,109,97,116,0,115,97,118,101,95,97,115,95,114,101,110,100,101,114,0,112,97,116,104,91,49,48,50,52,93,0,108,97,121,101,114,91,51,48,93,0,116,49,0,116,50,0,116,51,0,102,115,116,114,101,110,103,116,104,0,102,97,108,112,104,97,0,107,101,121,91,52,93,0,97,108,103,111,114,105,116,104,109,0,99,104,97,110,110,101,108,0,120,49,0,120,50,0,
121,49,0,121,50,0,102,97,99,95,120,49,0,102,97,99,95,120,50,0,102,97,99,95,121,49,0,102,97,99,95,121,50,0,121,99,99,95,109,111,100,101,0,98,107,116,121,112,101,0,112,114,101,118,105,101,119,0,103,97,109,99,111,0,110,111,95,122,98,117,102,0,109,97,120,98,108,117,114,0,98,116,104,114,101,115,104,0,42,100,105,99,116,0,42,110,111,100,101,0,115,116,97,114,95,52,53,0,115,116,114,101,97,107,115,0,99,111,108,109,
111,100,0,109,105,120,0,102,97,100,101,0,97,110,103,108,101,95,111,102,115,0,107,101,121,0,109,0,99,0,106,105,116,0,112,114,111,106,0,102,105,116,0,115,108,111,112,101,91,51,93,0,112,111,119,101,114,91,51,93,0,111,102,102,115,101,116,95,98,97,115,105,115,0,108,105,102,116,91,51,93,0,103,97,109,109,97,91,51,93,0,103,97,105,110,91,51,93,0,108,105,109,99,104,97,110,0,117,110,115,112,105,108,108,0,108,105,109,115,99,
97,108,101,0,117,115,112,105,108,108,114,0,117,115,112,105,108,108,103,0,117,115,112,105,108,108,98,0,102,114,111,109,95,99,111,108,111,114,95,115,112,97,99,101,91,54,52,93,0,116,111,95,99,111,108,111,114,95,115,112,97,99,101,91,54,52,93,0,115,105,122,101,95,120,0,115,105,122,101,95,121,0,116,101,120,95,109,97,112,112,105,110,103,0,99,111,108,111,114,95,109,97,112,112,105,110,103,0,115,107,121,95,109,111,100,101,108,0,115,117,
110,95,100,105,114,101,99,116,105,111,110,91,51,93,0,116,117,114,98,105,100,105,116,121,0,103,114,111,117,110,100,95,97,108,98,101,100,111,0,115,117,110,95,115,105,122,101,0,115,117,110,95,105,110,116,101,110,115,105,116,121,0,115,117,110,95,101,108,101,118,97,116,105,111,110,0,115,117,110,95,114,111,116,97,116,105,111,110,0,97,108,116,105,116,117,100,101,0,97,105,114,95,100,101,110,115,105,116,121,0,100,117,115,116,95,100,101,110,115,105,
116,121,0,111,122,111,110,101,95,100,101,110,115,105,116,121,0,115,117,110,95,100,105,115,99,0,99,111,108,111,114,95,115,112,97,99,101,0,112,114,111,106,101,99,116,105,111,110,0,112,114,111,106,101,99,116,105,111,110,95,98,108,101,110,100,0,105,110,116,101,114,112,111,108,97,116,105,111,110,0,101,120,116,101,110,115,105,111,110,0,111,102,102,115,101,116,95,102,114,101,113,0,115,113,117,97,115,104,95,102,114,101,113,0,115,113,117,97,115,104,
0,100,105,109,101,110,115,105,111,110,115,0,110,111,114,109,97,108,105,122,101,0,102,101,97,116,117,114,101,0,99,111,108,111,114,105,110,103,0,109,117,115,103,114,97,118,101,95,116,121,112,101,0,98,97,110,100,115,95,100,105,114,101,99,116,105,111,110,0,114,105,110,103,115,95,100,105,114,101,99,116,105,111,110,0,119,97,118,101,95,112,114,111,102,105,108,101,0,110,97,109,101,91,50,53,54,93,0,99,111,110,118,101,114,116,95,102,114,111,109,
0,99,111,110,118,101,114,116,95,116,111,0,112,111,105,110,116,95,115,111,117,114,99,101,0,112,97,114,116,105,99,108,101,95,115,121,115,116,101,109,0,99,111,108,111,114,95,115,111,117,114,99,101,0,111,98,95,99,111,108,111,114,95,115,111,117,114,99,101,0,112,100,0,99,97,99,104,101,100,95,114,101,115,111,108,117,116,105,111,110,0,118,101,114,116,101,120,95,97,116,116,114,105,98,117,116,101,95,110,97,109,101,91,54,56,93,0,117,115,101,
95,115,117,98,115,117,114,102,97,99,101,95,97,117,116,111,95,114,97,100,105,117,115,0,109,111,100,101,108,0,112,97,114,97,109,101,116,114,105,122,97,116,105,111,110,0,116,114,97,99,107,105,110,103,95,111,98,106,101,99,116,91,54,52,93,0,115,109,111,111,116,104,110,101,115,115,0,115,99,114,101,101,110,95,98,97,108,97,110,99,101,0,100,101,115,112,105,108,108,95,102,97,99,116,111,114,0,100,101,115,112,105,108,108,95,98,97,108,97,110,
99,101,0,101,100,103,101,95,107,101,114,110,101,108,95,114,97,100,105,117,115,0,101,100,103,101,95,107,101,114,110,101,108,95,116,111,108,101,114,97,110,99,101,0,99,108,105,112,95,98,108,97,99,107,0,99,108,105,112,95,119,104,105,116,101,0,100,105,108,97,116,101,95,100,105,115,116,97,110,99,101,0,102,101,97,116,104,101,114,95,100,105,115,116,97,110,99,101,0,102,101,97,116,104,101,114,95,102,97,108,108,111,102,102,0,98,108,117,114,95,
112,114,101,0,98,108,117,114,95,112,111,115,116,0,116,114,97,99,107,95,110,97,109,101,91,54,52,93,0,119,114,97,112,95,97,120,105,115,0,112,108,97,110,101,95,116,114,97,99,107,95,110,97,109,101,91,54,52,93,0,109,111,116,105,111,110,95,98,108,117,114,95,115,97,109,112,108,101,115,0,109,111,116,105,111,110,95,98,108,117,114,95,115,104,117,116,116,101,114,0,98,121,116,101,99,111,100,101,95,104,97,115,104,91,54,52,93,0,42,98,
121,116,101,99,111,100,101,0,100,105,114,101,99,116,105,111,110,95,116,121,112,101,0,117,118,95,109,97,112,91,54,52,93,0,115,111,117,114,99,101,91,50,93,0,114,97,121,95,108,101,110,103,116,104,0,101,110,99,111,100,101,100,95,104,97,115,104,0,97,100,100,91,51,93,0,114,101,109,111,118,101,91,51,93,0,101,110,116,114,105,101,115,0,42,109,97,116,116,101,95,105,100,0,110,117,109,95,105,110,112,117,116,115,0,104,100,114,0,112,114,
101,102,105,108,116,101,114,0,105,110,116,101,114,112,111,108,97,116,105,111,110,95,116,121,112,101,0,100,111,109,97,105,110,0,98,111,111,108,101,97,110,0,105,110,116,101,103,101,114,0,114,111,116,97,116,105,111,110,95,101,117,108,101,114,91,51,93,0,118,101,99,116,111,114,91,51,93,0,42,115,116,114,105,110,103,0,116,114,97,110,115,102,111,114,109,95,115,112,97,99,101,0,105,110,112,117,116,95,116,121,112,101,95,114,97,100,105,117,115,0,
116,97,114,103,101,116,95,101,108,101,109,101,110,116,0,102,105,108,108,95,116,121,112,101,0,99,111,117,110,116,95,109,111,100,101,0,105,110,112,117,116,95,116,121,112,101,0,42,105,116,101,109,115,95,97,114,114,97,121,0,110,101,120,116,95,105,100,101,110,116,105,102,105,101,114,0,101,110,117,109,95,100,101,102,105,110,105,116,105,111,110,0,115,112,108,105,110,101,95,116,121,112,101,0,104,97,110,100,108,101,95,116,121,112,101,0,117,115,101,95,
97,108,108,95,99,117,114,118,101,115,0,109,97,112,112,105,110,103,0,42,99,97,112,116,117,114,101,95,105,116,101,109,115,0,99,97,112,116,117,114,101,95,105,116,101,109,115,95,110,117,109,0,97,108,105,103,110,95,120,0,112,105,118,111,116,95,109,111,100,101,0,109,101,116,104,111,100,0,115,111,99,107,101,116,95,116,121,112,101,0,111,117,116,112,117,116,95,110,111,100,101,95,105,100,0,42,105,116,101,109,115,0,105,110,115,112,101,99,116,105,
111,110,95,105,110,100,101,120,0,102,97,99,116,111,114,95,109,111,100,101,0,99,108,97,109,112,95,102,97,99,116,111,114,0,99,108,97,109,112,95,114,101,115,117,108,116,0,98,108,101,110,100,95,116,121,112,101,0,118,101,108,91,51,93,0,116,104,114,101,97,100,115,0,115,104,111,119,95,97,100,118,97,110,99,101,100,111,112,116,105,111,110,115,0,114,101,115,111,108,117,116,105,111,110,120,121,122,0,112,114,101,118,105,101,119,114,101,115,120,121,
122,0,114,101,97,108,115,105,122,101,0,103,117,105,68,105,115,112,108,97,121,77,111,100,101,0,114,101,110,100,101,114,68,105,115,112,108,97,121,77,111,100,101,0,118,105,115,99,111,115,105,116,121,86,97,108,117,101,0,118,105,115,99,111,115,105,116,121,77,111,100,101,0,118,105,115,99,111,115,105,116,121,69,120,112,111,110,101,110,116,0,103,114,97,118,91,51,93,0,97,110,105,109,83,116,97,114,116,0,97,110,105,109,69,110,100,0,98,97,107,
101,83,116,97,114,116,0,98,97,107,101,69,110,100,0,102,114,97,109,101,79,102,102,115,101,116,0,103,115,116,97,114,0,109,97,120,82,101,102,105,110,101,0,105,110,105,86,101,108,120,0,105,110,105,86,101,108,121,0,105,110,105,86,101,108,122,0,115,117,114,102,100,97,116,97,80,97,116,104,91,49,48,50,52,93,0,98,98,83,116,97,114,116,91,51,93,0,98,98,83,105,122,101,91,51,93,0,116,121,112,101,70,108,97,103,115,0,100,111,109,
97,105,110,78,111,118,101,99,103,101,110,0,118,111,108,117,109,101,73,110,105,116,84,121,112,101,0,112,97,114,116,83,108,105,112,86,97,108,117,101,0,103,101,110,101,114,97,116,101,84,114,97,99,101,114,115,0,103,101,110,101,114,97,116,101,80,97,114,116,105,99,108,101,115,0,115,117,114,102,97,99,101,83,109,111,111,116,104,105,110,103,0,115,117,114,102,97,99,101,83,117,98,100,105,118,115,0,112,97,114,116,105,99,108,101,73,110,102,83,105,
122,101,0,112,97,114,116,105,99,108,101,73,110,102,65,108,112,104,97,0,102,97,114,70,105,101,108,100,83,105,122,101,0,42,109,101,115,104,86,101,108,111,99,105,116,105,101,115,0,99,112,115,84,105,109,101,83,116,97,114,116,0,99,112,115,84,105,109,101,69,110,100,0,99,112,115,81,117,97,108,105,116,121,0,97,116,116,114,97,99,116,102,111,114,99,101,83,116,114,101,110,103,116,104,0,97,116,116,114,97,99,116,102,111,114,99,101,82,97,100,
105,117,115,0,118,101,108,111,99,105,116,121,102,111,114,99,101,83,116,114,101,110,103,116,104,0,118,101,108,111,99,105,116,121,102,111,114,99,101,82,97,100,105,117,115,0,108,97,115,116,103,111,111,100,102,114,97,109,101,0,97,110,105,109,82,97,116,101,0,100,101,102,108,101,99,116,0,102,111,114,99,101,102,105,101,108,100,0,115,104,97,112,101,0,116,101,120,95,109,111,100,101,0,107,105,110,107,0,107,105,110,107,95,97,120,105,115,0,122,100,
105,114,0,102,95,115,116,114,101,110,103,116,104,0,102,95,100,97,109,112,0,102,95,102,108,111,119,0,102,95,119,105,110,100,95,102,97,99,116,111,114,0,102,95,115,105,122,101,0,102,95,112,111,119,101,114,0,109,97,120,100,105,115,116,0,109,105,110,100,105,115,116,0,102,95,112,111,119,101,114,95,114,0,109,97,120,114,97,100,0,109,105,110,114,97,100,0,112,100,101,102,95,100,97,109,112,0,112,100,101,102,95,114,100,97,109,112,0,112,100,
101,102,95,112,101,114,109,0,112,100,101,102,95,102,114,105,99,116,0,112,100,101,102,95,114,102,114,105,99,116,0,112,100,101,102,95,115,116,105,99,107,110,101,115,115,0,97,98,115,111,114,112,116,105,111,110,0,112,100,101,102,95,115,98,100,97,109,112,0,112,100,101,102,95,115,98,105,102,116,0,112,100,101,102,95,115,98,111,102,116,0,99,108,117,109,112,95,102,97,99,0,99,108,117,109,112,95,112,111,119,0,107,105,110,107,95,102,114,101,113,
0,107,105,110,107,95,115,104,97,112,101,0,107,105,110,107,95,97,109,112,0,102,114,101,101,95,101,110,100,0,116,101,120,95,110,97,98,108,97,0,102,95,110,111,105,115,101,0,100,114,97,119,118,101,99,49,91,52,93,0,100,114,97,119,118,101,99,50,91,52,93,0,100,114,97,119,118,101,99,95,102,97,108,108,111,102,102,95,109,105,110,91,51,93,0,100,114,97,119,118,101,99,95,102,97,108,108,111,102,102,95,109,97,120,91,51,93,0,42,102,
95,115,111,117,114,99,101,0,112,100,101,102,95,99,102,114,105,99,116,0,119,101,105,103,104,116,91,49,52,93,0,103,108,111,98,97,108,95,103,114,97,118,105,116,121,0,116,111,116,115,112,114,105,110,103,0,42,98,112,111,105,110,116,0,42,98,115,112,114,105,110,103,0,109,115,103,95,108,111,99,107,0,109,115,103,95,118,97,108,117,101,0,110,111,100,101,109,97,115,115,0,110,97,109,101,100,86,71,95,77,97,115,115,91,54,52,93,0,103,114,
97,118,0,109,101,100,105,97,102,114,105,99,116,0,114,107,108,105,109,105,116,0,112,104,121,115,105,99,115,95,115,112,101,101,100,0,110,97,109,101,100,86,71,95,83,111,102,116,103,111,97,108,91,54,52,93,0,102,117,122,122,121,110,101,115,115,0,105,110,115,112,114,105,110,103,0,105,110,102,114,105,99,116,0,110,97,109,101,100,86,71,95,83,112,114,105,110,103,95,75,91,54,52,93,0,115,111,108,118,101,114,102,108,97,103,115,0,42,42,107,
101,121,115,0,116,111,116,112,111,105,110,116,107,101,121,0,115,101,99,111,110,100,115,112,114,105,110,103,0,99,111,108,98,97,108,108,0,98,97,108,108,100,97,109,112,0,98,97,108,108,115,116,105,102,102,0,115,98,99,95,109,111,100,101,0,97,101,114,111,101,100,103,101,0,109,105,110,108,111,111,112,115,0,109,97,120,108,111,111,112,115,0,99,104,111,107,101,0,115,111,108,118,101,114,95,73,68,0,112,108,97,115,116,105,99,0,115,112,114,105,
110,103,112,114,101,108,111,97,100,0,42,115,99,114,97,116,99,104,0,115,104,101,97,114,115,116,105,102,102,0,105,110,112,117,115,104,0,42,115,104,97,114,101,100,0,42,99,111,108,108,105,115,105,111,110,95,103,114,111,117,112,0,108,99,111,109,91,51,93,0,108,114,111,116,91,51,93,91,51,93,0,108,115,99,97,108,101,91,51,93,91,51,93,0,108,97,115,116,95,102,114,97,109,101,0,118,101,99,91,56,93,91,51,93,0,117,115,97,103,101,
0,108,105,103,104,116,95,115,101,116,95,109,101,109,98,101,114,115,104,105,112,0,115,104,97,100,111,119,95,115,101,116,95,109,101,109,98,101,114,115,104,105,112,0,114,101,99,101,105,118,101,114,95,108,105,103,104,116,95,115,101,116,0,98,108,111,99,107,101,114,95,115,104,97,100,111,119,95,115,101,116,0,42,114,101,99,101,105,118,101,114,95,99,111,108,108,101,99,116,105,111,110,0,42,98,108,111,99,107,101,114,95,99,111,108,108,101,99,116,105,
111,110,0,42,115,99,117,108,112,116,0,112,97,114,49,0,112,97,114,50,0,112,97,114,51,0,42,112,114,111,120,121,0,42,112,114,111,120,121,95,103,114,111,117,112,0,42,112,114,111,120,121,95,102,114,111,109,0,42,112,111,115,101,108,105,98,0,42,112,111,115,101,0,42,95,112,97,100,48,0,100,101,102,98,97,115,101,0,102,109,97,112,115,0,103,114,101,97,115,101,112,101,110,99,105,108,95,109,111,100,105,102,105,101,114,115,0,115,104,97,
100,101,114,95,102,120,0,114,101,115,116,111,114,101,95,109,111,100,101,0,42,109,97,116,98,105,116,115,0,97,99,116,99,111,108,0,100,108,111,99,91,51,93,0,100,115,105,122,101,91,51,93,0,100,115,99,97,108,101,91,51,93,0,100,114,111,116,91,51,93,0,100,113,117,97,116,91,52,93,0,100,114,111,116,65,120,105,115,91,51,93,0,100,114,111,116,65,110,103,108,101,0,99,111,108,98,105,116,115,0,116,114,97,110,115,102,108,97,103,0,
110,108,97,102,108,97,103,0,100,117,112,108,105,99,97,116,111,114,95,118,105,115,105,98,105,108,105,116,121,95,102,108,97,103,0,98,97,115,101,95,102,108,97,103,0,98,97,115,101,95,108,111,99,97,108,95,118,105,101,119,95,98,105,116,115,0,99,111,108,95,103,114,111,117,112,0,99,111,108,95,109,97,115,107,0,98,111,117,110,100,116,121,112,101,0,99,111,108,108,105,115,105,111,110,95,98,111,117,110,100,116,121,112,101,0,101,109,112,116,121,
95,100,114,97,119,116,121,112,101,0,101,109,112,116,121,95,100,114,97,119,115,105,122,101,0,100,117,112,102,97,99,101,115,99,97,0,97,99,116,100,101,102,0,115,111,102,116,102,108,97,103,0,115,104,97,112,101,110,114,0,115,104,97,112,101,102,108,97,103,0,95,112,97,100,51,91,49,93,0,110,108,97,115,116,114,105,112,115,0,104,111,111,107,115,0,112,97,114,116,105,99,108,101,115,121,115,116,101,109,0,42,112,100,0,42,115,111,102,116,0,
42,100,117,112,95,103,114,111,117,112,0,42,102,108,117,105,100,115,105,109,83,101,116,116,105,110,103,115,0,112,99,95,105,100,115,0,42,114,105,103,105,100,98,111,100,121,95,111,98,106,101,99,116,0,42,114,105,103,105,100,98,111,100,121,95,99,111,110,115,116,114,97,105,110,116,0,105,109,97,95,111,102,115,91,50,93,0,42,105,117,115,101,114,0,101,109,112,116,121,95,105,109,97,103,101,95,118,105,115,105,98,105,108,105,116,121,95,102,108,97,
103,0,101,109,112,116,121,95,105,109,97,103,101,95,100,101,112,116,104,0,101,109,112,116,121,95,105,109,97,103,101,95,102,108,97,103,0,109,111,100,105,102,105,101,114,95,102,108,97,103,0,95,112,97,100,56,91,52,93,0,42,108,105,103,104,116,103,114,111,117,112,0,42,108,105,103,104,116,95,108,105,110,107,105,110,103,0,42,108,105,103,104,116,112,114,111,98,101,95,99,97,99,104,101,0,99,117,114,105,110,100,101,120,0,117,115,101,100,0,117,
115,101,100,101,108,101,109,0,115,101,101,107,0,119,111,114,108,100,95,99,111,91,51,93,0,114,111,116,91,52,93,0,97,118,101,91,51,93,0,42,103,114,111,117,110,100,0,119,97,110,100,101,114,91,51,93,0,114,101,115,116,95,108,101,110,103,116,104,0,112,97,114,116,105,99,108,101,95,105,110,100,101,120,91,50,93,0,100,101,108,101,116,101,95,102,108,97,103,0,110,117,109,0,112,97,91,52,93,0,119,91,52,93,0,102,117,118,91,52,93,
0,102,111,102,102,115,101,116,0,100,117,114,97,116,105,111,110,0,115,116,97,116,101,0,112,114,101,118,95,115,116,97,116,101,0,42,104,97,105,114,0,42,98,111,105,100,0,100,105,101,116,105,109,101,0,110,117,109,95,100,109,99,97,99,104,101,0,115,112,104,100,101,110,115,105,116,121,0,104,97,105,114,95,105,110,100,101,120,0,97,108,105,118,101,0,115,112,114,105,110,103,95,107,0,112,108,97,115,116,105,99,105,116,121,95,99,111,110,115,116,
97,110,116,0,121,105,101,108,100,95,114,97,116,105,111,0,112,108,97,115,116,105,99,105,116,121,95,98,97,108,97,110,99,101,0,121,105,101,108,100,95,98,97,108,97,110,99,101,0,118,105,115,99,111,115,105,116,121,95,111,109,101,103,97,0,118,105,115,99,111,115,105,116,121,95,98,101,116,97,0,115,116,105,102,102,110,101,115,115,95,107,0,115,116,105,102,102,110,101,115,115,95,107,110,101,97,114,0,114,101,115,116,95,100,101,110,115,105,116,121,
0,98,117,111,121,97,110,99,121,0,115,112,114,105,110,103,95,102,114,97,109,101,115,0,42,98,111,105,100,115,0,100,105,115,116,114,0,112,104,121,115,116,121,112,101,0,97,118,101,109,111,100,101,0,114,101,97,99,116,101,118,101,110,116,0,100,114,97,119,0,100,114,97,119,95,115,105,122,101,0,100,114,97,119,95,97,115,0,99,104,105,108,100,116,121,112,101,0,114,101,110,95,97,115,0,100,114,97,119,95,99,111,108,0,100,114,97,119,95,115,
116,101,112,0,114,101,110,95,115,116,101,112,0,104,97,105,114,95,115,116,101,112,0,107,101,121,115,95,115,116,101,112,0,97,100,97,112,116,95,97,110,103,108,101,0,97,100,97,112,116,95,112,105,120,0,105,110,116,101,103,114,97,116,111,114,0,114,111,116,102,114,111,109,0,98,98,95,97,108,105,103,110,0,98,98,95,117,118,95,115,112,108,105,116,0,98,98,95,97,110,105,109,0,98,98,95,115,112,108,105,116,95,111,102,102,115,101,116,0,98,
98,95,116,105,108,116,0,98,98,95,114,97,110,100,95,116,105,108,116,0,98,98,95,111,102,102,115,101,116,91,50,93,0,98,98,95,115,105,122,101,91,50,93,0,98,98,95,118,101,108,95,104,101,97,100,0,98,98,95,118,101,108,95,116,97,105,108,0,99,111,108,111,114,95,118,101,99,95,109,97,120,0,116,105,109,101,116,119,101,97,107,0,99,111,117,114,97,110,116,95,116,97,114,103,101,116,0,106,105,116,102,97,99,0,101,102,102,95,104,97,
105,114,0,103,114,105,100,95,114,97,110,100,0,112,115,95,111,102,102,115,101,116,91,49,93,0,103,114,105,100,95,114,101,115,0,101,102,102,101,99,116,111,114,95,97,109,111,117,110,116,0,116,105,109,101,95,102,108,97,103,0,112,97,114,116,102,97,99,0,116,97,110,102,97,99,0,116,97,110,112,104,97,115,101,0,114,101,97,99,116,102,97,99,0,111,98,95,118,101,108,91,51,93,0,97,118,101,102,97,99,0,112,104,97,115,101,102,97,99,0,
114,97,110,100,114,111,116,102,97,99,0,114,97,110,100,112,104,97,115,101,102,97,99,0,114,97,110,100,115,105,122,101,0,100,114,97,103,102,97,99,0,98,114,111,119,110,102,97,99,0,100,97,109,112,102,97,99,0,114,97,110,100,108,101,110,103,116,104,0,99,104,105,108,100,95,102,108,97,103,0,99,104,105,108,100,95,110,98,114,0,114,101,110,95,99,104,105,108,100,95,110,98,114,0,99,104,105,108,100,115,105,122,101,0,99,104,105,108,100,114,
97,110,100,115,105,122,101,0,99,104,105,108,100,114,97,100,0,99,104,105,108,100,102,108,97,116,0,99,108,117,109,112,102,97,99,0,99,108,117,109,112,112,111,119,0,107,105,110,107,95,102,108,97,116,0,107,105,110,107,95,97,109,112,95,99,108,117,109,112,0,107,105,110,107,95,101,120,116,114,97,95,115,116,101,112,115,0,107,105,110,107,95,97,120,105,115,95,114,97,110,100,111,109,0,107,105,110,107,95,97,109,112,95,114,97,110,100,111,109,0,
114,111,117,103,104,49,0,114,111,117,103,104,49,95,115,105,122,101,0,114,111,117,103,104,50,0,114,111,117,103,104,50,95,115,105,122,101,0,114,111,117,103,104,50,95,116,104,114,101,115,0,114,111,117,103,104,95,101,110,100,0,114,111,117,103,104,95,101,110,100,95,115,104,97,112,101,0,99,108,101,110,103,116,104,0,99,108,101,110,103,116,104,95,116,104,114,101,115,0,112,97,114,116,105,110,103,95,102,97,99,0,112,97,114,116,105,110,103,95,109,
105,110,0,112,97,114,116,105,110,103,95,109,97,120,0,98,114,97,110,99,104,95,116,104,114,101,115,0,100,114,97,119,95,108,105,110,101,91,50,93,0,112,97,116,104,95,115,116,97,114,116,0,112,97,116,104,95,101,110,100,0,116,114,97,105,108,95,99,111,117,110,116,0,107,101,121,101,100,95,108,111,111,112,115,0,42,99,108,117,109,112,99,117,114,118,101,0,42,114,111,117,103,104,99,117,114,118,101,0,99,108,117,109,112,95,110,111,105,115,101,
95,115,105,122,101,0,98,101,110,100,105,110,103,95,114,97,110,100,111,109,0,100,117,112,108,105,119,101,105,103,104,116,115,0,42,100,117,112,95,111,98,0,42,98,98,95,111,98,0,42,112,100,50,0,117,115,101,95,109,111,100,105,102,105,101,114,95,115,116,97,99,107,0,95,112,97,100,53,91,50,93,0,115,104,97,112,101,95,102,108,97,103,0,116,119,105,115,116,0,114,97,100,95,114,111,111,116,0,114,97,100,95,116,105,112,0,114,97,100,95,
115,99,97,108,101,0,42,116,119,105,115,116,99,117,114,118,101,0,42,95,112,97,100,55,0,42,112,97,114,116,0,42,112,97,114,116,105,99,108,101,115,0,42,101,100,105,116,0,40,42,102,114,101,101,95,101,100,105,116,41,40,41,0,42,42,112,97,116,104,99,97,99,104,101,0,42,42,99,104,105,108,100,99,97,99,104,101,0,112,97,116,104,99,97,99,104,101,98,117,102,115,0,99,104,105,108,100,99,97,99,104,101,98,117,102,115,0,42,99,108,
109,100,0,42,104,97,105,114,95,105,110,95,109,101,115,104,0,42,104,97,105,114,95,111,117,116,95,109,101,115,104,0,42,116,97,114,103,101,116,95,111,98,0,42,108,97,116,116,105,99,101,95,100,101,102,111,114,109,95,100,97,116,97,0,116,114,101,101,95,102,114,97,109,101,0,98,118,104,116,114,101,101,95,102,114,97,109,101,0,99,104,105,108,100,95,115,101,101,100,0,116,111,116,117,110,101,120,105,115,116,0,116,111,116,99,104,105,108,100,0,
116,111,116,99,97,99,104,101,100,0,116,111,116,99,104,105,108,100,99,97,99,104,101,0,116,97,114,103,101,116,95,112,115,121,115,0,116,111,116,107,101,121,101,100,0,98,97,107,101,115,112,97,99,101,0,98,98,95,117,118,110,97,109,101,91,51,93,91,54,56,93,0,118,103,114,111,117,112,91,49,51,93,0,118,103,95,110,101,103,0,114,116,51,0,95,112,97,100,51,91,54,93,0,42,101,102,102,101,99,116,111,114,115,0,42,102,108,117,105,100,
95,115,112,114,105,110,103,115,0,116,111,116,95,102,108,117,105,100,115,112,114,105,110,103,115,0,97,108,108,111,99,95,102,108,117,105,100,115,112,114,105,110,103,115,0,42,116,114,101,101,0,42,112,100,100,0,100,116,95,102,114,97,99,0,108,97,116,116,105,99,101,95,115,116,114,101,110,103,116,104,0,42,111,114,105,103,95,112,115,121,115,0,116,111,116,100,97,116,97,0,42,100,97,116,97,91,56,93,0,101,120,116,114,97,100,97,116,97,0,115,
105,109,102,114,97,109,101,0,115,116,97,114,116,102,114,97,109,101,0,101,110,100,102,114,97,109,101,0,101,100,105,116,102,114,97,109,101,0,108,97,115,116,95,101,120,97,99,116,0,108,97,115,116,95,118,97,108,105,100,0,112,114,101,118,95,110,97,109,101,91,54,52,93,0,42,99,97,99,104,101,100,95,102,114,97,109,101,115,0,99,97,99,104,101,100,95,102,114,97,109,101,115,95,108,101,110,0,109,101,109,95,99,97,99,104,101,0,95,112,97,
100,51,91,51,93,0,42,112,104,121,115,105,99,115,95,119,111,114,108,100,0,42,42,111,98,106,101,99,116,115,0,42,99,111,110,115,116,114,97,105,110,116,115,0,108,116,105,109,101,0,110,117,109,98,111,100,105,101,115,0,115,116,101,112,115,95,112,101,114,95,115,101,99,111,110,100,0,110,117,109,95,115,111,108,118,101,114,95,105,116,101,114,97,116,105,111,110,115,0,99,111,108,95,103,114,111,117,112,115,0,109,101,115,104,95,115,111,117,114,99,
101,0,114,101,115,116,105,116,117,116,105,111,110,0,109,97,114,103,105,110,0,108,105,110,95,100,97,109,112,105,110,103,0,97,110,103,95,100,97,109,112,105,110,103,0,108,105,110,95,115,108,101,101,112,95,116,104,114,101,115,104,0,97,110,103,95,115,108,101,101,112,95,116,104,114,101,115,104,0,111,114,110,91,52,93,0,112,111,115,91,51,93,0,42,111,98,49,0,42,111,98,50,0,98,114,101,97,107,105,110,103,95,116,104,114,101,115,104,111,108,
100,0,115,112,114,105,110,103,95,116,121,112,101,0,108,105,109,105,116,95,108,105,110,95,120,95,108,111,119,101,114,0,108,105,109,105,116,95,108,105,110,95,120,95,117,112,112,101,114,0,108,105,109,105,116,95,108,105,110,95,121,95,108,111,119,101,114,0,108,105,109,105,116,95,108,105,110,95,121,95,117,112,112,101,114,0,108,105,109,105,116,95,108,105,110,95,122,95,108,111,119,101,114,0,108,105,109,105,116,95,108,105,110,95,122,95,117,112,112,101,
114,0,108,105,109,105,116,95,97,110,103,95,120,95,108,111,119,101,114,0,108,105,109,105,116,95,97,110,103,95,120,95,117,112,112,101,114,0,108,105,109,105,116,95,97,110,103,95,121,95,108,111,119,101,114,0,108,105,109,105,116,95,97,110,103,95,121,95,117,112,112,101,114,0,108,105,109,105,116,95,97,110,103,95,122,95,108,111,119,101,114,0,108,105,109,105,116,95,97,110,103,95,122,95,117,112,112,101,114,0,115,112,114,105,110,103,95,115,116,105,
102,102,110,101,115,115,95,120,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,121,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,122,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,97,110,103,95,120,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,97,110,103,95,121,0,115,112,114,105,110,103,95,115,116,105,102,102,110,101,115,115,95,97,110,103,95,122,0,115,112,114,105,
110,103,95,100,97,109,112,105,110,103,95,120,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,121,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,122,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,97,110,103,95,120,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,97,110,103,95,121,0,115,112,114,105,110,103,95,100,97,109,112,105,110,103,95,97,110,103,95,122,0,109,111,116,111,114,95,108,105,110,95,
116,97,114,103,101,116,95,118,101,108,111,99,105,116,121,0,109,111,116,111,114,95,97,110,103,95,116,97,114,103,101,116,95,118,101,108,111,99,105,116,121,0,109,111,116,111,114,95,108,105,110,95,109,97,120,95,105,109,112,117,108,115,101,0,109,111,116,111,114,95,97,110,103,95,109,97,120,95,105,109,112,117,108,115,101,0,42,112,104,121,115,105,99,115,95,99,111,110,115,116,114,97,105,110,116,0,99,111,100,101,99,0,97,117,100,105,111,95,99,111,
100,101,99,0,118,105,100,101,111,95,98,105,116,114,97,116,101,0,97,117,100,105,111,95,98,105,116,114,97,116,101,0,97,117,100,105,111,95,109,105,120,114,97,116,101,0,97,117,100,105,111,95,99,104,97,110,110,101,108,115,0,97,117,100,105,111,95,118,111,108,117,109,101,0,103,111,112,95,115,105,122,101,0,109,97,120,95,98,95,102,114,97,109,101,115,0,99,111,110,115,116,97,110,116,95,114,97,116,101,95,102,97,99,116,111,114,0,102,102,109,
112,101,103,95,112,114,101,115,101,116,0,114,99,95,109,105,110,95,114,97,116,101,0,114,99,95,109,97,120,95,114,97,116,101,0,114,99,95,98,117,102,102,101,114,95,115,105,122,101,0,109,117,120,95,112,97,99,107,101,116,95,115,105,122,101,0,109,117,120,95,114,97,116,101,0,109,105,120,114,97,116,101,0,109,97,105,110,0,115,112,101,101,100,95,111,102,95,115,111,117,110,100,0,100,111,112,112,108,101,114,95,102,97,99,116,111,114,0,100,105,
115,116,97,110,99,101,95,109,111,100,101,108,0,108,97,121,95,122,109,97,115,107,0,108,97,121,95,101,120,99,108,117,100,101,0,112,97,115,115,95,120,111,114,0,102,114,101,101,115,116,121,108,101,67,111,110,102,105,103,0,115,117,102,102,105,120,91,54,52,93,0,118,105,101,119,102,108,97,103,0,100,105,115,112,108,97,121,95,109,111,100,101,0,97,110,97,103,108,121,112,104,95,116,121,112,101,0,105,110,116,101,114,108,97,99,101,95,116,121,112,
101,0,105,109,116,121,112,101,0,112,108,97,110,101,115,0,99,111,109,112,114,101,115,115,0,101,120,114,95,99,111,100,101,99,0,99,105,110,101,111,110,95,102,108,97,103,0,99,105,110,101,111,110,95,119,104,105,116,101,0,99,105,110,101,111,110,95,98,108,97,99,107,0,99,105,110,101,111,110,95,103,97,109,109,97,0,106,112,50,95,102,108,97,103,0,106,112,50,95,99,111,100,101,99,0,116,105,102,102,95,99,111,100,101,99,0,115,116,101,114,
101,111,51,100,95,102,111,114,109,97,116,0,99,111,108,111,114,95,109,97,110,97,103,101,109,101,110,116,0,95,112,97,100,49,91,55,93,0,118,105,101,119,95,115,101,116,116,105,110,103,115,0,100,105,115,112,108,97,121,95,115,101,116,116,105,110,103,115,0,108,105,110,101,97,114,95,99,111,108,111,114,115,112,97,99,101,95,115,101,116,116,105,110,103,115,0,99,97,103,101,95,101,120,116,114,117,115,105,111,110,0,109,97,120,95,114,97,121,95,100,
105,115,116,97,110,99,101,0,112,97,115,115,95,102,105,108,116,101,114,0,110,111,114,109,97,108,95,115,119,105,122,122,108,101,91,51,93,0,110,111,114,109,97,108,95,115,112,97,99,101,0,116,97,114,103,101,116,0,115,97,118,101,95,109,111,100,101,0,109,97,114,103,105,110,95,116,121,112,101,0,118,105,101,119,95,102,114,111,109,0,42,99,97,103,101,95,111,98,106,101,99,116,0,102,102,99,111,100,101,99,100,97,116,97,0,115,117,98,102,114,
97,109,101,0,112,115,102,114,97,0,112,101,102,114,97,0,105,109,97,103,101,115,0,102,114,97,109,97,112,116,111,0,102,114,97,109,101,108,101,110,0,102,114,97,109,101,95,115,116,101,112,0,100,105,109,101,110,115,105,111,110,115,112,114,101,115,101,116,0,120,115,99,104,0,121,115,99,104,0,116,105,108,101,120,0,116,105,108,101,121,0,115,117,98,105,109,116,121,112,101,0,117,115,101,95,108,111,99,107,95,105,110,116,101,114,102,97,99,101,0,
95,112,97,100,55,91,51,93,0,115,99,101,109,111,100,101,0,102,114,115,95,115,101,99,0,97,108,112,104,97,109,111,100,101,0,95,112,97,100,48,91,49,93,0,98,111,114,100,101,114,0,97,99,116,108,97,121,0,120,97,115,112,0,121,97,115,112,0,102,114,115,95,115,101,99,95,98,97,115,101,0,103,97,117,115,115,0,99,111,108,111,114,95,109,103,116,95,102,108,97,103,0,100,105,116,104,101,114,95,105,110,116,101,110,115,105,116,121,0,98,
97,107,101,95,102,108,97,103,0,98,97,107,101,95,102,105,108,116,101,114,0,98,97,107,101,95,115,97,109,112,108,101,115,0,98,97,107,101,95,109,97,114,103,105,110,95,116,121,112,101,0,95,112,97,100,57,91,54,93,0,98,97,107,101,95,98,105,97,115,100,105,115,116,0,98,97,107,101,95,117,115,101,114,95,115,99,97,108,101,0,112,105,99,91,49,48,50,52,93,0,115,116,97,109,112,0,115,116,97,109,112,95,102,111,110,116,95,105,100,0,
115,116,97,109,112,95,117,100,97,116,97,91,55,54,56,93,0,102,103,95,115,116,97,109,112,91,52,93,0,98,103,95,115,116,97,109,112,91,52,93,0,115,101,113,95,112,114,101,118,95,116,121,112,101,0,115,101,113,95,114,101,110,100,95,116,121,112,101,0,115,101,113,95,102,108,97,103,0,95,112,97,100,53,91,51,93,0,115,105,109,112,108,105,102,121,95,115,117,98,115,117,114,102,0,115,105,109,112,108,105,102,121,95,115,117,98,115,117,114,102,
95,114,101,110,100,101,114,0,115,105,109,112,108,105,102,121,95,103,112,101,110,99,105,108,0,115,105,109,112,108,105,102,121,95,112,97,114,116,105,99,108,101,115,0,115,105,109,112,108,105,102,121,95,112,97,114,116,105,99,108,101,115,95,114,101,110,100,101,114,0,115,105,109,112,108,105,102,121,95,118,111,108,117,109,101,115,0,108,105,110,101,95,116,104,105,99,107,110,101,115,115,95,109,111,100,101,0,117,110,105,116,95,108,105,110,101,95,116,104,105,
99,107,110,101,115,115,0,101,110,103,105,110,101,91,51,50,93,0,112,101,114,102,95,102,108,97,103,0,98,97,107,101,0,95,112,97,100,56,0,112,114,101,118,105,101,119,95,112,105,120,101,108,95,115,105,122,101,0,97,99,116,118,105,101,119,0,104,97,105,114,95,116,121,112,101,0,104,97,105,114,95,115,117,98,100,105,118,0,98,108,117,114,102,97,99,0,109,111,116,105,111,110,95,98,108,117,114,95,112,111,115,105,116,105,111,110,0,109,98,108,
117,114,95,115,104,117,116,116,101,114,95,99,117,114,118,101,0,99,111,109,112,111,115,105,116,111,114,95,100,101,118,105,99,101,0,99,111,109,112,111,115,105,116,111,114,95,112,114,101,99,105,115,105,111,110,0,116,111,111,108,95,111,102,102,115,101,116,0,42,116,111,111,108,95,115,108,111,116,115,0,116,111,111,108,95,115,108,111,116,115,95,108,101,110,0,42,112,97,108,101,116,116,101,0,42,99,97,118,105,116,121,95,99,117,114,118,101,0,42,112,
97,105,110,116,95,99,117,114,115,111,114,0,112,97,105,110,116,95,99,117,114,115,111,114,95,99,111,108,91,52,93,0,110,117,109,95,105,110,112,117,116,95,115,97,109,112,108,101,115,0,115,121,109,109,101,116,114,121,95,102,108,97,103,115,0,116,105,108,101,95,111,102,102,115,101,116,91,51,93,0,112,97,105,110,116,0,109,105,115,115,105,110,103,95,100,97,116,97,0,115,101,97,109,95,98,108,101,101,100,0,110,111,114,109,97,108,95,97,110,103,
108,101,0,115,99,114,101,101,110,95,103,114,97,98,95,115,105,122,101,91,50,93,0,42,115,116,101,110,99,105,108,0,42,99,108,111,110,101,0,115,116,101,110,99,105,108,95,99,111,108,91,51,93,0,100,105,116,104,101,114,0,99,97,110,118,97,115,95,115,111,117,114,99,101,0,42,99,97,110,118,97,115,95,105,109,97,103,101,0,105,109,97,103,101,95,117,115,101,114,0,105,110,118,101,114,116,0,116,111,116,114,101,107,101,121,0,116,111,116,97,
100,100,107,101,121,0,98,114,117,115,104,116,121,112,101,0,98,114,117,115,104,91,55,93,0,42,112,97,105,110,116,99,117,114,115,111,114,0,101,109,105,116,116,101,114,100,105,115,116,0,115,101,108,101,99,116,109,111,100,101,0,101,100,105,116,116,121,112,101,0,102,97,100,101,95,102,114,97,109,101,115,0,42,115,104,97,112,101,95,111,98,106,101,99,116,0,116,114,97,110,115,102,111,114,109,95,109,111,100,101,0,114,97,100,105,97,108,95,115,121,
109,109,91,51,93,0,100,101,116,97,105,108,95,115,105,122,101,0,115,121,109,109,101,116,114,105,122,101,95,100,105,114,101,99,116,105,111,110,0,103,114,97,118,105,116,121,95,102,97,99,116,111,114,0,99,111,110,115,116,97,110,116,95,100,101,116,97,105,108,0,100,101,116,97,105,108,95,112,101,114,99,101,110,116,0,42,97,117,116,111,109,97,115,107,105,110,103,95,99,97,118,105,116,121,95,99,117,114,118,101,95,111,112,0,42,103,114,97,118,105,
116,121,95,111,98,106,101,99,116,0,42,115,116,114,101,110,103,116,104,95,99,117,114,118,101,0,117,115,101,95,103,117,105,100,101,0,117,115,101,95,115,110,97,112,112,105,110,103,0,114,101,102,101,114,101,110,99,101,95,112,111,105,110,116,0,97,110,103,108,101,95,115,110,97,112,0,42,114,101,102,101,114,101,110,99,101,95,111,98,106,101,99,116,0,108,111,99,107,95,97,120,105,115,0,105,115,101,99,116,95,116,104,114,101,115,104,111,108,100,0,
42,99,117,114,95,102,97,108,108,111,102,102,0,42,99,117,114,95,112,114,105,109,105,116,105,118,101,0,103,117,105,100,101,0,42,99,117,115,116,111,109,95,105,112,111,0,108,97,115,116,95,114,97,107,101,91,50,93,0,108,97,115,116,95,114,97,107,101,95,97,110,103,108,101,0,108,97,115,116,95,115,116,114,111,107,101,95,118,97,108,105,100,0,97,118,101,114,97,103,101,95,115,116,114,111,107,101,95,97,99,99,117,109,91,51,93,0,97,118,101,
114,97,103,101,95,115,116,114,111,107,101,95,99,111,117,110,116,101,114,0,98,114,117,115,104,95,114,111,116,97,116,105,111,110,0,98,114,117,115,104,95,114,111,116,97,116,105,111,110,95,115,101,99,0,97,110,99,104,111,114,101,100,95,115,105,122,101,0,111,118,101,114,108,97,112,95,102,97,99,116,111,114,0,100,114,97,119,95,105,110,118,101,114,116,101,100,0,115,116,114,111,107,101,95,97,99,116,105,118,101,0,100,114,97,119,95,97,110,99,104,
111,114,101,100,0,100,111,95,108,105,110,101,97,114,95,99,111,110,118,101,114,115,105,111,110,0,108,97,115,116,95,108,111,99,97,116,105,111,110,91,51,93,0,108,97,115,116,95,104,105,116,0,97,110,99,104,111,114,101,100,95,105,110,105,116,105,97,108,95,109,111,117,115,101,91,50,93,0,112,105,120,101,108,95,114,97,100,105,117,115,0,105,110,105,116,105,97,108,95,112,105,120,101,108,95,114,97,100,105,117,115,0,115,116,97,114,116,95,112,105,
120,101,108,95,114,97,100,105,117,115,0,115,105,122,101,95,112,114,101,115,115,117,114,101,95,118,97,108,117,101,0,116,101,120,95,109,111,117,115,101,91,50,93,0,109,97,115,107,95,116,101,120,95,109,111,117,115,101,91,50,93,0,42,99,111,108,111,114,115,112,97,99,101,0,99,117,114,118,101,95,116,121,112,101,0,100,101,112,116,104,95,109,111,100,101,0,115,117,114,102,97,99,101,95,112,108,97,110,101,0,101,114,114,111,114,95,116,104,114,101,
115,104,111,108,100,0,114,97,100,105,117,115,95,109,105,110,0,114,97,100,105,117,115,95,109,97,120,0,114,97,100,105,117,115,95,116,97,112,101,114,95,115,116,97,114,116,0,114,97,100,105,117,115,95,116,97,112,101,114,95,101,110,100,0,115,117,114,102,97,99,101,95,111,102,102,115,101,116,0,99,111,114,110,101,114,95,97,110,103,108,101,0,111,118,101,114,104,97,110,103,95,97,120,105,115,0,111,118,101,114,104,97,110,103,95,109,105,110,0,111,
118,101,114,104,97,110,103,95,109,97,120,0,116,104,105,99,107,110,101,115,115,95,109,105,110,0,116,104,105,99,107,110,101,115,115,95,109,97,120,0,116,104,105,99,107,110,101,115,115,95,115,97,109,112,108,101,115,0,100,105,115,116,111,114,116,95,109,105,110,0,100,105,115,116,111,114,116,95,109,97,120,0,115,104,97,114,112,95,109,105,110,0,115,104,97,114,112,95,109,97,120,0,115,110,97,112,95,109,111,100,101,0,115,110,97,112,95,102,108,97,
103,0,111,118,101,114,108,97,112,95,109,111,100,101,0,115,110,97,112,95,100,105,115,116,97,110,99,101,0,112,105,118,111,116,95,112,111,105,110,116,0,42,118,112,97,105,110,116,0,42,119,112,97,105,110,116,0,117,118,115,99,117,108,112,116,0,42,103,112,95,112,97,105,110,116,0,42,103,112,95,118,101,114,116,101,120,112,97,105,110,116,0,42,103,112,95,115,99,117,108,112,116,112,97,105,110,116,0,42,103,112,95,119,101,105,103,104,116,112,97,
105,110,116,0,42,99,117,114,118,101,115,95,115,99,117,108,112,116,0,118,103,114,111,117,112,95,119,101,105,103,104,116,0,100,111,117,98,108,105,109,105,116,0,97,117,116,111,109,101,114,103,101,0,111,98,106,101,99,116,95,102,108,97,103,0,117,110,119,114,97,112,112,101,114,0,117,118,99,97,108,99,95,102,108,97,103,0,117,118,95,102,108,97,103,0,117,118,95,115,101,108,101,99,116,109,111,100,101,0,117,118,95,115,116,105,99,107,121,0,117,
118,99,97,108,99,95,109,97,114,103,105,110,0,97,117,116,111,105,107,95,99,104,97,105,110,108,101,110,0,103,112,101,110,99,105,108,95,102,108,97,103,115,0,103,112,101,110,99,105,108,95,118,51,100,95,97,108,105,103,110,0,103,112,101,110,99,105,108,95,118,50,100,95,97,108,105,103,110,0,97,110,110,111,116,97,116,101,95,118,51,100,95,97,108,105,103,110,0,97,110,110,111,116,97,116,101,95,116,104,105,99,107,110,101,115,115,0,103,112,101,
110,99,105,108,95,115,117,114,102,97,99,101,95,111,102,102,115,101,116,0,103,112,101,110,99,105,108,95,115,101,108,101,99,116,109,111,100,101,95,101,100,105,116,0,103,112,101,110,99,105,108,95,115,101,108,101,99,116,109,111,100,101,95,115,99,117,108,112,116,0,103,112,95,115,99,117,108,112,116,0,103,112,95,105,110,116,101,114,112,111,108,97,116,101,0,105,109,97,112,97,105,110,116,0,112,97,105,110,116,95,109,111,100,101,0,112,97,114,116,105,
99,108,101,0,112,114,111,112,111,114,116,105,111,110,97,108,95,115,105,122,101,0,115,101,108,101,99,116,95,116,104,114,101,115,104,0,107,101,121,105,110,103,95,102,108,97,103,0,97,117,116,111,107,101,121,95,109,111,100,101,0,107,101,121,102,114,97,109,101,95,116,121,112,101,0,109,117,108,116,105,114,101,115,95,115,117,98,100,105,118,95,116,121,112,101,0,101,100,103,101,95,109,111,100,101,0,101,100,103,101,95,109,111,100,101,95,108,105,118,101,
95,117,110,119,114,97,112,0,116,114,97,110,115,102,111,114,109,95,112,105,118,111,116,95,112,111,105,110,116,0,116,114,97,110,115,102,111,114,109,95,102,108,97,103,0,115,110,97,112,95,110,111,100,101,95,109,111,100,101,0,115,110,97,112,95,117,118,95,109,111,100,101,0,115,110,97,112,95,97,110,105,109,95,109,111,100,101,0,115,110,97,112,95,102,108,97,103,95,110,111,100,101,0,115,110,97,112,95,102,108,97,103,95,115,101,113,0,115,110,97,
112,95,102,108,97,103,95,97,110,105,109,0,115,110,97,112,95,117,118,95,102,108,97,103,0,115,110,97,112,95,116,97,114,103,101,116,0,115,110,97,112,95,116,114,97,110,115,102,111,114,109,95,109,111,100,101,95,102,108,97,103,0,115,110,97,112,95,102,97,99,101,95,110,101,97,114,101,115,116,95,115,116,101,112,115,0,112,114,111,112,111,114,116,105,111,110,97,108,95,101,100,105,116,0,112,114,111,112,95,109,111,100,101,0,112,114,111,112,111,114,
116,105,111,110,97,108,95,111,98,106,101,99,116,115,0,112,114,111,112,111,114,116,105,111,110,97,108,95,109,97,115,107,0,112,114,111,112,111,114,116,105,111,110,97,108,95,97,99,116,105,111,110,0,112,114,111,112,111,114,116,105,111,110,97,108,95,102,99,117,114,118,101,0,108,111,99,107,95,109,97,114,107,101,114,115,0,97,117,116,111,95,110,111,114,109,97,108,105,122,101,0,119,112,97,105,110,116,95,108,111,99,107,95,114,101,108,97,116,105,118,
101,0,109,117,108,116,105,112,97,105,110,116,0,119,101,105,103,104,116,117,115,101,114,0,118,103,114,111,117,112,115,117,98,115,101,116,0,103,112,101,110,99,105,108,95,115,101,108,101,99,116,109,111,100,101,95,118,101,114,116,101,120,0,117,118,95,115,99,117,108,112,116,95,115,101,116,116,105,110,103,115,0,119,111,114,107,115,112,97,99,101,95,116,111,111,108,95,116,121,112,101,0,95,112,97,100,53,91,49,93,0,115,99,117,108,112,116,95,112,97,
105,110,116,95,115,101,116,116,105,110,103,115,0,115,99,117,108,112,116,95,112,97,105,110,116,95,117,110,105,102,105,101,100,95,115,105,122,101,0,115,99,117,108,112,116,95,112,97,105,110,116,95,117,110,105,102,105,101,100,95,117,110,112,114,111,106,101,99,116,101,100,95,114,97,100,105,117,115,0,115,99,117,108,112,116,95,112,97,105,110,116,95,117,110,105,102,105,101,100,95,97,108,112,104,97,0,117,110,105,102,105,101,100,95,112,97,105,110,116,95,
115,101,116,116,105,110,103,115,0,99,117,114,118,101,95,112,97,105,110,116,95,115,101,116,116,105,110,103,115,0,115,116,97,116,118,105,115,0,110,111,114,109,97,108,95,118,101,99,116,111,114,91,51,93,0,95,112,97,100,54,91,52,93,0,42,99,117,115,116,111,109,95,98,101,118,101,108,95,112,114,111,102,105,108,101,95,112,114,101,115,101,116,0,42,115,101,113,117,101,110,99,101,114,95,116,111,111,108,95,115,101,116,116,105,110,103,115,0,115,110,
97,112,95,109,111,100,101,95,116,111,111,108,115,0,112,108,97,110,101,95,97,120,105,115,0,112,108,97,110,101,95,100,101,112,116,104,0,112,108,97,110,101,95,111,114,105,101,110,116,0,117,115,101,95,112,108,97,110,101,95,97,120,105,115,95,97,117,116,111,0,95,112,97,100,55,91,50,93,0,115,110,97,112,95,97,110,103,108,101,95,105,110,99,114,101,109,101,110,116,95,50,100,0,115,110,97,112,95,97,110,103,108,101,95,105,110,99,114,101,109,
101,110,116,95,50,100,95,112,114,101,99,105,115,105,111,110,0,115,110,97,112,95,97,110,103,108,101,95,105,110,99,114,101,109,101,110,116,95,51,100,0,115,110,97,112,95,97,110,103,108,101,95,105,110,99,114,101,109,101,110,116,95,51,100,95,112,114,101,99,105,115,105,111,110,0,115,99,97,108,101,95,108,101,110,103,116,104,0,115,121,115,116,101,109,0,115,121,115,116,101,109,95,114,111,116,97,116,105,111,110,0,108,101,110,103,116,104,95,117,110,
105,116,0,109,97,115,115,95,117,110,105,116,0,116,105,109,101,95,117,110,105,116,0,116,101,109,112,101,114,97,116,117,114,101,95,117,110,105,116,0,113,117,105,99,107,95,99,97,99,104,101,95,115,116,101,112,0,116,105,116,108,101,91,50,93,0,97,99,116,105,111,110,91,50,93,0,116,105,116,108,101,95,99,101,110,116,101,114,91,50,93,0,97,99,116,105,111,110,95,99,101,110,116,101,114,91,50,93,0,108,105,103,104,116,95,100,105,114,101,99,
116,105,111,110,91,51,93,0,115,104,97,100,111,119,95,115,104,105,102,116,0,115,104,97,100,111,119,95,102,111,99,117,115,0,109,97,116,99,97,112,95,115,115,97,111,95,100,105,115,116,97,110,99,101,0,109,97,116,99,97,112,95,115,115,97,111,95,97,116,116,101,110,117,97,116,105,111,110,0,109,97,116,99,97,112,95,115,115,97,111,95,115,97,109,112,108,101,115,0,118,105,101,119,112,111,114,116,95,97,97,0,114,101,110,100,101,114,95,97,97,
0,115,104,97,100,105,110,103,0,115,99,114,101,101,110,95,116,114,97,99,101,95,113,117,97,108,105,116,121,0,115,99,114,101,101,110,95,116,114,97,99,101,95,116,104,105,99,107,110,101,115,115,0,116,114,97,99,101,95,109,97,120,95,114,111,117,103,104,110,101,115,115,0,114,101,115,111,108,117,116,105,111,110,95,115,99,97,108,101,0,100,101,110,111,105,115,101,95,115,116,97,103,101,115,0,103,105,95,100,105,102,102,117,115,101,95,98,111,117,110,
99,101,115,0,103,105,95,99,117,98,101,109,97,112,95,114,101,115,111,108,117,116,105,111,110,0,103,105,95,118,105,115,105,98,105,108,105,116,121,95,114,101,115,111,108,117,116,105,111,110,0,103,105,95,105,114,114,97,100,105,97,110,99,101,95,115,109,111,111,116,104,105,110,103,0,103,105,95,103,108,111,115,115,121,95,99,108,97,109,112,0,103,105,95,102,105,108,116,101,114,95,113,117,97,108,105,116,121,0,103,105,95,105,114,114,97,100,105,97,110,
99,101,95,112,111,111,108,95,115,105,122,101,0,103,105,95,99,117,98,101,109,97,112,95,100,114,97,119,95,115,105,122,101,0,103,105,95,105,114,114,97,100,105,97,110,99,101,95,100,114,97,119,95,115,105,122,101,0,116,97,97,95,115,97,109,112,108,101,115,0,116,97,97,95,114,101,110,100,101,114,95,115,97,109,112,108,101,115,0,115,115,115,95,115,97,109,112,108,101,115,0,115,115,115,95,106,105,116,116,101,114,95,116,104,114,101,115,104,111,108,
100,0,115,115,114,95,113,117,97,108,105,116,121,0,115,115,114,95,109,97,120,95,114,111,117,103,104,110,101,115,115,0,115,115,114,95,116,104,105,99,107,110,101,115,115,0,115,115,114,95,98,111,114,100,101,114,95,102,97,100,101,0,115,115,114,95,102,105,114,101,102,108,121,95,102,97,99,0,118,111,108,117,109,101,116,114,105,99,95,115,116,97,114,116,0,118,111,108,117,109,101,116,114,105,99,95,101,110,100,0,118,111,108,117,109,101,116,114,105,99,
95,116,105,108,101,95,115,105,122,101,0,118,111,108,117,109,101,116,114,105,99,95,115,97,109,112,108,101,115,0,118,111,108,117,109,101,116,114,105,99,95,115,97,109,112,108,101,95,100,105,115,116,114,105,98,117,116,105,111,110,0,118,111,108,117,109,101,116,114,105,99,95,108,105,103,104,116,95,99,108,97,109,112,0,118,111,108,117,109,101,116,114,105,99,95,115,104,97,100,111,119,95,115,97,109,112,108,101,115,0,118,111,108,117,109,101,116,114,105,99,
95,114,97,121,95,100,101,112,116,104,0,103,116,97,111,95,100,105,115,116,97,110,99,101,0,103,116,97,111,95,102,97,99,116,111,114,0,103,116,97,111,95,113,117,97,108,105,116,121,0,103,116,97,111,95,116,104,105,99,107,110,101,115,115,0,103,116,97,111,95,102,111,99,117,115,0,103,116,97,111,95,114,101,115,111,108,117,116,105,111,110,0,102,97,115,116,95,103,105,95,115,116,101,112,95,99,111,117,110,116,0,102,97,115,116,95,103,105,95,114,
97,121,95,99,111,117,110,116,0,102,97,115,116,95,103,105,95,100,105,115,116,97,110,99,101,0,102,97,115,116,95,103,105,95,116,104,105,99,107,110,101,115,115,95,110,101,97,114,0,102,97,115,116,95,103,105,95,116,104,105,99,107,110,101,115,115,95,102,97,114,0,102,97,115,116,95,103,105,95,109,101,116,104,111,100,0,98,111,107,101,104,95,111,118,101,114,98,108,117,114,0,98,111,107,101,104,95,109,97,120,95,115,105,122,101,0,98,111,107,101,
104,95,116,104,114,101,115,104,111,108,100,0,98,111,107,101,104,95,110,101,105,103,104,98,111,114,95,109,97,120,0,98,111,107,101,104,95,100,101,110,111,105,115,101,95,102,97,99,0,98,108,111,111,109,95,99,111,108,111,114,91,51,93,0,98,108,111,111,109,95,116,104,114,101,115,104,111,108,100,0,98,108,111,111,109,95,107,110,101,101,0,98,108,111,111,109,95,105,110,116,101,110,115,105,116,121,0,98,108,111,111,109,95,114,97,100,105,117,115,0,
98,108,111,111,109,95,99,108,97,109,112,0,109,111,116,105,111,110,95,98,108,117,114,95,109,97,120,0,109,111,116,105,111,110,95,98,108,117,114,95,115,116,101,112,115,0,109,111,116,105,111,110,95,98,108,117,114,95,100,101,112,116,104,95,115,99,97,108,101,0,115,104,97,100,111,119,95,109,101,116,104,111,100,0,115,104,97,100,111,119,95,99,117,98,101,95,115,105,122,101,0,115,104,97,100,111,119,95,99,97,115,99,97,100,101,95,115,105,122,101,
0,115,104,97,100,111,119,95,112,111,111,108,95,115,105,122,101,0,115,104,97,100,111,119,95,114,97,121,95,99,111,117,110,116,0,115,104,97,100,111,119,95,115,116,101,112,95,99,111,117,110,116,0,115,104,97,100,111,119,95,114,101,115,111,108,117,116,105,111,110,95,115,99,97,108,101,0,99,108,97,109,112,95,115,117,114,102,97,99,101,95,100,105,114,101,99,116,0,99,108,97,109,112,95,115,117,114,102,97,99,101,95,105,110,100,105,114,101,99,116,
0,99,108,97,109,112,95,118,111,108,117,109,101,95,100,105,114,101,99,116,0,99,108,97,109,112,95,118,111,108,117,109,101,95,105,110,100,105,114,101,99,116,0,114,97,121,95,116,114,97,99,105,110,103,95,109,101,116,104,111,100,0,114,97,121,95,116,114,97,99,105,110,103,95,111,112,116,105,111,110,115,0,108,105,103,104,116,95,116,104,114,101,115,104,111,108,100,0,115,109,97,97,95,116,104,114,101,115,104,111,108,100,0,101,120,112,111,114,116,95,
109,101,116,104,111,100,0,105,110,100,101,120,95,99,117,115,116,111,109,0,42,119,111,114,108,100,0,42,115,101,116,0,99,117,114,115,111,114,0,108,97,121,97,99,116,0,42,101,100,0,42,116,111,111,108,115,101,116,116,105,110,103,115,0,115,97,102,101,95,97,114,101,97,115,0,97,117,100,105,111,0,116,114,97,110,115,102,111,114,109,95,115,112,97,99,101,115,0,111,114,105,101,110,116,97,116,105,111,110,95,115,108,111,116,115,91,52,93,0,42,
115,111,117,110,100,95,115,99,101,110,101,0,42,112,108,97,121,98,97,99,107,95,104,97,110,100,108,101,0,42,115,111,117,110,100,95,115,99,114,117,98,95,104,97,110,100,108,101,0,42,115,112,101,97,107,101,114,95,104,97,110,100,108,101,115,0,42,102,112,115,95,105,110,102,111,0,42,100,101,112,115,103,114,97,112,104,95,104,97,115,104,0,95,112,97,100,55,91,52,93,0,97,99,116,105,118,101,95,107,101,121,105,110,103,115,101,116,0,107,101,
121,105,110,103,115,101,116,115,0,117,110,105,116,0,112,104,121,115,105,99,115,95,115,101,116,116,105,110,103,115,0,42,95,112,97,100,56,0,99,117,115,116,111,109,100,97,116,97,95,109,97,115,107,0,99,117,115,116,111,109,100,97,116,97,95,109,97,115,107,95,109,111,100,97,108,0,115,101,113,117,101,110,99,101,114,95,99,111,108,111,114,115,112,97,99,101,95,115,101,116,116,105,110,103,115,0,42,114,105,103,105,100,98,111,100,121,95,119,111,114,
108,100,0,118,105,101,119,95,108,97,121,101,114,115,0,42,109,97,115,116,101,114,95,99,111,108,108,101,99,116,105,111,110,0,42,108,97,121,101,114,95,112,114,111,112,101,114,116,105,101,115,0,115,105,109,117,108,97,116,105,111,110,95,102,114,97,109,101,95,115,116,97,114,116,0,115,105,109,117,108,97,116,105,111,110,95,102,114,97,109,101,95,101,110,100,0,100,105,115,112,108,97,121,0,103,114,101,97,115,101,95,112,101,110,99,105,108,95,115,101,
116,116,105,110,103,115,0,104,121,100,114,97,0,42,95,112,97,100,57,0,118,101,114,116,98,97,115,101,0,101,100,103,101,98,97,115,101,0,97,114,101,97,98,97,115,101,0,119,105,110,105,100,0,114,101,100,114,97,119,115,95,102,108,97,103,0,100,111,95,100,114,97,119,0,100,111,95,114,101,102,114,101,115,104,0,100,111,95,100,114,97,119,95,103,101,115,116,117,114,101,0,100,111,95,100,114,97,119,95,112,97,105,110,116,99,117,114,115,111,114,
0,100,111,95,100,114,97,119,95,100,114,97,103,0,115,107,105,112,95,104,97,110,100,108,105,110,103,0,115,99,114,117,98,98,105,110,103,0,42,97,99,116,105,118,101,95,114,101,103,105,111,110,0,42,97,110,105,109,116,105,109,101,114,0,42,99,111,110,116,101,120,116,0,42,116,111,111,108,95,116,105,112,0,42,110,101,119,118,0,118,101,99,0,42,118,49,0,42,118,50,0,42,105,100,110,97,109,101,0,42,116,121,112,101,0,42,108,97,121,111,
117,116,0,112,97,110,101,108,110,97,109,101,91,54,52,93,0,42,100,114,97,119,110,97,109,101,0,111,102,115,120,0,111,102,115,121,0,98,108,111,99,107,115,105,122,101,120,0,98,108,111,99,107,115,105,122,101,121,0,108,97,98,101,108,111,102,115,0,115,111,114,116,111,114,100,101,114,0,42,97,99,116,105,118,101,100,97,116,97,0,108,97,121,111,117,116,95,112,97,110,101,108,95,115,116,97,116,101,115,0,108,105,115,116,95,105,100,91,49,50,
56,93,0,108,97,121,111,117,116,95,116,121,112,101,0,108,105,115,116,95,115,99,114,111,108,108,0,108,105,115,116,95,103,114,105,112,0,108,105,115,116,95,108,97,115,116,95,108,101,110,0,108,105,115,116,95,108,97,115,116,95,97,99,116,105,118,101,105,0,102,105,108,116,101,114,95,98,121,110,97,109,101,91,49,50,56,93,0,102,105,108,116,101,114,95,102,108,97,103,0,102,105,108,116,101,114,95,115,111,114,116,95,102,108,97,103,0,42,100,121,
110,95,100,97,116,97,0,109,97,116,91,51,93,91,51,93,0,112,114,101,118,105,101,119,95,105,100,91,54,52,93,0,99,117,114,95,102,105,120,101,100,95,104,101,105,103,104,116,0,115,105,122,101,95,109,105,110,0,115,105,122,101,95,109,97,120,0,97,108,105,103,110,0,42,116,111,111,108,0,105,115,95,116,111,111,108,95,115,101,116,0,42,118,51,0,42,118,52,0,42,102,117,108,108,0,98,117,116,115,112,97,99,101,116,121,112,101,0,98,117,
116,115,112,97,99,101,116,121,112,101,95,115,117,98,116,121,112,101,0,119,105,110,120,0,119,105,110,121,0,104,101,97,100,101,114,116,121,112,101,0,114,101,103,105,111,110,95,97,99,116,105,118,101,95,119,105,110,0,42,103,108,111,98,97,108,0,115,112,97,99,101,100,97,116,97,0,104,97,110,100,108,101,114,115,0,97,99,116,105,111,110,122,111,110,101,115,0,42,99,97,116,101,103,111,114,121,0,118,105,115,105,98,108,101,95,114,101,99,116,0,
111,102,102,115,101,116,95,120,0,111,102,102,115,101,116,95,121,0,42,98,108,111,99,107,95,110,97,109,101,95,109,97,112,0,42,112,111,112,117,112,95,98,108,111,99,107,95,112,97,110,101,108,0,119,105,110,114,99,116,0,100,114,97,119,114,99,116,0,99,97,116,101,103,111,114,121,95,115,99,114,111,108,108,0,118,105,115,105,98,108,101,0,114,101,103,105,111,110,116,121,112,101,0,97,108,105,103,110,109,101,110,116,0,111,118,101,114,108,97,112,
0,102,108,97,103,102,117,108,108,115,99,114,101,101,110,0,117,105,98,108,111,99,107,115,0,112,97,110,101,108,115,0,112,97,110,101,108,115,95,99,97,116,101,103,111,114,121,95,97,99,116,105,118,101,0,117,105,95,108,105,115,116,115,0,117,105,95,112,114,101,118,105,101,119,115,0,112,97,110,101,108,115,95,99,97,116,101,103,111,114,121,0,42,103,105,122,109,111,95,109,97,112,0,42,114,101,103,105,111,110,116,105,109,101,114,0,42,100,114,97,
119,95,98,117,102,102,101,114,0,42,104,101,97,100,101,114,115,116,114,0,42,114,101,103,105,111,110,100,97,116,97,0,97,115,115,101,116,95,108,105,98,114,97,114,121,95,114,101,102,101,114,101,110,99,101,0,101,110,97,98,108,101,100,95,99,97,116,97,108,111,103,95,112,97,116,104,115,0,42,97,99,116,105,118,101,95,99,97,116,97,108,111,103,95,112,97,116,104,0,115,101,97,114,99,104,95,115,116,114,105,110,103,91,54,52,93,0,112,114,101,
118,105,101,119,95,115,105,122,101,0,100,105,115,112,108,97,121,95,102,108,97,103,0,112,114,101,102,101,114,114,101,100,95,114,111,119,95,99,111,117,110,116,0,115,104,101,108,118,101,115,0,42,97,99,116,105,118,101,95,115,104,101,108,102,0,111,114,105,103,95,119,105,100,116,104,0,111,114,105,103,95,104,101,105,103,104,116,0,111,114,105,103,95,102,112,115,0,116,111,112,0,98,111,116,116,111,109,0,108,101,102,116,0,114,105,103,104,116,0,120,
111,102,115,0,121,111,102,115,0,111,114,105,103,105,110,91,50,93,0,102,105,108,101,91,50,53,54,93,0,98,117,105,108,100,95,115,105,122,101,95,102,108,97,103,115,0,98,117,105,108,100,95,116,99,95,102,108,97,103,115,0,98,117,105,108,100,95,102,108,97,103,115,0,115,116,111,114,97,103,101,0,100,111,110,101,0,115,116,97,114,116,115,116,105,108,108,0,101,110,100,115,116,105,108,108,0,42,115,116,114,105,112,100,97,116,97,0,42,99,114,
111,112,0,42,116,114,97,110,115,102,111,114,109,0,42,99,111,108,111,114,95,98,97,108,97,110,99,101,0,115,116,114,105,112,95,102,114,97,109,101,95,105,110,100,101,120,0,114,101,116,105,109,105,110,103,95,102,97,99,116,111,114,0,111,114,105,103,105,110,97,108,95,115,116,114,105,112,95,102,114,97,109,101,95,105,110,100,101,120,0,111,114,105,103,105,110,97,108,95,114,101,116,105,109,105,110,103,95,102,97,99,116,111,114,0,42,116,109,112,0,
115,116,97,114,116,111,102,115,0,101,110,100,111,102,115,0,109,97,99,104,105,110,101,0,115,116,97,114,116,100,105,115,112,0,101,110,100,100,105,115,112,0,109,117,108,0,97,110,105,109,95,112,114,101,115,101,101,107,0,115,116,114,101,97,109,105,110,100,101,120,0,109,117,108,116,105,99,97,109,95,115,111,117,114,99,101,0,99,108,105,112,95,102,108,97,103,0,42,115,116,114,105,112,0,42,115,99,101,110,101,95,99,97,109,101,114,97,0,42,109,
97,115,107,0,101,102,102,101,99,116,95,102,97,100,101,114,0,115,112,101,101,100,95,102,97,100,101,114,0,42,115,101,113,49,0,42,115,101,113,50,0,42,115,101,113,51,0,115,101,113,98,97,115,101,0,42,115,111,117,110,100,0,42,115,99,101,110,101,95,115,111,117,110,100,0,112,97,110,0,115,116,114,111,98,101,0,42,101,102,102,101,99,116,100,97,116,97,0,97,110,105,109,95,115,116,97,114,116,111,102,115,0,97,110,105,109,95,101,110,100,
111,102,115,0,98,108,101,110,100,95,111,112,97,99,105,116,121,0,109,101,100,105,97,95,112,108,97,121,98,97,99,107,95,114,97,116,101,0,115,112,101,101,100,95,102,97,99,116,111,114,0,42,114,101,116,105,109,105,110,103,95,104,97,110,100,108,101,115,0,114,101,116,105,109,105,110,103,95,104,97,110,100,108,101,95,110,117,109,0,42,111,108,100,98,97,115,101,112,0,42,111,108,100,95,99,104,97,110,110,101,108,115,0,42,112,97,114,115,101,113,
0,100,105,115,112,95,114,97,110,103,101,91,50,93,0,42,115,101,113,117,101,110,99,101,95,108,111,111,107,117,112,0,42,109,101,100,105,97,95,112,114,101,115,101,110,99,101,0,42,115,101,113,98,97,115,101,112,0,42,100,105,115,112,108,97,121,101,100,95,99,104,97,110,110,101,108,115,0,109,101,116,97,115,116,97,99,107,0,42,97,99,116,95,115,101,113,0,97,99,116,95,105,109,97,103,101,100,105,114,91,49,48,50,52,93,0,97,99,116,95,
115,111,117,110,100,100,105,114,91,49,48,50,52,93,0,112,114,111,120,121,95,100,105,114,91,49,48,50,52,93,0,112,114,111,120,121,95,115,116,111,114,97,103,101,0,111,118,101,114,95,111,102,115,0,111,118,101,114,95,99,102,114,97,0,111,118,101,114,95,102,108,97,103,0,111,118,101,114,95,98,111,114,100,101,114,0,115,104,111,119,95,109,105,115,115,105,110,103,95,109,101,100,105,97,95,102,108,97,103,0,114,101,99,121,99,108,101,95,109,97,
120,95,99,111,115,116,0,42,112,114,101,102,101,116,99,104,95,106,111,98,0,100,105,115,107,95,99,97,99,104,101,95,116,105,109,101,115,116,97,109,112,0,101,100,103,101,87,105,100,116,104,0,102,111,114,119,97,114,100,0,119,105,112,101,116,121,112,101,0,102,77,105,110,105,0,102,67,108,97,109,112,0,102,66,111,111,115,116,0,100,68,105,115,116,0,100,81,117,97,108,105,116,121,0,98,78,111,67,111,109,112,0,83,99,97,108,101,120,73,110,
105,0,83,99,97,108,101,121,73,110,105,0,120,73,110,105,0,121,73,110,105,0,114,111,116,73,110,105,0,117,110,105,102,111,114,109,95,115,99,97,108,101,0,99,111,108,91,51,93,0,42,102,114,97,109,101,77,97,112,0,103,108,111,98,97,108,83,112,101,101,100,0,115,112,101,101,100,95,99,111,110,116,114,111,108,95,116,121,112,101,0,115,112,101,101,100,95,102,97,100,101,114,95,108,101,110,103,116,104,0,115,112,101,101,100,95,102,97,100,101,
114,95,102,114,97,109,101,95,110,117,109,98,101,114,0,116,101,120,116,91,53,49,50,93,0,42,116,101,120,116,95,102,111,110,116,0,116,101,120,116,95,98,108,102,95,105,100,0,116,101,120,116,95,115,105,122,101,0,115,104,97,100,111,119,95,99,111,108,111,114,91,52,93,0,98,111,120,95,99,111,108,111,114,91,52,93,0,111,117,116,108,105,110,101,95,99,111,108,111,114,91,52,93,0,108,111,99,91,50,93,0,119,114,97,112,95,119,105,100,116,
104,0,98,111,120,95,109,97,114,103,105,110,0,115,104,97,100,111,119,95,97,110,103,108,101,0,115,104,97,100,111,119,95,111,102,102,115,101,116,0,115,104,97,100,111,119,95,98,108,117,114,0,111,117,116,108,105,110,101,95,119,105,100,116,104,0,98,108,101,110,100,95,101,102,102,101,99,116,0,109,97,115,107,95,105,110,112,117,116,95,116,121,112,101,0,109,97,115,107,95,116,105,109,101,0,42,109,97,115,107,95,115,101,113,117,101,110,99,101,0,
42,109,97,115,107,95,105,100,0,99,111,108,111,114,95,98,97,108,97,110,99,101,0,99,111,108,111,114,95,109,117,108,116,105,112,108,121,0,99,117,114,118,101,95,109,97,112,112,105,110,103,0,98,114,105,103,104,116,0,119,104,105,116,101,95,118,97,108,117,101,91,51,93,0,97,100,97,112,116,97,116,105,111,110,0,99,111,114,114,101,99,116,105,111,110,0,103,114,97,112,104,105,99,115,0,117,105,100,95,0,42,102,120,95,115,104,0,42,102,120,
95,115,104,95,98,0,42,102,120,95,115,104,95,99,0,115,104,97,100,101,114,102,120,0,114,97,100,105,117,115,91,50,93,0,108,111,119,95,99,111,108,111,114,91,52,93,0,104,105,103,104,95,99,111,108,111,114,91,52,93,0,102,108,105,112,109,111,100,101,0,103,108,111,119,95,99,111,108,111,114,91,52,93,0,115,101,108,101,99,116,95,99,111,108,111,114,91,51,93,0,98,108,117,114,91,50,93,0,114,103,98,97,91,52,93,0,114,105,109,95,
114,103,98,91,51,93,0,109,97,115,107,95,114,103,98,91,51,93,0,115,104,97,100,111,119,95,114,103,98,97,91,52,93,0,116,114,97,110,115,112,97,114,101,110,116,0,42,110,101,119,112,97,99,107,101,100,102,105,108,101,0,97,116,116,101,110,117,97,116,105,111,110,0,109,105,110,95,103,97,105,110,0,109,97,120,95,103,97,105,110,0,111,102,102,115,101,116,95,116,105,109,101,0,42,119,97,118,101,102,111,114,109,0,42,115,112,105,110,108,111,
99,107,0,115,97,109,112,108,101,114,97,116,101,0,114,112,116,95,109,97,115,107,0,115,112,97,99,101,95,115,117,98,116,121,112,101,0,109,97,105,110,98,0,109,97,105,110,98,111,0,109,97,105,110,98,117,115,101,114,0,111,117,116,108,105,110,101,114,95,115,121,110,99,0,100,97,116,97,105,99,111,110,0,42,112,105,110,105,100,0,42,116,101,120,117,115,101,114,0,116,114,101,101,0,42,116,114,101,101,115,116,111,114,101,0,111,117,116,108,105,
110,101,118,105,115,0,108,105,98,95,111,118,101,114,114,105,100,101,95,118,105,101,119,95,109,111,100,101,0,115,116,111,114,101,102,108,97,103,0,115,101,97,114,99,104,95,102,108,97,103,115,0,115,121,110,99,95,115,101,108,101,99,116,95,100,105,114,116,121,0,102,105,108,116,101,114,95,115,116,97,116,101,0,115,104,111,119,95,114,101,115,116,114,105,99,116,95,102,108,97,103,115,0,102,105,108,116,101,114,95,105,100,95,116,121,112,101,0,103,104,
111,115,116,95,99,117,114,118,101,115,0,42,97,100,115,0,99,117,114,115,111,114,84,105,109,101,0,99,117,114,115,111,114,86,97,108,0,97,114,111,117,110,100,0,99,104,97,110,115,104,111,119,110,0,122,101,98,114,97,0,111,118,101,114,108,97,121,95,116,121,112,101,0,100,114,97,119,95,102,108,97,103,0,103,105,122,109,111,95,102,108,97,103,0,99,117,114,115,111,114,91,50,93,0,112,114,101,118,105,101,119,95,111,118,101,114,108,97,121,0,
116,105,109,101,108,105,110,101,95,111,118,101,114,108,97,121,0,99,97,99,104,101,95,111,118,101,114,108,97,121,0,100,114,97,119,95,116,121,112,101,0,111,118,101,114,108,97,121,95,109,111,100,101,0,98,108,101,110,100,95,102,97,99,116,111,114,0,116,105,116,108,101,91,57,54,93,0,100,105,114,91,49,48,57,48,93,0,114,101,110,97,109,101,102,105,108,101,91,50,53,54,93,0,114,101,110,97,109,101,95,102,108,97,103,0,42,114,101,110,97,
109,101,95,105,100,0,102,105,108,116,101,114,95,103,108,111,98,91,50,53,54,93,0,102,105,108,116,101,114,95,115,101,97,114,99,104,91,54,52,93,0,102,105,108,116,101,114,95,105,100,0,97,99,116,105,118,101,95,102,105,108,101,0,104,105,103,104,108,105,103,104,116,95,102,105,108,101,0,115,101,108,95,102,105,114,115,116,0,115,101,108,95,108,97,115,116,0,116,104,117,109,98,110,97,105,108,95,115,105,122,101,0,115,111,114,116,0,100,101,116,
97,105,108,115,95,102,108,97,103,115,0,114,101,99,117,114,115,105,111,110,95,108,101,118,101,108,0,95,112,97,100,52,91,50,93,0,98,97,115,101,95,112,97,114,97,109,115,0,97,115,115,101,116,95,108,105,98,114,97,114,121,95,114,101,102,0,97,115,115,101,116,95,99,97,116,97,108,111,103,95,118,105,115,105,98,105,108,105,116,121,0,105,109,112,111,114,116,95,116,121,112,101,0,98,114,111,119,115,101,95,109,111,100,101,0,102,111,108,100,101,
114,115,95,112,114,101,118,0,102,111,108,100,101,114,115,95,110,101,120,116,0,115,99,114,111,108,108,95,111,102,102,115,101,116,0,42,112,97,114,97,109,115,0,42,97,115,115,101,116,95,112,97,114,97,109,115,0,42,102,105,108,101,115,0,42,102,111,108,100,101,114,115,95,112,114,101,118,0,42,102,111,108,100,101,114,115,95,110,101,120,116,0,102,111,108,100,101,114,95,104,105,115,116,111,114,105,101,115,0,42,111,112,0,42,115,109,111,111,116,104,
115,99,114,111,108,108,95,116,105,109,101,114,0,42,112,114,101,118,105,101,119,115,95,116,105,109,101,114,0,114,101,99,101,110,116,110,114,0,98,111,111,107,109,97,114,107,110,114,0,115,121,115,116,101,109,110,114,0,115,121,115,116,101,109,95,98,111,111,107,109,97,114,107,110,114,0,115,99,111,112,101,115,0,115,97,109,112,108,101,95,108,105,110,101,95,104,105,115,116,0,99,101,110,116,120,0,99,101,110,116,121,0,112,105,110,0,112,105,120,101,
108,95,115,110,97,112,95,109,111,100,101,0,108,111,99,107,0,100,116,95,117,118,0,100,116,95,117,118,115,116,114,101,116,99,104,0,103,114,105,100,95,115,104,97,112,101,95,115,111,117,114,99,101,0,117,118,95,111,112,97,99,105,116,121,0,115,116,114,101,116,99,104,95,111,112,97,99,105,116,121,0,116,105,108,101,95,103,114,105,100,95,115,104,97,112,101,91,50,93,0,99,117,115,116,111,109,95,103,114,105,100,95,115,117,98,100,105,118,91,50,
93,0,109,97,115,107,95,105,110,102,111,0,111,118,101,114,108,97,121,0,108,104,101,105,103,104,116,0,116,97,98,110,117,109,98,101,114,0,119,111,114,100,119,114,97,112,0,100,111,112,108,117,103,105,110,115,0,115,104,111,119,108,105,110,101,110,114,115,0,115,104,111,119,115,121,110,116,97,120,0,108,105,110,101,95,104,108,105,103,104,116,0,111,118,101,114,119,114,105,116,101,0,108,105,118,101,95,101,100,105,116,0,95,112,97,100,50,91,49,93,
0,102,105,110,100,115,116,114,91,50,53,54,93,0,114,101,112,108,97,99,101,115,116,114,91,50,53,54,93,0,109,97,114,103,105,110,95,99,111,108,117,109,110,0,42,112,121,95,100,114,97,119,0,42,112,121,95,101,118,101,110,116,0,42,112,121,95,98,117,116,116,111,110,0,42,112,121,95,98,114,111,119,115,101,114,99,97,108,108,98,97,99,107,0,42,112,121,95,103,108,111,98,97,108,100,105,99,116,0,108,97,115,116,115,112,97,99,101,0,115,
99,114,105,112,116,110,97,109,101,91,49,48,50,52,93,0,115,99,114,105,112,116,97,114,103,91,50,53,54,93,0,109,101,110,117,110,114,0,42,98,117,116,95,114,101,102,115,0,112,97,114,101,110,116,95,107,101,121,0,110,111,100,101,95,110,97,109,101,91,54,52,93,0,100,105,115,112,108,97,121,95,110,97,109,101,91,54,52,93,0,112,114,101,118,105,101,119,95,115,104,97,112,101,0,105,110,115,101,114,116,95,111,102,115,95,100,105,114,0,116,
114,101,101,112,97,116,104,0,42,101,100,105,116,116,114,101,101,0,116,114,101,101,95,105,100,110,97,109,101,91,54,52,93,0,116,114,101,101,116,121,112,101,0,116,101,120,102,114,111,109,0,115,104,97,100,101,114,102,114,111,109,0,103,101,111,109,101,116,114,121,95,110,111,100,101,115,95,116,121,112,101,0,42,103,101,111,109,101,116,114,121,95,110,111,100,101,115,95,116,111,111,108,95,116,114,101,101,0,108,101,110,95,97,108,108,111,99,0,42,108,
105,110,101,0,115,99,114,111,108,108,98,97,99,107,0,104,105,115,116,111,114,121,0,112,114,111,109,112,116,91,50,53,54,93,0,108,97,110,103,117,97,103,101,91,51,50,93,0,104,105,115,116,111,114,121,95,105,110,100,101,120,0,115,101,108,95,115,116,97,114,116,0,115,101,108,95,101,110,100,0,102,105,108,116,101,114,95,116,121,112,101,0,102,105,108,116,101,114,91,54,52,93,0,120,108,111,99,107,111,102,0,121,108,111,99,107,111,102,0,112,
97,116,104,95,108,101,110,103,116,104,0,115,116,97,98,109,97,116,91,52,93,91,52,93,0,117,110,105,115,116,97,98,109,97,116,91,52,93,91,52,93,0,112,111,115,116,112,114,111,99,95,102,108,97,103,0,103,112,101,110,99,105,108,95,115,114,99,0,42,100,105,115,112,108,97,121,95,110,97,109,101,0,99,111,108,117,109,110,115,0,114,111,119,95,102,105,108,116,101,114,115,0,118,105,101,119,101,114,95,112,97,116,104,0,103,101,111,109,101,116,
114,121,95,99,111,109,112,111,110,101,110,116,95,116,121,112,101,0,111,98,106,101,99,116,95,101,118,97,108,95,115,116,97,116,101,0,97,99,116,105,118,101,95,108,97,121,101,114,95,105,110,100,101,120,0,99,111,108,117,109,110,95,110,97,109,101,91,54,52,93,0,118,97,108,117,101,95,105,110,116,0,118,97,108,117,101,95,105,110,116,50,91,50,93,0,42,118,97,108,117,101,95,115,116,114,105,110,103,0,118,97,108,117,101,95,102,108,111,97,116,
0,118,97,108,117,101,95,102,108,111,97,116,50,91,50,93,0,118,97,108,117,101,95,102,108,111,97,116,51,91,51,93,0,118,97,108,117,101,95,99,111,108,111,114,91,52,93,0,118,111,108,117,109,101,95,109,97,120,0,118,111,108,117,109,101,95,109,105,110,0,100,105,115,116,97,110,99,101,95,109,97,120,0,100,105,115,116,97,110,99,101,95,114,101,102,101,114,101,110,99,101,0,99,111,110,101,95,97,110,103,108,101,95,111,117,116,101,114,0,99,
111,110,101,95,97,110,103,108,101,95,105,110,110,101,114,0,99,111,110,101,95,118,111,108,117,109,101,95,111,117,116,101,114,0,42,102,111,114,109,97,116,0,42,99,111,109,112,105,108,101,100,0,42,99,117,114,108,0,42,115,101,108,108,0,99,117,114,99,0,115,101,108,99,0,109,116,105,109,101,0,116,101,120,99,111,0,109,97,112,116,111,0,98,108,101,110,100,116,121,112,101,0,112,114,111,106,120,0,112,114,111,106,121,0,112,114,111,106,122,0,
98,114,117,115,104,95,109,97,112,95,109,111,100,101,0,98,114,117,115,104,95,97,110,103,108,101,95,109,111,100,101,0,119,104,105,99,104,95,111,117,116,112,117,116,0,111,102,115,91,51,93,0,114,111,116,0,114,97,110,100,111,109,95,97,110,103,108,101,0,107,0,100,101,102,95,118,97,114,0,99,111,108,102,97,99,0,97,108,112,104,97,102,97,99,0,116,105,109,101,102,97,99,0,108,101,110,103,116,104,102,97,99,0,107,105,110,107,102,97,99,
0,107,105,110,107,97,109,112,102,97,99,0,114,111,117,103,104,102,97,99,0,112,97,100,101,110,115,102,97,99,0,103,114,97,118,105,116,121,102,97,99,0,108,105,102,101,102,97,99,0,115,105,122,101,102,97,99,0,105,118,101,108,102,97,99,0,102,105,101,108,100,102,97,99,0,116,119,105,115,116,102,97,99,0,116,111,116,0,105,112,111,116,121,112,101,0,105,112,111,116,121,112,101,95,104,117,101,0,100,97,116,97,91,51,50,93,0,102,97,108,
108,111,102,102,95,115,111,102,116,110,101,115,115,0,112,115,121,115,95,99,97,99,104,101,95,115,112,97,99,101,0,111,98,95,99,97,99,104,101,95,115,112,97,99,101,0,42,112,111,105,110,116,95,116,114,101,101,0,42,112,111,105,110,116,95,100,97,116,97,0,110,111,105,115,101,95,115,105,122,101,0,110,111,105,115,101,95,100,101,112,116,104,0,110,111,105,115,101,95,105,110,102,108,117,101,110,99,101,0,110,111,105,115,101,95,98,97,115,105,115,
0,110,111,105,115,101,95,102,97,99,0,115,112,101,101,100,95,115,99,97,108,101,0,102,97,108,108,111,102,102,95,115,112,101,101,100,95,115,99,97,108,101,0,42,102,97,108,108,111,102,102,95,99,117,114,118,101,0,114,102,97,99,0,103,102,97,99,0,98,102,97,99,0,102,105,108,116,101,114,115,105,122,101,0,109,103,95,72,0,109,103,95,108,97,99,117,110,97,114,105,116,121,0,109,103,95,111,99,116,97,118,101,115,0,109,103,95,111,102,102,
115,101,116,0,109,103,95,103,97,105,110,0,100,105,115,116,95,97,109,111,117,110,116,0,110,115,95,111,117,116,115,99,97,108,101,0,118,110,95,119,49,0,118,110,95,119,50,0,118,110,95,119,51,0,118,110,95,119,52,0,118,110,95,109,101,120,112,0,118,110,95,100,105,115,116,109,0,118,110,95,99,111,108,116,121,112,101,0,110,111,105,115,101,100,101,112,116,104,0,110,111,105,115,101,116,121,112,101,0,110,111,105,115,101,98,97,115,105,115,0,
110,111,105,115,101,98,97,115,105,115,50,0,105,109,97,102,108,97,103,0,99,114,111,112,120,109,105,110,0,99,114,111,112,121,109,105,110,0,99,114,111,112,120,109,97,120,0,99,114,111,112,121,109,97,120,0,116,101,120,102,105,108,116,101,114,0,97,102,109,97,120,0,120,114,101,112,101,97,116,0,121,114,101,112,101,97,116,0,99,104,101,99,107,101,114,100,105,115,116,0,109,105,110,91,51,93,0,109,97,120,91,51,93,0,99,111,98,97,0,98,
108,101,110,100,95,99,111,108,111,114,91,51,93,0,42,105,110,116,114,105,110,115,105,99,115,0,100,105,115,116,111,114,116,105,111,110,95,109,111,100,101,108,0,115,101,110,115,111,114,95,119,105,100,116,104,0,112,105,120,101,108,95,97,115,112,101,99,116,0,102,111,99,97,108,0,117,110,105,116,115,0,112,114,105,110,99,105,112,97,108,95,112,111,105,110,116,91,50,93,0,112,114,105,110,99,105,112,97,108,91,50,93,0,107,49,0,107,50,0,107,
51,0,100,105,118,105,115,105,111,110,95,107,49,0,100,105,118,105,115,105,111,110,95,107,50,0,110,117,107,101,95,107,49,0,110,117,107,101,95,107,50,0,98,114,111,119,110,95,107,49,0,98,114,111,119,110,95,107,50,0,98,114,111,119,110,95,107,51,0,98,114,111,119,110,95,107,52,0,98,114,111,119,110,95,112,49,0,98,114,111,119,110,95,112,50,0,112,111,115,91,50,93,0,112,97,116,116,101,114,110,95,99,111,114,110,101,114,115,91,52,
93,91,50,93,0,115,101,97,114,99,104,95,109,105,110,91,50,93,0,115,101,97,114,99,104,95,109,97,120,91,50,93,0,112,97,116,95,109,105,110,91,50,93,0,112,97,116,95,109,97,120,91,50,93,0,109,97,114,107,101,114,115,110,114,0,42,109,97,114,107,101,114,115,0,98,117,110,100,108,101,95,112,111,115,91,51,93,0,112,97,116,95,102,108,97,103,0,115,101,97,114,99,104,95,102,108,97,103,0,102,114,97,109,101,115,95,108,105,109,105,
116,0,112,97,116,116,101,114,110,95,109,97,116,99,104,0,109,111,116,105,111,110,95,109,111,100,101,108,0,97,108,103,111,114,105,116,104,109,95,102,108,97,103,0,109,105,110,105,109,117,109,95,99,111,114,114,101,108,97,116,105,111,110,0,119,101,105,103,104,116,95,115,116,97,98,0,99,111,114,110,101,114,115,91,52,93,91,50,93,0,42,42,112,111,105,110,116,95,116,114,97,99,107,115,0,112,111,105,110,116,95,116,114,97,99,107,115,110,114,0,
105,109,97,103,101,95,111,112,97,99,105,116,121,0,108,97,115,116,95,109,97,114,107,101,114,0,100,101,102,97,117,108,116,95,109,111,116,105,111,110,95,109,111,100,101,108,0,100,101,102,97,117,108,116,95,97,108,103,111,114,105,116,104,109,95,102,108,97,103,0,100,101,102,97,117,108,116,95,109,105,110,105,109,117,109,95,99,111,114,114,101,108,97,116,105,111,110,0,100,101,102,97,117,108,116,95,112,97,116,116,101,114,110,95,115,105,122,101,0,100,
101,102,97,117,108,116,95,115,101,97,114,99,104,95,115,105,122,101,0,100,101,102,97,117,108,116,95,102,114,97,109,101,115,95,108,105,109,105,116,0,100,101,102,97,117,108,116,95,109,97,114,103,105,110,0,100,101,102,97,117,108,116,95,112,97,116,116,101,114,110,95,109,97,116,99,104,0,100,101,102,97,117,108,116,95,102,108,97,103,0,109,111,116,105,111,110,95,102,108,97,103,0,107,101,121,102,114,97,109,101,49,0,107,101,121,102,114,97,109,101,
50,0,114,101,99,111,110,115,116,114,117,99,116,105,111,110,95,102,108,97,103,0,114,101,102,105,110,101,95,99,97,109,101,114,97,95,105,110,116,114,105,110,115,105,99,115,0,99,108,101,97,110,95,102,114,97,109,101,115,0,99,108,101,97,110,95,97,99,116,105,111,110,0,99,108,101,97,110,95,101,114,114,111,114,0,111,98,106,101,99,116,95,100,105,115,116,97,110,99,101,0,116,111,116,95,116,114,97,99,107,0,97,99,116,95,116,114,97,99,107,
0,116,111,116,95,114,111,116,95,116,114,97,99,107,0,97,99,116,95,114,111,116,95,116,114,97,99,107,0,109,97,120,115,99,97,108,101,0,42,114,111,116,95,116,114,97,99,107,0,97,110,99,104,111,114,95,102,114,97,109,101,0,116,97,114,103,101,116,95,112,111,115,91,50,93,0,116,97,114,103,101,116,95,114,111,116,0,108,111,99,105,110,102,0,115,99,97,108,101,105,110,102,0,114,111,116,105,110,102,0,108,97,115,116,95,99,97,109,101,114,
97,0,99,97,109,110,114,0,42,99,97,109,101,114,97,115,0,116,114,97,99,107,115,0,112,108,97,110,101,95,116,114,97,99,107,115,0,42,97,99,116,105,118,101,95,116,114,97,99,107,0,42,97,99,116,105,118,101,95,112,108,97,110,101,95,116,114,97,99,107,0,114,101,99,111,110,115,116,114,117,99,116,105,111,110,0,109,101,115,115,97,103,101,91,50,53,54,93,0,116,111,116,95,115,101,103,109,101,110,116,0,109,97,120,95,115,101,103,109,101,
110,116,0,116,111,116,97,108,95,102,114,97,109,101,115,0,102,105,114,115,116,95,110,111,116,95,100,105,115,97,98,108,101,100,95,109,97,114,107,101,114,95,102,114,97,109,101,110,114,0,108,97,115,116,95,110,111,116,95,100,105,115,97,98,108,101,100,95,109,97,114,107,101,114,95,102,114,97,109,101,110,114,0,99,111,118,101,114,97,103,101,0,115,111,114,116,95,109,101,116,104,111,100,0,99,111,118,101,114,97,103,101,95,115,101,103,109,101,110,116,
115,0,116,111,116,95,99,104,97,110,110,101,108,0,99,97,109,101,114,97,0,115,116,97,98,105,108,105,122,97,116,105,111,110,0,42,97,99,116,95,112,108,97,110,101,95,116,114,97,99,107,0,111,98,106,101,99,116,115,0,111,98,106,101,99,116,110,114,0,116,111,116,95,111,98,106,101,99,116,0,100,111,112,101,115,104,101,101,116,0,117,105,102,111,110,116,95,105,100,0,112,111,105,110,116,115,0,105,116,97,108,105,99,0,98,111,108,100,0,115,
104,97,100,111,119,0,115,104,97,100,120,0,115,104,97,100,121,0,115,104,97,100,111,119,97,108,112,104,97,0,115,104,97,100,111,119,99,111,108,111,114,0,99,104,97,114,97,99,116,101,114,95,119,101,105,103,104,116,0,112,97,110,101,108,116,105,116,108,101,0,103,114,111,117,112,108,97,98,101,108,0,119,105,100,103,101,116,108,97,98,101,108,0,119,105,100,103,101,116,0,112,97,110,101,108,122,111,111,109,0,109,105,110,108,97,98,101,108,99,104,
97,114,115,0,109,105,110,119,105,100,103,101,116,99,104,97,114,115,0,99,111,108,117,109,110,115,112,97,99,101,0,116,101,109,112,108,97,116,101,115,112,97,99,101,0,98,111,120,115,112,97,99,101,0,98,117,116,116,111,110,115,112,97,99,101,120,0,98,117,116,116,111,110,115,112,97,99,101,121,0,112,97,110,101,108,115,112,97,99,101,0,112,97,110,101,108,111,117,116,101,114,0,111,117,116,108,105,110,101,91,52,93,0,105,110,110,101,114,91,52,
93,0,105,110,110,101,114,95,115,101,108,91,52,93,0,105,116,101,109,91,52,93,0,116,101,120,116,91,52,93,0,116,101,120,116,95,115,101,108,91,52,93,0,115,104,97,100,101,100,0,115,104,97,100,101,116,111,112,0,115,104,97,100,101,100,111,119,110,0,114,111,117,110,100,110,101,115,115,0,105,110,110,101,114,95,97,110,105,109,91,52,93,0,105,110,110,101,114,95,97,110,105,109,95,115,101,108,91,52,93,0,105,110,110,101,114,95,107,101,121,
91,52,93,0,105,110,110,101,114,95,107,101,121,95,115,101,108,91,52,93,0,105,110,110,101,114,95,100,114,105,118,101,110,91,52,93,0,105,110,110,101,114,95,100,114,105,118,101,110,95,115,101,108,91,52,93,0,105,110,110,101,114,95,111,118,101,114,114,105,100,100,101,110,91,52,93,0,105,110,110,101,114,95,111,118,101,114,114,105,100,100,101,110,95,115,101,108,91,52,93,0,105,110,110,101,114,95,99,104,97,110,103,101,100,91,52,93,0,105,110,
110,101,114,95,99,104,97,110,103,101,100,95,115,101,108,91,52,93,0,104,101,97,100,101,114,91,52,93,0,98,97,99,107,91,52,93,0,115,117,98,95,98,97,99,107,91,52,93,0,119,99,111,108,95,114,101,103,117,108,97,114,0,119,99,111,108,95,116,111,111,108,0,119,99,111,108,95,116,111,111,108,98,97,114,95,105,116,101,109,0,119,99,111,108,95,116,101,120,116,0,119,99,111,108,95,114,97,100,105,111,0,119,99,111,108,95,111,112,116,105,
111,110,0,119,99,111,108,95,116,111,103,103,108,101,0,119,99,111,108,95,110,117,109,0,119,99,111,108,95,110,117,109,115,108,105,100,101,114,0,119,99,111,108,95,116,97,98,0,119,99,111,108,95,109,101,110,117,0,119,99,111,108,95,112,117,108,108,100,111,119,110,0,119,99,111,108,95,109,101,110,117,95,98,97,99,107,0,119,99,111,108,95,109,101,110,117,95,105,116,101,109,0,119,99,111,108,95,116,111,111,108,116,105,112,0,119,99,111,108,95,
98,111,120,0,119,99,111,108,95,115,99,114,111,108,108,0,119,99,111,108,95,112,114,111,103,114,101,115,115,0,119,99,111,108,95,108,105,115,116,95,105,116,101,109,0,119,99,111,108,95,112,105,101,95,109,101,110,117,0,119,99,111,108,95,115,116,97,116,101,0,119,105,100,103,101,116,95,101,109,98,111,115,115,91,52,93,0,109,101,110,117,95,115,104,97,100,111,119,95,102,97,99,0,109,101,110,117,95,115,104,97,100,111,119,95,119,105,100,116,104,
0,101,100,105,116,111,114,95,111,117,116,108,105,110,101,91,52,93,0,116,114,97,110,115,112,97,114,101,110,116,95,99,104,101,99,107,101,114,95,112,114,105,109,97,114,121,91,52,93,0,116,114,97,110,115,112,97,114,101,110,116,95,99,104,101,99,107,101,114,95,115,101,99,111,110,100,97,114,121,91,52,93,0,116,114,97,110,115,112,97,114,101,110,116,95,99,104,101,99,107,101,114,95,115,105,122,101,0,105,99,111,110,95,97,108,112,104,97,0,105,
99,111,110,95,115,97,116,117,114,97,116,105,111,110,0,119,105,100,103,101,116,95,116,101,120,116,95,99,117,114,115,111,114,91,52,93,0,120,97,120,105,115,91,52,93,0,121,97,120,105,115,91,52,93,0,122,97,120,105,115,91,52,93,0,103,105,122,109,111,95,104,105,91,52,93,0,103,105,122,109,111,95,112,114,105,109,97,114,121,91,52,93,0,103,105,122,109,111,95,115,101,99,111,110,100,97,114,121,91,52,93,0,103,105,122,109,111,95,118,105,
101,119,95,97,108,105,103,110,91,52,93,0,103,105,122,109,111,95,97,91,52,93,0,103,105,122,109,111,95,98,91,52,93,0,105,99,111,110,95,115,99,101,110,101,91,52,93,0,105,99,111,110,95,99,111,108,108,101,99,116,105,111,110,91,52,93,0,105,99,111,110,95,111,98,106,101,99,116,91,52,93,0,105,99,111,110,95,111,98,106,101,99,116,95,100,97,116,97,91,52,93,0,105,99,111,110,95,109,111,100,105,102,105,101,114,91,52,93,0,105,
99,111,110,95,115,104,97,100,105,110,103,91,52,93,0,105,99,111,110,95,102,111,108,100,101,114,91,52,93,0,105,99,111,110,95,98,111,114,100,101,114,95,105,110,116,101,110,115,105,116,121,0,112,97,110,101,108,95,114,111,117,110,100,110,101,115,115,0,104,101,97,100,101,114,95,98,97,99,107,91,52,93,0,98,97,99,107,95,103,114,97,100,91,52,93,0,115,104,111,119,95,98,97,99,107,95,103,114,97,100,0,116,105,116,108,101,91,52,93,0,
116,101,120,116,95,104,105,91,52,93,0,104,101,97,100,101,114,95,116,105,116,108,101,91,52,93,0,104,101,97,100,101,114,95,116,101,120,116,91,52,93,0,104,101,97,100,101,114,95,116,101,120,116,95,104,105,91,52,93,0,116,97,98,95,97,99,116,105,118,101,91,52,93,0,116,97,98,95,105,110,97,99,116,105,118,101,91,52,93,0,116,97,98,95,98,97,99,107,91,52,93,0,116,97,98,95,111,117,116,108,105,110,101,91,52,93,0,98,117,116,
116,111,110,91,52,93,0,98,117,116,116,111,110,95,116,105,116,108,101,91,52,93,0,98,117,116,116,111,110,95,116,101,120,116,91,52,93,0,98,117,116,116,111,110,95,116,101,120,116,95,104,105,91,52,93,0,108,105,115,116,91,52,93,0,108,105,115,116,95,116,105,116,108,101,91,52,93,0,108,105,115,116,95,116,101,120,116,91,52,93,0,108,105,115,116,95,116,101,120,116,95,104,105,91,52,93,0,110,97,118,105,103,97,116,105,111,110,95,98,97,
114,91,52,93,0,101,120,101,99,117,116,105,111,110,95,98,117,116,115,91,52,93,0,112,97,110,101,108,99,111,108,111,114,115,0,97,115,115,101,116,95,115,104,101,108,102,0,115,104,97,100,101,49,91,52,93,0,115,104,97,100,101,50,91,52,93,0,104,105,108,105,116,101,91,52,93,0,103,114,105,100,91,52,93,0,118,105,101,119,95,111,118,101,114,108,97,121,91,52,93,0,119,105,114,101,91,52,93,0,119,105,114,101,95,101,100,105,116,91,52,
93,0,115,101,108,101,99,116,91,52,93,0,108,97,109,112,91,52,93,0,115,112,101,97,107,101,114,91,52,93,0,101,109,112,116,121,91,52,93,0,99,97,109,101,114,97,91,52,93,0,97,99,116,105,118,101,91,52,93,0,103,114,111,117,112,91,52,93,0,103,114,111,117,112,95,97,99,116,105,118,101,91,52,93,0,116,114,97,110,115,102,111,114,109,91,52,93,0,118,101,114,116,101,120,91,52,93,0,118,101,114,116,101,120,95,115,101,108,101,99,
116,91,52,93,0,118,101,114,116,101,120,95,97,99,116,105,118,101,91,52,93,0,118,101,114,116,101,120,95,98,101,118,101,108,91,52,93,0,118,101,114,116,101,120,95,117,110,114,101,102,101,114,101,110,99,101,100,91,52,93,0,101,100,103,101,91,52,93,0,101,100,103,101,95,115,101,108,101,99,116,91,52,93,0,101,100,103,101,95,109,111,100,101,95,115,101,108,101,99,116,91,52,93,0,101,100,103,101,95,115,101,97,109,91,52,93,0,101,100,103,
101,95,115,104,97,114,112,91,52,93,0,101,100,103,101,95,102,97,99,101,115,101,108,91,52,93,0,101,100,103,101,95,99,114,101,97,115,101,91,52,93,0,101,100,103,101,95,98,101,118,101,108,91,52,93,0,102,97,99,101,91,52,93,0,102,97,99,101,95,115,101,108,101,99,116,91,52,93,0,102,97,99,101,95,109,111,100,101,95,115,101,108,101,99,116,91,52,93,0,102,97,99,101,95,114,101,116,111,112,111,108,111,103,121,91,52,93,0,102,97,
99,101,95,98,97,99,107,91,52,93,0,102,97,99,101,95,102,114,111,110,116,91,52,93,0,102,97,99,101,95,100,111,116,91,52,93,0,101,120,116,114,97,95,101,100,103,101,95,108,101,110,91,52,93,0,101,120,116,114,97,95,101,100,103,101,95,97,110,103,108,101,91,52,93,0,101,120,116,114,97,95,102,97,99,101,95,97,110,103,108,101,91,52,93,0,101,120,116,114,97,95,102,97,99,101,95,97,114,101,97,91,52,93,0,110,111,114,109,97,108,
91,52,93,0,118,101,114,116,101,120,95,110,111,114,109,97,108,91,52,93,0,108,111,111,112,95,110,111,114,109,97,108,91,52,93,0,98,111,110,101,95,115,111,108,105,100,91,52,93,0,98,111,110,101,95,112,111,115,101,91,52,93,0,98,111,110,101,95,112,111,115,101,95,97,99,116,105,118,101,91,52,93,0,98,111,110,101,95,108,111,99,107,101,100,95,119,101,105,103,104,116,91,52,93,0,115,116,114,105,112,91,52,93,0,115,116,114,105,112,95,
115,101,108,101,99,116,91,52,93,0,99,102,114,97,109,101,91,52,93,0,116,105,109,101,95,107,101,121,102,114,97,109,101,91,52,93,0,116,105,109,101,95,103,112,95,107,101,121,102,114,97,109,101,91,52,93,0,102,114,101,101,115,116,121,108,101,95,101,100,103,101,95,109,97,114,107,91,52,93,0,102,114,101,101,115,116,121,108,101,95,102,97,99,101,95,109,97,114,107,91,52,93,0,115,99,114,117,98,98,105,110,103,95,98,97,99,107,103,114,111,
117,110,100,91,52,93,0,116,105,109,101,95,109,97,114,107,101,114,95,108,105,110,101,91,52,93,0,116,105,109,101,95,109,97,114,107,101,114,95,108,105,110,101,95,115,101,108,101,99,116,101,100,91,52,93,0,110,117,114,98,95,117,108,105,110,101,91,52,93,0,110,117,114,98,95,118,108,105,110,101,91,52,93,0,97,99,116,95,115,112,108,105,110,101,91,52,93,0,110,117,114,98,95,115,101,108,95,117,108,105,110,101,91,52,93,0,110,117,114,98,
95,115,101,108,95,118,108,105,110,101,91,52,93,0,108,97,115,116,115,101,108,95,112,111,105,110,116,91,52,93,0,104,97,110,100,108,101,95,102,114,101,101,91,52,93,0,104,97,110,100,108,101,95,97,117,116,111,91,52,93,0,104,97,110,100,108,101,95,118,101,99,116,91,52,93,0,104,97,110,100,108,101,95,97,108,105,103,110,91,52,93,0,104,97,110,100,108,101,95,97,117,116,111,95,99,108,97,109,112,101,100,91,52,93,0,104,97,110,100,108,
101,95,115,101,108,95,102,114,101,101,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,97,117,116,111,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,118,101,99,116,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,97,108,105,103,110,91,52,93,0,104,97,110,100,108,101,95,115,101,108,95,97,117,116,111,95,99,108,97,109,112,101,100,91,52,93,0,100,115,95,99,104,97,110,110,101,108,91,52,93,0,100,115,95,115,117,98,99,
104,97,110,110,101,108,91,52,93,0,100,115,95,105,112,111,108,105,110,101,91,52,93,0,107,101,121,116,121,112,101,95,107,101,121,102,114,97,109,101,91,52,93,0,107,101,121,116,121,112,101,95,101,120,116,114,101,109,101,91,52,93,0,107,101,121,116,121,112,101,95,98,114,101,97,107,100,111,119,110,91,52,93,0,107,101,121,116,121,112,101,95,106,105,116,116,101,114,91,52,93,0,107,101,121,116,121,112,101,95,109,111,118,101,104,111,108,100,91,52,
93,0,107,101,121,116,121,112,101,95,103,101,110,101,114,97,116,101,100,91,52,93,0,107,101,121,116,121,112,101,95,107,101,121,102,114,97,109,101,95,115,101,108,101,99,116,91,52,93,0,107,101,121,116,121,112,101,95,101,120,116,114,101,109,101,95,115,101,108,101,99,116,91,52,93,0,107,101,121,116,121,112,101,95,98,114,101,97,107,100,111,119,110,95,115,101,108,101,99,116,91,52,93,0,107,101,121,116,121,112,101,95,106,105,116,116,101,114,95,115,
101,108,101,99,116,91,52,93,0,107,101,121,116,121,112,101,95,109,111,118,101,104,111,108,100,95,115,101,108,101,99,116,91,52,93,0,107,101,121,116,121,112,101,95,103,101,110,101,114,97,116,101,100,95,115,101,108,101,99,116,91,52,93,0,107,101,121,98,111,114,100,101,114,91,52,93,0,107,101,121,98,111,114,100,101,114,95,115,101,108,101,99,116,91,52,93,0,99,111,110,115,111,108,101,95,111,117,116,112,117,116,91,52,93,0,99,111,110,115,111,
108,101,95,105,110,112,117,116,91,52,93,0,99,111,110,115,111,108,101,95,105,110,102,111,91,52,93,0,99,111,110,115,111,108,101,95,101,114,114,111,114,91,52,93,0,99,111,110,115,111,108,101,95,99,117,114,115,111,114,91,52,93,0,99,111,110,115,111,108,101,95,115,101,108,101,99,116,91,52,93,0,118,101,114,116,101,120,95,115,105,122,101,0,101,100,103,101,95,119,105,100,116,104,0,111,98,99,101,110,116,101,114,95,100,105,97,0,102,97,99,
101,100,111,116,95,115,105,122,101,0,110,111,111,100,108,101,95,99,117,114,118,105,110,103,0,103,114,105,100,95,108,101,118,101,108,115,0,100,97,115,104,95,97,108,112,104,97,0,115,121,110,116,97,120,108,91,52,93,0,115,121,110,116,97,120,115,91,52,93,0,115,121,110,116,97,120,98,91,52,93,0,115,121,110,116,97,120,110,91,52,93,0,115,121,110,116,97,120,118,91,52,93,0,115,121,110,116,97,120,99,91,52,93,0,115,121,110,116,97,120,
100,91,52,93,0,115,121,110,116,97,120,114,91,52,93,0,108,105,110,101,95,110,117,109,98,101,114,115,91,52,93,0,95,112,97,100,54,91,51,93,0,110,111,100,101,99,108,97,115,115,95,111,117,116,112,117,116,91,52,93,0,110,111,100,101,99,108,97,115,115,95,102,105,108,116,101,114,91,52,93,0,110,111,100,101,99,108,97,115,115,95,118,101,99,116,111,114,91,52,93,0,110,111,100,101,99,108,97,115,115,95,116,101,120,116,117,114,101,91,52,
93,0,110,111,100,101,99,108,97,115,115,95,115,104,97,100,101,114,91,52,93,0,110,111,100,101,99,108,97,115,115,95,115,99,114,105,112,116,91,52,93,0,110,111,100,101,99,108,97,115,115,95,112,97,116,116,101,114,110,91,52,93,0,110,111,100,101,99,108,97,115,115,95,108,97,121,111,117,116,91,52,93,0,110,111,100,101,99,108,97,115,115,95,103,101,111,109,101,116,114,121,91,52,93,0,110,111,100,101,99,108,97,115,115,95,97,116,116,114,105,
98,117,116,101,91,52,93,0,110,111,100,101,95,122,111,110,101,95,115,105,109,117,108,97,116,105,111,110,91,52,93,0,110,111,100,101,95,122,111,110,101,95,114,101,112,101,97,116,91,52,93,0,115,105,109,117,108,97,116,101,100,95,102,114,97,109,101,115,91,52,93,0,109,111,118,105,101,91,52,93,0,109,111,118,105,101,99,108,105,112,91,52,93,0,109,97,115,107,91,52,93,0,105,109,97,103,101,91,52,93,0,115,99,101,110,101,91,52,93,0,
97,117,100,105,111,91,52,93,0,101,102,102,101,99,116,91,52,93,0,116,114,97,110,115,105,116,105,111,110,91,52,93,0,109,101,116,97,91,52,93,0,116,101,120,116,95,115,116,114,105,112,91,52,93,0,99,111,108,111,114,95,115,116,114,105,112,91,52,93,0,97,99,116,105,118,101,95,115,116,114,105,112,91,52,93,0,115,101,108,101,99,116,101,100,95,115,116,114,105,112,91,52,93,0,95,112,97,100,55,91,49,93,0,107,101,121,102,114,97,109,
101,95,115,99,97,108,101,95,102,97,99,0,101,100,105,116,109,101,115,104,95,97,99,116,105,118,101,91,52,93,0,104,97,110,100,108,101,95,118,101,114,116,101,120,91,52,93,0,104,97,110,100,108,101,95,118,101,114,116,101,120,95,115,101,108,101,99,116,91,52,93,0,104,97,110,100,108,101,95,118,101,114,116,101,120,95,115,105,122,101,0,99,108,105,112,112,105,110,103,95,98,111,114,100,101,114,95,51,100,91,52,93,0,109,97,114,107,101,114,95,
111,117,116,108,105,110,101,91,52,93,0,109,97,114,107,101,114,91,52,93,0,97,99,116,95,109,97,114,107,101,114,91,52,93,0,115,101,108,95,109,97,114,107,101,114,91,52,93,0,100,105,115,95,109,97,114,107,101,114,91,52,93,0,108,111,99,107,95,109,97,114,107,101,114,91,52,93,0,98,117,110,100,108,101,95,115,111,108,105,100,91,52,93,0,112,97,116,104,95,98,101,102,111,114,101,91,52,93,0,112,97,116,104,95,97,102,116,101,114,91,
52,93,0,112,97,116,104,95,107,101,121,102,114,97,109,101,95,98,101,102,111,114,101,91,52,93,0,112,97,116,104,95,107,101,121,102,114,97,109,101,95,97,102,116,101,114,91,52,93,0,99,97,109,101,114,97,95,112,97,116,104,91,52,93,0,99,97,109,101,114,97,95,112,97,115,115,101,112,97,114,116,111,117,116,91,52,93,0,103,112,95,118,101,114,116,101,120,95,115,105,122,101,0,103,112,95,118,101,114,116,101,120,91,52,93,0,103,112,95,118,
101,114,116,101,120,95,115,101,108,101,99,116,91,52,93,0,112,114,101,118,105,101,119,95,98,97,99,107,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,102,97,99,101,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,101,100,103,101,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,118,101,114,116,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,115,116,105,116,
99,104,97,98,108,101,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,117,110,115,116,105,116,99,104,97,98,108,101,91,52,93,0,112,114,101,118,105,101,119,95,115,116,105,116,99,104,95,97,99,116,105,118,101,91,52,93,0,117,118,95,115,104,97,100,111,119,91,52,93,0,109,97,116,99,104,91,52,93,0,115,101,108,101,99,116,101,100,95,104,105,103,104,108,105,103,104,116,91,52,93,0,115,101,108,101,99,116,101,100,95,111,
98,106,101,99,116,91,52,93,0,97,99,116,105,118,101,95,111,98,106,101,99,116,91,52,93,0,101,100,105,116,101,100,95,111,98,106,101,99,116,91,52,93,0,114,111,119,95,97,108,116,101,114,110,97,116,101,91,52,93,0,115,107,105,110,95,114,111,111,116,91,52,93,0,97,110,105,109,95,97,99,116,105,118,101,91,52,93,0,97,110,105,109,95,110,111,110,95,97,99,116,105,118,101,91,52,93,0,97,110,105,109,95,112,114,101,118,105,101,119,95,
114,97,110,103,101,91,52,93,0,110,108,97,95,116,119,101,97,107,105,110,103,91,52,93,0,110,108,97,95,116,119,101,97,107,100,117,112,108,105,91,52,93,0,110,108,97,95,116,114,97,99,107,91,52,93,0,110,108,97,95,116,114,97,110,115,105,116,105,111,110,91,52,93,0,110,108,97,95,116,114,97,110,115,105,116,105,111,110,95,115,101,108,91,52,93,0,110,108,97,95,109,101,116,97,91,52,93,0,110,108,97,95,109,101,116,97,95,115,101,108,
91,52,93,0,110,108,97,95,115,111,117,110,100,91,52,93,0,110,108,97,95,115,111,117,110,100,95,115,101,108,91,52,93,0,105,110,102,111,95,115,101,108,101,99,116,101,100,91,52,93,0,105,110,102,111,95,115,101,108,101,99,116,101,100,95,116,101,120,116,91,52,93,0,105,110,102,111,95,101,114,114,111,114,91,52,93,0,105,110,102,111,95,101,114,114,111,114,95,116,101,120,116,91,52,93,0,105,110,102,111,95,119,97,114,110,105,110,103,91,52,
93,0,105,110,102,111,95,119,97,114,110,105,110,103,95,116,101,120,116,91,52,93,0,105,110,102,111,95,105,110,102,111,91,52,93,0,105,110,102,111,95,105,110,102,111,95,116,101,120,116,91,52,93,0,105,110,102,111,95,100,101,98,117,103,91,52,93,0,105,110,102,111,95,100,101,98,117,103,95,116,101,120,116,91,52,93,0,105,110,102,111,95,112,114,111,112,101,114,116,121,91,52,93,0,105,110,102,111,95,112,114,111,112,101,114,116,121,95,116,101,
120,116,91,52,93,0,105,110,102,111,95,111,112,101,114,97,116,111,114,91,52,93,0,105,110,102,111,95,111,112,101,114,97,116,111,114,95,116,101,120,116,91,52,93,0,112,97,105,110,116,95,99,117,114,118,101,95,112,105,118,111,116,91,52,93,0,112,97,105,110,116,95,99,117,114,118,101,95,104,97,110,100,108,101,91,52,93,0,109,101,116,97,100,97,116,97,98,103,91,52,93,0,109,101,116,97,100,97,116,97,116,101,120,116,91,52,93,0,115,111,
108,105,100,91,52,93,0,116,117,105,0,116,98,117,116,115,0,116,118,51,100,0,116,102,105,108,101,0,116,105,112,111,0,116,105,110,102,111,0,116,97,99,116,0,116,110,108,97,0,116,115,101,113,0,116,105,109,97,0,116,101,120,116,0,116,111,111,112,115,0,116,110,111,100,101,0,116,117,115,101,114,112,114,101,102,0,116,99,111,110,115,111,108,101,0,116,99,108,105,112,0,116,116,111,112,98,97,114,0,116,115,116,97,116,117,115,98,97,114,0,
115,112,97,99,101,95,115,112,114,101,97,100,115,104,101,101,116,0,116,97,114,109,91,50,48,93,0,99,111,108,108,101,99,116,105,111,110,95,99,111,108,111,114,91,56,93,0,115,116,114,105,112,95,99,111,108,111,114,91,57,93,0,97,99,116,105,118,101,95,116,104,101,109,101,95,97,114,101,97,0,109,111,100,117,108,101,91,49,50,56,93,0,112,97,116,104,91,55,54,56,93,0,115,112,97,99,101,95,116,121,112,101,0,99,111,110,116,101,120,116,
91,54,52,93,0,105,116,101,109,115,0,117,105,95,110,97,109,101,91,54,52,93,0,111,112,95,105,100,110,97,109,101,91,54,52,93,0,111,112,95,112,114,111,112,95,101,110,117,109,91,54,52,93,0,111,112,99,111,110,116,101,120,116,0,109,116,95,105,100,110,97,109,101,91,54,52,93,0,99,111,110,116,101,120,116,95,100,97,116,97,95,112,97,116,104,91,50,53,54,93,0,112,114,111,112,95,105,100,91,54,52,93,0,112,114,111,112,95,105,110,
100,101,120,0,105,109,112,111,114,116,95,109,101,116,104,111,100,0,109,111,100,117,108,101,91,52,56,93,0,42,97,99,99,101,115,115,95,116,111,107,101,110,0,99,117,115,116,111,109,95,100,105,114,112,97,116,104,91,49,48,50,52,93,0,114,101,109,111,116,101,95,117,114,108,91,49,48,50,52,93,0,115,109,111,111,116,104,0,115,112,101,99,91,52,93,0,109,111,117,115,101,95,115,112,101,101,100,0,119,97,108,107,95,115,112,101,101,100,0,119,
97,108,107,95,115,112,101,101,100,95,102,97,99,116,111,114,0,118,105,101,119,95,104,101,105,103,104,116,0,106,117,109,112,95,104,101,105,103,104,116,0,116,101,108,101,112,111,114,116,95,116,105,109,101,0,105,115,95,100,105,114,116,121,0,115,101,99,116,105,111,110,95,97,99,116,105,118,101,0,100,105,115,112,108,97,121,95,116,121,112,101,0,115,111,114,116,95,116,121,112,101,0,116,101,109,112,95,119,105,110,95,115,105,122,101,120,0,116,101,109,
112,95,119,105,110,95,115,105,122,101,121,0,117,115,101,95,117,110,100,111,95,108,101,103,97,99,121,0,110,111,95,111,118,101,114,114,105,100,101,95,97,117,116,111,95,114,101,115,121,110,99,0,117,115,101,95,99,121,99,108,101,115,95,100,101,98,117,103,0,117,115,101,95,101,101,118,101,101,95,100,101,98,117,103,0,115,104,111,119,95,97,115,115,101,116,95,100,101,98,117,103,95,105,110,102,111,0,110,111,95,97,115,115,101,116,95,105,110,100,101,
120,105,110,103,0,117,115,101,95,118,105,101,119,112,111,114,116,95,100,101,98,117,103,0,117,115,101,95,97,108,108,95,108,105,110,107,101,100,95,100,97,116,97,95,100,105,114,101,99,116,0,117,115,101,95,101,120,116,101,110,115,105,111,110,115,95,100,101,98,117,103,0,117,115,101,95,114,101,99,111,109,112,117,116,101,95,117,115,101,114,99,111,117,110,116,95,111,110,95,115,97,118,101,95,100,101,98,117,103,0,83,65,78,73,84,73,90,69,95,65,
70,84,69,82,95,72,69,82,69,0,117,115,101,95,110,101,119,95,99,117,114,118,101,115,95,116,111,111,108,115,0,117,115,101,95,110,101,119,95,112,111,105,110,116,95,99,108,111,117,100,95,116,121,112,101,0,117,115,101,95,115,99,117,108,112,116,95,116,111,111,108,115,95,116,105,108,116,0,117,115,101,95,101,120,116,101,110,100,101,100,95,97,115,115,101,116,95,98,114,111,119,115,101,114,0,117,115,101,95,115,99,117,108,112,116,95,116,101,120,116,
117,114,101,95,112,97,105,110,116,0,117,115,101,95,103,114,101,97,115,101,95,112,101,110,99,105,108,95,118,101,114,115,105,111,110,51,0,101,110,97,98,108,101,95,111,118,101,114,108,97,121,95,110,101,120,116,0,117,115,101,95,110,101,119,95,118,111,108,117,109,101,95,110,111,100,101,115,0,117,115,101,95,115,104,97,100,101,114,95,110,111,100,101,95,112,114,101,118,105,101,119,115,0,117,115,101,95,103,114,101,97,115,101,95,112,101,110,99,105,108,
95,118,101,114,115,105,111,110,51,95,99,111,110,118,101,114,116,95,111,110,95,108,111,97,100,0,117,115,101,95,97,110,105,109,97,116,105,111,110,95,98,97,107,108,97,118,97,0,100,105,114,95,112,97,116,104,91,55,54,56,93,0,115,104,101,108,102,95,105,100,110,97,109,101,91,54,52,93,0,100,117,112,102,108,97,103,0,112,114,101,102,95,102,108,97,103,0,115,97,118,101,116,105,109,101,0,109,111,117,115,101,95,101,109,117,108,97,116,101,95,
51,95,98,117,116,116,111,110,95,109,111,100,105,102,105,101,114,0,116,114,97,99,107,112,97,100,95,115,99,114,111,108,108,95,100,105,114,101,99,116,105,111,110,0,116,101,109,112,100,105,114,91,55,54,56,93,0,102,111,110,116,100,105,114,91,55,54,56,93,0,114,101,110,100,101,114,100,105,114,91,49,48,50,52,93,0,114,101,110,100,101,114,95,99,97,99,104,101,100,105,114,91,55,54,56,93,0,116,101,120,116,117,100,105,114,91,55,54,56,93,
0,112,121,116,104,111,110,100,105,114,91,55,54,56,93,0,115,111,117,110,100,100,105,114,91,55,54,56,93,0,105,49,56,110,100,105,114,91,55,54,56,93,0,105,109,97,103,101,95,101,100,105,116,111,114,91,49,48,50,52,93,0,116,101,120,116,95,101,100,105,116,111,114,91,49,48,50,52,93,0,116,101,120,116,95,101,100,105,116,111,114,95,97,114,103,115,91,50,53,54,93,0,97,110,105,109,95,112,108,97,121,101,114,91,49,48,50,52,93,0,
97,110,105,109,95,112,108,97,121,101,114,95,112,114,101,115,101,116,0,118,50,100,95,109,105,110,95,103,114,105,100,115,105,122,101,0,116,105,109,101,99,111,100,101,95,115,116,121,108,101,0,118,101,114,115,105,111,110,115,0,100,98,108,95,99,108,105,99,107,95,116,105,109,101,0,109,105,110,105,95,97,120,105,115,95,116,121,112,101,0,117,105,102,108,97,103,0,117,105,102,108,97,103,50,0,103,112,117,95,102,108,97,103,0,95,112,97,100,56,91,
54,93,0,97,112,112,95,102,108,97,103,0,118,105,101,119,122,111,111,109,0,108,97,110,103,117,97,103,101,0,109,105,120,98,117,102,115,105,122,101,0,97,117,100,105,111,100,101,118,105,99,101,0,97,117,100,105,111,114,97,116,101,0,97,117,100,105,111,102,111,114,109,97,116,0,97,117,100,105,111,99,104,97,110,110,101,108,115,0,117,105,95,115,99,97,108,101,0,117,105,95,108,105,110,101,95,119,105,100,116,104,0,100,112,105,0,115,99,97,108,
101,95,102,97,99,116,111,114,0,105,110,118,95,115,99,97,108,101,95,102,97,99,116,111,114,0,112,105,120,101,108,115,105,122,101,0,118,105,114,116,117,97,108,95,112,105,120,101,108,0,110,111,100,101,95,109,97,114,103,105,110,0,110,111,100,101,95,112,114,101,118,105,101,119,95,114,101,115,0,116,114,97,110,115,111,112,116,115,0,109,101,110,117,116,104,114,101,115,104,111,108,100,49,0,109,101,110,117,116,104,114,101,115,104,111,108,100,50,0,97,
112,112,95,116,101,109,112,108,97,116,101,91,54,52,93,0,116,104,101,109,101,115,0,117,105,102,111,110,116,115,0,117,105,115,116,121,108,101,115,0,117,115,101,114,95,107,101,121,109,97,112,115,0,117,115,101,114,95,107,101,121,99,111,110,102,105,103,95,112,114,101,102,115,0,97,100,100,111,110,115,0,97,117,116,111,101,120,101,99,95,112,97,116,104,115,0,115,99,114,105,112,116,95,100,105,114,101,99,116,111,114,105,101,115,0,117,115,101,114,95,
109,101,110,117,115,0,97,115,115,101,116,95,108,105,98,114,97,114,105,101,115,0,101,120,116,101,110,115,105,111,110,95,114,101,112,111,115,0,97,115,115,101,116,95,115,104,101,108,118,101,115,95,115,101,116,116,105,110,103,115,0,107,101,121,99,111,110,102,105,103,115,116,114,91,54,52,93,0,97,99,116,105,118,101,95,97,115,115,101,116,95,108,105,98,114,97,114,121,0,97,99,116,105,118,101,95,101,120,116,101,110,115,105,111,110,95,114,101,112,111,
0,101,120,116,101,110,115,105,111,110,95,102,108,97,103,0,110,101,116,119,111,114,107,95,116,105,109,101,111,117,116,0,110,101,116,119,111,114,107,95,99,111,110,110,101,99,116,105,111,110,95,108,105,109,105,116,0,95,112,97,100,49,52,91,51,93,0,117,110,100,111,115,116,101,112,115,0,117,110,100,111,109,101,109,111,114,121,0,103,112,117,95,118,105,101,119,112,111,114,116,95,113,117,97,108,105,116,121,0,103,112,95,109,97,110,104,97,116,116,101,
110,100,105,115,116,0,103,112,95,101,117,99,108,105,100,101,97,110,100,105,115,116,0,103,112,95,101,114,97,115,101,114,0,103,112,95,115,101,116,116,105,110,103,115,0,95,112,97,100,49,51,91,52,93,0,108,105,103,104,116,95,112,97,114,97,109,91,52,93,0,108,105,103,104,116,95,97,109,98,105,101,110,116,91,51,93,0,103,105,122,109,111,95,115,105,122,101,0,103,105,122,109,111,95,115,105,122,101,95,110,97,118,105,103,97,116,101,95,118,51,
100,0,95,112,97,100,51,91,53,93,0,101,100,105,116,95,115,116,117,100,105,111,95,108,105,103,104,116,0,108,111,111,107,100,101,118,95,115,112,104,101,114,101,95,115,105,122,101,0,118,98,111,116,105,109,101,111,117,116,0,118,98,111,99,111,108,108,101,99,116,114,97,116,101,0,116,101,120,116,105,109,101,111,117,116,0,116,101,120,99,111,108,108,101,99,116,114,97,116,101,0,109,101,109,99,97,99,104,101,108,105,109,105,116,0,112,114,101,102,101,
116,99,104,102,114,97,109,101,115,0,112,97,100,95,114,111,116,95,97,110,103,108,101,0,114,118,105,115,105,122,101,0,114,118,105,98,114,105,103,104,116,0,114,101,99,101,110,116,95,102,105,108,101,115,0,115,109,111,111,116,104,95,118,105,101,119,116,120,0,103,108,114,101,115,108,105,109,105,116,0,99,111,108,111,114,95,112,105,99,107,101,114,95,116,121,112,101,0,97,117,116,111,95,115,109,111,111,116,104,105,110,103,95,110,101,119,0,105,112,111,
95,110,101,119,0,107,101,121,104,97,110,100,108,101,115,95,110,101,119,0,95,112,97,100,49,49,91,52,93,0,118,105,101,119,95,102,114,97,109,101,95,116,121,112,101,0,118,105,101,119,95,102,114,97,109,101,95,107,101,121,102,114,97,109,101,115,0,118,105,101,119,95,102,114,97,109,101,95,115,101,99,111,110,100,115,0,103,112,117,95,98,97,99,107,101,110,100,0,109,97,120,95,115,104,97,100,101,114,95,99,111,109,112,105,108,97,116,105,111,110,
95,115,117,98,112,114,111,99,101,115,115,101,115,0,112,108,97,121,98,97,99,107,95,102,112,115,95,115,97,109,112,108,101,115,0,119,105,100,103,101,116,95,117,110,105,116,0,97,110,105,115,111,116,114,111,112,105,99,95,102,105,108,116,101,114,0,116,97,98,108,101,116,95,97,112,105,0,112,114,101,115,115,117,114,101,95,116,104,114,101,115,104,111,108,100,95,109,97,120,0,112,114,101,115,115,117,114,101,95,115,111,102,116,110,101,115,115,0,110,100,
111,102,95,115,101,110,115,105,116,105,118,105,116,121,0,110,100,111,102,95,111,114,98,105,116,95,115,101,110,115,105,116,105,118,105,116,121,0,110,100,111,102,95,100,101,97,100,122,111,110,101,0,110,100,111,102,95,102,108,97,103,0,111,103,108,95,109,117,108,116,105,115,97,109,112,108,101,115,0,105,109,97,103,101,95,100,114,97,119,95,109,101,116,104,111,100,0,103,108,97,108,112,104,97,99,108,105,112,0,97,117,116,111,107,101,121,95,102,108,97,
103,0,107,101,121,95,105,110,115,101,114,116,95,99,104,97,110,110,101,108,115,0,95,112,97,100,49,53,91,54,93,0,97,110,105,109,97,116,105,111,110,95,102,108,97,103,0,116,101,120,116,95,114,101,110,100,101,114,0,110,97,118,105,103,97,116,105,111,110,95,109,111,100,101,0,118,105,101,119,95,114,111,116,97,116,101,95,115,101,110,115,105,116,105,118,105,116,121,95,116,117,114,110,116,97,98,108,101,0,118,105,101,119,95,114,111,116,97,116,101,
95,115,101,110,115,105,116,105,118,105,116,121,95,116,114,97,99,107,98,97,108,108,0,99,111,98,97,95,119,101,105,103,104,116,0,115,99,117,108,112,116,95,112,97,105,110,116,95,111,118,101,114,108,97,121,95,99,111,108,91,51,93,0,103,112,101,110,99,105,108,95,110,101,119,95,108,97,121,101,114,95,99,111,108,91,52,93,0,100,114,97,103,95,116,104,114,101,115,104,111,108,100,95,109,111,117,115,101,0,100,114,97,103,95,116,104,114,101,115,104,
111,108,100,95,116,97,98,108,101,116,0,100,114,97,103,95,116,104,114,101,115,104,111,108,100,0,109,111,118,101,95,116,104,114,101,115,104,111,108,100,0,102,111,110,116,95,112,97,116,104,95,117,105,91,49,48,50,52,93,0,102,111,110,116,95,112,97,116,104,95,117,105,95,109,111,110,111,91,49,48,50,52,93,0,99,111,109,112,117,116,101,95,100,101,118,105,99,101,95,116,121,112,101,0,102,99,117,95,105,110,97,99,116,105,118,101,95,97,108,112,
104,97,0,112,105,101,95,116,97,112,95,116,105,109,101,111,117,116,0,112,105,101,95,105,110,105,116,105,97,108,95,116,105,109,101,111,117,116,0,112,105,101,95,97,110,105,109,97,116,105,111,110,95,116,105,109,101,111,117,116,0,112,105,101,95,109,101,110,117,95,99,111,110,102,105,114,109,0,112,105,101,95,109,101,110,117,95,114,97,100,105,117,115,0,112,105,101,95,109,101,110,117,95,116,104,114,101,115,104,111,108,100,0,115,101,113,117,101,110,99,
101,114,95,101,100,105,116,111,114,95,102,108,97,103,0,102,97,99,116,111,114,95,100,105,115,112,108,97,121,95,116,121,112,101,0,114,101,110,100,101,114,95,100,105,115,112,108,97,121,95,116,121,112,101,0,102,105,108,101,98,114,111,119,115,101,114,95,100,105,115,112,108,97,121,95,116,121,112,101,0,115,101,113,117,101,110,99,101,114,95,100,105,115,107,95,99,97,99,104,101,95,100,105,114,91,49,48,50,52,93,0,115,101,113,117,101,110,99,101,114,
95,100,105,115,107,95,99,97,99,104,101,95,99,111,109,112,114,101,115,115,105,111,110,0,115,101,113,117,101,110,99,101,114,95,100,105,115,107,95,99,97,99,104,101,95,115,105,122,101,95,108,105,109,105,116,0,115,101,113,117,101,110,99,101,114,95,100,105,115,107,95,99,97,99,104,101,95,102,108,97,103,0,115,101,113,117,101,110,99,101,114,95,112,114,111,120,121,95,115,101,116,117,112,0,99,111,108,108,101,99,116,105,111,110,95,105,110,115,116,97,
110,99,101,95,101,109,112,116,121,95,115,105,122,101,0,116,101,120,116,95,102,108,97,103,0,95,112,97,100,49,48,91,49,93,0,102,105,108,101,95,112,114,101,118,105,101,119,95,116,121,112,101,0,115,116,97,116,117,115,98,97,114,95,102,108,97,103,0,119,97,108,107,95,110,97,118,105,103,97,116,105,111,110,0,115,112,97,99,101,95,100,97,116,97,0,102,105,108,101,95,115,112,97,99,101,95,100,97,116,97,0,101,120,112,101,114,105,109,101,110,
116,97,108,0,116,105,109,101,95,108,111,119,0,116,105,109,101,95,109,105,100,0,116,105,109,101,95,104,105,95,97,110,100,95,118,101,114,115,105,111,110,0,99,108,111,99,107,95,115,101,113,95,104,105,95,97,110,100,95,114,101,115,101,114,118,101,100,0,99,108,111,99,107,95,115,101,113,95,108,111,119,0,110,111,100,101,91,54,93,0,118,97,108,117,101,91,52,93,91,52,93,0,116,114,97,110,115,91,52,93,0,115,99,97,108,101,91,52,93,91,
52,93,0,115,99,97,108,101,95,119,101,105,103,104,116,0,42,116,101,109,112,95,112,102,0,118,101,114,116,0,104,111,114,0,109,97,115,107,0,109,105,110,91,50,93,0,109,97,120,91,50,93,0,109,105,110,122,111,111,109,0,109,97,120,122,111,111,109,0,115,99,114,111,108,108,0,115,99,114,111,108,108,95,117,105,0,107,101,101,112,116,111,116,0,107,101,101,112,122,111,111,109,0,107,101,101,112,111,102,115,0,111,108,100,119,105,110,120,0,111,
108,100,119,105,110,121,0,97,108,112,104,97,95,118,101,114,116,0,97,108,112,104,97,95,104,111,114,0,112,97,103,101,95,115,105,122,101,95,121,0,42,115,109,115,0,42,115,109,111,111,116,104,95,116,105,109,101,114,0,119,105,110,109,97,116,91,52,93,91,52,93,0,118,105,101,119,109,97,116,91,52,93,91,52,93,0,118,105,101,119,105,110,118,91,52,93,91,52,93,0,112,101,114,115,109,97,116,91,52,93,91,52,93,0,112,101,114,115,105,110,
118,91,52,93,91,52,93,0,118,105,101,119,99,97,109,116,101,120,99,111,102,97,99,91,52,93,0,118,105,101,119,109,97,116,111,98,91,52,93,91,52,93,0,112,101,114,115,109,97,116,111,98,91,52,93,91,52,93,0,99,108,105,112,91,54,93,91,52,93,0,99,108,105,112,95,108,111,99,97,108,91,54,93,91,52,93,0,42,99,108,105,112,98,98,0,42,108,111,99,97,108,118,100,0,42,118,105,101,119,95,114,101,110,100,101,114,0,116,119,109,
97,116,91,52,93,91,52,93,0,116,119,95,97,120,105,115,95,109,105,110,91,51,93,0,116,119,95,97,120,105,115,95,109,97,120,91,51,93,0,116,119,95,97,120,105,115,95,109,97,116,114,105,120,91,51,93,91,51,93,0,103,114,105,100,118,105,101,119,0,118,105,101,119,113,117,97,116,91,52,93,0,99,97,109,100,120,0,99,97,109,100,121,0,112,105,120,115,105,122,101,0,99,97,109,122,111,111,109,0,105,115,95,112,101,114,115,112,0,112,101,
114,115,112,0,118,105,101,119,95,97,120,105,115,95,114,111,108,108,0,118,105,101,119,108,111,99,107,0,114,117,110,116,105,109,101,95,118,105,101,119,108,111,99,107,0,118,105,101,119,108,111,99,107,95,113,117,97,100,0,111,102,115,95,108,111,99,107,91,50,93,0,116,119,100,114,97,119,102,108,97,103,0,114,102,108,97,103,0,108,118,105,101,119,113,117,97,116,91,52,93,0,108,112,101,114,115,112,0,108,118,105,101,119,0,108,118,105,101,119,95,
97,120,105,115,95,114,111,108,108,0,95,112,97,100,56,91,49,93,0,114,111,116,95,97,110,103,108,101,0,114,111,116,95,97,120,105,115,91,51,93,0,114,111,116,97,116,105,111,110,95,113,117,97,116,101,114,110,105,111,110,91,52,93,0,114,111,116,97,116,105,111,110,95,97,120,105,115,91,51,93,0,114,111,116,97,116,105,111,110,95,97,110,103,108,101,0,112,114,101,118,95,116,121,112,101,0,112,114,101,118,95,116,121,112,101,95,119,105,114,101,
0,99,111,108,111,114,95,116,121,112,101,0,108,105,103,104,116,0,98,97,99,107,103,114,111,117,110,100,95,116,121,112,101,0,99,97,118,105,116,121,95,116,121,112,101,0,119,105,114,101,95,99,111,108,111,114,95,116,121,112,101,0,117,115,101,95,99,111,109,112,111,115,105,116,111,114,0,115,116,117,100,105,111,95,108,105,103,104,116,91,50,53,54,93,0,108,111,111,107,100,101,118,95,108,105,103,104,116,91,50,53,54,93,0,109,97,116,99,97,112,
91,50,53,54,93,0,115,104,97,100,111,119,95,105,110,116,101,110,115,105,116,121,0,115,105,110,103,108,101,95,99,111,108,111,114,91,51,93,0,115,116,117,100,105,111,108,105,103,104,116,95,114,111,116,95,122,0,115,116,117,100,105,111,108,105,103,104,116,95,98,97,99,107,103,114,111,117,110,100,0,115,116,117,100,105,111,108,105,103,104,116,95,105,110,116,101,110,115,105,116,121,0,115,116,117,100,105,111,108,105,103,104,116,95,98,108,117,114,0,111,
98,106,101,99,116,95,111,117,116,108,105,110,101,95,99,111,108,111,114,91,51,93,0,120,114,97,121,95,97,108,112,104,97,0,120,114,97,121,95,97,108,112,104,97,95,119,105,114,101,0,99,97,118,105,116,121,95,118,97,108,108,101,121,95,102,97,99,116,111,114,0,99,97,118,105,116,121,95,114,105,100,103,101,95,102,97,99,116,111,114,0,98,97,99,107,103,114,111,117,110,100,95,99,111,108,111,114,91,51,93,0,99,117,114,118,97,116,117,114,101,
95,114,105,100,103,101,95,102,97,99,116,111,114,0,99,117,114,118,97,116,117,114,101,95,118,97,108,108,101,121,95,102,97,99,116,111,114,0,114,101,110,100,101,114,95,112,97,115,115,0,97,111,118,95,110,97,109,101,91,54,52,93,0,101,100,105,116,95,102,108,97,103,0,110,111,114,109,97,108,115,95,108,101,110,103,116,104,0,110,111,114,109,97,108,115,95,99,111,110,115,116,97,110,116,95,115,99,114,101,101,110,95,115,105,122,101,0,112,97,105,
110,116,95,102,108,97,103,0,119,112,97,105,110,116,95,102,108,97,103,0,116,101,120,116,117,114,101,95,112,97,105,110,116,95,109,111,100,101,95,111,112,97,99,105,116,121,0,118,101,114,116,101,120,95,112,97,105,110,116,95,109,111,100,101,95,111,112,97,99,105,116,121,0,119,101,105,103,104,116,95,112,97,105,110,116,95,109,111,100,101,95,111,112,97,99,105,116,121,0,115,99,117,108,112,116,95,109,111,100,101,95,109,97,115,107,95,111,112,97,99,
105,116,121,0,115,99,117,108,112,116,95,109,111,100,101,95,102,97,99,101,95,115,101,116,115,95,111,112,97,99,105,116,121,0,118,105,101,119,101,114,95,97,116,116,114,105,98,117,116,101,95,111,112,97,99,105,116,121,0,120,114,97,121,95,97,108,112,104,97,95,98,111,110,101,0,98,111,110,101,95,119,105,114,101,95,97,108,112,104,97,0,102,97,100,101,95,97,108,112,104,97,0,119,105,114,101,102,114,97,109,101,95,116,104,114,101,115,104,111,108,
100,0,119,105,114,101,102,114,97,109,101,95,111,112,97,99,105,116,121,0,114,101,116,111,112,111,108,111,103,121,95,111,102,102,115,101,116,0,103,112,101,110,99,105,108,95,112,97,112,101,114,95,111,112,97,99,105,116,121,0,103,112,101,110,99,105,108,95,103,114,105,100,95,111,112,97,99,105,116,121,0,103,112,101,110,99,105,108,95,102,97,100,101,95,108,97,121,101,114,0,103,112,101,110,99,105,108,95,118,101,114,116,101,120,95,112,97,105,110,116,
95,111,112,97,99,105,116,121,0,104,97,110,100,108,101,95,100,105,115,112,108,97,121,0,115,99,117,108,112,116,95,99,117,114,118,101,115,95,99,97,103,101,95,111,112,97,99,105,116,121,0,42,112,114,111,112,101,114,116,105,101,115,95,115,116,111,114,97,103,101,0,40,42,112,114,111,112,101,114,116,105,101,115,95,115,116,111,114,97,103,101,95,102,114,101,101,41,40,41,0,42,108,111,99,97,108,95,115,116,97,116,115,0,98,117,110,100,108,101,95,
115,105,122,101,0,98,117,110,100,108,101,95,100,114,97,119,116,121,112,101,0,111,98,106,101,99,116,95,116,121,112,101,95,101,120,99,108,117,100,101,95,118,105,101,119,112,111,114,116,0,111,98,106,101,99,116,95,116,121,112,101,95,101,120,99,108,117,100,101,95,115,101,108,101,99,116,0,42,111,98,95,99,101,110,116,114,101,0,114,101,110,100,101,114,95,98,111,114,100,101,114,0,111,98,95,99,101,110,116,114,101,95,98,111,110,101,91,54,52,93,
0,108,111,99,97,108,95,118,105,101,119,95,117,117,105,100,0,108,111,99,97,108,95,99,111,108,108,101,99,116,105,111,110,115,95,117,117,105,100,0,100,101,98,117,103,95,102,108,97,103,0,111,98,95,99,101,110,116,114,101,95,99,117,114,115,111,114,0,115,99,101,110,101,108,111,99,107,0,103,112,95,102,108,97,103,0,110,101,97,114,0,102,97,114,0,103,105,122,109,111,95,115,104,111,119,95,111,98,106,101,99,116,0,103,105,122,109,111,95,115,
104,111,119,95,97,114,109,97,116,117,114,101,0,103,105,122,109,111,95,115,104,111,119,95,101,109,112,116,121,0,103,105,122,109,111,95,115,104,111,119,95,108,105,103,104,116,0,103,105,122,109,111,95,115,104,111,119,95,99,97,109,101,114,97,0,103,114,105,100,102,108,97,103,0,103,114,105,100,108,105,110,101,115,0,103,114,105,100,115,117,98,100,105,118,0,118,101,114,116,101,120,95,111,112,97,99,105,116,121,0,115,116,101,114,101,111,51,100,95,102,
108,97,103,0,115,116,101,114,101,111,51,100,95,99,97,109,101,114,97,0,115,116,101,114,101,111,51,100,95,99,111,110,118,101,114,103,101,110,99,101,95,102,97,99,116,111,114,0,115,116,101,114,101,111,51,100,95,118,111,108,117,109,101,95,97,108,112,104,97,0,115,116,101,114,101,111,51,100,95,99,111,110,118,101,114,103,101,110,99,101,95,97,108,112,104,97,0,42,117,105,95,110,97,109,101,0,42,109,111,100,105,102,105,101,114,95,110,97,109,101,
0,115,105,109,95,111,117,116,112,117,116,95,110,111,100,101,95,105,100,0,114,101,112,101,97,116,95,111,117,116,112,117,116,95,110,111,100,101,95,105,100,0,105,116,101,114,97,116,105,111,110,0,119,105,114,101,102,114,97,109,101,95,116,121,112,101,0,119,105,114,101,102,114,97,109,101,95,100,101,116,97,105,108,0,105,110,116,101,114,112,111,108,97,116,105,111,110,95,109,101,116,104,111,100,0,115,101,113,117,101,110,99,101,95,109,111,100,101,0,102,
114,97,109,101,95,100,117,114,97,116,105,111,110,0,97,99,116,105,118,101,95,103,114,105,100,0,114,101,110,100,101,114,0,118,101,108,111,99,105,116,121,95,103,114,105,100,91,54,52,93,0,108,105,115,116,0,112,114,105,110,116,108,101,118,101,108,0,115,116,111,114,101,108,101,118,101,108,0,42,114,101,112,111,114,116,116,105,109,101,114,0,42,108,111,99,107,0,115,101,115,115,105,111,110,95,115,101,116,116,105,110,103,115,0,42,119,105,110,100,114,
97,119,97,98,108,101,0,42,119,105,110,97,99,116,105,118,101,0,119,105,110,100,111,119,115,0,105,110,105,116,95,102,108,97,103,0,102,105,108,101,95,115,97,118,101,100,0,111,112,95,117,110,100,111,95,100,101,112,116,104,0,111,117,116,108,105,110,101,114,95,115,121,110,99,95,115,101,108,101,99,116,95,100,105,114,116,121,0,111,112,101,114,97,116,111,114,115,0,110,111,116,105,102,105,101,114,95,113,117,101,117,101,0,42,110,111,116,105,102,105,
101,114,95,113,117,101,117,101,95,115,101,116,0,101,120,116,101,110,115,105,111,110,115,95,117,112,100,97,116,101,115,0,95,112,97,100,51,0,106,111,98,115,0,112,97,105,110,116,99,117,114,115,111,114,115,0,100,114,97,103,115,0,107,101,121,99,111,110,102,105,103,115,0,42,100,101,102,97,117,108,116,99,111,110,102,0,42,97,100,100,111,110,99,111,110,102,0,42,117,115,101,114,99,111,110,102,0,116,105,109,101,114,115,0,42,97,117,116,111,115,
97,118,101,116,105,109,101,114,0,97,117,116,111,115,97,118,101,95,115,99,104,101,100,117,108,101,100,0,42,117,110,100,111,95,115,116,97,99,107,0,42,109,101,115,115,97,103,101,95,98,117,115,0,120,114,0,42,103,104,111,115,116,119,105,110,0,42,103,112,117,99,116,120,0,42,110,101,119,95,115,99,101,110,101,0,118,105,101,119,95,108,97,121,101,114,95,110,97,109,101,91,54,52,93,0,42,117,110,112,105,110,110,101,100,95,115,99,101,110,101,
0,42,119,111,114,107,115,112,97,99,101,95,104,111,111,107,0,103,108,111,98,97,108,95,97,114,101,97,95,109,97,112,0,42,115,99,114,101,101,110,0,112,111,115,120,0,112,111,115,121,0,119,105,110,100,111,119,115,116,97,116,101,0,108,97,115,116,99,117,114,115,111,114,0,109,111,100,97,108,99,117,114,115,111,114,0,103,114,97,98,99,117,114,115,111,114,0,112,105,101,95,101,118,101,110,116,95,116,121,112,101,95,108,111,99,107,0,112,105,101,
95,101,118,101,110,116,95,116,121,112,101,95,108,97,115,116,0,97,100,100,109,111,117,115,101,109,111,118,101,0,116,97,103,95,99,117,114,115,111,114,95,114,101,102,114,101,115,104,0,101,118,101,110,116,95,113,117,101,117,101,95,99,104,101,99,107,95,99,108,105,99,107,0,101,118,101,110,116,95,113,117,101,117,101,95,99,104,101,99,107,95,100,114,97,103,0,101,118,101,110,116,95,113,117,101,117,101,95,99,104,101,99,107,95,100,114,97,103,95,104,
97,110,100,108,101,100,0,101,118,101,110,116,95,113,117,101,117,101,95,99,111,110,115,101,99,117,116,105,118,101,95,103,101,115,116,117,114,101,95,116,121,112,101,0,101,118,101,110,116,95,113,117,101,117,101,95,99,111,110,115,101,99,117,116,105,118,101,95,103,101,115,116,117,114,101,95,120,121,91,50,93,0,42,101,118,101,110,116,95,113,117,101,117,101,95,99,111,110,115,101,99,117,116,105,118,101,95,103,101,115,116,117,114,101,95,100,97,116,97,0,
42,101,118,101,110,116,115,116,97,116,101,0,42,101,118,101,110,116,95,108,97,115,116,95,104,97,110,100,108,101,100,0,42,105,109,101,95,100,97,116,97,0,105,109,101,95,100,97,116,97,95,105,115,95,99,111,109,112,111,115,105,110,103,0,101,118,101,110,116,95,113,117,101,117,101,0,109,111,100,97,108,104,97,110,100,108,101,114,115,0,103,101,115,116,117,114,101,0,100,114,97,119,99,97,108,108,115,0,42,99,117,114,115,111,114,95,107,101,121,109,
97,112,95,115,116,97,116,117,115,0,101,118,101,110,116,115,116,97,116,101,95,112,114,101,118,95,112,114,101,115,115,95,116,105,109,101,95,109,115,0,112,114,111,112,118,97,108,117,101,95,115,116,114,91,54,52,93,0,112,114,111,112,118,97,108,117,101,0,115,104,105,102,116,0,99,116,114,108,0,97,108,116,0,111,115,107,101,121,0,107,101,121,109,111,100,105,102,105,101,114,0,109,97,112,116,121,112,101,0,42,112,116,114,0,42,114,101,109,111,118,
101,95,105,116,101,109,0,42,97,100,100,95,105,116,101,109,0,100,105,102,102,95,105,116,101,109,115,0,115,112,97,99,101,105,100,0,114,101,103,105,111,110,105,100,0,111,119,110,101,114,95,105,100,91,54,52,93,0,107,109,105,95,105,100,0,40,42,112,111,108,108,41,40,41,0,40,42,112,111,108,108,95,109,111,100,97,108,95,105,116,101,109,41,40,41,0,42,109,111,100,97,108,95,105,116,101,109,115,0,98,97,115,101,110,97,109,101,91,54,52,
93,0,107,101,121,109,97,112,115,0,97,99,116,107,101,121,109,97,112,0,42,99,117,115,116,111,109,100,97,116,97,0,42,114,101,112,111,114,116,115,0,109,97,99,114,111,0,42,111,112,109,0,105,100,110,97,109,101,95,102,97,108,108,98,97,99,107,91,54,52,93,0,108,97,121,111,117,116,115,0,104,111,111,107,95,108,97,121,111,117,116,95,114,101,108,97,116,105,111,110,115,0,111,119,110,101,114,95,105,100,115,0,116,111,111,108,115,0,42,112,
105,110,95,115,99,101,110,101,0,111,98,106,101,99,116,95,109,111,100,101,0,111,114,100,101,114,0,112,97,114,101,110,116,105,100,0,95,112,97,100,95,48,91,52,93,0,42,97,99,116,105,118,101,0,42,97,99,116,95,108,97,121,111,117,116,0,42,116,101,109,112,95,119,111,114,107,115,112,97,99,101,95,115,116,111,114,101,0,42,116,101,109,112,95,108,97,121,111,117,116,95,115,116,111,114,101,0,109,105,115,116,121,112,101,0,104,111,114,114,0,
104,111,114,103,0,104,111,114,98,0,101,120,112,0,109,105,115,105,0,109,105,115,116,115,116,97,0,109,105,115,116,100,105,115,116,0,109,105,115,116,104,105,0,97,111,100,105,115,116,0,97,111,101,110,101,114,103,121,0,112,114,111,98,101,95,114,101,115,111,108,117,116,105,111,110,0,115,117,110,95,116,104,114,101,115,104,111,108,100,0,115,117,110,95,115,104,97,100,111,119,95,109,97,120,105,109,117,109,95,114,101,115,111,108,117,116,105,111,110,0,
115,117,110,95,115,104,97,100,111,119,95,106,105,116,116,101,114,95,111,118,101,114,98,108,117,114,0,115,117,110,95,115,104,97,100,111,119,95,102,105,108,116,101,114,95,114,97,100,105,117,115,0,108,97,115,116,95,117,112,100,97,116,101,0,98,97,115,101,95,115,99,97,108,101,0,98,97,115,101,95,112,111,115,101,95,116,121,112,101,0,42,98,97,115,101,95,112,111,115,101,95,111,98,106,101,99,116,0,98,97,115,101,95,112,111,115,101,95,108,111,
99,97,116,105,111,110,91,51,93,0,98,97,115,101,95,112,111,115,101,95,97,110,103,108,101,0,100,114,97,119,95,102,108,97,103,115,0,99,111,110,116,114,111,108,108,101,114,95,100,114,97,119,95,115,116,121,108,101,0,99,108,105,112,95,115,116,97,114,116,0,99,108,105,112,95,101,110,100,0,112,97,116,104,91,49,57,50,93,0,112,114,111,102,105,108,101,91,50,53,54,93,0,99,111,109,112,111,110,101,110,116,95,112,97,116,104,115,0,102,108,
111,97,116,95,116,104,114,101,115,104,111,108,100,0,97,120,105,115,95,102,108,97,103,0,112,111,115,101,95,108,111,99,97,116,105,111,110,91,51,93,0,112,111,115,101,95,114,111,116,97,116,105,111,110,91,51,93,0,112,97,116,104,91,54,52,93,0,117,115,101,114,95,112,97,116,104,115,0,111,112,91,54,52,93,0,42,111,112,95,112,114,111,112,101,114,116,105,101,115,0,42,111,112,95,112,114,111,112,101,114,116,105,101,115,95,112,116,114,0,111,
112,95,102,108,97,103,0,97,99,116,105,111,110,95,102,108,97,103,0,104,97,112,116,105,99,95,102,108,97,103,0,112,111,115,101,95,102,108,97,103,0,104,97,112,116,105,99,95,110,97,109,101,91,54,52,93,0,104,97,112,116,105,99,95,100,117,114,97,116,105,111,110,0,104,97,112,116,105,99,95,102,114,101,113,117,101,110,99,121,0,104,97,112,116,105,99,95,97,109,112,108,105,116,117,100,101,0,115,101,108,98,105,110,100,105,110,103,0,98,105,
110,100,105,110,103,115,0,115,101,108,105,116,101,109,0,0,84,89,80,69,30,4,0,0,99,104,97,114,0,117,99,104,97,114,0,115,104,111,114,116,0,117,115,104,111,114,116,0,105,110,116,0,108,111,110,103,0,117,108,111,110,103,0,102,108,111,97,116,0,100,111,117,98,108,101,0,105,110,116,54,52,95,116,0,117,105,110,116,54,52,95,116,0,118,111,105,100,0,105,110,116,56,95,116,0,68,114,97,119,68,97,116,97,76,105,115,116,0,68,114,
97,119,68,97,116,97,0,73,68,80,114,111,112,101,114,116,121,85,73,68,97,116,97,0,73,68,80,114,111,112,101,114,116,121,85,73,68,97,116,97,69,110,117,109,73,116,101,109,0,73,68,80,114,111,112,101,114,116,121,85,73,68,97,116,97,73,110,116,0,73,68,80,114,111,112,101,114,116,121,85,73,68,97,116,97,66,111,111,108,0,73,68,80,114,111,112,101,114,116,121,85,73,68,97,116,97,70,108,111,97,116,0,73,68,80,114,111,112,101,114,
116,121,85,73,68,97,116,97,83,116,114,105,110,103,0,73,68,80,114,111,112,101,114,116,121,85,73,68,97,116,97,73,68,0,73,68,80,114,111,112,101,114,116,121,68,97,116,97,0,76,105,115,116,66,97,115,101,0,73,68,80,114,111,112,101,114,116,121,0,73,68,79,118,101,114,114,105,100,101,76,105,98,114,97,114,121,80,114,111,112,101,114,116,121,79,112,101,114,97,116,105,111,110,0,73,68,0,73,68,79,118,101,114,114,105,100,101,76,105,98,
114,97,114,121,80,114,111,112,101,114,116,121,0,73,68,79,118,101,114,114,105,100,101,76,105,98,114,97,114,121,0,73,68,79,118,101,114,114,105,100,101,76,105,98,114,97,114,121,82,117,110,116,105,109,101,0,73,68,95,82,117,110,116,105,109,101,95,82,101,109,97,112,0,73,68,95,82,117,110,116,105,109,101,0,68,101,112,115,103,114,97,112,104,0,76,105,98,114,97,114,121,0,65,115,115,101,116,77,101,116,97,68,97,116,97,0,76,105,98,114,
97,114,121,87,101,97,107,82,101,102,101,114,101,110,99,101,0,76,105,98,114,97,114,121,95,82,117,110,116,105,109,101,0,85,110,105,113,117,101,78,97,109,101,95,77,97,112,0,70,105,108,101,68,97,116,97,0,80,97,99,107,101,100,70,105,108,101,0,80,114,101,118,105,101,119,73,109,97,103,101,0,80,114,101,118,105,101,119,73,109,97,103,101,82,117,110,116,105,109,101,72,97,110,100,108,101,0,98,77,111,116,105,111,110,80,97,116,104,86,101,
114,116,0,98,77,111,116,105,111,110,80,97,116,104,0,71,80,85,86,101,114,116,66,117,102,72,97,110,100,108,101,0,71,80,85,66,97,116,99,104,72,97,110,100,108,101,0,98,65,110,105,109,86,105,122,83,101,116,116,105,110,103,115,0,98,80,111,115,101,67,104,97,110,110,101,108,95,66,66,111,110,101,83,101,103,109,101,110,116,66,111,117,110,100,97,114,121,0,98,80,111,115,101,67,104,97,110,110,101,108,95,82,117,110,116,105,109,101,0,83,
101,115,115,105,111,110,85,73,68,0,68,117,97,108,81,117,97,116,0,77,97,116,52,0,98,80,111,115,101,67,104,97,110,110,101,108,0,66,111,110,101,0,79,98,106,101,99,116,0,98,80,111,115,101,67,104,97,110,110,101,108,68,114,97,119,68,97,116,97,0,66,111,110,101,67,111,108,111,114,0,98,80,111,115,101,0,71,72,97,115,104,0,98,73,75,80,97,114,97,109,0,98,73,116,97,115,99,0,98,65,99,116,105,111,110,71,114,111,117,112,
0,84,104,101,109,101,87,105,114,101,67,111,108,111,114,0,98,65,99,116,105,111,110,0,65,99,116,105,111,110,76,97,121,101,114,0,65,99,116,105,111,110,66,105,110,100,105,110,103,0,98,68,111,112,101,83,104,101,101,116,0,67,111,108,108,101,99,116,105,111,110,0,83,112,97,99,101,65,99,116,105,111,110,95,82,117,110,116,105,109,101,0,83,112,97,99,101,65,99,116,105,111,110,0,83,112,97,99,101,76,105,110,107,0,86,105,101,119,50,68,
0,98,65,99,116,105,111,110,67,104,97,110,110,101,108,0,73,112,111,0,65,99,116,105,111,110,83,116,114,105,112,0,75,101,121,102,114,97,109,101,65,99,116,105,111,110,83,116,114,105,112,0,65,99,116,105,111,110,67,104,97,110,110,101,108,66,97,103,0,70,67,117,114,118,101,0,70,77,111,100,105,102,105,101,114,0,70,77,111,100,95,71,101,110,101,114,97,116,111,114,0,70,77,111,100,95,70,117,110,99,116,105,111,110,71,101,110,101,114,97,
116,111,114,0,70,67,77,95,69,110,118,101,108,111,112,101,68,97,116,97,0,70,77,111,100,95,69,110,118,101,108,111,112,101,0,70,77,111,100,95,67,121,99,108,101,115,0,70,77,111,100,95,80,121,116,104,111,110,0,84,101,120,116,0,70,77,111,100,95,76,105,109,105,116,115,0,114,99,116,102,0,70,77,111,100,95,78,111,105,115,101,0,70,77,111,100,95,83,116,101,112,112,101,100,0,68,114,105,118,101,114,84,97,114,103,101,116,0,68,114,
105,118,101,114,86,97,114,0,67,104,97,110,110,101,108,68,114,105,118,101,114,0,69,120,112,114,80,121,76,105,107,101,95,80,97,114,115,101,100,0,70,80,111,105,110,116,0,66,101,122,84,114,105,112,108,101,0,78,108,97,83,116,114,105,112,0,78,108,97,84,114,97,99,107,0,75,83,95,80,97,116,104,0,75,101,121,105,110,103,83,101,116,0,65,110,105,109,79,118,101,114,114,105,100,101,0,65,110,105,109,68,97,116,97,0,73,100,65,100,116,
84,101,109,112,108,97,116,101,0,66,111,110,101,95,82,117,110,116,105,109,101,0,98,65,114,109,97,116,117,114,101,95,82,117,110,116,105,109,101,0,66,111,110,101,67,111,108,108,101,99,116,105,111,110,0,98,65,114,109,97,116,117,114,101,0,69,100,105,116,66,111,110,101,0,66,111,110,101,67,111,108,108,101,99,116,105,111,110,77,101,109,98,101,114,0,66,111,110,101,67,111,108,108,101,99,116,105,111,110,82,101,102,101,114,101,110,99,101,0,65,
115,115,101,116,84,97,103,0,65,115,115,101,116,84,121,112,101,73,110,102,111,0,98,85,85,73,68,0,65,115,115,101,116,76,105,98,114,97,114,121,82,101,102,101,114,101,110,99,101,0,65,115,115,101,116,87,101,97,107,82,101,102,101,114,101,110,99,101,0,65,115,115,101,116,67,97,116,97,108,111,103,80,97,116,104,76,105,110,107,0,66,111,105,100,82,117,108,101,0,66,111,105,100,82,117,108,101,71,111,97,108,65,118,111,105,100,0,66,111,105,
100,82,117,108,101,65,118,111,105,100,67,111,108,108,105,115,105,111,110,0,66,111,105,100,82,117,108,101,70,111,108,108,111,119,76,101,97,100,101,114,0,66,111,105,100,82,117,108,101,65,118,101,114,97,103,101,83,112,101,101,100,0,66,111,105,100,82,117,108,101,70,105,103,104,116,0,66,111,105,100,68,97,116,97,0,66,111,105,100,83,116,97,116,101,0,66,111,105,100,83,101,116,116,105,110,103,115,0,66,114,117,115,104,67,108,111,110,101,0,73,
109,97,103,101,0,66,114,117,115,104,71,112,101,110,99,105,108,83,101,116,116,105,110,103,115,0,67,117,114,118,101,77,97,112,112,105,110,103,0,77,97,116,101,114,105,97,108,0,66,114,117,115,104,67,117,114,118,101,115,83,99,117,108,112,116,83,101,116,116,105,110,103,115,0,66,114,117,115,104,0,77,84,101,120,0,73,109,66,117,102,0,67,111,108,111,114,66,97,110,100,0,80,97,105,110,116,67,117,114,118,101,0,116,80,97,108,101,116,116,101,
67,111,108,111,114,72,83,86,0,80,97,108,101,116,116,101,67,111,108,111,114,0,80,97,108,101,116,116,101,0,80,97,105,110,116,67,117,114,118,101,80,111,105,110,116,0,67,97,99,104,101,79,98,106,101,99,116,80,97,116,104,0,67,97,99,104,101,70,105,108,101,76,97,121,101,114,0,67,97,99,104,101,70,105,108,101,0,67,97,99,104,101,65,114,99,104,105,118,101,72,97,110,100,108,101,0,71,83,101,116,0,67,97,109,101,114,97,83,116,101,
114,101,111,83,101,116,116,105,110,103,115,0,67,97,109,101,114,97,66,71,73,109,97,103,101,0,73,109,97,103,101,85,115,101,114,0,77,111,118,105,101,67,108,105,112,0,77,111,118,105,101,67,108,105,112,85,115,101,114,0,67,97,109,101,114,97,68,79,70,83,101,116,116,105,110,103,115,0,67,97,109,101,114,97,95,82,117,110,116,105,109,101,0,67,97,109,101,114,97,0,71,80,85,68,79,70,83,101,116,116,105,110,103,115,0,67,108,111,116,104,
83,105,109,83,101,116,116,105,110,103,115,0,76,105,110,107,78,111,100,101,0,69,102,102,101,99,116,111,114,87,101,105,103,104,116,115,0,67,108,111,116,104,67,111,108,108,83,101,116,116,105,110,103,115,0,67,111,108,108,101,99,116,105,111,110,76,105,103,104,116,76,105,110,107,105,110,103,0,67,111,108,108,101,99,116,105,111,110,79,98,106,101,99,116,0,67,111,108,108,101,99,116,105,111,110,67,104,105,108,100,0,67,111,108,108,101,99,116,105,111,
110,69,120,112,111,114,116,0,67,111,108,108,101,99,116,105,111,110,95,82,117,110,116,105,109,101,0,86,105,101,119,76,97,121,101,114,0,67,117,114,118,101,77,97,112,80,111,105,110,116,0,67,117,114,118,101,77,97,112,0,72,105,115,116,111,103,114,97,109,0,83,99,111,112,101,115,0,67,111,108,111,114,77,97,110,97,103,101,100,86,105,101,119,83,101,116,116,105,110,103,115,0,67,111,108,111,114,77,97,110,97,103,101,100,68,105,115,112,108,97,
121,83,101,116,116,105,110,103,115,0,67,111,108,111,114,77,97,110,97,103,101,100,67,111,108,111,114,115,112,97,99,101,83,101,116,116,105,110,103,115,0,98,67,111,110,115,116,114,97,105,110,116,67,104,97,110,110,101,108,0,98,67,111,110,115,116,114,97,105,110,116,0,98,67,111,110,115,116,114,97,105,110,116,84,97,114,103,101,116,0,98,80,121,116,104,111,110,67,111,110,115,116,114,97,105,110,116,0,98,75,105,110,101,109,97,116,105,99,67,111,
110,115,116,114,97,105,110,116,0,98,83,112,108,105,110,101,73,75,67,111,110,115,116,114,97,105,110,116,0,98,65,114,109,97,116,117,114,101,67,111,110,115,116,114,97,105,110,116,0,98,84,114,97,99,107,84,111,67,111,110,115,116,114,97,105,110,116,0,98,82,111,116,97,116,101,76,105,107,101,67,111,110,115,116,114,97,105,110,116,0,98,76,111,99,97,116,101,76,105,107,101,67,111,110,115,116,114,97,105,110,116,0,98,83,105,122,101,76,105,107,
101,67,111,110,115,116,114,97,105,110,116,0,98,83,97,109,101,86,111,108,117,109,101,67,111,110,115,116,114,97,105,110,116,0,98,84,114,97,110,115,76,105,107,101,67,111,110,115,116,114,97,105,110,116,0,98,77,105,110,77,97,120,67,111,110,115,116,114,97,105,110,116,0,98,65,99,116,105,111,110,67,111,110,115,116,114,97,105,110,116,0,98,76,111,99,107,84,114,97,99,107,67,111,110,115,116,114,97,105,110,116,0,98,68,97,109,112,84,114,97,
99,107,67,111,110,115,116,114,97,105,110,116,0,98,70,111,108,108,111,119,80,97,116,104,67,111,110,115,116,114,97,105,110,116,0,98,83,116,114,101,116,99,104,84,111,67,111,110,115,116,114,97,105,110,116,0,98,82,105,103,105,100,66,111,100,121,74,111,105,110,116,67,111,110,115,116,114,97,105,110,116,0,98,67,108,97,109,112,84,111,67,111,110,115,116,114,97,105,110,116,0,98,67,104,105,108,100,79,102,67,111,110,115,116,114,97,105,110,116,0,
98,84,114,97,110,115,102,111,114,109,67,111,110,115,116,114,97,105,110,116,0,98,80,105,118,111,116,67,111,110,115,116,114,97,105,110,116,0,98,76,111,99,76,105,109,105,116,67,111,110,115,116,114,97,105,110,116,0,98,82,111,116,76,105,109,105,116,67,111,110,115,116,114,97,105,110,116,0,98,83,105,122,101,76,105,109,105,116,67,111,110,115,116,114,97,105,110,116,0,98,68,105,115,116,76,105,109,105,116,67,111,110,115,116,114,97,105,110,116,0,
98,83,104,114,105,110,107,119,114,97,112,67,111,110,115,116,114,97,105,110,116,0,98,70,111,108,108,111,119,84,114,97,99,107,67,111,110,115,116,114,97,105,110,116,0,98,67,97,109,101,114,97,83,111,108,118,101,114,67,111,110,115,116,114,97,105,110,116,0,98,79,98,106,101,99,116,83,111,108,118,101,114,67,111,110,115,116,114,97,105,110,116,0,98,84,114,97,110,115,102,111,114,109,67,97,99,104,101,67,111,110,115,116,114,97,105,110,116,0,67,
97,99,104,101,82,101,97,100,101,114,0,66,80,111,105,110,116,0,78,117,114,98,0,67,104,97,114,73,110,102,111,0,84,101,120,116,66,111,120,0,67,117,114,118,101,0,69,100,105,116,78,117,114,98,0,75,101,121,0,67,117,114,118,101,80,114,111,102,105,108,101,0,69,100,105,116,70,111,110,116,0,86,70,111,110,116,0,67,117,114,118,101,115,0,67,117,114,118,101,80,114,111,102,105,108,101,80,111,105,110,116,0,67,117,114,118,101,115,71,101,
111,109,101,116,114,121,0,67,117,115,116,111,109,68,97,116,97,0,67,117,114,118,101,115,71,101,111,109,101,116,114,121,82,117,110,116,105,109,101,72,97,110,100,108,101,0,67,117,115,116,111,109,68,97,116,97,76,97,121,101,114,0,65,110,111,110,121,109,111,117,115,65,116,116,114,105,98,117,116,101,73,68,72,97,110,100,108,101,0,73,109,112,108,105,99,105,116,83,104,97,114,105,110,103,73,110,102,111,72,97,110,100,108,101,0,67,117,115,116,111,
109,68,97,116,97,69,120,116,101,114,110,97,108,0,66,76,73,95,109,101,109,112,111,111,108,0,67,117,115,116,111,109,68,97,116,97,95,77,101,115,104,77,97,115,107,115,0,68,121,110,97,109,105,99,80,97,105,110,116,83,117,114,102,97,99,101,0,68,121,110,97,109,105,99,80,97,105,110,116,67,97,110,118,97,115,83,101,116,116,105,110,103,115,0,80,97,105,110,116,83,117,114,102,97,99,101,68,97,116,97,0,80,111,105,110,116,67,97,99,104,
101,0,84,101,120,0,68,121,110,97,109,105,99,80,97,105,110,116,77,111,100,105,102,105,101,114,68,97,116,97,0,68,121,110,97,109,105,99,80,97,105,110,116,66,114,117,115,104,83,101,116,116,105,110,103,115,0,80,97,114,116,105,99,108,101,83,121,115,116,101,109,0,69,102,102,101,99,116,0,66,117,105,108,100,69,102,102,0,80,97,114,116,69,102,102,0,80,97,114,116,105,99,108,101,0,87,97,118,101,69,102,102,0,70,105,108,101,71,108,111,
98,97,108,0,98,83,99,114,101,101,110,0,83,99,101,110,101,0,70,108,117,105,100,68,111,109,97,105,110,83,101,116,116,105,110,103,115,0,70,108,117,105,100,77,111,100,105,102,105,101,114,68,97,116,97,0,77,65,78,84,65,0,71,80,85,84,101,120,116,117,114,101,0,70,108,117,105,100,70,108,111,119,83,101,116,116,105,110,103,115,0,77,101,115,104,0,70,108,117,105,100,69,102,102,101,99,116,111,114,83,101,116,116,105,110,103,115,0,70,114,
101,101,115,116,121,108,101,76,105,110,101,83,101,116,0,70,114,101,101,115,116,121,108,101,76,105,110,101,83,116,121,108,101,0,70,114,101,101,115,116,121,108,101,77,111,100,117,108,101,67,111,110,102,105,103,0,70,114,101,101,115,116,121,108,101,67,111,110,102,105,103,0,98,71,80,68,99,111,110,116,114,111,108,112,111,105,110,116,0,98,71,80,68,115,112,111,105,110,116,95,82,117,110,116,105,109,101,0,98,71,80,68,115,112,111,105,110,116,0,98,
71,80,68,116,114,105,97,110,103,108,101,0,98,71,80,68,112,97,108,101,116,116,101,99,111,108,111,114,0,98,71,80,68,112,97,108,101,116,116,101,0,98,71,80,68,99,117,114,118,101,95,112,111,105,110,116,0,98,71,80,68,99,117,114,118,101,0,98,71,80,68,115,116,114,111,107,101,95,82,117,110,116,105,109,101,0,98,71,80,68,115,116,114,111,107,101,0,77,68,101,102,111,114,109,86,101,114,116,0,98,71,80,68,102,114,97,109,101,95,82,
117,110,116,105,109,101,0,98,71,80,68,102,114,97,109,101,0,98,71,80,68,108,97,121,101,114,95,77,97,115,107,0,98,71,80,68,108,97,121,101,114,95,82,117,110,116,105,109,101,0,98,71,80,68,108,97,121,101,114,0,98,71,80,100,97,116,97,95,82,117,110,116,105,109,101,0,71,112,101,110,99,105,108,66,97,116,99,104,67,97,99,104,101,0,76,105,110,101,97,114,116,67,97,99,104,101,0,71,80,101,110,99,105,108,85,112,100,97,116,101,
67,97,99,104,101,0,98,71,80,103,114,105,100,0,98,71,80,100,97,116,97,0,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,78,111,105,115,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,83,117,98,100,105,118,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,84,104,105,99,107,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,84,105,
109,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,83,101,103,109,101,110,116,0,84,105,109,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,67,111,108,111,114,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,79,112,97,99,105,116,121,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,79,117,116,108,105,110,101,71,112,101,110,99,105,108,77,111,100,105,102,
105,101,114,68,97,116,97,0,65,114,114,97,121,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,66,117,105,108,100,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,76,97,116,116,105,99,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,76,97,116,116,105,99,101,68,101,102,111,114,109,68,97,116,97,0,76,101,110,103,116,104,71,112,101,110,99,105,108,77,111,100,105,
102,105,101,114,68,97,116,97,0,68,97,115,104,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,83,101,103,109,101,110,116,0,68,97,115,104,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,77,105,114,114,111,114,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,72,111,111,107,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,83,105,109,112,108,105,102,121,71,112,
101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,79,102,102,115,101,116,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,83,109,111,111,116,104,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,65,114,109,97,116,117,114,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,77,117,108,116,105,112,108,121,71,112,101,110,99,105,108,77,111,100,105,102,105,101,
114,68,97,116,97,0,84,105,110,116,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,84,101,120,116,117,114,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,80,114,111,120,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,65,110,103,108,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,76,105,
110,101,97,114,116,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,76,105,110,101,97,114,116,68,97,116,97,0,83,104,114,105,110,107,119,114,97,112,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,83,104,114,105,110,107,119,114,97,112,84,114,101,101,68,97,116,97,0,69,110,118,101,108,111,112,101,71,112,101,110,99,105,108,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,
80,101,110,99,105,108,68,114,97,119,105,110,103,66,97,115,101,0,71,114,101,97,115,101,80,101,110,99,105,108,68,114,97,119,105,110,103,0,71,114,101,97,115,101,80,101,110,99,105,108,68,114,97,119,105,110,103,82,117,110,116,105,109,101,72,97,110,100,108,101,0,71,114,101,97,115,101,80,101,110,99,105,108,68,114,97,119,105,110,103,82,101,102,101,114,101,110,99,101,0,71,114,101,97,115,101,80,101,110,99,105,108,0,71,114,101,97,115,101,80,
101,110,99,105,108,70,114,97,109,101,0,71,114,101,97,115,101,80,101,110,99,105,108,76,97,121,101,114,70,114,97,109,101,115,77,97,112,83,116,111,114,97,103,101,0,71,114,101,97,115,101,80,101,110,99,105,108,76,97,121,101,114,77,97,115,107,0,71,114,101,97,115,101,80,101,110,99,105,108,76,97,121,101,114,84,114,101,101,78,111,100,101,0,71,114,101,97,115,101,80,101,110,99,105,108,76,97,121,101,114,84,114,101,101,71,114,111,117,112,0,
71,114,101,97,115,101,80,101,110,99,105,108,76,97,121,101,114,0,71,114,101,97,115,101,80,101,110,99,105,108,76,97,121,101,114,82,117,110,116,105,109,101,72,97,110,100,108,101,0,71,114,101,97,115,101,80,101,110,99,105,108,76,97,121,101,114,71,114,111,117,112,82,117,110,116,105,109,101,72,97,110,100,108,101,0,71,114,101,97,115,101,80,101,110,99,105,108,79,110,105,111,110,83,107,105,110,110,105,110,103,83,101,116,116,105,110,103,115,0,71,
114,101,97,115,101,80,101,110,99,105,108,82,117,110,116,105,109,101,72,97,110,100,108,101,0,73,109,97,103,101,65,110,105,109,0,73,109,66,117,102,65,110,105,109,0,73,109,97,103,101,86,105,101,119,0,73,109,97,103,101,80,97,99,107,101,100,70,105,108,101,0,82,101,110,100,101,114,83,108,111,116,0,82,101,110,100,101,114,82,101,115,117,108,116,0,73,109,97,103,101,84,105,108,101,95,82,117,110,116,105,109,101,0,73,109,97,103,101,84,105,
108,101,0,73,109,97,103,101,95,82,117,110,116,105,109,101,0,80,97,114,116,105,97,108,85,112,100,97,116,101,82,101,103,105,115,116,101,114,0,80,97,114,116,105,97,108,85,112,100,97,116,101,85,115,101,114,0,77,111,118,105,101,67,97,99,104,101,0,83,116,101,114,101,111,51,100,70,111,114,109,97,116,0,73,112,111,68,114,105,118,101,114,0,73,112,111,67,117,114,118,101,0,75,101,121,66,108,111,99,107,0,76,97,116,116,105,99,101,0,69,
100,105,116,76,97,116,116,0,66,97,115,101,0,86,105,101,119,76,97,121,101,114,69,110,103,105,110,101,68,97,116,97,0,68,114,97,119,69,110,103,105,110,101,84,121,112,101,0,76,97,121,101,114,67,111,108,108,101,99,116,105,111,110,0,86,105,101,119,76,97,121,101,114,69,69,86,69,69,0,86,105,101,119,76,97,121,101,114,65,79,86,0,86,105,101,119,76,97,121,101,114,76,105,103,104,116,103,114,111,117,112,0,76,105,103,104,116,103,114,111,
117,112,77,101,109,98,101,114,115,104,105,112,0,83,99,101,110,101,83,116,97,116,115,0,87,111,114,108,100,0,76,97,109,112,0,98,78,111,100,101,84,114,101,101,0,76,105,103,104,116,80,114,111,98,101,0,76,105,103,104,116,80,114,111,98,101,67,97,99,104,101,0,76,105,103,104,116,71,114,105,100,67,97,99,104,101,0,76,105,103,104,116,67,97,99,104,101,84,101,120,116,117,114,101,0,76,105,103,104,116,67,97,99,104,101,0,76,105,103,104,
116,80,114,111,98,101,66,97,107,105,110,103,68,97,116,97,0,76,105,103,104,116,80,114,111,98,101,73,114,114,97,100,105,97,110,99,101,68,97,116,97,0,76,105,103,104,116,80,114,111,98,101,86,105,115,105,98,105,108,105,116,121,68,97,116,97,0,76,105,103,104,116,80,114,111,98,101,67,111,110,110,101,99,116,105,118,105,116,121,68,97,116,97,0,76,105,103,104,116,80,114,111,98,101,66,108,111,99,107,68,97,116,97,0,76,105,103,104,116,80,
114,111,98,101,71,114,105,100,67,97,99,104,101,70,114,97,109,101,0,76,105,103,104,116,80,114,111,98,101,79,98,106,101,99,116,67,97,99,104,101,0,76,105,110,101,83,116,121,108,101,77,111,100,105,102,105,101,114,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,65,108,111,110,103,83,116,114,111,107,101,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,65,108,111,
110,103,83,116,114,111,107,101,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,65,108,111,110,103,83,116,114,111,107,101,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,67,97,109,101,114,97,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,
101,70,114,111,109,67,97,109,101,114,97,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,67,97,109,101,114,97,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,79,98,106,101,99,116,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,
101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,79,98,106,101,99,116,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,68,105,115,116,97,110,99,101,70,114,111,109,79,98,106,101,99,116,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,67,117,114,118,97,116,117,114,101,95,51,68,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,
100,105,102,105,101,114,95,67,117,114,118,97,116,117,114,101,95,51,68,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,67,117,114,118,97,116,117,114,101,95,51,68,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,78,111,105,115,101,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,78,111,105,115,101,0,76,105,
110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,78,111,105,115,101,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,67,114,101,97,115,101,65,110,103,108,101,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,67,114,101,97,115,101,65,110,103,108,101,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,
111,100,105,102,105,101,114,95,67,114,101,97,115,101,65,110,103,108,101,0,76,105,110,101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,84,97,110,103,101,110,116,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,84,97,110,103,101,110,116,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,84,97,110,103,101,110,116,0,76,105,110,
101,83,116,121,108,101,67,111,108,111,114,77,111,100,105,102,105,101,114,95,77,97,116,101,114,105,97,108,0,76,105,110,101,83,116,121,108,101,65,108,112,104,97,77,111,100,105,102,105,101,114,95,77,97,116,101,114,105,97,108,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,77,97,116,101,114,105,97,108,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,
101,114,95,83,97,109,112,108,105,110,103,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,66,101,122,105,101,114,67,117,114,118,101,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,83,105,110,117,115,68,105,115,112,108,97,99,101,109,101,110,116,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,
83,112,97,116,105,97,108,78,111,105,115,101,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,80,101,114,108,105,110,78,111,105,115,101,49,68,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,80,101,114,108,105,110,78,111,105,115,101,50,68,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,66,
97,99,107,98,111,110,101,83,116,114,101,116,99,104,101,114,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,84,105,112,82,101,109,111,118,101,114,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,80,111,108,121,103,111,110,97,108,105,122,97,116,105,111,110,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,
101,114,95,71,117,105,100,105,110,103,76,105,110,101,115,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,66,108,117,101,112,114,105,110,116,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,50,68,79,102,102,115,101,116,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,50,68,84,114,97,110,115,
102,111,114,109,0,76,105,110,101,83,116,121,108,101,71,101,111,109,101,116,114,121,77,111,100,105,102,105,101,114,95,83,105,109,112,108,105,102,105,99,97,116,105,111,110,0,76,105,110,101,83,116,121,108,101,84,104,105,99,107,110,101,115,115,77,111,100,105,102,105,101,114,95,67,97,108,108,105,103,114,97,112,104,121,0,76,105,110,107,0,76,105,110,107,68,97,116,97,0,77,97,115,107,0,77,97,115,107,80,97,114,101,110,116,0,77,97,115,107,83,
112,108,105,110,101,80,111,105,110,116,85,87,0,77,97,115,107,83,112,108,105,110,101,80,111,105,110,116,0,77,97,115,107,83,112,108,105,110,101,0,77,97,115,107,76,97,121,101,114,83,104,97,112,101,0,77,97,115,107,76,97,121,101,114,0,84,101,120,80,97,105,110,116,83,108,111,116,0,77,97,116,101,114,105,97,108,71,80,101,110,99,105,108,83,116,121,108,101,0,77,97,116,101,114,105,97,108,76,105,110,101,65,114,116,0,77,83,101,108,101,
99,116,0,77,80,111,108,121,0,77,76,111,111,112,0,77,86,101,114,116,0,77,69,100,103,101,0,77,84,70,97,99,101,0,84,70,97,99,101,0,77,67,111,108,0,77,70,97,99,101,0,77,101,115,104,82,117,110,116,105,109,101,72,97,110,100,108,101,0,77,70,108,111,97,116,80,114,111,112,101,114,116,121,0,77,73,110,116,80,114,111,112,101,114,116,121,0,77,83,116,114,105,110,103,80,114,111,112,101,114,116,121,0,77,66,111,111,108,80,114,
111,112,101,114,116,121,0,77,73,110,116,56,80,114,111,112,101,114,116,121,0,77,68,101,102,111,114,109,87,101,105,103,104,116,0,77,86,101,114,116,83,107,105,110,0,77,76,111,111,112,67,111,108,0,77,80,114,111,112,67,111,108,0,77,68,105,115,112,115,0,71,114,105,100,80,97,105,110,116,77,97,115,107,0,70,114,101,101,115,116,121,108,101,69,100,103,101,0,70,114,101,101,115,116,121,108,101,70,97,99,101,0,77,76,111,111,112,85,86,0,
77,82,101,99,97,115,116,0,77,101,116,97,69,108,101,109,0,66,111,117,110,100,66,111,120,0,77,101,116,97,66,97,108,108,0,77,111,100,105,102,105,101,114,68,97,116,97,0,77,97,112,112,105,110,103,73,110,102,111,77,111,100,105,102,105,101,114,68,97,116,97,0,83,117,98,115,117,114,102,77,111,100,105,102,105,101,114,68,97,116,97,0,76,97,116,116,105,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,67,117,114,118,101,77,111,100,
105,102,105,101,114,68,97,116,97,0,66,117,105,108,100,77,111,100,105,102,105,101,114,68,97,116,97,0,77,97,115,107,77,111,100,105,102,105,101,114,68,97,116,97,0,65,114,114,97,121,77,111,100,105,102,105,101,114,68,97,116,97,0,77,105,114,114,111,114,77,111,100,105,102,105,101,114,68,97,116,97,0,69,100,103,101,83,112,108,105,116,77,111,100,105,102,105,101,114,68,97,116,97,0,66,101,118,101,108,77,111,100,105,102,105,101,114,68,97,116,
97,0,68,105,115,112,108,97,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,85,86,80,114,111,106,101,99,116,77,111,100,105,102,105,101,114,68,97,116,97,0,68,101,99,105,109,97,116,101,77,111,100,105,102,105,101,114,68,97,116,97,0,83,109,111,111,116,104,77,111,100,105,102,105,101,114,68,97,116,97,0,67,97,115,116,77,111,100,105,102,105,101,114,68,97,116,97,0,87,97,118,101,77,111,100,105,102,105,101,114,68,97,116,97,0,65,
114,109,97,116,117,114,101,77,111,100,105,102,105,101,114,68,97,116,97,0,72,111,111,107,77,111,100,105,102,105,101,114,68,97,116,97,0,83,111,102,116,98,111,100,121,77,111,100,105,102,105,101,114,68,97,116,97,0,67,108,111,116,104,77,111,100,105,102,105,101,114,68,97,116,97,0,67,108,111,116,104,0,67,108,111,116,104,72,97,105,114,68,97,116,97,0,67,108,111,116,104,83,111,108,118,101,114,82,101,115,117,108,116,0,67,111,108,108,105,115,
105,111,110,77,111,100,105,102,105,101,114,68,97,116,97,0,66,86,72,84,114,101,101,0,83,117,114,102,97,99,101,77,111,100,105,102,105,101,114,68,97,116,97,95,82,117,110,116,105,109,101,0,66,86,72,84,114,101,101,70,114,111,109,77,101,115,104,0,83,117,114,102,97,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,66,111,111,108,101,97,110,77,111,100,105,102,105,101,114,68,97,116,97,0,77,68,101,102,73,110,102,108,117,101,110,99,
101,0,77,68,101,102,67,101,108,108,0,77,101,115,104,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,80,97,114,116,105,99,108,101,83,121,115,116,101,109,77,111,100,105,102,105,101,114,68,97,116,97,0,80,97,114,116,105,99,108,101,73,110,115,116,97,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,69,120,112,108,111,100,101,77,111,100,105,102,105,101,114,68,97,116,97,0,77,117,108,116,105,114,101,115,77,111,
100,105,102,105,101,114,68,97,116,97,0,70,108,117,105,100,115,105,109,77,111,100,105,102,105,101,114,68,97,116,97,0,70,108,117,105,100,115,105,109,83,101,116,116,105,110,103,115,0,83,109,111,107,101,77,111,100,105,102,105,101,114,68,97,116,97,0,83,104,114,105,110,107,119,114,97,112,77,111,100,105,102,105,101,114,68,97,116,97,0,83,105,109,112,108,101,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,83,104,97,112,101,
75,101,121,77,111,100,105,102,105,101,114,68,97,116,97,0,83,111,108,105,100,105,102,121,77,111,100,105,102,105,101,114,68,97,116,97,0,83,99,114,101,119,77,111,100,105,102,105,101,114,68,97,116,97,0,79,99,101,97,110,77,111,100,105,102,105,101,114,68,97,116,97,0,79,99,101,97,110,0,79,99,101,97,110,67,97,99,104,101,0,87,97,114,112,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,86,71,69,100,105,116,77,
111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,86,71,77,105,120,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,86,71,80,114,111,120,105,109,105,116,121,77,111,100,105,102,105,101,114,68,97,116,97,0,82,101,109,101,115,104,77,111,100,105,102,105,101,114,68,97,116,97,0,83,107,105,110,77,111,100,105,102,105,101,114,68,97,116,97,0,84,114,105,97,110,103,117,108,97,116,101,77,111,100,105,102,105,101,
114,68,97,116,97,0,76,97,112,108,97,99,105,97,110,83,109,111,111,116,104,77,111,100,105,102,105,101,114,68,97,116,97,0,67,111,114,114,101,99,116,105,118,101,83,109,111,111,116,104,68,101,108,116,97,67,97,99,104,101,0,67,111,114,114,101,99,116,105,118,101,83,109,111,111,116,104,77,111,100,105,102,105,101,114,68,97,116,97,0,85,86,87,97,114,112,77,111,100,105,102,105,101,114,68,97,116,97,0,77,101,115,104,67,97,99,104,101,77,111,
100,105,102,105,101,114,68,97,116,97,0,76,97,112,108,97,99,105,97,110,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,87,105,114,101,102,114,97,109,101,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,108,100,77,111,100,105,102,105,101,114,68,97,116,97,0,68,97,116,97,84,114,97,110,115,102,101,114,77,111,100,105,102,105,101,114,68,97,116,97,0,78,111,114,109,97,108,69,100,105,116,77,111,100,105,102,105,101,
114,68,97,116,97,0,77,101,115,104,83,101,113,67,97,99,104,101,77,111,100,105,102,105,101,114,68,97,116,97,0,83,68,101,102,66,105,110,100,0,83,68,101,102,86,101,114,116,0,83,117,114,102,97,99,101,68,101,102,111,114,109,77,111,100,105,102,105,101,114,68,97,116,97,0,87,101,105,103,104,116,101,100,78,111,114,109,97,108,77,111,100,105,102,105,101,114,68,97,116,97,0,78,111,100,101,115,77,111,100,105,102,105,101,114,83,101,116,116,105,
110,103,115,0,78,111,100,101,115,77,111,100,105,102,105,101,114,68,97,116,97,66,108,111,99,107,0,78,111,100,101,115,77,111,100,105,102,105,101,114,66,97,107,101,0,78,111,100,101,115,77,111,100,105,102,105,101,114,80,97,110,101,108,0,78,111,100,101,115,77,111,100,105,102,105,101,114,68,97,116,97,0,78,111,100,101,115,77,111,100,105,102,105,101,114,82,117,110,116,105,109,101,72,97,110,100,108,101,0,77,101,115,104,84,111,86,111,108,117,109,
101,77,111,100,105,102,105,101,114,68,97,116,97,0,86,111,108,117,109,101,68,105,115,112,108,97,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,86,111,108,117,109,101,84,111,77,101,115,104,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,77,111,100,105,102,105,101,114,73,110,102,108,117,101,110,99,101,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,79,112,97,99,105,116,121,77,
111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,83,117,98,100,105,118,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,67,111,108,111,114,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,84,105,110,116,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,83,109,111,111,116,104,
77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,79,102,102,115,101,116,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,78,111,105,115,101,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,77,105,114,114,111,114,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,84,104,105,
99,107,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,76,97,116,116,105,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,68,97,115,104,77,111,100,105,102,105,101,114,83,101,103,109,101,110,116,0,71,114,101,97,115,101,80,101,110,99,105,108,68,97,115,104,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,
77,117,108,116,105,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,76,101,110,103,116,104,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,87,101,105,103,104,116,65,110,103,108,101,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,65,114,114,97,121,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,
101,80,101,110,99,105,108,87,101,105,103,104,116,80,114,111,120,105,109,105,116,121,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,72,111,111,107,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,76,105,110,101,97,114,116,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,65,114,109,97,116,117,114,101,77,111,100,105,
102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,84,105,109,101,77,111,100,105,102,105,101,114,83,101,103,109,101,110,116,0,71,114,101,97,115,101,80,101,110,99,105,108,84,105,109,101,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,69,110,118,101,108,111,112,101,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,79,117,116,108,105,
110,101,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,83,104,114,105,110,107,119,114,97,112,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,66,117,105,108,100,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,101,110,99,105,108,83,105,109,112,108,105,102,121,77,111,100,105,102,105,101,114,68,97,116,97,0,71,114,101,97,115,101,80,
101,110,99,105,108,84,101,120,116,117,114,101,77,111,100,105,102,105,101,114,68,97,116,97,0,77,111,118,105,101,67,108,105,112,80,114,111,120,121,0,77,111,118,105,101,67,108,105,112,95,82,117,110,116,105,109,101,71,80,85,84,101,120,116,117,114,101,0,77,111,118,105,101,67,108,105,112,95,82,117,110,116,105,109,101,0,77,111,118,105,101,67,108,105,112,67,97,99,104,101,0,77,111,118,105,101,84,114,97,99,107,105,110,103,0,77,111,118,105,101,
67,108,105,112,83,99,111,112,101,115,0,77,111,118,105,101,84,114,97,99,107,105,110,103,77,97,114,107,101,114,0,77,111,118,105,101,84,114,97,99,107,105,110,103,84,114,97,99,107,0,98,65,99,116,105,111,110,77,111,100,105,102,105,101,114,0,98,65,99,116,105,111,110,83,116,114,105,112,0,98,78,111,100,101,84,114,101,101,73,110,116,101,114,102,97,99,101,73,116,101,109,0,98,78,111,100,101,84,114,101,101,73,110,116,101,114,102,97,99,101,
83,111,99,107,101,116,0,98,78,111,100,101,84,114,101,101,73,110,116,101,114,102,97,99,101,80,97,110,101,108,0,98,78,111,100,101,84,114,101,101,73,110,116,101,114,102,97,99,101,0,98,78,111,100,101,84,114,101,101,73,110,116,101,114,102,97,99,101,82,117,110,116,105,109,101,72,97,110,100,108,101,0,98,78,111,100,101,83,116,97,99,107,0,98,78,111,100,101,83,111,99,107,101,116,0,98,78,111,100,101,83,111,99,107,101,116,84,121,112,101,
72,97,110,100,108,101,0,98,78,111,100,101,76,105,110,107,0,98,78,111,100,101,83,111,99,107,101,116,82,117,110,116,105,109,101,72,97,110,100,108,101,0,98,78,111,100,101,80,97,110,101,108,83,116,97,116,101,0,98,78,111,100,101,0,98,78,111,100,101,84,121,112,101,72,97,110,100,108,101,0,98,78,111,100,101,82,117,110,116,105,109,101,72,97,110,100,108,101,0,98,78,111,100,101,73,110,115,116,97,110,99,101,75,101,121,0,98,78,101,115,
116,101,100,78,111,100,101,80,97,116,104,0,98,78,101,115,116,101,100,78,111,100,101,82,101,102,0,98,78,111,100,101,84,114,101,101,84,121,112,101,72,97,110,100,108,101,0,78,111,100,101,73,110,115,116,97,110,99,101,72,97,115,104,72,97,110,100,108,101,0,71,101,111,109,101,116,114,121,78,111,100,101,65,115,115,101,116,84,114,97,105,116,115,0,98,78,111,100,101,84,114,101,101,82,117,110,116,105,109,101,72,97,110,100,108,101,0,98,78,111,
100,101,83,111,99,107,101,116,86,97,108,117,101,73,110,116,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,70,108,111,97,116,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,66,111,111,108,101,97,110,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,86,101,99,116,111,114,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,82,111,116,97,116,105,111,110,0,98,78,111,100,101,83,111,99,107,
101,116,86,97,108,117,101,82,71,66,65,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,83,116,114,105,110,103,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,79,98,106,101,99,116,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,73,109,97,103,101,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,67,111,108,108,101,99,116,105,111,110,0,98,78,111,100,101,83,111,99,107,101,116,86,97,
108,117,101,84,101,120,116,117,114,101,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,77,97,116,101,114,105,97,108,0,98,78,111,100,101,83,111,99,107,101,116,86,97,108,117,101,77,101,110,117,0,82,117,110,116,105,109,101,78,111,100,101,69,110,117,109,73,116,101,109,115,72,97,110,100,108,101,0,78,111,100,101,70,114,97,109,101,0,78,111,100,101,73,109,97,103,101,65,110,105,109,0,67,111,108,111,114,67,111,114,114,101,99,116,
105,111,110,68,97,116,97,0,78,111,100,101,67,111,108,111,114,67,111,114,114,101,99,116,105,111,110,0,78,111,100,101,66,111,107,101,104,73,109,97,103,101,0,78,111,100,101,66,111,120,77,97,115,107,0,78,111,100,101,69,108,108,105,112,115,101,77,97,115,107,0,78,111,100,101,73,109,97,103,101,76,97,121,101,114,0,78,111,100,101,66,108,117,114,68,97,116,97,0,78,111,100,101,68,66,108,117,114,68,97,116,97,0,78,111,100,101,66,105,108,
97,116,101,114,97,108,66,108,117,114,68,97,116,97,0,78,111,100,101,75,117,119,97,104,97,114,97,68,97,116,97,0,78,111,100,101,65,110,116,105,65,108,105,97,115,105,110,103,68,97,116,97,0,78,111,100,101,72,117,101,83,97,116,0,78,111,100,101,73,109,97,103,101,70,105,108,101,0,73,109,97,103,101,70,111,114,109,97,116,68,97,116,97,0,78,111,100,101,73,109,97,103,101,77,117,108,116,105,70,105,108,101,0,78,111,100,101,73,109,97,
103,101,77,117,108,116,105,70,105,108,101,83,111,99,107,101,116,0,78,111,100,101,67,104,114,111,109,97,0,78,111,100,101,84,119,111,88,89,115,0,78,111,100,101,84,119,111,70,108,111,97,116,115,0,78,111,100,101,86,101,114,116,101,120,67,111,108,0,78,111,100,101,67,77,80,67,111,109,98,83,101,112,67,111,108,111,114,0,78,111,100,101,68,101,102,111,99,117,115,0,78,111,100,101,83,99,114,105,112,116,68,105,99,116,0,78,111,100,101,71,
108,97,114,101,0,78,111,100,101,84,111,110,101,109,97,112,0,78,111,100,101,76,101,110,115,68,105,115,116,0,78,111,100,101,67,111,108,111,114,66,97,108,97,110,99,101,0,78,111,100,101,67,111,108,111,114,115,112,105,108,108,0,78,111,100,101,67,111,110,118,101,114,116,67,111,108,111,114,83,112,97,99,101,0,78,111,100,101,68,105,108,97,116,101,69,114,111,100,101,0,78,111,100,101,77,97,115,107,0,78,111,100,101,83,101,116,65,108,112,104,
97,0,78,111,100,101,84,101,120,66,97,115,101,0,84,101,120,77,97,112,112,105,110,103,0,67,111,108,111,114,77,97,112,112,105,110,103,0,78,111,100,101,84,101,120,83,107,121,0,78,111,100,101,84,101,120,73,109,97,103,101,0,78,111,100,101,84,101,120,67,104,101,99,107,101,114,0,78,111,100,101,84,101,120,66,114,105,99,107,0,78,111,100,101,84,101,120,69,110,118,105,114,111,110,109,101,110,116,0,78,111,100,101,84,101,120,71,114,97,100,
105,101,110,116,0,78,111,100,101,84,101,120,78,111,105,115,101,0,78,111,100,101,84,101,120,86,111,114,111,110,111,105,0,78,111,100,101,84,101,120,77,117,115,103,114,97,118,101,0,78,111,100,101,84,101,120,87,97,118,101,0,78,111,100,101,84,101,120,77,97,103,105,99,0,78,111,100,101,83,104,97,100,101,114,65,116,116,114,105,98,117,116,101,0,78,111,100,101,83,104,97,100,101,114,86,101,99,116,84,114,97,110,115,102,111,114,109,0,78,111,
100,101,83,104,97,100,101,114,84,101,120,80,111,105,110,116,68,101,110,115,105,116,121,0,80,111,105,110,116,68,101,110,115,105,116,121,0,78,111,100,101,83,104,97,100,101,114,80,114,105,110,99,105,112,108,101,100,0,78,111,100,101,83,104,97,100,101,114,72,97,105,114,80,114,105,110,99,105,112,108,101,100,0,84,101,120,78,111,100,101,79,117,116,112,117,116,0,78,111,100,101,75,101,121,105,110,103,83,99,114,101,101,110,68,97,116,97,0,78,111,
100,101,75,101,121,105,110,103,68,97,116,97,0,78,111,100,101,84,114,97,99,107,80,111,115,68,97,116,97,0,78,111,100,101,84,114,97,110,115,108,97,116,101,68,97,116,97,0,78,111,100,101,80,108,97,110,101,84,114,97,99,107,68,101,102,111,114,109,68,97,116,97,0,78,111,100,101,83,104,97,100,101,114,83,99,114,105,112,116,0,78,111,100,101,83,104,97,100,101,114,84,97,110,103,101,110,116,0,78,111,100,101,83,104,97,100,101,114,78,111,
114,109,97,108,77,97,112,0,78,111,100,101,83,104,97,100,101,114,85,86,77,97,112,0,78,111,100,101,83,104,97,100,101,114,86,101,114,116,101,120,67,111,108,111,114,0,78,111,100,101,83,104,97,100,101,114,84,101,120,73,69,83,0,78,111,100,101,83,104,97,100,101,114,79,117,116,112,117,116,65,79,86,0,78,111,100,101,83,117,110,66,101,97,109,115,0,67,114,121,112,116,111,109,97,116,116,101,69,110,116,114,121,0,67,114,121,112,116,111,109,
97,116,116,101,76,97,121,101,114,0,78,111,100,101,67,114,121,112,116,111,109,97,116,116,101,95,82,117,110,116,105,109,101,0,78,111,100,101,67,114,121,112,116,111,109,97,116,116,101,0,78,111,100,101,68,101,110,111,105,115,101,0,78,111,100,101,77,97,112,82,97,110,103,101,0,78,111,100,101,82,97,110,100,111,109,86,97,108,117,101,0,78,111,100,101,65,99,99,117,109,117,108,97,116,101,70,105,101,108,100,0,78,111,100,101,73,110,112,117,116,
66,111,111,108,0,78,111,100,101,73,110,112,117,116,73,110,116,0,78,111,100,101,73,110,112,117,116,82,111,116,97,116,105,111,110,0,78,111,100,101,73,110,112,117,116,86,101,99,116,111,114,0,78,111,100,101,73,110,112,117,116,67,111,108,111,114,0,78,111,100,101,73,110,112,117,116,83,116,114,105,110,103,0,78,111,100,101,71,101,111,109,101,116,114,121,69,120,116,114,117,100,101,77,101,115,104,0,78,111,100,101,71,101,111,109,101,116,114,121,79,
98,106,101,99,116,73,110,102,111,0,78,111,100,101,71,101,111,109,101,116,114,121,80,111,105,110,116,115,84,111,86,111,108,117,109,101,0,78,111,100,101,71,101,111,109,101,116,114,121,67,111,108,108,101,99,116,105,111,110,73,110,102,111,0,78,111,100,101,71,101,111,109,101,116,114,121,80,114,111,120,105,109,105,116,121,0,78,111,100,101,71,101,111,109,101,116,114,121,86,111,108,117,109,101,84,111,77,101,115,104,0,78,111,100,101,71,101,111,109,101,
116,114,121,77,101,115,104,84,111,86,111,108,117,109,101,0,78,111,100,101,71,101,111,109,101,116,114,121,83,117,98,100,105,118,105,115,105,111,110,83,117,114,102,97,99,101,0,78,111,100,101,71,101,111,109,101,116,114,121,77,101,115,104,67,105,114,99,108,101,0,78,111,100,101,71,101,111,109,101,116,114,121,77,101,115,104,67,121,108,105,110,100,101,114,0,78,111,100,101,71,101,111,109,101,116,114,121,77,101,115,104,67,111,110,101,0,78,111,100,101,
71,101,111,109,101,116,114,121,77,101,114,103,101,66,121,68,105,115,116,97,110,99,101,0,78,111,100,101,71,101,111,109,101,116,114,121,77,101,115,104,76,105,110,101,0,78,111,100,101,83,119,105,116,99,104,0,78,111,100,101,69,110,117,109,73,116,101,109,0,78,111,100,101,69,110,117,109,68,101,102,105,110,105,116,105,111,110,0,78,111,100,101,77,101,110,117,83,119,105,116,99,104,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,
83,112,108,105,110,101,84,121,112,101,0,78,111,100,101,71,101,111,109,101,116,114,121,83,101,116,67,117,114,118,101,72,97,110,100,108,101,80,111,115,105,116,105,111,110,115,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,83,101,116,72,97,110,100,108,101,115,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,83,101,108,101,99,116,72,97,110,100,108,101,115,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,
114,118,101,80,114,105,109,105,116,105,118,101,65,114,99,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,80,114,105,109,105,116,105,118,101,76,105,110,101,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,80,114,105,109,105,116,105,118,101,66,101,122,105,101,114,83,101,103,109,101,110,116,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,80,114,105,109,105,116,105,118,101,67,105,114,99,108,101,0,
78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,80,114,105,109,105,116,105,118,101,81,117,97,100,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,82,101,115,97,109,112,108,101,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,70,105,108,108,101,116,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,84,114,105,109,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,
84,111,80,111,105,110,116,115,0,78,111,100,101,71,101,111,109,101,116,114,121,67,117,114,118,101,83,97,109,112,108,101,0,78,111,100,101,71,101,111,109,101,116,114,121,84,114,97,110,115,102,101,114,65,116,116,114,105,98,117,116,101,0,78,111,100,101,71,101,111,109,101,116,114,121,83,97,109,112,108,101,73,110,100,101,120,0,78,111,100,101,71,101,111,109,101,116,114,121,82,97,121,99,97,115,116,0,78,111,100,101,71,101,111,109,101,116,114,121,67,
117,114,118,101,70,105,108,108,0,78,111,100,101,71,101,111,109,101,116,114,121,77,101,115,104,84,111,80,111,105,110,116,115,0,78,111,100,101,71,101,111,109,101,116,114,121,65,116,116,114,105,98,117,116,101,67,97,112,116,117,114,101,73,116,101,109,0,78,111,100,101,71,101,111,109,101,116,114,121,65,116,116,114,105,98,117,116,101,67,97,112,116,117,114,101,0,78,111,100,101,71,101,111,109,101,116,114,121,83,116,111,114,101,78,97,109,101,100,65,116,
116,114,105,98,117,116,101,0,78,111,100,101,71,101,111,109,101,116,114,121,73,110,112,117,116,78,97,109,101,100,65,116,116,114,105,98,117,116,101,0,78,111,100,101,71,101,111,109,101,116,114,121,83,116,114,105,110,103,84,111,67,117,114,118,101,115,0,78,111,100,101,71,101,111,109,101,116,114,121,68,101,108,101,116,101,71,101,111,109,101,116,114,121,0,78,111,100,101,71,101,111,109,101,116,114,121,68,117,112,108,105,99,97,116,101,69,108,101,109,101,
110,116,115,0,78,111,100,101,71,101,111,109,101,116,114,121,83,101,112,97,114,97,116,101,71,101,111,109,101,116,114,121,0,78,111,100,101,71,101,111,109,101,116,114,121,73,109,97,103,101,84,101,120,116,117,114,101,0,78,111,100,101,71,101,111,109,101,116,114,121,86,105,101,119,101,114,0,78,111,100,101,71,101,111,109,101,116,114,121,85,86,85,110,119,114,97,112,0,78,111,100,101,83,105,109,117,108,97,116,105,111,110,73,116,101,109,0,78,111,100,
101,71,101,111,109,101,116,114,121,83,105,109,117,108,97,116,105,111,110,73,110,112,117,116,0,78,111,100,101,71,101,111,109,101,116,114,121,83,105,109,117,108,97,116,105,111,110,79,117,116,112,117,116,0,78,111,100,101,82,101,112,101,97,116,73,116,101,109,0,78,111,100,101,71,101,111,109,101,116,114,121,82,101,112,101,97,116,73,110,112,117,116,0,78,111,100,101,71,101,111,109,101,116,114,121,82,101,112,101,97,116,79,117,116,112,117,116,0,73,110,
100,101,120,83,119,105,116,99,104,73,116,101,109,0,78,111,100,101,73,110,100,101,120,83,119,105,116,99,104,0,78,111,100,101,71,101,111,109,101,116,114,121,68,105,115,116,114,105,98,117,116,101,80,111,105,110,116,115,73,110,86,111,108,117,109,101,0,78,111,100,101,70,117,110,99,116,105,111,110,67,111,109,112,97,114,101,0,78,111,100,101,67,111,109,98,83,101,112,67,111,108,111,114,0,78,111,100,101,83,104,97,100,101,114,77,105,120,0,78,111,
100,101,71,101,111,109,101,116,114,121,66,97,107,101,73,116,101,109,0,78,111,100,101,71,101,111,109,101,116,114,121,66,97,107,101,0,70,108,117,105,100,86,101,114,116,101,120,86,101,108,111,99,105,116,121,0,80,97,114,116,68,101,102,108,101,99,116,0,83,66,86,101,114,116,101,120,0,83,111,102,116,66,111,100,121,95,83,104,97,114,101,100,0,83,111,102,116,66,111,100,121,0,66,111,100,121,80,111,105,110,116,0,66,111,100,121,83,112,114,105,
110,103,0,83,66,83,99,114,97,116,99,104,0,98,68,101,102,111,114,109,71,114,111,117,112,0,98,70,97,99,101,77,97,112,0,79,98,106,101,99,116,76,105,110,101,65,114,116,0,76,105,103,104,116,76,105,110,107,105,110,103,82,117,110,116,105,109,101,0,76,105,103,104,116,76,105,110,107,105,110,103,0,83,99,117,108,112,116,83,101,115,115,105,111,110,0,82,105,103,105,100,66,111,100,121,79,98,0,82,105,103,105,100,66,111,100,121,67,111,110,
0,79,98,106,101,99,116,82,117,110,116,105,109,101,72,97,110,100,108,101,0,79,98,72,111,111,107,0,84,114,101,101,83,116,111,114,101,69,108,101,109,0,84,114,101,101,83,116,111,114,101,0,72,97,105,114,75,101,121,0,80,97,114,116,105,99,108,101,75,101,121,0,66,111,105,100,80,97,114,116,105,99,108,101,0,80,97,114,116,105,99,108,101,83,112,114,105,110,103,0,67,104,105,108,100,80,97,114,116,105,99,108,101,0,80,97,114,116,105,99,
108,101,84,97,114,103,101,116,0,80,97,114,116,105,99,108,101,68,117,112,108,105,87,101,105,103,104,116,0,80,97,114,116,105,99,108,101,68,97,116,97,0,83,80,72,70,108,117,105,100,83,101,116,116,105,110,103,115,0,80,97,114,116,105,99,108,101,83,101,116,116,105,110,103,115,0,80,84,67,97,99,104,101,69,100,105,116,0,80,97,114,116,105,99,108,101,67,97,99,104,101,75,101,121,0,75,68,84,114,101,101,95,51,100,0,80,97,114,116,105,
99,108,101,68,114,97,119,68,97,116,97,0,80,84,67,97,99,104,101,69,120,116,114,97,0,80,84,67,97,99,104,101,77,101,109,0,80,111,105,110,116,67,108,111,117,100,0,80,111,105,110,116,67,108,111,117,100,82,117,110,116,105,109,101,72,97,110,100,108,101,0,82,105,103,105,100,66,111,100,121,87,111,114,108,100,95,83,104,97,114,101,100,0,82,105,103,105,100,66,111,100,121,87,111,114,108,100,0,82,105,103,105,100,66,111,100,121,79,98,95,
83,104,97,114,101,100,0,70,70,77,112,101,103,67,111,100,101,99,68,97,116,97,0,65,117,100,105,111,68,97,116,97,0,83,99,101,110,101,82,101,110,100,101,114,76,97,121,101,114,0,83,99,101,110,101,82,101,110,100,101,114,86,105,101,119,0,66,97,107,101,68,97,116,97,0,82,101,110,100,101,114,68,97,116,97,0,84,105,109,101,77,97,114,107,101,114,0,80,97,105,110,116,95,82,117,110,116,105,109,101,0,80,97,105,110,116,84,111,111,108,
83,108,111,116,0,80,97,105,110,116,0,73,109,97,103,101,80,97,105,110,116,83,101,116,116,105,110,103,115,0,80,97,105,110,116,77,111,100,101,83,101,116,116,105,110,103,115,0,80,97,114,116,105,99,108,101,66,114,117,115,104,68,97,116,97,0,80,97,114,116,105,99,108,101,69,100,105,116,83,101,116,116,105,110,103,115,0,83,99,117,108,112,116,0,67,117,114,118,101,115,83,99,117,108,112,116,0,85,118,83,99,117,108,112,116,0,71,112,80,97,
105,110,116,0,71,112,86,101,114,116,101,120,80,97,105,110,116,0,71,112,83,99,117,108,112,116,80,97,105,110,116,0,71,112,87,101,105,103,104,116,80,97,105,110,116,0,86,80,97,105,110,116,0,71,80,95,83,99,117,108,112,116,95,71,117,105,100,101,0,71,80,95,83,99,117,108,112,116,95,83,101,116,116,105,110,103,115,0,71,80,95,73,110,116,101,114,112,111,108,97,116,101,95,83,101,116,116,105,110,103,115,0,85,110,105,102,105,101,100,80,
97,105,110,116,83,101,116,116,105,110,103,115,0,67,111,108,111,114,83,112,97,99,101,0,67,117,114,118,101,80,97,105,110,116,83,101,116,116,105,110,103,115,0,77,101,115,104,83,116,97,116,86,105,115,0,83,101,113,117,101,110,99,101,114,84,111,111,108,83,101,116,116,105,110,103,115,0,84,111,111,108,83,101,116,116,105,110,103,115,0,85,110,105,116,83,101,116,116,105,110,103,115,0,80,104,121,115,105,99,115,83,101,116,116,105,110,103,115,0,68,
105,115,112,108,97,121,83,97,102,101,65,114,101,97,115,0,83,99,101,110,101,68,105,115,112,108,97,121,0,86,105,101,119,51,68,83,104,97,100,105,110,103,0,82,97,121,116,114,97,99,101,69,69,86,69,69,0,83,99,101,110,101,69,69,86,69,69,0,83,99,101,110,101,71,112,101,110,99,105,108,0,83,99,101,110,101,72,121,100,114,97,0,84,114,97,110,115,102,111,114,109,79,114,105,101,110,116,97,116,105,111,110,83,108,111,116,0,86,105,101,
119,51,68,67,117,114,115,111,114,0,69,100,105,116,105,110,103,0,83,99,101,110,101,82,117,110,116,105,109,101,72,97,110,100,108,101,0,65,82,101,103,105,111,110,0,119,109,84,105,109,101,114,0,119,109,84,111,111,108,116,105,112,83,116,97,116,101,0,83,99,114,86,101,114,116,0,118,101,99,50,115,0,83,99,114,69,100,103,101,0,83,99,114,65,114,101,97,77,97,112,0,76,97,121,111,117,116,80,97,110,101,108,83,116,97,116,101,0,80,97,
110,101,108,0,80,97,110,101,108,84,121,112,101,0,117,105,76,97,121,111,117,116,0,80,97,110,101,108,95,82,117,110,116,105,109,101,0,80,97,110,101,108,67,97,116,101,103,111,114,121,83,116,97,99,107,0,117,105,76,105,115,116,0,117,105,76,105,115,116,84,121,112,101,0,117,105,76,105,115,116,68,121,110,0,84,114,97,110,115,102,111,114,109,79,114,105,101,110,116,97,116,105,111,110,0,117,105,80,114,101,118,105,101,119,0,83,99,114,71,108,
111,98,97,108,65,114,101,97,68,97,116,97,0,83,99,114,65,114,101,97,95,82,117,110,116,105,109,101,0,98,84,111,111,108,82,101,102,0,83,99,114,65,114,101,97,0,114,99,116,105,0,83,112,97,99,101,84,121,112,101,0,65,82,101,103,105,111,110,95,82,117,110,116,105,109,101,0,65,82,101,103,105,111,110,84,121,112,101,0,119,109,71,105,122,109,111,77,97,112,0,119,109,68,114,97,119,66,117,102,102,101,114,0,65,115,115,101,116,83,104,
101,108,102,83,101,116,116,105,110,103,115,0,65,115,115,101,116,83,104,101,108,102,0,65,115,115,101,116,83,104,101,108,102,84,121,112,101,0,82,101,103,105,111,110,65,115,115,101,116,83,104,101,108,102,0,70,105,108,101,72,97,110,100,108,101,114,0,70,105,108,101,72,97,110,100,108,101,114,84,121,112,101,72,97,110,100,108,101,0,83,116,114,105,112,65,110,105,109,0,83,116,114,105,112,69,108,101,109,0,83,116,114,105,112,67,114,111,112,0,83,
116,114,105,112,84,114,97,110,115,102,111,114,109,0,83,116,114,105,112,67,111,108,111,114,66,97,108,97,110,99,101,0,83,116,114,105,112,80,114,111,120,121,0,83,116,114,105,112,0,83,101,113,82,101,116,105,109,105,110,103,72,97,110,100,108,101,0,83,101,113,117,101,110,99,101,82,117,110,116,105,109,101,0,83,101,113,117,101,110,99,101,0,98,83,111,117,110,100,0,77,101,116,97,83,116,97,99,107,0,83,101,113,84,105,109,101,108,105,110,101,
67,104,97,110,110,101,108,0,69,100,105,116,105,110,103,82,117,110,116,105,109,101,0,83,101,113,117,101,110,99,101,76,111,111,107,117,112,0,77,101,100,105,97,80,114,101,115,101,110,99,101,0,83,101,113,67,97,99,104,101,0,80,114,101,102,101,116,99,104,74,111,98,0,87,105,112,101,86,97,114,115,0,71,108,111,119,86,97,114,115,0,84,114,97,110,115,102,111,114,109,86,97,114,115,0,83,111,108,105,100,67,111,108,111,114,86,97,114,115,0,
83,112,101,101,100,67,111,110,116,114,111,108,86,97,114,115,0,71,97,117,115,115,105,97,110,66,108,117,114,86,97,114,115,0,84,101,120,116,86,97,114,115,0,67,111,108,111,114,77,105,120,86,97,114,115,0,83,101,113,117,101,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,67,111,108,111,114,66,97,108,97,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,67,117,114,118,101,115,77,111,100,105,102,105,101,114,68,97,116,97,
0,72,117,101,67,111,114,114,101,99,116,77,111,100,105,102,105,101,114,68,97,116,97,0,66,114,105,103,104,116,67,111,110,116,114,97,115,116,77,111,100,105,102,105,101,114,68,97,116,97,0,83,101,113,117,101,110,99,101,114,77,97,115,107,77,111,100,105,102,105,101,114,68,97,116,97,0,87,104,105,116,101,66,97,108,97,110,99,101,77,111,100,105,102,105,101,114,68,97,116,97,0,83,101,113,117,101,110,99,101,114,84,111,110,101,109,97,112,77,111,
100,105,102,105,101,114,68,97,116,97,0,69,81,67,117,114,118,101,77,97,112,112,105,110,103,68,97,116,97,0,83,111,117,110,100,69,113,117,97,108,105,122,101,114,77,111,100,105,102,105,101,114,68,97,116,97,0,83,104,97,100,101,114,70,120,68,97,116,97,0,83,104,97,100,101,114,70,120,68,97,116,97,95,82,117,110,116,105,109,101,0,68,82,87,83,104,97,100,105,110,103,71,114,111,117,112,0,66,108,117,114,83,104,97,100,101,114,70,120,68,
97,116,97,0,67,111,108,111,114,105,122,101,83,104,97,100,101,114,70,120,68,97,116,97,0,70,108,105,112,83,104,97,100,101,114,70,120,68,97,116,97,0,71,108,111,119,83,104,97,100,101,114,70,120,68,97,116,97,0,80,105,120,101,108,83,104,97,100,101,114,70,120,68,97,116,97,0,82,105,109,83,104,97,100,101,114,70,120,68,97,116,97,0,83,104,97,100,111,119,83,104,97,100,101,114,70,120,68,97,116,97,0,83,119,105,114,108,83,104,97,
100,101,114,70,120,68,97,116,97,0,87,97,118,101,83,104,97,100,101,114,70,120,68,97,116,97,0,83,112,97,99,101,73,110,102,111,0,83,112,97,99,101,66,117,116,115,0,83,112,97,99,101,80,114,111,112,101,114,116,105,101,115,95,82,117,110,116,105,109,101,0,83,112,97,99,101,79,111,112,115,0,83,112,97,99,101,79,117,116,108,105,110,101,114,95,82,117,110,116,105,109,101,0,83,112,97,99,101,71,114,97,112,104,95,82,117,110,116,105,109,
101,0,83,112,97,99,101,73,112,111,0,83,112,97,99,101,78,108,97,0,83,101,113,117,101,110,99,101,114,80,114,101,118,105,101,119,79,118,101,114,108,97,121,0,83,101,113,117,101,110,99,101,114,84,105,109,101,108,105,110,101,79,118,101,114,108,97,121,0,83,101,113,117,101,110,99,101,114,67,97,99,104,101,79,118,101,114,108,97,121,0,83,112,97,99,101,83,101,113,0,83,112,97,99,101,83,101,113,95,82,117,110,116,105,109,101,0,77,97,115,
107,83,112,97,99,101,73,110,102,111,0,70,105,108,101,83,101,108,101,99,116,80,97,114,97,109,115,0,70,105,108,101,65,115,115,101,116,83,101,108,101,99,116,80,97,114,97,109,115,0,70,105,108,101,70,111,108,100,101,114,72,105,115,116,111,114,121,0,70,105,108,101,70,111,108,100,101,114,76,105,115,116,115,0,83,112,97,99,101,70,105,108,101,0,70,105,108,101,76,105,115,116,0,119,109,79,112,101,114,97,116,111,114,0,70,105,108,101,76,97,
121,111,117,116,0,83,112,97,99,101,70,105,108,101,95,82,117,110,116,105,109,101,0,83,112,97,99,101,73,109,97,103,101,79,118,101,114,108,97,121,0,83,112,97,99,101,73,109,97,103,101,0,83,112,97,99,101,84,101,120,116,0,83,112,97,99,101,84,101,120,116,95,82,117,110,116,105,109,101,0,83,99,114,105,112,116,0,83,112,97,99,101,83,99,114,105,112,116,0,98,78,111,100,101,84,114,101,101,80,97,116,104,0,83,112,97,99,101,78,111,
100,101,79,118,101,114,108,97,121,0,83,112,97,99,101,78,111,100,101,0,83,112,97,99,101,78,111,100,101,95,82,117,110,116,105,109,101,0,67,111,110,115,111,108,101,76,105,110,101,0,83,112,97,99,101,67,111,110,115,111,108,101,0,83,112,97,99,101,85,115,101,114,80,114,101,102,0,83,112,97,99,101,67,108,105,112,0,83,112,97,99,101,84,111,112,66,97,114,0,83,112,97,99,101,83,116,97,116,117,115,66,97,114,0,83,112,114,101,97,100,
115,104,101,101,116,67,111,108,117,109,110,73,68,0,83,112,114,101,97,100,115,104,101,101,116,67,111,108,117,109,110,0,83,112,97,99,101,83,112,114,101,97,100,115,104,101,101,116,0,86,105,101,119,101,114,80,97,116,104,0,83,112,97,99,101,83,112,114,101,97,100,115,104,101,101,116,95,82,117,110,116,105,109,101,0,83,112,114,101,97,100,115,104,101,101,116,82,111,119,70,105,108,116,101,114,0,83,112,101,97,107,101,114,0,84,101,120,116,76,105,
110,101,0,67,66,68,97,116,97,0,77,111,118,105,101,82,101,99,111,110,115,116,114,117,99,116,101,100,67,97,109,101,114,97,0,77,111,118,105,101,84,114,97,99,107,105,110,103,67,97,109,101,114,97,0,77,111,118,105,101,84,114,97,99,107,105,110,103,80,108,97,110,101,77,97,114,107,101,114,0,77,111,118,105,101,84,114,97,99,107,105,110,103,80,108,97,110,101,84,114,97,99,107,0,77,111,118,105,101,84,114,97,99,107,105,110,103,83,101,116,
116,105,110,103,115,0,77,111,118,105,101,84,114,97,99,107,105,110,103,83,116,97,98,105,108,105,122,97,116,105,111,110,0,77,111,118,105,101,84,114,97,99,107,105,110,103,82,101,99,111,110,115,116,114,117,99,116,105,111,110,0,77,111,118,105,101,84,114,97,99,107,105,110,103,79,98,106,101,99,116,0,77,111,118,105,101,84,114,97,99,107,105,110,103,83,116,97,116,115,0,77,111,118,105,101,84,114,97,99,107,105,110,103,68,111,112,101,115,104,101,
101,116,67,104,97,110,110,101,108,0,77,111,118,105,101,84,114,97,99,107,105,110,103,68,111,112,101,115,104,101,101,116,67,111,118,101,114,97,103,101,83,101,103,109,101,110,116,0,77,111,118,105,101,84,114,97,99,107,105,110,103,68,111,112,101,115,104,101,101,116,0,117,105,70,111,110,116,83,116,121,108,101,0,117,105,83,116,121,108,101,0,117,105,87,105,100,103,101,116,67,111,108,111,114,115,0,117,105,87,105,100,103,101,116,83,116,97,116,101,67,
111,108,111,114,115,0,117,105,80,97,110,101,108,67,111,108,111,114,115,0,84,104,101,109,101,85,73,0,84,104,101,109,101,65,115,115,101,116,83,104,101,108,102,0,84,104,101,109,101,83,112,97,99,101,0,84,104,101,109,101,67,111,108,108,101,99,116,105,111,110,67,111,108,111,114,0,84,104,101,109,101,83,116,114,105,112,67,111,108,111,114,0,98,84,104,101,109,101,0,98,65,100,100,111,110,0,98,80,97,116,104,67,111,109,112,97,114,101,0,98,
85,115,101,114,77,101,110,117,0,98,85,115,101,114,77,101,110,117,73,116,101,109,0,98,85,115,101,114,77,101,110,117,73,116,101,109,95,79,112,0,98,85,115,101,114,77,101,110,117,73,116,101,109,95,77,101,110,117,0,98,85,115,101,114,77,101,110,117,73,116,101,109,95,80,114,111,112,0,98,85,115,101,114,65,115,115,101,116,76,105,98,114,97,114,121,0,98,85,115,101,114,69,120,116,101,110,115,105,111,110,82,101,112,111,0,83,111,108,105,100,
76,105,103,104,116,0,87,97,108,107,78,97,118,105,103,97,116,105,111,110,0,85,115,101,114,68,101,102,95,82,117,110,116,105,109,101,0,85,115,101,114,68,101,102,95,83,112,97,99,101,68,97,116,97,0,85,115,101,114,68,101,102,95,70,105,108,101,83,112,97,99,101,68,97,116,97,0,85,115,101,114,68,101,102,95,69,120,112,101,114,105,109,101,110,116,97,108,0,98,85,115,101,114,83,99,114,105,112,116,68,105,114,101,99,116,111,114,121,0,98,
85,115,101,114,65,115,115,101,116,83,104,101,108,102,83,101,116,116,105,110,103,115,0,85,115,101,114,68,101,102,0,118,101,99,50,102,0,118,101,99,50,105,0,118,101,99,51,105,0,118,101,99,51,102,0,118,101,99,52,102,0,109,97,116,52,120,52,102,0,86,70,111,110,116,68,97,116,97,0,83,109,111,111,116,104,86,105,101,119,50,68,83,116,111,114,101,0,82,101,103,105,111,110,86,105,101,119,51,68,0,86,105,101,119,82,101,110,100,101,114,
0,83,109,111,111,116,104,86,105,101,119,51,68,83,116,111,114,101,0,86,105,101,119,51,68,79,118,101,114,108,97,121,0,86,105,101,119,51,68,95,82,117,110,116,105,109,101,0,86,105,101,119,51,68,0,86,105,101,119,101,114,80,97,116,104,69,108,101,109,0,73,68,86,105,101,119,101,114,80,97,116,104,69,108,101,109,0,77,111,100,105,102,105,101,114,86,105,101,119,101,114,80,97,116,104,69,108,101,109,0,71,114,111,117,112,78,111,100,101,86,
105,101,119,101,114,80,97,116,104,69,108,101,109,0,83,105,109,117,108,97,116,105,111,110,90,111,110,101,86,105,101,119,101,114,80,97,116,104,69,108,101,109,0,82,101,112,101,97,116,90,111,110,101,86,105,101,119,101,114,80,97,116,104,69,108,101,109,0,86,105,101,119,101,114,78,111,100,101,86,105,101,119,101,114,80,97,116,104,69,108,101,109,0,86,111,108,117,109,101,68,105,115,112,108,97,121,0,86,111,108,117,109,101,82,101,110,100,101,114,0,
86,111,108,117,109,101,0,86,111,108,117,109,101,82,117,110,116,105,109,101,72,97,110,100,108,101,0,82,101,112,111,114,116,76,105,115,116,0,115,116,100,95,109,117,116,101,120,95,116,121,112,101,0,119,109,88,114,68,97,116,97,0,119,109,88,114,82,117,110,116,105,109,101,68,97,116,97,0,88,114,83,101,115,115,105,111,110,83,101,116,116,105,110,103,115,0,119,109,87,105,110,100,111,119,77,97,110,97,103,101,114,0,119,109,87,105,110,100,111,119,
0,119,109,75,101,121,67,111,110,102,105,103,0,85,110,100,111,83,116,97,99,107,0,119,109,77,115,103,66,117,115,0,87,105,110,100,111,119,77,97,110,97,103,101,114,82,117,110,116,105,109,101,72,97,110,100,108,101,0,87,111,114,107,83,112,97,99,101,73,110,115,116,97,110,99,101,72,111,111,107,0,119,109,69,118,101,110,116,95,67,111,110,115,101,99,117,116,105,118,101,68,97,116,97,0,119,109,69,118,101,110,116,0,119,109,73,77,69,68,97,
116,97,0,119,109,75,101,121,77,97,112,73,116,101,109,0,80,111,105,110,116,101,114,82,78,65,0,119,109,75,101,121,77,97,112,68,105,102,102,73,116,101,109,0,119,109,75,101,121,77,97,112,0,98,111,111,108,0,119,109,75,101,121,67,111,110,102,105,103,80,114,101,102,0,119,109,79,112,101,114,97,116,111,114,84,121,112,101,0,98,84,111,111,108,82,101,102,95,82,117,110,116,105,109,101,0,87,111,114,107,83,112,97,99,101,76,97,121,111,117,
116,0,119,109,79,119,110,101,114,73,68,0,87,111,114,107,83,112,97,99,101,0,87,111,114,107,83,112,97,99,101,82,117,110,116,105,109,101,72,97,110,100,108,101,0,87,111,114,107,83,112,97,99,101,68,97,116,97,82,101,108,97,116,105,111,110,0,88,114,67,111,109,112,111,110,101,110,116,80,97,116,104,0,88,114,65,99,116,105,111,110,77,97,112,66,105,110,100,105,110,103,0,88,114,85,115,101,114,80,97,116,104,0,88,114,65,99,116,105,111,
110,77,97,112,73,116,101,109,0,88,114,65,99,116,105,111,110,77,97,112,0,0,0,0,84,76,69,78,1,0,1,0,2,0,2,0,4,0,4,0,4,0,4,0,8,0,8,0,8,0,0,0,1,0,16,0,0,0,16,0,32,0,64,0,32,0,80,0,24,0,24,0,32,0,16,0,136,0,64,0,208,0,48,0,56,0,0,0,16,0,32,0,0,0,0,9,152,0,68,4,40,4,0,0,0,0,16,0,48,0,0,0,16,0,88,0,0,0,0,0,
32,0,32,0,168,0,8,0,100,0,0,0,248,3,216,1,136,4,0,0,24,0,136,0,0,0,4,0,40,0,120,0,16,0,80,1,88,0,76,0,112,0,120,1,8,0,80,1,40,0,152,0,120,0,248,0,16,0,32,0,16,0,120,0,128,0,24,0,24,0,16,0,24,0,8,0,16,0,24,1,24,0,16,0,20,0,20,0,104,0,152,3,48,1,0,0,16,0,72,0,216,0,104,0,112,0,240,4,32,0,176,0,216,0,16,0,16,0,120,0,
160,1,0,0,24,0,24,0,80,0,0,0,16,0,8,0,24,0,24,0,56,0,80,0,64,0,104,0,72,0,64,0,20,0,128,0,104,0,24,0,64,6,0,1,168,1,136,1,48,0,168,8,216,0,0,0,8,3,224,0,28,0,32,0,232,0,76,0,16,16,24,4,112,9,0,0,0,0,24,0,104,0,40,0,8,10,8,0,96,0,216,0,200,2,32,0,16,1,0,0,72,0,72,0,4,0,32,0,32,0,96,0,64,0,80,1,12,0,80,0,
40,20,152,20,160,0,64,0,64,0,56,0,192,0,168,0,112,0,184,0,48,0,24,0,88,0,80,0,80,0,80,0,8,0,80,0,88,0,112,0,80,0,80,0,24,0,104,0,104,0,16,0,144,0,232,0,88,0,28,0,32,0,28,0,88,0,24,0,160,0,16,0,152,0,16,8,0,0,36,0,88,0,8,0,16,0,128,2,0,0,56,1,72,0,0,0,232,4,40,3,40,0,32,2,248,0,0,0,128,0,0,0,0,0,0,4,0,0,40,0,
32,6,96,0,0,0,112,5,232,1,144,0,96,0,184,2,24,0,32,0,136,1,0,0,64,0,80,4,80,1,144,26,240,8,160,0,0,0,0,0,216,0,176,6,56,0,128,0,40,2,32,0,56,0,32,0,16,0,80,0,12,0,120,0,104,0,124,0,16,0,168,0,216,1,16,0,16,0,56,0,152,0,16,0,168,2,192,0,0,0,0,0,0,0,40,0,104,2,104,0,104,1,8,1,80,1,88,0,224,0,24,1,80,1,216,0,80,1,120,1,
80,1,0,0,240,0,96,0,208,0,8,1,232,1,16,1,152,1,80,1,192,0,24,1,112,1,96,1,80,1,72,1,176,1,0,0,48,1,0,0,24,1,8,0,48,2,0,0,16,0,80,2,12,0,24,0,32,0,40,0,64,0,232,0,0,0,0,0,40,0,0,0,24,0,0,0,80,4,32,4,88,0,0,0,24,0,136,0,32,0,0,0,0,0,0,0,8,0,144,0,112,0,184,0,144,1,0,0,48,0,40,0,0,0,64,0,8,0,88,0,
80,0,64,0,0,0,144,1,144,1,32,2,80,1,160,0,160,0,32,0,128,0,48,0,32,0,32,0,8,0,16,0,168,0,16,0,96,0,104,0,112,0,120,0,112,0,120,0,128,0,120,0,128,0,136,0,120,0,120,0,128,0,120,0,120,0,112,0,112,0,120,0,128,0,104,0,112,0,120,0,112,0,112,0,120,0,104,0,104,0,112,0,112,0,120,0,120,0,104,0,104,0,104,0,104,0,120,0,112,0,128,0,104,0,112,0,16,0,24,0,
16,1,184,0,12,0,16,1,224,0,40,0,144,0,40,0,152,0,8,0,8,0,12,0,8,0,16,0,12,0,32,0,64,0,4,0,20,0,0,0,4,0,4,0,0,1,1,0,1,0,8,0,16,0,4,0,16,0,24,0,16,0,1,0,1,0,12,0,4,0,104,0,96,0,56,1,120,0,24,1,152,0,208,0,208,0,136,0,208,0,208,0,168,0,128,0,248,0,112,1,40,1,208,0,192,0,216,0,152,1,208,0,120,1,120,0,224,0,0,0,
0,0,0,0,208,0,0,0,40,0,0,0,160,0,152,0,8,0,8,0,112,1,168,0,48,1,216,0,136,0,136,0,216,4,128,0,216,0,216,0,120,0,104,1,168,0,112,5,0,0,0,0,8,2,184,1,240,1,192,1,144,0,128,0,136,0,200,0,32,0,248,0,176,1,224,4,216,0,208,0,192,0,32,1,232,0,144,8,32,0,16,0,56,1,192,0,8,0,32,0,48,0,8,0,184,0,0,0,160,0,160,0,224,0,168,0,56,1,56,1,
56,1,80,1,56,1,136,1,80,1,48,1,56,1,48,1,88,0,56,1,72,1,96,1,120,1,120,1,120,1,200,1,200,1,48,1,80,0,72,1,56,1,64,1,80,1,160,1,56,1,72,1,8,3,48,0,16,0,0,0,128,1,136,0,64,0,208,0,72,0,168,0,8,0,72,0,48,0,64,0,0,0,48,0,16,2,0,0,56,0,0,0,8,0,104,1,0,0,0,0,4,0,8,0,16,0,0,0,0,0,4,0,0,0,16,0,16,0,1,0,
24,0,12,0,16,0,8,4,8,0,8,0,8,0,8,0,8,0,16,0,0,0,4,0,16,0,24,0,104,0,20,0,24,0,24,0,68,0,40,0,28,0,12,0,20,0,12,0,12,0,80,5,72,1,88,5,112,5,44,0,24,0,8,0,64,0,2,0,32,0,16,0,32,0,32,0,8,0,80,0,20,0,128,0,1,0,8,0,1,0,192,3,144,0,48,3,0,4,0,4,192,3,208,3,248,3,200,3,200,3,216,3,200,3,208,3,200,3,8,1,
16,0,208,4,176,0,4,0,8,0,64,0,68,0,48,0,128,0,4,0,136,0,80,4,72,0,68,0,64,0,64,0,4,4,64,0,12,0,88,0,80,0,40,0,176,0,2,0,8,0,1,0,2,0,1,0,4,0,12,0,12,0,16,0,8,0,1,0,1,0,2,0,1,0,1,0,1,0,1,0,2,0,1,0,1,0,1,0,1,0,2,0,1,0,24,0,24,0,32,0,1,0,1,0,2,0,2,0,1,0,1,0,1,0,1,0,1,0,1,0,
1,0,1,0,1,0,4,0,4,0,4,0,2,0,1,0,1,0,16,0,24,0,2,0,1,0,4,0,2,0,1,0,1,0,2,0,2,0,1,0,16,0,4,0,24,0,16,0,4,0,24,0,4,0,24,0,1,0,4,0,1,0,8,0,24,0,24,0,12,0,232,0,16,0,24,0,224,1,0,0,0,0,0,0,88,0,88,0,16,0,24,0,40,0,0,0,88,0,152,0,0,0,0,1,16,0,16,0,36,0,56,0,56,0,16,0,64,0,40,0,
32,0,200,0,68,0,200,3,0,0,0,0,0,0,0,0,32,0,112,0,0,2,0,0,32,0,88,0,0,0,80,0,32,0,192,0,152,0,112,5,0,17,104,0,8,0,8,0,88,0,152,0,56,0,16,0,176,0,184,0,88,0,24,0,96,0,96,0,96,0,96,0,104,0,40,0,80,0,8,0,168,0,0,0,32,0,40,0,20,0,200,3,16,0,24,0,32,0,216,3,176,3,24,0,56,1,8,0,8,0,16,0,64,0,168,12,0,0,184,1,
0,0,0,0,32,0,4,0,40,0,48,0,32,0,192,0,0,0,0,0,0,0,80,0,72,1,0,0,0,0,120,0,88,0,12,0,16,0,168,0,184,0,16,0,0,0,48,0,0,0,0,0,0,0,120,0,216,0,0,0,24,0,8,0,0,0,24,0,12,1,16,0,32,0,84,0,24,4,136,3,40,0,8,0,184,1,64,5,48,0,88,0,16,0,0,0,0,0,0,0,0,0,12,0,24,0,32,0,16,0,32,0,8,0,120,2,8,0,112,0,
200,0,24,2,24,2,120,0,112,0,128,0,144,0,184,1,128,0,104,0,40,0,0,0,168,0,192,0,152,0,208,0,176,0,200,0,224,0,168,0,168,0,48,0,248,0,0,0,56,1,0,0,24,0,248,0,208,0,8,0,8,0,8,0,24,1,0,0,16,0,40,8,80,8,56,0,0,0,160,0,0,0,168,0,0,0,0,0,8,0,128,41,88,2,0,0,0,6,64,0,168,0,8,0,104,1,0,0,40,0,120,1,112,0,160,1,40,0,40,0,
8,0,40,0,112,0,16,0,0,0,152,0,16,1,40,0,24,0,72,0,96,0,40,0,128,0,64,0,72,0,24,0,168,0,0,1,120,0,32,0,48,0,32,0,232,0,40,0,48,0,16,0,192,3,8,0,160,3,4,0,4,0,216,74,152,0,24,3,104,0,88,0,232,0,152,0,160,1,88,4,144,8,56,0,32,0,8,0,8,0,40,0,24,0,80,3,96,0,8,57,8,0,8,0,12,0,12,0,16,0,64,0,0,0,0,0,160,3,0,0,
0,0,96,0,32,0,88,5,32,0,40,0,40,0,40,0,40,0,40,0,40,0,32,0,16,0,144,5,0,0,48,0,0,0,240,3,0,0,232,3,176,5,120,1,168,0,0,0,0,0,0,0,32,0,0,0,0,0,0,0,184,0,0,0,32,0,208,0,0,0,88,0,0,0,0,0,88,0,80,0,72,1,0,0,40,0,208,0,128,1,80,0,32,1,104,0,83,84,82,67,148,3,0,0,13,0,2,0,14,0,0,0,14,0,1,0,15,0,3,0,
0,0,2,0,4,0,3,0,0,0,4,0,16,0,5,0,0,0,5,0,0,0,6,0,0,0,2,0,4,0,7,0,4,0,8,0,17,0,11,0,15,0,9,0,4,0,10,0,4,0,11,0,4,0,12,0,4,0,13,0,4,0,14,0,4,0,15,0,4,0,16,0,4,0,17,0,4,0,18,0,16,0,19,0,18,0,5,0,15,0,9,0,12,0,10,0,4,0,11,0,0,0,20,0,12,0,17,0,19,0,11,0,15,0,9,0,8,0,10,0,
4,0,11,0,0,0,4,0,7,0,16,0,4,0,21,0,8,0,12,0,8,0,13,0,8,0,14,0,8,0,15,0,8,0,17,0,20,0,2,0,15,0,9,0,0,0,22,0,21,0,3,0,15,0,9,0,2,0,23,0,0,0,24,0,22,0,4,0,11,0,25,0,23,0,26,0,4,0,27,0,4,0,28,0,24,0,11,0,24,0,29,0,24,0,30,0,0,0,31,0,0,0,32,0,2,0,33,0,0,0,34,0,0,0,35,0,22,0,36,0,
4,0,37,0,4,0,38,0,15,0,39,0,25,0,12,0,25,0,29,0,25,0,30,0,2,0,40,0,2,0,33,0,2,0,41,0,0,0,42,0,0,0,43,0,0,0,44,0,4,0,45,0,4,0,46,0,26,0,47,0,26,0,48,0,27,0,7,0,27,0,29,0,27,0,30,0,0,0,49,0,23,0,50,0,2,0,41,0,0,0,51,0,4,0,52,0,28,0,7,0,26,0,53,0,23,0,54,0,26,0,55,0,26,0,56,0,29,0,57,0,
4,0,33,0,0,0,58,0,30,0,4,0,4,0,59,0,4,0,60,0,4,0,61,0,4,0,62,0,31,0,3,0,30,0,63,0,32,0,64,0,11,0,65,0,26,0,20,0,11,0,29,0,11,0,30,0,26,0,66,0,33,0,67,0,34,0,68,0,0,0,69,0,2,0,33,0,4,0,41,0,4,0,70,0,4,0,71,0,4,0,72,0,4,0,73,0,4,0,74,0,4,0,75,0,24,0,76,0,28,0,77,0,26,0,78,0,11,0,79,0,
35,0,80,0,31,0,81,0,36,0,9,0,37,0,82,0,38,0,83,0,0,0,84,0,33,0,85,0,3,0,41,0,0,0,24,0,4,0,86,0,2,0,87,0,2,0,88,0,33,0,4,0,26,0,89,0,0,0,90,0,39,0,91,0,36,0,81,0,35,0,3,0,0,0,92,0,0,0,93,0,0,0,51,0,40,0,6,0,4,0,94,0,4,0,95,0,2,0,96,0,2,0,97,0,4,0,98,0,41,0,57,0,42,0,2,0,7,0,99,0,
4,0,33,0,43,0,13,0,42,0,100,0,4,0,101,0,4,0,102,0,4,0,103,0,7,0,104,0,7,0,105,0,4,0,106,0,4,0,33,0,0,0,107,0,44,0,108,0,45,0,109,0,45,0,110,0,11,0,65,0,46,0,11,0,2,0,72,0,2,0,111,0,2,0,112,0,2,0,113,0,2,0,114,0,2,0,115,0,0,0,4,0,4,0,116,0,4,0,117,0,4,0,118,0,4,0,119,0,47,0,4,0,7,0,120,0,7,0,121,0,
7,0,122,0,7,0,123,0,48,0,11,0,49,0,75,0,50,0,124,0,4,0,125,0,7,0,126,0,0,0,127,0,51,0,128,0,51,0,129,0,51,0,130,0,50,0,131,0,47,0,132,0,11,0,65,0,52,0,69,0,52,0,29,0,52,0,30,0,24,0,133,0,23,0,134,0,0,0,34,0,2,0,33,0,2,0,135,0,2,0,136,0,2,0,137,0,0,0,138,0,0,0,139,0,0,0,140,0,0,0,141,0,0,0,35,0,53,0,142,0,
52,0,85,0,52,0,143,0,23,0,144,0,23,0,145,0,43,0,146,0,54,0,147,0,52,0,148,0,7,0,149,0,7,0,150,0,7,0,151,0,7,0,152,0,7,0,153,0,7,0,154,0,7,0,155,0,7,0,156,0,7,0,157,0,7,0,158,0,7,0,159,0,2,0,160,0,0,0,24,0,7,0,161,0,7,0,162,0,7,0,163,0,7,0,164,0,7,0,165,0,7,0,166,0,7,0,167,0,7,0,168,0,7,0,169,0,7,0,170,0,
7,0,171,0,7,0,172,0,7,0,173,0,7,0,174,0,7,0,175,0,7,0,176,0,7,0,177,0,7,0,178,0,7,0,179,0,7,0,180,0,7,0,181,0,7,0,182,0,7,0,183,0,7,0,184,0,7,0,185,0,7,0,186,0,7,0,187,0,52,0,188,0,52,0,189,0,11,0,190,0,55,0,191,0,52,0,192,0,56,0,193,0,48,0,81,0,57,0,14,0,23,0,194,0,58,0,195,0,52,0,196,0,2,0,33,0,0,0,51,0,
7,0,197,0,7,0,198,0,7,0,199,0,23,0,200,0,4,0,201,0,4,0,202,0,11,0,203,0,11,0,204,0,46,0,205,0,59,0,1,0,4,0,202,0,60,0,12,0,4,0,202,0,7,0,21,0,2,0,206,0,2,0,207,0,7,0,208,0,7,0,209,0,2,0,210,0,2,0,33,0,7,0,211,0,7,0,212,0,7,0,213,0,7,0,214,0,61,0,7,0,61,0,29,0,61,0,30,0,23,0,215,0,4,0,33,0,4,0,216,0,
0,0,34,0,62,0,217,0,63,0,18,0,26,0,89,0,64,0,218,0,4,0,219,0,4,0,220,0,65,0,221,0,4,0,222,0,4,0,223,0,23,0,224,0,23,0,194,0,23,0,225,0,23,0,226,0,4,0,33,0,4,0,227,0,4,0,228,0,0,0,4,0,7,0,229,0,7,0,230,0,40,0,231,0,66,0,8,0,26,0,232,0,23,0,194,0,67,0,233,0,0,0,234,0,4,0,235,0,4,0,236,0,4,0,33,0,4,0,237,0,
68,0,2,0,0,0,33,0,0,0,238,0,69,0,17,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,71,0,243,0,63,0,244,0,66,0,245,0,7,0,246,0,2,0,33,0,0,0,247,0,0,0,248,0,0,0,249,0,0,0,250,0,0,0,251,0,68,0,81,0,72,0,8,0,72,0,29,0,72,0,30,0,61,0,252,0,73,0,253,0,23,0,254,0,4,0,33,0,0,0,34,0,4,0,255,0,
64,0,8,0,0,0,34,0,7,0,0,1,1,0,1,1,12,0,2,1,1,0,42,0,74,0,3,1,4,0,4,1,1,0,127,0,65,0,4,0,0,0,69,0,1,0,42,0,4,0,5,1,4,0,6,1,74,0,5,0,12,0,7,1,1,0,8,1,7,0,229,0,7,0,230,0,7,0,9,1,75,0,4,0,74,0,10,1,76,0,11,1,4,0,12,1,1,0,4,0,76,0,3,0,4,0,13,1,4,0,14,1,77,0,15,1,78,0,14,0,
78,0,29,0,78,0,30,0,77,0,16,1,11,0,17,1,0,0,34,0,2,0,31,0,2,0,33,0,2,0,18,1,0,0,24,0,7,0,0,1,7,0,19,1,7,0,20,1,7,0,21,1,7,0,22,1,79,0,5,0,7,0,23,1,4,0,24,1,4,0,25,1,4,0,247,0,4,0,33,0,80,0,6,0,7,0,26,1,7,0,27,1,7,0,28,1,7,0,29,1,4,0,31,0,4,0,33,0,81,0,5,0,7,0,12,0,7,0,13,0,
7,0,30,1,2,0,31,1,2,0,32,1,82,0,5,0,81,0,17,1,4,0,33,1,7,0,34,1,7,0,12,0,7,0,13,0,83,0,4,0,2,0,35,1,2,0,36,1,2,0,37,1,2,0,38,1,84,0,2,0,85,0,39,1,24,0,133,0,86,0,3,0,87,0,40,1,4,0,33,0,0,0,4,0,88,0,6,0,7,0,41,1,7,0,42,1,7,0,43,1,7,0,44,1,2,0,45,1,2,0,46,1,89,0,5,0,7,0,47,1,
7,0,44,1,7,0,102,0,7,0,103,0,4,0,33,0,90,0,11,0,26,0,48,1,0,0,49,0,0,0,49,1,2,0,50,1,0,0,51,1,0,0,52,1,2,0,33,0,2,0,53,1,4,0,5,1,4,0,54,1,7,0,55,1,91,0,8,0,91,0,29,0,91,0,30,0,0,0,34,0,90,0,56,1,0,0,57,1,0,0,31,0,2,0,33,0,7,0,58,1,92,0,8,0,23,0,59,1,0,0,60,1,11,0,61,1,93,0,62,1,
7,0,58,1,7,0,0,1,4,0,31,0,4,0,33,0,94,0,3,0,7,0,63,1,4,0,33,0,0,0,4,0,77,0,20,0,77,0,29,0,77,0,30,0,61,0,252,0,92,0,64,1,23,0,65,1,95,0,66,1,94,0,67,1,4,0,33,1,4,0,68,1,7,0,58,1,2,0,33,0,2,0,69,1,0,0,70,1,0,0,20,0,4,0,71,1,0,0,49,0,4,0,72,1,7,0,104,0,7,0,73,1,7,0,74,1,96,0,26,0,
96,0,29,0,96,0,30,0,23,0,75,1,63,0,76,1,23,0,77,1,23,0,65,1,0,0,34,0,7,0,0,1,7,0,78,1,7,0,79,1,7,0,80,1,7,0,81,1,7,0,82,1,7,0,83,1,7,0,84,1,7,0,21,1,7,0,22,1,2,0,85,1,2,0,86,1,0,0,87,1,2,0,31,0,11,0,88,1,4,0,33,0,0,0,107,0,96,0,89,1,11,0,90,1,97,0,6,0,97,0,29,0,97,0,30,0,23,0,75,1,
4,0,33,0,4,0,91,1,0,0,34,0,98,0,11,0,98,0,29,0,98,0,30,0,26,0,48,1,0,0,92,1,4,0,5,1,2,0,93,1,2,0,33,0,0,0,49,0,4,0,71,1,2,0,94,1,2,0,95,1,99,0,12,0,99,0,29,0,99,0,30,0,23,0,96,1,0,0,97,1,0,0,34,0,0,0,98,1,0,0,99,1,4,0,100,1,2,0,33,0,2,0,94,1,2,0,95,1,0,0,24,0,100,0,5,0,100,0,29,0,
100,0,30,0,0,0,49,0,4,0,71,1,7,0,7,0,101,0,16,0,63,0,244,0,4,0,13,1,0,0,101,1,1,0,42,0,63,0,102,1,23,0,103,1,97,0,104,1,96,0,105,1,23,0,106,1,23,0,107,1,77,0,108,1,4,0,33,0,2,0,109,1,2,0,110,1,7,0,111,1,1,0,127,0,102,0,2,0,26,0,89,0,101,0,112,1,56,0,3,0,12,0,113,1,1,0,238,0,62,0,114,1,103,0,1,0,23,0,115,1,
53,0,53,0,53,0,29,0,53,0,30,0,24,0,133,0,53,0,85,0,23,0,116,1,0,0,34,0,7,0,117,1,7,0,118,1,7,0,119,1,7,0,120,1,4,0,33,0,0,0,127,0,56,0,193,0,0,0,121,1,0,0,20,0,7,0,122,1,7,0,123,1,7,0,124,1,7,0,125,1,7,0,126,1,7,0,127,1,7,0,128,1,7,0,101,0,7,0,129,1,7,0,130,1,7,0,131,1,7,0,174,0,7,0,175,0,7,0,176,0,
7,0,177,0,7,0,178,0,7,0,179,0,7,0,180,0,7,0,181,0,7,0,182,0,7,0,183,0,7,0,184,0,7,0,185,0,7,0,186,0,7,0,187,0,7,0,155,0,4,0,132,1,2,0,133,1,0,0,134,1,0,0,135,1,0,0,136,1,0,0,137,1,4,0,138,1,2,0,139,1,2,0,140,1,53,0,188,0,53,0,189,0,103,0,81,0,104,0,3,0,4,0,141,1,1,0,35,0,105,0,142,1,106,0,24,0,26,0,89,0,
101,0,112,1,23,0,143,1,58,0,144,1,11,0,145,1,23,0,146,1,53,0,147,1,107,0,148,1,0,0,149,1,0,0,8,1,4,0,33,0,4,0,150,1,2,0,151,1,2,0,152,1,23,0,115,1,105,0,153,1,4,0,154,1,4,0,155,1,0,0,156,1,4,0,157,1,4,0,132,1,4,0,158,1,7,0,159,1,104,0,81,0,105,0,9,0,105,0,29,0,105,0,30,0,0,0,34,0,23,0,160,1,1,0,161,1,1,0,238,0,
4,0,162,1,4,0,163,1,24,0,133,0,108,0,3,0,108,0,29,0,108,0,30,0,53,0,142,0,109,0,3,0,109,0,29,0,109,0,30,0,105,0,164,1,110,0,3,0,110,0,29,0,110,0,30,0,0,0,34,0,34,0,12,0,111,0,165,1,24,0,76,0,112,0,166,1,0,0,167,1,0,0,168,1,0,0,2,0,0,0,169,1,0,0,170,1,23,0,171,1,2,0,172,1,2,0,173,1,0,0,4,0,113,0,3,0,2,0,31,0,
0,0,87,1,4,0,174,1,114,0,4,0,0,0,24,0,2,0,175,1,0,0,176,1,0,0,177,1,115,0,3,0,115,0,29,0,115,0,30,0,0,0,178,1,116,0,5,0,116,0,29,0,116,0,30,0,4,0,31,0,4,0,33,0,0,0,179,1,117,0,6,0,116,0,180,1,54,0,181,1,4,0,53,1,7,0,182,1,4,0,183,1,4,0,215,0,118,0,3,0,116,0,180,1,4,0,53,1,7,0,184,1,119,0,8,0,116,0,180,1,
54,0,181,1,7,0,154,0,7,0,185,1,7,0,186,1,7,0,187,1,4,0,53,1,4,0,188,1,120,0,5,0,116,0,180,1,7,0,189,1,7,0,190,1,7,0,191,1,0,0,35,0,121,0,3,0,116,0,180,1,7,0,187,1,7,0,192,1,122,0,4,0,7,0,193,1,7,0,194,1,2,0,195,1,2,0,247,0,123,0,14,0,123,0,29,0,123,0,30,0,23,0,196,1,23,0,197,1,23,0,198,1,0,0,179,1,4,0,89,0,
4,0,33,0,4,0,199,1,7,0,200,1,4,0,183,1,4,0,215,0,7,0,201,1,7,0,202,1,124,0,23,0,4,0,53,1,4,0,203,1,7,0,204,1,7,0,205,1,7,0,206,1,7,0,207,1,7,0,193,1,7,0,208,1,7,0,42,1,7,0,209,1,7,0,210,1,7,0,211,1,7,0,212,1,7,0,213,1,7,0,214,1,7,0,215,1,7,0,216,1,7,0,217,1,7,0,218,1,7,0,219,1,7,0,220,1,7,0,221,1,
23,0,222,1,125,0,4,0,126,0,223,1,7,0,224,1,7,0,225,1,0,0,4,0,127,0,56,0,7,0,226,1,7,0,227,1,7,0,228,1,7,0,229,1,7,0,230,1,7,0,231,1,7,0,232,1,7,0,233,1,2,0,234,1,2,0,235,1,2,0,236,1,2,0,237,1,7,0,238,1,0,0,239,1,12,0,240,1,0,0,52,1,4,0,241,1,4,0,242,1,4,0,243,1,4,0,244,1,4,0,71,0,4,0,245,1,7,0,246,1,
4,0,247,1,4,0,248,1,7,0,249,1,7,0,250,1,7,0,251,1,4,0,33,0,7,0,252,1,7,0,253,1,7,0,254,1,7,0,255,1,4,0,0,2,4,0,1,2,4,0,2,2,2,0,3,2,2,0,4,2,7,0,5,2,7,0,6,2,7,0,7,2,7,0,8,2,4,0,9,2,128,0,10,2,128,0,11,2,128,0,12,2,128,0,13,2,128,0,14,2,128,0,15,2,128,0,16,2,128,0,17,2,128,0,18,2,7,0,19,2,
0,0,127,0,129,0,20,2,129,0,21,2,130,0,11,0,4,0,22,2,4,0,23,2,4,0,33,0,7,0,24,2,7,0,25,2,7,0,26,2,7,0,27,2,4,0,28,2,1,0,29,2,0,0,30,2,128,0,31,2,131,0,125,0,26,0,89,0,125,0,32,2,128,0,16,1,132,0,33,2,132,0,34,2,131,0,35,2,133,0,36,2,40,0,231,0,134,0,37,2,135,0,38,2,0,0,39,2,7,0,40,2,7,0,41,2,2,0,42,2,
2,0,43,2,7,0,127,1,4,0,41,1,4,0,33,0,4,0,241,1,4,0,44,2,4,0,245,1,4,0,45,2,7,0,46,2,4,0,47,2,4,0,48,2,4,0,49,2,4,0,50,2,7,0,51,2,7,0,52,2,7,0,53,2,7,0,225,1,7,0,54,2,7,0,55,2,7,0,56,2,7,0,57,2,7,0,58,2,4,0,59,2,7,0,60,2,7,0,61,2,7,0,62,2,7,0,63,2,4,0,64,2,4,0,65,2,7,0,122,0,
4,0,66,2,0,0,67,2,0,0,68,2,0,0,69,2,0,0,70,2,7,0,71,2,0,0,72,2,0,0,73,2,0,0,74,2,0,0,75,2,0,0,76,2,0,0,77,2,0,0,78,2,0,0,79,2,0,0,80,2,0,0,81,2,0,0,251,0,7,0,82,2,7,0,83,2,7,0,84,2,7,0,85,2,7,0,86,2,7,0,87,2,7,0,88,2,7,0,89,2,7,0,205,1,7,0,90,2,4,0,91,2,7,0,92,2,4,0,93,2,
4,0,94,2,4,0,95,2,7,0,96,2,7,0,97,2,7,0,98,2,7,0,99,2,4,0,100,2,7,0,101,2,4,0,102,2,4,0,103,2,7,0,104,2,4,0,105,2,4,0,106,2,4,0,107,2,4,0,108,2,4,0,109,2,7,0,110,2,4,0,111,2,4,0,112,2,4,0,113,2,7,0,114,2,7,0,115,2,7,0,116,2,7,0,117,2,7,0,118,2,4,0,119,2,7,0,120,2,7,0,121,2,4,0,122,2,7,0,123,2,
4,0,124,2,4,0,125,2,4,0,126,2,4,0,127,2,4,0,128,2,7,0,129,2,7,0,130,2,4,0,131,2,4,0,132,2,7,0,238,1,7,0,133,2,7,0,134,2,7,0,135,2,7,0,136,2,7,0,137,2,7,0,138,2,127,0,139,2,130,0,140,2,4,0,141,2,7,0,142,2,128,0,143,2,136,0,5,0,7,0,53,2,7,0,7,0,7,0,144,2,7,0,145,2,7,0,146,2,137,0,4,0,137,0,29,0,137,0,30,0,
7,0,53,2,7,0,7,0,138,0,4,0,26,0,89,0,23,0,147,2,4,0,148,2,0,0,4,0,139,0,2,0,95,0,149,2,7,0,150,2,135,0,4,0,26,0,89,0,139,0,100,0,4,0,151,2,4,0,152,2,140,0,3,0,140,0,29,0,140,0,30,0,0,0,153,2,141,0,5,0,141,0,29,0,141,0,30,0,0,0,154,2,4,0,33,0,4,0,155,2,142,0,26,0,26,0,89,0,101,0,112,1,23,0,156,2,23,0,157,2,
0,0,154,2,0,0,158,2,0,0,159,2,0,0,160,2,0,0,161,2,7,0,84,1,7,0,162,2,7,0,9,1,0,0,4,0,2,0,33,0,0,0,31,0,0,0,163,2,0,0,164,2,0,0,165,2,4,0,166,2,4,0,167,2,0,0,168,2,0,0,169,2,0,0,170,2,143,0,171,2,0,0,172,2,144,0,173,2,145,0,8,0,7,0,174,2,7,0,175,2,2,0,176,2,2,0,177,2,2,0,33,0,0,0,51,0,7,0,178,2,
7,0,179,2,146,0,12,0,146,0,29,0,146,0,30,0,126,0,180,2,147,0,181,2,148,0,182,2,149,0,183,2,7,0,224,1,7,0,84,1,7,0,184,2,7,0,225,1,2,0,33,0,2,0,185,2,150,0,9,0,54,0,186,2,0,0,187,2,7,0,188,2,7,0,189,2,7,0,190,2,7,0,191,2,4,0,192,2,2,0,33,0,0,0,51,0,151,0,5,0,7,0,193,2,7,0,194,2,7,0,195,2,7,0,196,2,7,0,197,2,
152,0,37,0,26,0,89,0,101,0,112,1,0,0,31,0,0,0,198,2,2,0,33,0,7,0,199,2,7,0,200,2,7,0,201,2,7,0,202,2,7,0,203,2,7,0,204,2,7,0,205,2,7,0,206,2,7,0,207,2,7,0,208,2,7,0,209,2,0,0,210,2,0,0,211,2,0,0,51,0,7,0,212,2,7,0,213,2,7,0,214,2,7,0,215,2,7,0,216,2,7,0,217,2,7,0,218,2,7,0,219,2,7,0,220,2,7,0,221,2,
7,0,222,2,73,0,253,0,54,0,223,2,153,0,224,2,150,0,225,2,23,0,226,2,145,0,227,2,151,0,81,0,154,0,70,0,155,0,228,2,7,0,229,2,7,0,230,2,7,0,231,2,7,0,232,2,7,0,233,2,7,0,234,2,7,0,235,2,7,0,236,2,7,0,237,2,7,0,238,2,7,0,239,2,7,0,240,2,7,0,241,2,7,0,242,2,7,0,243,2,7,0,244,2,7,0,245,2,7,0,246,2,7,0,247,2,7,0,248,2,
7,0,249,2,7,0,250,2,7,0,251,2,7,0,252,2,7,0,253,2,7,0,254,2,7,0,255,2,7,0,0,3,7,0,1,3,7,0,2,3,7,0,3,3,7,0,4,3,7,0,5,3,7,0,6,3,2,0,7,3,0,0,8,3,7,0,9,3,7,0,10,3,4,0,11,3,4,0,161,1,4,0,12,3,4,0,13,3,2,0,14,3,2,0,15,3,2,0,16,3,2,0,17,3,2,0,18,3,2,0,19,3,2,0,20,3,2,0,21,3,
156,0,22,3,2,0,23,3,2,0,24,3,7,0,25,3,7,0,26,3,7,0,27,3,7,0,28,3,7,0,29,3,7,0,30,3,7,0,31,3,7,0,32,3,7,0,33,3,2,0,34,3,0,0,87,1,7,0,35,3,7,0,36,3,7,0,37,3,7,0,38,3,0,0,35,0,157,0,18,0,155,0,39,3,7,0,40,3,7,0,41,3,7,0,42,3,7,0,43,3,7,0,44,3,7,0,45,3,7,0,46,3,4,0,161,1,2,0,47,3,
2,0,48,3,0,0,4,0,67,0,49,3,2,0,50,3,2,0,51,3,0,0,107,0,7,0,52,3,7,0,53,3,158,0,2,0,1,0,54,3,1,0,20,0,159,0,5,0,159,0,29,0,159,0,30,0,54,0,181,1,158,0,55,3,4,0,155,2,160,0,5,0,160,0,29,0,160,0,30,0,67,0,56,3,158,0,55,3,4,0,155,2,161,0,6,0,161,0,29,0,161,0,30,0,0,0,57,3,24,0,58,3,4,0,33,0,4,0,69,2,
162,0,6,0,23,0,59,3,23,0,60,3,23,0,61,3,58,0,62,3,1,0,41,0,0,0,238,0,67,0,19,0,26,0,89,0,26,0,63,3,23,0,64,3,23,0,65,3,0,0,35,0,4,0,66,3,23,0,67,3,40,0,231,0,4,0,132,1,7,0,68,3,1,0,33,0,12,0,69,3,0,0,87,1,1,0,70,3,1,0,71,3,1,0,72,3,1,0,73,3,163,0,74,3,162,0,81,0,164,0,4,0,7,0,75,3,7,0,76,3,
2,0,33,0,2,0,77,3,165,0,14,0,2,0,78,3,2,0,33,0,7,0,210,1,7,0,79,3,7,0,80,3,7,0,81,3,7,0,82,3,164,0,16,1,164,0,83,3,164,0,84,3,7,0,85,3,7,0,86,3,2,0,87,3,0,0,24,0,128,0,13,0,4,0,33,0,4,0,88,3,4,0,89,3,4,0,90,3,87,0,91,3,87,0,92,3,165,0,93,3,7,0,94,3,7,0,95,3,7,0,96,3,7,0,97,3,2,0,98,3,
0,0,24,0,166,0,13,0,4,0,215,0,4,0,99,3,7,0,100,3,7,0,101,3,7,0,102,3,7,0,103,3,7,0,104,3,7,0,105,3,7,0,106,3,2,0,247,0,2,0,33,0,4,0,205,1,7,0,107,3,167,0,19,0,4,0,108,3,4,0,109,3,4,0,110,3,4,0,111,3,4,0,112,3,4,0,113,3,4,0,114,3,4,0,115,3,7,0,209,1,7,0,116,3,7,0,117,3,7,0,118,3,7,0,119,3,166,0,120,3,
7,0,121,3,7,0,122,3,7,0,123,3,7,0,124,3,7,0,125,3,168,0,8,0,4,0,33,0,0,0,4,0,0,0,126,3,0,0,127,3,7,0,128,3,7,0,129,3,128,0,130,3,11,0,131,3,169,0,1,0,0,0,132,3,170,0,1,0,0,0,34,0,171,0,5,0,171,0,29,0,171,0,30,0,73,0,253,0,2,0,33,0,0,0,133,3,172,0,16,0,172,0,29,0,172,0,30,0,11,0,17,1,2,0,31,0,2,0,33,0,
0,0,134,3,0,0,135,3,2,0,18,1,54,0,136,3,0,0,137,3,0,0,34,0,7,0,138,3,7,0,139,3,73,0,253,0,7,0,140,3,7,0,141,3,173,0,11,0,173,0,29,0,173,0,30,0,54,0,142,3,0,0,143,3,7,0,144,3,2,0,145,3,2,0,33,0,2,0,31,0,2,0,146,3,7,0,127,1,0,0,4,0,174,0,7,0,85,0,147,3,24,0,133,0,4,0,33,0,4,0,148,3,23,0,149,3,54,0,142,3,
0,0,143,3,175,0,15,0,54,0,142,3,2,0,150,3,2,0,33,0,2,0,151,3,2,0,152,3,0,0,143,3,54,0,153,3,0,0,154,3,7,0,155,3,7,0,127,1,7,0,156,3,7,0,157,3,2,0,31,0,2,0,247,0,7,0,126,1,176,0,12,0,54,0,142,3,7,0,100,0,2,0,158,3,2,0,159,3,2,0,33,0,2,0,160,3,2,0,161,3,2,0,20,0,7,0,162,3,7,0,163,3,7,0,164,3,7,0,165,3,
177,0,3,0,4,0,33,0,0,0,4,0,23,0,149,3,178,0,6,0,54,0,142,3,4,0,166,3,4,0,167,3,4,0,161,1,0,0,4,0,0,0,143,3,179,0,6,0,54,0,142,3,4,0,33,0,0,0,168,3,0,0,169,3,0,0,51,0,0,0,143,3,180,0,4,0,54,0,142,3,4,0,33,0,4,0,166,3,0,0,143,3,181,0,4,0,54,0,142,3,4,0,33,0,7,0,170,3,0,0,143,3,182,0,4,0,0,0,33,0,
0,0,247,0,0,0,51,0,7,0,201,1,183,0,5,0,54,0,142,3,4,0,33,0,0,0,169,3,0,0,20,0,0,0,143,3,184,0,6,0,54,0,142,3,4,0,171,3,7,0,44,1,4,0,33,0,0,0,143,3,4,0,155,2,185,0,13,0,54,0,142,3,2,0,31,0,2,0,172,3,4,0,79,1,4,0,80,1,7,0,12,0,7,0,13,0,4,0,33,0,0,0,169,3,0,0,20,0,7,0,173,3,63,0,76,1,0,0,143,3,
186,0,4,0,54,0,142,3,4,0,174,3,4,0,175,3,0,0,143,3,187,0,4,0,54,0,142,3,4,0,174,3,0,0,4,0,0,0,143,3,188,0,6,0,54,0,142,3,7,0,44,1,7,0,176,3,4,0,177,3,2,0,174,3,2,0,178,3,189,0,10,0,54,0,142,3,4,0,33,0,4,0,179,3,4,0,180,3,7,0,181,3,7,0,162,3,7,0,163,3,7,0,164,3,7,0,165,3,0,0,143,3,190,0,14,0,54,0,142,3,
54,0,143,0,4,0,31,0,7,0,182,3,7,0,183,3,7,0,184,3,7,0,185,3,7,0,186,3,7,0,187,3,7,0,188,3,7,0,189,3,7,0,190,3,2,0,33,0,0,0,24,0,191,0,3,0,54,0,142,3,4,0,33,0,4,0,241,1,192,0,5,0,54,0,142,3,4,0,33,0,0,0,4,0,7,0,191,3,0,0,143,3,193,0,24,0,54,0,142,3,0,0,143,3,2,0,192,3,2,0,193,3,0,0,194,3,0,0,195,3,
0,0,196,3,0,0,197,3,0,0,198,3,0,0,199,3,0,0,200,3,0,0,20,0,7,0,201,3,7,0,202,3,7,0,203,3,7,0,204,3,7,0,205,3,7,0,206,3,7,0,207,3,7,0,208,3,7,0,209,3,7,0,210,3,7,0,211,3,7,0,212,3,194,0,5,0,54,0,142,3,0,0,143,3,7,0,213,3,2,0,214,3,2,0,33,0,195,0,8,0,7,0,215,3,7,0,105,3,7,0,216,3,7,0,106,3,7,0,217,3,
7,0,218,3,2,0,33,0,2,0,241,1,196,0,10,0,7,0,215,3,7,0,105,3,7,0,216,3,7,0,106,3,7,0,217,3,7,0,218,3,2,0,33,0,2,0,241,1,0,0,168,3,0,0,20,0,197,0,8,0,7,0,215,3,7,0,105,3,7,0,216,3,7,0,106,3,7,0,217,3,7,0,218,3,2,0,33,0,2,0,241,1,198,0,7,0,54,0,142,3,0,0,143,3,7,0,126,1,7,0,219,3,2,0,33,0,2,0,247,0,
0,0,4,0,199,0,10,0,54,0,220,3,7,0,126,1,2,0,221,3,0,0,222,3,0,0,223,3,7,0,224,3,0,0,225,3,0,0,33,0,0,0,226,3,0,0,155,2,200,0,7,0,148,0,182,2,0,0,227,3,4,0,33,0,4,0,228,3,0,0,229,3,54,0,230,3,54,0,231,3,201,0,3,0,148,0,182,2,4,0,33,0,0,0,4,0,202,0,6,0,148,0,182,2,4,0,33,0,0,0,4,0,0,0,229,3,7,0,191,3,
54,0,230,3,203,0,4,0,142,0,232,3,0,0,233,3,204,0,234,3,0,0,235,3,95,0,17,0,7,0,236,3,7,0,237,3,7,0,127,1,7,0,238,3,0,0,239,3,1,0,240,3,1,0,241,3,1,0,31,1,1,0,32,1,1,0,242,3,0,0,243,3,0,0,244,3,7,0,245,3,7,0,26,1,7,0,246,3,0,0,247,3,0,0,20,0,205,0,8,0,7,0,248,3,7,0,237,3,7,0,127,1,1,0,31,1,0,0,249,3,
2,0,243,3,7,0,238,3,0,0,4,0,206,0,22,0,206,0,29,0,206,0,30,0,2,0,31,0,2,0,250,3,2,0,243,3,2,0,33,0,4,0,251,3,4,0,252,3,0,0,4,0,2,0,253,3,2,0,254,3,2,0,255,3,2,0,0,4,2,0,1,4,2,0,2,4,7,0,3,4,7,0,4,4,205,0,5,4,95,0,66,1,2,0,6,4,2,0,7,4,4,0,8,4,207,0,4,0,7,0,9,4,2,0,250,3,0,0,33,0,
0,0,10,4,208,0,4,0,7,0,75,3,7,0,76,3,7,0,11,4,7,0,144,2,209,0,77,0,26,0,89,0,101,0,112,1,23,0,12,4,210,0,13,4,54,0,14,4,54,0,15,4,54,0,16,4,73,0,253,0,211,0,17,4,129,0,18,4,212,0,19,4,7,0,154,0,7,0,155,0,2,0,31,0,0,0,20,4,0,0,238,0,2,0,21,4,7,0,22,4,7,0,23,4,4,0,24,4,2,0,25,4,2,0,26,4,4,0,33,0,
7,0,27,4,7,0,28,4,7,0,29,4,2,0,253,3,2,0,254,3,2,0,30,4,2,0,31,4,4,0,32,4,4,0,33,4,0,0,34,4,0,0,35,4,0,0,36,4,0,0,37,4,0,0,38,4,0,0,155,2,2,0,39,4,7,0,49,2,7,0,40,4,7,0,236,2,7,0,41,4,7,0,42,4,7,0,43,4,7,0,44,4,7,0,45,4,7,0,46,4,7,0,47,4,4,0,48,4,4,0,49,4,4,0,50,4,4,0,51,4,
4,0,37,0,0,0,52,4,213,0,53,4,0,0,54,4,214,0,55,4,214,0,56,4,214,0,57,4,214,0,58,4,208,0,59,4,4,0,60,4,4,0,61,4,207,0,62,4,207,0,63,4,7,0,197,0,7,0,64,4,7,0,65,4,0,0,66,4,0,0,67,4,0,0,68,4,7,0,69,4,215,0,70,4,0,0,71,4,0,0,72,4,11,0,73,4,216,0,9,0,7,0,75,3,7,0,76,3,2,0,33,0,0,0,240,3,0,0,241,3,
7,0,74,4,7,0,75,4,0,0,4,0,212,0,76,4,212,0,10,0,2,0,77,4,2,0,78,4,4,0,89,3,216,0,178,1,216,0,83,3,216,0,79,4,4,0,33,0,4,0,90,3,87,0,80,4,87,0,81,4,217,0,9,0,4,0,82,4,218,0,83,4,218,0,84,4,4,0,85,4,4,0,86,4,23,0,87,4,4,0,88,4,0,0,4,0,219,0,57,0,215,0,13,0,26,0,89,0,101,0,112,1,217,0,89,4,4,0,33,0,
4,0,90,4,129,0,18,4,2,0,26,4,0,0,91,4,0,0,92,4,0,0,4,0,54,0,93,4,0,0,94,4,11,0,73,4,220,0,13,0,4,0,31,0,4,0,44,1,4,0,33,0,4,0,95,4,4,0,96,4,4,0,97,4,4,0,98,4,4,0,99,4,0,0,100,4,0,0,127,0,11,0,17,1,221,0,101,4,222,0,102,4,223,0,1,0,0,0,103,4,218,0,7,0,220,0,104,4,4,0,105,4,4,0,106,4,4,0,107,4,
4,0,108,4,224,0,109,4,223,0,110,4,225,0,5,0,10,0,111,4,10,0,112,4,10,0,113,4,10,0,114,4,10,0,115,4,226,0,47,0,226,0,29,0,226,0,30,0,227,0,116,4,228,0,17,1,67,0,117,4,156,0,22,3,229,0,118,4,23,0,119,4,4,0,120,4,0,0,34,0,2,0,121,4,2,0,31,0,2,0,122,4,2,0,123,4,2,0,124,4,2,0,125,4,4,0,161,1,4,0,126,4,4,0,127,4,4,0,128,4,
4,0,102,0,4,0,103,0,7,0,129,4,230,0,130,4,0,0,131,4,4,0,132,4,4,0,133,4,7,0,134,4,7,0,135,4,7,0,136,4,7,0,137,4,7,0,138,4,7,0,139,4,7,0,140,4,7,0,141,4,7,0,142,4,7,0,143,4,7,0,144,4,7,0,145,4,7,0,146,4,7,0,147,4,7,0,148,4,0,0,107,0,0,0,149,4,0,0,150,4,0,0,151,4,0,0,152,4,227,0,6,0,231,0,153,4,23,0,154,4,
2,0,155,4,2,0,161,1,0,0,4,0,0,0,156,4,232,0,22,0,231,0,153,4,233,0,157,4,4,0,161,1,4,0,158,4,7,0,159,4,7,0,160,4,7,0,161,4,7,0,225,1,7,0,162,4,7,0,163,4,7,0,164,4,7,0,165,4,134,0,166,4,134,0,167,4,2,0,168,4,2,0,169,4,2,0,170,4,0,0,51,0,7,0,171,4,7,0,172,4,7,0,173,4,7,0,174,4,234,0,6,0,234,0,29,0,234,0,30,0,
2,0,31,0,2,0,33,0,2,0,175,4,0,0,42,0,235,0,8,0,235,0,29,0,235,0,30,0,2,0,31,0,2,0,33,0,2,0,175,4,0,0,42,0,7,0,37,0,7,0,19,1,236,0,45,0,236,0,29,0,236,0,30,0,2,0,31,0,2,0,33,0,2,0,175,4,2,0,176,4,2,0,177,4,2,0,178,4,7,0,179,4,7,0,80,1,7,0,180,4,4,0,181,4,4,0,182,4,4,0,183,4,7,0,184,4,7,0,185,4,
7,0,186,4,7,0,187,4,7,0,188,4,7,0,189,4,7,0,190,4,7,0,191,4,7,0,192,4,7,0,193,4,7,0,194,4,0,0,4,0,7,0,195,4,7,0,196,4,2,0,197,4,2,0,198,4,2,0,199,4,2,0,200,4,2,0,201,4,2,0,202,4,2,0,203,4,2,0,204,4,2,0,241,1,2,0,205,4,2,0,206,4,2,0,207,4,0,0,208,4,0,0,209,4,7,0,210,4,237,0,211,4,67,0,49,3,238,0,16,0,
238,0,29,0,238,0,30,0,2,0,31,0,2,0,33,0,2,0,175,4,2,0,176,4,7,0,212,4,7,0,213,4,7,0,205,1,7,0,27,4,7,0,214,4,7,0,191,1,7,0,215,4,7,0,190,4,7,0,216,4,7,0,180,4,239,0,14,0,0,0,217,4,2,0,218,4,2,0,219,4,2,0,220,4,0,0,24,0,240,0,221,4,241,0,222,4,163,0,223,4,11,0,145,1,4,0,224,4,4,0,225,4,10,0,226,4,0,0,227,4,
0,0,103,4,242,0,185,0,243,0,228,4,244,0,229,4,244,0,230,4,11,0,231,4,67,0,232,4,67,0,233,4,67,0,234,4,245,0,235,4,245,0,236,4,245,0,237,4,245,0,238,4,245,0,239,4,245,0,240,4,245,0,241,4,245,0,242,4,245,0,243,4,245,0,244,4,245,0,245,4,245,0,246,4,245,0,247,4,54,0,248,4,156,0,22,3,7,0,249,4,7,0,250,4,7,0,251,4,7,0,252,4,7,0,253,4,7,0,254,4,
4,0,255,4,7,0,0,5,7,0,1,5,7,0,210,4,7,0,2,5,7,0,3,5,7,0,4,5,4,0,5,5,4,0,6,5,4,0,7,5,4,0,8,5,4,0,9,5,7,0,10,5,7,0,84,1,4,0,11,5,7,0,12,5,4,0,13,5,4,0,14,5,7,0,15,5,4,0,16,5,4,0,17,5,4,0,18,5,4,0,161,1,7,0,232,2,4,0,19,5,2,0,31,0,0,0,68,4,7,0,225,1,7,0,20,5,4,0,133,4,
7,0,21,5,7,0,22,5,4,0,23,5,7,0,24,5,7,0,25,5,7,0,26,5,7,0,27,5,7,0,28,5,7,0,29,5,7,0,30,5,7,0,31,5,7,0,32,5,4,0,33,5,4,0,34,5,0,0,35,5,7,0,36,5,4,0,37,5,4,0,38,5,4,0,39,5,7,0,163,4,7,0,40,5,7,0,41,5,7,0,42,5,7,0,43,5,4,0,44,5,2,0,45,5,0,0,46,5,7,0,47,5,0,0,48,5,7,0,49,5,
7,0,50,5,4,0,51,5,7,0,52,5,7,0,53,5,7,0,54,5,4,0,55,5,4,0,56,5,4,0,57,5,2,0,58,5,0,0,59,5,4,0,60,5,4,0,61,5,7,0,62,5,7,0,63,5,7,0,64,5,7,0,65,5,7,0,66,5,7,0,67,5,4,0,68,5,4,0,69,5,7,0,70,5,7,0,71,5,7,0,72,5,7,0,73,5,4,0,74,5,4,0,75,5,0,0,76,5,0,0,77,5,0,0,8,3,7,0,78,5,
4,0,79,5,7,0,80,5,4,0,81,5,2,0,82,5,0,0,83,5,4,0,84,5,4,0,85,5,4,0,86,5,4,0,87,5,4,0,88,5,4,0,89,5,4,0,90,5,4,0,91,5,4,0,92,5,0,0,93,5,0,0,94,5,0,0,95,5,0,0,96,5,0,0,97,5,0,0,156,4,2,0,98,5,0,0,99,5,0,0,100,5,7,0,233,2,7,0,101,5,7,0,102,5,7,0,103,5,7,0,244,2,7,0,104,5,4,0,105,5,
4,0,106,5,7,0,107,5,7,0,108,5,7,0,109,5,7,0,110,5,134,0,111,5,7,0,112,5,7,0,113,5,7,0,114,5,7,0,115,5,0,0,116,5,0,0,117,5,0,0,118,5,0,0,119,5,0,0,120,5,0,0,121,5,0,0,122,5,0,0,123,5,0,0,124,5,0,0,125,5,0,0,126,5,0,0,127,5,0,0,128,5,0,0,129,5,7,0,130,5,4,0,131,5,7,0,132,5,0,0,133,5,0,0,134,5,4,0,135,5,
0,0,136,5,229,0,137,5,23,0,138,5,4,0,139,5,4,0,140,5,0,0,141,5,0,0,142,5,246,0,31,0,243,0,228,4,247,0,143,5,233,0,157,4,230,0,144,5,7,0,145,5,4,0,146,5,7,0,147,5,7,0,148,5,7,0,149,5,7,0,150,5,0,0,127,0,7,0,58,2,7,0,104,0,7,0,151,5,7,0,152,5,7,0,153,5,7,0,154,5,7,0,155,5,4,0,156,5,7,0,157,5,7,0,158,5,0,0,107,0,
0,0,149,4,0,0,35,5,2,0,159,5,2,0,31,0,2,0,160,5,2,0,185,2,2,0,161,5,2,0,162,5,4,0,161,1,248,0,12,0,243,0,228,4,247,0,143,5,7,0,145,5,4,0,146,5,7,0,154,5,4,0,161,1,4,0,156,5,2,0,31,0,0,0,251,0,7,0,147,5,2,0,163,5,0,0,239,1,249,0,14,0,249,0,29,0,249,0,30,0,0,0,34,0,4,0,161,1,4,0,164,5,2,0,165,5,0,0,87,1,
4,0,166,5,4,0,167,5,4,0,168,5,4,0,169,5,0,0,107,0,67,0,49,3,250,0,170,5,251,0,5,0,251,0,29,0,251,0,30,0,85,0,39,1,2,0,171,5,0,0,24,0,252,0,8,0,23,0,172,5,4,0,247,0,4,0,173,5,4,0,161,1,7,0,174,5,7,0,175,5,7,0,176,5,23,0,177,5,253,0,5,0,7,0,75,3,7,0,76,3,7,0,178,5,7,0,179,5,4,0,41,1,254,0,3,0,255,0,180,5,
4,0,181,5,0,0,35,0,255,0,13,0,7,0,75,3,7,0,76,3,7,0,178,5,7,0,150,2,7,0,42,1,7,0,30,1,4,0,33,0,7,0,182,5,7,0,183,5,7,0,184,5,7,0,185,5,0,0,107,0,254,0,81,0,0,1,1,0,4,0,186,5,1,1,7,0,1,1,29,0,1,1,30,0,0,0,187,5,7,0,179,5,7,0,188,5,2,0,33,0,0,0,24,0,2,1,6,0,2,1,29,0,2,1,30,0,23,0,147,2,
0,0,187,5,2,0,33,0,0,0,24,0,3,1,10,0,95,0,189,5,7,0,150,2,7,0,42,1,4,0,190,5,4,0,33,0,7,0,182,5,7,0,183,5,7,0,184,5,7,0,185,5,0,0,4,0,4,1,4,0,3,1,191,5,4,0,192,5,2,0,33,0,0,0,51,0,5,1,9,0,0,0,193,5,7,0,194,5,4,0,195,5,4,0,196,5,4,0,197,5,4,0,198,5,4,0,69,2,6,1,199,5,11,0,131,3,6,1,29,0,
6,1,29,0,6,1,30,0,255,0,100,0,0,1,200,5,4,0,201,5,4,0,202,5,2,0,203,5,2,0,33,0,2,0,51,0,8,0,204,5,0,0,205,5,4,0,250,3,2,0,206,5,7,0,252,1,7,0,253,1,7,0,207,5,7,0,208,5,7,0,209,5,7,0,210,5,7,0,211,5,7,0,212,5,4,0,213,5,0,0,214,5,7,1,215,5,11,0,90,1,7,0,216,5,4,1,217,5,5,1,81,0,11,0,218,5,8,1,3,0,
4,0,219,5,4,0,220,5,9,1,221,5,9,1,7,0,9,1,29,0,9,1,30,0,23,0,222,5,4,0,223,5,2,0,33,0,2,0,224,5,8,1,81,0,10,1,6,0,10,1,29,0,10,1,30,0,0,0,225,5,2,0,33,0,2,0,226,5,0,0,4,0,11,1,3,0,4,0,71,0,0,0,4,0,12,1,227,5,12,1,36,0,12,1,29,0,12,1,30,0,23,0,228,5,9,1,229,5,2,0,33,0,2,0,230,5,7,0,179,5,
7,0,188,5,0,0,231,5,2,0,203,5,2,0,232,5,54,0,85,0,7,0,233,5,0,0,234,5,2,0,235,5,2,0,236,5,7,0,237,5,7,0,238,5,0,0,239,5,4,0,240,5,7,0,241,5,2,0,242,5,2,0,243,5,7,0,244,5,7,0,245,5,0,0,127,0,23,0,246,5,4,0,247,5,0,0,107,0,7,0,248,5,7,0,249,5,7,0,250,5,7,0,251,5,7,0,252,5,0,0,35,5,11,1,81,0,13,1,23,0,
11,0,253,5,44,0,254,5,44,0,255,5,45,0,0,6,6,1,1,6,2,0,2,6,2,0,3,6,2,0,4,6,0,0,87,1,4,0,5,6,4,0,6,6,7,0,216,5,7,0,207,5,7,0,7,6,7,0,8,6,4,0,9,6,4,0,10,6,4,0,11,6,253,0,12,6,131,0,13,6,14,1,14,6,15,1,15,6,16,1,16,6,17,1,6,0,7,0,104,0,7,0,17,6,7,0,224,1,0,0,127,0,4,0,39,4,0,0,4,0,
18,1,32,0,26,0,89,0,101,0,112,1,23,0,157,2,4,0,33,0,4,0,18,6,7,0,19,6,7,0,20,6,23,0,21,6,23,0,87,4,7,0,22,6,7,0,23,6,7,0,24,6,4,0,25,6,4,0,230,5,2,0,242,5,2,0,243,5,7,0,244,5,7,0,245,5,7,0,26,6,129,0,18,4,2,0,26,4,2,0,106,4,2,0,27,6,0,0,68,4,4,0,28,6,4,0,78,3,2,0,29,6,2,0,30,6,4,0,31,6,
4,0,88,4,17,1,32,6,13,1,81,0,19,1,9,0,19,1,29,0,19,1,30,0,4,0,31,0,4,0,247,0,0,0,35,0,2,0,33,0,2,0,18,1,0,0,34,0,0,0,33,6,20,1,19,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,38,6,7,0,39,6,7,0,40,6,7,0,41,6,7,0,34,5,7,0,42,6,2,0,43,6,0,0,51,0,4,0,16,0,
4,0,44,6,4,0,183,4,128,0,45,6,21,1,10,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,4,0,232,5,4,0,33,0,4,0,190,1,4,0,44,6,2,0,31,0,0,0,24,0,22,1,12,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,46,6,4,0,203,5,4,0,44,6,0,0,4,0,128,0,47,6,23,1,6,0,0,0,34,0,24,1,48,6,
4,0,49,6,4,0,50,6,4,0,51,6,4,0,52,6,24,1,14,0,19,1,34,6,129,0,20,2,0,0,35,6,4,0,44,6,4,0,33,0,4,0,44,1,7,0,53,6,4,0,247,0,4,0,19,1,4,0,20,1,0,0,4,0,23,1,79,4,4,0,78,4,4,0,54,6,25,1,12,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,4,0,232,5,4,0,33,0,7,0,55,6,0,0,56,6,0,0,20,0,4,0,44,6,
0,0,127,0,128,0,45,6,26,1,13,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,38,6,0,0,56,6,0,0,20,0,4,0,44,6,7,0,57,6,128,0,45,6,27,1,11,0,19,1,34,6,54,0,58,6,129,0,20,2,0,0,35,6,4,0,232,5,4,0,33,0,4,0,203,5,7,0,59,6,4,0,60,6,4,0,44,6,129,0,61,6,28,1,17,0,19,1,34,6,
54,0,58,6,129,0,20,2,4,0,62,6,4,0,33,0,7,0,213,3,7,0,255,4,7,0,63,6,7,0,64,6,7,0,65,6,0,0,4,0,4,0,183,4,4,0,232,5,0,0,35,6,0,0,36,6,4,0,66,6,4,0,44,6,29,1,24,0,19,1,34,6,129,0,20,2,0,0,35,6,4,0,232,5,0,0,36,6,4,0,44,6,7,0,102,0,7,0,103,0,7,0,67,6,7,0,101,0,2,0,33,0,2,0,247,0,2,0,68,6,
2,0,69,6,7,0,70,6,7,0,71,6,2,0,72,6,0,0,24,0,54,0,58,6,7,0,73,6,7,0,74,6,0,0,75,6,7,0,76,6,7,0,77,6,30,1,11,0,19,1,34,6,54,0,58,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,42,1,4,0,44,6,31,1,78,6,32,1,19,0,19,1,34,6,129,0,20,2,0,0,35,6,4,0,232,5,4,0,33,0,4,0,44,6,
7,0,79,6,7,0,80,6,7,0,81,6,7,0,82,6,7,0,83,6,7,0,84,6,4,0,183,4,4,0,16,0,4,0,247,0,0,0,4,0,7,0,85,6,7,0,86,6,7,0,87,6,33,1,8,0,0,0,34,0,34,1,88,6,4,0,89,6,4,0,90,6,7,0,238,3,7,0,238,5,4,0,250,3,4,0,33,0,34,1,10,0,19,1,34,6,129,0,20,2,0,0,35,6,4,0,232,5,4,0,33,0,4,0,44,6,4,0,91,6,
33,1,79,4,4,0,78,4,4,0,54,6,35,1,9,0,19,1,34,6,54,0,58,6,129,0,20,2,0,0,35,6,0,0,36,6,4,0,232,5,4,0,33,0,4,0,44,6,0,0,4,0,36,1,18,0,19,1,34,6,54,0,58,6,129,0,20,2,0,0,143,3,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,44,6,0,0,4,0,4,0,33,0,0,0,92,6,0,0,164,2,7,0,93,6,7,0,94,6,7,0,202,1,
7,0,95,6,128,0,96,6,37,1,13,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,4,0,232,5,4,0,33,0,7,0,38,6,2,0,247,0,2,0,16,0,4,0,44,6,7,0,101,0,7,0,130,2,7,0,187,1,38,1,19,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,154,0,7,0,97,6,7,0,250,5,7,0,63,6,7,0,64,6,7,0,65,6,
4,0,183,4,4,0,247,0,4,0,98,6,4,0,99,6,4,0,44,6,0,0,4,0,39,1,12,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,38,6,4,0,16,0,4,0,44,6,0,0,127,0,128,0,45,6,40,1,7,0,19,1,34,6,2,0,151,1,2,0,100,6,4,0,155,2,54,0,58,6,7,0,101,6,0,0,37,6,41,1,14,0,19,1,34,6,129,0,20,2,
0,0,35,6,0,0,36,6,4,0,232,5,4,0,33,0,4,0,44,6,4,0,161,1,4,0,102,6,7,0,187,1,7,0,44,1,7,0,103,6,7,0,104,6,7,0,105,6,42,1,16,0,19,1,34,6,54,0,58,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,44,6,4,0,33,0,4,0,247,0,7,0,38,6,7,0,238,3,7,0,53,2,4,0,31,0,128,0,45,6,134,0,106,6,43,1,17,0,
19,1,34,6,129,0,20,2,0,0,35,6,0,0,36,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,107,6,7,0,212,5,7,0,108,6,7,0,109,6,7,0,110,6,4,0,44,6,2,0,111,6,2,0,247,0,7,0,112,6,0,0,4,0,44,1,12,0,19,1,34,6,0,0,75,6,129,0,20,2,0,0,35,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,113,6,4,0,44,6,7,0,114,6,7,0,115,6,54,0,58,6,
45,1,12,0,19,1,34,6,0,0,75,6,129,0,20,2,0,0,35,6,0,0,37,6,4,0,232,5,4,0,33,0,7,0,113,6,4,0,44,6,2,0,116,6,2,0,145,3,7,0,117,6,46,1,42,0,19,1,34,6,3,0,118,6,0,0,119,6,0,0,120,6,2,0,121,6,2,0,122,6,54,0,123,6,54,0,124,6,54,0,125,6,67,0,126,6,129,0,127,6,0,0,128,6,0,0,129,6,0,0,37,6,7,0,130,6,7,0,131,6,
7,0,132,6,7,0,133,6,7,0,134,6,7,0,238,5,2,0,203,5,0,0,135,6,0,0,136,6,0,0,137,6,0,0,138,6,0,0,139,6,0,0,10,4,7,0,140,6,7,0,141,6,7,0,142,6,7,0,143,6,4,0,144,6,4,0,161,1,7,0,145,6,0,0,146,6,0,0,147,6,2,0,148,6,0,0,149,6,0,0,150,6,0,0,68,4,15,1,228,2,47,1,151,6,48,1,20,0,19,1,34,6,54,0,220,3,54,0,152,6,
129,0,20,2,0,0,35,6,0,0,37,6,4,0,232,5,4,0,33,0,4,0,44,6,7,0,153,6,2,0,154,6,0,0,155,6,0,0,156,6,7,0,157,6,0,0,158,6,0,0,159,6,0,0,24,0,7,0,160,6,4,0,161,6,49,1,78,6,50,1,14,0,19,1,34,6,129,0,20,2,0,0,35,6,0,0,37,6,4,0,232,5,4,0,33,0,4,0,247,0,4,0,250,3,7,0,203,5,7,0,42,1,4,0,162,6,4,0,44,6,
4,0,163,6,0,0,4,0,153,0,8,0,7,0,188,2,7,0,164,6,7,0,165,6,7,0,166,6,7,0,184,2,7,0,167,6,4,0,168,6,4,0,169,6,51,1,3,0,12,0,31,0,0,0,20,0,4,0,33,0,52,1,3,0,51,1,9,0,217,0,89,4,53,1,57,0,54,1,2,0,51,1,9,0,55,1,170,6,56,1,4,0,4,0,171,6,4,0,33,0,12,0,31,0,0,0,20,0,57,1,4,0,4,0,211,4,56,1,172,6,
4,0,41,1,4,0,33,0,58,1,5,0,58,1,29,0,58,1,30,0,0,0,173,6,3,0,33,0,0,0,24,0,59,1,7,0,59,1,29,0,59,1,30,0,60,1,85,0,0,0,6,0,12,0,31,0,1,0,104,0,4,0,33,0,61,1,17,0,59,1,9,0,57,1,174,6,12,0,240,5,0,0,20,0,7,0,238,5,23,0,175,6,4,0,176,6,0,0,107,0,54,0,85,0,0,0,177,6,7,0,93,6,7,0,178,6,7,0,249,5,
7,0,250,5,0,0,35,5,0,0,179,6,62,1,57,0,60,1,3,0,59,1,9,0,23,0,65,3,63,1,57,0,64,1,10,0,7,0,238,5,12,0,247,0,1,0,33,0,1,0,180,6,0,0,10,4,2,0,181,6,2,0,182,6,7,0,183,6,7,0,184,6,0,0,107,0,55,1,19,0,26,0,89,0,101,0,112,1,51,1,185,6,4,0,186,6,0,0,4,0,60,1,187,6,218,0,188,6,4,0,90,4,0,0,107,0,59,1,189,6,
129,0,190,6,2,0,191,6,0,0,192,6,4,0,33,0,23,0,87,4,4,0,88,4,0,0,214,5,64,1,193,6,65,1,57,0,147,0,13,0,241,0,194,6,4,0,195,6,4,0,228,5,4,0,44,1,4,0,19,1,0,0,196,6,0,0,197,6,2,0,198,6,4,0,199,6,2,0,200,6,2,0,201,6,2,0,132,1,2,0,33,0,66,1,3,0,66,1,29,0,66,1,30,0,67,1,202,6,68,1,4,0,68,1,29,0,68,1,30,0,
0,0,34,0,0,0,154,2,69,1,6,0,69,1,29,0,69,1,30,0,39,0,91,0,4,0,201,6,4,0,203,6,0,0,154,2,70,1,4,0,70,1,29,0,70,1,30,0,0,0,34,0,71,1,204,6,72,1,4,0,4,0,205,6,4,0,155,2,4,0,206,6,4,0,207,6,73,1,11,0,73,1,29,0,73,1,30,0,72,1,81,0,4,0,203,6,4,0,208,6,4,0,209,6,0,0,210,6,0,0,211,6,2,0,212,6,7,0,213,6,
0,0,214,6,74,1,4,0,11,0,215,6,75,1,216,6,76,1,217,6,7,0,218,6,126,0,44,0,26,0,89,0,101,0,112,1,13,0,219,6,0,0,90,0,77,1,228,2,245,0,220,6,23,0,221,6,71,1,222,6,23,0,223,6,2,0,224,6,2,0,225,6,4,0,33,0,2,0,185,2,2,0,31,0,4,0,226,6,4,0,227,6,2,0,228,6,2,0,229,6,2,0,230,6,2,0,231,6,2,0,232,6,0,0,239,1,39,0,91,0,
23,0,233,6,40,0,231,0,4,0,234,6,4,0,208,6,4,0,209,6,0,0,210,6,0,0,211,6,2,0,212,6,7,0,213,6,7,0,235,6,7,0,236,6,170,0,237,6,0,0,238,6,0,0,155,2,0,0,239,6,0,0,240,6,4,0,241,6,23,0,242,6,23,0,243,6,78,1,244,6,74,1,81,0,79,1,6,0,54,0,181,1,2,0,245,6,2,0,246,6,2,0,31,0,2,0,33,0,0,0,225,5,80,1,21,0,80,1,29,0,
80,1,30,0,205,0,5,4,95,0,66,1,87,0,247,6,87,0,248,6,2,0,245,6,2,0,246,6,2,0,249,6,2,0,33,1,2,0,239,3,2,0,250,6,2,0,33,0,0,0,42,0,7,0,216,3,7,0,106,3,4,0,251,6,7,0,252,6,7,0,253,6,7,0,58,1,79,1,64,1,73,0,7,0,26,0,89,0,23,0,254,6,87,0,88,3,2,0,245,6,2,0,255,6,2,0,0,7,0,0,51,0,81,1,15,0,81,1,29,0,
81,1,30,0,7,0,48,4,7,0,58,1,2,0,31,0,0,0,87,1,2,0,1,7,2,0,33,0,4,0,2,7,4,0,99,4,11,0,17,1,0,0,34,0,0,0,3,7,7,0,4,7,7,0,5,7,211,0,15,0,26,0,89,0,101,0,112,1,81,1,6,7,0,0,7,7,4,0,8,7,0,0,4,0,23,0,9,7,73,0,253,0,26,0,10,7,4,0,182,4,2,0,33,0,0,0,31,0,0,0,11,7,7,0,197,0,4,0,12,7,
82,1,30,0,26,0,89,0,101,0,112,1,2,0,251,3,2,0,252,3,2,0,13,7,2,0,33,0,2,0,14,7,2,0,15,7,2,0,16,7,0,0,168,2,0,0,17,7,0,0,18,7,0,0,19,7,4,0,20,7,7,0,21,7,7,0,22,7,7,0,23,7,7,0,24,7,7,0,25,7,7,0,26,7,205,0,27,7,73,0,253,0,211,0,17,4,7,1,215,5,0,0,3,7,23,0,87,4,4,0,88,4,0,0,35,0,83,1,28,7,
11,0,73,4,84,1,11,0,84,1,29,0,84,1,30,0,54,0,58,6,84,1,29,7,4,0,30,7,2,0,33,0,2,0,31,7,2,0,32,7,2,0,33,7,2,0,34,7,0,0,87,1,85,1,5,0,85,1,29,0,85,1,30,0,86,1,35,7,11,0,56,0,11,0,36,7,87,1,10,0,87,1,29,0,87,1,30,0,67,0,56,3,11,0,145,1,2,0,33,0,2,0,37,7,0,0,4,0,23,0,38,7,2,0,34,7,2,0,168,2,
88,1,2,0,4,0,39,7,4,0,10,4,89,1,5,0,89,1,29,0,89,1,30,0,0,0,34,0,4,0,33,0,4,0,31,0,90,1,3,0,90,1,29,0,90,1,30,0,0,0,34,0,91,1,1,0,0,0,34,0,163,0,29,0,163,0,29,0,163,0,30,0,0,0,34,0,2,0,33,0,0,0,24,0,23,0,40,7,92,1,41,7,84,1,42,7,23,0,38,7,87,1,142,1,4,0,43,7,4,0,44,7,7,0,45,7,2,0,46,7,
2,0,47,7,0,0,127,0,4,0,48,7,129,0,49,7,93,1,50,7,24,0,51,7,252,0,52,7,88,1,53,7,23,0,54,7,89,1,55,7,23,0,56,7,90,1,57,7,23,0,219,6,84,1,58,7,58,0,59,7,94,1,48,0,26,0,89,0,101,0,112,1,2,0,31,0,2,0,33,0,4,0,247,0,7,0,159,4,7,0,160,4,7,0,161,4,7,0,60,7,7,0,238,3,7,0,61,7,7,0,62,7,2,0,63,7,2,0,64,7,
7,0,65,7,7,0,66,7,7,0,67,7,7,0,68,7,7,0,69,7,7,0,70,7,7,0,71,7,7,0,72,7,2,0,73,7,2,0,74,7,7,0,75,7,7,0,200,2,7,0,201,2,7,0,76,7,7,0,77,7,7,0,78,7,4,0,79,7,7,0,80,7,7,0,81,7,7,0,82,7,7,0,83,7,7,0,84,7,7,0,85,7,7,0,86,7,7,0,87,7,7,0,88,7,7,0,89,7,7,0,90,7,0,0,35,5,40,0,231,0,
95,1,91,7,73,0,253,0,7,0,92,7,7,0,11,7,96,1,35,0,26,0,89,0,101,0,112,1,0,0,31,0,0,0,33,0,0,0,93,7,0,0,94,7,0,0,95,7,0,0,8,1,7,0,96,7,7,0,97,7,7,0,202,1,7,0,200,2,7,0,201,2,7,0,98,7,7,0,99,7,7,0,100,7,7,0,101,7,4,0,102,7,4,0,103,7,4,0,104,7,4,0,105,7,7,0,106,7,7,0,107,7,7,0,108,7,7,0,109,7,
7,0,110,7,7,0,111,7,7,0,112,7,7,0,113,7,7,0,114,7,7,0,115,7,4,0,116,7,67,0,117,7,7,0,118,7,0,0,127,0,97,1,7,0,7,0,119,7,7,0,94,7,7,0,120,7,7,0,93,7,7,0,192,6,7,0,121,7,7,0,122,7,98,1,15,0,7,0,123,7,4,0,124,7,4,0,44,1,7,0,125,7,7,0,126,7,7,0,127,7,7,0,128,7,7,0,129,7,7,0,130,7,7,0,131,7,7,0,132,7,
7,0,133,7,7,0,134,7,7,0,135,7,7,0,136,7,99,1,6,0,245,0,137,7,0,0,17,1,4,0,138,7,0,0,139,7,0,0,140,7,0,0,51,0,100,1,14,0,4,0,33,0,4,0,141,7,4,0,31,0,4,0,142,7,4,0,143,7,4,0,144,7,4,0,145,7,4,0,146,7,0,0,147,7,99,1,148,7,99,1,149,7,99,1,150,7,97,1,151,7,98,1,152,7,101,1,6,0,7,0,153,7,7,0,154,7,7,0,155,7,
7,0,156,7,7,0,157,7,7,0,158,7,102,1,4,0,7,0,153,7,7,0,154,7,7,0,155,7,7,0,156,7,103,1,4,0,7,0,159,7,7,0,160,7,7,0,161,7,7,0,162,7,104,1,1,0,1,0,157,7,105,1,2,0,4,0,213,3,4,0,190,1,106,1,12,0,4,0,155,0,4,0,163,7,4,0,164,7,4,0,165,7,105,1,166,7,101,1,167,7,102,1,168,7,103,1,169,7,104,1,170,7,0,0,4,0,4,0,171,7,
11,0,172,7,107,1,5,0,4,0,98,5,0,0,173,7,0,0,174,7,0,0,42,0,106,1,175,7,108,1,7,0,108,1,29,0,108,1,30,0,0,0,34,0,4,0,31,0,7,0,0,1,4,0,161,1,4,0,42,2,109,1,2,0,108,1,34,6,134,0,176,7,110,1,4,0,108,1,34,6,128,0,16,1,4,0,161,1,0,0,4,0,111,1,6,0,108,1,34,6,128,0,16,1,4,0,161,1,7,0,177,7,7,0,178,7,0,0,4,0,
112,1,4,0,108,1,34,6,134,0,176,7,7,0,179,7,7,0,180,7,113,1,6,0,108,1,34,6,128,0,16,1,4,0,161,1,7,0,179,7,7,0,180,7,0,0,4,0,114,1,8,0,108,1,34,6,128,0,16,1,4,0,161,1,7,0,179,7,7,0,180,7,7,0,177,7,7,0,178,7,0,0,4,0,115,1,5,0,108,1,34,6,54,0,220,3,134,0,176,7,7,0,179,7,7,0,180,7,116,1,7,0,108,1,34,6,54,0,220,3,
128,0,16,1,4,0,161,1,7,0,179,7,7,0,180,7,0,0,4,0,117,1,9,0,108,1,34,6,54,0,220,3,128,0,16,1,4,0,161,1,7,0,179,7,7,0,180,7,7,0,177,7,7,0,178,7,0,0,4,0,118,1,6,0,108,1,34,6,7,0,181,7,7,0,182,7,134,0,176,7,7,0,179,7,7,0,180,7,119,1,6,0,108,1,34,6,128,0,16,1,4,0,161,1,7,0,181,7,7,0,182,7,0,0,4,0,120,1,8,0,
108,1,34,6,128,0,16,1,4,0,161,1,0,0,4,0,7,0,181,7,7,0,182,7,7,0,183,7,7,0,184,7,121,1,6,0,108,1,34,6,134,0,176,7,7,0,246,3,7,0,26,1,4,0,183,4,0,0,4,0,122,1,6,0,108,1,34,6,128,0,16,1,4,0,161,1,7,0,246,3,7,0,26,1,4,0,183,4,123,1,5,0,108,1,34,6,7,0,246,3,7,0,26,1,4,0,161,1,4,0,183,4,124,1,4,0,108,1,34,6,
134,0,176,7,7,0,185,7,7,0,87,6,125,1,6,0,108,1,34,6,128,0,16,1,4,0,161,1,7,0,185,7,7,0,87,6,0,0,4,0,126,1,8,0,108,1,34,6,128,0,16,1,4,0,161,1,0,0,4,0,7,0,185,7,7,0,87,6,7,0,183,7,7,0,184,7,127,1,2,0,108,1,34,6,134,0,176,7,128,1,4,0,108,1,34,6,128,0,16,1,4,0,161,1,0,0,4,0,129,1,6,0,108,1,34,6,128,0,16,1,
4,0,161,1,7,0,183,7,7,0,184,7,0,0,4,0,130,1,4,0,108,1,34,6,134,0,176,7,4,0,161,1,4,0,186,7,131,1,4,0,108,1,34,6,128,0,16,1,4,0,161,1,4,0,186,7,132,1,6,0,108,1,34,6,128,0,16,1,4,0,161,1,7,0,177,7,7,0,178,7,4,0,186,7,133,1,3,0,108,1,34,6,7,0,187,7,0,0,4,0,134,1,3,0,108,1,34,6,7,0,188,7,0,0,4,0,135,1,5,0,
108,1,34,6,7,0,189,7,7,0,26,1,7,0,43,1,0,0,4,0,136,1,5,0,108,1,34,6,7,0,26,1,7,0,84,1,4,0,190,7,4,0,161,1,137,1,7,0,108,1,34,6,7,0,191,7,7,0,26,1,7,0,117,6,4,0,190,7,4,0,183,4,0,0,127,0,138,1,7,0,108,1,34,6,7,0,191,7,7,0,26,1,7,0,117,6,4,0,190,7,4,0,183,4,0,0,127,0,139,1,3,0,108,1,34,6,7,0,192,7,
0,0,4,0,140,1,3,0,108,1,34,6,7,0,193,7,0,0,4,0,141,1,3,0,108,1,34,6,7,0,188,7,0,0,4,0,142,1,3,0,108,1,34,6,7,0,44,1,0,0,4,0,143,1,7,0,108,1,34,6,4,0,161,1,4,0,194,7,7,0,192,7,4,0,195,7,4,0,196,7,4,0,197,7,144,1,5,0,108,1,34,6,7,0,79,1,7,0,80,1,7,0,75,3,7,0,76,3,145,1,9,0,108,1,34,6,4,0,177,2,
7,0,198,7,7,0,199,7,7,0,117,6,7,0,200,7,7,0,201,7,7,0,202,7,0,0,4,0,146,1,3,0,108,1,34,6,7,0,203,7,0,0,4,0,147,1,5,0,108,1,34,6,7,0,183,7,7,0,184,7,7,0,204,7,0,0,4,0,250,0,45,0,26,0,89,0,101,0,112,1,7,0,159,4,7,0,160,4,7,0,161,4,7,0,225,1,7,0,203,5,4,0,205,7,7,0,206,7,4,0,33,0,4,0,207,7,4,0,208,7,
4,0,194,7,7,0,209,7,7,0,185,7,7,0,87,6,7,0,210,7,7,0,211,7,4,0,212,7,2,0,213,7,2,0,214,7,2,0,215,7,2,0,216,7,2,0,217,7,2,0,218,7,4,0,219,7,4,0,220,7,7,0,221,7,2,0,222,7,2,0,73,7,2,0,74,7,0,0,24,0,2,0,223,7,2,0,224,7,2,0,225,7,2,0,226,7,2,0,227,7,2,0,228,7,4,0,229,7,132,0,230,7,95,1,91,7,23,0,231,7,
23,0,232,7,23,0,233,7,23,0,234,7,148,1,2,0,148,1,29,0,148,1,30,0,149,1,3,0,149,1,29,0,149,1,30,0,11,0,17,1,23,0,2,0,11,0,0,0,11,0,1,0,150,1,10,0,26,0,89,0,101,0,112,1,13,0,219,6,23,0,235,7,4,0,236,7,4,0,237,7,4,0,19,1,4,0,20,1,4,0,33,0,0,0,4,0,151,1,7,0,4,0,23,0,4,0,31,0,26,0,48,1,0,0,238,7,0,0,239,7,
7,0,240,7,7,0,241,7,152,1,3,0,7,0,242,7,7,0,11,4,4,0,33,0,153,1,5,0,95,0,189,5,0,0,4,0,4,0,243,7,152,1,244,7,151,1,245,7,154,1,9,0,154,1,29,0,154,1,30,0,2,0,33,0,0,0,246,7,0,0,247,7,4,0,248,7,153,1,100,0,151,1,245,7,153,1,249,7,155,1,7,0,155,1,29,0,155,1,30,0,7,0,17,1,4,0,250,7,4,0,162,2,0,0,33,0,0,0,30,2,
156,1,14,0,156,1,29,0,156,1,30,0,0,0,34,0,23,0,251,7,23,0,252,7,154,1,253,7,153,1,254,7,7,0,225,1,0,0,42,2,0,0,255,7,0,0,202,1,0,0,30,2,0,0,33,0,0,0,0,8,157,1,6,0,126,0,180,2,147,0,1,8,0,0,2,8,0,0,3,8,4,0,4,8,4,0,5,8,158,1,25,0,126,0,6,8,126,0,180,2,7,0,7,8,7,0,8,8,7,0,9,8,2,0,33,0,2,0,91,1,
2,0,10,8,2,0,11,8,7,0,12,8,7,0,13,8,7,0,14,8,0,0,107,0,7,0,15,8,7,0,16,8,7,0,17,8,7,0,18,8,7,0,19,8,7,0,20,8,7,0,21,8,4,0,247,0,4,0,22,8,7,0,23,8,4,0,24,8,7,0,112,6,159,1,5,0,4,0,161,1,0,0,136,6,0,0,25,8,0,0,26,8,0,0,155,2,129,0,46,0,26,0,89,0,101,0,112,1,2,0,33,0,0,0,27,8,0,0,249,3,
7,0,159,4,7,0,160,4,7,0,161,4,7,0,28,8,7,0,29,8,7,0,30,8,7,0,31,8,7,0,225,1,7,0,32,8,7,0,33,8,7,0,34,8,7,0,35,8,7,0,36,8,0,0,74,7,0,0,37,8,2,0,73,7,2,0,38,8,2,0,91,1,95,1,91,7,73,0,253,0,40,0,231,0,7,0,39,8,2,0,40,8,2,0,41,8,2,0,42,8,2,0,43,8,2,0,44,8,0,0,45,8,0,0,46,8,7,0,47,8,
7,0,48,8,0,0,49,8,0,0,50,8,0,0,255,7,0,0,51,8,7,0,52,8,0,0,35,5,157,1,53,8,23,0,54,8,158,1,55,8,159,1,56,8,247,0,53,0,26,0,89,0,101,0,112,1,73,0,253,0,211,0,17,4,129,0,18,4,4,0,33,1,4,0,57,8,4,0,58,8,4,0,59,8,4,0,60,8,218,0,61,8,218,0,62,8,218,0,63,8,218,0,64,8,23,0,87,4,4,0,88,4,4,0,90,4,160,1,65,8,
4,0,66,8,4,0,67,8,247,0,68,8,7,0,154,0,7,0,155,0,0,0,20,4,0,0,69,8,3,0,33,0,7,0,70,8,7,0,71,8,7,0,72,8,4,0,73,8,4,0,74,8,0,0,75,8,0,0,76,8,0,0,91,4,0,0,77,8,2,0,26,4,0,0,78,8,0,0,60,6,0,0,79,8,0,0,80,8,161,1,81,8,162,1,82,8,163,1,83,8,164,1,84,8,7,1,215,5,165,1,85,8,166,1,86,8,167,1,87,8,
168,1,88,8,218,0,89,8,4,0,90,8,0,0,127,0,169,1,57,0,166,1,8,0,11,0,91,8,7,0,92,8,4,0,93,8,0,0,33,0,0,0,94,8,2,0,247,0,2,0,199,6,2,0,95,8,160,1,2,0,4,0,91,1,4,0,31,0,170,1,1,0,7,0,96,8,171,1,1,0,4,0,97,8,172,1,2,0,0,0,98,8,0,0,99,8,173,1,1,0,1,0,161,4,174,1,1,0,12,0,97,8,175,1,2,0,4,0,100,8,
7,0,127,1,7,1,3,0,175,1,101,8,4,0,102,8,4,0,33,0,176,1,2,0,7,0,103,8,4,0,33,0,177,1,4,0,0,0,159,4,0,0,160,4,0,0,161,4,0,0,28,8,178,1,1,0,7,0,179,5,179,1,4,0,4,0,104,8,4,0,190,1,7,0,105,8,4,0,106,8,180,1,3,0,7,0,17,1,4,0,190,1,0,0,4,0,181,1,1,0,0,0,33,0,182,1,1,0,0,0,33,0,164,1,5,0,4,0,107,8,
4,0,108,8,0,0,109,8,0,0,110,8,2,0,33,0,161,1,5,0,4,0,111,8,4,0,59,8,2,0,250,3,0,0,33,0,0,0,155,2,183,1,2,0,7,0,112,8,4,0,33,0,163,1,4,0,7,0,99,0,0,0,33,0,0,0,110,8,0,0,51,0,162,1,2,0,4,0,146,2,4,0,113,8,168,1,7,0,4,0,107,8,4,0,108,8,4,0,114,8,4,0,115,8,2,0,250,3,0,0,116,8,0,0,33,0,165,1,1,0,
7,0,92,8,167,1,4,0,0,0,28,8,0,0,159,4,0,0,160,4,0,0,161,4,184,1,1,0,4,0,97,8,185,1,19,0,185,1,29,0,185,1,30,0,186,1,117,8,2,0,31,0,2,0,33,0,0,0,4,0,7,0,75,3,7,0,76,3,7,0,178,5,7,0,157,0,7,0,118,8,7,0,119,8,7,0,120,8,7,0,121,8,7,0,122,8,7,0,145,2,7,0,37,0,7,0,123,8,7,0,124,8,187,1,19,0,26,0,89,0,
101,0,112,1,23,0,125,8,23,0,126,8,73,0,253,0,129,0,18,4,0,0,33,0,0,0,241,1,2,0,26,4,0,0,20,4,0,0,51,0,0,0,149,1,7,0,154,0,7,0,155,0,7,0,127,8,7,0,128,8,7,0,129,8,0,0,35,0,185,1,130,8,188,1,13,0,188,1,29,0,188,1,30,0,4,0,31,0,4,0,247,0,7,0,131,8,2,0,33,0,2,0,18,1,3,0,132,8,0,0,51,0,4,0,133,8,0,0,34,0,
0,0,33,6,11,0,57,0,189,1,8,0,188,1,34,6,230,0,134,8,54,0,135,8,0,0,136,8,0,0,149,4,0,0,127,0,4,0,137,8,4,0,138,8,190,1,11,0,188,1,34,6,2,0,139,8,2,0,140,8,2,0,141,8,2,0,161,1,2,0,142,8,2,0,143,8,2,0,144,8,0,0,51,0,11,0,145,8,11,0,146,8,191,1,7,0,188,1,34,6,54,0,58,6,0,0,34,0,7,0,42,1,2,0,33,0,0,0,51,0,
11,0,145,1,192,1,7,0,188,1,34,6,54,0,58,6,0,0,34,0,2,0,147,8,2,0,33,0,0,0,4,0,11,0,145,1,193,1,6,0,188,1,34,6,7,0,79,1,7,0,101,0,2,0,33,0,2,0,148,8,4,0,183,4,194,1,7,0,188,1,34,6,54,0,149,8,0,0,3,7,2,0,247,0,2,0,33,0,7,0,150,8,11,0,145,1,195,1,14,0,188,1,34,6,54,0,151,8,54,0,152,8,54,0,153,8,54,0,154,8,
7,0,213,3,7,0,250,5,7,0,101,0,7,0,155,8,4,0,156,8,4,0,157,8,4,0,161,1,4,0,62,6,7,0,158,8,196,1,11,0,188,1,34,6,2,0,116,6,2,0,33,0,7,0,203,7,7,0,159,8,1,0,160,8,0,0,20,0,7,0,158,8,7,0,161,8,54,0,162,8,11,0,145,1,197,1,3,0,188,1,34,6,7,0,163,8,4,0,161,1,198,1,23,0,188,1,34,6,7,0,7,0,4,0,164,8,2,0,161,1,
2,0,165,8,2,0,166,8,2,0,167,8,2,0,168,8,2,0,169,8,2,0,170,8,2,0,171,8,2,0,172,8,2,0,173,8,2,0,174,8,0,0,175,8,0,0,155,2,7,0,176,8,7,0,177,8,7,0,163,6,0,0,178,8,0,0,127,0,212,0,179,8,11,0,131,3,243,0,7,0,188,1,34,6,242,0,180,8,246,0,181,8,248,0,182,8,7,0,30,1,4,0,31,0,11,0,145,1,199,1,15,0,188,1,34,6,230,0,134,8,
54,0,135,8,0,0,136,8,0,0,149,4,0,0,127,0,4,0,137,8,4,0,138,8,7,0,42,1,4,0,183,8,0,0,178,8,7,0,184,8,4,0,145,3,2,0,33,0,0,0,68,4,200,1,10,0,188,1,34,6,54,0,185,8,0,0,107,0,4,0,186,8,7,0,187,8,7,0,188,8,7,0,189,8,7,0,190,8,0,0,149,4,4,0,137,8,201,1,11,0,188,1,34,6,7,0,191,8,2,0,192,8,0,0,193,8,0,0,194,8,
7,0,117,6,0,0,178,8,7,0,195,8,2,0,33,0,2,0,247,0,4,0,196,8,202,1,5,0,188,1,34,6,7,0,197,8,0,0,178,8,2,0,33,0,2,0,83,1,203,1,9,0,188,1,34,6,54,0,58,6,7,0,197,8,7,0,238,3,7,0,41,1,0,0,178,8,2,0,33,0,2,0,31,0,11,0,145,1,204,1,24,0,188,1,34,6,230,0,134,8,54,0,135,8,0,0,136,8,0,0,149,4,0,0,127,0,4,0,137,8,
4,0,138,8,54,0,198,8,0,0,178,8,2,0,33,0,0,0,239,1,7,0,212,4,7,0,213,4,7,0,205,1,7,0,27,4,7,0,214,4,7,0,191,1,7,0,190,4,7,0,202,1,7,0,216,4,7,0,180,4,0,0,35,5,11,0,199,8,205,1,7,0,188,1,34,6,2,0,151,1,2,0,100,6,0,0,107,0,54,0,58,6,7,0,101,6,0,0,178,8,206,1,15,0,188,1,34,6,54,0,58,6,0,0,143,3,0,0,33,0,
0,0,92,6,0,0,24,0,7,0,93,6,7,0,94,6,7,0,202,1,128,0,96,6,4,0,200,8,4,0,201,8,7,0,95,6,0,0,34,0,11,0,145,1,207,1,1,0,188,1,34,6,208,1,12,0,188,1,34,6,209,1,202,8,154,0,203,8,157,0,204,8,229,0,205,8,23,0,119,4,210,1,206,8,7,0,207,8,7,0,208,8,4,0,209,8,7,0,210,8,211,1,211,8,212,1,15,0,188,1,34,6,7,0,212,8,7,0,213,8,
7,0,214,8,7,0,215,8,7,0,216,8,7,0,217,8,4,0,218,8,4,0,219,8,4,0,220,8,7,0,221,8,7,0,222,8,0,0,223,8,0,0,30,2,213,1,224,8,214,1,6,0,7,0,225,8,7,0,226,8,247,0,143,5,215,1,224,8,4,0,227,8,4,0,228,8,216,1,2,0,188,1,34,6,214,1,81,0,217,1,10,0,188,1,34,6,54,0,58,6,67,0,56,3,7,0,229,8,0,0,40,0,0,0,210,0,0,0,230,8,
0,0,33,0,0,0,231,8,0,0,30,2,218,1,2,0,4,0,232,8,7,0,127,1,219,1,2,0,4,0,44,1,4,0,233,8,220,1,22,0,188,1,34,6,54,0,58,6,0,0,178,8,2,0,234,8,2,0,33,0,0,0,4,0,218,1,235,8,4,0,236,8,7,0,237,8,4,0,33,1,4,0,238,8,219,1,239,8,218,1,240,8,4,0,241,8,4,0,242,8,4,0,233,8,7,0,243,8,7,0,244,8,7,0,245,8,7,0,246,8,
7,0,247,8,11,0,248,8,221,1,10,0,188,1,34,6,233,0,157,4,247,0,249,8,247,0,250,8,4,0,251,8,4,0,252,8,4,0,253,8,2,0,33,0,0,0,51,0,11,0,145,1,222,1,15,0,188,1,34,6,54,0,181,1,2,0,254,8,2,0,33,0,2,0,116,6,2,0,145,3,7,0,255,8,7,0,0,9,7,0,184,2,7,0,1,9,7,0,2,9,7,0,3,9,0,0,4,9,0,0,5,9,11,0,145,1,223,1,8,0,
188,1,34,6,4,0,6,9,2,0,33,0,2,0,7,9,7,0,8,9,0,0,9,9,0,0,127,0,11,0,131,3,224,1,12,0,188,1,34,6,0,0,10,9,0,0,11,9,0,0,12,9,0,0,13,9,0,0,14,9,0,0,161,1,0,0,51,0,2,0,143,8,2,0,142,8,2,0,144,8,0,0,239,1,225,1,3,0,188,1,34,6,226,1,15,9,11,0,145,1,227,1,3,0,188,1,34,6,4,0,31,0,4,0,155,2,228,1,12,0,
188,1,34,6,54,0,220,3,54,0,16,9,0,0,17,9,7,0,18,9,2,0,221,3,0,0,19,9,0,0,225,3,7,0,224,3,0,0,222,3,0,0,20,9,0,0,51,0,229,1,10,0,188,1,34,6,54,0,21,9,0,0,17,9,7,0,38,6,7,0,22,9,0,0,247,0,0,0,116,6,0,0,23,9,0,0,33,0,11,0,145,1,230,1,1,0,188,1,34,6,231,1,20,0,188,1,34,6,0,0,178,8,0,0,24,9,0,0,25,9,
7,0,44,1,7,0,176,3,7,0,26,9,7,0,27,9,0,0,247,0,0,0,28,9,0,0,29,9,0,0,155,2,7,0,30,9,7,0,31,9,7,0,32,9,4,0,33,0,2,0,33,9,2,0,34,9,7,0,35,9,7,0,36,9,232,1,12,0,188,1,34,6,54,0,37,9,4,0,38,9,4,0,39,9,4,0,192,8,7,0,40,9,7,0,117,6,7,0,155,8,2,0,33,0,0,0,116,6,0,0,52,1,11,0,145,1,233,1,34,0,
188,1,34,6,234,1,41,9,235,1,42,9,4,0,43,9,4,0,44,9,4,0,45,9,7,0,46,9,7,0,190,4,7,0,47,9,7,0,45,1,7,0,48,9,7,0,49,9,7,0,50,9,7,0,51,9,7,0,52,9,7,0,30,1,4,0,53,9,7,0,54,9,7,0,55,9,4,0,56,9,4,0,57,9,0,0,58,9,0,0,59,9,0,0,60,9,0,0,61,9,0,0,62,9,0,0,33,0,0,0,11,7,2,0,63,9,2,0,64,9,
4,0,183,4,7,0,41,1,7,0,65,9,0,0,4,0,236,1,20,0,188,1,34,6,230,0,134,8,54,0,135,8,0,0,136,8,0,0,149,4,0,0,127,0,4,0,137,8,4,0,138,8,54,0,66,9,54,0,67,9,0,0,68,9,0,0,69,9,128,0,96,6,0,0,178,8,7,0,42,1,7,0,70,9,0,0,33,0,0,0,92,6,0,0,68,4,11,0,90,1,237,1,17,0,188,1,34,6,0,0,178,8,2,0,71,9,2,0,92,6,
7,0,72,9,128,0,73,9,7,0,74,9,7,0,75,9,7,0,76,9,0,0,77,9,4,0,78,9,230,0,79,9,54,0,80,9,0,0,81,9,4,0,82,9,0,0,83,9,11,0,145,1,238,1,19,0,188,1,34,6,0,0,84,9,0,0,85,9,7,0,86,9,7,0,87,9,0,0,169,3,0,0,88,9,0,0,242,0,7,0,76,9,0,0,77,9,4,0,78,9,230,0,79,9,54,0,80,9,0,0,81,9,4,0,82,9,0,0,83,9,
0,0,127,0,0,0,33,0,0,0,168,2,239,1,19,0,188,1,34,6,0,0,178,8,128,0,73,9,4,0,89,9,4,0,90,9,54,0,91,9,7,0,76,9,0,0,77,9,4,0,78,9,230,0,79,9,54,0,80,9,0,0,81,9,4,0,82,9,0,0,83,9,0,0,127,0,7,0,92,9,7,0,93,9,2,0,92,6,0,0,42,0,231,0,5,0,188,1,34,6,227,0,116,4,232,0,94,9,4,0,31,0,0,0,4,0,240,1,10,0,
188,1,34,6,7,0,150,8,7,0,84,1,7,0,95,9,0,0,45,1,0,0,33,0,0,0,247,0,0,0,155,2,7,0,96,9,7,0,97,9,241,1,5,0,188,1,34,6,7,0,98,9,0,0,33,0,0,0,99,9,0,0,51,0,242,1,5,0,188,1,34,6,4,0,33,0,4,0,100,9,4,0,101,9,4,0,102,9,243,1,7,0,188,1,34,6,7,0,103,9,7,0,104,9,0,0,127,0,0,0,178,8,2,0,33,0,2,0,83,1,
244,1,9,0,7,0,105,9,4,0,106,9,7,0,103,9,7,0,84,1,2,0,83,1,2,0,33,0,0,0,107,9,0,0,108,9,0,0,24,0,245,1,12,0,188,1,34,6,7,0,109,9,4,0,110,9,7,0,103,9,7,0,84,1,2,0,83,1,2,0,33,0,0,0,107,9,0,0,108,9,0,0,24,0,0,0,178,8,244,1,111,9,246,1,15,0,188,1,34,6,0,0,112,9,0,0,113,9,2,0,33,0,7,0,114,9,7,0,224,1,
7,0,17,6,7,0,184,2,54,0,115,9,0,0,116,9,54,0,117,9,0,0,118,9,0,0,17,9,0,0,149,4,0,0,4,0,247,1,19,0,188,1,34,6,0,0,33,0,0,0,31,0,0,0,72,6,0,0,119,9,0,0,159,2,0,0,160,2,0,0,120,9,0,0,5,8,7,0,38,6,0,0,121,9,0,0,178,8,0,0,30,2,7,0,229,0,7,0,53,6,7,0,122,9,7,0,173,3,7,0,123,9,0,0,154,2,248,1,8,0,
188,1,34,6,0,0,124,9,4,0,125,9,4,0,83,1,7,0,126,9,11,0,127,9,2,0,33,0,0,0,24,0,249,1,9,0,188,1,34,6,0,0,178,8,7,0,44,1,7,0,176,3,7,0,26,9,7,0,128,9,2,0,33,0,2,0,33,9,0,0,4,0,250,1,6,0,188,1,34,6,7,0,155,8,0,0,178,8,0,0,247,0,0,0,33,0,0,0,51,0,251,1,18,0,188,1,34,6,54,0,129,9,4,0,130,9,4,0,131,9,
4,0,132,9,4,0,133,9,4,0,134,9,7,0,135,9,7,0,136,9,7,0,137,9,0,0,127,0,4,0,138,9,4,0,139,9,4,0,169,3,7,0,12,8,0,0,178,8,4,0,161,1,11,0,131,3,252,1,12,0,188,1,34,6,0,0,178,8,54,0,220,3,2,0,247,0,2,0,33,0,2,0,169,3,0,0,51,0,7,0,12,8,7,0,140,9,7,0,213,3,0,0,35,0,11,0,145,1,253,1,8,0,188,1,34,6,142,0,232,3,
0,0,233,3,0,0,141,9,0,0,20,0,7,0,130,5,204,0,234,3,0,0,235,3,254,1,6,0,4,0,142,9,4,0,146,5,4,0,247,0,7,0,143,9,7,0,144,9,7,0,0,1,255,1,3,0,254,1,145,9,4,0,146,9,4,0,147,9,0,2,15,0,188,1,34,6,32,0,64,0,54,0,220,3,255,1,148,9,11,0,145,1,7,0,202,1,4,0,149,9,4,0,146,5,4,0,150,9,4,0,151,9,4,0,161,1,7,0,123,7,
7,0,42,1,0,0,178,8,4,0,11,7,1,2,6,0,188,1,34,6,0,0,178,8,0,0,247,0,0,0,33,0,2,0,127,1,7,0,129,8,2,2,1,0,24,0,76,0,3,2,5,0,0,0,152,9,0,0,153,9,26,0,48,1,4,0,23,0,0,0,4,0,4,2,10,0,4,0,89,0,4,0,33,0,1,0,154,9,0,0,30,2,0,0,155,9,4,0,229,0,4,0,230,0,4,0,156,9,4,0,157,9,3,2,158,9,5,2,2,0,
4,0,89,0,4,0,33,0,6,2,12,0,188,1,34,6,95,1,159,9,2,2,160,9,0,0,161,9,12,0,33,0,0,0,20,0,4,0,162,9,4,2,163,9,0,0,107,0,4,0,164,9,5,2,165,9,7,2,57,0,8,2,9,0,188,1,34,6,54,0,58,6,4,0,166,9,7,0,96,9,4,0,167,9,7,0,168,9,7,0,58,2,0,0,107,0,11,0,90,1,9,2,7,0,188,1,34,6,230,0,134,8,54,0,169,9,4,0,170,9,
7,0,42,1,7,0,171,9,7,0,172,9,10,2,10,0,188,1,34,6,54,0,58,6,7,0,150,8,7,0,97,9,4,0,33,0,4,0,166,9,7,0,96,9,4,0,167,9,0,0,173,9,11,0,145,1,11,2,9,0,4,0,33,0,0,0,127,0,0,0,174,9,129,0,20,2,4,0,44,6,4,0,175,9,0,0,176,9,128,0,177,9,11,0,131,3,12,2,8,0,188,1,34,6,11,2,0,1,4,0,33,0,0,0,72,1,0,0,164,2,
7,0,178,9,7,0,179,9,11,0,131,3,13,2,6,0,188,1,34,6,11,2,0,1,4,0,31,0,4,0,190,1,0,0,180,9,11,0,145,1,14,2,6,0,188,1,34,6,11,2,0,1,0,0,72,1,0,0,164,2,7,0,55,6,11,0,131,3,15,2,11,0,188,1,34,6,11,2,0,1,2,0,33,0,0,0,72,1,0,0,181,9,7,0,38,6,7,0,238,3,7,0,104,0,54,0,58,6,134,0,176,7,11,0,65,0,16,2,7,0,
188,1,34,6,11,2,0,1,4,0,33,0,7,0,38,6,4,0,16,0,0,0,4,0,11,0,145,1,17,2,15,0,188,1,34,6,11,2,0,1,4,0,33,0,4,0,246,7,7,0,154,0,7,0,97,6,7,0,250,5,7,0,182,9,7,0,183,9,7,0,184,9,4,0,183,4,4,0,98,6,4,0,99,6,0,0,127,0,11,0,131,3,18,2,14,0,188,1,34,6,11,2,0,1,4,0,33,0,7,0,38,6,7,0,39,6,7,0,40,6,
7,0,41,6,7,0,34,5,7,0,42,6,2,0,43,6,0,0,51,0,4,0,16,0,4,0,183,4,11,0,145,1,19,2,5,0,188,1,34,6,11,2,0,1,54,0,58,6,4,0,33,0,0,0,4,0,20,2,7,0,188,1,34,6,11,2,0,1,4,0,33,0,7,0,46,6,7,0,203,5,0,0,4,0,11,0,145,1,21,2,5,0,188,1,34,6,11,2,0,1,54,0,58,6,7,0,42,1,0,0,4,0,22,2,7,0,0,0,34,0,
4,0,89,6,4,0,90,6,7,0,238,3,7,0,238,5,4,0,250,3,4,0,33,0,23,2,7,0,188,1,34,6,11,2,0,1,22,2,185,9,4,0,186,9,4,0,54,6,4,0,91,6,0,0,4,0,24,2,11,0,188,1,34,6,11,2,0,1,4,0,33,0,4,0,102,6,7,0,187,1,7,0,44,1,7,0,103,6,7,0,104,6,7,0,105,6,4,0,69,2,11,0,65,0,25,2,17,0,188,1,34,6,11,2,0,1,4,0,33,0,
7,0,79,6,7,0,80,6,7,0,81,6,7,0,82,6,7,0,83,6,7,0,84,6,4,0,183,4,4,0,16,0,4,0,247,0,0,0,4,0,7,0,85,6,7,0,86,6,7,0,87,6,11,0,145,1,26,2,9,0,188,1,34,6,11,2,0,1,4,0,33,0,7,0,113,6,2,0,116,6,2,0,145,3,7,0,117,6,0,0,75,6,11,0,65,0,27,2,13,0,188,1,34,6,11,2,0,1,54,0,58,6,4,0,62,6,4,0,33,0,
7,0,213,3,7,0,255,4,7,0,63,6,7,0,64,6,7,0,65,6,0,0,4,0,4,0,183,4,4,0,66,6,28,2,8,0,188,1,34,6,11,2,0,1,4,0,33,0,0,0,75,6,7,0,113,6,7,0,114,6,7,0,115,6,54,0,58,6,29,2,12,0,188,1,34,6,11,2,0,1,54,0,58,6,0,0,143,3,0,0,4,0,4,0,33,0,0,0,92,6,0,0,164,2,7,0,93,6,7,0,94,6,7,0,202,1,7,0,95,6,
30,2,43,0,188,1,34,6,3,0,168,5,0,0,119,6,0,0,120,6,2,0,121,6,2,0,122,6,54,0,123,6,54,0,124,6,54,0,125,6,67,0,126,6,129,0,127,6,0,0,128,6,0,0,129,6,0,0,37,6,7,0,130,6,7,0,131,6,7,0,132,6,7,0,133,6,7,0,134,6,7,0,238,5,2,0,203,5,0,0,187,9,0,0,188,9,0,0,137,6,0,0,138,6,0,0,139,6,0,0,10,4,7,0,140,6,7,0,141,6,
7,0,142,6,7,0,143,6,4,0,144,6,4,0,161,1,7,0,145,6,0,0,146,6,0,0,147,6,2,0,148,6,0,0,149,6,0,0,150,6,0,0,68,4,15,1,189,9,15,1,228,2,47,1,151,6,31,2,5,0,188,1,34,6,11,2,0,1,54,0,58,6,2,0,151,1,0,0,24,0,32,2,5,0,0,0,34,0,4,0,190,9,4,0,191,9,4,0,192,9,4,0,193,9,33,2,11,0,188,1,34,6,11,2,0,1,4,0,33,0,
4,0,44,1,7,0,53,6,4,0,247,0,4,0,19,1,4,0,20,1,32,2,185,9,4,0,186,9,4,0,54,6,34,2,8,0,188,1,34,6,11,2,0,1,4,0,247,0,4,0,250,3,7,0,203,5,7,0,42,1,4,0,162,6,4,0,163,6,35,2,8,0,188,1,34,6,11,2,0,1,54,0,58,6,4,0,33,0,4,0,203,5,7,0,59,6,4,0,60,6,129,0,61,6,36,2,15,0,188,1,34,6,11,2,0,1,54,0,220,3,
54,0,152,6,7,0,153,6,2,0,154,6,0,0,155,6,0,0,156,6,7,0,157,6,0,0,158,6,0,0,159,6,0,0,51,0,7,0,160,6,4,0,161,6,49,1,78,6,37,2,20,0,188,1,34,6,11,2,0,1,7,0,102,0,7,0,103,0,7,0,67,6,7,0,101,0,2,0,33,0,2,0,247,0,2,0,68,6,2,0,69,6,7,0,70,6,7,0,71,6,2,0,72,6,0,0,24,0,54,0,58,6,7,0,73,6,7,0,74,6,
0,0,75,6,7,0,76,6,7,0,77,6,38,2,9,0,188,1,34,6,11,2,0,1,2,0,247,0,0,0,4,0,2,0,16,0,7,0,38,6,7,0,101,0,7,0,130,2,7,0,187,1,39,2,12,0,188,1,34,6,11,2,0,1,7,0,107,6,7,0,212,5,7,0,108,6,7,0,109,6,7,0,110,6,4,0,44,6,2,0,111,6,2,0,247,0,7,0,112,6,0,0,4,0,149,0,3,0,4,0,195,6,2,0,194,9,2,0,195,9,
40,2,5,0,0,0,196,9,2,0,197,9,2,0,143,8,2,0,198,9,2,0,199,9,41,2,4,0,11,0,29,0,11,0,30,0,149,0,200,9,245,0,201,9,42,2,1,0,23,0,202,9,148,0,21,0,26,0,89,0,101,0,112,1,13,0,219,6,0,0,90,0,4,0,185,2,4,0,155,2,4,0,203,9,7,0,235,6,7,0,236,6,67,1,202,6,43,2,228,2,18,1,204,9,44,2,205,9,11,0,206,9,40,2,207,9,4,0,33,0,
4,0,37,0,4,0,102,0,4,0,9,1,170,0,237,6,42,2,81,0,45,2,15,0,2,0,108,3,2,0,208,9,4,0,209,9,4,0,210,9,4,0,211,9,46,2,212,9,133,0,213,9,133,0,214,9,7,0,215,9,2,0,216,9,2,0,217,9,4,0,218,9,47,2,219,9,46,2,220,9,7,0,221,9,48,2,10,0,48,2,29,0,48,2,30,0,2,0,31,0,2,0,33,0,0,0,222,9,7,0,223,9,7,0,224,9,2,0,215,0,
2,0,225,9,54,0,181,1,49,2,22,0,49,2,29,0,49,2,30,0,2,0,33,0,2,0,247,0,2,0,226,9,2,0,227,9,73,0,253,0,63,0,76,1,54,0,58,6,7,0,79,1,7,0,80,1,7,0,81,1,7,0,82,1,7,0,228,9,7,0,229,9,7,0,83,1,7,0,84,1,7,0,21,1,7,0,22,1,0,0,230,9,0,0,231,9,23,0,65,1,50,2,2,0,0,0,232,9,0,0,30,2,51,2,11,0,50,2,233,9,
0,0,6,0,0,0,2,0,0,0,234,9,4,0,33,0,2,0,235,9,2,0,236,9,0,0,237,9,0,0,5,0,11,0,238,9,24,0,76,0,52,2,8,0,50,2,233,9,0,0,6,0,0,0,2,0,4,0,33,0,0,0,4,0,50,2,239,9,4,0,240,9,4,0,241,9,53,2,4,0,52,2,242,9,4,0,243,9,4,0,244,9,54,2,57,0,55,2,11,0,7,0,248,3,7,0,12,0,7,0,13,0,11,0,17,1,2,0,245,9,
2,0,246,9,2,0,247,9,2,0,248,9,2,0,249,9,2,0,250,9,0,0,4,0,56,2,26,0,56,2,29,0,56,2,30,0,24,0,133,0,0,0,251,9,0,0,34,0,11,0,56,0,2,0,31,0,2,0,33,0,2,0,252,9,2,0,253,9,57,2,254,9,0,0,97,1,11,0,22,0,2,0,255,9,0,0,0,10,0,0,235,9,0,0,4,0,0,0,214,6,0,0,1,10,0,0,2,10,0,0,237,9,4,0,3,10,4,0,4,10,
58,2,5,10,55,2,6,10,59,2,57,0,60,2,3,0,4,0,241,9,0,0,33,0,0,0,20,0,61,2,30,0,61,2,29,0,61,2,30,0,23,0,7,10,23,0,8,10,0,0,34,0,4,0,241,9,4,0,33,0,0,0,97,1,62,2,254,9,2,0,31,0,2,0,9,10,2,0,10,10,2,0,11,10,7,0,12,10,7,0,13,10,26,0,48,1,11,0,56,0,24,0,133,0,61,2,85,0,7,0,14,10,7,0,15,10,7,0,27,4,
7,0,205,1,7,0,16,10,7,0,17,10,0,0,214,6,7,0,104,0,4,0,18,10,60,2,19,10,63,2,57,0,64,2,1,0,4,0,7,0,58,2,8,0,58,2,29,0,58,2,30,0,61,2,20,10,61,2,21,10,56,2,22,10,56,2,23,10,4,0,33,0,4,0,24,10,65,2,2,0,4,0,25,10,4,0,26,10,66,2,3,0,4,0,89,0,0,0,4,0,65,2,27,10,95,1,29,0,26,0,89,0,101,0,112,1,26,0,63,3,
67,2,254,9,0,0,97,1,0,0,2,0,18,1,204,9,7,0,28,10,23,0,29,10,23,0,30,10,4,0,31,0,4,0,31,10,4,0,33,0,4,0,32,10,4,0,33,10,4,0,21,0,4,0,69,3,0,0,4,0,87,0,34,10,23,0,7,10,23,0,8,10,53,2,35,10,68,2,36,10,64,2,37,10,4,0,38,10,66,2,39,10,69,2,40,10,40,0,231,0,70,2,57,0,71,2,4,0,4,0,32,0,4,0,7,0,4,0,12,0,
4,0,13,0,72,2,4,0,4,0,32,0,7,0,7,0,7,0,12,0,7,0,13,0,73,2,1,0,0,0,7,0,74,2,4,0,4,0,32,0,7,0,41,10,7,0,12,0,7,0,13,0,75,2,1,0,7,0,42,10,76,2,1,0,7,0,43,10,77,2,3,0,4,0,32,0,0,0,4,0,0,0,44,10,78,2,1,0,54,0,45,10,79,2,1,0,126,0,45,10,80,2,1,0,67,0,45,10,81,2,1,0,230,0,45,10,82,2,1,0,
129,0,45,10,83,2,3,0,4,0,7,0,4,0,37,7,84,2,19,0,69,2,1,0,4,0,33,0,85,2,2,0,2,0,33,0,2,0,46,10,86,2,6,0,4,0,228,5,4,0,19,1,4,0,47,10,0,0,48,10,0,0,49,10,0,0,51,0,87,2,6,0,7,0,50,10,7,0,51,10,7,0,129,3,7,0,52,10,7,0,53,10,0,0,4,0,88,2,6,0,87,2,54,10,87,2,55,10,87,2,56,10,87,2,57,10,7,0,58,10,
7,0,59,10,89,2,5,0,7,0,117,6,4,0,60,10,7,0,61,10,7,0,62,10,7,0,63,10,90,2,6,0,7,0,75,3,7,0,76,3,7,0,184,2,7,0,205,1,7,0,27,4,0,0,4,0,91,2,6,0,7,0,75,3,7,0,76,3,7,0,184,2,7,0,205,1,7,0,27,4,0,0,4,0,92,2,2,0,4,0,232,5,0,0,64,10,93,2,16,0,2,0,65,10,2,0,66,10,2,0,48,7,2,0,67,10,2,0,68,10,
2,0,1,7,2,0,69,10,2,0,70,10,7,0,197,8,7,0,71,10,7,0,72,10,2,0,73,10,0,0,74,10,0,0,129,3,4,0,75,10,4,0,76,10,94,2,8,0,7,0,77,10,7,0,78,10,7,0,187,1,7,0,117,6,7,0,79,10,7,0,80,10,2,0,192,8,0,0,51,0,95,2,4,0,7,0,81,10,7,0,82,10,2,0,192,8,0,0,51,0,96,2,7,0,2,0,41,1,2,0,83,10,4,0,84,10,7,0,85,10,
7,0,86,10,0,0,87,10,0,0,20,0,97,2,3,0,7,0,150,8,7,0,88,10,7,0,89,10,98,2,3,0,7,0,90,10,7,0,91,10,7,0,27,0,99,2,4,0,0,0,90,0,100,2,92,10,4,0,19,1,4,0,20,1,101,2,6,0,0,0,93,10,100,2,121,4,4,0,19,1,4,0,20,1,4,0,94,10,0,0,4,0,102,2,8,0,2,0,95,10,2,0,96,10,0,0,97,10,0,0,164,2,0,0,98,10,100,2,121,4,
0,0,99,10,0,0,239,1,103,2,9,0,7,0,100,10,7,0,101,10,7,0,102,10,7,0,41,4,7,0,103,10,7,0,104,10,7,0,105,10,2,0,106,10,2,0,107,10,104,2,8,0,2,0,108,10,2,0,109,10,2,0,110,10,2,0,111,10,7,0,112,10,7,0,113,10,7,0,114,10,7,0,115,10,105,2,2,0,7,0,75,3,7,0,76,3,106,2,1,0,0,0,34,0,107,2,2,0,1,0,247,0,1,0,116,10,108,2,12,0,
0,0,117,10,0,0,69,2,0,0,118,10,0,0,119,10,2,0,48,7,2,0,120,10,7,0,164,6,7,0,121,10,7,0,122,10,7,0,84,1,7,0,184,2,0,0,127,0,109,2,2,0,11,0,123,10,11,0,124,10,110,2,14,0,0,0,143,8,0,0,31,0,0,0,192,8,0,0,117,6,0,0,69,2,0,0,41,1,0,0,125,10,0,0,126,10,7,0,127,10,7,0,128,10,7,0,150,8,7,0,129,10,7,0,130,10,0,0,127,0,
111,2,8,0,7,0,131,10,7,0,44,1,7,0,129,3,7,0,96,8,7,0,132,10,7,0,28,8,7,0,133,10,4,0,31,0,112,2,4,0,2,0,134,10,2,0,135,10,2,0,136,10,0,0,51,0,113,2,8,0,7,0,137,10,7,0,213,3,7,0,138,10,7,0,139,10,0,0,4,0,7,0,140,10,7,0,141,10,7,0,142,10,114,2,6,0,2,0,143,10,2,0,144,10,7,0,145,10,7,0,146,10,7,0,147,10,7,0,148,10,
115,2,2,0,0,0,149,10,0,0,150,10,116,2,1,0,0,0,202,1,117,2,2,0,4,0,151,10,4,0,152,10,118,2,1,0,0,0,247,0,119,2,2,0,120,2,153,10,121,2,154,10,122,2,15,0,119,2,9,0,4,0,155,10,7,0,156,10,7,0,157,10,7,0,158,10,7,0,159,10,7,0,160,10,7,0,161,10,7,0,162,10,7,0,163,10,7,0,164,10,7,0,165,10,7,0,166,10,0,0,167,10,0,0,30,2,123,2,8,0,
119,2,9,0,147,0,181,2,4,0,168,10,4,0,169,10,7,0,170,10,4,0,171,10,4,0,172,10,0,0,4,0,124,2,1,0,119,2,9,0,125,2,5,0,119,2,9,0,4,0,173,10,4,0,174,10,7,0,44,1,7,0,175,10,126,2,6,0,119,2,9,0,147,0,181,2,4,0,168,10,4,0,169,10,4,0,171,10,0,0,4,0,127,2,3,0,119,2,9,0,4,0,22,8,0,0,4,0,128,2,5,0,119,2,9,0,4,0,176,10,
1,0,31,0,1,0,177,10,0,0,51,0,129,2,7,0,119,2,9,0,4,0,176,10,4,0,178,10,4,0,187,1,4,0,177,10,4,0,179,10,0,0,4,0,130,2,3,0,119,2,9,0,4,0,180,10,4,0,176,10,131,2,5,0,119,2,9,0,4,0,169,4,4,0,181,10,4,0,182,10,4,0,183,10,132,2,3,0,119,2,9,0,4,0,45,1,0,0,4,0,133,2,3,0,0,0,184,10,4,0,31,0,0,0,4,0,134,2,4,0,
4,0,31,0,4,0,185,10,4,0,186,10,0,0,4,0,135,2,13,0,119,2,9,0,2,0,187,10,0,0,51,0,4,0,188,10,7,0,238,3,4,0,43,9,2,0,145,3,2,0,171,10,2,0,189,10,2,0,190,10,136,2,191,10,4,0,192,10,0,0,193,10,137,2,2,0,0,0,194,10,0,0,20,0,138,2,3,0,2,0,195,10,2,0,196,10,0,0,4,0,139,2,1,0,0,0,34,0,140,2,2,0,0,0,197,10,7,0,198,10,
141,2,12,0,7,0,199,10,7,0,200,10,7,0,201,10,4,0,202,10,7,0,203,10,7,0,204,10,7,0,205,10,4,0,206,10,4,0,207,10,4,0,208,10,4,0,209,10,4,0,210,10,142,2,2,0,0,0,197,10,0,0,211,10,143,2,3,0,0,0,212,10,0,0,1,7,2,0,171,10,144,2,6,0,0,0,197,10,0,0,213,10,0,0,33,0,0,0,214,10,0,0,51,0,7,0,215,10,145,2,5,0,4,0,247,0,4,0,33,0,
0,0,154,2,0,0,216,10,0,0,217,10,146,2,3,0,4,0,218,10,4,0,116,6,0,0,219,10,147,2,2,0,4,0,145,3,0,0,219,10,148,2,1,0,0,0,219,10,149,2,1,0,0,0,174,9,150,2,2,0,4,0,247,0,0,0,154,2,151,2,1,0,0,0,34,0,152,2,2,0,7,0,220,10,7,0,221,10,153,2,5,0,153,2,29,0,153,2,30,0,7,0,222,10,0,0,34,0,0,0,4,0,154,2,3,0,153,2,29,0,
153,2,30,0,0,0,34,0,155,2,3,0,23,0,157,2,7,0,223,10,7,0,224,10,156,2,7,0,147,0,181,2,23,0,225,10,0,0,174,9,0,0,226,10,4,0,227,10,0,0,4,0,155,2,81,0,157,2,2,0,0,0,228,10,0,0,229,10,158,2,4,0,1,0,139,7,1,0,230,10,1,0,52,3,0,0,52,1,159,2,1,0,1,0,139,7,160,2,2,0,1,0,139,7,1,0,231,10,161,2,1,0,1,0,232,10,162,2,1,0,
4,0,233,10,163,2,1,0,7,0,234,10,164,2,1,0,7,0,235,10,165,2,1,0,7,0,179,5,166,2,1,0,0,0,236,10,167,2,1,0,1,0,247,0,168,2,1,0,1,0,237,10,169,2,2,0,1,0,166,9,1,0,238,10,170,2,1,0,1,0,237,10,171,2,1,0,1,0,239,10,172,2,1,0,1,0,166,9,173,2,1,0,1,0,166,9,174,2,2,0,1,0,142,8,1,0,144,8,175,2,1,0,1,0,240,10,176,2,1,0,
1,0,240,10,177,2,1,0,1,0,240,10,178,2,1,0,1,0,247,0,179,2,2,0,1,0,247,0,1,0,241,10,180,2,1,0,1,0,242,10,181,2,4,0,0,0,6,0,0,0,2,0,4,0,241,9,0,0,4,0,182,2,5,0,181,2,243,10,4,0,240,9,4,0,243,9,4,0,244,10,0,0,4,0,183,2,3,0,182,2,245,10,1,0,139,7,0,0,30,2,184,2,1,0,1,0,246,10,185,2,1,0,1,0,247,0,186,2,2,0,
1,0,247,10,1,0,247,0,187,2,2,0,1,0,247,10,1,0,247,0,188,2,1,0,1,0,247,0,189,2,1,0,1,0,247,0,190,2,1,0,1,0,247,0,191,2,1,0,1,0,247,0,192,2,1,0,1,0,247,0,193,2,1,0,1,0,247,0,194,2,1,0,1,0,247,0,195,2,1,0,1,0,247,0,196,2,1,0,1,0,247,0,197,2,4,0,1,0,247,0,12,0,248,10,12,0,139,7,0,0,10,4,198,2,4,0,12,0,139,7,
12,0,231,10,1,0,247,0,0,0,10,4,199,2,4,0,12,0,139,7,12,0,231,10,12,0,52,3,0,0,10,4,200,2,2,0,1,0,249,10,12,0,139,7,201,2,1,0,1,0,247,0,202,2,1,0,1,0,247,0,203,2,4,0,12,0,139,7,0,0,20,0,4,0,241,9,0,0,6,0,204,2,7,0,12,0,139,7,12,0,231,10,0,0,51,0,4,0,244,10,203,2,250,10,4,0,251,10,4,0,243,9,205,2,2,0,12,0,139,7,
12,0,231,10,206,2,1,0,12,0,139,7,207,2,4,0,1,0,34,4,1,0,252,10,1,0,36,4,1,0,253,10,208,2,2,0,12,0,231,10,12,0,247,0,209,2,1,0,12,0,231,10,210,2,1,0,12,0,231,10,211,2,2,0,12,0,171,10,12,0,172,10,212,2,2,0,12,0,139,7,12,0,231,10,213,2,1,0,1,0,254,10,214,2,4,0,0,0,6,0,2,0,255,10,2,0,235,9,4,0,241,9,215,2,1,0,4,0,0,11,
216,2,5,0,214,2,1,11,4,0,240,9,4,0,243,9,4,0,244,10,4,0,155,2,217,2,4,0,0,0,6,0,2,0,255,10,0,0,51,0,4,0,241,9,218,2,1,0,4,0,0,11,219,2,5,0,217,2,1,11,4,0,240,9,4,0,243,9,4,0,244,10,4,0,2,11,220,2,1,0,4,0,241,9,221,2,5,0,220,2,1,11,4,0,240,9,4,0,139,7,4,0,244,10,0,0,4,0,222,2,1,0,1,0,247,0,223,2,4,0,
12,0,40,0,12,0,139,7,12,0,247,0,0,0,10,4,224,2,1,0,12,0,247,0,225,2,6,0,12,0,139,7,12,0,3,11,12,0,4,11,12,0,5,11,12,0,6,11,0,0,20,0,226,2,6,0,0,0,6,0,2,0,255,10,2,0,235,9,4,0,241,9,4,0,33,0,0,0,4,0,227,2,5,0,226,2,1,11,4,0,240,9,4,0,244,10,4,0,243,9,0,0,4,0,228,2,1,0,7,0,7,11,226,1,52,0,225,1,228,4,
4,0,8,11,0,0,127,0,2,0,31,0,2,0,9,11,2,0,10,11,2,0,11,11,7,0,12,11,2,0,13,11,2,0,14,11,7,0,15,11,2,0,16,11,2,0,17,11,7,0,18,11,7,0,19,11,7,0,20,11,4,0,21,11,4,0,22,11,4,0,23,11,0,0,107,0,7,0,24,11,4,0,25,11,7,0,26,11,7,0,27,11,7,0,28,11,0,0,29,11,7,0,30,11,7,0,31,11,73,0,253,0,2,0,32,11,0,0,33,11,
0,0,34,11,7,0,35,11,4,0,36,11,7,0,37,11,7,0,38,11,4,0,39,11,4,0,33,0,7,0,40,11,7,0,41,11,7,0,42,11,228,2,43,11,4,0,33,1,7,0,44,11,7,0,45,11,7,0,46,11,7,0,47,11,7,0,48,11,7,0,49,11,7,0,50,11,4,0,51,11,7,0,52,11,229,2,50,0,4,0,33,0,2,0,53,11,2,0,54,11,2,0,202,1,2,0,55,11,2,0,56,11,2,0,57,11,2,0,58,11,
2,0,59,11,7,0,60,11,7,0,61,11,7,0,62,11,7,0,63,11,0,0,35,0,7,0,64,11,7,0,65,11,7,0,66,11,7,0,67,11,7,0,68,11,7,0,69,11,7,0,70,11,7,0,71,11,7,0,72,11,7,0,73,11,7,0,74,11,7,0,75,11,7,0,76,11,7,0,77,11,7,0,78,11,7,0,79,11,7,0,80,11,7,0,81,11,7,0,82,11,7,0,83,11,7,0,84,11,7,0,85,11,7,0,86,11,7,0,87,11,
230,0,137,7,7,0,88,11,4,0,183,4,7,0,89,11,7,0,90,11,7,0,91,11,0,0,127,0,7,0,92,11,0,0,107,0,54,0,93,11,7,0,94,11,0,0,4,0,156,0,5,0,67,0,49,3,7,0,95,11,7,0,96,11,2,0,33,0,0,0,51,0,230,2,1,0,7,0,248,3,231,2,2,0,229,0,118,4,23,0,119,4,232,2,54,0,4,0,78,3,4,0,97,11,233,2,98,11,234,2,99,11,0,0,155,2,0,0,100,11,
2,0,101,11,7,0,102,11,0,0,103,11,7,0,104,11,7,0,105,11,7,0,106,11,7,0,107,11,7,0,250,2,7,0,251,2,7,0,229,2,7,0,245,2,7,0,249,2,2,0,177,4,0,0,108,11,2,0,109,11,7,0,110,11,7,0,111,11,0,0,112,11,0,0,251,0,0,0,172,3,0,0,113,11,230,2,114,11,4,0,115,11,4,0,182,4,7,0,116,11,7,0,117,11,7,0,118,11,7,0,119,11,2,0,120,11,2,0,121,11,
2,0,122,11,2,0,123,11,2,0,124,11,2,0,125,11,2,0,126,11,2,0,127,11,235,2,128,11,7,0,129,11,7,0,130,11,231,2,131,11,229,0,118,4,23,0,119,4,67,0,132,11,156,0,22,3,7,0,133,11,7,0,134,11,7,0,135,11,4,0,136,11,236,2,5,0,236,2,29,0,236,2,30,0,0,0,34,0,0,0,33,0,0,0,238,0,237,2,5,0,237,2,29,0,237,2,30,0,0,0,34,0,0,0,33,0,0,0,238,0,
186,1,1,0,7,0,137,11,238,2,5,0,2,0,138,11,2,0,161,1,7,0,140,6,0,0,26,8,0,0,30,2,239,2,5,0,10,0,139,11,10,0,140,11,1,0,141,11,1,0,142,11,1,0,24,0,240,2,3,0,67,0,143,11,67,0,144,11,239,2,81,0,54,0,107,0,26,0,89,0,101,0,112,1,13,0,219,6,241,2,145,11,2,0,31,0,2,0,235,5,4,0,146,11,4,0,147,11,4,0,148,11,0,0,234,5,54,0,85,0,
54,0,219,9,54,0,149,11,54,0,150,11,54,0,151,11,73,0,253,0,63,0,244,0,63,0,152,11,57,0,153,11,11,0,17,1,18,1,204,9,46,0,205,0,43,0,146,0,11,0,154,11,23,0,254,0,23,0,126,4,23,0,155,11,23,0,156,11,23,0,65,1,23,0,157,11,23,0,158,11,4,0,247,0,4,0,159,11,129,0,18,4,0,0,160,11,4,0,26,4,4,0,161,11,7,0,154,0,7,0,162,11,7,0,155,0,7,0,163,11,
7,0,164,11,7,0,97,6,7,0,165,11,7,0,157,0,7,0,166,11,7,0,158,0,7,0,167,11,7,0,159,0,7,0,168,11,7,0,93,6,7,0,165,0,4,0,30,7,2,0,33,0,2,0,169,11,2,0,170,11,2,0,136,0,2,0,174,3,2,0,178,3,2,0,171,11,0,0,64,7,0,0,172,11,2,0,173,11,2,0,174,11,2,0,175,11,2,0,176,11,2,0,160,0,0,0,177,11,0,0,178,11,2,0,198,2,0,0,233,2,
0,0,179,11,7,0,180,11,7,0,181,11,2,0,91,1,2,0,182,11,0,0,107,0,7,0,93,8,2,0,183,11,2,0,0,8,2,0,184,11,0,0,185,11,0,0,186,11,23,0,134,0,23,0,187,11,23,0,188,11,23,0,189,11,229,2,190,11,232,2,191,11,67,0,192,11,226,1,193,11,23,0,194,11,242,2,195,11,243,2,196,11,7,0,197,11,147,0,198,11,0,0,199,11,0,0,200,11,0,0,201,11,1,0,202,11,0,0,203,11,
40,0,231,0,238,2,56,8,91,1,204,11,240,2,205,11,107,1,206,11,244,2,57,0,245,2,14,0,245,2,29,0,245,2,30,0,54,0,85,0,7,0,93,6,7,0,123,7,7,0,94,6,7,0,202,1,0,0,34,0,4,0,200,8,4,0,201,8,4,0,207,11,2,0,31,0,2,0,95,4,7,0,95,6,246,2,5,0,2,0,31,0,2,0,47,10,2,0,33,0,2,0,208,11,26,0,48,1,247,2,3,0,4,0,2,7,4,0,209,11,
246,2,17,1,39,0,3,0,4,0,41,1,4,0,210,11,11,0,17,1,248,2,6,0,7,0,99,0,7,0,30,1,7,0,127,1,2,0,69,8,0,0,51,0,7,0,211,11,249,2,5,0,7,0,99,0,7,0,7,11,7,0,212,11,7,0,213,11,7,0,30,1,250,2,5,0,54,0,214,11,122,0,36,0,7,0,232,2,7,0,215,11,0,0,35,0,251,2,3,0,7,0,216,11,4,0,217,11,4,0,218,11,252,2,7,0,4,0,219,11,
4,0,245,7,4,0,220,11,7,0,221,11,7,0,222,11,7,0,223,11,0,0,35,0,253,2,8,0,253,2,29,0,253,2,30,0,54,0,181,1,4,0,254,8,2,0,33,0,2,0,247,0,7,0,30,1,7,0,224,11,254,2,7,0,254,2,29,0,254,2,30,0,54,0,181,1,2,0,62,6,2,0,33,0,2,0,91,1,0,0,42,0,255,2,19,0,249,2,225,11,249,2,226,11,248,2,227,11,249,2,211,4,250,2,228,11,4,0,182,4,
7,0,30,1,7,0,180,4,7,0,229,11,4,0,219,11,4,0,230,11,7,0,222,11,7,0,223,11,7,0,41,1,7,0,231,11,0,0,4,0,4,0,232,11,2,0,33,0,2,0,233,11,0,3,17,0,7,0,238,3,7,0,234,11,7,0,216,11,7,0,235,11,7,0,236,11,7,0,237,11,7,0,238,11,7,0,239,11,7,0,240,11,7,0,241,11,7,0,242,11,7,0,243,11,7,0,244,11,4,0,33,0,4,0,245,11,2,0,210,0,
0,0,24,0,1,3,147,0,26,0,89,0,101,0,112,1,124,0,246,11,0,3,229,4,156,0,22,3,67,0,132,11,4,0,33,0,0,0,127,0,2,0,31,0,2,0,192,3,2,0,247,11,2,0,222,7,2,0,248,11,2,0,160,0,2,0,249,11,2,0,250,11,4,0,251,11,7,0,252,11,2,0,253,11,2,0,254,11,0,0,107,0,2,0,255,11,2,0,156,5,2,0,0,12,2,0,1,12,2,0,2,12,2,0,3,12,2,0,4,12,
2,0,5,12,2,0,6,12,2,0,206,4,2,0,202,4,2,0,171,10,2,0,7,12,2,0,8,12,2,0,57,11,2,0,58,11,2,0,9,12,2,0,10,12,2,0,11,12,2,0,12,12,7,0,13,12,7,0,14,12,7,0,15,12,7,0,16,12,7,0,17,12,7,0,18,12,7,0,19,12,7,0,179,4,7,0,80,1,7,0,180,4,7,0,188,4,7,0,20,12,7,0,21,12,7,0,22,12,7,0,23,12,7,0,24,12,7,0,25,12,
4,0,181,4,4,0,178,4,4,0,26,12,4,0,27,12,2,0,28,12,0,0,242,0,7,0,184,4,7,0,185,4,7,0,186,4,7,0,29,12,7,0,30,12,7,0,31,12,7,0,32,12,7,0,33,12,7,0,34,12,7,0,35,12,7,0,36,12,7,0,37,12,7,0,234,2,7,0,41,1,7,0,38,12,7,0,194,1,7,0,39,12,7,0,40,12,7,0,41,12,7,0,42,12,4,0,43,12,0,0,35,5,4,0,44,12,4,0,45,12,
7,0,61,3,7,0,46,12,7,0,47,12,7,0,48,12,7,0,49,12,7,0,50,12,7,0,51,12,7,0,85,11,7,0,83,11,7,0,84,11,7,0,52,12,7,0,53,12,4,0,54,12,0,0,214,5,7,0,55,12,7,0,56,12,7,0,57,12,7,0,58,12,7,0,59,12,7,0,60,12,7,0,61,12,7,0,62,12,7,0,63,12,7,0,64,12,7,0,65,12,7,0,66,12,7,0,67,12,7,0,68,12,7,0,69,12,7,0,70,12,
7,0,71,12,7,0,72,12,4,0,73,12,4,0,74,12,128,0,75,12,128,0,76,12,7,0,77,12,7,0,78,12,132,0,230,7,67,0,192,11,23,0,79,12,67,0,233,4,54,0,80,12,54,0,81,12,73,0,253,0,229,2,190,11,229,2,82,12,2,0,83,12,0,0,84,12,2,0,85,12,0,0,59,5,7,0,86,12,0,0,203,11,7,0,55,11,7,0,87,12,7,0,88,12,7,0,89,12,128,0,90,12,11,0,91,12,233,0,55,0,
233,0,29,0,233,0,30,0,1,3,92,12,255,2,93,12,252,2,143,0,2,3,94,12,11,0,95,12,3,3,96,12,3,3,97,12,23,0,98,12,23,0,99,12,208,1,100,12,247,0,101,12,247,0,102,12,54,0,103,12,31,1,104,12,54,0,85,0,23,0,149,3,0,0,34,0,7,0,210,4,7,0,186,1,7,0,105,12,7,0,106,12,4,0,183,4,4,0,107,12,4,0,33,0,4,0,181,4,4,0,108,12,4,0,109,12,4,0,110,12,
4,0,111,12,4,0,72,0,2,0,112,12,2,0,113,12,2,0,114,12,0,0,251,0,0,0,115,12,0,0,107,0,2,0,116,12,2,0,117,12,2,0,118,12,0,0,119,12,229,0,118,4,23,0,119,4,23,0,120,12,251,2,121,12,4,0,122,12,4,0,123,12,4,3,124,12,213,1,224,8,5,3,125,12,7,0,126,12,7,0,127,12,11,0,73,4,233,0,128,12,6,3,5,0,6,3,29,0,6,3,30,0,4,0,31,0,4,0,129,12,
11,0,17,1,7,3,8,0,7,3,29,0,7,3,30,0,4,0,162,2,4,0,78,3,4,0,130,9,4,0,33,0,11,0,130,12,23,0,131,12,229,0,25,0,229,0,29,0,229,0,30,0,4,0,33,0,4,0,16,0,4,0,132,12,4,0,133,12,4,0,134,12,4,0,135,12,4,0,136,12,4,0,137,12,0,0,4,0,4,0,78,3,4,0,91,1,2,0,26,3,0,0,42,0,0,0,34,0,0,0,138,12,0,0,231,5,0,0,98,10,
0,0,139,12,4,0,140,12,0,0,127,0,23,0,141,12,2,3,94,12,11,0,95,12,8,3,12,0,26,0,89,0,101,0,112,1,4,0,33,0,4,0,78,3,218,0,63,8,4,0,90,4,4,0,132,7,129,0,18,4,2,0,26,4,2,0,142,12,9,3,57,0,11,0,73,4,10,3,3,0,229,0,118,4,23,0,119,4,11,0,143,12,11,3,14,0,156,0,22,3,67,0,49,3,54,0,144,12,67,0,145,12,0,0,4,0,7,0,146,12,
10,3,131,11,229,0,118,4,23,0,119,4,4,0,147,12,2,0,148,12,2,0,149,12,4,0,33,0,7,0,244,2,242,2,18,0,2,0,31,0,2,0,55,11,4,0,33,0,4,0,150,12,2,0,151,12,0,0,51,0,7,0,234,2,7,0,42,3,7,0,152,12,7,0,153,12,7,0,154,12,7,0,155,12,7,0,156,12,7,0,157,12,7,0,158,12,7,0,159,12,0,0,127,0,12,3,131,11,243,2,37,0,54,0,160,12,54,0,161,12,
2,0,31,0,2,0,149,12,4,0,33,0,7,0,162,12,0,0,163,12,0,0,20,0,7,0,164,12,7,0,165,12,7,0,166,12,7,0,167,12,7,0,168,12,7,0,169,12,7,0,170,12,7,0,171,12,7,0,172,12,7,0,173,12,7,0,174,12,7,0,175,12,7,0,176,12,7,0,177,12,7,0,178,12,7,0,179,12,7,0,180,12,7,0,181,12,7,0,182,12,7,0,183,12,7,0,184,12,7,0,185,12,7,0,186,12,7,0,187,12,
7,0,188,12,7,0,189,12,7,0,190,12,7,0,191,12,11,0,192,12,13,3,19,0,4,0,31,0,4,0,193,12,4,0,194,12,4,0,195,12,4,0,196,12,4,0,197,12,4,0,198,12,7,0,199,12,4,0,200,12,4,0,201,12,4,0,161,1,4,0,202,12,4,0,203,12,4,0,204,12,4,0,205,12,4,0,206,12,4,0,207,12,4,0,208,12,11,0,145,1,14,3,9,0,4,0,209,12,7,0,210,12,7,0,211,12,7,0,212,12,
4,0,213,12,2,0,33,0,0,0,51,0,7,0,201,1,0,0,107,0,15,3,15,0,15,3,29,0,15,3,30,0,0,0,34,0,129,0,49,7,93,1,50,7,4,0,30,7,4,0,214,12,4,0,215,12,4,0,43,7,4,0,44,7,4,0,216,12,4,0,48,7,7,0,45,7,24,0,133,0,252,0,217,12,16,3,6,0,16,3,29,0,16,3,30,0,0,0,34,0,0,0,218,12,4,0,219,12,0,0,107,0,78,1,5,0,2,0,33,0,
0,0,220,12,0,0,221,12,0,0,222,12,0,0,20,0,100,2,22,0,0,0,223,12,0,0,45,1,0,0,224,12,0,0,33,0,0,0,143,8,0,0,225,12,0,0,226,12,0,0,227,12,2,0,228,12,2,0,229,12,7,0,230,12,0,0,231,12,0,0,232,12,0,0,233,12,0,0,4,0,0,0,240,6,78,1,234,12,0,0,235,12,0,0,236,12,168,0,237,12,169,0,238,12,170,0,239,12,17,3,17,0,100,2,92,10,0,0,154,2,
2,0,27,4,2,0,205,1,2,0,153,12,2,0,33,0,7,0,240,12,7,0,241,12,4,0,242,12,0,0,243,12,0,0,244,12,0,0,245,12,0,0,246,12,0,0,247,12,0,0,248,12,0,0,4,0,54,0,249,12,18,3,86,0,100,2,92,10,11,0,65,0,13,3,250,12,4,0,186,1,4,0,19,1,4,0,20,1,7,0,251,12,4,0,252,12,4,0,253,12,4,0,254,12,4,0,255,12,2,0,33,0,2,0,8,11,7,0,0,13,
4,0,1,13,2,0,2,13,2,0,41,1,4,0,3,13,4,0,4,13,4,0,5,13,4,0,6,13,2,0,224,12,2,0,223,12,2,0,7,13,2,0,143,8,0,0,8,13,0,0,9,13,4,0,10,13,4,0,247,0,2,0,11,13,0,0,12,13,0,0,13,13,87,0,14,13,23,0,157,2,2,0,15,13,0,0,87,1,7,0,16,13,7,0,17,13,7,0,18,13,7,0,19,13,4,0,20,13,7,0,21,13,2,0,154,9,2,0,22,13,
2,0,23,13,2,0,24,13,2,0,25,13,0,0,26,13,7,0,27,13,7,0,28,13,0,0,29,13,4,0,30,13,2,0,31,13,0,0,192,6,0,0,32,13,7,0,33,13,7,0,34,13,0,0,35,13,0,0,36,13,0,0,37,13,0,0,38,13,2,0,39,13,2,0,40,13,2,0,41,13,7,0,42,13,7,0,43,13,7,0,44,13,4,0,45,13,7,0,46,13,0,0,47,13,0,0,239,1,2,0,48,13,17,3,49,13,4,0,50,13,
2,0,51,13,2,0,132,7,23,0,243,6,2,0,52,13,2,0,240,6,2,0,53,13,2,0,54,13,7,0,55,13,4,0,56,13,128,0,57,13,4,0,58,13,4,0,59,13,19,3,7,0,19,3,29,0,19,3,30,0,4,0,162,2,0,0,34,0,4,0,33,0,54,0,230,3,24,0,133,0,20,3,3,0,4,0,60,13,2,0,43,2,0,0,51,0,21,3,1,0,131,0,94,9,22,3,14,0,131,0,94,9,21,3,61,13,4,0,62,13,
0,0,127,0,138,0,63,13,128,0,64,13,11,0,65,13,0,0,66,13,4,0,161,1,4,0,67,13,4,0,68,13,7,0,69,13,0,0,107,0,20,3,81,0,23,3,14,0,22,3,70,13,2,0,33,0,2,0,71,13,2,0,72,13,2,0,73,13,2,0,74,13,4,0,247,0,126,0,75,13,126,0,76,13,126,0,116,4,7,0,77,13,7,0,78,13,4,0,5,8,0,0,4,0,24,3,4,0,0,0,79,13,0,0,30,2,126,0,80,13,
147,0,81,13,25,3,6,0,2,0,41,1,2,0,16,0,2,0,82,13,2,0,62,6,4,0,33,0,7,0,42,1,26,3,15,0,2,0,33,0,2,0,83,13,2,0,84,13,2,0,85,13,25,3,86,13,11,0,87,13,7,0,88,13,0,0,35,0,4,0,89,13,4,0,90,13,4,0,1,12,4,0,91,13,241,0,194,6,54,0,58,6,54,0,92,13,27,3,20,0,22,3,70,13,4,0,161,1,4,0,93,13,4,0,94,2,4,0,94,13,
7,0,95,13,4,0,96,13,7,0,97,13,7,0,98,13,7,0,99,13,4,0,95,2,4,0,141,2,7,0,142,2,7,0,96,2,7,0,97,2,7,0,98,2,7,0,99,2,128,0,143,2,128,0,100,13,54,0,101,13,28,3,1,0,22,3,70,13,29,3,5,0,128,0,102,13,4,0,41,1,7,0,42,1,12,0,91,2,0,0,30,2,30,3,3,0,22,3,70,13,4,0,33,0,4,0,247,0,31,3,3,0,22,3,70,13,4,0,33,0,
0,0,4,0,32,3,3,0,22,3,70,13,4,0,33,0,0,0,4,0,33,3,3,0,22,3,70,13,4,0,33,0,0,0,4,0,34,3,4,0,22,3,70,13,0,0,33,0,0,0,20,0,4,0,94,13,35,3,10,0,0,0,103,13,0,0,104,13,0,0,105,13,0,0,31,0,0,0,107,0,7,0,117,6,7,0,106,13,7,0,49,2,7,0,248,5,54,0,107,13,36,3,8,0,11,0,87,13,4,0,33,0,4,0,108,13,7,0,109,13,
0,0,4,0,128,0,110,13,128,0,111,13,35,3,112,13,37,3,1,0,128,0,113,13,38,3,32,0,4,0,41,1,7,0,129,2,7,0,225,1,7,0,127,1,7,0,53,2,7,0,62,2,4,0,245,1,4,0,33,0,0,0,4,0,7,0,114,13,7,0,115,13,4,0,116,13,7,0,117,13,4,0,118,13,7,0,119,13,7,0,120,13,4,0,121,13,7,0,122,13,0,0,123,13,0,0,124,13,0,0,125,13,0,0,126,13,7,0,127,13,
4,0,128,13,7,0,129,13,7,0,130,13,7,0,131,13,7,0,132,13,7,0,133,13,7,0,134,13,7,0,135,13,39,3,136,13,40,3,13,0,0,0,137,13,0,0,33,0,0,0,138,13,0,0,139,13,0,0,111,6,0,0,155,2,2,0,140,13,7,0,141,13,7,0,142,13,7,0,143,13,7,0,144,13,7,0,145,13,7,0,146,13,41,3,13,0,0,0,31,0,0,0,87,1,0,0,147,13,7,0,148,13,7,0,149,13,7,0,150,13,
7,0,151,13,0,0,152,13,0,0,168,2,7,0,153,13,7,0,154,13,7,0,155,13,7,0,156,13,42,3,6,0,4,0,111,6,2,0,157,13,2,0,158,13,4,0,159,13,4,0,160,13,4,0,161,13,43,3,95,0,34,3,162,13,34,3,163,13,27,3,145,11,29,3,164,13,30,3,165,13,31,3,166,13,32,3,167,13,33,3,168,13,28,3,169,13,7,0,170,13,7,0,171,13,0,0,172,13,0,0,173,13,0,0,89,13,0,0,174,13,
0,0,175,13,0,0,176,13,0,0,177,13,0,0,178,13,7,0,179,13,2,0,180,13,0,0,181,13,0,0,182,13,0,0,183,13,0,0,184,13,2,0,185,13,7,0,186,13,0,0,187,13,0,0,188,13,0,0,242,0,36,3,189,13,37,3,190,13,23,3,191,13,24,3,192,13,26,3,193,13,7,0,194,13,7,0,195,13,2,0,196,13,0,0,197,13,0,0,198,13,0,0,199,13,0,0,200,13,0,0,201,13,0,0,202,13,0,0,203,13,
0,0,204,13,2,0,157,13,2,0,205,13,2,0,206,13,2,0,158,13,2,0,207,13,2,0,208,13,2,0,209,13,2,0,210,13,0,0,4,0,0,0,211,13,0,0,212,13,2,0,213,13,0,0,214,13,0,0,215,13,0,0,216,13,0,0,217,13,0,0,218,13,0,0,219,13,0,0,220,13,0,0,221,13,0,0,222,13,0,0,223,13,0,0,224,13,0,0,225,13,0,0,226,13,0,0,227,13,0,0,228,13,0,0,229,13,2,0,230,13,
4,0,231,13,7,0,232,13,7,0,233,13,38,3,234,13,40,3,235,13,41,3,236,13,7,0,237,13,0,0,238,13,212,0,239,13,42,3,240,13,2,0,241,13,0,0,242,13,0,0,243,13,0,0,244,13,0,0,245,13,0,0,246,13,7,0,247,13,7,0,248,13,7,0,249,13,7,0,250,13,44,3,9,0,7,0,251,13,0,0,252,13,0,0,253,13,2,0,33,0,0,0,254,13,0,0,255,13,0,0,0,14,0,0,1,14,0,0,4,0,
45,3,4,0,7,0,232,2,4,0,33,0,4,0,2,14,0,0,35,0,46,3,4,0,7,0,3,14,7,0,4,14,7,0,5,14,7,0,6,14,47,3,10,0,7,0,7,14,7,0,8,14,7,0,9,14,7,0,10,14,7,0,11,14,4,0,12,14,0,0,13,14,0,0,14,14,0,0,24,0,48,3,15,14,49,3,6,0,7,0,16,14,7,0,17,14,7,0,18,14,4,0,19,14,4,0,33,0,4,0,20,14,50,3,72,0,4,0,33,0,
4,0,21,14,4,0,22,14,4,0,23,14,7,0,24,14,7,0,25,14,7,0,26,14,4,0,27,14,7,0,28,14,7,0,29,14,4,0,30,14,4,0,31,14,4,0,32,14,7,0,33,14,7,0,34,14,7,0,35,14,7,0,36,14,7,0,37,14,7,0,38,14,7,0,39,14,7,0,40,14,4,0,41,14,4,0,42,14,7,0,43,14,7,0,44,14,4,0,45,14,4,0,46,14,7,0,47,14,7,0,48,14,7,0,49,14,7,0,50,14,
7,0,51,14,4,0,52,14,4,0,53,14,4,0,54,14,7,0,55,14,7,0,56,14,7,0,57,14,0,0,58,14,0,0,8,1,7,0,59,14,7,0,60,14,7,0,61,14,7,0,62,14,7,0,63,14,7,0,64,14,7,0,65,14,7,0,66,14,7,0,67,14,7,0,68,14,7,0,69,14,4,0,214,10,4,0,70,14,4,0,71,14,4,0,56,13,7,0,215,10,7,0,72,14,4,0,73,14,4,0,74,14,4,0,75,14,4,0,76,14,
4,0,77,14,4,0,78,14,7,0,79,14,7,0,80,14,7,0,81,14,7,0,82,14,7,0,83,14,4,0,84,14,49,3,85,14,7,0,130,6,7,0,86,14,51,3,2,0,7,0,87,14,0,0,4,0,52,3,2,0,4,0,88,14,4,0,69,2,53,3,4,0,4,0,31,0,4,0,89,14,0,0,33,0,0,0,238,0,241,0,58,0,26,0,89,0,101,0,112,1,13,0,219,6,54,0,230,3,93,1,90,14,241,0,91,14,23,0,9,0,
84,1,42,7,11,0,145,1,54,3,92,14,4,0,30,7,4,0,93,14,0,0,107,0,2,0,33,0,0,0,74,7,0,0,186,11,95,1,91,7,55,3,94,14,43,3,95,14,11,0,199,8,46,3,96,14,18,3,159,4,14,3,97,14,23,0,226,0,23,0,98,14,53,3,99,14,11,0,100,14,11,0,101,14,11,0,102,14,11,0,103,14,11,0,104,14,58,0,105,14,0,0,106,14,4,0,107,14,23,0,108,14,44,3,109,14,18,1,204,9,
148,0,182,2,45,3,110,14,11,0,111,14,225,0,112,14,225,0,113,14,168,0,237,12,169,0,238,12,170,0,114,14,11,3,115,14,40,0,231,0,23,0,116,14,67,0,117,14,24,0,118,14,4,0,119,14,4,0,120,14,47,3,121,14,50,3,53,7,51,3,122,14,52,3,123,14,56,3,57,0,11,0,124,14,240,0,24,0,26,0,89,0,23,0,125,14,23,0,126,14,23,0,127,14,23,0,239,0,241,0,194,6,2,0,33,0,2,0,128,14,
2,0,129,14,0,0,255,0,0,0,225,11,0,0,130,14,0,0,131,14,0,0,132,14,0,0,133,14,0,0,134,14,0,0,135,14,0,0,136,14,0,0,10,4,57,3,137,14,58,3,138,14,11,0,139,14,59,3,140,14,40,0,231,0,60,3,6,0,60,3,29,0,60,3,30,0,60,3,141,14,61,3,142,14,2,0,33,0,2,0,69,8,62,3,7,0,62,3,29,0,62,3,30,0,60,3,143,14,60,3,144,14,2,0,14,13,2,0,33,0,
0,0,4,0,63,3,3,0,23,0,125,14,23,0,126,14,23,0,127,14,64,3,5,0,64,3,29,0,64,3,30,0,0,0,145,14,1,0,33,0,0,0,30,2,65,3,21,0,65,3,29,0,65,3,30,0,66,3,146,14,67,3,147,14,0,0,148,14,0,0,149,14,4,0,150,14,4,0,151,14,4,0,65,10,4,0,66,10,4,0,152,14,4,0,153,14,2,0,154,14,2,0,33,0,2,0,37,7,0,0,24,0,4,0,155,14,11,0,156,14,
23,0,65,3,23,0,157,14,68,3,57,0,69,3,3,0,69,3,29,0,69,3,30,0,0,0,97,1,70,3,15,0,70,3,29,0,70,3,30,0,71,3,146,14,0,0,158,14,4,0,159,14,4,0,33,0,4,0,160,14,4,0,161,14,4,0,162,14,4,0,163,14,0,0,164,14,4,0,165,14,4,0,166,14,24,0,76,0,72,3,167,14,73,3,5,0,73,3,29,0,73,3,30,0,0,0,34,0,7,0,168,14,0,0,4,0,74,3,5,0,
74,3,29,0,74,3,30,0,0,0,169,14,2,0,205,1,0,0,251,0,75,3,6,0,2,0,170,14,2,0,171,14,2,0,172,14,2,0,173,14,2,0,33,0,0,0,51,0,76,3,3,0,77,3,174,14,0,0,175,14,0,0,238,0,78,3,25,0,78,3,29,0,78,3,30,0,60,3,143,14,60,3,144,14,60,3,176,14,60,3,177,14,240,0,178,14,79,3,248,6,0,0,240,0,0,0,179,14,2,0,180,14,2,0,181,14,2,0,182,14,
0,0,183,14,0,0,131,14,2,0,33,0,2,0,184,14,0,0,51,0,80,3,146,14,75,3,185,14,23,0,186,14,23,0,239,0,23,0,187,14,23,0,188,14,76,3,81,0,81,3,6,0,0,0,189,14,79,3,190,14,4,0,191,14,4,0,192,14,58,0,193,14,65,3,194,14,57,3,33,0,57,3,29,0,57,3,30,0,71,0,243,0,79,3,195,14,79,3,196,14,2,0,181,14,2,0,182,14,4,0,197,14,0,0,35,0,2,0,198,14,
2,0,199,14,2,0,200,14,2,0,33,0,2,0,65,10,2,0,66,10,2,0,130,14,2,0,133,14,2,0,201,14,2,0,202,14,82,3,146,14,23,0,203,14,23,0,204,14,23,0,205,14,23,0,206,14,23,0,207,14,23,0,187,14,23,0,208,14,83,3,209,14,58,3,210,14,84,3,211,14,0,0,212,14,11,0,213,14,81,3,81,0,85,3,9,0,85,3,29,0,85,3,30,0,113,0,214,14,23,0,215,14,0,0,216,14,0,0,217,14,
2,0,218,14,2,0,219,14,0,0,127,0,86,3,7,0,86,3,29,0,86,3,30,0,0,0,97,1,87,3,146,14,85,3,160,9,2,0,220,14,0,0,24,0,88,3,2,0,23,0,221,14,86,3,222,14,89,3,1,0,90,3,146,14,91,3,3,0,91,3,29,0,91,3,30,0,67,1,202,6,92,3,4,0,0,0,184,10,4,0,223,14,4,0,224,14,7,0,225,14,93,3,4,0,4,0,226,14,4,0,227,14,4,0,228,14,4,0,229,14,
94,3,7,0,7,0,230,14,7,0,231,14,7,0,198,7,7,0,199,7,7,0,184,2,7,0,232,14,4,0,180,6,95,3,9,0,4,0,254,10,7,0,140,10,7,0,141,10,7,0,142,10,7,0,137,10,7,0,213,3,7,0,138,10,4,0,33,0,0,0,4,0,96,3,10,0,0,0,196,9,0,0,233,14,67,1,202,6,2,0,197,9,2,0,143,8,2,0,234,14,2,0,235,14,2,0,236,14,0,0,237,14,0,0,52,1,97,3,13,0,
97,3,29,0,97,3,30,0,4,0,70,0,4,0,238,14,4,0,239,14,4,0,240,14,92,3,241,14,0,0,196,9,96,3,149,11,93,3,242,14,94,3,243,14,95,3,244,14,170,0,237,6,98,3,8,0,8,0,245,14,4,0,33,0,4,0,69,2,7,0,246,14,0,0,127,0,8,0,247,14,7,0,248,14,0,0,107,0,99,3,1,0,49,0,75,0,100,3,66,0,100,3,29,0,100,3,30,0,11,0,249,14,11,0,67,0,0,0,34,0,
4,0,33,0,4,0,31,0,4,0,37,0,7,0,79,1,7,0,250,14,7,0,251,14,7,0,239,14,7,0,240,14,4,0,252,14,4,0,155,2,4,0,253,14,4,0,254,14,7,0,91,10,7,0,255,14,7,0,64,7,2,0,0,15,2,0,1,15,4,0,2,15,4,0,3,15,97,3,4,15,73,0,253,0,241,0,194,6,54,0,5,15,148,0,182,2,150,1,6,15,23,0,221,6,7,0,7,15,7,0,8,15,100,3,9,15,100,3,10,15,
100,3,11,15,23,0,12,15,23,0,215,0,101,3,13,15,11,0,14,15,7,0,201,1,7,0,207,1,7,0,15,15,7,0,16,15,11,0,17,15,4,0,18,15,4,0,19,15,4,0,240,5,7,0,20,15,12,0,69,3,0,0,238,6,0,0,239,1,4,0,92,5,4,0,19,1,0,0,240,6,0,0,142,12,78,1,244,6,24,0,133,0,23,0,65,1,7,0,21,15,7,0,22,15,98,3,23,15,11,0,218,5,4,0,24,15,0,0,238,13,
99,3,81,0,102,3,6,0,102,3,29,0,102,3,30,0,23,0,25,15,23,0,26,15,100,3,27,15,4,0,28,15,103,3,5,0,103,3,29,0,103,3,30,0,0,0,34,0,4,0,91,1,4,0,33,0,104,3,2,0,105,3,29,15,106,3,30,15,55,3,23,0,23,0,31,15,23,0,32,15,11,0,154,11,23,0,12,15,23,0,33,15,23,0,215,0,100,3,34,15,0,0,35,15,0,0,36,15,0,0,37,15,4,0,38,15,4,0,39,15,
4,0,40,15,4,0,41,15,87,0,42,15,4,0,43,15,4,0,64,7,107,3,228,2,7,0,44,15,4,0,92,5,108,3,45,15,9,0,46,15,104,3,81,0,109,3,4,0,7,0,47,15,7,0,117,6,2,0,48,15,2,0,49,15,110,3,6,0,7,0,50,15,7,0,51,15,7,0,52,15,7,0,53,15,4,0,54,15,4,0,55,15,111,3,8,0,7,0,56,15,7,0,57,15,7,0,58,15,7,0,59,15,7,0,60,15,4,0,191,8,
4,0,171,10,4,0,61,15,112,3,2,0,7,0,62,15,0,0,4,0,113,3,7,0,7,0,63,15,7,0,64,15,4,0,161,1,4,0,65,15,7,0,8,15,7,0,66,15,7,0,67,15,114,3,2,0,7,0,151,10,7,0,152,10,115,3,19,0,0,0,68,15,214,0,69,15,4,0,70,15,7,0,71,15,7,0,179,5,7,0,72,15,7,0,73,15,7,0,74,15,7,0,75,15,7,0,76,15,7,0,77,15,7,0,78,15,7,0,79,15,
7,0,80,15,7,0,81,15,0,0,33,0,0,0,173,14,0,0,36,4,0,0,52,1,116,3,2,0,4,0,82,15,7,0,38,6,117,3,9,0,117,3,29,0,117,3,30,0,4,0,31,0,4,0,33,0,0,0,34,0,4,0,83,15,4,0,84,15,100,3,85,15,150,1,86,15,118,3,3,0,117,3,34,6,95,3,87,15,7,0,88,15,119,3,2,0,117,3,34,6,128,0,89,15,120,3,2,0,117,3,34,6,128,0,89,15,121,3,3,0,
117,3,34,6,7,0,90,15,7,0,51,10,122,3,1,0,117,3,34,6,123,3,3,0,117,3,34,6,7,0,91,15,0,0,4,0,124,3,9,0,117,3,34,6,7,0,131,10,7,0,44,1,7,0,129,3,7,0,101,7,7,0,51,10,7,0,92,15,7,0,93,15,4,0,31,0,125,3,3,0,125,3,29,0,125,3,30,0,128,0,89,15,126,3,2,0,117,3,34,6,23,0,94,15,49,0,1,0,10,0,95,15,127,3,9,0,127,3,29,0,
127,3,30,0,4,0,31,0,4,0,247,0,0,0,35,0,2,0,33,0,2,0,18,1,0,0,34,0,0,0,33,6,128,3,5,0,7,0,154,0,0,0,4,0,129,3,96,15,129,3,97,15,129,3,98,15,130,3,7,0,127,3,99,15,7,0,100,15,4,0,33,0,4,0,48,7,7,0,184,2,0,0,4,0,128,3,81,0,131,3,8,0,127,3,99,15,4,0,247,0,7,0,101,15,7,0,102,15,7,0,38,6,4,0,33,0,0,0,4,0,
128,3,81,0,132,3,4,0,127,3,99,15,4,0,33,0,4,0,103,15,128,3,81,0,133,3,12,0,127,3,99,15,7,0,104,15,7,0,105,15,7,0,150,8,4,0,33,0,4,0,247,0,7,0,106,15,4,0,48,7,7,0,184,2,4,0,240,5,0,0,4,0,128,3,81,0,134,3,5,0,127,3,99,15,4,0,155,0,4,0,33,0,7,0,107,15,128,3,81,0,135,3,10,0,127,3,99,15,4,0,224,1,4,0,33,0,7,0,108,15,
7,0,109,15,4,0,247,0,4,0,106,15,4,0,48,7,0,0,4,0,128,3,81,0,136,3,15,0,127,3,99,15,54,0,58,6,4,0,224,1,4,0,33,0,7,0,110,15,7,0,26,1,7,0,246,3,7,0,43,1,4,0,204,7,7,0,17,6,7,0,184,2,4,0,106,15,4,0,48,7,0,0,4,0,128,3,81,0,137,3,7,0,127,3,99,15,54,0,58,6,4,0,33,0,4,0,238,3,7,0,117,6,4,0,111,15,128,3,81,0,
138,3,8,0,127,3,99,15,7,0,26,1,7,0,246,3,7,0,43,1,4,0,204,7,4,0,33,0,0,0,4,0,128,3,81,0,101,3,22,0,26,0,89,0,0,0,90,0,39,0,91,0,11,0,171,2,39,0,112,15,73,0,253,0,7,0,201,1,7,0,113,15,7,0,207,1,7,0,114,15,7,0,115,15,7,0,187,1,2,0,161,1,2,0,171,1,0,0,4,0,8,0,116,15,11,0,228,2,11,0,117,15,11,0,101,14,11,0,118,15,
4,0,198,12,4,0,119,15,70,0,6,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,139,3,8,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,0,0,120,15,0,0,30,2,140,3,21,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,71,0,243,0,2,0,121,15,2,0,122,15,2,0,123,15,2,0,124,15,
2,0,118,10,0,0,4,0,0,0,33,0,0,0,125,15,11,0,178,1,4,0,152,1,4,0,126,15,26,0,127,15,11,0,128,15,141,3,57,0,142,3,22,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,71,0,243,0,23,0,129,15,224,0,130,15,0,0,217,14,2,0,33,0,2,0,131,15,2,0,132,15,2,0,133,15,0,0,134,15,0,0,24,0,0,0,135,15,4,0,180,6,0,0,136,15,
0,0,137,15,2,0,138,15,143,3,57,0,144,3,3,0,0,0,33,0,0,0,30,2,23,0,139,15,145,3,16,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,71,0,243,0,66,0,140,15,2,0,247,0,2,0,249,0,4,0,33,0,7,0,141,15,7,0,142,15,4,0,143,15,0,0,4,0,144,3,81,0,146,3,11,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,
0,0,242,0,2,0,249,0,2,0,33,0,0,0,4,0,66,0,140,15,71,0,243,0,147,3,2,0,4,0,33,0,0,0,35,0,148,3,2,0,4,0,33,0,0,0,35,0,149,3,2,0,4,0,33,0,0,0,35,0,150,3,28,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,71,0,243,0,7,0,45,4,7,0,46,4,2,0,122,15,2,0,194,9,2,0,144,15,2,0,145,15,4,0,33,0,
7,0,80,10,0,0,201,6,0,0,146,15,0,0,147,15,0,0,148,15,0,0,4,0,7,0,149,15,18,1,204,9,147,3,150,15,148,3,151,15,149,3,152,15,0,0,197,6,0,0,135,1,151,3,57,0,152,3,6,0,150,1,6,15,0,0,147,15,0,0,153,15,0,0,154,15,0,0,186,11,7,0,155,15,153,3,26,0,0,0,156,15,0,0,157,15,0,0,233,14,0,0,158,15,2,0,159,15,0,0,4,0,26,0,160,15,11,0,90,1,
0,0,161,15,0,0,162,15,10,0,163,15,4,0,164,15,4,0,165,15,4,0,166,15,4,0,167,15,2,0,168,15,0,0,87,1,2,0,31,0,2,0,33,0,2,0,169,15,2,0,121,14,0,0,170,15,0,0,168,2,4,0,180,6,2,0,171,15,0,0,172,15,154,3,7,0,153,3,173,15,113,0,174,15,2,0,175,15,0,0,24,0,112,0,166,1,2,0,176,15,0,0,68,4,155,3,6,0,156,3,29,0,156,3,30,0,0,0,177,15,
0,0,30,2,23,0,178,15,23,0,179,15,157,3,26,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,0,0,177,15,0,0,249,3,2,0,171,1,4,0,180,15,153,3,181,15,154,3,182,15,11,0,131,3,158,3,183,15,23,0,184,15,23,0,185,15,23,0,186,15,159,3,187,15,58,3,188,15,58,3,189,15,160,3,147,14,2,0,190,15,2,0,191,15,2,0,192,15,2,0,193,15,161,3,57,0,
162,3,2,0,4,0,33,0,0,0,4,0,163,3,35,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,126,0,223,1,147,0,181,2,167,0,194,15,166,0,195,15,18,1,204,9,7,0,149,15,7,0,45,4,7,0,46,4,7,0,80,10,7,0,196,15,7,0,197,15,0,0,247,0,0,0,248,0,0,0,198,15,0,0,199,15,0,0,200,15,0,0,201,15,0,0,202,15,0,0,143,15,0,0,148,15,
0,0,203,15,0,0,251,0,4,0,33,0,7,0,204,15,7,0,205,15,4,0,206,15,4,0,207,15,152,3,208,15,162,3,209,15,164,3,26,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,85,0,147,3,4,0,226,14,4,0,228,14,0,0,127,0,2,0,161,1,2,0,210,15,4,0,211,15,0,0,212,15,0,0,213,15,0,0,214,15,0,0,215,15,0,0,216,15,0,0,217,15,0,0,218,15,
0,0,219,15,0,0,220,15,0,0,221,15,2,0,222,15,0,0,192,6,165,3,57,0,166,3,10,0,26,0,89,0,11,0,223,15,11,0,224,15,11,0,225,15,11,0,226,15,11,0,227,15,4,0,161,1,4,0,228,15,0,0,229,15,0,0,230,15,167,3,11,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,166,3,39,1,2,0,161,1,2,0,231,15,0,0,127,0,11,0,232,15,168,3,8,0,
168,3,29,0,168,3,30,0,95,1,91,7,64,2,233,15,0,0,4,0,7,0,28,10,0,0,234,15,0,0,235,15,169,3,2,0,4,0,33,0,4,0,236,15,170,3,27,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,71,0,243,0,26,0,48,1,26,0,10,7,2,0,33,0,0,0,237,15,0,0,64,7,7,0,45,4,7,0,46,4,7,0,80,10,23,0,238,15,95,1,239,15,95,1,91,7,
0,0,240,15,4,0,241,15,2,0,242,15,0,0,243,15,0,0,244,15,95,1,245,15,18,1,204,9,169,3,209,15,171,3,57,0,172,3,7,0,172,3,29,0,172,3,30,0,4,0,246,15,4,0,37,0,0,0,247,15,4,0,92,14,4,0,31,0,173,3,14,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,23,0,248,15,23,0,249,15,0,0,250,15,0,0,251,15,4,0,210,15,4,0,252,15,
4,0,253,15,4,0,254,15,174,3,9,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,0,0,236,12,0,0,255,15,0,0,0,16,175,3,33,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,0,0,148,15,0,0,164,2,7,0,45,4,7,0,46,4,7,0,1,16,7,0,2,16,7,0,80,10,149,0,200,9,148,0,182,2,45,2,194,15,4,0,33,0,
2,0,247,0,2,0,201,6,4,0,3,16,7,0,75,15,7,0,84,1,7,0,117,6,0,0,4,0,7,0,4,16,7,0,5,16,4,0,6,16,2,0,7,16,0,0,239,1,4,0,143,15,0,0,214,5,7,0,149,15,152,3,208,15,176,3,6,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,177,3,6,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,
178,3,1,0,0,0,6,0,179,3,6,0,179,3,29,0,179,3,30,0,178,3,48,1,1,0,139,7,0,0,238,0,0,0,8,16,180,3,17,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,0,0,241,0,0,0,242,0,23,0,9,16,23,0,10,16,181,3,11,16,1,0,165,14,1,0,12,16,1,0,235,9,1,0,13,16,4,0,14,16,4,0,33,0,0,0,127,0,182,3,57,0,183,3,15,0,183,3,29,0,183,3,30,0,
0,0,15,16,1,0,40,0,1,0,33,0,0,0,42,0,4,0,16,16,4,0,17,16,0,0,18,16,7,0,19,16,7,0,150,8,7,0,20,16,7,0,21,16,7,0,22,16,0,0,127,0,184,3,15,0,26,0,89,0,101,0,112,1,101,3,13,15,7,0,23,16,7,0,24,16,7,0,25,16,7,0,26,16,7,0,113,15,7,0,27,16,7,0,28,16,7,0,29,16,7,0,201,1,7,0,207,1,2,0,33,0,0,0,251,0,185,3,6,0,
185,3,29,0,185,3,30,0,0,0,247,15,0,0,30,16,4,0,37,0,0,0,35,0,85,0,11,0,26,0,89,0,0,0,6,0,11,0,31,16,4,0,161,1,0,0,35,0,23,0,39,4,185,3,32,16,185,3,33,16,4,0,34,16,4,0,35,16,8,0,36,16,132,0,39,0,2,0,37,16,2,0,38,16,2,0,39,16,0,0,239,1,54,0,58,6,230,0,137,7,0,0,9,9,0,0,40,16,0,0,41,16,0,0,42,16,0,0,249,10,
0,0,43,16,0,0,44,16,2,0,45,16,7,0,46,16,7,0,155,0,7,0,47,16,7,0,48,16,7,0,159,4,7,0,160,4,7,0,161,4,7,0,49,16,7,0,50,16,7,0,51,16,7,0,52,16,7,0,53,16,7,0,54,16,7,0,50,12,7,0,41,12,7,0,55,16,7,0,56,16,7,0,57,16,7,0,58,16,7,0,59,16,7,0,60,16,7,0,61,16,7,0,62,16,7,0,63,16,7,0,64,16,186,3,6,0,7,0,159,4,
7,0,160,4,7,0,161,4,7,0,28,8,7,0,48,4,4,0,88,3,134,0,7,0,2,0,65,16,2,0,88,3,0,0,66,16,0,0,67,16,0,0,72,1,0,0,10,4,186,3,68,16,136,2,28,0,2,0,33,0,2,0,92,6,7,0,69,16,7,0,238,3,2,0,185,2,0,0,42,0,2,0,189,10,2,0,190,10,4,0,201,5,54,0,58,6,4,0,254,8,2,0,70,16,2,0,71,16,0,0,193,10,0,0,127,0,11,0,72,16,
7,0,73,16,7,0,74,16,2,0,75,16,2,0,76,16,2,0,77,16,0,0,68,4,7,0,78,16,7,0,79,16,7,0,80,16,0,0,35,5,134,0,111,5,128,0,81,16,230,0,60,0,26,0,89,0,101,0,112,1,13,0,219,6,7,0,223,9,7,0,224,9,7,0,90,15,7,0,51,10,7,0,50,10,7,0,82,16,7,0,83,16,7,0,84,16,7,0,85,16,0,0,107,0,7,0,86,16,7,0,87,16,7,0,88,16,7,0,89,16,
7,0,90,16,7,0,91,16,7,0,92,16,7,0,93,16,7,0,94,16,7,0,95,16,7,0,96,16,7,0,97,16,2,0,98,16,2,0,99,16,2,0,100,16,2,0,101,16,2,0,102,16,2,0,103,16,2,0,104,16,2,0,33,0,2,0,31,0,2,0,176,4,7,0,105,16,7,0,106,16,7,0,107,16,7,0,108,16,4,0,109,16,4,0,110,16,2,0,111,16,2,0,112,16,2,0,69,1,2,0,69,2,4,0,37,0,4,0,228,5,
4,0,44,1,4,0,19,1,7,0,113,16,7,0,191,4,0,0,127,0,147,0,181,2,95,1,91,7,73,0,253,0,126,0,180,2,134,0,111,5,40,0,231,0,0,0,74,7,0,0,30,2,120,2,13,0,7,0,154,0,7,0,97,6,7,0,155,0,4,0,33,0,0,0,40,16,0,0,41,16,0,0,42,16,0,0,249,10,4,0,31,0,7,0,123,7,7,0,114,16,7,0,115,16,54,0,181,1,121,2,9,0,134,0,116,16,7,0,90,15,
7,0,51,10,7,0,50,10,4,0,33,0,7,0,117,16,7,0,155,15,4,0,6,11,0,0,4,0,187,3,3,0,4,0,195,6,7,0,188,7,7,0,123,7,188,3,23,0,11,0,118,16,2,0,119,16,0,0,51,0,7,0,120,16,7,0,121,16,7,0,122,16,2,0,123,16,0,0,87,1,7,0,124,16,7,0,125,16,7,0,126,16,7,0,127,16,7,0,128,16,7,0,129,16,7,0,130,16,7,0,131,16,7,0,132,16,7,0,133,16,
7,0,134,16,7,0,135,16,7,0,136,16,7,0,137,16,7,0,138,16,46,2,6,0,7,0,139,16,7,0,140,16,7,0,141,16,7,0,142,16,4,0,195,6,4,0,33,0,47,2,26,0,47,2,29,0,47,2,30,0,0,0,34,0,7,0,143,16,7,0,144,16,7,0,141,16,7,0,142,16,7,0,224,1,4,0,145,16,4,0,155,2,46,2,146,16,7,0,147,16,7,0,188,7,4,0,33,0,4,0,148,16,4,0,149,16,7,0,104,0,
2,0,150,16,2,0,153,12,2,0,151,16,2,0,152,16,4,0,153,16,7,0,154,16,18,1,204,9,7,0,127,1,7,0,155,16,189,3,3,0,7,0,156,16,4,0,195,6,4,0,33,0,190,3,12,0,190,3,29,0,190,3,30,0,0,0,34,0,47,2,157,16,4,0,158,16,0,0,4,0,189,3,146,16,4,0,145,16,4,0,33,0,126,0,223,1,7,0,159,16,4,0,160,16,191,3,21,0,2,0,161,16,2,0,162,16,7,0,163,16,
2,0,164,16,2,0,165,16,2,0,166,16,2,0,167,16,2,0,168,16,2,0,169,16,7,0,72,9,2,0,170,16,2,0,191,1,4,0,171,16,4,0,172,16,4,0,173,16,4,0,174,16,7,0,126,1,4,0,175,16,4,0,176,16,7,0,177,16,7,0,178,16,192,3,16,0,4,0,33,0,4,0,179,16,4,0,180,16,4,0,181,16,4,0,182,16,7,0,183,16,47,2,184,16,4,0,185,16,7,0,186,16,7,0,187,16,7,0,84,1,
7,0,188,16,7,0,189,16,7,0,190,16,4,0,180,6,4,0,155,2,193,3,5,0,4,0,33,0,7,0,188,7,4,0,191,16,4,0,192,16,187,3,193,16,194,3,12,0,194,3,29,0,194,3,30,0,0,0,34,0,4,0,33,0,7,0,84,1,23,0,194,16,23,0,195,16,47,2,196,16,190,3,197,16,193,3,198,16,4,0,171,16,4,0,172,16,195,3,1,0,0,0,199,16,196,3,11,0,196,3,29,0,196,3,30,0,47,2,219,9,
0,0,4,0,0,0,34,0,4,0,200,16,4,0,79,4,4,0,201,16,4,0,202,16,4,0,203,16,4,0,204,16,197,3,6,0,197,3,29,0,197,3,30,0,4,0,205,16,4,0,102,0,4,0,103,0,0,0,4,0,198,3,7,0,4,0,108,3,2,0,206,16,2,0,33,0,23,0,207,16,23,0,215,0,4,0,208,16,0,0,4,0,44,2,13,0,191,3,160,9,188,3,209,16,23,0,194,16,23,0,195,16,193,3,198,16,192,3,210,16,
47,2,104,1,190,3,211,16,23,0,212,16,4,0,213,16,4,0,214,16,195,3,41,7,198,3,215,16,199,3,12,0,2,0,216,16,0,0,87,1,7,0,217,16,2,0,218,16,2,0,219,16,2,0,220,16,2,0,221,16,2,0,222,16,0,0,42,0,7,0,223,16,7,0,224,16,4,0,225,16,200,3,18,0,200,3,29,0,200,3,30,0,0,0,34,0,199,3,226,16,199,3,227,16,199,3,228,16,199,3,229,16,7,0,230,16,2,0,231,16,
2,0,232,16,2,0,233,16,2,0,234,16,2,0,235,16,2,0,236,16,2,0,237,16,2,0,238,16,2,0,239,16,0,0,42,0,201,3,11,0,0,0,240,16,0,0,241,16,0,0,242,16,0,0,243,16,0,0,244,16,0,0,245,16,0,0,246,16,0,0,238,0,2,0,247,16,2,0,248,16,7,0,249,16,202,3,12,0,0,0,250,16,0,0,251,16,0,0,252,16,0,0,253,16,0,0,254,16,0,0,255,16,0,0,0,17,0,0,1,17,
0,0,2,17,0,0,3,17,7,0,42,2,0,0,35,0,203,3,4,0,0,0,4,17,0,0,5,17,0,0,6,17,0,0,35,0,204,3,51,0,201,3,7,17,201,3,8,17,201,3,9,17,201,3,10,17,201,3,11,17,201,3,12,17,201,3,13,17,201,3,14,17,201,3,15,17,201,3,16,17,201,3,17,17,201,3,18,17,201,3,19,17,201,3,20,17,201,3,21,17,201,3,22,17,201,3,23,17,201,3,24,17,201,3,25,17,201,3,26,17,
202,3,27,17,0,0,28,17,7,0,29,17,2,0,30,17,0,0,31,17,0,0,32,17,0,0,33,17,0,0,34,17,0,0,249,3,7,0,35,17,7,0,36,17,0,0,37,17,0,0,38,17,0,0,39,17,0,0,40,17,0,0,41,17,0,0,42,17,0,0,43,17,0,0,44,17,0,0,45,17,0,0,46,17,0,0,47,17,0,0,48,17,0,0,49,17,0,0,50,17,0,0,51,17,0,0,52,17,0,0,53,17,7,0,54,17,7,0,55,17,
0,0,107,0,205,3,2,0,0,0,56,17,0,0,5,17,206,3,238,0,0,0,5,17,0,0,57,17,0,0,58,17,0,0,8,1,0,0,59,17,0,0,244,16,0,0,60,17,0,0,4,17,0,0,61,17,0,0,62,17,0,0,63,17,0,0,64,17,0,0,65,17,0,0,66,17,0,0,67,17,0,0,68,17,0,0,69,17,0,0,70,17,0,0,71,17,0,0,72,17,0,0,73,17,0,0,74,17,0,0,75,17,0,0,76,17,0,0,77,17,
203,3,78,17,205,3,79,17,0,0,80,17,0,0,81,17,0,0,82,17,0,0,83,17,0,0,84,17,0,0,85,17,0,0,86,17,0,0,87,17,0,0,88,17,0,0,89,17,0,0,90,17,0,0,91,17,0,0,92,17,0,0,93,17,0,0,94,17,0,0,95,17,0,0,96,17,0,0,97,17,0,0,98,17,0,0,99,17,0,0,100,17,0,0,101,17,0,0,102,17,0,0,103,17,0,0,104,17,0,0,105,17,0,0,106,17,0,0,107,17,
0,0,108,17,0,0,109,17,0,0,110,17,0,0,111,17,0,0,112,17,0,0,113,17,0,0,114,17,0,0,115,17,0,0,116,17,0,0,117,17,0,0,118,17,0,0,119,17,0,0,120,17,0,0,121,17,0,0,122,17,0,0,123,17,0,0,124,17,0,0,125,17,0,0,126,17,0,0,127,17,0,0,128,17,0,0,129,17,0,0,130,17,0,0,131,17,0,0,132,17,0,0,133,17,0,0,134,17,0,0,135,17,0,0,136,17,0,0,137,17,
0,0,138,17,0,0,139,17,0,0,140,17,0,0,141,17,0,0,142,17,0,0,143,17,0,0,144,17,0,0,145,17,0,0,146,17,0,0,147,17,0,0,148,17,0,0,149,17,0,0,150,17,0,0,151,17,0,0,152,17,0,0,153,17,0,0,154,17,0,0,155,17,0,0,156,17,0,0,157,17,0,0,158,17,0,0,159,17,0,0,160,17,0,0,161,17,0,0,162,17,0,0,163,17,0,0,164,17,0,0,165,17,0,0,166,17,0,0,167,17,
0,0,168,17,0,0,169,17,0,0,162,5,0,0,170,17,0,0,171,17,0,0,172,17,0,0,173,17,0,0,174,17,0,0,175,17,0,0,176,17,0,0,177,17,0,0,81,15,0,0,178,17,0,0,179,17,0,0,180,17,0,0,181,17,0,0,84,12,7,0,182,17,0,0,183,17,0,0,184,17,0,0,185,17,0,0,186,17,0,0,187,17,0,0,188,17,0,0,189,17,0,0,190,17,0,0,191,17,0,0,192,17,0,0,193,17,0,0,194,17,
0,0,195,17,0,0,196,17,0,0,197,17,0,0,198,17,0,0,199,17,0,0,200,17,0,0,201,17,0,0,202,17,0,0,203,17,0,0,204,17,0,0,205,17,0,0,206,17,0,0,207,17,0,0,208,17,0,0,209,17,0,0,210,17,0,0,211,17,0,0,212,17,0,0,213,17,0,0,214,17,0,0,215,17,0,0,216,17,0,0,217,17,0,0,218,17,0,0,219,17,7,0,220,17,0,0,221,17,0,0,222,17,0,0,223,17,0,0,224,17,
0,0,225,17,0,0,226,17,0,0,227,17,0,0,228,17,0,0,229,17,0,0,230,17,0,0,231,17,0,0,232,17,0,0,233,17,0,0,234,17,0,0,235,17,0,0,236,17,0,0,237,17,0,0,238,17,0,0,87,1,0,0,239,17,0,0,240,17,0,0,241,17,0,0,242,17,0,0,243,17,0,0,244,17,0,0,245,17,0,0,246,17,0,0,247,17,0,0,248,17,0,0,249,17,0,0,250,17,0,0,251,17,0,0,252,17,0,0,253,17,
0,0,254,17,0,0,255,17,0,0,0,18,0,0,1,18,0,0,2,18,0,0,3,18,0,0,4,18,0,0,5,18,0,0,6,18,0,0,7,18,0,0,8,18,0,0,9,18,0,0,10,18,0,0,11,18,0,0,12,18,0,0,13,18,0,0,14,18,0,0,15,18,0,0,16,18,0,0,17,18,0,0,18,18,0,0,19,18,0,0,20,18,0,0,21,18,0,0,22,18,0,0,23,18,0,0,24,18,0,0,25,18,0,0,26,18,0,0,27,18,
0,0,28,18,0,0,29,18,0,0,30,18,62,0,5,0,0,0,31,18,0,0,87,17,0,0,92,17,2,0,33,0,0,0,42,0,207,3,1,0,0,0,179,5,208,3,1,0,0,0,179,5,209,3,27,0,209,3,29,0,209,3,30,0,0,0,34,0,0,0,154,2,204,3,32,18,206,3,33,18,206,3,34,18,206,3,35,18,206,3,36,18,206,3,37,18,206,3,38,18,206,3,39,18,206,3,40,18,206,3,41,18,206,3,42,18,206,3,43,18,
206,3,44,18,206,3,45,18,206,3,46,18,206,3,47,18,206,3,48,18,206,3,49,18,206,3,50,18,62,0,51,18,207,3,52,18,208,3,53,18,4,0,54,18,210,3,4,0,210,3,29,0,210,3,30,0,0,0,55,18,24,0,133,0,211,3,5,0,211,3,29,0,211,3,30,0,0,0,56,18,0,0,33,0,0,0,238,0,212,3,6,0,212,3,29,0,212,3,30,0,0,0,57,18,0,0,238,0,0,0,58,18,23,0,59,18,213,3,5,0,
213,3,29,0,213,3,30,0,0,0,60,18,0,0,31,0,0,0,238,0,214,3,6,0,213,3,233,9,0,0,61,18,24,0,133,0,0,0,62,18,0,0,63,18,0,0,238,0,215,3,2,0,213,3,233,9,0,0,64,18,216,3,5,0,213,3,233,9,0,0,65,18,0,0,66,18,4,0,67,18,0,0,35,0,217,3,7,0,217,3,29,0,217,3,30,0,0,0,34,0,0,0,98,10,2,0,68,18,2,0,33,0,0,0,35,0,218,3,10,0,
218,3,29,0,218,3,30,0,0,0,34,0,0,0,69,18,0,0,70,18,0,0,71,18,0,0,72,18,1,0,33,0,1,0,185,2,0,0,242,0,219,3,5,0,4,0,33,0,7,0,73,18,7,0,93,8,7,0,74,18,7,0,248,3,220,3,8,0,7,0,75,18,7,0,76,18,7,0,77,18,7,0,78,18,7,0,79,18,7,0,80,18,2,0,33,0,0,0,242,0,221,3,2,0,0,0,81,18,0,0,238,0,222,3,3,0,0,0,82,18,
0,0,33,0,0,0,242,0,223,3,9,0,4,0,83,18,4,0,168,15,4,0,84,18,4,0,170,15,4,0,33,0,4,0,69,2,10,0,163,15,4,0,85,18,4,0,86,18,224,3,23,0,0,0,87,18,0,0,88,18,0,0,89,18,0,0,90,18,0,0,91,18,0,0,92,18,0,0,93,18,0,0,94,18,0,0,95,18,0,0,96,18,0,0,97,18,0,0,98,18,0,0,99,18,0,0,100,18,0,0,101,18,0,0,102,18,0,0,103,18,
0,0,104,18,0,0,105,18,0,0,106,18,0,0,107,18,0,0,108,18,0,0,51,0,225,3,4,0,225,3,29,0,225,3,30,0,0,0,34,0,0,0,109,18,226,3,4,0,226,3,29,0,226,3,30,0,0,0,110,18,23,0,215,14,227,3,170,0,4,0,87,0,4,0,88,0,4,0,33,0,4,0,111,18,0,0,112,18,0,0,113,18,0,0,114,18,0,0,115,18,0,0,116,18,0,0,117,18,0,0,118,18,0,0,119,18,0,0,120,18,
0,0,121,18,0,0,122,18,0,0,123,18,0,0,124,18,0,0,125,18,0,0,126,18,0,0,127,18,4,0,128,18,2,0,129,18,2,0,130,18,2,0,131,18,2,0,132,18,0,0,8,1,0,0,133,18,4,0,134,18,0,0,135,18,0,0,136,18,0,0,137,18,0,0,138,18,0,0,139,18,2,0,140,18,4,0,141,18,4,0,142,18,4,0,143,18,4,0,144,18,4,0,145,18,7,0,146,18,4,0,147,18,4,0,148,18,7,0,149,18,
7,0,150,18,7,0,151,18,4,0,152,18,4,0,248,15,0,0,153,18,0,0,154,18,2,0,155,18,2,0,156,18,2,0,157,18,0,0,158,18,23,0,159,18,23,0,160,18,23,0,161,18,23,0,162,18,23,0,163,18,23,0,164,18,23,0,165,18,23,0,166,18,23,0,167,18,23,0,168,18,23,0,169,18,23,0,170,18,0,0,171,18,2,0,172,18,2,0,173,18,0,0,174,18,1,0,175,18,1,0,176,18,0,0,177,18,2,0,178,18,
4,0,179,18,7,0,180,18,2,0,181,18,2,0,182,18,2,0,183,18,2,0,184,18,0,0,185,18,219,3,186,18,7,0,187,18,0,0,148,15,0,0,188,18,0,0,189,18,0,0,190,18,2,0,191,18,2,0,192,18,2,0,193,18,2,0,194,18,2,0,195,18,2,0,196,18,4,0,197,18,4,0,198,18,7,0,199,18,0,0,136,5,2,0,200,18,2,0,201,18,2,0,202,18,2,0,203,18,2,0,204,18,2,0,205,18,0,0,206,18,
0,0,207,18,0,0,208,18,0,0,209,18,0,0,210,18,4,0,211,18,7,0,212,18,2,0,213,18,2,0,214,18,2,0,215,18,2,0,216,18,2,0,217,18,2,0,218,18,7,0,219,18,7,0,220,18,7,0,221,18,7,0,222,18,7,0,223,18,4,0,224,18,2,0,225,18,2,0,226,18,7,0,227,18,2,0,197,13,2,0,228,18,2,0,229,18,0,0,230,18,2,0,231,18,0,0,232,18,0,0,233,18,7,0,234,18,7,0,235,18,
134,0,236,18,7,0,237,18,7,0,238,18,0,0,239,18,0,0,240,18,0,0,241,18,0,0,242,18,0,0,243,18,0,0,244,18,4,0,245,18,7,0,246,18,2,0,247,18,2,0,248,18,2,0,249,18,2,0,250,18,2,0,251,18,2,0,252,18,4,0,253,18,0,0,254,18,0,0,13,14,0,0,255,18,0,0,0,19,0,0,1,19,4,0,2,19,4,0,3,19,2,0,4,19,2,0,5,19,7,0,6,19,0,0,7,19,0,0,8,19,
0,0,9,19,0,0,10,19,220,3,11,19,222,3,12,19,223,3,13,19,224,3,14,19,221,3,81,0,112,0,6,0,4,0,15,19,3,0,16,19,3,0,17,19,1,0,18,19,1,0,19,19,1,0,20,19,61,3,2,0,2,0,75,3,2,0,76,3,228,3,2,0,7,0,75,3,7,0,76,3,229,3,2,0,4,0,75,3,4,0,76,3,230,3,3,0,4,0,75,3,4,0,76,3,4,0,178,5,231,3,3,0,7,0,75,3,7,0,76,3,
7,0,178,5,232,3,4,0,7,0,75,3,7,0,76,3,7,0,178,5,7,0,11,4,233,3,1,0,7,0,21,19,79,3,4,0,4,0,215,3,4,0,105,3,4,0,216,3,4,0,106,3,87,0,4,0,7,0,215,3,7,0,105,3,7,0,216,3,7,0,106,3,50,0,4,0,7,0,157,0,7,0,22,19,7,0,23,19,7,0,24,19,214,0,5,0,26,0,89,0,0,0,90,0,234,3,17,1,39,0,91,0,39,0,25,19,71,0,27,0,
87,0,65,16,87,0,88,3,79,3,26,19,79,3,27,19,79,3,28,19,7,0,29,19,7,0,30,19,7,0,31,19,7,0,32,19,2,0,33,19,2,0,34,19,2,0,35,19,2,0,36,19,2,0,37,19,2,0,33,0,2,0,173,14,2,0,181,14,2,0,182,14,2,0,38,19,2,0,39,19,2,0,143,15,0,0,40,19,0,0,41,19,0,0,51,0,7,0,42,19,235,3,43,19,58,3,44,19,236,3,45,0,7,0,45,19,7,0,46,19,
7,0,47,19,7,0,48,19,7,0,49,19,7,0,50,19,7,0,51,19,7,0,52,19,7,0,53,19,7,0,54,19,186,1,55,19,236,3,56,19,237,3,57,19,238,3,43,19,58,3,44,19,7,0,58,19,7,0,59,19,7,0,60,19,7,0,61,19,7,0,62,19,7,0,63,19,7,0,126,1,7,0,64,19,7,0,65,19,7,0,66,19,7,0,46,16,7,0,67,19,0,0,68,19,0,0,69,19,0,0,201,6,0,0,70,19,0,0,71,19,
0,0,72,19,0,0,73,19,0,0,10,4,7,0,74,19,2,0,75,19,2,0,76,19,7,0,77,19,0,0,78,19,0,0,79,19,0,0,80,19,0,0,81,19,7,0,82,19,7,0,83,19,54,3,7,0,7,0,248,5,7,0,84,19,7,0,234,10,7,0,85,19,7,0,86,19,2,0,51,1,0,0,24,0,48,3,32,0,0,0,31,0,0,0,87,19,0,0,88,19,0,0,89,19,2,0,33,0,0,0,90,19,0,0,91,19,0,0,92,19,
0,0,93,19,0,0,94,19,0,0,155,2,0,0,95,19,0,0,96,19,0,0,97,19,7,0,98,19,7,0,99,19,7,0,100,19,7,0,101,19,7,0,102,19,7,0,103,19,7,0,104,19,7,0,105,19,7,0,106,19,7,0,107,19,7,0,108,19,7,0,109,19,7,0,110,19,7,0,111,19,4,0,112,19,0,0,113,19,24,0,133,0,11,0,131,3,239,3,24,0,4,0,33,0,4,0,114,19,7,0,115,19,7,0,116,19,4,0,117,19,
4,0,118,19,7,0,119,19,7,0,120,19,7,0,121,19,7,0,122,19,7,0,123,19,7,0,124,19,7,0,125,19,7,0,126,19,7,0,127,19,7,0,128,19,7,0,129,19,7,0,130,19,7,0,131,19,7,0,132,19,7,0,133,19,7,0,134,19,4,0,135,19,7,0,136,19,240,3,5,0,11,0,137,19,11,0,138,19,4,0,33,0,0,0,127,0,92,1,139,19,241,3,60,0,70,0,29,0,70,0,30,0,23,0,239,0,0,0,240,0,
0,0,241,0,0,0,242,0,7,0,63,19,7,0,126,1,7,0,140,19,0,0,141,19,0,0,150,1,0,0,186,11,0,0,197,6,4,0,142,19,4,0,143,19,2,0,69,19,2,0,201,6,54,0,230,3,54,0,144,19,87,0,145,19,241,3,56,19,0,0,146,19,2,0,147,19,0,0,59,5,4,0,93,14,2,0,148,19,2,0,246,13,2,0,149,19,2,0,150,19,2,0,151,19,2,0,152,19,2,0,33,0,4,0,241,1,7,0,202,2,
7,0,32,6,7,0,153,19,7,0,154,19,7,0,46,16,0,0,10,4,0,0,148,15,0,0,155,19,0,0,156,19,0,0,157,19,0,0,158,19,0,0,159,19,0,0,160,19,2,0,161,19,2,0,162,19,7,0,163,19,18,1,204,9,2,0,164,19,0,0,165,19,0,0,132,7,7,0,166,19,7,0,167,19,7,0,168,19,48,3,15,14,239,3,209,15,181,3,11,16,240,3,81,0,242,3,5,0,242,3,29,0,242,3,30,0,4,0,31,0,
0,0,4,0,0,0,169,19,243,3,2,0,242,3,9,0,26,0,48,1,244,3,2,0,242,3,9,0,0,0,170,19,245,3,3,0,242,3,9,0,4,0,25,10,0,0,127,0,246,3,3,0,242,3,9,0,4,0,171,19,0,0,127,0,247,3,3,0,242,3,9,0,4,0,172,19,4,0,173,19,248,3,3,0,242,3,9,0,4,0,25,10,0,0,127,0,181,3,1,0,23,0,27,10,249,3,8,0,7,0,58,2,4,0,174,19,4,0,175,19,
4,0,176,19,4,0,116,5,4,0,117,5,7,0,108,5,4,0,10,4,250,3,4,0,4,0,21,0,4,0,145,3,7,0,47,1,7,0,132,5,251,3,23,0,26,0,89,0,101,0,112,1,0,0,154,2,39,0,91,0,0,0,158,2,0,0,177,19,0,0,87,1,4,0,229,0,4,0,178,19,4,0,9,1,4,0,33,0,4,0,179,19,129,0,18,4,2,0,26,4,2,0,168,2,250,3,180,19,249,3,121,14,0,0,181,19,0,0,142,12,
0,0,169,2,7,0,130,5,11,0,73,4,252,3,57,0,253,3,7,0,23,0,182,19,4,0,183,19,4,0,184,19,4,0,33,0,0,0,4,0,58,3,185,19,254,3,186,19,255,3,2,0,0,4,57,0,1,4,187,19,2,4,30,0,26,0,89,0,3,4,188,19,3,4,189,19,23,0,190,19,1,0,191,19,0,0,13,13,2,0,192,19,2,0,193,19,2,0,194,19,23,0,195,19,23,0,196,19,144,0,197,19,11,0,145,1,4,0,198,19,
4,0,199,19,23,0,200,19,23,0,201,19,23,0,202,19,23,0,203,19,4,4,204,19,4,4,205,19,4,4,206,19,23,0,207,19,58,3,208,19,0,0,209,19,0,0,135,1,5,4,210,19,6,4,211,19,255,3,212,19,7,4,57,0,3,4,46,0,3,4,29,0,3,4,30,0,11,0,213,19,11,0,214,19,3,4,85,0,241,0,194,6,241,0,215,19,0,0,216,19,241,0,217,19,8,4,218,19,63,3,219,19,240,0,220,19,4,0,128,14,
2,0,221,19,2,0,222,19,2,0,65,10,2,0,66,10,0,0,223,19,0,0,95,4,2,0,92,14,2,0,224,19,2,0,225,19,2,0,226,19,2,0,227,19,2,0,228,19,0,0,229,19,0,0,230,19,0,0,231,19,0,0,232,19,0,0,233,19,0,0,234,19,4,0,235,19,9,4,236,19,10,4,237,19,10,4,238,19,11,4,239,19,0,0,240,19,0,0,236,12,23,0,241,19,23,0,187,14,23,0,242,19,23,0,243,19,78,1,244,6,
23,0,244,19,11,0,245,19,10,0,246,19,12,4,19,0,12,4,29,0,12,4,30,0,0,0,97,1,24,0,76,0,0,0,247,19,2,0,248,19,2,0,31,0,12,0,27,0,12,0,183,8,2,0,249,19,2,0,250,19,2,0,251,19,2,0,252,19,2,0,253,19,2,0,33,0,2,0,254,19,2,0,89,0,0,0,51,0,13,4,255,19,14,4,4,0,14,4,29,0,14,4,30,0,12,4,0,20,12,4,1,20,15,4,13,0,15,4,29,0,
15,4,30,0,23,0,59,18,23,0,2,20,0,0,97,1,2,0,3,20,2,0,4,20,0,0,5,20,2,0,33,0,2,0,6,20,16,4,7,20,16,4,8,20,11,0,9,20,17,4,4,0,17,4,29,0,17,4,30,0,0,0,97,1,24,0,133,0,4,4,8,0,4,4,29,0,4,4,30,0,0,0,97,1,0,0,10,20,23,0,11,20,4,0,12,20,2,0,33,0,0,0,42,0,159,3,14,0,159,3,29,0,159,3,30,0,0,0,97,1,
24,0,76,0,18,4,146,14,11,0,13,20,11,0,79,0,13,4,255,19,253,3,14,20,23,0,15,20,159,3,16,20,67,3,147,14,2,0,33,0,0,0,24,0,77,3,9,0,77,3,29,0,77,3,30,0,0,0,97,1,0,0,17,20,2,0,41,0,2,0,57,18,4,0,247,0,24,0,76,0,19,4,57,0,20,4,4,0,20,4,29,0,20,4,30,0,240,0,220,19,0,0,34,0,21,4,3,0,21,4,29,0,21,4,30,0,0,0,34,0,
22,4,13,0,26,0,89,0,23,0,18,20,23,0,19,20,23,0,20,20,23,0,21,20,241,0,22,20,0,0,4,0,4,0,23,20,4,0,161,1,4,0,24,20,23,4,57,0,113,0,174,15,181,3,11,16,24,4,6,0,24,4,29,0,24,4,30,0,11,0,85,0,11,0,45,10,4,0,25,20,0,0,26,20,8,4,4,0,22,4,27,20,20,4,28,20,22,4,29,20,20,4,30,20,93,1,39,0,26,0,89,0,101,0,112,1,13,0,219,6,
0,0,35,0,2,0,222,7,2,0,31,20,7,0,32,20,7,0,33,20,7,0,34,20,7,0,128,3,7,0,35,20,7,0,210,1,2,0,247,0,0,0,68,4,7,0,36,20,7,0,37,20,7,0,38,20,7,0,39,20,7,0,40,20,7,0,41,20,2,0,33,0,0,0,192,6,4,0,42,20,7,0,43,20,7,0,69,7,7,0,44,20,7,0,45,20,7,0,46,20,0,0,214,5,73,0,253,0,2,0,73,7,2,0,74,7,0,0,4,0,
40,0,231,0,95,1,91,7,91,1,204,11,11,0,145,1,23,0,54,8,10,0,47,20,1,4,15,0,48,3,15,14,7,0,48,20,0,0,20,0,0,0,49,20,54,0,50,20,7,0,51,20,7,0,52,20,0,0,53,20,0,0,54,20,0,0,239,1,7,0,55,20,7,0,56,20,4,0,33,0,4,0,142,19,4,0,143,19,25,4,3,0,25,4,29,0,25,4,30,0,0,0,57,20,26,4,10,0,26,4,29,0,26,4,30,0,0,0,34,0,
0,0,58,20,23,0,59,20,7,0,60,20,2,0,61,20,0,0,51,0,7,0,62,20,7,0,63,20,27,4,3,0,27,4,29,0,27,4,30,0,0,0,64,20,28,4,20,0,28,4,29,0,28,4,30,0,0,0,34,0,0,0,31,0,0,0,30,2,23,0,65,20,0,0,66,20,24,0,67,20,13,4,68,20,2,0,69,20,2,0,70,20,2,0,71,20,2,0,72,20,0,0,73,20,7,0,74,20,7,0,75,20,7,0,76,20,2,0,77,20,
0,0,192,6,23,0,78,20,29,4,6,0,29,4,29,0,29,4,30,0,0,0,34,0,23,0,59,18,2,0,79,20,0,0,24,0};

int bfBlenderLen=sizeof(bfBlenderFBT);
//===============================================================================================

#endif //bftBlend_imp_
#endif //FBTBLEND_IMPLEMENTATION

