#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "cdaudio.h"

typedef struct disc_info * Audio__CD__Info;
typedef struct disc_data * Audio__CD__Data;
typedef struct track_data * Audio__CD__Track;
typedef int Audio__CD;

void cddb_lookup(int cd_desc, struct disc_data *data);
#define cd_cddb_lookup cddb_lookup

#define cd_init(sv_class, device) cd_init_device(device)

#define CD_Info_total_tracks(info) info->disc_total_tracks

#define CD_Data_title(data) data->data_title
#define CD_Data_artist(data) data->data_artist
#define CD_Data_extended(data) data->data_extended
#define CD_Data_genre(data) cddb_genre(data->data_genre)

#define CD_Track_name(track) track->track_name
#define CD_Track_artist(track) track->track_artist
#define CD_Track_extended(track) track->track_extended

static SV *CD_Data_track_new(struct track_data *td)
{
    SV *sv = newSV(0);
#if 0
    struct track_data *new_td = (struct track_data *)safemalloc(sizeof(*new_td));
    Copy(td, new_td, 1, struct track_data);
#endif
    sv_setref_pv(sv, "Audio::CD::Track", (void*)td);
    return sv;
}

MODULE = Audio::CD   PACKAGE = Audio::CD   PREFIX = cd_

Audio::CD
cd_init(sv_class, device="/dev/cdrom")
    SV *sv_class
    char *device

Audio::CD::Info
cd_stat(cd_desc)
    Audio::CD cd_desc

    CODE:
    RETVAL = (Audio__CD__Info)safemalloc(sizeof(*RETVAL));
    cd_stat(cd_desc, RETVAL);

    OUTPUT:
    RETVAL

Audio::CD::Data
cd_cddb_lookup(cd_desc)
    Audio::CD cd_desc

    CODE:
    RETVAL = (Audio__CD__Data)safemalloc(sizeof(*RETVAL));
    cd_cddb_lookup(cd_desc, RETVAL);

    OUTPUT:
    RETVAL

MODULE = Audio::CD   PACKAGE = Audio::CD::Info   PREFIX = CD_Info_

int
CD_Info_total_tracks(info)
   Audio::CD::Info info

void
DESTROY(info)
   Audio::CD::Info info

   CODE:
   safefree(info);

MODULE = Audio::CD   PACKAGE = Audio::CD::Data   PREFIX = CD_Data_

char *
CD_Data_title(data)
   Audio::CD::Data data

char *
CD_Data_artist(data)
   Audio::CD::Data data

char *
CD_Data_extended(data)
   Audio::CD::Data data

char *
CD_Data_genre(data)
   Audio::CD::Data data

AV *
CD_Data_tracks(data, disc)
   Audio::CD::Data data
   Audio::CD::Info disc

   PREINIT:
   int track;

   CODE:
   RETVAL = newAV();
   for(track = 0; track < disc->disc_total_tracks; track++) {
       av_push(RETVAL, CD_Data_track_new(&data->data_track[track]));
   }

   OUTPUT:
   RETVAL

void
DESTROY(data)
   Audio::CD::Data data

   CODE:
   safefree(data);

MODULE = Audio::CD   PACKAGE = Audio::CD::Track   PREFIX = CD_Track_

char *
CD_Track_name(track)
    Audio::CD::Track track

char *
CD_Track_artist(track)
    Audio::CD::Track track

char *
CD_Track_extended(track)
    Audio::CD::Track track

MODULE = Audio::CD   PACKAGE = Audio::CDDB   PREFIX = cddb_

PROTOTYPES: disable


