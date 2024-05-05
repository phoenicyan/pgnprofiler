#pragma once

#include <cstddef>            // for std::size_t
#include <memory>             // for std::auto_ptr
#include <algorithm>          // for std::swap
#include <functional>         // for std::less
#include <cassert>

//#include "windows.h"

namespace myboost {

//  shared_in_base  ----------------------------------------------------------//

//  This base class meets the requirements for shared_in_ptr T, and keeps the
//  count private to prevent abuse.  Note that shared_in_ptr does not require
//  inheriting from shared_in_base<> as long as shared_in_ptr<> requirements are
//  met (presumably by simply providing a member named shared_in_count
//  initialized to 0).

//  Requirements on RefCounter: must meet shared_in_ptr requirements.
//  For example, int or long.

template<typename RefCounter = long>
class shared_in_base
{
private:
	volatile RefCounter shared_in_count;

#if defined(BOOST_NO_MEMBER_TEMPLATES) \
    || defined(BOOST_NO_MEMBER_TEMPLATE_FRIENDS)
public:
#else
    template<typename U> friend class shared_in_ptr;
#endif
	

public:
	shared_in_base() : shared_in_count(0) {}

	inline RefCounter _shared_addref()
	{
		return InterlockedIncrement(&shared_in_count);
	}

	inline RefCounter _shared_release()
	{
		return InterlockedDecrement(&shared_in_count);
	}

	inline void _shared_set(RefCounter aValue)
	{
		InterlockedExchange(&shared_in_count, aValue);
	}

};

//  shared_in_ptr  -------------------------------------------------------------
//
//  Requirements: For px of type T*, px->shared_in_count must be initialized to 0
//  and the following expressions must be well-formed and not throw exceptions.
//
//  Expression             Type       Effects
//  ==========             ========   =======
//  ++px->shared_in_count     ---      increments px->shared_in_count by one.
//  --px->shared_in_count   integral   decrements px->shared_in_count by one,
//                         type       returns px->shared_in_count.
//  px->shared_in_count     integral   returns px->shared_in_count.
//                         type


template<typename T> class shared_in_ptr 
{
public:
	typedef T element_type;

	explicit shared_in_ptr(T* p =0) : px(p) 
	{ 
		if (px) 
			px->_shared_addref(); 
	}

	shared_in_ptr(const shared_in_ptr& r) : px(r.px) 
	{ 
		if (px) 
			px->_shared_addref(); 
	}

	~shared_in_ptr() 
	{ 
		dispose(); 
	}

	shared_in_ptr& operator=(const shared_in_ptr& r) 
	{
		share(r.px);
		return *this;
	}

	bool operator <( const shared_in_ptr & other ) const 
	{ 
		return *px < *other.px; 
	}

#if !defined( BOOST_NO_MEMBER_TEMPLATES )
	template<typename Y>
	shared_in_ptr(const shared_in_ptr<Y>& r) : px(r.px) 
	{ 
		if (px) 
			px->_shared_addref();
	}

	template<typename Y>
	shared_in_ptr(std::unique_ptr<Y>& r) 
	{
		px = r.release();
		if (px) 
			px->_shared_set(1);
	}

	template<typename Y>
	shared_in_ptr& operator=(const shared_in_ptr<Y>& r) 
	{
		share(r.px);
		return *this;
	}

	template<typename Y>
	shared_in_ptr& operator=(std::unique_ptr<Y>& r) 
	{
		dispose();
		px = r.release();
		if (px) 
			px->_shared_set(1);
		return *this;
	}
#endif

	void reset(T* p=0) 
	{
		if ( px == p ) 
			return;
		dispose();
		px = p;
		if (px) 
			px->_shared_addref();
	} // reset

	T& operator*() const throw()  { return *px; }
	T* operator->() const throw() { return px; }
	T* get() const throw()        { return px; }
 #ifdef BOOST_SMART_PTR_CONVERSION
	// get() is safer! Define BOOST_SMART_PTR_CONVERSION at your own risk!
	operator T*() const throw()   { return px; }
 #endif

	//long use_count() const        { return px ? px->shared_in_count : 0; }
	//bool unique() const throw()   { return px && px->shared_in_count == 1; }

	void swap(shared_in_ptr<T>& other) throw()
	{
		std::swap(px,other.px); 
	}

// Tasteless as this may seem, making all members public allows member templates
// to work in the absence of member template friends. (Matthew Langston)
#if defined(BOOST_NO_MEMBER_TEMPLATES) \
    || !defined( BOOST_NO_MEMBER_TEMPLATE_FRIENDS )
   private:
#endif

   T*     px;     // contained pointer

#if !defined( BOOST_NO_MEMBER_TEMPLATES ) \
    && !defined( BOOST_NO_MEMBER_TEMPLATE_FRIENDS )
	template<typename Y> friend class shared_in_ptr;
#endif

	void dispose() 
	{ 
		if (px && px->_shared_release() == 0) 
		{ 
			delete px; 
		} 
	}

	void share(T* rpx) 
	{
		if (px != rpx) 
		{
			dispose();
			px = rpx;
			if (px) 
				px->_shared_addref();
		}
	} // share
};  // shared_in_ptr

template<typename T, typename U>
inline bool operator==(const shared_in_ptr<T>& a, const shared_in_ptr<U>& b)
{ 
	return a.get() == b.get(); 
}

template<typename T, typename U>
inline bool operator!=(const shared_in_ptr<T>& a, const shared_in_ptr<U>& b)
{ 
	return a.get() != b.get(); 
}


} // namespace boost
