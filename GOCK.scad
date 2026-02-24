sides_per_type = [6, 4, 5, 7, 3];
seperation = 50;

module hole(depth, r=1.6, c=0.5) {
    rotate_extrude($fn=32) difference() {
        translate([0, -depth]) square([r+c, depth+c]);
        translate([r+c, -c]) circle(c);
        translate([r, -depth]) square([c+1, depth-c]);
    }
    translate([0,0,depth*-.5]) cube([r*10,0.2,depth], center=true);
}
module pole(length, r=3, fn=6) {
    fudge = 1/cos(180/fn);
    linear_extrude(length) rotate(180 / fn) circle(r=r * fudge, $fn=fn);
}

module poleWithHole(l, d, fn=6, hr=1.5) {
    render(convexity=2) difference() {
        pole(l, fn=fn);
        translate([0, 0, l]) hole(d, r=hr);
    }
}

module connection(type) {
    poleWithHole(15, 8, fn=sides_per_type[type]);
}

module connector(label) {
    difference() {
        union() { children(); }
        translate([0,0, -10]) cube([50, 50, 20], center=true);
        linear_extrude(0.5) text(label, 5, halign="center", valign="center");
    }
}

include <export.scad>;
