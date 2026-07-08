/// @ref core
/// @file glm/detail/type_gentype.hpp

#pragma once

namespace glm
{
	enum profile
	{
		nice,
		fast,
		simd
	};

	typedef std::size_t sizeType;
	
namespace detail
{
	template
	<
		typename VALTYPE, 
		template <typename> class TYPE
	>
	struct genType
	{
	public:
		enum ctor{null};

		typedef VALTYPE value_type;
		typedef VALTYPE & value_reference;
		typedef VALTYPE * value_pointer;
		typedef VALTYPE const * value_const_pointer;
		typedef TYPE<bool> bool_type;

		typedef sizeType size_type;
		static bool is_vector();
		static bool is_matrix();
		
		typedef TYPE<VALTYPE> type;
		typedef TYPE<VALTYPE> * pointer;
		typedef TYPE<VALTYPE> const * const_pointer;
		typedef TYPE<VALTYPE> const * const const_pointer_const;
		typedef TYPE<VALTYPE> * const pointer_const;
		typedef TYPE<VALTYPE> & reference;
		typedef TYPE<VALTYPE> const & const_reference;
		typedef TYPE<VALTYPE> const & param_type;

		//////////////////////////////////////
		// Address (Implementation details)

		value_const_pointer value_address() const{return value_pointer(this);}
		value_pointer value_address(){return value_pointer(this);}

	//protected:
	//	enum kind
	//	{
	//		GEN_TYPE,
	//		VEC_TYPE,
	//		MAT_TYPE
	//	};

	//	typedef typename TYPE::kind kind;
	};

	template
	<
		typename VALTYPE, 
		template <typename> class TYPE
	>
	bool genType<VALTYPE, TYPE>::is_vector()
	{
		return true;
	}
/*
	template <typename valTypeT, unsigned int colT, unsigned int rowT, profile proT = nice>
	class base
	{
	public:
		//////////////////////////////////////
		// Traits

		typedef sizeType							size_type;
		typedef valTypeT							value_type;

		typedef base<value_type, colT, rowT>		class_type;

		typedef base<bool, colT, rowT>				bool_type;
		typedef base<value_type, rowT, 1>			col_type;
		typedef base<value_type, colT, 1>			row_type;
		typedef base<value_type, rowT, colT>		transpose_type;

		static size_type							col_size();
		static size_type							row_size();
		static size_type							value_size();
		static bool									is_scalar();
		static bool									is_vector();
		static bool									is_matrix();

	private:
		// Data 
		col_type value[colT];		

	public:
		//////////////////////////////////////
		// Constructors
		base();
		base(class_type const & m);

		explicit base(T const & x);
		explicit base(value_type const * const x);
		explicit base(col_type const * const x);

		//////////////////////////////////////
		// Conversions
		template <typename vU, uint cU, uint rU, profile pU>
		explicit base(base<vU, cU, rU, pU> const & m);

		//////////////////////////////////////
		// Accesses
		col_type& operator[](size_type i);
		col_type const & operator[](size_type i) const;

		//////////////////////////////////////
		// Unary updatable operators
		class_type& operator=  (class_type const & x);
		class_type& operator+= (T const & x);
		class_type& operator+= (class_type const & x);
		class_type& operator-= (T const & x);
		class_type& operator-= (class_type const & x);
		class_type& operator*= (T const & x);
		class_type& operator*= (class_type const & x);
		class_type& operator/= (T const & x);
		class_type& operator/= (class_type const & x);
		class_type& operator++ ();
		class_type& operator-- ();
	};
*/
	
	//template <typename T>
	//struct traits
	//{
	//	static const bool is_signed = false;
	//	static const bool is_float = false;
	//	static const bool is_vector = false;
	//	static const bool is_matrix = false;
	//	static const bool is_genType = false;
	//	static const bool is_genIType = false;
	//	static const bool is_genUType = false;
	//};
	
	//template <>
	//struct traits<half>
	//{
	//	static const bool is_float = true;
	//	static const bool is_genType = true;
	//};
	
	//template <>
	//struct traits<float>
	//{
	//	static const bool is_float = true;
	//	static const bool is_genType = true;
	//};
	
	//template <>
	//struct traits<double>
	//{
	//	static const bool is_float = true;
	//	static const bool is_genType = true;
	//};
	
	//template <typename genType>
	//struct desc
	//{
	//	typedef genType							type;
	//	typedef genType *						pointer;
	//	typedef genType const*					const_pointer;
	//	typedef genType const *const			const_pointer_const;
	//	typedef genType *const					pointer_const;
	//	typedef genType &						reference;
	//	typedef genType const&					const_reference;
	//	typedef genType const&					param_type;
	
	//	typedef typename genType::value_type	value_type;
	//	typedef typename genType::size_type		size_type;
	//	static const typename size_type			value_size;
	//};
	
	//template <typename genType>
	//const typename desc<genType>::size_type desc<genType>::value_size = genType::value_size();
	
}//namespace detail
}//namespace glm

//#include "type_gentype.inl"
