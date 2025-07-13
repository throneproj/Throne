/***************************************************************************\
* Name        : function_ref                                                *
* Description : non-owning reference to a callable                          *
* Author      : LLVM                                                        *
* Reference   : https://llvm.org/doxygen/classllvm_1_1function__ref_3_01Ret_07Params_8_8_8_08_4.html
*
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/
#pragma once
#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

namespace spb::detail
{

template < typename Fn >
class function_ref;

template < typename Ret, typename... Params >
class function_ref< Ret( Params... ) >
{
    Ret ( *callback )( intptr_t callable, Params... params ) = nullptr;
    intptr_t callable;

    template < typename Callable >
    static Ret callback_fn( intptr_t callable, Params... params )
    {
        return ( *reinterpret_cast< Callable * >( callable ) )(
            std::forward< Params >( params )... );
    }

public:
    function_ref( ) = default;
    function_ref( std::nullptr_t )
    {
    }

    template < typename Callable >
    function_ref( Callable && callable,
                  // This is not the copy-constructor.
                  std::enable_if_t< !std::is_same< std::remove_cvref_t< Callable >,
                                                   function_ref >::value > * = nullptr,
                  // Functor must be callable and return a suitable type.
                  std::enable_if_t< std::is_void< Ret >::value ||
                                    std::is_convertible< decltype( std::declval< Callable >( )(
                                                             std::declval< Params >( )... ) ),
                                                         Ret >::value > * = nullptr )
        : callback( callback_fn< std::remove_reference_t< Callable > > )
        , callable( reinterpret_cast< intptr_t >( &callable ) )
    {
    }

    Ret operator( )( Params... params ) const
    {
        return callback( callable, std::forward< Params >( params )... );
    }

    explicit operator bool( ) const
    {
        return callback;
    }
};
}// namespace spb::detail
