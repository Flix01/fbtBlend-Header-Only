// https://github.com/Flix01/fbtBlend-Header-Only
/*
Basically this started as a custom single-file header-only replacement
for a SUBSET of the glm library (https://github.com/g-truc/glm),
to make one of my demos work:
-> without (a specific version of) glm (glm is much faster, but is made of many files and does not support old compilers anymore).
-> with some functions that were not included in glm (so that I should have added another file to glm with the missing methods).
Basically I prefer using less portable files as possible, rather then having faster stuff (but I'm sure some people will try the opposite route and
use legacy glm).

However USE IT AT YOUR OWN RISK. That the implementation of some methods might be wrong/incorrect/buggy!

MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

Some functions are ported from
-> the OgreMath library (www.ogre3d.org) - MIT licensed.
-> the repository https://github.com/juj/MathGeoLib - Apache 2 license
-> the glm library (https://github.com/g-truc/glm) - licensed under the Happy Bunny License (Modified MIT) or the MIT License.
*/

#ifndef _MATH_HELPER_HPP_
#define _MATH_HELPER_HPP_

#include <math.h>
#include <stdio.h>  //FILE* and printf
#include <string.h> //memcpy

#ifdef MATH_HELPER_ASSERT_ON_BAD_QUATERNION_CTR // w is the first argument, so it will assert on quat(0,0,0,1) which is a common error
#include <assert.h>
#endif //MATH_HELPER_ASSERT_ON_BAD_QUATERNION_CTR

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_HALF_PI
#define M_HALF_PI (M_PI/2.0)
#endif
#ifndef M_PIOVER180
#define M_PIOVER180 (3.14159265358979323846/180.0)
#endif
#ifndef M_180OVERPI
#define M_180OVERPI (180.0/3.14159265358979323846)
#endif

#ifndef MATH_HELPER_NAMESPACE_NAME
#   ifndef MATH_HELPER_DISABLE_GLM_ALIASING
#       define MATH_HELPER_NAMESPACE_NAME glm
#       ifndef GLM_INLINE
#           define GLM_INLINE inline
#       endif //GLM_INLINE
#   else //MATH_HELPER_DISABLE_GLM_ALIASING
#       define MATH_HELPER_NAMESPACE_NAME mhe
#   endif //MATH_HELPER_DISABLE_GLM_ALIASING
#endif //MATH_HELPER_NAMESPACE_NAME

//#define MATH_HELPER_DISABLE_GLM_MATRIX_SUBSCRIPT_OPERATOR   // Better IMO, but then code is less compatible with glm


