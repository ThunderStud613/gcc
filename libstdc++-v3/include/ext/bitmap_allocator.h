// Bitmap Allocator. -*- C++ -*-

// Copyright (C) 2004 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

/** @file ext/bitmap_allocator.h
 *  This file is a GNU extension to the Standard C++ Library.
 *  You should only include this header if you are using GCC 3 or later.
 */

#ifndef _BITMAP_ALLOCATOR_H
#define _BITMAP_ALLOCATOR_H 1

// For std::size_t, and ptrdiff_t.
#include <cstddef>

// For std::pair.
#include <utility>

// For greater_equal, and less_equal.
#include <functional>

// For operator new.
#include <new>

// For __gthread_mutex_t, __gthread_mutex_lock and __gthread_mutex_unlock.
#include <bits/gthr.h>

// Define this to enable error checking withing the allocator
// itself(to debug the allocator itself).
//#define _BALLOC_SANITY_CHECK

#if defined _BALLOC_SANITY_CHECK
#include <cassert>
#define _BALLOC_ASSERT(_EXPR) assert(_EXPR)
#else
#define _BALLOC_ASSERT(_EXPR)
#endif


namespace __gnu_cxx
{
#if defined __GTHREADS
  namespace
  {
    // If true, then the application being compiled will be using
    // threads, so use mutexes as a synchronization primitive, else do
    // no use any synchronization primitives.
    bool const __threads_enabled = __gthread_active_p();
  }
#endif

#if defined __GTHREADS
  // _Mutex is an OO-Wrapper for __gthread_mutex_t. It does not allow
  // you to copy or assign an already initialized mutex. This is used
  // merely as a convenience for the locking classes.
  class _Mutex 
  {
    __gthread_mutex_t _M_mut;

    // Prevent Copying and assignment.
    _Mutex(_Mutex const&);
    _Mutex& operator=(_Mutex const&);

  public:
    _Mutex()
    {
      if (__threads_enabled)
	{
#if !defined __GTHREAD_MUTEX_INIT
	  __GTHREAD_MUTEX_INIT_FUNCTION(&_M_mut);
#else
	  __gthread_mutex_t __mtemp = __GTHREAD_MUTEX_INIT;
	  _M_mut = __mtemp;
#endif
	}
    }

    ~_Mutex()
    {
      // Gthreads does not define a Mutex Destruction Function.
    }

    __gthread_mutex_t*
    _M_get() { return &_M_mut; }
  };

  // _Lock is a simple manual lokcing class which allows you to
  // manually lock and unlock a mutex associated with the lock. There
  // is not automatic locking or unlocking happening without the
  // programmer's explicit instructions. This class unlocks the mutex
  // ONLY if it has not been locked. However, this check does not
  // apply for lokcing, and wayward use may cause dead-locks.
  class _Lock 
  {
    _Mutex* _M_pmt;
    bool _M_locked;

    // Prevent Copying and assignment.
    _Lock(_Lock const&);
    _Lock& operator=(_Lock const&);

  public:
    _Lock(_Mutex* __mptr)
    : _M_pmt(__mptr), _M_locked(false)
    { }

    void
    _M_lock()
    {
      if (__threads_enabled)
	{
	  _M_locked = true;
	  __gthread_mutex_lock(_M_pmt->_M_get());
	}
    }

    void
    _M_unlock()
    {
      if (__threads_enabled)
	{
	  if (__builtin_expect(_M_locked, true))
	    {
	      __gthread_mutex_unlock(_M_pmt->_M_get());
	      _M_locked = false;
	    }
	}
    }
    
    ~_Lock() { }
  };

  // _Auto_Lock locks the associated mutex on construction, and
  // unlocks on it's destruction. There are no checks performed, and
  // this calss follows the RAII principle.
  class _Auto_Lock 
  {
    _Mutex* _M_pmt;
    // Prevent Copying and assignment.
    _Auto_Lock(_Auto_Lock const&);
    _Auto_Lock& operator=(_Auto_Lock const&);

    void
    _M_lock()
    {
      if (__threads_enabled)
	__gthread_mutex_lock(_M_pmt->_M_get());
    }

    void
    _M_unlock()
    {
      if (__threads_enabled)
	__gthread_mutex_unlock(_M_pmt->_M_get());
    }

  public:
    _Auto_Lock(_Mutex* __mptr) : _M_pmt(__mptr)
    { this->_M_lock(); }

