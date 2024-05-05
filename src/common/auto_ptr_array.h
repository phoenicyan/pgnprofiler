/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#ifndef _THROW0()
#define _THROW0() noexcept
#endif

template<class _Ty>
class auto_ptr_array 
{
public:

	typedef _Ty element_type;

	explicit auto_ptr_array(_Ty *_Ptr = 0) _THROW0()
		: _Myptr(_Ptr)
	{	// construct from object pointer
	}

	auto_ptr_array(auto_ptr_array<_Ty>& _Right) _THROW0()
		: _Myptr(_Right.release())
	{	// construct by assuming pointer from _Right auto_ptr
	}

	template<class _Other>
		operator auto_ptr_array<_Other>() _THROW0()
	{	// convert to compatible auto_ptr
		return (auto_ptr_array<_Other>(*this));
	}

	template<class _Other>
		auto_ptr_array<_Ty>& operator=(auto_ptr_array<_Other>& _Right) _THROW0()
	{	// assign compatible _Right (assume pointer)
		reset(_Right.release());
		return (*this);
	}

	template<class _Other>
		auto_ptr_array(auto_ptr_array<_Other>& _Right) _THROW0()
		: _Myptr(_Right.release())
	{	// construct by assuming pointer from _Right
	}

	auto_ptr_array<_Ty>& operator=(auto_ptr_array<_Ty>& _Right) _THROW0()
	{	// assign compatible _Right (assume pointer)
		reset(_Right.release());
		return (*this);
	}

	~auto_ptr_array()
	{	// destroy the object
		delete [] _Myptr;
	}

	inline _Ty& operator*() const _THROW0()
	{	// return designated value
		return (*_Myptr);
	}

	inline _Ty *operator->() const _THROW0()
	{	// return pointer to class object
		return (&**this);
	}

	inline _Ty *get() const _THROW0()
	{	// return wrapped pointer
		return (_Myptr);
	}

	inline _Ty *release() _THROW0()
	{	// return wrapped pointer and give up ownership
		_Ty *_Tmp = _Myptr;
		_Myptr = 0;
		return (_Tmp);
	}

	inline void reset(_Ty* _Ptr = 0)
	{	// destroy designated object and store new pointer
		if (_Ptr != _Myptr)
			delete [] _Myptr;
		_Myptr = _Ptr;
	}

private:
	_Ty *_Myptr;	// the wrapped object pointer
};

