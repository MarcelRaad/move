//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_MOVE_UNIQUE_PTR_HPP_INCLUDED
#define BOOST_MOVE_UNIQUE_PTR_HPP_INCLUDED

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/detail/workaround.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/move/detail/meta_utils.hpp>
#include <boost/static_assert.hpp>
#include <boost/assert.hpp>

#if !defined(BOOST_NO_CXX11_NULLPTR)
#include <cstddef>   //For std::nullptr_t
#endif

//!\file
//! Describes the smart pointer unique_ptr, a drop-in replacement for std::unique_ptr,
//! usable also from C++03 compilers.
//!
//! Main differences from std::unique_ptr to avoid heavy dependencies,
//! specially in C++03 compilers:
//!   - <tt>operator < </tt> uses pointer <tt>operator < </tt>instead of <tt>std::less<common_type></tt>. 
//!      This avoids dependencies on <tt>std::common_type</tt> and <tt>std::less</tt>
//!      (<tt><type_traits>/<functional></tt> headers. In C++03 this avoid pulling Boost.Typeof and other
//!      cascading dependencies. As in all Boost platforms <tt>operator <</tt> on raw pointers and
//!      other smart pointers provides strict weak ordering in practice this should not be a problem for users.
//!   - assignable from literal 0 for compilers without nullptr
//!   - <tt>unique_ptr<T[]></tt> is constructible and assignable from <tt>unique_ptr<U[]></tt> if
//!      cv-less T and cv-less U are the same type and T is more CV qualified than U.

namespace boost{

namespace move_detail {

template <class D>
struct deleter_types
{
   typedef typename add_lvalue_reference<D>::type           del_ref;
   typedef typename add_const_lvalue_reference<D>::type     del_cref;
   typedef typename if_c
      < is_lvalue_reference<D>::value, D, del_cref >::type  deleter_arg_type1;
   #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   typedef typename remove_reference<D>::type && deleter_arg_type2;
   #else
   typedef ::boost::rv<D> &deleter_arg_type2;
   #endif
};

////////////////////////////////////////////
//          unique_ptr_data
////////////////////////////////////////////
template <class P, class D, bool = is_unary_function<D>::value || is_reference<D>::value >
struct unique_ptr_data
{
   typedef typename deleter_types<D>::deleter_arg_type1 deleter_arg_type1;
   typedef typename deleter_types<D>::deleter_arg_type2 deleter_arg_type2;
   typedef typename deleter_types<D>::del_ref         del_ref;
   typedef typename deleter_types<D>::del_cref        del_cref;

   unique_ptr_data() BOOST_NOEXCEPT
      : m_p(), d()
   {}

   explicit unique_ptr_data(P p) BOOST_NOEXCEPT
      : m_p(p), d()
   {}

   unique_ptr_data(P p, deleter_arg_type1 d1) BOOST_NOEXCEPT
      : m_p(p), d(d1)
   {}

   unique_ptr_data(P p, deleter_arg_type2 d2) BOOST_NOEXCEPT
      : m_p(p), d(::boost::move(d2))
   {}

   template <class U>
   unique_ptr_data(P p, BOOST_FWD_REF(U) d) BOOST_NOEXCEPT
      : m_p(::boost::forward<U>(p)), d(::boost::forward<U>(d))
   {}

   del_ref deleter()       { return d; }
   del_cref deleter() const{ return d; }

   P m_p;
   D d;

