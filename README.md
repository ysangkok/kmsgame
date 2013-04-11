1. Make sure kmscon builds (if syntax error on `.y` file: get bison. On libxkbcommon issues: http://superuser.com/a/581412/33303)
1. Link `kmsconiface.c` to `kmscon/tests`
1. Patch `Makefile.am`:

```
+test_kmsiface_SOURCES = \
+       $(test_sources) \
+       tests/kmsconiface.c
+test_kmsiface_CPPFLAGS = $(test_cflags) $(PIXMAN_CFLAGS)
+test_kmsiface_LDADD = \
+       $(test_libs) \
+       libuterm.la \
+       $(PIXMAN_LIBS) \
+       -ldl
```

1. In `kmscon`: `automake` and `make check`
1. Build this
1. Symlink `game.so` to `kmscon/.libs`
1. `test_kmsiface` now runnable