namespace MATH_HELPER_NAMESPACE_NAME	{

// Warning: enum starts at 1 to match an enum order used by Blender
 enum EulerRotationMode {
    EULER_XYZ = 1,  // 1
    EULER_XZY = 2,  // 2
    EULER_YXZ = 3,  // 3
    EULER_YZX = 4,  // 4
    EULER_ZXY = 5,  // 5
    EULER_ZYX = 6  // 6
    //AXIS_ANGLE = -1       // (these are used by Blender too)
    //QUATERNION_WXYZ = 0
};

#pragma pack(push,1)
template <typename T> inline T radians(T deg) {return deg*M_PIOVER180;}
template <typename T> inline T degrees(T rad) {return rad*M_180OVERPI;}
inline bool isEqual(float a,float b,float eps = static_cast <float>(0.0000001)) {return fabs(a-b)<eps;}
inline bool isEqual(double a,float b,double eps = static_cast <double>(0.0000001)) {return fabs(a-b)<eps;}
template <typename T> inline static void SinCos(T x,T& sinx,T& cosx) {sinx=sin(x);cosx=cos(x);}

template <typename T> class Vec2Base {	
	public:
	union {	
		struct {T x,y;};
		struct {T r,g;};
	};
	inline Vec2Base(T a,T b) : x(a),y(b) {}
	inline Vec2Base() : x(0),y(0) {}
	inline Vec2Base(const Vec2Base<T>& o) : x(o.x),y(o.y) {}	

	inline const Vec2Base<T>& operator=(const Vec2Base<T>& o) {x=o.x;y=o.y;return *this;}
	inline const Vec2Base<T>& operator=(T s) {x=s;y=s;return *this;}		
	inline T operator[](int i) const {return *(((T*)&x)+i);}
	inline T& operator[](int i) {return *(((T*)&x)+i);}	

	inline Vec2Base<T> operator+(const Vec2Base<T>& o) const	{return Vec2Base<T>(x+o.x,y+o.y);}
	inline void operator+=(const Vec2Base<T>& o) {x+=o.x;y+=o.y;}
	inline Vec2Base<T> operator+(T s) const	{return Vec2Base<T>(x+s,y+s);}
	inline void operator+=(T s) {x+=s;y+=s;}

	inline Vec2Base<T> operator-(const Vec2Base<T>& o) const	{return Vec2Base<T>(x-o.x,y-o.y);}
	inline void operator-=(const Vec2Base<T>& o) {x-=o.x;y-=o.y;}
	inline Vec2Base<T> operator-(T s) const	{return Vec2Base<T>(x-s,y-s);}	
	inline void operator-=(T s) {x-=s;y-=s;}
    inline Vec2Base<T> operator-() const	{return Vec2Base<T>(-x,-y);}
	
	inline Vec2Base<T> operator*(const Vec2Base<T>& o) const	{return Vec2Base<T>(x*o.x,y*o.y);}
	inline void operator*=(const Vec2Base<T>& o) {x*=o.x;y*=o.y;}
	inline Vec2Base<T> operator*(T s) const	{return Vec2Base<T>(x*s,y*s);}
	inline void operator*=(T s) {x*=s;y*=s;}
	
	inline Vec2Base<T> operator/(const Vec2Base<T>& o) const	{return Vec2Base<T>(x/o.x,y/o.y);}
	inline void operator/=(const Vec2Base<T>& o) {x/=o.x;y/=o.y;}
	inline Vec2Base<T> operator/(T s) const	{return Vec2Base<T>(x/s,y/s);}
	inline void operator/=(T s) {x/=s;y/=s;}
	
	inline T dot(const Vec2Base<T>& o) const {return x*o.x+y*o.y;}
	
	inline T  length2()  const  {return x*x+y*y;}
	inline T  length()  const  {return sqrt(x*x+y*y);}
	inline void normalize() {const T len=length();if (len>0) {x/=len;y/=len;} else {x=y=T(0);}}		
	inline Vec2Base<T> normalized() const	{const T len=length();return (len>0) ? Vec2Base<T>(x/len,y/len) : Vec2Base<T>();}
		
	inline Vec2Base<T> lerp(const Vec2Base<T>& o,T t) const   {float ct = T(1)-t;return Vec2Base<T>(x*ct+o.x*t,y*ct+o.y*t);}

	inline Vec2Base<T> proj(const Vec2Base<T>& onto) const {return onto*this->dot(onto)/onto.dot(onto);}
	inline T angle_between(const Vec2Base<T>& o) const {return acos(this->dot(o)/(this->length()*o.length()));}
 
    inline void   fprintp(FILE* stream,int width, int precision) const {int w = width, p = precision;fprintf(stream, "( %*.*f %*.*f )\n",w, p, x, w, p, y);}
	inline void   fprint(FILE* stream) const {fprintp(stream, 6, 2);}
	inline void   printp(int width, int precision) const {fprintp(stdout, width, precision);}
	inline void   print() {fprintp(stdout, 6, 2);}

};
template <typename T> inline T* value_ptr(Vec2Base<T>& o) {return &o.x;}
template <typename T> inline const T* value_ptr(const Vec2Base<T>& o) {return &o.x;}
template <typename T> inline bool isEqual(const Vec2Base<T>& a,const Vec2Base<T>& b,T eps=T(0.0000001)) {return isEqual(a.x,b.x,eps) && isEqual(a.y,b.y,eps);}
typedef Vec2Base<float> Vec2f;
typedef Vec2Base<double> Vec2d;

template <typename T> class Vec3Base {	
	public:
	union {	
		struct {T x,y,z;};
		struct {T r,g,b;};
	};
	inline Vec3Base(T a,T b, T c) : x(a),y(b),z(c) {}
	inline Vec3Base() : x(0),y(0),z(0) {}
    inline Vec3Base(T v) : x(v),y(v),z(v) {}
    inline Vec3Base(const Vec3Base<T>& o) : x(o.x),y(o.y),z(o.z) {}
	
	inline const Vec3Base<T>& operator=(const Vec3Base<T>& o) {x=o.x;y=o.y;z=o.z;return *this;}
	inline const Vec3Base<T>& operator=(T s) {x=s;y=s;z=s;return *this;}		
	inline T operator[](int i) const {return *(((T*)&x)+i);}
	inline T& operator[](int i) {return *(((T*)&x)+i);}	
	
    //inline operator Vec2Base<T>() const {return Vec2Base<T>(x,y);}
	//inline Vec2Base<T>* castToVec2Ptr() {return (Vec2Base<T>*) this;}
	//inline const Vec2Base<T>* castToVec2Ptr() const {return (const Vec2Base<T>*) this;}

	inline Vec3Base<T> operator+(const Vec3Base<T>& o) const	{return Vec3Base<T>(x+o.x,y+o.y,z+o.z);}
	inline void operator+=(const Vec3Base<T>& o) {x+=o.x;y+=o.y;z+=o.z;}
	inline Vec3Base<T> operator+(T s) const	{return Vec3Base<T>(x+s,y+s,z+s);}
	inline void operator+=(T s) {x+=s;y+=s;z+=s;}

	inline Vec3Base<T> operator-(const Vec3Base<T>& o) const	{return Vec3Base<T>(x-o.x,y-o.y,z-o.z);}
	inline void operator-=(const Vec3Base<T>& o) {x-=o.x;y-=o.y;z-=o.z;}
	inline Vec3Base<T> operator-(T s) const	{return Vec3Base<T>(x-s,y-s,z-s);}	
	inline void operator-=(T s) {x-=s;y-=s;z-=s;}
    inline Vec3Base<T> operator-() const	{return Vec3Base<T>(-x,-y,-z);}

	inline Vec3Base<T> operator*(const Vec3Base<T>& o) const	{return Vec3Base<T>(x*o.x,y*o.y,z*o.z);}
	inline void operator*=(const Vec3Base<T>& o) {x*=o.x;y*=o.y;z*=o.z;}
	inline Vec3Base<T> operator*(T s) const	{return Vec3Base<T>(x*s,y*s,z*s);}
	inline void operator*=(T s) {x*=s;y*=s;z*=s;}
	
	inline Vec3Base<T> operator/(const Vec3Base<T>& o) const	{return Vec3Base<T>(x/o.x,y/o.y,z/o.z);}
	inline void operator/=(const Vec3Base<T>& o) {x/=o.x;y/=o.y;z/=o.z;}
	inline Vec3Base<T> operator/(T s) const	{return Vec3Base<T>(x/s,y/s,z/s);}
	inline void operator/=(T s) {x/=s;y/=s;z/=s;}
	
	inline T dot(const Vec3Base<T>& o) const {return x*o.x+y*o.y+z*o.z;}
    inline Vec3Base<T> cross(const Vec3Base<T>& o) const	{return Vec3Base<T>(y*o.z-z*o.y,z*o.x-x*o.z,x*o.y-y*o.x);}
	
	inline T  length2()  const  {return x*x+y*y+z*z;}
	inline T  length()  const  {return sqrt(x*x+y*y+z*z);}
    inline void normalize() {const T len=length();if (len>T(0.0000001)) {x/=len;y/=len;z/=len;} else {x=z=T(0);y=T(1);}}
    inline Vec3Base<T> normalized() const	{const T len=length();return (len>T(0.0000001)) ? Vec3Base<T>(x/len,y/len,z/len) : Vec3Base<T>(0,1,0);}
		
	inline Vec3Base<T> lerp(const Vec3Base<T>& o,T t) const   {float ct = T(1)-t;return Vec3Base<T>(x*ct+o.x*t,y*ct+o.y*t,z*ct+o.z*t);}

	inline Vec3Base<T> proj(const Vec3Base<T>& onto) const {return onto*this->dot(onto)/onto.dot(onto);}
	inline T angle_between(const Vec3Base<T>& o) const {return acos(this->dot(o)/(this->length()*o.length()));}
 
    inline void   fprintp(FILE* stream,int width, int precision) const {int w = width, p = precision;fprintf(stream, "( %*.*f %*.*f %*.*f )\n",w, p, x, w, p, y, w, p, z);}
	inline void   fprint(FILE* stream) const {fprintp(stream, 6, 2);}
	inline void   printp(int width, int precision) const {fprintp(stdout, width, precision);}
	inline void   print() {fprintp(stdout, 6, 2);}

};
template <typename T> inline T* value_ptr(Vec3Base<T>& o) {return &o.x;}
template <typename T> inline const T* value_ptr(const Vec3Base<T>& o) {return &o.x;}
template <typename T> inline Vec3Base<T> normalize(const Vec3Base<T>& o) {return o.normalized();}
template <typename T> inline Vec3Base<T> cross(const Vec3Base<T>& a,const Vec3Base<T>& b) {return a.cross(b);}
template <typename T> inline bool isEqual(const Vec3Base<T>& a,const Vec3Base<T>& b,T eps=T(0.0000001)) {return isEqual(a.x,b.x,eps) && isEqual(a.y,b.y,eps) && isEqual(a.z,b.z,eps);}

typedef Vec3Base<float> Vec3f;
typedef Vec3Base<double> Vec3d;

template <typename T> class Vec4Base {
	public:
	union {	
		struct {T x,y,z,w;};
		struct {T r,g,b,a;};
	};
	inline Vec4Base(T a,T b, T c, T d) : x(a),y(b),z(c),w(d) {}
	inline Vec4Base() : x(0),y(0),z(0),w(0) {}
	inline Vec4Base(const Vec4Base<T>& o) : x(o.x),y(o.y),z(o.z),w(o.w) {}	
    inline Vec4Base(const Vec3Base<T>& o,T _w) : x(o.x),y(o.y),z(o.z),w(_w) {}

	inline const Vec4Base<T>& operator=(const Vec4Base<T>& o) {x=o.x;y=o.y;z=o.z;w=o.w;return *this;}
	inline const Vec4Base<T>& operator=(T s) {x=y=z=w=s;return *this;}		
	inline T operator[](int i) const {return *(((T*)&x)+i);}
	inline T& operator[](int i) {return *(((T*)&x)+i);}	

    inline operator Vec3Base<T>() const {return Vec3Base<T>(x,y,z);}
	//inline Vec3Base<T>* castToVec3Ptr() {return (Vec3Base<T>*) this;}
	//inline const Vec3Base<T>* castToVec3Ptr() const {return (const Vec3Base<T>*) this;}
	
    //inline operator Vec2Base<T>() const {return Vec2Base<T>(x,y);}
	//inline Vec2Base<T>* castToVec2Ptr() {return (Vec2Base<T>*) this;}
	//inline const Vec2Base<T>* castToVec2Ptr() const {return (const Vec2Base<T>*) this;}

	inline Vec4Base<T> operator+(const Vec4Base<T>& o) const	{return Vec4Base<T>(x+o.x,y+o.y,z+o.z,w+o.w);}
	inline void operator+=(const Vec4Base<T>& o) {x+=o.x;y+=o.y;z+=o.z;w+=o.w;}
	inline Vec4Base<T> operator+(T s) const	{return Vec4Base<T>(x+s,y+s,z+s,w+s);}
	inline void operator+=(T s) {x+=s;y+=s;z+=s;w+=s;}

	inline Vec4Base<T> operator-(const Vec4Base<T>& o) const	{return Vec4Base<T>(x-o.x,y-o.y,z-o.z,w-o.w);}
	inline void operator-=(const Vec4Base<T>& o) {x-=o.x;y-=o.y;z-=o.z;w-=o.w;}
	inline Vec4Base<T> operator-(T s) const	{return Vec4Base<T>(x-s,y-s,z-s,w-s);}	
	inline void operator-=(T s) {x-=s;y-=s;z-=s;w-=s;}
    inline Vec4Base<T> operator-() const	{return Vec4Base<T>(-x,-y,-z,-w);}
	
	inline Vec4Base<T> operator*(const Vec4Base<T>& o) const	{return Vec4Base<T>(x*o.x,y*o.y,z*o.z,w*o.w);}
	inline void operator*=(const Vec4Base<T>& o) {x*=o.x;y*=o.y;z*=o.z;w*=o.w;}
	inline Vec4Base<T> operator*(T s) const	{return Vec4Base<T>(x*s,y*s,z*s,w*s);}
	inline void operator*=(T s) {x*=s;y*=s;z*=s;w*=s;}
	
	inline Vec4Base<T> operator/(const Vec4Base<T>& o) const	{return Vec4Base<T>(x/o.x,y/o.y,z/o.z,w/o.w);}
	inline void operator/=(const Vec4Base<T>& o) {x/=o.x;y/=o.y;z/=o.z;w/=o.w;}
	inline Vec4Base<T> operator/(T s) const	{return Vec4Base<T>(x/s,y/s,z/s,w/s);}
	inline void operator/=(T s) {x/=s;y/=s;z/=s;w/=s;}
	
	inline T dot(const Vec4Base<T>& o) const {return x*o.x+y*o.y+z*o.z+w*o.w;}

	inline T  length2()  const  {return x*x+y*y+z*z+w*w;}
	inline T  length()  const  {return sqrt(x*x+y*y+z*z+w*w);}
	inline void normalize() {const T len=length();if (len>0) {x/=len;y/=len;z/=len;w/=len;} else {x=y=z=w=T(0);}}		
	inline Vec4Base<T> normalized() const	{const T len=length();return (len>0) ? Vec4Base<T>(x/len,y/len,z/len,w/=len) : Vec4Base<T>();}
		
	inline Vec4Base<T> lerp(const Vec4Base<T>& o,T t) const   {float ct = T(1)-t;return Vec4Base<T>(x*ct+o.x*t,y*ct+o.y*t,z*ct+o.z*t,w*ct+o.w*t);}

	inline Vec4Base<T> proj(const Vec4Base<T>& onto) const {return onto*this->dot(onto)/onto.dot(onto);}
 
    inline void   fprintp(FILE* stream,int width, int precision) const {int d = width, p = precision;fprintf(stream, "( %*.*f %*.*f %*.*f %*.*f )\n",d, p, x, d, p, y, d, p, z, d, p, w);}
	inline void   fprint(FILE* stream) const {fprintp(stream, 6, 2);}
	inline void   printp(int width, int precision) const {fprintp(stdout, width, precision);}
	inline void   print() {fprintp(stdout, 6, 2);}

};
template <typename T> inline T* value_ptr(Vec4Base<T>& o) {return &o.x;}
template <typename T> inline const T* value_ptr(const Vec4Base<T>& o) {return &o.x;}
template <typename T> inline bool isEqual(const Vec4Base<T>& a,const Vec4Base<T>& b,T eps=T(0.0000001)) {return isEqual(a.x,b.x,eps) && isEqual(a.y,b.y,eps) && isEqual(a.z,b.z,eps) && isEqual(a.w,b.w,eps);}
typedef Vec4Base<float> Vec4f;
typedef Vec4Base<double> Vec4d;

template <typename T> class QuatBase {
	public:
	T x,y,z,w;

    inline QuatBase(T _w,T _x,T _y, T _z) : x(_x),y(_y),z(_z),w(_w) {
#       ifdef MATH_HELPER_ASSERT_ON_BAD_QUATERNION_CTR
        assert(!(z==1 && x==0 && y==0 && z==0));    // w is the first component, not the last
#       endif //MATH_HELPER_ASSERT_ON_BAD_QUATERNION_CTR
    }
    inline QuatBase() : x(0),y(0),z(0),w(1) {}
    inline QuatBase(const QuatBase<T>& o) : x(o.x),y(o.y),z(o.z),w(o.w) {}
    inline QuatBase(T rfAngle,const Vec3Base<T>& rkAxis)	{fromAngleAxis(rfAngle,rkAxis);}

	inline const QuatBase<T>& operator=(const QuatBase<T>& o) {x=o.x;y=o.y;z=o.z;w=o.w;return *this;}
    inline const QuatBase<T>& operator=(T s) {x=y=z=w=s;return *this;}
	inline T operator[](int i) const {return *(((T*)&x)+i);}
	inline T& operator[](int i) {return *(((T*)&x)+i);}	

    inline QuatBase<T> operator+(const QuatBase<T>& o) const	{return QuatBase<T>(w+o.w,x+o.x,y+o.y,z+o.z);}
    inline void operator+=(const QuatBase<T>& o) {x+=o.x;y+=o.y;z+=o.z;w+=o.w;}
    inline QuatBase<T> operator+(T s) const	{return QuatBase<T>(w+s,x+s,y+s,z+s);}
	inline void operator+=(T s) {x+=s;y+=s;z+=s;w+=s;}

    inline QuatBase<T> operator-(const QuatBase<T>& o) const	{return QuatBase<T>(w-o.w,x-o.x,y-o.y,z-o.z);}
	inline void operator-=(const QuatBase<T>& o) {x-=o.x;y-=o.y;z-=o.z;w-=o.w;}
    inline QuatBase<T> operator-(T s) const	{return QuatBase<T>(w-s,x-s,y-s,z-s);}
	inline void operator-=(T s) {x-=s;y-=s;z-=s;w-=s;}
    inline QuatBase<T> operator-() const    {return QuatBase<T>(-w,-x,-y,-z);}

	inline QuatBase<T> operator*(const QuatBase<T>& o) const	{
        //return QuatBase<T>(w*o.w,x*o.x,y*o.y,z*o.z);
        return QuatBase<T>(
                    w * o.w - x * o.x - y * o.y - z * o.z,
                    w * o.x + x * o.w + y * o.z - z * o.y,
                    w * o.y + y * o.w + z * o.x - x * o.z,
                    w * o.z + z * o.w + x * o.y - y * o.x
                    );
	}
	inline void operator*=(const QuatBase<T>& o) {
		//x*=o.x;y*=o.y;z*=o.z;w*=o.w;
		*this = *this*o;
	}
    inline QuatBase<T> operator*(T s) const	{return QuatBase<T>(w*s,x*s,y*s,z*s);}
	inline void operator*=(T s) {x*=s;y*=s;z*=s;w*=s;}

    inline QuatBase<T> operator/(const QuatBase<T>& o) const	{return QuatBase<T>(w/o.w,x/o.x,y/o.y,z/o.z);}
    inline void operator/=(const QuatBase<T>& o) {x/=o.x;y/=o.y;z/=o.z;w/=o.w;}
    inline QuatBase<T> operator/(T s) const	{return QuatBase<T>(w/s,x/s,y/s,z/s);}
    inline void operator/=(T s) {x/=s;y/=s;z/=s;w/=s;}
	

	inline T dot(const QuatBase<T>& o) const {return x*o.x+y*o.y+z*o.z+w*o.w;}
	inline QuatBase<T> cross(const QuatBase<T>& o) const {
        return QuatBase<T>(
            w * o.w - x * o.x - y * o.y - z * o.z,
			w * o.x + x * o.w + y * o.z - z * o.y,
			w * o.y + y * o.w + z * o.x - x * o.z,
            w * o.z + z * o.w + x * o.y - y * o.x
            );
	}

	inline T  length2()  const  {return x*x+y*y+z*z+w*w;}
	inline T  length()  const  {return sqrt(x*x+y*y+z*z+w*w);}
    inline void normalize() {const T len=length();if (len>T(0.000001)) {x/=len;y/=len;z/=len;w/=len;} else {x=y=z=T(0);w=T(1);}}
    inline QuatBase<T> normalized() const	{const T len=length();return (len>T(0.000001)) ? QuatBase<T>(w/len,x/len,y/len,z/len) : QuatBase<T>();}
		
    inline QuatBase<T> lerp(const QuatBase<T>& o,T t,bool normalizeQOutAfterLerp=true) const   {float ct = T(1)-t;return normalizeQOutAfterLerp ? QuatBase<T>(w*ct+o.w*t,x*ct+o.x*t,y*ct+o.y*t,z*ct+o.z*t).normalized() : QuatBase<T>(w*ct+o.w*t,x*ct+o.x*t,y*ct+o.y*t,z*ct+o.z*t);}
    QuatBase<T> slerp(const QuatBase<T>& o,T slerpTime_In_0_1,bool normalizeQOutAfterLerp=true,T eps=T(0.0001)) const;
	
    inline QuatBase<T> conjugated() const {return QuatBase<T>(w, -x, -y, -z);}
	inline void conjugate() const {x=-x;y=-y;z=-z;}
    inline QuatBase<T> inverted() const	{return conjugated()/dot(*this);}
    inline void inverse() const {conjugate();(*this)/=dot(*this);}

    inline Vec3Base<T> operator*(const Vec3Base<T>& v) const    {
        const Vec3Base<T> QuatVector(x,y,z);
        const Vec3Base<T> uv(QuatVector.cross(v));
        const Vec3Base<T> uuv(QuatVector.cross(uv));

        return v + ((uv * w) + uuv) * static_cast<T>(2);
    }
    inline Vec3Base<T> mulInv(const Vec3Base<T>& v) const    {return inverted()*v;}

	void fromAngleAxis(T rfAngle,const Vec3Base<T>& rkAxis);
	void toAngleAxis(T& rfAngle,Vec3Base<T>& rkAxis) const;

    static QuatBase<T> LookAtYX(const Vec3Base<T>& sourcePosition,const Vec3Base<T>& targetPosition);
    static void FromEuler(QuatBase<T>& qOut,T fAngleX,T fAngleY,T fAngleZ,EulerRotationMode eulerRotationMode);

    inline void   fprintp(FILE* stream,int width, int precision) const {int d = width, p = precision;fprintf(stream, "( %*.*f %*.*f %*.*f %*.*f )\n",d, p, w, d, p, x, d, p, y, d, p, z);}
	inline void   fprint(FILE* stream) const {fprintp(stream, 6, 2);}
	inline void   printp(int width, int precision) const {fprintp(stdout, width, precision);}
	inline void   print() {fprintp(stdout, 6, 2);}

};
template <typename T> inline T* value_ptr(QuatBase<T>& o) {return &o.x;}
template <typename T> inline const T* value_ptr(const QuatBase<T>& o) {return &o.x;}
template <typename T> inline void quatSlerp(QuatBase<T>& output,T slerpTime_In_0_1,const QuatBase<T>& qStart,const QuatBase<T>& qEnd,bool normalizeQOutAfterLerp=true,T eps=T(0.0001)) {output = qStart.slerp(qEnd,slerpTime_In_0_1,normalizeQOutAfterLerp,eps);}
template <typename T> inline void quatLerp(QuatBase<T>& output,T slerpTime_In_0_1,const QuatBase<T>& qStart,const QuatBase<T>& qEnd,bool normalizeQOutAfterLerp=true) {output = qStart.lerp(qEnd,slerpTime_In_0_1,normalizeQOutAfterLerp);}
template <typename T> inline void fromAngleAxis(QuatBase<T>& output,T radAngle,const Vec3Base<T>& axis) {return output.fromAngleAxis(radAngle,axis);}
template <typename T> inline bool isEqual(const QuatBase<T>& a,const QuatBase<T>& b,T eps=T(0.0000001)) {return isEqual(a.x,b.x,eps) && isEqual(a.y,b.y,eps) && isEqual(a.z,b.z,eps) && isEqual(a.w,b.w,eps);}
template <typename T> inline QuatBase<T> conjugate(const QuatBase<T>& q) {return q.conjugated();}
template <typename T> inline QuatBase<T> inverse(const QuatBase<T>& q) {return q.inverted();}
template <typename T> inline QuatBase<T> normalize(const QuatBase<T>& q) {return q.normalized();}
typedef QuatBase<float> Quatf;
typedef QuatBase<double> Quatd;


template <typename T> class Mat3Base {
	public:
	union {	
        struct {T m[9];};		// column-major order: e.g. m[3],m[4],m[5] are the Y axis rotation part of the matrix
        struct {T col[3][3];};	// e.g. col[1] is: col[1][0]=m[3]  col[1][1]=m[4]  col[1][2]=m[5]

        // Mat3Base<T>::print() displays:
        /*
                 xAxis            yAxis            zAxis
                   |                |                |
                   v                v                v
         |  m[0]==col[0][0]  m[3]==col[1][0]  m[6]==col[2][0]  |
         |  m[1]==col[0][1]  m[4]==col[1][1]  m[7]==col[2][1]  |
         |  m[2]==col[0][2]  m[5]==col[1][2]  m[8]==col[2][2]  |

        */
    };

	inline explicit Mat3Base(const T* m9) {memcpy(m,m9,9*sizeof(T));}
	inline Mat3Base(T m00,T m10, T m20, T m01, T m11, T m21, T m02, T m12, T m22) {
		T* p = m;		
		*p++ = m00; *p++ = m10; *p++ = m20;
		*p++ = m01; *p++ = m11; *p++ = m21;
		*p++ = m02; *p++ = m12; *p++ = m22;
	}
	inline Mat3Base() {m[0]=m[4]=m[8]=T(1);m[1]=m[2]=m[3]=m[5]=m[6]=m[7]=T(0);}
    inline Mat3Base(T d) {m[0]=m[4]=m[8]=d;m[1]=m[2]=m[3]=m[5]=m[6]=m[7]=T(0);}
    inline Mat3Base(const Mat3Base<T>& o) {memcpy(m,o.m,9*sizeof(T));}
    inline explicit Mat3Base(const QuatBase<T>& o) {setQuaternionToMatrixRotation(o);}
	inline const Mat3Base<T>& operator=(const Mat3Base<T>& o) {memcpy(m,o.m,9*sizeof(T));return *this;}		
#   ifndef MATH_HELPER_DISABLE_GLM_MATRIX_SUBSCRIPT_OPERATOR
    inline const T* operator[](int i) const {return col[i];}
    inline T* operator[](int i) {return col[i];}
#   else //MATH_HELPER_DISABLE_GLM_MATRIX_SUBSCRIPT_OPERATOR
    inline T operator[](int i) const {return m[i];}
    inline T& operator[](int i) {return m[i];}
#   endif //MATH_HELPER_DISABLE_GLM_MATRIX_SUBSCRIPT_OPERATOR

	Mat3Base<T> operator*(const Mat3Base<T>& o) const;

	QuatBase<T> getQuaternionFromMatrixRotation() const;
	void setQuaternionToMatrixRotation(const QuatBase<T>& q);

	// A few static methods:
    //static int InvertMatrix(Mat3Base<T>& dst, const Mat3Base<T>& src);
	static void SlerpMatrix(Mat3Base<T>& o ,const Mat3Base<T>& mStart,const Mat3Base<T>& mEnd,T slerpTime_In_0_1);	

    static void FromEulerXYZ(Mat3Base<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerXZY(Mat3Base<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerYXZ(Mat3Base<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerYZX(Mat3Base<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerZXY(Mat3Base<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerZYX(Mat3Base<T>& m,T fXAngle,T fYAngle, T fZAngle);

    static bool ToEulerXYZ(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerXZY(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerYXZ(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerYZX(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerZXY(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerZYX(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);

    static void FromEuler(Mat3Base<T>& m,T fXAngle,T fYAngle,T fZAngle,EulerRotationMode mode);
    static bool ToEuler(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle,EulerRotationMode mode);

    inline Mat3Base<T> transposed() const	{return Mat3Base<T>(m[0],m[3],m[6],m[1],m[4],m[7],m[2],m[5],m[8]);}
    inline void transpose() {*this=transposed();}
    /*inline Mat3Base<T> inverted() const	{Mat3Base<T> o;InvertMatrix(o,*this);return o;}
    inline int inverse() {return InvertMatrix(*this,*this);}*/

	inline Mat3Base<T> slerp(const Mat3Base<T>& o,T slerpTime_In_0_1) const {Mat3Base<T> rv;SlerpMatrix(rv,*this,o,slerpTime_In_0_1);return rv;}

	inline Vec3Base<T> mulDir(T dirX,T dirY,T dirZ) const {return Vec3Base<T>(dirX*m[0] + dirY*m[3] + dirZ*m[6], dirX*m[1] + dirY*m[4] + dirZ*m[7], dirX*m[2] + dirY*m[5] + dirZ*m[8]);}
    inline Vec3Base<T> operator*(const Vec3Base<T>& o) const {return Vec3Base<T>(o.x*m[0] + o.y*m[3] + o.z*m[6], o.x*m[1] + o.y*m[4] + o.z*m[7], o.x*m[2] + o.y*m[5] + o.z*m[8]);}


    inline void   fprintp(FILE* stream,int width, int precision) const {
		int w = width, p = precision;
		for(int r = 0; r < 3; r++) {
			fprintf(stream, "| %*.*f %*.*f %*.*f |\n",
				w, p, m[r], w, p, m[r+3], w, p, m[r+6]
			);
		}
	}
	inline void   fprint(FILE* stream) const {fprintp(stream, 6, 2);}
	inline void   printp(int width, int precision) const {fprintp(stdout, width, precision);}
	inline void   print() {fprintp(stdout, 6, 2);}

};
template <typename T> inline T* value_ptr(Mat3Base<T>& o) {return o.m;}
template <typename T> inline const T* value_ptr(const Mat3Base<T>& o) {return o.m;}
template <typename T> inline Mat3Base<T> transpose(const Mat3Base<T>& o) {return o.transposed();}
//template <typename T> inline Mat3Base<T> inverse(const Mat3Base<T>& o) {return o.inverted();}
template <typename T> inline Mat3Base<T> mat3_cast(const QuatBase<T>& q) {return Mat3Base<T>(q);}
template <typename T> inline QuatBase<T> quat_cast(const Mat3Base<T>& o) {return o.getQuaternionFromMatrixRotation();}
template <typename T> inline bool isEqual(const Mat3Base<T>& a,const Mat3Base<T>& b,T eps=T(0.0000001)) {for (int i=0;i<9;i++) {if (!isEqual(a.m[i],b.m[i],eps)) return false;} return true;}
typedef Mat3Base<float> Mat3f;
typedef Mat3Base<double> Mat3d;

template <typename T> class MatBase {
	public:
	union {	
		struct {T m[16];};		// column-major order: e.g. m[12],m[13],m[14] are the translation part of the matrix
        struct {T col[4][4];};	// e.g. col[3] is the translation part: col[3][0]=m[12]  col[3][1]=m[13]  col[3][2]=m[14]

        // MatBase<T>::print() displays something like:
        /*
                 xAxis            yAxis            zAxis            trasl
                   |                |                |                |
                   v                v                v                v
         |  m[0]==col[0][0]  m[4]==col[1][0]  m[8] ==col[2][0]  m[12]==col[3][0]  |
         |  m[1]==col[0][1]  m[5]==col[1][1]  m[9] ==col[2][1]  m[13]==col[3][1]  |
         |  m[2]==col[0][2]  m[6]==col[1][2]  m[10]==col[2][2]  m[14]==col[3][2]  |
         |  --------------------------------------------------------------------  |
         |  m[3]==col[0][3]  m[7]==col[1][3]  m[11]==col[2][3]  m[15]==col[3][3]  |
        */
    };

	inline explicit MatBase(const T* m16) {memcpy(m,m16,16*sizeof(T));}
	inline MatBase(T m00,T m10, T m20, T m30, T m01, T m11, T m21, T m31, T m02, T m12, T m22, T m32, T m03, T m13, T m23, T m33) {
		T* p = m;		
		*p++ = m00; *p++ = m10; *p++ = m20; *p++ = m30;
		*p++ = m01; *p++ = m11; *p++ = m21; *p++ = m31;
		*p++ = m02; *p++ = m12; *p++ = m22; *p++ = m32;
		*p++ = m03; *p++ = m13; *p++ = m23; *p++ = m33;		
	}
	inline MatBase() {m[0]=m[5]=m[10]=m[15]=T(1);m[1]=m[2]=m[3]=m[4]=m[6]=m[7]=m[8]=m[9]=m[11]=m[12]=m[13]=m[14]=T(0);}
    inline MatBase(T d) {m[0]=m[5]=m[10]=m[15]=d;m[1]=m[2]=m[3]=m[4]=m[6]=m[7]=m[8]=m[9]=m[11]=m[12]=m[13]=m[14]=T(0);}
    inline MatBase(const MatBase<T>& o) {memcpy(m,o.m,16*sizeof(T));}
    inline MatBase(const Mat3Base<T>& o) {memcpy(&m[0],&o.m[0],3*sizeof(T));memcpy(&m[4],&o.m[4],3*sizeof(T));memcpy(&m[8],&o.m[8],3*sizeof(T));m[3]=m[7]=m[11]=T(0);m[15]=T(1);}
    inline explicit MatBase(const QuatBase<T>& o) {setQuaternionToMatrixRotation(o);m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=T(0);m[15]=T(1);}
    inline const MatBase<T>& operator=(const MatBase<T>& o) {memcpy(m,o.m,16*sizeof(T));return *this;}
#   ifndef MATH_HELPER_DISABLE_GLM_MATRIX_SUBSCRIPT_OPERATOR
    inline const T* operator[](int i) const {return col[i];}
    inline T* operator[](int i) {return col[i];}
#   else //MATH_HELPER_DISABLE_GLM_MATRIX_SUBSCRIPT_OPERATOR
    inline T operator[](int i) const {return m[i];}
    inline T& operator[](int i) {return m[i];}
#   endif //MATH_HELPER_DISABLE_GLM_MATRIX_SUBSCRIPT_OPERATOR

	MatBase<T> operator*(const MatBase<T>& o) const;
    inline const MatBase<T>& operator*=(const MatBase<T>& o) {*this = (*this)*o;return *this;}


	MatBase<T>& translate(T x,T y,T z);
	MatBase<T>& rotate(T degAngle,T x,T y,T z);
	MatBase<T>& scale(T x,T y,T z);

	QuatBase<T> getQuaternionFromMatrixRotation() const;
	void setQuaternionToMatrixRotation(const QuatBase<T>& q);

	// A few static methods
	static MatBase<T> LookAt(T eyeX,T eyeY,T eyeZ,T centerX,T centerY,T centerZ,T upX,T upY,T upZ);
	static MatBase<T> Perspective(T degfovy,T aspect, T zNear, T zFar);
	static MatBase<T> Ortho(T left,T right, T bottom, T top,T nearVal,T farVal);
	static int InvertMatrix(MatBase<T>& dst, const MatBase<T>& src);
	static void SlerpMatrix(MatBase<T>& o ,const MatBase<T>& mStart,const MatBase<T>& mEnd,T slerpTime_In_0_1);
	static void FromRotScaTra(MatBase<T>& m,const Mat3Base<T>& rot,const Vec3Base<T>& sca,const Vec3Base<T>& tra);
	static void LookAtYX(MatBase<T>& m,const Vec3Base<T>& lookAt,T minDistance=0,T maxDistance=0,T pitchLimit = 1.483486111);

    static void FromEulerXYZ(MatBase<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerXZY(MatBase<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerYXZ(MatBase<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerYZX(MatBase<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerZXY(MatBase<T>& m,T fXAngle,T fYAngle, T fZAngle);
    static void FromEulerZYX(MatBase<T>& m,T fXAngle,T fYAngle, T fZAngle);

    static bool ToEulerXYZ(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerXZY(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerYXZ(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerYZX(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerZXY(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);
    static bool ToEulerZYX(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle);

    static void FromEuler(MatBase<T>& m,T fXAngle,T fYAngle,T fZAngle,EulerRotationMode mode);
    static bool ToEuler(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle,EulerRotationMode mode);

	inline void getRotationToMat3(Mat3Base<T>& o) const {for (int i=0;i<3;i++) for (int j=0;j<3;j++) o.col[i][j]=col[i][j];}
	inline void setRotationFromMat3(const Mat3Base<T>& m) {for (int i=0;i<3;i++) for (int j=0;j<3;j++) col[i][j]=m.col[i][j];}	

    inline MatBase<T> transposed() const	{return MatBase<T>(m[0],m[4],m[8],m[12],m[1],m[5],m[9],m[13],m[2],m[6],m[10],m[14],m[3],m[7],m[11],m[15]);}
    inline void transpose() {*this=transposed();}
    inline MatBase<T> inverted() const	{MatBase<T> o;InvertMatrix(o,*this);return o;}
	inline int inverse() {return InvertMatrix(*this,*this);}

	MatBase<T> invertedFast() const;
	inline void invertFast() {*this = invertedFast();}

	inline MatBase<T> slerp(const MatBase<T>& o,T slerpTime_In_0_1) const {MatBase<T> rv;SlerpMatrix(rv,*this,o,slerpTime_In_0_1);return rv;}

	void invertXZAxis();
	inline MatBase<T> invertedXZAxis() const {MatBase<T> o=*this;o.invertXZAxis();return o;}
    //inline Vec3Base<T> mulDir(T dirX,T dirY,T dirZ) const {return Vec3Base<T>(dirX*m[0] + dirY*m[4] + dirZ*m[8], dirX*m[1] + dirY*m[5] + dirZ*m[9], dirX*m[2] + dirY*m[6] + dirZ*m[10]);}
    //Vec3Base<T> mulPos(T posX,T posY,T posZ) const;

    inline Vec4Base<T> operator*(const Vec4Base<T>& o) const {return Vec4Base<T>(o.x*m[0] + o.y*m[4] + o.z*m[8] + o.w*m[12],o.x*m[1] + o.y*m[5] + o.z*m[9] + o.w*m[13],o.x*m[2] + o.y*m[6] + o.z*m[10]+ o.w*m[14],o.x*m[3] + o.y*m[7] + o.z*m[11]+ o.w*m[15]);}

/*
	inline MatBase<T> operator+(const MatBase<T>& o) const	{return MatBase<T>(x+o.x,y+o.y,z+o.z,w+o.w);}
	inline void operator+=(const MatBase<T>& o) {x+=o.x;y+=o.y;z+=o.z;w+=o.w;}
	inline MatBase<T> operator+(T s) const	{return MatBase<T>(x+s,y+s,z+s,w+s);}
	inline void operator+=(T s) {x+=s;y+=s;z+=s;w+=s;}

	inline MatBase<T> operator-(const MatBase<T>& o) const	{return MatBase<T>(x-o.x,y-o.y,z-o.z,w-o.w);}
	inline void operator-=(const MatBase<T>& o) {x-=o.x;y-=o.y;z-=o.z;w-=o.w;}
	inline MatBase<T> operator-(T s) const	{return MatBase<T>(x-s,y-s,z-s,w-s);}	
	inline void operator-=(T s) {x-=s;y-=s;z-=s;w-=s;}
	
	inline MatBase<T> operator*(const MatBase<T>& o) const	{return MatBase<T>(x*o.x,y*o.y,z*o.z,w*o.w);}
	inline void operator*=(const MatBase<T>& o) {x*=o.x;y*=o.y;z*=o.z;w*=o.w;}
	inline MatBase<T> operator*(T s) const	{return MatBase<T>(x*s,y*s,z*s,w*s);}
	inline void operator*=(T s) {x*=s;y*=s;z*=s;w*=s;}
	
	inline MatBase<T> operator/(const MatBase<T>& o) const	{return MatBase<T>(x/o.x,y/o.y,z/o.z,w/o.w);}
	inline void operator/=(const MatBase<T>& o) {x/=o.x;y/=o.y;z/=o.z;w/=o.w;}
	inline MatBase<T> operator/(T s) const	{return MatBase<T>(x/s,y/s,z/s,w/s);}
	inline void operator/=(T s) {x/=s;y/=s;z/=s;w/=s;}
	
	inline T dot(const MatBase<T>& o) const {return x*o.x+y*o.y+z*o.z+w*o.w;}

	inline T  length2()  const  {return x*x+y*y+z*z+w*w;}
	inline T  length()  const  {return sqrt(x*x+y*y+z*z+w*w);}
	inline void normalize() {const T len=length();if (len>0) {x/=len;y/=len;z/=len;w/=len;} else {x=y=z=w=T(0);}}		
	inline MatBase<T> normalized() const	{const T len=length();return (len>0) ? MatBase<T>(x/len,y/len,z/len,w/=len) : MatBase<T>();}
		
	inline MatBase<T> lerp(const MatBase<T>& o,T t) const   {float ct = T(1)-t;return MatBase<T>(x*ct+o.x*t,y*ct+o.y*t,z*ct+o.z*t,w*ct+o.w*t);}

	MatBase<T> slerp(const MatBase<T>& o,T factor);
*/

    inline void   fprintp(FILE* stream,int width, int precision) const {
		int w = width, p = precision;
		for(int r = 0; r < 4; r++) {
			fprintf(stream, "| %*.*f %*.*f %*.*f %*.*f |\n",
				w, p, m[r], w, p, m[r+4], w, p, m[r+8], w, p, m[r+12]
			);
		}
	}
	inline void   fprint(FILE* stream) const {fprintp(stream, 6, 2);}
	inline void   printp(int width, int precision) const {fprintp(stdout, width, precision);}
	inline void   print() {fprintp(stdout, 6, 2);}

};
template <typename T> inline T* value_ptr(MatBase<T>& o) {return o.m;}
template <typename T> inline const T* value_ptr(const MatBase<T>& o) {return o.m;}
template <typename T> inline MatBase<T> transpose(const MatBase<T>& o) {return o.transposed();}
template <typename T> inline MatBase<T> inverse(const MatBase<T>& o) {return o.inverted();}
template <typename T> inline Mat3Base<T> mat3_cast(const MatBase<T>& o) {Mat3Base<T> tmp;o.getRotationToMat3(tmp);return tmp;}
template <typename T> inline MatBase<T> mat4_cast(const QuatBase<T>& q) {return MatBase<T>(q);}
template <typename T> inline QuatBase<T> quat_cast(const MatBase<T>& o) {return o.getQuaternionFromMatrixRotation();}
template <typename T> inline MatBase<T> perspective(T radfovy,T aspect, T zNear, T zFar) {return MatBase<T>::Perspective(degrees(radfovy),aspect,zNear,zFar);}
template <typename T> inline MatBase<T> ortho(T left,T right, T bottom, T top,T nearVal,T farVal) {return MatBase<T>::Ortho(left,right,bottom,top,nearVal,farVal);}
template <typename T> inline MatBase<T> lookAt(const Vec3Base<T>& eye,const Vec3Base<T>& center,const Vec3Base<T>& up) {return MatBase<T>::LookAt(eye.x,eye.y,eye.z,center.x,center.y,center.z,up.x,up.y,up.z);}
template <typename T> inline MatBase<T> rotate(const MatBase<T>& m,T radAngle,const Vec3Base<T>& axis) {MatBase<T> o(m);o.rotate(degrees(radAngle),axis.x,axis.y,axis.z);return o;}
template <typename T> inline MatBase<T> translate(const MatBase<T>& m,const Vec3Base<T>& offset) {MatBase<T> o(m);o.translate(offset.x,offset.y,offset.z);return o;}
template <typename T> inline MatBase<T> scale(const MatBase<T>& m,const Vec3Base<T>& scaling) {MatBase<T> o(m);o.scale(scaling.x,scaling.y,scaling.z);return o;}
template <typename T> inline bool isEqual(const MatBase<T>& a,const MatBase<T>& b,T eps=T(0.0000001)) {for (int i=0;i<16;i++) {if (!isEqual(a.m[i],b.m[i],eps)) return false;} return true;}
template <typename T> inline void FromRotScaTra(MatBase<T>& output,const Mat3Base<T>& rot,const Vec3Base<T>& sca,const Vec3Base<T>& tra) {MatBase<T>::FromRotScaTra(output,rot,sca,tra);}

typedef MatBase<float> Matf;
typedef MatBase<double> Matd;
typedef Matf Mat4f;
typedef Matd Mat4d;


#ifdef MATH_HELPER_DEFAULT_DOUBLE_PRECISION
typedef Vec2d Vec2;
typedef Vec3d Vec3;
typedef Vec4d Vec4;
typedef Quatd Quat;
typedef Mat3d Mat3;
typedef Mat4d Mat4;
typedef Matd Mat;
#else //MATH_HELPER_DEFAULT_DOUBLE_PRECISION
typedef Vec2f Vec2;
typedef Vec3f Vec3;
typedef Vec4f Vec4;
typedef Quatf Quat;
typedef Mat3f Mat3;
typedef Mat4f Mat4;
typedef Matf Mat;
#endif //MATH_HELPER_DEFAULT_DOUBLE_PRECISION

#ifndef MATH_HELPER_DISABLE_LOWERCASE_TYPEDEFS
typedef Vec2f vec2f;typedef Vec2d vec2d;typedef Vec2 vec2;
typedef Vec3f vec3f;typedef Vec3d vec3d;typedef Vec3 vec3;
typedef Vec4f vec4f;typedef Vec4d vec4d;typedef Vec4 vec4;
typedef Quatf quatf;typedef Quatd quatd;typedef Quat quat;
typedef Mat3f mat3f;typedef Mat3d mat3d;typedef Mat3 mat3;
typedef Mat4f mat4f;typedef Mat4d mat4d;typedef Mat4 mat4;
typedef Matf matf;typedef Matd matd;typedef Mat mat;
#endif //MATH_HELPER_DISABLE_LOWERCASE_TYPEDEFS


#pragma pack(pop)

} // namespace MATH_HELPER_NAMESPACE_NAME


#ifndef MATH_HELPER_DISABLE_GLM_ALIASING
namespace mhe=glm;
#endif //MATH_HELPER_DISABLE_GLM_ALIASING





// Start Implementation here (template classes must have it in the same header file)--------------

namespace MATH_HELPER_NAMESPACE_NAME {


// Implementation:
template <typename T> QuatBase<T> QuatBase<T>::slerp(const QuatBase<T>& o,T slerpTime_In_0_1,bool normalizeQOutAfterLerp,T eps) const	{
	//	
	//
    //const int normalizeQOutAfterLerp = 1;            // When using Lerp instead of Slerp qOut should be normalized. However some users prefer setting eps small enough so that they can leave the Lerp as it is.
    //const T eps(0.0001);                      	 // In [0 = 100% Slerp,1 = 100% Lerp] Faster but less precise with bigger epsilon (Lerp is used instead of Slerp more often). Users should tune it to achieve a performance boost.
    const int useAcosAndSinInsteadOfAtan2AndSqrt = 0;// Another possible minimal Speed vs Precision tweak (I suggest just changing it here and not in the caller code)
	const T one(1.0);
    const QuatBase<T>& qStart=*this;
	QuatBase<T> qEnd=o;
	QuatBase<T> qOut;	

	T fCos = qStart.x * qEnd.x + qStart.y * qEnd.y + qStart.z * qEnd.z + qStart.w * qEnd.w;

    // Do we need to invert rotation?
    if(fCos < 0)	//Originally it was if(fCos < static_cast < T >(0.0) && shortestPath)
        {fCos = -fCos;qEnd.x = -qEnd.x;qEnd.y = -qEnd.y;qEnd.z = -qEnd.z;qEnd.w = -qEnd.w;}

    if( fCos < one - eps)	// Originally if was "Ogre::Math::Abs(fCos)" instead of "fCos", but we know fCos>0, because we have hard coded shortestPath=true
    {
        // Standard case (slerp)
        T fSin,fAngle;
        if (!useAcosAndSinInsteadOfAtan2AndSqrt)	{
            // Ogre::Quaternion uses this branch by default
            fSin = sqrt(one - fCos*fCos);
            fAngle = atan2(fSin, fCos);
        }
        else	{
            // Possible replacement of the two lines above
            // (it's hard to tell if they're faster, but my instinct tells me I should trust atan2 better than acos (geometry geeks needed here...)):
            // But probably sin(...) is faster than (sqrt + 1 subtraction and mult)
            fAngle = acos(fCos);
            fSin = sin(fAngle);
        }

		{
        const float fInvSin = one / fSin;
        const float fCoeff0 = sin((one - slerpTime_In_0_1) * fAngle) * fInvSin;
        const float fCoeff1 = sin(slerpTime_In_0_1 * fAngle) * fInvSin;

        //qOut =  fCoeff0 * qStart + fCoeff1 * qEnd; //Avoided for maximum portability and conversion of the code
        qOut.x = (fCoeff0 * qStart.x + fCoeff1 * qEnd.x);
        qOut.y = (fCoeff0 * qStart.y + fCoeff1 * qEnd.y);
        qOut.z = (fCoeff0 * qStart.z + fCoeff1 * qEnd.z);
        qOut.w = (fCoeff0 * qStart.w + fCoeff1 * qEnd.w);
		}
    } else
    {
        // There are two situations:
        // 1. "qStart" and "qEnd" are very close (fCos ~= +1), so we can do a linear
        //    interpolation safely.
        // 2. "qStart" and "qEnd" are almost inverse of each other (fCos ~= -1), there
        //    are an infinite number of possibilities interpolation. but we haven't
        //    have method to fix this case, so just use linear interpolation here.
        // IMPORTANT: CASE 2 can't happen anymore because we have hardcoded "shortestPath = true" and now fCos > 0

        const float fCoeff0 = one - slerpTime_In_0_1;
        const float fCoeff1 = slerpTime_In_0_1;

        //qOut =  fCoeff0 * qStart + fCoeff1 * qEnd; //Avoided for maximum portability and conversion of the code
        qOut.x = (fCoeff0 * qStart.x + fCoeff1 * qEnd.x);
        qOut.y = (fCoeff0 * qStart.y + fCoeff1 * qEnd.y);
        qOut.z = (fCoeff0 * qStart.z + fCoeff1 * qEnd.z);
        qOut.w = (fCoeff0 * qStart.w + fCoeff1 * qEnd.w);
        if (normalizeQOutAfterLerp)  qOut.normalize();;
    }

   	return qOut;
}
template <typename T> void QuatBase<T>::fromAngleAxis(T rfAngle,const Vec3Base<T>& rkAxis)	{
        // assert:  axis[] is unit length
        //
        // The quaternion representing the rotation is
        //   q = cos(A/2)+sin(A/2)*(x*i+y*j+z*k)

        const T fHalfAngle ( static_cast<T>(0.5)*rfAngle );const T fSin = sin(fHalfAngle);
        w = cos(fHalfAngle);x = fSin*rkAxis.x;y = fSin*rkAxis.y;z = fSin*rkAxis.z;
}
template <typename T> void QuatBase<T>::toAngleAxis(T& rfAngle,Vec3Base<T>& rkAxis) const	{
// These both seem to work.
// Implementation 1
        // The quaternion representing the rotation is
        //   q = cos(A/2)+sin(A/2)*(x*i+y*j+z*k)

        T fSqrLength = x*x+y*y+z*z;
        if ( fSqrLength > T(0.0) )  {
            rfAngle = T(2.0)*acos(w);T fInvLength = T(1.0)/sqrt(fSqrLength);
            rkAxis[0] = x*fInvLength;rkAxis[1] = y*fInvLength;rkAxis[2] = z*fInvLength;
        }
        else    {
            // angle is 0 (mod 2*pi), so any axis will do
            rfAngle = rkAxis[0] = rkAxis[2] = static_cast < T > (0.0);
            rkAxis[1] = static_cast < T > (1.0);
        }
/*// Implementation 2
    // more based on the glm library code:
    rfAngle = acos(w) * T(2);
    T tmp1 = static_cast<T>(1) - w * w;
    if(tmp1 <= static_cast<T>(0))   rkAxis = Vec3Base<T>(0, 0, 1);
    else    {
        T tmp2 = static_cast<T>(1) / sqrt(tmp1);
        rkAxis = Vec3Base<T>(x * tmp2, y * tmp2, z * tmp2);
    }*/
}
//-----------------------------------------------------------------------
template <typename T> QuatBase<T> QuatBase<T>::LookAtYX(const Vec3Base<T>& sourcePosition,const Vec3Base<T>& targetPosition)	{
    Vec3Base<T> D(targetPosition-sourcePosition);
    T Dxz2(D.x*D.x+D.z*D.z);
    T Dxz(::sqrt(Dxz2));
    T AY(::atan2(D.x,D.z));
    T AX(-::atan2(D.y,Dxz));
    return QuatBase<T>(AY,Vec3Base<T>(0.,1.,0.)) * QuatBase<T>(AX,Vec3Base<T>(1.,0.,0.));
}
template <typename T> void QuatBase<T>::FromEuler(QuatBase<T>& qOut,T fAngleX,T fAngleY,T fAngleZ,EulerRotationMode eulerRotationMode)	{
    QuatBase<T> qx(fAngleX,Vec3Base<T>(1.0,0.0,0.0));
    QuatBase<T> qy(fAngleY,Vec3Base<T>(0.0,1.0,0.0));
    QuatBase<T> qz(fAngleZ,Vec3Base<T>(0.0,0.0,1.0));
    switch (eulerRotationMode) {
    case EULER_XYZ: qOut = qz*qy*qx;break;
    case EULER_XZY: qOut = qy*qz*qx;break;
    case EULER_YXZ: qOut = qz*qx*qy;break;
    case EULER_YZX: qOut = qx*qz*qy;break;
    case EULER_ZXY: qOut = qy*qx*qz;break;
    case EULER_ZYX: qOut = qx*qy*qz;break;
    default: qOut = qz*qx*qy;break;
    }
}
template <typename T> Mat3Base<T> Mat3Base<T>::operator*(const Mat3Base<T>& o) const	{
    int i,j,j3;Mat3Base<T> result9;
    for(i = 0; i < 3; i++) {
        for(j = 0; j < 3; j++) {
        	  j3 = 3*j; result9.m[i+j3] = m[i]*o.m[j3] + m[i+3]*o.m[1+j3] + m[i+6]*o.m[2+j3];
        }
    }
    return result9;
}
#if 0
template <typename T> int Mat3Base<T>::InvertMatrix(Mat3Base<T>& dst,const Mat3Base<T>& src) {
    // What about this:
	//dst.setQuaternionToMatrixRotation(mhe::inverse(mhe::quat_cast(src)));
	
	// this code comes from the glm library:	
    const T det = (
            + src.col[0][0] * (src.col[1][1] * src.col[2][2] - src.col[2][1] * src.col[1][2])
            - src.col[1][0] * (src.col[0][1] * src.col[2][2] - src.col[2][1] * src.col[0][2])
            + src.col[2][0] * (src.col[0][1] * src.col[1][2] - src.col[1][1] * src.col[0][2]));
    if (det==0) return 0;
    const T OneOverDeterminant = static_cast<T>(1) / det;

    dst.col[0][0] = + (src.col[1][1] * src.col[2][2] - src.col[2][1] * src.col[1][2]) * OneOverDeterminant;
    dst.col[1][0] = - (src.col[1][0] * src.col[2][2] - src.col[2][0] * src.col[1][2]) * OneOverDeterminant;
    dst.col[2][0] = + (src.col[1][0] * src.col[2][1] - src.col[2][0] * src.col[1][1]) * OneOverDeterminant;
    dst.col[0][1] = - (src.col[0][1] * src.col[2][2] - src.col[2][1] * src.col[0][2]) * OneOverDeterminant;
    dst.col[1][1] = + (src.col[0][0] * src.col[2][2] - src.col[2][0] * src.col[0][2]) * OneOverDeterminant;
    dst.col[2][1] = - (src.col[0][0] * src.col[2][1] - src.col[2][0] * src.col[0][1]) * OneOverDeterminant;
    dst.col[0][2] = + (src.col[0][1] * src.col[1][2] - src.col[1][1] * src.col[0][2]) * OneOverDeterminant;
    dst.col[1][2] = - (src.col[0][0] * src.col[1][2] - src.col[1][0] * src.col[0][2]) * OneOverDeterminant;
    dst.col[2][2] = + (src.col[0][0] * src.col[1][1] - src.col[1][0] * src.col[0][1]) * OneOverDeterminant;
    return 1;

/*		// this from OgreMath:
        // Invert a 3x3 using cofactors.  This is about 8 times faster than
        // the Numerical Recipes code which uses Gaussian elimination.

        dst.col[0][0] = src.col[1][1]*src.col[2][2] - src.col[1][2]*src.col[2][1];
        dst.col[0][1] = src.col[0][2]*src.col[2][1] - src.col[0][1]*src.col[2][2];
        dst.col[0][2] = src.col[0][1]*src.col[1][2] - src.col[0][2]*src.col[1][1];
        dst.col[1][0] = src.col[1][2]*src.col[2][0] - src.col[1][0]*src.col[2][2];
        dst.col[1][1] = src.col[0][0]*src.col[2][2] - src.col[0][2]*src.col[2][0];
        dst.col[1][2] = src.col[0][2]*src.col[1][0] - src.col[0][0]*src.col[1][2];
        dst.col[2][0] = src.col[1][0]*src.col[2][1] - src.col[1][1]*src.col[2][0];
        dst.col[2][1] = src.col[0][1]*src.col[2][0] - src.col[0][0]*src.col[2][1];
        dst.col[2][2] = src.col[0][0]*src.col[1][1] - src.col[0][1]*src.col[1][0];

        T fDet =
            src.col[0][0]*dst.col[0][0]+
            src.col[0][1]*dst.col[1][0]+
            src.col[0][2]*dst.col[2][0];

        if ( fabs(fDet) <= 0) return 0;

        T fInvDet = T(1.0)/fDet;
        for (int iRow = 0; iRow < 3; iRow++)
        {
            for (int iCol = 0; iCol < 3; iCol++)
                dst.col[iRow][iCol] *= fInvDet;
        }
        return 1;
        */
/* From GTEngine (maybe we must swap row and columns)

    int invertible = 0;
    T c00 = src.col[1, 1]*src.col[2, 2] - src.col[1, 2]*src.col[2, 1];
    T c10 = src.col[1, 2]*src.col[2, 0] - src.col[1, 0]*src.col[2, 2];
    T c20 = src.col[1, 0]*src.col[2, 1] - src.col[1, 1]*src.col[2, 0];
    T det = src.col[0, 0]*c00 + src.col[0, 1]*c10 + src.col[0, 2]*c20;
    if (det != (T)0)
    {
        T invDet = ((T)1) / det;

        dst.col[0][0] = c00*invDet,
        dst.col[1][0] = (src.col[0, 2]*src.col[2, 1] - src.col[0, 1]*src.col[2, 2])*invDet,
        dst.col[2][0] = (src.col[0, 1]*src.col[1, 2] - src.col[0, 2]*src.col[1, 1])*invDet,
        dst.col[0][1] = c10*invDet,
        dst.col[1][1] = (src.col[0, 0]*src.col[2, 2] - src.col[0, 2]*src.col[2, 0])*invDet,
        dst.col[2][1] = (src.col[0, 2]*src.col[1, 0] - src.col[0, 0]*src.col[1, 2])*invDet,
        dst.col[0][2] = c20*invDet,
        dst.col[1][2] = (src.col[0, 1]*src.col[2, 0] - src.col[0, 0]*src.col[2, 1])*invDet,
        dst.col[2][2] = (src.col[0, 0]*src.col[1, 1] - src.col[0, 1]*src.col[1, 0])*invDet

        invertible = 1;
    }
    return invertible;
*/
}
#endif
template <typename T> QuatBase<T> Mat3Base<T>::getQuaternionFromMatrixRotation() const	{
	QuatBase<T> q;    
#if 0   // incorrect
	T m00=m[0], m10=m[1], m20=m[2];
    T m01=m[3], m11=m[4], m21=m[5];
    T m02=m[6], m12=m[7], m22=m[8];
    /*T m00=m[0], m10=m[3], m20=m[6];
    T m01=m[1], m11=m[4], m21=m[7];
    T m02=m[2], m12=m[5], m22=m[8]; */
    T trace = m00 + m11 + m22;
    if (trace > 0) {
        T s = sqrt(trace + 1.0);
        q[3]=(s * 0.5);s = 0.5 / s;
        q[0]=((m12 - m21) * s);
        q[1]=((m20 - m02) * s);
        q[2]=((m01 - m10) * s);
    }
    else {
        // i,j,k are all 0,1 or 2
        int i = m00 < m11 ?
            (m11 < m22 ? 2 : 1) :
            (m00 < m22 ? 2 : 0);
        int j = (i + 1) % 3;
        int k = (i + 2) % 3;
        /*T s = sqrt(m[i][i] - m[j][j] - m[k][k] + 1.0f);
        q[i] = s * 0.5;s = 0.5 / s;
        q[3] = (m[j][k] - m[k][j]) * s;
        q[j] = (m[i][j] + m[j][i]) * s;
        q[k] = (m[i][k] + m[k][i]) * s;*/
        // >>> m[i][j] = m[j*4+i] because m[2][1] = m21 = m[6]
        int i3 = 3*i, j3 = 3*j, k3 = 3*k;
        T s = sqrt(m[i3+i] - m[j3+j] - m[k3+k] + 1.0);
        q[i] = s * 0.5;s = 0.5 / s;
        q[3] = (m[k3+j] - m[j3+k]) * s;
        q[j] = (m[j3+i] + m[i3+j]) * s;
        q[k] = (m[k3+i] + m[i3+k]) * s;
        /*T s = sqrt(m[i3+i] - m[j3+j] - m[k3+k] + 1.0);
        q[i] = s * 0.5;s = 0.5 / s;
        q[3] = (m[k+j3] - m[j+k3]) * s;
        q[j] = (m[j+i3] + m[i+j3]) * s;
        q[k] = (m[k+i3] + m[i+k3]) * s;*/
    }
#else   // glm based
        T fourXSquaredMinus1 = col[0][0] - col[1][1] - col[2][2];
        T fourYSquaredMinus1 = col[1][1] - col[0][0] - col[2][2];
        T fourZSquaredMinus1 = col[2][2] - col[0][0] - col[1][1];
        T fourWSquaredMinus1 = col[0][0] + col[1][1] + col[2][2];

        int biggestIndex = 0;
        T fourBiggestSquaredMinus1 = fourWSquaredMinus1;
        if(fourXSquaredMinus1 > fourBiggestSquaredMinus1)
        {
            fourBiggestSquaredMinus1 = fourXSquaredMinus1;
            biggestIndex = 1;
        }
        if(fourYSquaredMinus1 > fourBiggestSquaredMinus1)
        {
            fourBiggestSquaredMinus1 = fourYSquaredMinus1;
            biggestIndex = 2;
        }
        if(fourZSquaredMinus1 > fourBiggestSquaredMinus1)
        {
            fourBiggestSquaredMinus1 = fourZSquaredMinus1;
            biggestIndex = 3;
        }

        T biggestVal = sqrt(fourBiggestSquaredMinus1 + T(1)) * T(0.5);
        T mult = static_cast<T>(0.25) / biggestVal;

        switch(biggestIndex)
        {
        case 0:
            q.w = biggestVal;
            q.x = (col[1][2] - col[2][1]) * mult;
            q.y = (col[2][0] - col[0][2]) * mult;
            q.z = (col[0][1] - col[1][0]) * mult;
            break;
        case 1:
            q.w = (col[1][2] - col[2][1]) * mult;
            q.x = biggestVal;
            q.y = (col[0][1] + col[1][0]) * mult;
            q.z = (col[2][0] + col[0][2]) * mult;
            break;
        case 2:
            q.w = (col[2][0] - col[0][2]) * mult;
            q.x = (col[0][1] + col[1][0]) * mult;
            q.y = biggestVal;
            q.z = (col[1][2] + col[2][1]) * mult;
            break;
        case 3:
            q.w = (col[0][1] - col[1][0]) * mult;
            q.x = (col[2][0] + col[0][2]) * mult;
            q.y = (col[1][2] + col[2][1]) * mult;
            q.z = biggestVal;
            break;

        default:					// Silence a -Wswitch-default warning in GCC. Should never actually get here. Assert is just for sanity.
            //assert(false);
            q.x=q.y=q.z=T(0);q.w=T(1);
            break;
        }
#endif
	return q;
}
template <typename T> void Mat3Base<T>::setQuaternionToMatrixRotation(const QuatBase<T>& q)	{
#if 0 // incorrect
    T d = q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
    if (d == 0.0) {m[0]=m[4]=m[8]=1;m[1]=m[2]=m[3]=m[5]=m[6]=m[7]=0;return;}
    {
    T s = 2.0 / d;
    T xs = q[0] * s,   ys = q[1] * s,   zs = q[2] * s;
    T wx = q[3] * xs,  wy = q[3] * ys,  wz = q[3] * zs;
    T xx = q[0] * xs,  xy = q[0] * ys,  xz = q[0] * zs;
    T yy = q[1] * ys,  yz = q[1] * zs,  zz = q[2] * zs;

    m[0] = 1.0 - (yy + zz);  m[1] = xy - wz;           m[2] = xz + wy;
    m[3] = xy + wz;          m[4] = 1.0 - (xx + zz);   m[5] = yz - wx;
    m[6] = xz - wy;          m[7] = yz + wx;           m[8] = 1.0 - (xx + yy);

    /*m[0] = 1.0 - (yy + zz); m[3] = xy - wz;           m[6] = xz + wy;
    m[1] = xy + wz;          m[4] = 1.0f - (xx + zz);  m[7] = yz - wx;
    m[2] = xz - wy;          m[5] = yz + wx;           m[8] = 1.0 - (xx + yy); */
    }
#else // glm based
        T qxx(q.x * q.x);
        T qyy(q.y * q.y);
        T qzz(q.z * q.z);
        T qxz(q.x * q.z);
        T qxy(q.x * q.y);
        T qyz(q.y * q.z);
        T qwx(q.w * q.x);
        T qwy(q.w * q.y);
        T qwz(q.w * q.z);

        col[0][0] = 1 - 2 * (qyy +  qzz);
        col[0][1] = 2 * (qxy + qwz);
        col[0][2] = 2 * (qxz - qwy);

        col[1][0] = 2 * (qxy - qwz);
        col[1][1] = 1 - 2 * (qxx +  qzz);
        col[1][2] = 2 * (qyz + qwx);

        col[2][0] = 2 * (qxz + qwy);
        col[2][1] = 2 * (qyz - qwx);
        col[2][2] = 1 - 2 * (qxx +  qyy);
#endif
}
template <typename T> void Mat3Base<T>::SlerpMatrix(Mat3Base<T>& o ,const Mat3Base<T>& mStart,const Mat3Base<T>& mEnd,T slerpTime_In_0_1)	{
	const T* mStart9 = mStart.m;   
	const T* mEnd9 = mEnd.m;   
	T* mOut9 = o.m;   
	if (slerpTime_In_0_1<=0)        {if (mOut9!=mStart9) o=mStart;return;}
    else if (slerpTime_In_0_1>=1)   {if (mOut9!=mEnd9)   o=mEnd; return;}

    {
 		// Slerp rotations
		QuatBase<T> qStart = mStart.getQuaternionFromMatrixRotation();
		QuatBase<T> qEnd = mEnd.getQuaternionFromMatrixRotation();
		o.setMatrixRotationFromQuaternion(qStart.slerp(qEnd,slerpTime_In_0_1));
    }
}


// The __MatTypeFromEuler/__MatTypeToEuler methods are ported/adapted/arranged/inspired from the following sources:
//
// https://github.com/juj/MathGeoLib (Apache 2 license)
// [And going down the chain some of them are adapted from http://www.geometrictools.com/Documentation/EulerAngles.pdf]
//
// OgreMatrix3x3.cpp (based on geometrictools itself)
//
// Reference code used a row-major matrix storage so I had to change many things... I'm not sure it's 100% correct!
// Use it at your own risk!
//
// ALso we should make roundtrip tests and see if these methods pass them or not!

// Slow! By factorizing the 3 matrix multiplications of each __MatTypeFromEuler function we can improve performance
// https://github.com/juj/MathGeoLib (and the Wild Magic Math Library from http://www.geometrictools.com) provides
// factorized implementation, but they use a different convention (row-major order and maybe a different matrix multiplication order),
// so I could not use their code.
template <typename T,typename MatType> inline static void __MatTypeFromEulerCommonHelper(MatType& kXMat,MatType& kYMat,MatType& kZMat,T x,T y,T z)	{
    T cx, sx, cy, sy, cz, sz;

    SinCos(x, sx, cx);
    kXMat=Mat3Base<T>(1.0,0.0,0.0,0.0,cx,sx,0.0,-sx,cx);

    SinCos(y, sy, cy);
    kYMat=Mat3Base<T>(cy,0.0,-sy,0.0,1.0,0.0,sy,0.0,cy);

    SinCos(z, sz, cz);
    kZMat=Mat3Base<T>(cz,sz,0.0,-sz,cz,0.0,0.0,0.0,1.0);
}
template <typename T,typename MatType> inline static void __MatTypeFromEulerXYZ(MatType& m,T x,T y,T z)	{
    Mat3Base<T> kXMat,kYMat,kZMat;__MatTypeFromEulerCommonHelper(kXMat,kYMat,kZMat,x,y,z);
    m=kZMat*kYMat*kXMat;
}
template <typename T,typename MatType> inline static void __MatTypeFromEulerXZY(MatType& m,T x,T y,T z)	{
    Mat3Base<T> kXMat,kYMat,kZMat;__MatTypeFromEulerCommonHelper(kXMat,kYMat,kZMat,x,y,z);
    m=kYMat*kZMat*kXMat;
}
template <typename T,typename MatType> inline static void __MatTypeFromEulerYXZ(MatType& m,T x,T y,T z)	{
    Mat3Base<T> kXMat,kYMat,kZMat;__MatTypeFromEulerCommonHelper(kXMat,kYMat,kZMat,x,y,z);
    m=kZMat*kXMat*kYMat;
}
template <typename T,typename MatType> inline static void __MatTypeFromEulerYZX(MatType& m,T x,T y,T z)	{
    Mat3Base<T> kXMat,kYMat,kZMat;__MatTypeFromEulerCommonHelper(kXMat,kYMat,kZMat,x,y,z);
    m=kXMat*kZMat*kYMat;
}
template <typename T,typename MatType> inline static void __MatTypeFromEulerZXY(MatType& m,T x,T y,T z)	{
    Mat3Base<T> kXMat,kYMat,kZMat;__MatTypeFromEulerCommonHelper(kXMat,kYMat,kZMat,x,y,z);
    m=kYMat*kXMat*kZMat;
}
template <typename T,typename MatType> inline static void __MatTypeFromEulerZYX(MatType& m,T x,T y,T z)	{
    Mat3Base<T> kXMat,kYMat,kZMat;__MatTypeFromEulerCommonHelper(kXMat,kYMat,kZMat,x,y,z);
    m=kXMat*kYMat*kZMat;
}

template <typename T,typename MatType> inline static bool __MatTypeToEulerXYZ(const MatType& m,T& x,T& y,T& z)	{
    bool rv = false;
    if (m.col[0][2] < T(1))
    {
        if (m.col[0][2] > -T(1))
        {
            y = asin(-m.col[0][2]);
            z = atan2(m.col[0][1], m.col[0][0]);
            x = atan2(m.col[1][2], m.col[2][2]);
            rv = true;
        }
        else
        {
            // Not a unique solution.
            y = M_HALF_PI;
            z = -atan2(-m.col[2][1], m.col[1][1]);
            x = T(0);
        }
    }
    else
    {
        // Not a unique solution.
        y = -M_HALF_PI;
        z = atan2(-m.col[2][1], m.col[1][1]);
        x = T(0);
    }
    return rv;
}
template <typename T,typename MatType> inline static bool __MatTypeToEulerYXZ(const MatType& m,T& x,T& y,T& z)	{
    bool rv = false;
    if (m.col[1][2] < T(1))
    {
        if (m.col[1][2] > -T(1))
        {
            x = asin(m.col[1][2]);
            z = atan2(-m.col[1][0], m.col[1][1]);
            y = atan2(-m.col[0][2], m.col[2][2]);
            rv = true;
        }
        else
        {
            // Not a unique solution.
            x = -M_HALF_PI;
            z = -atan2(-m.col[2][0], m.col[0][0]);
            y = T(0);
        }
    }
    else
    {
        // Not a unique solution.
        x = M_HALF_PI;
        z = atan2(m.col[2][0], m.col[0][0]);
        y = T(0);
    }
    return rv;
}
template <typename T,typename MatType> inline static bool __MatTypeToEulerXZY(const MatType& m,T& x,T& y,T& z)	{
    bool rv = false;
    if (m.col[0][1] < T(1))
    {
        if (m.col[0][1] > -T(1))
        {
            z = asin(m.col[0][1]);
            y = -atan2(m.col[0][2], m.col[0][0]);	// Modified: added minus sign
            x = -atan2(m.col[2][1], m.col[1][1]);	// Modified: added minus sign
            rv = true;
        }
        else
        {
            // Not a unique solution.
            z = -M_HALF_PI;
            y = -atan2(m.col[1][2], m.col[2][2]);
            x = T(0);
        }
    }
    else
    {
        // Not a unique solution.
        z = M_HALF_PI;
        y = atan2(-m.col[1][2], m.col[2][2]);
        x = T(0);
    }
    return rv;
}
template <typename T,typename MatType> inline static bool __MatTypeToEulerZXY(const MatType& m,T& x,T& y,T& z)	{
    bool rv = false;
    if (m.col[2][1] < T(1))
    {
        if (m.col[2][1] > -T(1))
        {
            x = asin(-m.col[2][1]);
            y = atan2(m.col[2][0], m.col[2][2]);
            z = atan2(m.col[0][1], m.col[1][1]);
            rv = true;
        }
        else
        {
            // Not a unique solution.
            x = M_HALF_PI;
            y = -atan2(-m.col[1][0], m.col[0][0]);
            z = T(0);
        }
    }
    else
    {
        // Not a unique solution.
        x = -M_HALF_PI;
        y = atan2(-m.col[1][0], m.col[0][0]);
        z = T(0);
    }
    return rv;
}
template <typename T,typename MatType> inline static bool __MatTypeToEulerYZX(const MatType& m,T& x,T& y,T& z)	{
    bool rv = false;
    if (m.col[1][0] < T(1))
    {
        if (m.col[1][0] > -T(1))
        {
            z = asin(-m.col[1][0]);
            x = atan2(m.col[1][2], m.col[1][1]);
            y = atan2(m.col[2][0], m.col[0][0]);
            rv = true;
        }
        else
        {
            // Not a unique solution: y - x = atan2(-m.col[0][2], m.col[2][2]);
            z = M_HALF_PI;
            x = atan2(-m.col[0][2], m.col[2][2]);
            y = T(0);
        }
    }
    else
    {
        // Not a unique solution: y + x = atan2(-m.col[0][2], m.col[2][2]);
        z = -M_HALF_PI;
        x = atan2(-m.col[0][2], m.col[2][2]);
        y = T(0);
    }
    return rv;
}
template <typename T,typename MatType> inline static bool __MatTypeToEulerZYX(const MatType& m,T& x,T& y,T& z)	{
    bool rv = false;
    if (m.col[2][0] < T(1))
    {
        if (m.col[2][0] > -T(1))
        {
            y = asin(m.col[2][0]);
            x = atan2(-m.col[2][1], m.col[2][2]);
            z = atan2(-m.col[1][0], m.col[0][0]);
            rv = true;
        }
        else
        {
            // Not a unique solution: z - x = atan2(m.col[0][1], m.col[1][1]);
            y = -M_HALF_PI;
            x = -atan2(m.col[0][1], m.col[1][1]);
            z = T(0);
        }
    }
    else
    {
        // Not a unique solution: z + x = atan2(m.col[0][1], m.col[1][1]);
        y = M_HALF_PI;
        x = atan2(m.col[0][1], m.col[1][1]);
        z = T(0);
    }
    return rv;
}

template <typename T,typename MatType> inline static void __MatTypeFromEuler(MatType& m,T x,T y,T z,EulerRotationMode mode)   {
    switch (mode) {
    case EULER_XYZ:
        __MatTypeFromEulerXYZ(m,x,y,z);
        break;
    case EULER_XZY:
        __MatTypeFromEulerXZY(m,x,y,z);
        break;
    case EULER_YXZ:
        __MatTypeFromEulerYXZ(m,x,y,z);
        break;
    case EULER_YZX:
        __MatTypeFromEulerYZX(m,x,y,z);
        break;
    case EULER_ZXY:
        __MatTypeFromEulerZXY(m,x,y,z);
        break;
    case EULER_ZYX:
        __MatTypeFromEulerZYX(m,x,y,z);
        break;
    default:
        __MatTypeFromEulerYXZ(m,x,y,z);
        break;
    }
}
template <typename T,typename MatType> inline static bool __MatTypeToEuler(const MatType& m,T& x,T& y,T& z,EulerRotationMode mode)   {
    switch (mode) {
    case EULER_XYZ:
        return __MatTypeToEulerXYZ(m,x,y,z);
        break;
    case EULER_XZY:
        return __MatTypeToEulerXZY(m,x,y,z);
        break;
    case EULER_YXZ:
        return __MatTypeToEulerYXZ(m,x,y,z);
        break;
    case EULER_YZX:
        return __MatTypeToEulerYZX(m,x,y,z);
        break;
    case EULER_ZXY:
        return __MatTypeToEulerZXY(m,x,y,z);
        break;
    case EULER_ZYX:
        return __MatTypeToEulerZYX(m,x,y,z);
        break;
    default:
        return __MatTypeToEulerYXZ(m,x,y,z);
        break;
    }
    return false;
 }


// Mat3Base<T> specialization
template <typename T> void Mat3Base<T>::FromEulerXYZ(Mat3Base<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerXYZ( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool Mat3Base<T>::ToEulerXYZ(const Mat3Base<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerXYZ(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void Mat3Base<T>::FromEulerXZY(Mat3Base<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerXZY( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool Mat3Base<T>::ToEulerXZY(const Mat3Base<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerXZY(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void Mat3Base<T>::FromEulerYXZ(Mat3Base<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerYXZ( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool Mat3Base<T>::ToEulerYXZ(const Mat3Base<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerYXZ(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void Mat3Base<T>::FromEulerYZX(Mat3Base<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerYZX( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool Mat3Base<T>::ToEulerYZX(const Mat3Base<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerYZX(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void Mat3Base<T>::FromEulerZXY(Mat3Base<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerZXY( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool Mat3Base<T>::ToEulerZXY(const Mat3Base<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerZXY(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void Mat3Base<T>::FromEulerZYX(Mat3Base<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerZYX( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool Mat3Base<T>::ToEulerZYX(const Mat3Base<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerZYX(    m,rfYAngle,rfPAngle,rfRAngle);}
// Not sure at all on why I did these (do they make any sense?):-------------------------------------
template <typename T> void Mat3Base<T>::FromEuler(Mat3Base<T>& m,T fXAngle,T fYAngle,T fZAngle,EulerRotationMode mode)   {__MatTypeFromEuler(m,fXAngle,fYAngle,fZAngle,mode);}
template <typename T> bool Mat3Base<T>::ToEuler(const Mat3Base<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle,EulerRotationMode mode)   {return __MatTypeToEuler(m,rfXAngle,rfYAngle,rfZAngle,mode);}

// MatBase<T> specialization
template <typename T> void MatBase<T>::FromEulerXYZ(MatBase<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerXYZ( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool MatBase<T>::ToEulerXYZ(const MatBase<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerXYZ(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void MatBase<T>::FromEulerXZY(MatBase<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerXZY( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool MatBase<T>::ToEulerXZY(const MatBase<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerXZY(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void MatBase<T>::FromEulerYXZ(MatBase<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerYXZ( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool MatBase<T>::ToEulerYXZ(const MatBase<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerYXZ(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void MatBase<T>::FromEulerYZX(MatBase<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerYZX( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool MatBase<T>::ToEulerYZX(const MatBase<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerYZX(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void MatBase<T>::FromEulerZXY(MatBase<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerZXY( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool MatBase<T>::ToEulerZXY(const MatBase<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerZXY(    m,rfYAngle,rfPAngle,rfRAngle);}
template <typename T> void MatBase<T>::FromEulerZYX(MatBase<T>& m,T fYAngle,T fPAngle,T fRAngle)              {__MatTypeFromEulerZYX( m,fYAngle,fPAngle,fRAngle);}
template <typename T> bool MatBase<T>::ToEulerZYX(const MatBase<T>& m,T& rfYAngle,T& rfPAngle,T& rfRAngle)	{return __MatTypeToEulerZYX(    m,rfYAngle,rfPAngle,rfRAngle);}
// Not sure at all on why I did these (do they make any sense?):-------------------------------------
template <typename T> void MatBase<T>::FromEuler(MatBase<T>& m,T fXAngle,T fYAngle,T fZAngle,EulerRotationMode mode)   {__MatTypeFromEuler(m,fXAngle,fYAngle,fZAngle,mode);}
template <typename T> bool MatBase<T>::ToEuler(const MatBase<T>& m,T& rfXAngle,T& rfYAngle,T& rfZAngle,EulerRotationMode mode)   {return __MatTypeToEuler(m,rfXAngle,rfYAngle,rfZAngle,mode);}


template <typename T> MatBase<T> MatBase<T>::operator*(const MatBase<T>& o) const {
    int i,j,j4;MatBase<T> result16;
    for(i = 0; i < 4; i++) {
        for(j = 0; j < 4; j++) {
        	  j4 = 4*j; result16.m[i+j4] = m[i]*o.m[j4] + m[i+4]*o.m[1+j4] + m[i+8]*o.m[2+j4] + m[i+12]*o.m[3+j4];
        }
    }
    return result16;		
}

template <typename T> MatBase<T> MatBase<T>::invertedFast() const	{
    // It works only for translation + rotation, and only
    // when rotation can be represented by an unit quaternion
    // scaling is discarded
	MatBase<T> o;T* n = o.m;
    const T t[3] = {-m[12],-m[13],-m[14]};T w;
    // Step 1. Transpose the 3x3 submatrix
    n[3]=m[3];n[7]=m[7];n[11]=m[11];n[15]=m[15];
    n[0]=m[0];n[1]=m[4];n[2]=m[8];
    n[4]=m[1];n[5]=m[5];n[6]=m[9];
    n[8]=m[2];n[9]=m[6];n[10]=m[10];
    // Step2. Adjust translation
    n[12]=t[0]*n[0] + t[1]*n[4] +t[2]*n[8];
    n[13]=t[0]*n[1] + t[1]*n[5] +t[2]*n[9];
    n[14]=t[0]*n[2] + t[1]*n[6] +t[2]*n[10];
    w    =t[0]*n[3] + t[1]*n[7] +t[2]*n[11];
    if (w!=0 && w!=1) {n[12]/=w;n[13]/=w;n[14]/=w;} // These last 2 lines are not strictly necessary AFAIK
	return o;
}
template <typename T> MatBase<T> MatBase<T>::LookAt(T eyeX,T eyeY,T eyeZ,T centerX,T centerY,T centerZ,T upX,T upY,T upZ)    {
	MatBase<T> o;T* m = o.m;
    const T eps(0.0001);

    T F[3] = {eyeX-centerX,eyeY-centerY,eyeZ-centerZ};
    T length = F[0]*F[0]+F[1]*F[1]+F[2]*F[2];	// length2 now
    T up[3] = {upX,upY,upZ};

    T S[3] = {up[1]*F[2]-up[2]*F[1],up[2]*F[0]-up[0]*F[2],up[0]*F[1]-up[1]*F[0]};
    T U[3] = {F[1]*S[2]-F[2]*S[1],F[2]*S[0]-F[0]*S[2],F[0]*S[1]-F[1]*S[0]};

    if (length==0) length = eps;
    length = sqrt(length);
    F[0]/=length;F[1]/=length;F[2]/=length;

    length = S[0]*S[0]+S[1]*S[1]+S[2]*S[2];if (length==0) length = eps;
    length = sqrt(length);
    S[0]/=length;S[1]/=length;S[2]/=length;

    length = U[0]*U[0]+U[1]*U[1]+U[2]*U[2];if (length==0) length = eps;
    length = sqrt(length);
    U[0]/=length;U[1]/=length;U[2]/=length;

    /*
                S0	S1	S2  0		1	0	0	-ex
                U0	U1	U2	0   *   0	1	0	-ey
                F0	F1	F2  0		0	0	1	-ez
                0	0	0	1		0	0	0	1

            */
    m[0] = S[0];
    m[1] = U[0];
    m[2] = F[0];
    m[3]= 0;

    m[4] = S[1];
    m[5] = U[1];
    m[6] = F[1];
    m[7]= 0;

    m[8] = S[2];
    m[9] = U[2];
    m[10]= F[2];
    m[11]= 0;

    m[12] = -S[0]*eyeX -S[1]*eyeY -S[2]*eyeZ;
    m[13] = -U[0]*eyeX -U[1]*eyeY -U[2]*eyeZ;
    m[14]= -F[0]*eyeX -F[1]*eyeY -F[2]*eyeZ;
    m[15]= 1;

	return o;
}
template <typename T> MatBase<T> MatBase<T>::Perspective(T degfovy,T aspect, T zNear, T zFar) {
	MatBase<T> o;T* res = o.m;
    const T eps(0.0001);    
    T f = 1.0/tan(degfovy*1.5707963268/180.0); //cotg
    T Dfn = (zFar-zNear);
    if (Dfn==0) {zFar+=eps;zNear-=eps;Dfn=zFar-zNear;}
    if (aspect==0) aspect = 1.0;

    res[0]  = f/aspect;
    res[1]  = 0;
    res[2]  = 0;
    res[3]  = 0;

    res[4]  = 0;
    res[5]  = f;
    res[6]  = 0;
    res[7] = 0;

    res[8]  = 0;
    res[9]  = 0;
    res[10] = -(zFar+zNear)/Dfn;
    res[11] = -1;

    res[12]  = 0;
    res[13]  = 0;
    res[14] = -2.0*zFar*zNear/Dfn;
    res[15] = 0;

	return o;
}
template <typename T> MatBase<T> MatBase<T>::Ortho(T left,T right, T bottom, T top,T nearVal,T farVal) {
	MatBase<T> o;T* res = o.m;
    const T eps(0.0001); 
    T Drl = (right-left);
    T Dtb = (top-bottom);
    T Dfn = (farVal-nearVal);
    if (Drl==0) {right+=eps;left-=eps;Drl=right-left;}
    if (Dtb==0) {top+=eps;bottom-=eps;Dtb=top-bottom;}
    if (Dfn==0) {farVal+=eps;nearVal-=eps;Dfn=farVal-nearVal;}

    res[0]  = 2.0/Drl;
    res[1]  = 0;
    res[2]  = 0;
    res[3] = 0;

    res[4]  = 0;
    res[5]  = 2.0/Dtb;
    res[6]  = 0;
    res[7] = 0;

    res[8]  = 0;
    res[9]  = 0;
    res[10] = -2.0/Dfn;
    res[11] = 0;

    res[12]  = -(right+left)/Drl;
    res[13]  = -(top+bottom)/Dtb;
    res[14] = (farVal+nearVal)/Dfn;
    res[15] = 1;

	return o;
}
template <typename T> MatBase<T>& MatBase<T>::translate(T x,T y,T z)  {
    const MatBase<T> mt(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        x, y, z, 1);
	*this = this->operator*(mt);
    return *this;
}
template <typename T> MatBase<T>& MatBase<T>::rotate(T degAngle,T x,T y,T z)  {
    const T angle = degAngle*M_PIOVER180;
    const T c = cos(angle);
    const T s = sin(angle);
    T len = x*x+y*y+z*z;
    if (len<0.9999 || len>1.0001) {len=sqrt(len);x/=len;y/=len;z/=len;}
    const MatBase<T> mr(
        c + x*x*(1-c),  y*x*(1-c)+z*s,    z*x*(1-c)-y*s,    0,
        x*y*(1-c) - z*s,  c + y*y*(1-c),      z*y*(1-c) + x*s,    0,
        x*z*(1-c) + y*s,  y*z*(1-c) - x*s,    c + z*z*(1-c),      0,
        0,              0,                  0,                  1);
	*this = this->operator*(mr);
    return *this; 
}
template <typename T> MatBase<T>& MatBase<T>::scale(T x,T y,T z)  {
    const MatBase<T> ms(x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1);
	*this = this->operator*(ms);
    return *this;
}
template <typename T> int MatBase<T>::InvertMatrix(MatBase<T>& dst, const MatBase<T>& src)	{
	T* n = dst.m;
    const T* m = src.m;

    T m00 = m[0],  m10 = m[1],  m20 = m[2],  m30 = m[3];
    T m01 = m[4],  m11 = m[5],  m21 = m[6],  m31 = m[7];
    T m02 = m[8],  m12 = m[9],  m22 = m[10], m32 = m[11];
    T m03 = m[12], m13 = m[13], m23 = m[14], m33 = m[15];

    T v0 = m20 * m31 - m21 * m30;
    T v1 = m20 * m32 - m22 * m30;
    T v2 = m20 * m33 - m23 * m30;
    T v3 = m21 * m32 - m22 * m31;
    T v4 = m21 * m33 - m23 * m31;
    T v5 = m22 * m33 - m23 * m32;

    T t00 = + (v5 * m11 - v4 * m12 + v3 * m13);
    T t10 = - (v5 * m10 - v2 * m12 + v1 * m13);
    T t20 = + (v4 * m10 - v2 * m11 + v0 * m13);
    T t30 = - (v3 * m10 - v1 * m11 + v0 * m12);

    T det = (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);
    if (det==0) return 0;
    {
        T invDet = T(1.0) / det;

        T d00 = t00 * invDet;
        T d10 = t10 * invDet;
        T d20 = t20 * invDet;
        T d30 = t30 * invDet;

        T d01 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
        T d11 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
        T d21 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
        T d31 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

        v0 = m10 * m31 - m11 * m30;
        v1 = m10 * m32 - m12 * m30;
        v2 = m10 * m33 - m13 * m30;
        v3 = m11 * m32 - m12 * m31;
        v4 = m11 * m33 - m13 * m31;
        v5 = m12 * m33 - m13 * m32;
        {
            T d02 = + (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
            T d12 = - (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
            T d22 = + (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
            T d32 = - (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

            v0 = m21 * m10 - m20 * m11;
            v1 = m22 * m10 - m20 * m12;
            v2 = m23 * m10 - m20 * m13;
            v3 = m22 * m11 - m21 * m12;
            v4 = m23 * m11 - m21 * m13;
            v5 = m23 * m12 - m22 * m13;
            {
                T d03 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
                T d13 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
                T d23 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
                T d33 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

                n[0] =d00; n[1] =d10; n[2] =d20; n[3] =d30;
                n[4] =d01; n[5] =d11; n[6] =d21; n[7] =d31;
                n[8] =d02; n[9] =d12; n[10]=d22; n[11]=d32;
                n[12]=d03; n[13]=d13; n[14]=d23; n[15]=d33;
            }
        }
    }
    return 1;
}
template <typename T> QuatBase<T> MatBase<T>::getQuaternionFromMatrixRotation() const {
    QuatBase<T> q;
#if 0 // incorrect
	T m00=m[0], m10=m[1], m20=m[2];
    T m01=m[4], m11=m[5], m21=m[6];
    T m02=m[8], m12=m[9], m22=m[10];
    /*T m00=m[0], m10=m[4], m20=m[8];
    T m01=m[1], m11=m[5], m21=m[9];
    T m02=m[2], m12=m[6], m22=m[10]; */
    T trace = m00 + m11 + m22;
    if (trace > 0) {
        T s = sqrt(trace + 1.0);
        q[3]=(s * 0.5);s = 0.5 / s;
        q[0]=((m12 - m21) * s);
        q[1]=((m20 - m02) * s);
        q[2]=((m01 - m10) * s);
    }
    else {
        // i,j,k are all 0,1 or 2
        int i = m00 < m11 ?
            (m11 < m22 ? 2 : 1) :
            (m00 < m22 ? 2 : 0);
        int j = (i + 1) % 3;
        int k = (i + 2) % 3;
        /*T s = sqrt(m[i][i] - m[j][j] - m[k][k] + 1.0f);
        q[i] = s * 0.5;s = 0.5 / s;
        q[3] = (m[j][k] - m[k][j]) * s;
        q[j] = (m[i][j] + m[j][i]) * s;
        q[k] = (m[i][k] + m[k][i]) * s;*/
        // >>> m[i][j] = m[j*4+i] because m[2][1] = m21 = m[6]
        int i4 = 4*i, j4 = 4*j, k4 = 4*k;
        T s = sqrt(m[i4+i] - m[j4+j] - m[k4+k] + 1.0);
        q[i] = s * 0.5;s = 0.5 / s;
        q[3] = (m[k4+j] - m[j4+k]) * s;
        q[j] = (m[j4+i] + m[i4+j]) * s;
        q[k] = (m[k4+i] + m[i4+k]) * s;
        /*T s = sqrt(m[i4+i] - m[j4+j] - m[k4+k] + 1.0);
        q[i] = s * 0.5;s = 0.5 / s;
        q[3] = (m[k+j4] - m[j+k4]) * s;
        q[j] = (m[j+i4] + m[i+j4]) * s;
        q[k] = (m[k+i4] + m[i+k4]) * s;*/
    }
#else // glm based
        T fourXSquaredMinus1 = col[0][0] - col[1][1] - col[2][2];
        T fourYSquaredMinus1 = col[1][1] - col[0][0] - col[2][2];
        T fourZSquaredMinus1 = col[2][2] - col[0][0] - col[1][1];
        T fourWSquaredMinus1 = col[0][0] + col[1][1] + col[2][2];

        int biggestIndex = 0;
        T fourBiggestSquaredMinus1 = fourWSquaredMinus1;
        if(fourXSquaredMinus1 > fourBiggestSquaredMinus1)
        {
            fourBiggestSquaredMinus1 = fourXSquaredMinus1;
            biggestIndex = 1;
        }
        if(fourYSquaredMinus1 > fourBiggestSquaredMinus1)
        {
            fourBiggestSquaredMinus1 = fourYSquaredMinus1;
            biggestIndex = 2;
        }
        if(fourZSquaredMinus1 > fourBiggestSquaredMinus1)
        {
            fourBiggestSquaredMinus1 = fourZSquaredMinus1;
            biggestIndex = 3;
        }

        T biggestVal = sqrt(fourBiggestSquaredMinus1 + T(1)) * T(0.5);
        T mult = static_cast<T>(0.25) / biggestVal;

        switch(biggestIndex)
        {
        case 0:
            q.w = biggestVal;
            q.x = (col[1][2] - col[2][1]) * mult;
            q.y = (col[2][0] - col[0][2]) * mult;
            q.z = (col[0][1] - col[1][0]) * mult;
            break;
        case 1:
            q.w = (col[1][2] - col[2][1]) * mult;
            q.x = biggestVal;
            q.y = (col[0][1] + col[1][0]) * mult;
            q.z = (col[2][0] + col[0][2]) * mult;
            break;
        case 2:
            q.w = (col[2][0] - col[0][2]) * mult;
            q.x = (col[0][1] + col[1][0]) * mult;
            q.y = biggestVal;
            q.z = (col[1][2] + col[2][1]) * mult;
            break;
        case 3:
            q.w = (col[0][1] - col[1][0]) * mult;
            q.x = (col[2][0] + col[0][2]) * mult;
            q.y = (col[1][2] + col[2][1]) * mult;
            q.z = biggestVal;
            break;

        default:					// Silence a -Wswitch-default warning in GCC. Should never actually get here. Assert is just for sanity.
            //assert(false);
            q.x=q.y=q.z=T(0);q.w=T(1);
            break;
        }
#endif
    return q;
}
template <typename T> void MatBase<T>::setQuaternionToMatrixRotation(const QuatBase<T>& q) {
#if 0 // incorrect
    T d = q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
    if (d == 0.0) {m[0]=m[5]=m[10]=1;m[1]=m[2]=m[4]=m[6]=m[8]=m[9]=0;return;}
    {
    T s = 2.0 / d;
    T xs = q[0] * s,   ys = q[1] * s,   zs = q[2] * s;
    T wx = q[3] * xs,  wy = q[3] * ys,  wz = q[3] * zs;
    T xx = q[0] * xs,  xy = q[0] * ys,  xz = q[0] * zs;
    T yy = q[1] * ys,  yz = q[1] * zs,  zz = q[2] * zs;

    m[0] = 1.0 - (yy + zz); m[1] = xy - wz;           m[2] = xz + wy;
    m[4] = xy + wz;          m[5] = 1.0 - (xx + zz);  m[6] = yz - wx;
    m[8] = xz - wy;          m[9] = yz + wx;           m[10] = 1.0 - (xx + yy);

    /*m[0] = 1.0 - (yy + zz); m[4] = xy - wz;           m[8] = xz + wy;
    m[1] = xy + wz;          m[5] = 1.0f - (xx + zz);  m[9] = yz - wx;
    m[2] = xz - wy;          m[6] = yz + wx;           m[10] = 1.0 - (xx + yy); */
    }
#else // glm based
        T qxx(q.x * q.x);
        T qyy(q.y * q.y);
        T qzz(q.z * q.z);
        T qxz(q.x * q.z);
        T qxy(q.x * q.y);
        T qyz(q.y * q.z);
        T qwx(q.w * q.x);
        T qwy(q.w * q.y);
        T qwz(q.w * q.z);

        col[0][0] = 1 - 2 * (qyy +  qzz);
        col[0][1] = 2 * (qxy + qwz);
        col[0][2] = 2 * (qxz - qwy);

        col[1][0] = 2 * (qxy - qwz);
        col[1][1] = 1 - 2 * (qxx +  qzz);
        col[1][2] = 2 * (qyz + qwx);

        col[2][0] = 2 * (qxz + qwy);
        col[2][1] = 2 * (qyz - qwx);
        col[2][2] = 1 - 2 * (qxx +  qyy);
#endif
}
template <typename T> void MatBase<T>::SlerpMatrix(MatBase<T>& o ,const MatBase<T>& mStart,const MatBase<T>& mEnd,T slerpTime_In_0_1)    {
	const T* mStart16 = mStart.m;   
	const T* mEnd16 = mEnd.m;   
	T* mOut16 = o.m;   
	if (slerpTime_In_0_1<=0)        {if (mOut16!=mStart16) o=mStart;return;}
    else if (slerpTime_In_0_1>=1)   {if (mOut16!=mEnd16)   o=mEnd; return;}


    {
        if (1)  {
            // Lerp positions
            const T T1[3] = {mStart16[12],mStart16[13],mStart16[14]};
            const T T2[3] = {mEnd16[12],mEnd16[13],mEnd16[14]};
            const T t = slerpTime_In_0_1;
            const T one_minus_t = 1.0-t;

            //mOut16[12] = T1[0]*one_minus_t + T2[0]*t;
            //mOut16[13] = T1[1]*one_minus_t + T2[1]*t;
            //mOut16[14] = T1[2]*one_minus_t + T2[2]*t;
            int i;for (i=0;i<3;i++) mOut16[12+i] = T1[i]*one_minus_t + T2[i]*t;
        }
        if (1)  {
            // Slerp rotations
            QuatBase<T> qStart = mStart.getQuaternionFromMatrixRotation();
            QuatBase<T> qEnd = mEnd.getQuaternionFromMatrixRotation();
            o.setMatrixRotationFromQuaternion(qStart.slerp(qEnd,slerpTime_In_0_1));
        }

    }
}
template <typename T> void MatBase<T>::invertXZAxis() {
    m[0]=-m[0]; m[8] =-m[8];
    m[1]=-m[1]; m[9] =-m[9];
    m[2]=-m[2]; m[10]=-m[10];

    /*m[0]=-m[0]; m[2]=-m[2];
    m[4]=-m[4]; m[6]=-m[6];
    m[8]=-m[8]; m[10]=-m[10];*/
}
/*template <typename T> Vec3Base<T> MatBase<T>::mulPos(T posX,T posY,T posZ) const {
    T w;
	Vec3Base<T> posOut3(
	    posX*m[0] + posY*m[4] + posZ*m[8] + m[12],
    	posX*m[1] + posY*m[5] + posZ*m[9] + m[13],
    	posX*m[2] + posY*m[6] + posZ*m[10]+ m[14]
	);
    w = posX*m[3] + posY*m[7] + posZ*m[11]+ m[15];
    if (w!=0 && w!=1) {posOut3[0]/=w;posOut3[1]/=w;posOut3[2]/=w;}
	return posOut3;
}*/
template <typename T> void MatBase<T>::FromRotScaTra(MatBase<T>& m,const Mat3Base<T>& rot,const Vec3Base<T>& sca,const Vec3Base<T>& tra) {
    for (int i=0;i<3;i++) for (int j=0;j<3;j++) m.col[i][j]=rot.col[i][j];
    for (int i=0;i<3;i++) m.col[i][3] = 0;
    for (int j=0;j<3;j++) m.col[3][j] = 0;  //translation[j];
    m.col[3][3] = 1;
    m.scale(sca.x,sca.y,sca.z);

    for (int j=0;j<3;j++) m.col[3][j] = tra[j];
}
    //---------------------------------------------------------------------------------------------------
template <typename T> void MatBase<T>::LookAtYX(MatBase<T>& m,const Vec3Base<T>& lookAt,T minDistance,T maxDistance,T pitchLimit)	{

        Vec3Base<T> D=lookAt-Vec3Base<T>(m.col[3][0],m.col[3][1],m.col[3][2]);
        T Dxz2=D.x*D.x+D.z*D.z;
        T Dxz=::sqrt(Dxz2);
        /*
        T AY= ::atan2(D.x,D.z);
        T AX=-::atan2(D.y,Dxz);
        FromEulerYXZ(mat3_cast_in_place(T),AY,AX,0);
        */
        //FromEulerYXZ(mat3_cast_in_place(T),::atan2(D.x,D.z),-::atan2(D.y,Dxz),0);

        T pitch = -::atan2(D.y,Dxz);
        if (pitch<-pitchLimit) pitch = -pitchLimit;
        else if (pitch>pitchLimit) pitch = pitchLimit;
        FromEulerYXZ(m,::atan2(D.x,D.z),pitch,0);

        if (minDistance<maxDistance)	{
            T distance=::sqrt(Dxz2+D.y*D.y);
            Vec3Base<T> zAxis(m.col[2][0],m.col[2][1],m.col[2][2]),origin(m.col[3][0],m.col[3][1],m.col[3][2]),tmp;// T.getBasis().getColumn(2)
            if (distance<minDistance)       tmp = origin-zAxis*(minDistance-distance);
            else if (distance>maxDistance)  tmp = origin+zAxis*(distance-maxDistance);
            m.col[3][0] = tmp.x;
            m.col[3][1] = tmp.y;
            m.col[3][2] = tmp.z;
        }
}
// End Implementation -----------------------------------------------------------------------------


} // nmespace MATH_HELPER_NAMESPACE_NAME




#endif //_MATH_HELPER_HPP_