   private:
   unique_ptr_data& operator=(const unique_ptr_data&);
   unique_ptr_data(const unique_ptr_data&);
};

template <class P, class D>
struct unique_ptr_data<P, D, false>
   : private D
{
   typedef typename deleter_types<D>::deleter_arg_type1 deleter_arg_type1;
   typedef typename deleter_types<D>::deleter_arg_type2 deleter_arg_type2;
   typedef typename deleter_types<D>::del_ref       del_ref;
   typedef typename deleter_types<D>::del_cref del_cref;

   unique_ptr_data() BOOST_NOEXCEPT
      : D(), m_p()
   {}

   unique_ptr_data(P p) BOOST_NOEXCEPT
      : D(), m_p(p)
   {}

   unique_ptr_data(P p, deleter_arg_type1 d1) BOOST_NOEXCEPT
      : D(d1), m_p(p)
   {}

   unique_ptr_data(P p, deleter_arg_type2 d2) BOOST_NOEXCEPT
      : D(::boost::move(d2)), m_p(p)
   {}

   template <class U>
   unique_ptr_data(P p, BOOST_FWD_REF(U) d) BOOST_NOEXCEPT
      : D(::boost::forward<U>(d)), m_p(p)
   {}

   del_ref deleter()       {  return static_cast<del_ref>(*this);   }
   del_cref deleter() const{  return static_cast<del_cref>(*this);  }

   P m_p;

   private:
   unique_ptr_data& operator=(const unique_ptr_data&);
   unique_ptr_data(const unique_ptr_data&);
};

////////////////////////////////////////////
//          is_unique_ptr_convertible
////////////////////////////////////////////

//Although non-standard, we avoid using pointer_traits
//to avoid heavy dependencies
template <typename T>
struct get_element_type
{
   struct DefaultWrap { typedef natify<T> element_type; };
   template <typename X>   static char test(int, typename X::element_type*);
   template <typename X>   static int test(...);
   static const bool value = (1 == sizeof(test<T>(0, 0)));
   typedef typename if_c<value, T, DefaultWrap>::type::element_type type;
};

template<class T>
struct get_element_type<T*>
{
   typedef T type;
};

template<class T>
struct get_cvelement
{
   typedef typename remove_cv<typename get_element_type<T>::type>::type type;
};

template <class P1, class P2>
struct is_same_cvelement_and_convertible
{
   typedef typename remove_reference<P1>::type arg1;
   typedef typename remove_reference<P2>::type arg2;
   static const bool same_cvless =
      is_same<typename get_cvelement<arg1>::type,typename get_cvelement<arg2>::type>::value;
   static const bool value = same_cvless && is_convertible<arg1, arg2>::value;
};

template<bool IsArray, class FromPointer, class ThisPointer>
struct is_unique_ptr_convertible
   : is_same_cvelement_and_convertible<FromPointer, ThisPointer>
{};

template<class FromPointer, class ThisPointer>
struct is_unique_ptr_convertible<false, FromPointer, ThisPointer>
   : is_convertible<FromPointer, ThisPointer>
{};

////////////////////////////////////////
////     enable_def_del / enable_defdel_call
////////////////////////////////////////

template<class U, class T, class Type = nat>
struct enable_def_del
   : enable_if_c
      < (is_array<T>::value == is_array<U>::value) && is_unique_ptr_convertible
            <is_array<T>::value, typename remove_extent<U>::type*, typename remove_extent<T>::type*>::value
      , Type>
{};

template<class U, class T, class Type = nat>
struct enable_defdel_call
   : public enable_def_del<U, T, Type>
{};

template<class U, class T, class Type>
struct enable_defdel_call<U, T[], Type>
   : public enable_def_del<U[], T[], Type>
{};

////////////////////////////////////////
////     enable_up_moveconv_assign
////////////////////////////////////////

template<class T, class FromPointer, class ThisPointer, class Type = nat>
struct enable_up_ptr
   : enable_if_c< is_unique_ptr_convertible < is_array<T>::value, FromPointer, ThisPointer>::value, Type>
{};

////////////////////////////////////////
////     enable_up_moveconv_assign
////////////////////////////////////////

template<class T, class D, class U, class E>
struct unique_moveconvert_assignable
{
   static const bool value = !is_array<U>::value && is_unique_ptr_convertible
      <false, typename pointer_type<U, E>::type, typename pointer_type<T, D>::type>::value;
};

template<class T, class D, class U, class E>
struct unique_moveconvert_assignable<T[], D, U, E>
{
   static const bool value = is_array<U>::value && is_unique_ptr_convertible
      <true, typename pointer_type<U, E>::type, typename pointer_type<T, D>::type>::value;
};

template<class T, class D, class U, class E, class Type = nat>
struct enable_up_moveconv_assign
   : enable_if_c<unique_moveconvert_assignable<T, D, U, E>::value, Type>
{};

////////////////////////////////////////
////     enable_up_moveconv_constr
////////////////////////////////////////

template<class D, class E, bool IsReference = is_reference<D>::value>
struct unique_deleter_is_initializable
{
   static const bool value = is_same<D, E>::value;
};

template <class T, class U>
class is_rvalue_convertible
{
   #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   typedef typename remove_reference<T>::type&& t_from;
   #else
   typedef typename if_c
      < has_move_emulation_enabled<T>::value && !is_reference<T>::value
      , boost::rv<T>&
      , typename add_lvalue_reference<T>::type
      >::type t_from;
   #endif

   typedef char true_t;
   class false_t { char dummy[2]; };
   static false_t dispatch(...);
   static true_t  dispatch(U);
   static t_from trigger();
   public:
   static const bool value = sizeof(dispatch(trigger())) == sizeof(true_t);
};

template<class D, class E>
struct unique_deleter_is_initializable<D, E, false>
{
   #if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
   //Clang has some problems with is_rvalue_convertible with non-copyable types
   //so use intrinsic if available
   #if defined(BOOST_CLANG)
      #if __has_feature(is_convertible_to)
      static const bool value = __is_convertible_to(E, D);
      #else
      static const bool value = is_rvalue_convertible<E, D>::value;
      #endif
   #else
   static const bool value = is_rvalue_convertible<E, D>::value;
   #endif

   #else //!defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
   //No hope for compilers with move emulation for now. In several compilers is_convertible
   // leads to errors, so just move the Deleter and see if the conversion works
   static const bool value = true;  /*is_rvalue_convertible<E, D>::value*/
   #endif
};

template<class T, class D, class U, class E, class Type = nat>
struct enable_up_moveconv_constr
   : enable_if_c<unique_moveconvert_assignable<T, D, U, E>::value &&
                 unique_deleter_is_initializable<D, E>::value, Type>
{};

////////////////////////////////////////
////     Some bool literal zero conversion utilities
////////////////////////////////////////

struct bool_conversion {int for_bool; int for_arg(); };
typedef int bool_conversion::* explicit_bool_arg;

#if !defined(BOOST_NO_CXX11_NULLPTR) 
typedef std::nullptr_t nullptr_type;
#else
typedef int (bool_conversion::*nullptr_type)();
#endif

}  //namespace move_detail {

namespace movelib {

namespace bmd = boost::move_detail;

//!The class template <tt>default_delete</tt> serves as the default deleter
//!(destruction policy) for the class template <tt>unique_ptr</tt>.
template <class T>
struct default_delete
{
   //! Default constructor.
   //!
   BOOST_CONSTEXPR default_delete()
   //Avoid "defaulted on its first declaration must not have an exception-specification" error for GCC 4.6
   #if !defined(BOOST_GCC) || (BOOST_GCC < 40600 && BOOST_GCC >= 40700) || defined(BOOST_MOVE_DOXYGEN_INVOKED)
   BOOST_NOEXCEPT
   #endif
   #if !defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) || defined(BOOST_MOVE_DOXYGEN_INVOKED)
   = default;
   #else
   {};
   #endif

