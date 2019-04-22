// ConsoleApplication5.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <complex>
#include <ipp.h>

#include <functional>
#include <type_traits>

namespace traits
{
	/// Identity class which is used to carry parameter packs.
	template<typename... Args>
	struct identity { };

	namespace detail
	{
		template<typename Function>
		struct _Unwrap_function;

		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(_ATy...)>
		{
			/// The return type of the function.
			typedef _RTy return_type;

			/// The argument types of the function as pack in fu::identity.
			typedef identity<_ATy...> argument_type;

			/// The function provided as std::function
			typedef std::function<_RTy(_ATy...)> function_type;

			/// The function provided as function_ptr
			typedef _RTy(*function_ptr)(_ATy...);
		};

		/// STL: std::function
		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<std::function<_RTy(_ATy...)>>
			: _Unwrap_function<_RTy(_ATy...)> { };

		/// STL: std::tuple
		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<std::tuple<_RTy, _ATy...>>
			: _Unwrap_function<_RTy(_ATy...)> { };

		/// Function pointers
		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(*const)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

#ifdef _WIN32
		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__cdecl*)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__cdecl&)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__vectorcall*)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__vectorcall&)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

#ifndef _WIN64
		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__fastcall*)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__fastcall&)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__stdcall*)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };

		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(__stdcall&)(_ATy...)>
			: _Unwrap_function<_RTy(_ATy...)> { };
