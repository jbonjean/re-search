function re_search
	set -l tmp (mktemp -t fish.XXXXXX)
	set -x SEARCH_BUFFER (commandline -b)
	re-search > $tmp
	set -l res $status
	commandline -f repaint
	if [ -s $tmp ]
		commandline -r (cat $tmp)
	end
	if [ $res = 0 ]
		commandline -f execute
	end
	rm -f $tmp
end