    ~_Auto_Lock() { this->_M_unlock(); }
  };
#endif 

  namespace balloc
  {
    // __mini_vector<> is to be used only for built-in types or
    // PODs. It is a stripped down version of the full-fledged
    // std::vector<>. Noteable differences are: 
    // 
    // 1. Not all accessor functions are present.
    // 2. Used ONLY for PODs.
    // 3. No Allocator template argument. Uses ::operator new() to get
    // memory, and ::operator delete() to free it.
    template<typename _Tp>
      class __mini_vector
      {
	__mini_vector(const __mini_vector&);
	__mini_vector& operator=(const __mini_vector&);

      public:
	typedef _Tp value_type;
	typedef _Tp* pointer;
	typedef _Tp& reference;
	typedef const _Tp& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
	typedef pointer iterator;

      private:
	pointer _M_start;
	pointer _M_finish;
	pointer _M_end_of_storage;

	size_type
	_M_space_left() const throw()
	{ return _M_end_of_storage - _M_finish; }

	pointer
	allocate(size_type __n)
	{ return static_cast<pointer>(::operator new(__n * sizeof(_Tp))); }

	void
	deallocate(pointer __p, size_type)
	{ ::operator delete(__p); }

      public:
	// Members used: size(), push_back(), pop_back(),
	// insert(iterator, const_reference), erase(iterator),
	// begin(), end(), back(), operator[].

	__mini_vector() : _M_start(0), _M_finish(0), 
			  _M_end_of_storage(0)
	{ }

	~__mini_vector()
	{
	  if (this->_M_start)
	    {
	      this->deallocate(this->_M_start, this->_M_end_of_storage 
			       - this->_M_start);
	    }
	}

	size_type
	size() const throw()
	{ return _M_finish - _M_start; }

	iterator
	begin() const throw()
	{ return this->_M_start; }

	iterator
	end() const throw()
	{ return this->_M_finish; }

	reference
	back() const throw()
	{ return *(this->end() - 1); }

	reference
	operator[](const size_type __pos) const throw()
	{ return this->_M_start[__pos]; }

	void
	insert(iterator __pos, const_reference __x);

	void
	push_back(const_reference __x)
	{
	  if (this->_M_space_left())
	    {
	      *this->end() = __x;
	      ++this->_M_finish;
	    }
	  else
	    this->insert(this->end(), __x);
	}

	void
	pop_back() throw()
	{ --this->_M_finish; }

	void
	erase(iterator __pos) throw();

	void
	clear() throw()
	{ this->_M_finish = this->_M_start; }
      };

    // Out of line function definitions.
    template<typename _Tp>
      void __mini_vector<_Tp>::
      insert(iterator __pos, const_reference __x)
      {
	if (this->_M_space_left())
	  {
	    size_type __to_move = this->_M_finish - __pos;
	    iterator __dest = this->end();
	    iterator __src = this->end() - 1;

	    ++this->_M_finish;
	    while (__to_move)
	      {
		*__dest = *__src;
		--__dest; --__src; --__to_move;
	      }
	    *__pos = __x;
	  }
	else
	  {
	    size_type __new_size = this->size() ? this->size() * 2 : 1;
	    iterator __new_start = this->allocate(__new_size);
	    iterator __first = this->begin();
	    iterator __start = __new_start;
	    while (__first != __pos)
	      {
		*__start = *__first;
		++__start; ++__first;
	      }
	    *__start = __x;
	    ++__start;
	    while (__first != this->end())
	      {
		*__start = *__first;
		++__start; ++__first;
	      }
	    if (this->_M_start)
	      this->deallocate(this->_M_start, this->size());

	    this->_M_start = __new_start;
	    this->_M_finish = __start;
	    this->_M_end_of_storage = this->_M_start + __new_size;
	  }
      }

    template<typename _Tp>
      void __mini_vector<_Tp>::
      erase(iterator __pos) throw()
      {
	while (__pos + 1 != this->end())
	  {
	    *__pos = __pos[1];
	    ++__pos;
	  }
	--this->_M_finish;
      }


    template<typename _Tp>
      struct __mv_iter_traits
      {
	typedef typename _Tp::value_type value_type;
	typedef typename _Tp::difference_type difference_type;
      };

    template<typename _Tp>
      struct __mv_iter_traits<_Tp*>
      {
	typedef _Tp value_type;
	typedef std::ptrdiff_t difference_type;
      };

