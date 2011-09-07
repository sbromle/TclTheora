#!/usr/bin/env wish

lappend auto_path /usr/local/lib;
package require tcltheora;

if {[llength $argv]==0} {
	puts "Usage: $argv0 ogg_theora_file.ogv";
	exit 0;
}

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

proc do_loop {t p fn fd {lastms 0}} {
	# calculate frame delay (warning! susceptible to accumulated drift!);
	if {[$t next $p]!=0} {
		set ms [clock milliseconds];
		set dt [expr {$ms-$lastms}];
		set delay [expr {int($fd*1000.0/$fn)}];
		if {$delay-$dt<0} {
			$t next $p; # immediately draw this frame;
			puts stderr $dt;
			after $delay [list do_loop $t $p $fn $fd $ms];
		} else {
			after [expr {$delay-$dt}] [list do_loop $t $p $fn $fd $ms];
		}
	}
}

lassign [make_gui .] w photo;
set t [theora new [lindex $argv 0]];
puts "Theora object $t created.";
lassign [$t frameRate] fn fd;
puts "Theora object $t frameRate = $fn/$fd.";
#do_loop $t $photo $fn $fd;
set ms [clock milliseconds];
set dtsum 0;
set nframes 0;
while {[$t next $photo]!=0} {
	set m1 [clock milliseconds];
	update idletasks;
	set dtsum [expr {$dtsum+($m1-$ms)}];
	incr nframes;
	set ms $m1;
}
puts stderr "Average delay was [expr {$dtsum*1.0/$nframes}]";
puts stderr "Desired delay was [expr {$fd*1000.0/$fn}]";






