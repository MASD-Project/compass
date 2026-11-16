# The SQL grammar for tree-sitter, built from its release tarball. The
# repository does not commit the generated parser; the tarball ships it.
# CMakeLists.txt in this directory compiles parser.c and scanner.c.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/DerekStride/tree-sitter-sql/releases/download/v${VERSION}/tree-sitter-sql-v${VERSION}.tar.gz"
    FILENAME "tree-sitter-sql-v${VERSION}.tar.gz"
    SHA512 4396c1d6957a6b899cdfac49b10e0dd7cdca1aa14f294c6122f36f1a8c4a181831e20f3eba9847cda00e2f5c45fee066c5daaa7c339c2d7d8c31a26ce3745adb
)

vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    NO_REMOVE_ONE_LEVEL
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
    DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS -DGRAMMAR=sql
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "unofficial-tree-sitter-sql")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