    enum 
      { 
	bits_per_byte = 8, 
	bits_per_block = sizeof(unsigned int) * bits_per_byte 
      };

    template<typename _ForwardIterator, typename _Tp, typename _Compare>
      _ForwardIterator
      __lower_bound(_ForwardIterator __first, _ForwardIterator __last,
		    const _Tp& __val, _Compare __comp)
      {
	typedef typename __mv_iter_traits<_ForwardIterator>::value_type
	  _ValueType;
	typedef typename __mv_iter_traits<_ForwardIterator>::difference_type
	  _DistanceType;

	_DistanceType __len = __last - __first;
	_DistanceType __half;
	_ForwardIterator __middle;

	while (__len > 0)
	  {
	    __half = __len >> 1;
	    __middle = __first;
	    __middle += __half;
	    if (__comp(*__middle, __val))
	      {
		__first = __middle;
		++__first;
		__len = __len - __half - 1;
	      }
	    else
	      __len = __half;
	  }
	return __first;
      }

    template<typename _InputIterator, typename _Predicate>
      inline _InputIterator
      __find_if(_InputIterator __first, _InputIterator __last, _Predicate __p)
      {
	while (__first != __last && !__p(*__first))
	  ++__first;
	return __first;
      }

    template<typename _AddrPair>
      inline size_t
      __num_blocks(_AddrPair __ap)
      { return (__ap.second - __ap.first) + 1; }

    template<typename _AddrPair>
      inline size_t 
      __num_bitmaps(_AddrPair __ap)
      { return __num_blocks(__ap) / bits_per_block; }

    // _Tp should be a pointer type.
    template<typename _Tp>
      class _Inclusive_between 
      : public std::unary_function<typename std::pair<_Tp, _Tp>, bool>
      {
	typedef _Tp pointer;
	pointer _M_ptr_value;
	typedef typename std::pair<_Tp, _Tp> _Block_pair;
	
      public:
	_Inclusive_between(pointer __ptr) : _M_ptr_value(__ptr) 
	{ }
	
	bool 
	operator()(_Block_pair __bp) const throw()
	{
	  if (std::less_equal<pointer>()(_M_ptr_value, __bp.second) 
	      && std::greater_equal<pointer>()(_M_ptr_value, __bp.first))
	    return true;
	  else
	    return false;
	}
      };
  
    // Used to pass a Functor to functions by reference.
    template<typename _Functor>
      class _Functor_Ref 
      : public std::unary_function<typename _Functor::argument_type, 
				   typename _Functor::result_type>
      {
	_Functor& _M_fref;
	
      public:
	typedef typename _Functor::argument_type argument_type;
	typedef typename _Functor::result_type result_type;

	_Functor_Ref(_Functor& __fref) : _M_fref(__fref) 
	{ }

	result_type 
	operator()(argument_type __arg) 
	{ return _M_fref(__arg); }
      };

    // _Tp should be a pointer type, and _Alloc is the Allocator for
    // the vector.
    template<typename _Tp>
      class _Ffit_finder 
      : public std::unary_function<typename std::pair<_Tp, _Tp>, bool>
      {
	typedef typename std::pair<_Tp, _Tp> _Block_pair;
	typedef typename balloc::__mini_vector<_Block_pair> _BPVector;
	typedef typename _BPVector::difference_type _Counter_type;

	unsigned int* _M_pbitmap;
	unsigned int _M_data_offset;

      public:
	_Ffit_finder() : _M_pbitmap(0), _M_data_offset(0)
	{ }

	bool 
	operator()(_Block_pair __bp) throw()
	{
	  // Set the _rover to the last unsigned integer, which is the
	  // bitmap to the first free block. Thus, the bitmaps are in exact
	  // reverse order of the actual memory layout. So, we count down
	  // the bimaps, which is the same as moving up the memory.

	  // If the used count stored at the start of the Bit Map headers
	  // is equal to the number of Objects that the current Block can
	  // store, then there is definitely no space for another single
	  // object, so just return false.
	  _Counter_type __diff = 
	    __gnu_cxx::balloc::__num_bitmaps(__bp);

	  if (*reinterpret_cast<unsigned int*>
	      (reinterpret_cast<char*>(__bp.first) - (sizeof(unsigned int) * 
						      (__diff+1)))
	      == __gnu_cxx::balloc::__num_blocks(__bp))
	    return false;

	  unsigned int* __rover = reinterpret_cast<unsigned int*>(__bp.first) - 1;

	  for (_Counter_type __i = 0; __i < __diff; ++__i)
	    {
	      _M_data_offset = __i;
	      if (*__rover)
		{
		  _M_pbitmap = __rover;
		  return true;
		}
	      --__rover;
	    }
	  return false;
	}

    
	unsigned int*
	_M_get() const throw()
	{ return _M_pbitmap; }

	unsigned int
	_M_offset() const throw()
	{ return _M_data_offset * bits_per_block; }
      };


  
    // _Tp should be a pointer type.
    template<typename _Tp>
      class _Bitmap_counter
      {
	typedef typename balloc::__mini_vector<typename std::pair<_Tp, _Tp> > 
	_BPVector;
	typedef typename _BPVector::size_type _Index_type;
	typedef _Tp pointer;
    
