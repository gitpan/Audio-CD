/*
cdcd - Command Driven CD player
Copyright (C)1998-99 Tony Arcieri

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cdaudio.h"
#include "stdio.h"

#define PACKAGE "Audio::CD"

static int verbosity = 0;
static int timestamp = 0;

/* Timestamped discid */
static int timestamped_discid = 0;

int
#if __STDC__
cdcd_cd_stat(int cd_desc, struct disc_info *disc)
#else
cdcd_cd_stat(cd_desc, disc)
   int cd_desc;
   struct disc_info *disc;
#endif
{
   cd_stat(cd_desc, disc);
   if(!disc->disc_present) {
      cd_close(cd_desc);
      cd_stat(cd_desc, disc);
      if(!disc->disc_present) {
	 puts("No disc in drive");
	 return -1;
      }
   }
    
   return 0;
}

void cddb_lookup(int cd_desc, struct disc_data *data)
{
   int index, serverindex, selection, sock = -1;
   struct disc_info disc;
   struct cddb_conf conf;
   struct cddb_serverlist list;
   struct cddb_server *proxy;
   struct cddb_entry entry;
   struct cddb_hello hello;
   struct cddb_query query;
   char inbuffer[256], http_string[512], discid[CDINDEX_ID_SIZE];
   
   if(cdcd_cd_stat(cd_desc, &disc) < 0)
     return;
   
   if(!verbosity)
     cddb_read_disc_data(cd_desc, data);
   else {
      cddb_stat_disc_data(cd_desc, &entry);
      
      if(entry.entry_present) {
	 if(entry.entry_timestamp == timestamp && entry.entry_id == timestamped_discid)
	   return;
	 
	 cddb_read_disc_data(cd_desc, data);
	 timestamp = entry.entry_timestamp;
	 timestamped_discid = entry.entry_id;
      } else {
	 proxy = (struct cddb_server *)malloc(sizeof(struct cddb_server));
	 cddb_read_serverlist(&conf, &list, proxy);
	 if(conf.conf_access == CDDB_ACCESS_LOCAL) {
	    free(proxy);
	    cddb_generate_unknown_entry(cd_desc, data);
	    return;
	 }
	 if(!conf.conf_proxy) {
	    free(proxy);
	    proxy = NULL;
	 } else 
	    printf("Using proxy http://%s:%d/\n", proxy->server_name, proxy->server_port);	
	      		
	 strncpy(hello.hello_program, PACKAGE, 256);
	 strncpy(hello.hello_version, VERSION, 256);
	 
	 serverindex = 0;
	      
         do {
	    switch(list.list_host[serverindex].host_protocol) {
	     case CDDB_MODE_CDDBP:
	       printf("Trying CDDB server cddbp://%s:%d/\n", list.list_host[serverindex].host_server.server_name, list.list_host[serverindex].host_server.server_port);
	       sock = cddb_connect_server(&list.list_host[serverindex++], proxy, &hello);
	       break;
	     case CDDB_MODE_HTTP:
	       printf("Trying CDDB server http://%s:%d/%s\n", list.list_host[serverindex].host_server.server_name, list.list_host[serverindex].host_server.server_port, list.list_host[serverindex].host_addressing);
	       sock = cddb_connect_server(&list.list_host[serverindex++], proxy, &hello, http_string, 512);
	       break;
	     case CDINDEX_MODE_HTTP:
	       printf("Trying CD Index server http://%s:%d/%s\n", list.list_host[serverindex].host_server.server_name, list.list_host[serverindex].host_server.server_port, list.list_host[serverindex].host_addressing);
	       sock = cdindex_connect_server(&list.list_host[serverindex++], proxy, http_string, 512);
	       break;
	     default:
	       puts("Invalid protocol selected!");
	       return;
	    }
	    if(sock == -1) printf("Connection error: %s\n", cddb_message);
	 } while(serverindex < list.list_len && sock == -1);
	 
	 if(sock == -1) {
	    puts("Could not establish connection with any CDDB servers!");
	    if(conf.conf_proxy) free(proxy);
	    cddb_generate_unknown_entry(cd_desc, data);
	    return;
	 }
	 serverindex--;
         puts("Connection established.");
         
	 switch(list.list_host[serverindex].host_protocol) {
	  case CDDB_MODE_CDDBP:
	    printf("Retrieving information on %02lx.\n", cddb_discid(cd_desc));
            if(cddb_query(cd_desc, sock, CDDB_MODE_CDDBP, &query) < 0) { 
	       fprintf(stderr, "CDDB query error: %s", cddb_message);
	       if(conf.conf_proxy) free(proxy);
	       cddb_generate_unknown_entry(cd_desc, data);
	       return;
	    }
            break;
	  case CDDB_MODE_HTTP:
	    printf("Retrieving information on %02lx.\n", cddb_discid(cd_desc));
	    if(cddb_query(cd_desc, sock, CDDB_MODE_HTTP, &query, http_string) < 0) {
	       fprintf(stderr, "CDDB query error: %s", cddb_message);
	       if(conf.conf_proxy) free(proxy);
	       cddb_generate_unknown_entry(cd_desc, data);
	       return;
	    }
	    shutdown(sock, 2);
	    close(sock);
		 
	    if((sock = cddb_connect_server(&list.list_host[serverindex], proxy, &hello, http_string, 512)) < 0) {
	       perror("HTTP server reconnection error");
	       if(conf.conf_proxy) free(proxy);
	       cddb_generate_unknown_entry(cd_desc, data);
	       return;
	    }
	    break;
	  case CDINDEX_MODE_HTTP:
	    cdindex_discid(cd_desc, discid, CDINDEX_ID_SIZE);
	    printf("Retrieving information on %s.\n", discid);
	    if(cdindex_read(cd_desc, sock, data, http_string) < 0) {
	       printf("No match for %s.\n", discid);
	       if(conf.conf_proxy) free(proxy);
	       cddb_generate_unknown_entry(cd_desc, data);
	       return;
	    }
	    printf("Match for %s: %s / %s\nDownloading data...\n", discid, data->data_artist, data->data_title);
	    cddb_write_disc_data(cd_desc, data);
	    return;
	 }
	 
	 if(conf.conf_proxy) free(proxy);
	 
	 if(list.list_host[serverindex].host_protocol == CDINDEX_MODE_HTTP);
	 
         switch(query.query_match) {
          case QUERY_EXACT:
	    if(strlen(query.query_list[0].list_artist) > 0)
	      printf("Match for %02lx: %s / %s\nDownloading data...\n", cddb_discid(cd_desc), query.query_list[0].list_artist, query.query_list[0].list_title);
	    else
	      printf("Match for %02lx: %s\nDownloading data...\n", cddb_discid(cd_desc), query.query_list[0].list_title);
	    entry.entry_genre = query.query_list[0].list_genre;
	    entry.entry_id = query.query_list[0].list_id;
	    switch(list.list_host[serverindex].host_protocol) {
	      case CDDB_MODE_CDDBP:
		if(cddb_read(cd_desc, sock, CDDB_MODE_CDDBP, &entry, data) < 0) {
		   perror("CDDB read error");
		   cddb_generate_unknown_entry(cd_desc, data);
		   return;
		}	
		cddb_quit(sock);
		break;
	      case CDDB_MODE_HTTP:
		if(cddb_read(cd_desc, sock, CDDB_MODE_HTTP, &entry, data, http_string) < 0) {
		   perror("CDDB read error");
		   cddb_generate_unknown_entry(cd_desc, data);
		   return;
		}
		    
		shutdown(sock, 2);
		close(sock);
		break;
	    }
	    break;
          case QUERY_INEXACT:
	    printf("Inexact match for %02lx.\n", cddb_discid(cd_desc));
	    puts("Please choose from the following inexact matches:");
	    for(index = 0; index < query.query_matches; index++)
	      if(strlen(query.query_list[index].list_artist) < 1)
		printf("%d: %s\n", index + 1, query.query_list[index].list_title);
	      else
	        printf("%d: %s / %s\n", index + 1, query.query_list[index].list_artist, query.query_list[index].list_title);
	    printf("%d: None of the above.\n", index + 1);
	    printf("> ");
	    fgets(inbuffer, 256, stdin);
	    selection = strtol(inbuffer, NULL, 10);
	    if(selection > 0 && selection <= query.query_matches) {
	       entry.entry_genre = query.query_list[selection - 1].list_genre;
	       entry.entry_id = query.query_list[selection - 1].list_id;
	       puts("Downloading data...");
	       switch(list.list_host[serverindex].host_protocol) {
	         case CDDB_MODE_CDDBP:
		   if(cddb_read(cd_desc, sock, CDDB_MODE_CDDBP, &entry, data) < 0) {
		      perror("CDDB read error");
		      cddb_generate_unknown_entry(cd_desc, data);
		      return;
		   }
		   cddb_quit(sock);
		   break;
	         case CDDB_MODE_HTTP:
		   if(cddb_read(cd_desc, sock, CDDB_MODE_HTTP, &entry, data, http_string) < 0) {
		      perror("CDDB read error");
		      cddb_generate_unknown_entry(cd_desc, data);
		      return;
		   }
		   shutdown(sock, 2);
		   close(sock);
		   break;
	       }
	       break;
	    }
          case QUERY_NOMATCH:
	    printf("No match for %02lx.\n", cddb_discid(cd_desc));
	    cddb_generate_unknown_entry(cd_desc, data);
         }
         close(sock);
         cddb_write_disc_data(cd_desc, data);
      }
   }
   return; 
}
