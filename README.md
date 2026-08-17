# What's this?
This is a runtime memory-safety solution

# Build
```
bash build.sh
cd build && make
```

# Products
```
./build/src/libsanitypass.so
./build/src/libsanity.a
./build/include/sanity
```

# How to apply to my project?
You need to complete several key steps:
1. Use the clang compiler and specify the libsanitypass.so plugin
2. Link the target program with libsanity.a
3. Implement a memory allocator in your planned address space and colorize it with sanity_poison/sanity_unpoison when necessary

For more information, please refer to test_sanity

# License

ob-sanity is licensed under the [Apache License, Version 2.0](LICENSE).
See [NOTICE](NOTICE) for attribution information.
