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

#### Duplicate history entries

Because of performance issues with large history file, the duplicate check is
disabled by default. If you want to enable it, just pass the correct cflag to
make:

```
make CFLAGS=-DCHECK_DUPLICATES
```

#### Sub-search (experimental)

This feature allows you to do a new search in the current results.

For example, you are looking for the git-clone command of couchbase repository,
the following sequence shows how it could help you:

* [C-r] clone
* [C-u]
* couchbase

### Internal key bindings

* C-r, up, pg-up: backward search.
* C-s, down, pg-down: forward search.
* C-c, left, esc, home: cancel search.
* C-e, right, end: accept result.
* C-u: save search result and start sub-search.
* Enter: execute result.

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
#define PROMPT(buffer, saved, direction, index, result) \
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

### Bash support (experimental)

* Switch to Bash support with the compilation flag:
```
make CFLAGS=-DBASH
```
* If it's not already configured, it is better to make bash save commands to
  the history file in realtime:
```
shopt -s histappend
PROMPT_COMMAND="history -a;$PROMPT_COMMAND"
```
* Also, because ctrl-s is used for forward search, you may need to disable flow control:
```
stty -ixon
```
#### Add the binding:

```
re_search() {
	SEARCH_BUFFER="$READLINE_LINE" re-search > /tmp/.re-search
	res="$?"
	[ -s /tmp/.re-search ] || return 1
	READLINE_LINE="$(cat /tmp/.re-search)"
	READLINE_POINT=${#READLINE_LINE}
	return $res
}
bind -x '"\C-r":"if re_search; then xdotool key KP_Enter; fi"'
```