   #if defined(BOOST_MOVE_DOXYGEN_INVOKED)
   default_delete(const default_delete&) BOOST_NOEXCEPT = default;
   default_delete &operator=(const default_delete&) BOOST_NOEXCEPT = default;
   #else
   typedef typename bmd::remove_extent<T>::type element_type;
   #endif

   //! <b>Effects</b>: Constructs a default_delete object from another <tt>default_delete<U></tt> object.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!   - If T is not an array type and U* is implicitly convertible to T*.
   //!   - If T is an array type and U* is a more CV qualified pointer to remove_extent<T>::type.
   template <class U>
   default_delete(const default_delete<U>&
      BOOST_MOVE_DOCIGN(BOOST_MOVE_I typename bmd::enable_def_del<U BOOST_MOVE_I T>::type* =0)
      ) BOOST_NOEXCEPT
   {}

   //! <b>Effects</b>: Constructs a default_delete object from another <tt>default_delete<U></tt> object.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!   - If T is not an array type and U* is implicitly convertible to T*.
   //!   - If T is an array type and U* is a more CV qualified pointer to remove_extent<T>::type.
   template <class U>
   BOOST_MOVE_DOC1ST(default_delete&, 
      typename bmd::enable_def_del<U BOOST_MOVE_I T BOOST_MOVE_I default_delete &>::type)
      operator=(const default_delete<U>&) BOOST_NOEXCEPT
   {  return *this;  }

   //! <b>Effects</b>: if T is not an array type, calls <tt>delete</tt> on static_cast<T*>(ptr),
   //!   otherwise calls <tt>delete[]</tt> on static_cast<remove_extent<T>::type*>(ptr).
   //!
   //! <b>Remarks</b>: If U is an incomplete type, the program is ill-formed.
   //!   This operator shall not participate in overload resolution unless:
   //!      - T is not an array type and U* is convertible to T*, OR
   //!      - T is an array type, and remove_cv<U>::type is the same type as
   //!         remove_cv<remove_extent<T>::type>::type and U* is convertible to remove_extent<T>::type*.
   template <class U>
   BOOST_MOVE_DOC1ST(void, typename bmd::enable_defdel_call<U BOOST_MOVE_I T BOOST_MOVE_I void>::type)
      operator()(U* ptr) const BOOST_NOEXCEPT
   {
      //U must be a complete type
      BOOST_STATIC_ASSERT(sizeof(U) > 0);
      element_type * const p = static_cast<element_type*>(ptr);
      bmd::is_array<T>::value ? delete [] p : delete p;
   }
};


//! A unique pointer is an object that owns another object and
//! manages that other object through a pointer.
//! 
//! More precisely, a unique pointer is an object u that stores a pointer to a second object p and will dispose
//! of p when u is itself destroyed (e.g., when leaving block scope). In this context, u is said to own p.
//! 
//! The mechanism by which u disposes of p is known as p's associated deleter, a function object whose correct
//! invocation results in p's appropriate disposition (typically its deletion).
//! 
//! Let the notation u.p denote the pointer stored by u, and let u.d denote the associated deleter. Upon request,
//! u can reset (replace) u.p and u.d with another pointer and deleter, but must properly dispose of its owned
//! object via the associated deleter before such replacement is considered completed.
//! 
//! Additionally, u can, upon request, transfer ownership to another unique pointer u2. Upon completion of
//! such a transfer, the following postconditions hold:
//!   - u2.p is equal to the pre-transfer u.p,
//!   - u.p is equal to nullptr, and
//!   - if the pre-transfer u.d maintained state, such state has been transferred to u2.d.
//! 
//! As in the case of a reset, u2 must properly dispose of its pre-transfer owned object via the pre-transfer
//! associated deleter before the ownership transfer is considered complete.
//! 
//! Each object of a type U instantiated from the unique_ptr template specified in this subclause has the strict
//! ownership semantics, specified above, of a unique pointer. In partial satisfaction of these semantics, each
//! such U is MoveConstructible and MoveAssignable, but is not CopyConstructible nor CopyAssignable.
//! The template parameter T of unique_ptr may be an incomplete type.
//! 
//! The uses of unique_ptr include providing exception safety for dynamically allocated memory, passing
//! ownership of dynamically allocated memory to a function, and returning dynamically allocated memory from
//! a function.
//!
//! If T is an array type (e.g. unique_ptr<MyType[]>) the interface is slightly altered:
//!   - Pointers to types derived from T are rejected by the constructors, and by reset.
//!   - The observers <tt>operator*</tt> and <tt>operator-></tt> are not provided.
//!   - The indexing observer <tt>operator[]</tt> is provided.
//!
//! \tparam T Provides the type of the stored pointer.
//! \tparam D The deleter type:
//!   -  The default type for the template parameter D is default_delete. A client-supplied template argument
//!      D shall be a function object type, lvalue-reference to function, or lvalue-reference to function object type
//!      for which, given a value d of type D and a value ptr of type unique_ptr<T, D>::pointer, the expression
//!      d(ptr) is valid and has the effect of disposing of the pointer as appropriate for that deleter.
//!   -  If the deleter's type D is not a reference type, D shall satisfy the requirements of Destructible.
//!   -  If the type <tt>remove_reference<D>::type::pointer</tt> exists, it shall satisfy the requirements of NullablePointer.
template <class T, class D = default_delete<T> >
class unique_ptr
{
   #if defined(BOOST_MOVE_DOXYGEN_INVOKED)
   public:
   unique_ptr(const unique_ptr&) = delete;
   unique_ptr& operator=(const unique_ptr&) = delete;
   private:
   #else
   BOOST_MOVABLE_BUT_NOT_COPYABLE(unique_ptr)

