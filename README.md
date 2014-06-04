re-search
=========

Basic incremental history search, implemented to be used with fish shell.

It doesn't support all the terminal implementations because of some ANSI
escape sequences used, and is only tested on GNU/Linux.

* Compile and add the binary to your PATH.
* Copy the file `re_search.fish` to the directory `~/.config/fish/functions/`.
* Add the binding to `~/.config/fish/functions/fish_user_key_bindings.fish`:
```
bind \cr re_search
```

Internal bindings:

* C-r, up, pg-up for backward search.
* C-e, down, pg-down for forward search.
* C-c, left, esc(2x) to cancel search.
* Enter, right to accept result.