	_BPVector& _M_vbp;
	unsigned int* _M_curr_bmap;
	unsigned int* _M_last_bmap_in_block;
	_Index_type _M_curr_index;
    
      public:
	// Use the 2nd parameter with care. Make sure that such an
	// entry exists in the vector before passing that particular
	// index to this ctor.
	_Bitmap_counter(_BPVector& Rvbp, int __index = -1) : _M_vbp(Rvbp)
	{ this->_M_reset(__index); }
    
	void 
	_M_reset(int __index = -1) throw()
	{
	  if (__index == -1)
	    {
	      _M_curr_bmap = 0;
	      _M_curr_index = static_cast<_Index_type>(-1);
	      return;
	    }

	  _M_curr_index = __index;
	  _M_curr_bmap = reinterpret_cast<unsigned int*>
	    (_M_vbp[_M_curr_index].first) - 1;

	  _BALLOC_ASSERT(__index <= (int)_M_vbp.size() - 1);
	
	  _M_last_bmap_in_block = _M_curr_bmap
	    - ((_M_vbp[_M_curr_index].second 
		- _M_vbp[_M_curr_index].first + 1) 
	       / bits_per_block - 1);
	}
    
	// Dangerous Function! Use with extreme care. Pass to this
	// function ONLY those values that are known to be correct,
	// otherwise this will mess up big time.
	void
	_M_set_internal_bitmap(unsigned int* __new_internal_marker) throw()
	{ _M_curr_bmap = __new_internal_marker; }
    
	bool
	_M_finished() const throw()
	{ return(_M_curr_bmap == 0); }
    
	_Bitmap_counter&
	operator++() throw()
	{
	  if (_M_curr_bmap == _M_last_bmap_in_block)
	    {
	      if (++_M_curr_index == _M_vbp.size())
		_M_curr_bmap = 0;
	      else
		this->_M_reset(_M_curr_index);
	    }
	  else
	    --_M_curr_bmap;
	  return *this;
	}
    
	unsigned int*
	_M_get() const throw()
	{ return _M_curr_bmap; }
    
	pointer 
	_M_base() const throw()
	{ return _M_vbp[_M_curr_index].first; }

	unsigned int 
	_M_offset() const throw()
	{
	  return bits_per_block
	    * ((reinterpret_cast<unsigned int*>(this->_M_base()) 
		- _M_curr_bmap) - 1);
	}
    
	unsigned int
	_M_where() const throw()
	{ return _M_curr_index; }
      };

    inline void 
    __bit_allocate(unsigned int* __pbmap, unsigned int __pos) throw()
    {
      unsigned int __mask = 1 << __pos;
      __mask = ~__mask;
      *__pbmap &= __mask;
    }
  
    inline void 
    __bit_free(unsigned int* __pbmap, unsigned int __pos) throw()
    {
      unsigned int __mask = 1 << __pos;
      *__pbmap |= __mask;
    }
  } // namespace balloc

  // Generic Version of the bsf instruction.
  inline unsigned int 
  _Bit_scan_forward(register unsigned int __num)
  { return static_cast<unsigned int>(__builtin_ctz(__num)); }

  class free_list
  {
    typedef unsigned int* value_type;
    typedef balloc::__mini_vector<value_type> vector_type;
    typedef vector_type::iterator iterator;

    struct _LT_pointer_compare
    {
      bool
      operator()(const unsigned int* __pui, const unsigned int __cui) const throw()
      { return *__pui < __cui; }
    };

#if defined __GTHREADS 
    static _Mutex _S_bfl_mutex;
#endif
    static vector_type _S_free_list;
    