   typedef move_detail::pointer_type<T, D >                    pointer_type_obtainer;
   typedef bmd::unique_ptr_data
      <typename pointer_type_obtainer::type, D>                data_type;
   typedef typename data_type::deleter_arg_type1               deleter_arg_type1;
   typedef typename data_type::deleter_arg_type2               deleter_arg_type2;

   typedef bmd::integral_constant
      <bool, move_detail::is_same< D, default_delete<T> >::value > is_default_deleter_t;
   data_type m_data;
   #endif

   public:
   //! If the type <tt>remove_reference<D>::type::pointer</tt> exists, then it shall be a
   //! synonym for <tt>remove_reference<D>::type::pointer</tt>. Otherwise it shall be a
   //! synonym for T*.
   typedef typename BOOST_MOVE_SEEDOC(pointer_type_obtainer::type) pointer;
   //! If T is an array type, then element_type is equal to T. Otherwise, if T is a type
   //! in the form U[], element_type is equal to U.
   typedef typename BOOST_MOVE_SEEDOC(bmd::remove_extent<T>::type) element_type;
   typedef D deleter_type;

   //! <b>Requires</b>: D shall satisfy the requirements of DefaultConstructible, and
   //!   that construction shall not throw an exception.
   //!
   //! <b>Effects</b>: Constructs a unique_ptr object that owns nothing, value-initializing the
   //!   stored pointer and the stored deleter.
   //!
   //! <b>Postconditions</b>: <tt>get() == nullptr</tt>. <tt>get_deleter()</tt> returns a reference to the stored deleter.
   //!
   //! <b>Remarks</b>: If this constructor is instantiated with a pointer type or reference type
   //!   for the template argument D, the program is ill-formed.   
   BOOST_CONSTEXPR unique_ptr() BOOST_NOEXCEPT
      : m_data()
   {
      //If this constructor is instantiated with a pointer type or reference type
      //for the template argument D, the program is ill-formed.
      BOOST_STATIC_ASSERT(!bmd::is_pointer<D>::value);
      BOOST_STATIC_ASSERT(!bmd::is_reference<D>::value);
   }

   //! <b>Effects</b>: Same as <tt>unique_ptr()</tt> (default constructor).
   //! 
   BOOST_CONSTEXPR unique_ptr(BOOST_MOVE_DOC0PTR(bmd::nullptr_type)) BOOST_NOEXCEPT
      : m_data()
   {
      //If this constructor is instantiated with a pointer type or reference type
      //for the template argument D, the program is ill-formed.
      BOOST_STATIC_ASSERT(!bmd::is_pointer<D>::value);
      BOOST_STATIC_ASSERT(!bmd::is_reference<D>::value);
   }

   //! <b>Requires</b>: D shall satisfy the requirements of DefaultConstructible, and
   //!   that construction shall not throw an exception.
   //!
   //! <b>Effects</b>: Constructs a unique_ptr which owns p, initializing the stored pointer 
   //!   with p and value initializing the stored deleter.
   //!
   //! <b>Postconditions</b>: <tt>get() == p</tt>. <tt>get_deleter()</tt> returns a reference to the stored deleter.
   //!
   //! <b>Remarks</b>: If this constructor is instantiated with a pointer type or reference type
   //!   for the template argument D, the program is ill-formed.
   //!   This constructor shall not participate in overload resolution unless:
   //!      - If T is not an array type and Pointer is implicitly convertible to pointer.
   //!      - If T is an array type and Pointer is a more CV qualified pointer to element_type.
   template<class Pointer>
   explicit unique_ptr(Pointer p
      BOOST_MOVE_DOCIGN(BOOST_MOVE_I typename bmd::enable_up_ptr<T BOOST_MOVE_I Pointer BOOST_MOVE_I pointer>::type* =0)
                 ) BOOST_NOEXCEPT
      : m_data(p)
   {
      //If this constructor is instantiated with a pointer type or reference type
      //for the template argument D, the program is ill-formed.
      BOOST_STATIC_ASSERT(!bmd::is_pointer<D>::value);
      BOOST_STATIC_ASSERT(!bmd::is_reference<D>::value);
   }

