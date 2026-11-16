# Builds a tree-sitter grammar from its shipped, already generated parser.c
# and scanner.c, so neither the tree-sitter CLI nor Node.js is needed. The
# tree-sitter-cpp and tree-sitter-sql ports carry identical copies.
cmake_minimum_required(VERSION 3.20)
project(tree-sitter-grammar LANGUAGES C)

if(NOT GRAMMAR)
    message(FATAL_ERROR "GRAMMAR must name the grammar, e.g. cpp.")
endif()

set(target tree-sitter-${GRAMMAR})
set(package unofficial-tree-sitter-${GRAMMAR})
string(TOUPPER "${GRAMMAR}" GUARD)

add_library(${target} STATIC src/parser.c src/scanner.c)
set_target_properties(${target} PROPERTIES C_STANDARD 11)
target_include_directories(${target}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    INTERFACE "$<INSTALL_INTERFACE:include>")

file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}.h" CONTENT
"#ifndef TREE_SITTER_@GUARD@_H_
#define TREE_SITTER_@GUARD@_H_

typedef struct TSLanguage TSLanguage;

#ifdef __cplusplus
extern \"C\" {
#endif

const TSLanguage *tree_sitter_@GRAMMAR@(void);

#ifdef __cplusplus
}
#endif

#endif
" @ONLY)

include(GNUInstallDirs)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${target}.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tree_sitter")
install(TARGETS ${target} EXPORT ${package}-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(EXPORT ${package}-targets
    FILE ${package}-config.cmake
    NAMESPACE unofficial::${target}::
    DESTINATION "share/${package}")