#endif
#endif
		/// Class Method pointers
		template<typename _CTy, typename _RTy, typename... _ATy>
		struct _Unwrap_function<_RTy(_CTy::*)(_ATy...) const>
			: _Unwrap_function<_RTy(_ATy...)> { };

		/// Pack in fu::identity
		template<typename _RTy, typename... _ATy>
		struct _Unwrap_function<identity<_RTy, _ATy...>>
			: _Unwrap_function<_RTy(_ATy...)> { };

		/// Unwrap through pointer of functor.
		template<typename Function>
		static _Unwrap_function<decltype(&Function::operator())> select_best_unwrap_unary_arg(int);

		/// Unwrap through plain type.
		template<typename Function>
		static _Unwrap_function<Function> select_best_unwrap_unary_arg(long);

		template<typename... _FTy>
		struct select_best_unwrap;

		/// Enable only if 1 template argument is given.
		template<typename _FTy>
		struct select_best_unwrap<_FTy>
		{
			typedef decltype(select_best_unwrap_unary_arg<_FTy>(0)) type;
		};

		// Enable if more then 1 template argument is given.
		// (Handles lazy typing)
		template<typename _RTy, typename... _ATy>
		struct select_best_unwrap<_RTy, _ATy...>
		{
			typedef _Unwrap_function<_RTy(_ATy...)> type;
		};

		template<typename _Enable = void, typename... Function>
		struct _Is_unwrappable : std::false_type { };

		template<typename... Function>
		struct _Is_unwrappable<typename select_best_unwrap<Function...>::type::function_type, Function...> : std::true_type { };

	} // detail

   /// Trait to unwrap function parameters of various types:
   /// Function style definition: Result(Parameters...)
   /// STL `std::function` : std::function<Result(Parameters...)>
   /// STL `std::tuple` : std::tuple<Result, Parameters...>
   /// C++ Function pointers: `Result(*)(Parameters...)`
   /// C++ Class method pointers: `Result(Class::*)(Parameters...)`
   /// Lazy typed signatures: `Result, Parameters...`
   /// Also provides optimized unwrap of functors and functional objects.
	template<typename... Function>
	using unwrap_function =
		typename detail::select_best_unwrap<Function...>::type;

	/// Trait which defines the return type of the function.
	template<typename... Function>
	using return_type_of_t =
		typename detail::select_best_unwrap<Function...>::type::return_type;

	/// Trait which defines the argument types of the function packed in std::tuple.
	template<typename... Function>
	using argument_type_of_t =
		typename detail::select_best_unwrap<Function...>::type::argument_type;

	/// Trait which defines the std::function type of the function.
	template<typename... Function>
	using function_type_of_t =
		typename detail::select_best_unwrap<Function...>::type::function_type;

	/// Trait which defines the function pointer type of the function.
	template<typename... Function>
	using function_ptr_of_t =
		typename detail::select_best_unwrap<Function...>::type::function_ptr;

	/// Trait which defines if the given function is unwrappable or not.
	template<typename... Function>
	struct is_unwrappable :
		detail::_Is_unwrappable<Function...> { };


	template<typename T, typename U>
	struct copy_cv_reference
	{
	private:
		using R = std::remove_reference_t<T>;
		using U1 = std::conditional_t<std::is_const<R>::value, std::add_const_t<U>, U>;
		using U2 = std::conditional_t<std::is_volatile<R>::value, std::add_volatile_t<U1>, U1>;
		using U3 = std::conditional_t<std::is_lvalue_reference<T>::value, std::add_lvalue_reference_t<U2>, U2>;
		using U4 = std::conditional_t<std::is_rvalue_reference<T>::value, std::add_rvalue_reference_t<U3>, U3>;
	public:
		using type = U2;
	};

	template<typename T, typename U>
	using copy_cv_reference_t = typename copy_cv_reference<T, U>::type;



	template <typename _Ty>
	struct remove_deepest_const { typedef _Ty type; };

	template <typename _Ty>
	struct remove_deepest_const<const _Ty> { typedef _Ty type; };

	template <typename _Ty>
	struct remove_deepest_const<_Ty*>
	{
		typedef typename remove_deepest_const<_Ty>::type* type;
	};

	template <typename _Ty>
	struct remove_deepest_const<_Ty* const>
	{
		typedef typename remove_deepest_const<_Ty>::type* const type;
	};

	template <typename _Ty>
	struct remove_deepest_const<_Ty&>
	{
		typedef typename remove_deepest_const<_Ty>::type& type;
	};

	template <typename _Ty>
	struct remove_deepest_const<_Ty&&>
	{
		typedef typename remove_deepest_const<_Ty>::type&& type;
	};

	template <typename _Ty> using remove_deepest_const_t = typename remove_deepest_const<_Ty>::type;


	template <typename _Ty>
	struct is_deepest_const : std::false_type {};

	template <typename _Ty>
	struct is_deepest_const<const _Ty> : std::true_type {};

	template <typename _Ty>
	struct is_deepest_const<_Ty*> : std::bool_constant<is_deepest_const<_Ty>::value> {};

	template <typename _Ty>
	struct is_deepest_const<_Ty* const> : std::bool_constant<is_deepest_const<_Ty>::value> {};

	template <typename _Ty>
	struct is_deepest_const<_Ty&> : std::bool_constant<is_deepest_const<_Ty>::value> {};

	template <typename _Ty>
	struct is_deepest_const<_Ty&&> : std::bool_constant<is_deepest_const<_Ty>::value> {};

	template <typename _Ty>
	inline constexpr bool is_deepest_const_v = is_deepest_const<_Ty>::value;


	template <class _InputType, class _OutputType>
	std::enable_if_t<
		sizeof(_InputType) == sizeof(_OutputType) &&
		is_deepest_const_v<std::remove_reference_t<_InputType>>, _OutputType>&&
		forward_as(std::remove_reference_t<_InputType>& _Arg) noexcept
	{ // forward an lvalue as either an lvalue or an rvalue
		return reinterpret_cast<_OutputType&&>(_Arg);
	}

	template <class _InputType, class _OutputType>
	std::enable_if_t<
		sizeof(_InputType) == sizeof(_OutputType) &&
		!is_deepest_const_v<std::remove_reference_t<_InputType>>, remove_deepest_const_t<_OutputType>> &&
		forward_as(std::remove_reference_t<_InputType> & _Arg) noexcept
	{ // forward an lvalue as either an lvalue or an rvalue
		return reinterpret_cast<remove_deepest_const_t<_OutputType>&&>(_Arg);
	}

	template <class _InputType, class _OutputType>
	std::enable_if_t<
		sizeof(_InputType) == sizeof(_OutputType) &&
		is_deepest_const_v<std::remove_reference_t<_InputType>>, _OutputType>&&
		forward_as(std::remove_reference_t<_InputType>&& _Arg) noexcept
	{ // forward an rvalue as an rvalue
		static_assert(!std::is_lvalue_reference_v<_InputType>, "bad forward call");
		return reinterpret_cast<_OutputType&&>(_Arg);
	}

	template <class _InputType, class _OutputType>
	std::enable_if_t<
		sizeof(_InputType) == sizeof(_OutputType) &&
		!is_deepest_const_v<std::remove_reference_t<_InputType>>, remove_deepest_const_t<_OutputType>> &&
		forward_as(std::remove_reference_t<_InputType> && _Arg) noexcept
	{ // forward an rvalue as an rvalue
		static_assert(!std::is_lvalue_reference_v<_InputType>, "bad forward call");
		return reinterpret_cast<remove_deepest_const_t<_OutputType>&&>(_Arg);
	}

	template<typename _Function, typename... _Args, typename... _Types>
	return_type_of_t<_Function> _Reinterpret_invoke(_Function&& func, identity<_Types...>, _Args&& ... args)
	{
		static_assert(sizeof...(_Args) == sizeof...(_Types), "Invalid number of arguments for function call");
		return std::forward<_Function>(func)(forward_as<_Args, _Types>(args)...);
	}

	template<typename _Function, typename... _Args>
	return_type_of_t<_Function> reinterpret_invoke(_Function&& func, _Args&& ... args)
	{
		return _Reinterpret_invoke(
			std::forward<_Function>(func),
			argument_type_of_t<_Function>{},
			std::forward<_Args>(args)...);
	}
}