   //!The signature of this constructor depends upon whether D is a reference type.
   //!   - If D is non-reference type A, then the signature is <tt>unique_ptr(pointer p, const A& d)</tt>.
   //!   - If D is an lvalue-reference type A&, then the signature is <tt>unique_ptr(pointer p, A& d)</tt>.
   //!   - If D is an lvalue-reference type const A&, then the signature is <tt>unique_ptr(pointer p, const A& d)</tt>.
   //!
   //! <b>Effects</b>: Constructs a unique_ptr object which owns p, initializing the stored pointer with p and
   //!   initializing the deleter as described above.
   //! 
   //! <b>Postconditions</b>: <tt>get() == p.get_deleter()</tt> returns a reference to the stored deleter. If D is a
   //!   reference type then <tt>get_deleter()</tt> returns a reference to the lvalue d.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!      - If T is not an array type and Pointer is implicitly convertible to pointer.
   //!      - If T is an array type and Pointer is a more CV qualified pointer to element_type.
   template<class Pointer>
   unique_ptr(Pointer p, BOOST_MOVE_SEEDOC(deleter_arg_type1) d1
      BOOST_MOVE_DOCIGN(BOOST_MOVE_I typename bmd::enable_up_ptr<T BOOST_MOVE_I Pointer BOOST_MOVE_I pointer>::type* =0)
              ) BOOST_NOEXCEPT
      : m_data(p, d1)
   {}

   unique_ptr(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), BOOST_MOVE_SEEDOC(deleter_arg_type1) d1) BOOST_NOEXCEPT
      : m_data(pointer(), d1)
   {}

   //! The signature of this constructor depends upon whether D is a reference type.
   //!   - If D is non-reference type A, then the signature is <tt>unique_ptr(pointer p, A&& d)</tt>.
   //!   - If D is an lvalue-reference type A&, then the signature is <tt>unique_ptr(pointer p, A&& d)</tt>.
   //!   - If D is an lvalue-reference type const A&, then the signature is <tt>unique_ptr(pointer p, const A&& d)</tt>.
   //!
   //! <b>Effects</b>: Constructs a unique_ptr object which owns p, initializing the stored pointer with p and
   //!   initializing the deleter as described above.
   //! 
   //! <b>Postconditions</b>: <tt>get() == p.get_deleter()</tt> returns a reference to the stored deleter. If D is a
   //!   reference type then <tt>get_deleter()</tt> returns a reference to the lvalue d.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!      - If T is not an array type and Pointer is implicitly convertible to pointer.
   //!      - If T is an array type and Pointer is a more CV qualified pointer to element_type.
   template<class Pointer>
   unique_ptr(Pointer p, BOOST_MOVE_SEEDOC(deleter_arg_type2) d2
      BOOST_MOVE_DOCIGN(BOOST_MOVE_I typename bmd::enable_up_ptr<T BOOST_MOVE_I Pointer BOOST_MOVE_I pointer>::type* =0)
             ) BOOST_NOEXCEPT
      : m_data(p, ::boost::move(d2))
   {}

   unique_ptr(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), BOOST_MOVE_SEEDOC(deleter_arg_type2) d2) BOOST_NOEXCEPT
      : m_data(pointer(), ::boost::move(d2))
   {}

   //! <b>Requires</b>: If D is not a reference type, D shall satisfy the requirements of MoveConstructible.
   //! Construction of the deleter from an rvalue of type D shall not throw an exception.
   //! 
   //! <b>Effects</b>: Constructs a unique_ptr by transferring ownership from u to *this. If D is a reference type,
   //! this deleter is copy constructed from u's deleter; otherwise, this deleter is move constructed from u's
   //! deleter.
   //! 
   //! <b>Postconditions</b>: <tt>get()</tt> yields the value u.get() yielded before the construction. <tt>get_deleter()</tt>
   //! returns a reference to the stored deleter that was constructed from u.get_deleter(). If D is a
   //! reference type then <tt>get_deleter()</tt> and <tt>u.get_deleter()</tt> both reference the same lvalue deleter.
   unique_ptr(BOOST_RV_REF(unique_ptr) u) BOOST_NOEXCEPT
      : m_data(u.release(), ::boost::move_if_not_lvalue_reference<D>(u.get_deleter()))
   {}

