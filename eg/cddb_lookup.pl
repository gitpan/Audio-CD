use strict;
use Audio::CD ();

my $id = Audio::CD->init;
my $cddb = $id->cddb;
my $info = $id->stat;

printf "cddb id = %lx, total tracks = %d\n", 
  $cddb->discid, $info->total_tracks;

my $data = $cddb->lookup;

printf "%s / %s [%s]\n", $data->artist, $data->title, $data->genre;

my $tracks = $data->tracks($info);
for my $track (@$tracks) {
    print $track->name, "\n";
}