    void
    _M_validate(unsigned int* __addr) throw()
    {
      const unsigned int __max_size = 64;
      if (_S_free_list.size() >= __max_size)
	{
	  // Ok, the threshold value has been reached.  We determine
	  // which block to remove from the list of free blocks.
	  if (*__addr >= *_S_free_list.back())
	    {
	      // Ok, the new block is greater than or equal to the
	      // last block in the list of free blocks. We just free
	      // the new block.
	      ::operator delete(static_cast<void*>(__addr));
	      return;
	    }
	  else
	    {
	      // Deallocate the last block in the list of free lists,
	      // and insert the new one in it's correct position.
	      ::operator delete(static_cast<void*>(_S_free_list.back()));
	      _S_free_list.pop_back();
	    }
	}
	  
      // Just add the block to the list of free lists unconditionally.
      iterator __temp = __gnu_cxx::balloc::__lower_bound
	(_S_free_list.begin(), _S_free_list.end(), 
	 *__addr, _LT_pointer_compare());

      // We may insert the new free list before _temp;
      _S_free_list.insert(__temp, __addr);
    }

    bool 
    _M_should_i_give(unsigned int __block_size, 
		     unsigned int __required_size) throw()
    {
      const unsigned int __max_wastage_percentage = 36;
      if (__block_size >= __required_size && 
	  (((__block_size - __required_size) * 100 / __block_size)
	   < __max_wastage_percentage))
	return true;
      else
	return false;
    }

  public:
    inline void 
    _M_insert(unsigned int* __addr) throw()
    {
#if defined __GTHREADS
      _Auto_Lock __bfl_lock(&_S_bfl_mutex);
#endif
      // Call _M_validate to decide what should be done with
      // this particular free list.
      this->_M_validate(reinterpret_cast<unsigned int*>
			(reinterpret_cast<char*>(__addr) 
			 - sizeof(unsigned int)));
    }
    
    unsigned int*
    _M_get(unsigned int __sz) throw(std::bad_alloc);

    // This function just clears the internal Free List, and gives back
    // all the memory to the OS.
    void 
    _M_clear();
  };


  // Forward declare the class.
  template<typename _Tp> 
    class bitmap_allocator;

  // Specialize for void:
  template<>
    class bitmap_allocator<void>
    {
    public:
      typedef void*       pointer;
      typedef const void* const_pointer;

      // Reference-to-void members are impossible.
      typedef void  value_type;
      template<typename _Tp1>
        struct rebind
	{
	  typedef bitmap_allocator<_Tp1> other;
	};
    };

  template<typename _Tp>
    class bitmap_allocator : private free_list
    {
    public:
      typedef std::size_t    size_type;
      typedef std::ptrdiff_t difference_type;
      typedef _Tp*        pointer;
      typedef const _Tp*  const_pointer;
      typedef _Tp&        reference;
      typedef const _Tp&  const_reference;
      typedef _Tp         value_type;
      template<typename _Tp1>
        struct rebind
	{
	  typedef bitmap_allocator<_Tp1> other;
	};

    private:
      template<unsigned int _BSize, unsigned int _AlignSize>
        struct aligned_size
	{
	  enum
	    { 
	      modulus = _BSize % _AlignSize,
	      value = _BSize + (modulus ? _AlignSize - (modulus) : 0)
	    };
	};

      struct _Alloc_block
      {
	char __M_unused[aligned_size<sizeof(value_type), 8>::value];
      };


      typedef typename std::pair<_Alloc_block*, _Alloc_block*> _Block_pair;

      typedef typename 
      balloc::__mini_vector<_Block_pair> _BPVector;

#if defined _BALLOC_SANITY_CHECK
      // Complexity: O(lg(N)). Where, N is the number of block of size
      // sizeof(value_type).
      void 
      _S_check_for_free_blocks() throw()
      {
	typedef typename 
	  __gnu_cxx::balloc::_Ffit_finder<_Alloc_block*> _FFF;
	_FFF __fff;
	typedef typename _BPVector::iterator _BPiter;
	_BPiter __bpi = 
	  __gnu_cxx::balloc::__find_if
	  (_S_mem_blocks.begin(), _S_mem_blocks.end(), 
	   __gnu_cxx::balloc::_Functor_Ref<_FFF>(__fff));

	_BALLOC_ASSERT(__bpi == _S_mem_blocks.end());
      }
#endif

