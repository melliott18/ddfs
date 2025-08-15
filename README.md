# DDFS: Deduplicating Key-Value Block Storage System

DDFS is a custom block-based file system designed for deduplication and efficient storage management. It provides a simple key-value interface on top of a virtual disk, supporting basic file system operations and block-level deduplication. This project is intended for educational and experimental use on Unix-like systems (e.g., FreeBSD, macOS).

## Requirements

- Unix-like OS (FreeBSD or macOS recommended)
- C compiler (e.g., gcc or clang)
- `make` utility
- Root privileges for disk operations

## Directory Structure

- `src/` — Source code for the file system and utilities
	- `ddfs.c`, `ddfs.h` — Core file system logic and definitions
	- `ddfs_inode.c`, `ddfs_inode.h` — Inode management
	- `ddfs_bitmap.c`, `ddfs_bitmap.h` — Bitmap management
	- `makefs-ddfs.c` — Tool to initialize a DDFS image
	- `Makefile` — Build script for the main tools
- `test/` — Test suite for DDFS
	- `ddfs_test.c` — Test program for DDFS images
	- `Makefile` — Build script for tests

## Building

To build the file system tools and tests:

```
cd src
make
cd ../test
make
```

## Usage Example

After creating and initializing a memory disk as described above, you can use the test suite to verify the file system:

```
./ddfs_test <image-file>
```

For more advanced usage, see the source code and comments in `src/` and `test/`.

## License

This project is provided for educational purposes. See the source files for copyright and licensing details.

## Author

Maintained by melliott18 (https://github.com/melliott18)

## How to create a memory disk (1GB example)

### Run:

```
sudo mdconfig -s 1g -u md0
sudo chmod 1777 /dev/md0
```

## How to initialize the file system

### Run:

```
cd src
make
./makefs-ddfs <image-file>
```

## How to test the file system

### Run:

```
cd test
make
./ddfs_test <image-file>
```
