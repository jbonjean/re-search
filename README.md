re-search
=========

Basic incremental history search, implemented to be used with fish shell.

It doesn't support all the terminal implementations because of some ANSI
escape sequences used, and is only tested on GNU/Linux.

* Compile and add the binary to your path.
* Exemple of fish configuration to bind to C-r (like zsh or bash):


```
function rsearch
	set -l tmp (mktemp -t fish.XXXXXX)
	set -x SEARCH_BUFFER (commandline -b)
	re-search > $tmp
	commandline -f repaint
	if [ -s $tmp ]
	    commandline -r (cat $tmp)
	end
	rm -f $tmp
end


function fish_user_key_bindings
	bind \cr rsearch
end
```


Bindings:

* C-r, up, pg-up for backward search.
* C-e, down, pg-down for forward search.
* C-c, left, esc(2x) to cancel search.
* Enter, right to accept result.

NOTE: I am not a C developer so please don't complain about the code, fork it
and submit a pull request instead.
