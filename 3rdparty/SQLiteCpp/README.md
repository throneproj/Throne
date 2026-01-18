SQLiteC++
---------

[![release](https://img.shields.io/github/release/SRombauts/SQLiteCpp.svg)](https://github.com/SRombauts/SQLiteCpp/releases)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/SRombauts/SQLiteCpp/blob/master/LICENSE.txt)
[![Travis CI Linux Build Status](https://travis-ci.org/SRombauts/SQLiteCpp.svg?branch=master)](https://travis-ci.org/SRombauts/SQLiteCpp "Travis CI Linux Build Status")
[![AppVeyor Windows Build status](https://ci.appveyor.com/api/projects/status/github/SRombauts/SQLiteCpp?svg=true)](https://ci.appveyor.com/project/SbastienRombauts/SQLiteCpp "AppVeyor Windows Build status")
[![GitHub Actions Build status](https://github.com/SRombauts/SQLiteCpp/workflows/build/badge.svg)](https://github.com/SRombauts/SQLiteCpp/actions "GitHhub Actions Build status")
[![Coveralls](https://img.shields.io/coveralls/SRombauts/SQLiteCpp.svg)](https://coveralls.io/github/SRombauts/SQLiteCpp "Coveralls test coverage")
[![Coverity](https://img.shields.io/coverity/scan/14508.svg)](https://scan.coverity.com/projects/srombauts-sqlitecpp "Coverity Scan Build Status")
[![Join the chat at https://gitter.im/SRombauts/SQLiteCpp](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/SRombauts/SQLiteCpp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

SQLiteC++ (SQLiteCpp) is a lean and easy to use C++ SQLite3 wrapper.

<!--Keywords: sqlite, sqlite3, C, library, wrapper C++-->
<meta name="keywords" content="sqlite, sqlite3, C, library, wrapper C++">

## About SQLiteC++:

SQLiteC++ offers an encapsulation around the native C APIs of SQLite,
with a few intuitive and well documented C++ classes.

### License:

Copyright (c) 2012-2025 Sébastien Rombauts (sebastien.rombauts@gmail.com)

Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
or copy at http://opensource.org/licenses/MIT)

#### Note on redistribution of SQLite source files

As stated by the MIT License, you are welcome to reuse, modify, and redistribute the SQLiteCpp source code
the way you want it to, be it a git submodule, a subdirectory, or a selection of some source files.

I would love a mention in your README, a web link to the SQLite repository, and a mention of the author,
but none of those are mandatory.

### About SQLite underlying library:

SQLite is a library that implements a serverless transactional SQL database engine.
It is the most widely deployed SQL database engine in the world.
All of the code and documentation in SQLite has been dedicated to the public domain by the authors.
[http://www.sqlite.org/about.html](http://www.sqlite.org/about.html)

### The goals of SQLiteC++ are:

- to offer the best of the existing simple C++ SQLite wrappers
- to be elegantly written with good C++11 design, STL, exceptions and RAII idiom
- to keep dependencies to a minimum (C++11 STL and SQLite3)
- to be portable
- to be light and fast
- to be thread-safe only as much as SQLite "Multi-thread" mode (see below)
- to have a good unit test coverage
- to use API names sticking with those of the SQLite library
- to be well documented with Doxygen tags, and with some good examples
- to be well maintained
- to use a permissive MIT license, similar to BSD or Boost, for proprietary/commercial usage

It is designed using the Resource Acquisition Is Initialization (RAII) idiom
(see [http://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization](http://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization)),
and throwing exceptions in case of SQLite errors (except in destructors,
where assert() are used instead).
Each SQLiteC++ object must be constructed with a valid SQLite database connection,
and then is always valid until destroyed.

### Supported platforms:

Now requires a C++11 compiler. Use branch [sqlitecpp-2.x](https://github.com/SRombauts/SQLiteCpp/tree/sqlitecpp-2.x) for latest pre-C++11 developments.

Developments and tests are done under the following OSs:
- Ubuntu 14.04, 16.04 and 18.04 (Travis CI and Github Actions)
- Windows 10, and Windows Server 2012 R2, Windows Server 2016, Windows Server 2022 (AppVeyor and Github Actions)
- MacOS 10.11 and 11.7 (Travis CI and Github Actions)
- Valgrind memcheck tool

And the following IDEs/Compilers
- GCC 4.8.4, 5.3.0, 7.1.1 and latest eg 9.4 (C++11, C++14, C++17)
- Clang 5 and 7 (Travis CI)
- AppleClang 8, 9 and 13 (Travis CI and Github Actions)
- Xcode 8 & 9 (Travis CI)
- Visual Studio Community/Entreprise 2022, 2019, 2017, and 2015 (AppVeyor and Github Actions)

### Dependencies

- a modern C++11 STL implementation with GCC, Clang, or Visual Studio 2015
- exception support (the class Exception inherits from std::runtime_error)
- the SQLite library (3.7.15 minimum from 2012-12-12) either by linking to it dynamically or statically (install the libsqlite3-dev package under Debian/Ubuntu/Mint Linux),
  or by adding its source file in your project code base (source code provided in src/sqlite3 for Windows),
  with the `SQLITE_ENABLE_COLUMN_METADATA` macro defined (see http://www.sqlite.org/compile.html#enable_column_metadata).

## Getting started
### Installation

To use this wrapper, you need to add the SQLiteC++ source files from the src/ directory
in your project code base, and compile/link against the sqlite library.

The easiest way to do this is to add the wrapper as a library.
The "CMakeLists.txt" file defining the static library is provided in the root directory,
so you simply have to add_subdirectory(SQLiteCpp) to you main CMakeLists.txt
and link to the "SQLiteCpp" wrapper library.

Example for Linux: 
```cmake
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/thirdparty/SQLiteCpp)

add_executable(main src/main.cpp)
target_link_libraries(main
  SQLiteCpp
  sqlite3
  pthread
  dl
  )
``` 
Thus this SQLiteCpp repository can be directly used as a Git submodule.
See the [SQLiteCpp_Example](https://github.com/SRombauts/SQLiteCpp_Example) side repository for a standalone "from scratch" example.

Under Debian/Ubuntu/Mint Linux, you can install the libsqlite3-dev package if you don't want to use the embedded sqlite3 library.

### Building example and unit-tests:

Use git to clone the repository. Then init and update submodule "googletest".

```Shell
git clone https://github.com/SRombauts/SQLiteCpp.git
cd SQLiteCpp
git submodule init
git submodule update
```

### Installing SQLiteCpp (vcpkg)

Alternatively, you can build and install SQLiteCpp using [vcpkg](https://github.com/Microsoft/vcpkg/) dependency manager:

```bash or powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install sqlitecpp
```

The SQLiteCpp port in vcpkg is kept up to date by Microsoft team members and community contributors. If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.


#### Using SQLiteCpp on a system-wide installation

If you installed this package to your system, a `SQLiteCppConfig.cmake` file will be generated & installed to your system.  
This file lets you link against the SQLiteCpp library for use in your Cmake project.

Here's an example of using this in your CMakeLists.txt
```cmake
# You can optionally define a minimum version in this call
find_package(SQLiteCpp REQUIRED)
# For this example, lets say you created an target with add_executable (or add_library) called "my_target"
# You can optionally declare PUBLIC or PRIVATE linkage here, depending on your needs.
target_link_libraries(my_target PRIVATE SQLiteCpp)
```

#### CMake and tests
A CMake configuration file is also provided for multi-platform support and testing.

Typical generic build for MS Visual Studio under Windows (from [build.bat](build.bat)):

```Batchfile
mkdir build
cd build

cmake ..        # cmake .. -G "Visual Studio 16 2019"    # for Visual Studio 2019
@REM Generate a Visual Studio solution for latest version found
cmake -DSQLITECPP_BUILD_EXAMPLES=ON -DSQLITECPP_BUILD_TESTS=ON ..

@REM Build default configuration (ie 'Debug')
cmake --build .

@REM Build and run tests
ctest --output-on-failure
```

Generating the Linux Makefile, building in Debug and executing the tests (from [build.sh](build.sh)):

```Shell
mkdir Debug
cd Debug

# Generate a Makefile for GCC (or Clang, depanding on CC/CXX envvar)
cmake -DSQLITECPP_BUILD_EXAMPLES=ON -DSQLITECPP_BUILD_TESTS=ON ..

# Build (ie 'make')
cmake --build .

# Build and run unit-tests (ie 'make test')
ctest --output-on-failure
```

#### Building with meson

You can build SQLiteCpp with [meson](https://mesonbuild.com/) using the provided meson project.

you can install meson using pip: `pip install meson` however you may need to install ninja and other dependencies depending on your platform as an compiler toolchain

Arch Linux:

```sh
# install clang (compiler toolchain) and ninja (recommended build system)
sudo pacman -Syu clang ninja
# install python and pip (required for meson)
sudo pacman -Syu python python-pip
# install meson 
pip install meson
```

Ubuntu:

```sh
# install gcc(compiler toolchain) and ninja (recommended build system)
sudo apt install build-essential ninja-build
# install python and pip (required for meson)
sudo apt install python3 python3-pip
# install meson
pip install meson
```

for example you can build the library using the default options with:

```sh
# setup the build directory
meson setup builddir 
# build sqlitecpp
meson compile -C builddir
```

or if you wish to build with tests and examples:

```sh
# setup the build directory with tests and examples enabled
meson setup builddir -DSQLITECPP_BUILD_TESTS=true -DSQLITECPP_BUILD_EXAMPLES=true
# build sqlitecpp
meson compile -C builddir
```

#### Using SQLiteCpp as subproject in meson

please check the examples in the examples folder for usage of SQLiteCpp as a subproject in meson, as for the wrap file you can use the one provided in the subprojects folder called `SQLiteCpp.wrap`

> keep in mind that even that this wrap should be up to date, it is recommended to check the latest version of SQLiteCpp and update the wrap file accordingly

#### System SQLiteCpp support under meson

additionally meson can detect and use the bundled sqlitecpp library included on your system if available, for example with vcpkg you would need to set the `PKG_CONFIG_PATH` environment variable to the vcpkg directory before running meson setup, and if applies the corresponding `PKG-CONFIG` executable to the path.

#### Building the Doxygen/html documentation

Make sure you have Dogygen installed and configure CMake using the `SQLITECPP_RUN_DOXYGEN=ON` flag:

```
cmake -DSQLITECPP_RUN_DOXYGEN=ON   <MORE ARGUMENTS_HERE>
```

and then execute the `SQLiteCpp_doxygen` target (or build all targets, see above).
The documentation will be generated in the 'doc' subfolder of the source tree.

#### CMake options

  * For more options on customizing the build, see the [CMakeLists.txt](https://github.com/SRombauts/SQLiteCpp/blob/master/CMakeLists.txt) file.

#### Troubleshooting

Under Linux, if you get multiple linker errors like "undefined reference to sqlite3_xxx",
it's that you lack the "sqlite3" library: install the libsqlite3-dev package.

If you get a single linker error "Column.cpp: undefined reference to sqlite3_column_origin_name",
it's that your "sqlite3" library was not compiled with
the `SQLITE_ENABLE_COLUMN_METADATA` macro defined (see [http://www.sqlite.org/compile.html#enable_column_metadata](http://www.sqlite.org/compile.html#enable_column_metadata)).
You can:
 - either recompile the sqlite3 library provided by your distribution yourself (seek help online)
 - or turn off the `option(SQLITE_ENABLE_COLUMN_METADATA "Enable Column::getColumnOriginName(). Require support from sqlite3 library." ON)` in [CMakeFiles.txt](CMakeFiles.txt) (or other build system scripts)
 - or turn on the `option(SQLITECPP_INTERNAL_SQLITE "Add the internal SQLite3 source to the project." ON)` in [CMakeFiles.txt](CMakeFiles.txt)

### Continuous Integration

This project is continuously tested under Ubuntu Linux with the gcc and clang compilers
using the Travis CI community service with the above CMake building and testing procedure.
It is also tested in the same way under Windows Server 2012 R2 with Visual Studio 2013 compiler
using the AppVeyor continuous integration service.

Detailed results can be seen online:
 - [https://travis-ci.org/SRombauts/SQLiteCpp](https://travis-ci.org/SRombauts/SQLiteCpp)
 - [https://ci.appveyor.com/project/SbastienRombauts/SQLiteCpp](https://ci.appveyor.com/project/SbastienRombauts/SQLiteCpp)

### Thread-safety

SQLite supports three modes of thread safety, as describe in "SQLite And Multiple Threads":
see [http://www.sqlite.org/threadsafe.html](http://www.sqlite.org/threadsafe.html)

This SQLiteC++ wrapper does no add any locks (no mutexes) nor any other thread-safety mechanism
above the SQLite library itself, by design, for lightness and speed.

Thus, SQLiteC++ naturally supports the "Multi Thread" mode of SQLite:
"In this mode, SQLite can be safely used by multiple threads
provided that no single database connection is used simultaneously in two or more threads."

But SQLiteC++ does not support the fully thread-safe "Serialized" mode of SQLite,
because of the way it shares the underlying SQLite precompiled statement
in a custom shared pointer (See the inner class "Statement::Ptr").

### Valgrind memcheck

Run valgrind to search for memory leaks in your application, the SQLiteCpp wrapper, or the sqlite3 library.
Execute the following command under Unix like OS (Linux, MacOS or WSL2/Ubuntu under Windows Subsystem for Linux):

```Shell
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose build/SQLiteCpp_example1
```

or uncoment the line at the end of [build.sh](build.sh)

## Examples
### The first sample demonstrates how to query a database and get results: 

```C++
try
{
    // Open a database file
    SQLite::Database    db("example.db3");
    
    // Compile a SQL query, containing one parameter (index 1)
    SQLite::Statement   query(db, "SELECT * FROM test WHERE size > ?");
    
    // Bind the integer value 6 to the first parameter of the SQL query
    query.bind(1, 6);
    
    // Loop to execute the query step by step, to get rows of result
    while (query.executeStep())
    {
        // Demonstrate how to get some typed column value
        int         id      = query.getColumn(0);
        const char* value   = query.getColumn(1);
        int         size    = query.getColumn(2);
        
        std::cout << "row: " << id << ", " << value << ", " << size << std::endl;
    }
}
catch (std::exception& e)
{
    std::cout << "exception: " << e.what() << std::endl;
}
```

### The second sample shows how to manage a transaction:

```C++
try
{
    SQLite::Database    db("transaction.db3", SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);

    db.exec("DROP TABLE IF EXISTS test");

    // Begin transaction
    SQLite::Transaction transaction(db);

    db.exec("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");

    int nb = db.exec("INSERT INTO test VALUES (NULL, \"test\")");
    std::cout << "INSERT INTO test VALUES (NULL, \"test\")\", returned " << nb << std::endl;

    // Commit transaction
    transaction.commit();
}
catch (std::exception& e)
{
    std::cout << "exception: " << e.what() << std::endl;
}
```

### The third sample shows how to manage a prepared statement with a transaction:

```C++
try 
{ 
    SQLite::Database    db("test.db3", SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);

    db.exec("DROP TABLE IF EXISTS test");

    db.exec("CREATE TABLE test (value INTEGER)");

    // Begin transaction
    SQLite::Transaction transaction(db);

    // Prepare query
    SQLite::Statement query {db, "INSERT INTO test (value) VALUES (?)"};

    // Collection to save in database
    std::vector<int> values{1, 2, 3};

    for (const auto& v: values)
    {
      query.bind(1, v);
      query.exec();
      query.reset();
    }

    // Commit transaction
    transaction.commit();
}
catch (std::exception& e)
{
  std::cout << "exception: " << e.what() << std::endl;
}
```

### How to handle assertion in SQLiteC++:
Exceptions shall not be used in destructors, so SQLiteC++ uses SQLITECPP_ASSERT() to check for errors in destructors.
If you don't want assert() to be called, you have to enable and define an assert handler as shown below,
and by setting the flag SQLITECPP_ENABLE_ASSERT_HANDLER when compiling the lib.

```C++
#ifdef SQLITECPP_ENABLE_ASSERT_HANDLER
namespace SQLite
{
/// definition of the assertion handler enabled when SQLITECPP_ENABLE_ASSERT_HANDLER is defined in the project (CMakeList.txt)
void assertion_failed(const char* apFile, const long apLine, const char* apFunc, const char* apExpr, const char* apMsg)
{
    // Print a message to the standard error output stream, and abort the program.
    std::cerr << apFile << ":" << apLine << ":" << " error: assertion failed (" << apExpr << ") in " << apFunc << "() with message \"" << apMsg << "\"\n";
    std::abort();
}
}
#endif
```

## How to contribute
### GitHub website
The most efficient way to help and contribute to this wrapper project is to
use the tools provided by GitHub:
- please fill bug reports and feature requests here: [https://github.com/SRombauts/SQLiteCpp/issues](https://github.com/SRombauts/SQLiteCpp/issues)
- fork the repository, make some small changes and submit them with pull-request

### Contact
You can also email me directly, I will try to answer questions and requests whenever I get the time for it.

### Coding Style Guidelines
The source code use the CamelCase naming style variant where:
- type names (class, struct, typedef, enums...) begin with a capital letter
- files (.cpp/.h) are named like the class they contain
- function and variable names begin with a lower case letter
- member variables begin with a 'm', function arguments begin with a 'a', booleans with a 'b', pointers with a 'p'
- each file, class, method and member variable is documented using Doxygen tags
- braces on their own line
See also [http://www.appinf.com/download/CppCodingStyleGuide.pdf](http://www.appinf.com/download/CppCodingStyleGuide.pdf) for good guidelines

## See also - Some other simple C++ SQLite wrappers:

See bellow a short comparison of other wrappers done at the time of writing:
 - [sqdbcpp](http://code.google.com/p/sqdbcpp/): RAII design, simple, no dependencies, UTF-8/UTF-16, new BSD license
 - [sqlite3cc](http://ed.am/dev/sqlite3cc): uses boost, modern design, LPGPL
 - [sqlite3pp](https://github.com/iwongu/sqlite3pp): modern design inspired by boost, MIT License
 - [SQLite++](http://sqlitepp.berlios.de/): uses boost build system, Boost License 1.0 
 - [CppSQLite](http://www.codeproject.com/Articles/6343/CppSQLite-C-Wrapper-for-SQLite/): famous Code Project but old design, BSD License 
 - [easySQLite](http://code.google.com/p/easysqlite/): manages table as structured objects, complex 
 - [sqlite_modern_cpp](https://github.com/keramer/sqlite_modern_cpp): modern C++11, all in one file, MIT license
 - [sqlite_orm](https://github.com/fnc12/sqlite_orm): modern C++14, header only all in one file, no raw string queries, BSD-3 license
