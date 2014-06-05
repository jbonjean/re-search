re-search
=========

Basic incremental history search, implemented to be used with fish shell.

It doesn't support all the terminal implementations because of some ANSI
escape sequences used, and is only tested on GNU/Linux.

### Install

* Compile and add the binary to your PATH.
* Copy the file `re_search.fish` to the directory `~/.config/fish/functions/`.
* Add the binding to `~/.config/fish/functions/fish_user_key_bindings.fish`:
```
bind \cr re_search
```

### Internal key bindings

* C-r, up, pg-up: backward search.
* C-e, down, pg-down: forward search.
* C-c, left, esc, home: cancel search.
* Enter, right, end: accept result.

### Customize the prompt

* Stop tracking changes on `config.h`
```
git update-index --assume-unchanged config.h
```
* Redefine the prompt macro in `config.h`, for example, a very simple prompt
could look like:
```
#define PROMPT(buffer, direction, index, result) \
        do { fprintf(stderr, "%s", result); } while (0)
```
or, a more compact version of the native prompt:
```
#define PROMPT(buffer, direction, index, result) \
        do { \
        	fprintf(stderr, "[%c%d] %s", direction[0], index, buffer); \
        	if (index > 0) {\
	        	int i = 0; \
        		i = fprintf(stderr, " > %s", result); \
        		fprintf(stderr, "\033[%dD", i); \
        	} \
        } while (0)
 ```
* Recompile
