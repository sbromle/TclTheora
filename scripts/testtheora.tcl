#!/usr/bin/env wish

lappend auto_path /usr/local/lib;
package require tcltheora;

proc make_gui {w} {
	set c [canvas $w.c]
	set x [scrollbar $w.sx -ori hori -command [list $c xview]]
	set y [scrollbar $w.sy -ori vert -command [list $c yview]]
	$c config -xscrollcommand [list $x set] -yscrollcommand [list $y set]
	set i [image create photo]
	$c create image 0 0 -image $i -anchor nw -tag img
	grid $c $y -in $w -sticky news
	grid $x -in $w -sticky ew
	grid rowconfig $w 0 -weight 1
	grid columnconfig $w 0 -weight 1
	$c config -scrollregion [$c bbox all]
	$c config -cursor trek
	return [list $w $i];
}

lassign [make_gui .] w photo;
lassign [theora new [lindex $argv 0]] t;
puts "Theora object $t created.";
$t next $photo;
for {set i 0} {$i<10} {incr i} {
	$t next $photo;
	update idletasks
	after 1000;
}





