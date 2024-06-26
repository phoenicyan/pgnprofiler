#ifndef RETHREAD_DETAIL_INTRUSIVE_LIST_HPP
#define RETHREAD_DETAIL_INTRUSIVE_LIST_HPP

// Copyright (c) 2016, Boris Sazonov
//
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted,
// provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "detail/iterator_base.hpp"
#include "detail/utility.hpp"

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace rethread {
namespace detail
{

	template <typename T_>
	class intrusive_list;


	template <typename T_, bool IsMutable_>
	class intrusive_list_iterator;


	template <bool IsMutable_>
	class intrusive_list_node
	{
		template <typename T_>
		friend class intrusive_list;

		template <typename T_, bool>
		friend class intrusive_list_iterator;

		using node_type = intrusive_list_node<IsMutable_>;
		using node_ptr = typename std::conditional<IsMutable_, const node_type*, node_type*>::type;

	private:
		mutable node_ptr _prev;
		mutable node_ptr _next;

	protected:
		intrusive_list_node() : _prev(this), _next(this)
		{ }

		~intrusive_list_node()
		{ RETHREAD_ASSERT(!is_linked(), "Node is linked!"); }

		intrusive_list_node(const intrusive_list_node&) = delete;
		intrusive_list_node& operator =(const intrusive_list_node&) = delete;

	private:
		void insert_before(node_type& other)
		{
			_prev = other._prev;
			_next = &other;

			_next->_prev = this;
			_prev->_next = this;
		}

		void unlink()
		{
			_next->_prev = _prev;
			_prev->_next = _next;
			_prev = _next = this;
		}

		void insert_before(node_type& other) const
		{
			_prev = other._prev;
			_next = &other;

			_next->_prev = this;
			_prev->_next = this;
		}

		void unlink() const
		{
			_next->_prev = _prev;
			_prev->_next = _next;
			_prev = _next = this;
		}

		bool is_linked() const
		{ return _next != this || _prev != this; }
	};


	template <typename T_, bool IsMutable_>
	class intrusive_list_iterator : public iterator_base<intrusive_list_iterator<T_, IsMutable_>, std::bidirectional_iterator_tag, T_>
	{
		using node_type = intrusive_list_node<IsMutable_>;
		using node_ptr = typename std::conditional<IsMutable_, const node_type*, node_type*>::type;

		node_ptr _node;

	public:
		explicit intrusive_list_iterator(node_ptr node = nullptr) : _node(node)
		{ }

		T_& dereference() const
		{ return static_cast<T_&>(*_node); }

		bool equal(intrusive_list_iterator<T_, IsMutable_> other) const
		{ return _node == other._node; }

		void increment()
		{ _node = _node->_next; }

		void decrement()
		{ _node = _node->_prev; }
	};


	template <typename T_>
	class intrusive_list
	{
		static RETHREAD_CONSTEXPR bool IsMutable = std::is_const<T_>::value;
		static_assert(std::is_base_of<intrusive_list_node<IsMutable>, T_>::value, "intrusive_list_node should be a base of T_");

	public:
		using iterator = intrusive_list_iterator<T_, IsMutable>;
		using const_iterator = intrusive_list_iterator<const T_, IsMutable>;

	private:
		intrusive_list_node<IsMutable> _root;

	public:
		iterator begin()
		{ return iterator(_root._next); }

		iterator end()
		{ return iterator(&_root); }

		const_iterator begin() const
		{ return const_iterator(_root._next); }

		const_iterator end() const
		{ return const_iterator(&_root); }

		bool empty() const
		{ return !_root.is_linked(); }

		size_t size() const
		{ return std::distance(begin(), end()); }

		void push_back(T_& node)
		{ node.insert_before(_root); }

		void erase(T_& node)
		{ node.unlink(); }
	};

}}

#endif