   //! <b>Requires</b>: If E is not a reference type, construction of the deleter from an rvalue of type E shall be
   //!   well formed and shall not throw an exception. Otherwise, E is a reference type and construction of the
   //!   deleter from an lvalue of type E shall be well formed and shall not throw an exception.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!   - <tt>unique_ptr<U, E>::pointer</tt> is implicitly convertible to pointer,
   //!   - U is not an array type, and
   //!   - either D is a reference type and E is the same type as D, or D is not a reference type and E is
   //!      implicitly convertible to D.
   //!
   //! <b>Effects</b>: Constructs a unique_ptr by transferring ownership from u to *this. If E is a reference type,
   //!   this deleter is copy constructed from u's deleter; otherwise, this deleter is move constructed from u's deleter.
   //!
   //! <b>Postconditions</b>: <tt>get()</tt> yields the value <tt>u.get()</tt> yielded before the construction. <tt>get_deleter()</tt>
   //!   returns a reference to the stored deleter that was constructed from <tt>u.get_deleter()</tt>.
   template <class U, class E>
   unique_ptr( BOOST_RV_REF_BEG unique_ptr<U, E> BOOST_RV_REF_END u
      BOOST_MOVE_DOCIGN(BOOST_MOVE_I typename bmd::enable_up_moveconv_constr<T BOOST_MOVE_I D BOOST_MOVE_I U BOOST_MOVE_I E>::type* =0)
      ) BOOST_NOEXCEPT
      : m_data(u.release(), ::boost::move_if_not_lvalue_reference<E>(u.get_deleter()))
   {}

   //! <b>Requires</b>: The expression <tt>get_deleter()(get())</tt> shall be well formed, shall have well-defined behavior,
   //!   and shall not throw exceptions.
   //!
   //! <b>Effects</b>: If <tt>get() == nullpt1r</tt> there are no effects. Otherwise <tt>get_deleter()(get())</tt>.
   //!
   //! <b>Note</b>: The use of default_delete requires T to be a complete type
   ~unique_ptr()
   {  if(m_data.m_p) m_data.deleter()(m_data.m_p);   }

   //! <b>Requires</b>: If D is not a reference type, D shall satisfy the requirements of MoveAssignable
   //!   and assignment of the deleter from an rvalue of type D shall not throw an exception. Otherwise, D
   //!   is a reference type; <tt>remove_reference<D>::type</tt> shall satisfy the CopyAssignable requirements and
   //!   assignment of the deleter from an lvalue of type D shall not throw an exception.
   //!
   //! <b>Effects</b>: Transfers ownership from u to *this as if by calling <tt>reset(u.release())</tt> followed
   //!   by <tt>get_deleter() = std::forward<D>(u.get_deleter())</tt>.
   //!
   //! <b>Returns</b>: *this.
   unique_ptr& operator=(BOOST_RV_REF(unique_ptr) u) BOOST_NOEXCEPT
   {
      this->reset(u.release());
      m_data.deleter() = ::boost::move_if_not_lvalue_reference<D>(u.get_deleter());
      return *this;
   }

   //! <b>Requires</b>: If E is not a reference type, assignment of the deleter from an rvalue of type E shall be
   //!   well-formed and shall not throw an exception. Otherwise, E is a reference type and assignment of the
   //!   deleter from an lvalue of type E shall be well-formed and shall not throw an exception.
   //!
   //! <b>Remarks</b>: This operator shall not participate in overload resolution unless:
   //!   - <tt>unique_ptr<U, E>::pointer</tt> is implicitly convertible to pointer and
   //!   - U is not an array type.
   //!
   //! <b>Effects</b>: Transfers ownership from u to *this as if by calling <tt>reset(u.release())</tt> followed by
   //!   <tt>get_deleter() = std::forward<E>(u.get_deleter())</tt>.
   //!
   //! <b>Returns</b>: *this.
   template <class U, class E>
   BOOST_MOVE_DOC1ST(unique_ptr&, typename bmd::enable_up_moveconv_assign
         <T BOOST_MOVE_I D BOOST_MOVE_I U BOOST_MOVE_I E BOOST_MOVE_I unique_ptr &>::type)
      operator=(BOOST_RV_REF_BEG unique_ptr<U, E> BOOST_RV_REF_END u) BOOST_NOEXCEPT
   {
      this->reset(u.release());
      m_data.deleter() = ::boost::move_if_not_lvalue_reference<E>(u.get_deleter());
      return *this;
   }

   //! <b>Effects</b>: <tt>reset()</tt>.
   //!
   //! <b>Postcondition</b>: <tt>get() == nullptr</tt>
   //!
   //! <b>Returns</b>: *this.
   unique_ptr& operator=(BOOST_MOVE_DOC0PTR(bmd::nullptr_type)) BOOST_NOEXCEPT
   {
      this->reset();
      return *this;
   }

   //! <b>Requires</b>: <tt>get() != nullptr</tt>.
   //!
   //! <b>Returns</b>: <tt>*get()</tt>.
   //!
   //! <b>Remarks</b: If T is an array type, the program is ill-formed.
   BOOST_MOVE_DOC1ST(element_type&, typename bmd::add_lvalue_reference<element_type>::type)
      operator*() const
   {
      BOOST_STATIC_ASSERT((!bmd::is_array<T>::value));
      return *get();
   }

   //! <b>Requires</b>: i < the number of elements in the array to which the stored pointer points.
   //!
   //! <b>Returns</b>: <tt>get()[i]</tt>.
   //!
   //! <b>Remarks</b: If T is not an array type, the program is ill-formed.
   BOOST_MOVE_DOC1ST(element_type&, typename bmd::add_lvalue_reference<element_type>::type)
      operator[](size_t i) const
   {
      BOOST_STATIC_ASSERT((bmd::is_array<T>::value));
      const pointer p = this->get();
      BOOST_ASSERT(p);
      return p[i];
   }

