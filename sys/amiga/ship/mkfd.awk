# mkfd.awk
# make filldir script from shipping list
function chknam(x){
	if( ! (x in dirlist) ){
		dirlist[x]=x
#		print "if not exists <prefix>-" x > "filldir"
#		print "	makedir <prefix>-" x > "filldir"
#		print "endif" > "filldir"
	}
	}
BEGIN	{
	print ".key prefix/a" > "filldir"
	print "; This file generated by mkfd.awk - do not edit!" > "filldir"

	# kludge to avoid a proper but complex chknam to deal with subdirs
	# d is also broken
	print "if not exists <prefix>-1" > "filldir"
	print " makedir <prefix>-1" > "filldir"
	print "endif" > "filldir"
	print "if not exists <prefix>-2" > "filldir"
	print " makedir <prefix>-2" > "filldir"
	print "endif" > "filldir"

	print "if not exists <prefix>-2/HackExe" > "filldir"
	print " makedir <prefix>-2/HackExe" > "filldir"
	print "endif" > "filldir"
	print "if not exists <prefix>-1/NetHack" > "filldir"
	print " makedir <prefix>-1/NetHack" > "filldir"
	print "endif" > "filldir"

	print "if not exists <prefix>-2/HackExe2" > "filldir"
	print " makedir <prefix>-2/HackExe2" > "filldir"
	print "endif" > "filldir"

	print "if not exists <prefix>-1/NetHack/hack" > "filldir"
	print " makedir <prefix>-1/NetHack/hack" > "filldir"
	print "endif" > "filldir"

	print "if not exists <prefix>-1/NetHack/sounds" > "filldir"
	print " makedir <prefix>-1/NetHack/sounds" > "filldir"
	print "endif" > "filldir"

	}
/^#/	{}
/^f/	{
	chknam($3)
	print "cmove nethack:" $2 " <prefix>-" $3 "/" $4 "/" $2 > "filldir"
	}
/^B/	{
	chknam($3)
	print "slink nethack:" $2 " to <prefix>-" $3 "/" $4 "/" $2 " ND" > "filldir"
	}
/^E/	{
	chknam($3)
	print "copy NIL: <prefix>-" $3 "/" $4 "/" $2 > "filldir"
	}
/^F/	{
	chknam($3)
	if(sub(":","",$4)){
		div=":"
	} else {
		div ="/"
	}
	print "cmove " $4 div $2 " <prefix>-" $3 "/" $4 "/" $2 > "filldir"
	}
#/^r/	{
#	chknam($3)
#	print "cmove nethack:" $2 " <prefix>-" $3 "/" $4 > "filldir"
#	}
#/^R/	{
#	chknam($3)
#	print "cmove " $4 "/" $2 " <prefix>-" $3 "/" $5 > "filldir"
#	}
/^d/	{
	chknam($3)
	chknam($3 "/" $2)
# DO WE NEED A DUMMY FILE HERE?
# NO - do it in shiplist
#	print "if not exists <prefix>-" $3 "/" $2 > "filldir"
#	print "	makedir <prefix>-" $3 > "filldir"
#	print "endif" > "filldir"
	}
/^S/	{
	print "blink nethack:nethack to ram:nethack ND" > "filldir"
	print "set here=`cd`" > "filldir"
	print "cd ram:" > "filldir"
	print "amiga:splitter/splitter nethack" > "filldir"
	print "cd $here" > "filldir"
	}
/^[^f#dFrRBES]/{		#out of date
	print "line " $0 " rejected - bad type"
	}
