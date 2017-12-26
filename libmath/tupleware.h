#pragma once

#include <tuple>
#include <functional>
#include <utility>

//template <typename SCALAR, size_t RANK>
//struct vec;


namespace tupleware
{
    // is_tuple - return bool_constant if given input is tuple
    template<typename INPUT>
    struct is_tuple : std::false_type {};
    template<typename ... TYPES>
    struct is_tuple<std::tuple<TYPES...>> : std::true_type {};
    template<typename INPUT>
    constexpr bool is_tuple_v = is_tuple<INPUT>::value;

    // tuple::ntuple - creates a repeated tuple with a given type and repetition count
    template <class TYPE, size_t N, class ... RESULT>
    struct ntuple {
        using type = typename ntuple<TYPE, N - 1, RESULT..., TYPE>::type;
    };
    template <class TYPE, class ... RESULT>
    struct ntuple<TYPE, 0, RESULT...> {
        using type = std::tuple<RESULT...>;
    };
    template <class TYPE, size_t N>
    using ntuple_t = typename ntuple<TYPE, N>::type;


    // tuple::append - appends a new element to a given tuple
    template <typename NEWTYPE, typename GIVEN_TUPLE>
    struct append;
    // append new element to given tuple
    template <typename NEWTYPE, typename ... TYPES>
    struct append<NEWTYPE, std::tuple<TYPES...>>
    {
        using type = std::tuple<TYPES..., NEWTYPE>;
    };
    // merge new tuple to given tuple
    template <typename ... NEWTYPES, typename ... TYPES>
    struct append<std::tuple<NEWTYPES...>, std::tuple<TYPES...>>
    {
        using type = std::tuple<TYPES..., NEWTYPES...>;
    };

    // tuple::extract - extracts a tuple from another tuple given a sequence of indexes
    //
    // example: given tup0 = std::tuple<int,char,std::string,std::list<int>,std::vector<char>>;
    //     using tup1 = typename tuple::extract<tup0, std::index_sequence<0,3,4>;
    //     // tup1 is now <int,std::list<int>,std::vector<char>>;
    //     // and tup1 is convertible from tup0
    //
    template<typename INPUT_TUPLE, typename INDEX_SEQ>
    struct extract;

    template<class ... TYPES, size_t ... INDEX>
    struct extract<std::tuple<TYPES...>, std::integer_sequence<size_t, INDEX...> >
    {
        using input_tuple = std::tuple<TYPES...>;

        template<typename OUTPUT, size_t INDEX0, size_t ... INDEXES>
        struct enumerate
        {
            using item = std::tuple_element_t<INDEX0, input_tuple>;
            using output = typename append<item, OUTPUT>::type;
            using type = typename enumerate<output, INDEXES...>::type;

            template <typename TUPLE, typename ... ARGS>
            static constexpr decltype(auto) get( TUPLE const& input, ARGS const& ...args )
            {
                return enumerate<output, INDEXES...>::get( input, args..., std::get<INDEX0>( input ) );
            }
            template <typename TUPLE, typename ... ARGS>
            static constexpr decltype(auto) get( TUPLE& input, ARGS& ...args )
            {
                return enumerate<output, INDEXES...>::get( input, args..., std::get<INDEX0>( input ) );
            }
        };

        template<typename OUTPUT, size_t LASTINDEX>
        struct enumerate<OUTPUT, LASTINDEX>
        {
            using item = std::tuple_element_t<LASTINDEX, input_tuple>;
            using type = typename append<item, OUTPUT>::type;

            template <typename TUPLE, typename ... ARGS>
            static constexpr decltype(auto) get( TUPLE const& input, ARGS const& ...args )
            {
                return std::make_tuple( args..., std::get<LASTINDEX>( input ) );
            }
            template <typename TUPLE, typename ... ARGS>
            static constexpr decltype(auto) get( TUPLE& input, ARGS& ...args )
            {
                return std::forward_as_tuple( args..., std::get<LASTINDEX>( input ) );
            }
        };

        // result of extraction
        using type = typename enumerate<std::tuple<>, INDEX...>::type;