   //! <b>Requires</b>: <tt>get() != nullptr</tt>.
   //!
   //! <b>Returns</b>: <tt>get()</tt>.
   //!
   //! <b>Note</b>: use typically requires that T be a complete type.
   //!
   //! <b>Remarks</b: If T is an array type, the program is ill-formed.
   pointer operator->() const BOOST_NOEXCEPT
   {
      BOOST_STATIC_ASSERT((!bmd::is_array<T>::value));
      return get();
   }

   //! <b>Returns</b>: The stored pointer.
   //!
   pointer get() const BOOST_NOEXCEPT
   {  return m_data.m_p;  }

   //! <b>Returns</b>: A reference to the stored deleter.
   //!
   BOOST_MOVE_DOC1ST(D&, typename bmd::add_lvalue_reference<D>::type)
      get_deleter() BOOST_NOEXCEPT
   {  return m_data.deleter();  }   

   //! <b>Returns</b>: A reference to the stored deleter.
   //!
   BOOST_MOVE_DOC1ST(const D&, typename bmd::add_const_lvalue_reference<D>::type)
      get_deleter() const BOOST_NOEXCEPT
   {  return m_data.deleter();  }

   #ifdef BOOST_MOVE_DOXYGEN_INVOKED
   //! <b>Returns</b>: Returns: get() != nullptr.
   //!
   explicit operator bool
   #else
   operator bmd::explicit_bool_arg
   #endif
      ()const BOOST_NOEXCEPT
   {
      return m_data.m_p
         ? &bmd::bool_conversion::for_bool
         : bmd::explicit_bool_arg(0);
   }


   //! <b>Postcondition</b>: <tt>get() == nullptr</tt>.
   //!
   //! <b>Returns</b>: The value <tt>get()</tt> had at the start of the call to release.   
   pointer release() BOOST_NOEXCEPT
   {
      const pointer tmp = m_data.m_p;
      m_data.m_p = pointer();
      return tmp;
   }

   //! <b>Requires</b>: The expression <tt>get_deleter()(get())</tt> shall be well formed, shall have well-defined behavior,
   //!   and shall not throw exceptions.
   //!
   //! <b>Effects</b>: assigns p to the stored pointer, and then if the old value of the stored pointer, old_p, was not
   //!   equal to nullptr, calls <tt>get_deleter()(old_p)</tt>. Note: The order of these operations is significant
   //!   because the call to <tt>get_deleter()</tt> may destroy *this.
   //!
   //! <b>Postconditions</b>: <tt>get() == p</tt>. Note: The postcondition does not hold if the call to <tt>get_deleter()</tt>
   //!   destroys *this since <tt>this->get()</tt> is no longer a valid expression.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!      - If T is not an array type and Pointer is implicitly convertible to pointer.
   //!      - If T is an array type and Pointer is a more CV qualified pointer to element_type.
   template<class Pointer>
   BOOST_MOVE_DOC1ST(void, typename bmd::enable_up_ptr<T BOOST_MOVE_I Pointer BOOST_MOVE_I pointer BOOST_MOVE_I void>::type)
      reset(Pointer p) BOOST_NOEXCEPT
   {
      pointer tmp = m_data.m_p;
      m_data.m_p = p;
      if(tmp) m_data.deleter()(tmp);
   }

   //! <b>Requires</b>: The expression <tt>get_deleter()(get())</tt> shall be well formed, shall have well-defined behavior,
   //!   and shall not throw exceptions.
   //!
   //! <b>Effects</b>: assigns nullptr to the stored pointer, and then if the old value of the stored pointer, old_p, was not
   //!   equal to nullptr, calls <tt>get_deleter()(old_p)</tt>. Note: The order of these operations is significant
   //!   because the call to <tt>get_deleter()</tt> may destroy *this.
   //!
   //! <b>Postconditions</b>: <tt>get() == p</tt>. Note: The postcondition does not hold if the call to <tt>get_deleter()</tt>
   //!   destroys *this since <tt>this->get()</tt> is no longer a valid expression.
   void reset() BOOST_NOEXCEPT
   {  this->reset(pointer());  }

   //! <b>Effects</b>: Same as <tt>reset()</tt>
   //! 
   void reset(BOOST_MOVE_DOC0PTR(bmd::nullptr_type)) BOOST_NOEXCEPT
   {  this->reset(); }

   //! <b>Requires</b>: <tt>get_deleter()</tt> shall be swappable and shall not throw an exception under swap.
   //!
   //! <b>Effects</b>: Invokes swap on the stored pointers and on the stored deleters of *this and u.
   void swap(unique_ptr& u) BOOST_NOEXCEPT
   {
      using bmd::swap;
      swap(m_data.m_p, u.m_data.m_p);
      swap(m_data.deleter(), u.m_data.deleter());
   }
};

//! <b>Effects</b>: Calls <tt>x.swap(y)</tt>.
//!
template <class T, class D>
inline void swap(unique_ptr<T, D> &x, unique_ptr<T, D> &y) BOOST_NOEXCEPT
{  x.swap(y); }

//! <b>Returns</b>: <tt>x.get() == y.get()</tt>.
//!
template <class T1, class D1, class T2, class D2>
inline bool operator==(const unique_ptr<T1, D1> &x, const unique_ptr<T2, D2> &y)
{  return x.get() == y.get(); }

