//  (C) Copyright Greg Colvin, Beman Dawes, and Dave Abrahams 1998-2000.
//  Permission to copy, use, modify, sell and distribute this software
//  is granted provided this copyright notice appears in all copies. This
// software is provided "as is" without express or implied warranty, and
// with no claim as to its suitability for any purpose.


#ifndef _fast_shared_ptr_hpp
#define _fast_shared_ptr_hpp


#include <boost/config.hpp>
#include <memory>


namespace fast_shared
{

// common base class for allocating/deallocating counters
struct count_alloc
{
    union count { count* next; long n; };

    // stupid, slow implementation just to keep this all in a header file
    // for real speed turn this into a static data member.
    static count*& free_list() { static count* p = 0; return p; }

    // allocate a long initialized to the value 1
    static long* new_long()
    {
        count*& fl = free_list();
        if (!fl) {  // free list empty?
            // allocate a block of 1000 counts
            count *pc = new count[1000];
            fl = pc;
            // link them into a chain
            for (count* last = pc + 1000 - 1; pc != last; ++pc)
                pc->next = pc + 1;
            pc->next = 0; // end of the line
        }
        long* pn = &fl->n;
        fl = fl->next;
        *pn = 1;
        return pn;
    }

    // return the given long to the free list
    static void delete_pn(long* pn)
    {
        count* pc = reinterpret_cast<count*>(pn);
        count*& fl = free_list();
        pc->next = fl;
        fl = pc;
    }
};

template<typename T> class shared_ptr : count_alloc {
  public:
   typedef T element_type;

   explicit shared_ptr(T* p =0) : px(p) {
      try { pn = new_long(); }  // fix: prevent leak if new throws
      catch (...) { delete p; throw; }
   }

   shared_ptr(const shared_ptr& r) throw() : px(r.px) { ++*(pn = r.pn); }

   ~shared_ptr() { dispose(); }

   shared_ptr& operator=(const shared_ptr& r) {
      if (pn != r.pn) {
         if (--*pn == 0) { delete px; delete_pn(pn); }
//         dispose();
         px = r.px;
         ++*(pn = r.pn);
      }
//      share(r.px,r.pn);
      return *this;
   }

   bool operator <( const shared_ptr & other ) const { return *px < *other.px; }

#if !defined( BOOST_NO_MEMBER_TEMPLATES )
   template<typename Y>
      shared_ptr(const shared_ptr<Y>& r) throw() : px(r.px) {
         ++*(pn = r.pn);
      }

   template<typename Y>
      shared_ptr(std::auto_ptr<Y>& r) {
         pn = new_long(); // may throw
         px = r.release(); // fix: moved here to stop leak if new throws
      }

   template<typename Y>
      shared_ptr& operator=(const shared_ptr<Y>& r) {
         share(r.px,r.pn);
         return *this;
      }

   template<typename Y>
      shared_ptr& operator=(std::auto_ptr<Y>& r) {
         // code choice driven by guarantee of "no effect if new throws"
         if (*pn == 1) { delete px; }
         else { // allocate new reference counter
           long * tmp = new_long(); // may throw
           --*pn; // only decrement once danger of new throwing is past
           pn = tmp;
         } // allocate new reference counter
         px = r.release(); // fix: moved here so doesn't leak if new throws
         return *this;
      }
#endif

   void reset(T* p=0) {
      if ( px == p ) return;  // fix: self-assignment safe
      if (--*pn == 0) { delete px; }
      else { // allocate new reference counter
        try { pn = new_long(); }  // fix: prevent leak if new throws
        catch (...) {
          ++*pn;  // undo effect of --*pn above to meet effects guarantee
          delete p;
          throw;
        } // catch
      } // allocate new reference counter
      *pn = 1;
      px = p;
   } // reset

   T& operator*() const throw()  { return *px; }
   T* operator->() const throw() { return px; }
   T* get() const throw()        { return px; }
 #ifdef BOOST_SMART_PTR_CONVERSION
   // get() is safer! Define BOOST_SMART_PTR_CONVERSION at your own risk!
   operator T*() const throw()   { return px; }
 #endif

   long use_count() const throw(){ return *pn; }
   bool unique() const throw()   { return *pn == 1; }

   void swap(shared_ptr<T>& other) throw()
     { std::swap(px,other.px); std::swap(pn,other.pn); }

// Tasteless as this may seem, making all members public allows member templates
// to work in the absence of member template friends. (Matthew Langston)
#if defined(BOOST_NO_MEMBER_TEMPLATES) \
    || !defined( BOOST_NO_MEMBER_TEMPLATE_FRIENDS )
   private:
#endif

   T*     px;     // contained pointer
   long*  pn;     // ptr to reference counter

#if !defined( BOOST_NO_MEMBER_TEMPLATES ) \
    && !defined( BOOST_NO_MEMBER_TEMPLATE_FRIENDS )
   template<typename Y> friend class shared_ptr;
#endif

   void dispose() { if (--*pn == 0) { delete px; delete_pn(pn); } }

   void share(T* rpx, long* rpn) {
      if (pn != rpn) {
         dispose();
         px = rpx;
         ++*(pn = rpn);
      }
   } // share
};  // shared_ptr

template<typename T, typename U>
  inline bool operator==(const shared_ptr<T>& a, const shared_ptr<U>& b)
    { return a.get() == b.get(); }

template<typename T, typename U>
  inline bool operator!=(const shared_ptr<T>& a, const shared_ptr<U>& b)
    { return a.get() != b.get(); }


}


#endif