      // Complexity: O(1), but internally depends upon the complexity
      // of the function free_list::_M_get. The
      // part where the bitmap headers are written is of worst case
      // complexity: O(X),where X is the number of blocks of size
      // sizeof(value_type) within the newly acquired block. Having a
      // tight bound.
      void 
      _S_refill_pool() throw(std::bad_alloc)
      {
#if defined _BALLOC_SANITY_CHECK
	_S_check_for_free_blocks();
#endif

	const unsigned int __num_bitmaps = _S_block_size / balloc::bits_per_block;
	const unsigned int __size_to_allocate = sizeof(unsigned int)
	  + _S_block_size * sizeof(_Alloc_block) 
	  + __num_bitmaps * sizeof(unsigned int);

	unsigned int* __temp = 
	  reinterpret_cast<unsigned int*>(this->_M_get(__size_to_allocate));
	*__temp = 0;
	// ++__temp;
	__temp = reinterpret_cast<unsigned int*>
	  (reinterpret_cast<char*>(__temp) + sizeof(unsigned int));

	// The Header information goes at the Beginning of the Block.
	_Block_pair __bp = 
	  std::make_pair(reinterpret_cast<_Alloc_block*>
			 (__temp + __num_bitmaps), 
			 reinterpret_cast<_Alloc_block*>
			 (__temp + __num_bitmaps) 
			 + _S_block_size - 1);
	
	// Fill the Vector with this information.
	_S_mem_blocks.push_back(__bp);

	unsigned int __bit_mask = 0; // 0 Indicates all Allocated.
	__bit_mask = ~__bit_mask; // 1 Indicates all Free.

	for (unsigned int __i = 0; __i < __num_bitmaps; ++__i)
	  __temp[__i] = __bit_mask;

	_S_block_size *= 2;
      }


      static _BPVector _S_mem_blocks;
      static unsigned int _S_block_size;
      static __gnu_cxx::balloc::
      _Bitmap_counter<_Alloc_block*> _S_last_request;
      static typename _BPVector::size_type _S_last_dealloc_index;
#if defined __GTHREADS
      static _Mutex _S_mut;
#endif

    public:

      // Complexity: Worst case complexity is O(N), but that is hardly
      // ever hit. if and when this particular case is encountered,
      // the next few cases are guaranteed to have a worst case
      // complexity of O(1)!  That's why this function performs very
      // well on the average. you can consider this function to be
      // having a complexity referred to commonly as: Amortized
      // Constant time.
      pointer 
      _M_allocate_single_object() throw(std::bad_alloc)
      {
#if defined __GTHREADS
	_Auto_Lock __bit_lock(&_S_mut);
#endif

	// The algorithm is something like this: The last_request
	// variable points to the last accessed Bit Map. When such a
	// condition occurs, we try to find a free block in the
	// current bitmap, or succeeding bitmaps until the last bitmap
	// is reached. If no free block turns up, we resort to First
	// Fit method.

	// WARNING: Do not re-order the condition in the while
	// statement below, because it relies on C++'s short-circuit
	// evaluation. The return from _S_last_request->_M_get() will
	// NOT be dereference able if _S_last_request->_M_finished()
	// returns true. This would inevitably lead to a NULL pointer
	// dereference if tinkered with.
	while (_S_last_request._M_finished() == false
	       && (*(_S_last_request._M_get()) == 0))
	  {
	    _S_last_request.operator++();
	  }

	if (__builtin_expect(_S_last_request._M_finished() == true, false))
	  {
	    // Fall Back to First Fit algorithm.
	    typedef typename 
	      __gnu_cxx::balloc::_Ffit_finder<_Alloc_block*> _FFF;
	    _FFF __fff;
	    typedef typename _BPVector::iterator _BPiter;
	    _BPiter __bpi = 
	      __gnu_cxx::balloc::__find_if
	      (_S_mem_blocks.begin(), _S_mem_blocks.end(), 
	       __gnu_cxx::balloc::_Functor_Ref<_FFF>(__fff));

	    if (__bpi != _S_mem_blocks.end())
	      {
		// Search was successful. Ok, now mark the first bit from
		// the right as 0, meaning Allocated. This bit is obtained
		// by calling _M_get() on __fff.
		unsigned int __nz_bit = _Bit_scan_forward(*__fff._M_get());
		balloc::__bit_allocate(__fff._M_get(), __nz_bit);

		_S_last_request._M_reset(__bpi - _S_mem_blocks.begin());

		// Now, get the address of the bit we marked as allocated.
		pointer __ret = reinterpret_cast<pointer>
		  (__bpi->first + __fff._M_offset() + __nz_bit);
		unsigned int* __puse_count = reinterpret_cast<unsigned int*>
		  (reinterpret_cast<char*>
		   (__bpi->first) - (sizeof(unsigned int) * 
				     (__gnu_cxx::balloc::__num_bitmaps(*__bpi)+1)));
		
		++(*__puse_count);
		return __ret;
	      }
	    else
	      {
		// Search was unsuccessful. We Add more memory to the
		// pool by calling _S_refill_pool().
		_S_refill_pool();

		// _M_Reset the _S_last_request structure to the first
		// free block's bit map.
		_S_last_request._M_reset(_S_mem_blocks.size() - 1);

		// Now, mark that bit as allocated.
	      }
	  }

	// _S_last_request holds a pointer to a valid bit map, that
	// points to a free block in memory.
	unsigned int __nz_bit = _Bit_scan_forward(*_S_last_request._M_get());
	balloc::__bit_allocate(_S_last_request._M_get(), __nz_bit);

	pointer __ret = reinterpret_cast<pointer>
	  (_S_last_request._M_base() + _S_last_request._M_offset() + __nz_bit);

	unsigned int* __puse_count = reinterpret_cast<unsigned int*>
	  (reinterpret_cast<char*>
	   (_S_mem_blocks[_S_last_request._M_where()].first)
	   - (sizeof(unsigned int) * 
	      (__gnu_cxx::balloc::
	       __num_bitmaps(_S_mem_blocks[_S_last_request._M_where()])+1)));

	++(*__puse_count);
	return __ret;
      }

