function re_search
	set -l tmp (mktemp -t fish.XXXXXX)
	set -x SEARCH_BUFFER (commandline -b)
	re-search > $tmp
	commandline -f repaint
	if [ -s $tmp ]
		commandline -r (cat $tmp)
	end
	rm -f $tmp
end