        constexpr decltype(auto) operator()( input_tuple const& input ) const
        {
            return enumerate<std::tuple<>, INDEX...>::get( input );
        }
        constexpr decltype(auto) operator()( input_tuple& input ) const
        {
            return enumerate<std::tuple<>, INDEX...>::get( input );
        }
    };

    // extracting single element returns its type
    template<class ... TYPES, size_t INDEX>
    struct extract<std::tuple<TYPES...>, std::integer_sequence<size_t, INDEX> >
    {
        using input_tuple = std::tuple<TYPES...>;

        // result of extraction
        using type = std::tuple_element_t<INDEX, input_tuple>;

        constexpr decltype(auto) operator()( input_tuple const& input ) const
        {
            return std::get<INDEX>( input );
        }

        constexpr decltype(auto) operator()( input_tuple& input ) const
        {
            return std::get<INDEX>( input );
        }
    };

    namespace visitors
    {
        struct get
        {
            template<size_t INDEX, typename RETURN_TUPLE, typename TUPLE >
            static constexpr decltype(auto) visit( TUPLE& values )
            {
                return std::tuple_element_t<INDEX, RETURN_TUPLE>( std::get<INDEX>( values ) );
            }
        };

        struct repeat
        {
            template<size_t INDEX, typename RESULT_TUPLE, typename VALUE >
            static constexpr decltype(auto) visit( VALUE const& value )
            {
                return std::tuple_element_t<INDEX, RESULT_TUPLE>( value );
            }
        };

        struct merger
        {
            template<size_t INDEX, typename RETURN_TYPE, typename TUPLE, typename MERGER >
            static constexpr decltype(auto) visit( TUPLE const& values, MERGER const& merger )
            {
                return merger( std::get<INDEX>( values ) );
            }
        };

    }

    template <class TRANSFORM_TUPLE, class VISITOR, class RESULT_TUPLE = TRANSFORM_TUPLE >
    struct foreach
    {
        // generic visitor
        template<class ... ARGS, size_t ... INDEX>
        static constexpr decltype(auto) visit( std::index_sequence<INDEX...>, ARGS& ... args )
        {
            return RESULT_TUPLE{ VISITOR::visit<INDEX, TRANSFORM_TUPLE, ARGS...>( args... )... };
        };
        template<class ... ARGS, size_t ... INDEX>
        static constexpr decltype(auto) visit( std::index_sequence<INDEX...>, ARGS const& ... args )
        {
            return RESULT_TUPLE{ VISITOR::visit<INDEX, TRANSFORM_TUPLE, ARGS...>( args... )... };
        };

    };

    template <class SCALAR, size_t RANK>
    struct repeat_v
    {
        using tuple = ntuple_t<SCALAR, RANK>;
        static constexpr decltype(auto) with( SCALAR const v )
        {
            using iterator = foreach<tuple, visitors::repeat>;
            return iterator::visit( std::make_index_sequence<RANK>{}, v );
        }
    };


    template<typename AGGREGATE_FUNCTOR>
    struct aggregate
    {
        template<typename RESULT_VALUE, typename TUPLE, size_t INDEX>
        struct iterate
        {
            static constexpr decltype(auto) get( RESULT_VALUE const result, TUPLE const& values )
            {
                return iterate<RESULT_VALUE, TUPLE, INDEX - 1>::get( AGGREGATE_FUNCTOR::value( result, std::get<INDEX>( values ) ), values );
            }
        };
        template<typename RESULT_VALUE, typename TUPLE>
        struct iterate<RESULT_VALUE, TUPLE, 0>
        {
            static constexpr decltype(auto) get( RESULT_VALUE const result, TUPLE const& values )
            {
                return AGGREGATE_FUNCTOR::value( result, std::get<0>( values ) );
            }
        };

        template<typename ... TYPES>
        static constexpr decltype(auto) from( std::tuple<TYPES...> const& values )
        {
            using result_t = typename AGGREGATE_FUNCTOR::value_type;
            using tuple_t = std::tuple<TYPES...>;
            using iterator = iterate<result_t, tuple_t, sizeof...(TYPES) -1>;
            return iterator::get( AGGREGATE_FUNCTOR::initial_value, values );
        }
    };

}