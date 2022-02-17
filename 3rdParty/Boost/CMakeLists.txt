find_package(Boost 1.74.0 COMPONENTS
	program_options
		REQUIRED)

set(CFLAGS
	BOOST_ALL_DYN_LINK=1
)

if ( Boost_FOUND )
	set_target_properties(Boost::boost
		Boost::program_options
			PROPERTIES INTERFACE_LINK_DIRECTORIES "${BOOST_LIBRARYDIR}"
	)
	set_target_properties( Boost::boost
		Boost::program_options
			PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${CFLAGS}" 
	)

	set_target_properties( Boost::boost
		Boost::program_options
			PROPERTIES IMPORTED_GLOBAL TRUE
	)
endif ()
