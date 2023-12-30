SET(test_source "int main() { }")

try_compile(FUZZING_WORKS SOURCE_FROM_VAR test.cpp test_source LINK_OPTIONS -fsanitize=fuzzer)

if (NOT FUZZING_WORKS)
	function(add_fuzz_target TARGET SOURCE DICTIONARY)
		add_executable(${TARGET} ${SOURCE})
	endfunction()
	return()
endif()

add_custom_target(fuzz)

function(add_fuzz_target TARGET SOURCE DICTIONARY)
	add_executable(${TARGET} ${SOURCE})
	target_link_libraries(${TARGET} PUBLIC -fsanitize=fuzzer,address -fsanitize-coverage=trace-pc-guard)
	
	add_custom_target(fuzz-${TARGET} DEPENDS ${TARGET} COMMAND ${TARGET} ${DICTIONARY})
	add_dependencies(fuzz fuzz-${TARGET})
endfunction()