//! <b>Returns</b>: <tt>x.get() != y.get()</tt>.
//!
template <class T1, class D1, class T2, class D2>
inline bool operator!=(const unique_ptr<T1, D1> &x, const unique_ptr<T2, D2> &y)
{  return x.get() != y.get(); }

//! <b>Returns</b>: x.get() < y.get().
//!
//! <b>Remarks</b>: This comparison shall induce a
//!   strict weak ordering betwen pointers.
template <class T1, class D1, class T2, class D2>
inline bool operator<(const unique_ptr<T1, D1> &x, const unique_ptr<T2, D2> &y)
{  return x.get() < y.get();  }

//! <b>Returns</b>: !(y < x).
//!
template <class T1, class D1, class T2, class D2>
inline bool operator<=(const unique_ptr<T1, D1> &x, const unique_ptr<T2, D2> &y)
{  return !(y < x);  }

//! <b>Returns</b>: y < x.
//!
template <class T1, class D1, class T2, class D2>
inline bool operator>(const unique_ptr<T1, D1> &x, const unique_ptr<T2, D2> &y)
{  return y < x;  }

//! <b>Returns</b>:!(x < y).
//!
template <class T1, class D1, class T2, class D2>
inline bool operator>=(const unique_ptr<T1, D1> &x, const unique_ptr<T2, D2> &y)
{  return !(x < y);  }

//! <b>Returns</b>:!x.
//!
template <class T1, class D1>
inline bool operator==(const unique_ptr<T1, D1> &x, BOOST_MOVE_DOC0PTR(bmd::nullptr_type)) BOOST_NOEXCEPT
{  return !x;  }

//! <b>Returns</b>:!x.
//!
template <class T1, class D1>
inline bool operator==(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), const unique_ptr<T1, D1> &x) BOOST_NOEXCEPT
{  return !x;  }

//! <b>Returns</b>: (bool)x.
//!
template <class T1, class D1>
inline bool operator!=(const unique_ptr<T1, D1> &x, BOOST_MOVE_DOC0PTR(bmd::nullptr_type)) BOOST_NOEXCEPT
{  return !!x;  }

//! <b>Returns</b>: (bool)x.
//!
template <class T1, class D1>
inline bool operator!=(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), const unique_ptr<T1, D1> &x) BOOST_NOEXCEPT
{  return !!x;  }

//! <b>Requires</b>: <tt>operator </tt> shall induce a strict weak ordering on unique_ptr<T1, D1>::pointer values.
//!
//! <b>Returns</b>: Returns <tt>x.get() < pointer()</tt>.
template <class T1, class D1>
inline bool operator<(const unique_ptr<T1, D1> &x, BOOST_MOVE_DOC0PTR(bmd::nullptr_type))
{
   typedef typename unique_ptr<T1, D1>::pointer pointer;
   return x.get() < pointer();
}

//! <b>Requires</b>: <tt>operator </tt> shall induce a strict weak ordering on unique_ptr<T1, D1>::pointer values.
//!
//! <b>Returns</b>: Returns <tt>pointer() < x.get()</tt>.
template <class T1, class D1>
inline bool operator<(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), const unique_ptr<T1, D1> &x)
{
   typedef typename unique_ptr<T1, D1>::pointer pointer;
   return pointer() < x.get();
}

//! <b>Returns</b>: <tt>nullptr < x</tt>.
//!
template <class T1, class D1>
inline bool operator>(const unique_ptr<T1, D1> &x, BOOST_MOVE_DOC0PTR(bmd::nullptr_type))
{
   typedef typename unique_ptr<T1, D1>::pointer pointer;
   return x.get() > pointer();
}

//! <b>Returns</b>: <tt>x < nullptr</tt>.
//!
template <class T1, class D1>
inline bool operator>(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), const unique_ptr<T1, D1> &x)
{
   typedef typename unique_ptr<T1, D1>::pointer pointer;
   return pointer() > x.get();
}

//! <b>Returns</b>: <tt>!(nullptr < x)</tt>.
//!
template <class T1, class D1>
inline bool operator<=(const unique_ptr<T1, D1> &x, BOOST_MOVE_DOC0PTR(bmd::nullptr_type))
{  return !(bmd::nullptr_type() < x);  }

//! <b>Returns</b>: <tt>!(x < nullptr)</tt>.
//!
template <class T1, class D1>
inline bool operator<=(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), const unique_ptr<T1, D1> &x)
{  return !(x < bmd::nullptr_type());  }

//! <b>Returns</b>: <tt>!(x < nullptr)</tt>.
//!
template <class T1, class D1>
inline bool operator>=(const unique_ptr<T1, D1> &x, BOOST_MOVE_DOC0PTR(bmd::nullptr_type))
{  return !(x < bmd::nullptr_type());  }

//! <b>Returns</b>: <tt>!(nullptr < x)</tt>.
//!
template <class T1, class D1>
inline bool operator>=(BOOST_MOVE_DOC0PTR(bmd::nullptr_type), const unique_ptr<T1, D1> &x)
{  return !(bmd::nullptr_type() < x);  }

}  //namespace movelib {
}  //namespace boost{

#include <boost/move/detail/config_end.hpp>

#endif   //#ifndef BOOST_MOVE_UNIQUE_PTR_HPP_INCLUDED