template<typename _Ty>
struct ipp_data_type : std::integral_constant<IppDataType, ippUndef> {};
template<>
struct ipp_data_type<float> : std::integral_constant<IppDataType, ipp32f> {};
template<>
struct ipp_data_type<std::complex<float>> : std::integral_constant<IppDataType, ipp32fc> {};
template<>
struct ipp_data_type<double> : std::integral_constant<IppDataType, ipp64f> {};
template<>
struct ipp_data_type<std::complex<double>> : std::integral_constant<IppDataType, ipp64fc> {};


//template <typename T>
//class is_functor
//{
//	typedef char one;
//	typedef long two;
//
//	template <typename C> static one test(decltype(&C::operator()));
//	template <typename C> static two test(...);
//public:
//	static constexpr bool value = (sizeof(test<T>(0)) == sizeof(char));
//
//	using type = std::conditional_t<value, decltype(&T::operator()), T>;
//};

int main()
{
	std::vector<std::complex<float>> vec(10, 0.0f);
	std::complex<float> v{ 1.0f, 0.4f };
	std::complex<float>* p = &vec[0];
	traits::reinterpret_invoke(ippsSet_32fc, v, p, 10);

    std::cout << vec[3] << '\n';

	traits::reinterpret_invoke(ippsAddC_32fc_I, v, p, 10);

	const std::complex<float>* q = p;
	traits::reinterpret_invoke(ippsAddC_32fc, q, v, p, 10);

	auto i = traits::reinterpret_invoke(std::plus<float>{}, 1, 2);
	std::cout << i << '\n';

	std::cout << vec[3] << '\n';

	//is_functor<std::plus<float>>::value;
	//is_functor<decltype(ippsAddC_32fc)>::value;
	//is_functor<std::plus<float>>::type;
	//is_functor<decltype(ippsAddC_32fc)>::type;

	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