      // Complexity: O(lg(N)), but the worst case is hit quite often!
      // I need to do something about this. I'll be able to work on
      // it, only when I have some solid figures from a few real apps.
      void 
      _M_deallocate_single_object(pointer __p) throw()
      {
#if defined __GTHREADS
	_Auto_Lock __bit_lock(&_S_mut);
#endif
	_Alloc_block* __real_p = reinterpret_cast<_Alloc_block*>(__p);

	typedef typename _BPVector::iterator _Iterator;
	typedef typename _BPVector::difference_type _Difference_type;

	_Difference_type __diff;
	int __displacement;

	_BALLOC_ASSERT(_S_last_dealloc_index >= 0);

	
	if (__gnu_cxx::balloc::_Inclusive_between<_Alloc_block*>
	    (__real_p)
	    (_S_mem_blocks[_S_last_dealloc_index]))
	  {
	    _BALLOC_ASSERT(_S_last_dealloc_index <= _S_mem_blocks.size() - 1);

	    // Initial Assumption was correct!
	    __diff = _S_last_dealloc_index;
	    __displacement = __real_p - _S_mem_blocks[__diff].first;
	  }
	else
	  {
	    _Iterator _iter = 
	      __gnu_cxx::balloc::__find_if(_S_mem_blocks.begin(), 
					   _S_mem_blocks.end(), 
					   __gnu_cxx::balloc::
					   _Inclusive_between<_Alloc_block*>(__real_p));
	    _BALLOC_ASSERT(_iter != _S_mem_blocks.end());

	    __diff = _iter - _S_mem_blocks.begin();
	    __displacement = __real_p - _S_mem_blocks[__diff].first;
	    _S_last_dealloc_index = __diff;
	  }

	// Get the position of the iterator that has been found.
	const unsigned int __rotate = __displacement % balloc::bits_per_block;
	unsigned int* __bitmapC = 
	  reinterpret_cast<unsigned int*>(_S_mem_blocks[__diff].first) - 1;
	__bitmapC -= (__displacement / balloc::bits_per_block);
      
	balloc::__bit_free(__bitmapC, __rotate);
	unsigned int* __puse_count = reinterpret_cast<unsigned int*>
		  (reinterpret_cast<char*>
		   (_S_mem_blocks[__diff].first)
		   - (sizeof(unsigned int) * 
		      (__gnu_cxx::balloc::__num_bitmaps(_S_mem_blocks[__diff])+1)));
	
	_BALLOC_ASSERT(*__puse_count != 0);

	--(*__puse_count);

	if (__builtin_expect(*__puse_count == 0, false))
	  {
	    _S_block_size /= 2;
	  
	    // We can safely remove this block.
	    // _Block_pair __bp = _S_mem_blocks[__diff];
	    this->_M_insert(__puse_count);
	    _S_mem_blocks.erase(_S_mem_blocks.begin() + __diff);

	    // Reset the _S_last_request variable to reflect the
	    // erased block. We do this to protect future requests
	    // after the last block has been removed from a particular
	    // memory Chunk, which in turn has been returned to the
	    // free list, and hence had been erased from the vector,
	    // so the size of the vector gets reduced by 1.
	    if ((_Difference_type)_S_last_request._M_where() >= __diff--)
	      _S_last_request._M_reset(__diff); 

	    // If the Index into the vector of the region of memory
	    // that might hold the next address that will be passed to
	    // deallocated may have been invalidated due to the above
	    // erase procedure being called on the vector, hence we
	    // try to restore this invariant too.
	    if (_S_last_dealloc_index >= _S_mem_blocks.size())
	      {
		_S_last_dealloc_index =(__diff != -1 ? __diff : 0);
		_BALLOC_ASSERT(_S_last_dealloc_index >= 0);
	      }
	  }
      }

    public:
      bitmap_allocator() throw()
      { }

      bitmap_allocator(const bitmap_allocator&)
      { }

      template<typename _Tp1>
        bitmap_allocator(const bitmap_allocator<_Tp1>&) throw()
        { }

      ~bitmap_allocator() throw()
      { }

      // Complexity: O(1), but internally the complexity depends upon the
      // complexity of the function(s) _S_allocate_single_object and
      // operator new.
      pointer 
      allocate(size_type __n)
      {
	if (__builtin_expect(__n == 1, true))
	  return this->_M_allocate_single_object();
	else
	  { 
	    const size_type __b = __n * sizeof(value_type);
	    return reinterpret_cast<pointer>(::operator new(__b));
	  }
      }

      pointer 
      allocate(size_type __n, typename bitmap_allocator<void>::const_pointer)
      { return allocate(__n); }

      void 
      deallocate(pointer __p, size_type __n) throw()
      {
	if (__builtin_expect(__p != 0, true))
	  {
	    if (__builtin_expect(__n == 1, true))
	      this->_M_deallocate_single_object(__p);
	    else
	      ::operator delete(__p);
	  }
      }

      pointer 
      address(reference __r) const
      { return &__r; }

      const_pointer 
      address(const_reference __r) const
      { return &__r; }

      size_type 
      max_size() const throw()
      { return (size_type()-1)/sizeof(value_type); }

      void 
      construct(pointer __p, const_reference __data)
      { ::new(__p) value_type(__data); }

      void 
      destroy(pointer __p)
      { __p->~value_type(); }
    };

  template<typename _Tp1, typename _Tp2>
    bool 
    operator==(const bitmap_allocator<_Tp1>&, 
	       const bitmap_allocator<_Tp2>&) throw()
    { return true; }
  
  template<typename _Tp1, typename _Tp2>
    bool 
    operator!=(const bitmap_allocator<_Tp1>&, 
	       const bitmap_allocator<_Tp2>&) throw() 
  { return false; }

  // Static member definitions.
  template<typename _Tp>
    typename bitmap_allocator<_Tp>::_BPVector
    bitmap_allocator<_Tp>::_S_mem_blocks;

  template<typename _Tp>
    unsigned int bitmap_allocator<_Tp>::_S_block_size = balloc::bits_per_block;

  template<typename _Tp>
    typename __gnu_cxx::bitmap_allocator<_Tp>::_BPVector::size_type 
    bitmap_allocator<_Tp>::_S_last_dealloc_index = 0;

  template<typename _Tp>
    __gnu_cxx::balloc::_Bitmap_counter 
  <typename bitmap_allocator<_Tp>::_Alloc_block*>
    bitmap_allocator<_Tp>::_S_last_request(_S_mem_blocks);

#if defined __GTHREADS
  template<typename _Tp>
    __gnu_cxx::_Mutex
    bitmap_allocator<_Tp>::_S_mut;
#endif


}

#endif 

//  LocalWords:  namespace GTHREADS bool const gthread endif Mutex mutex
