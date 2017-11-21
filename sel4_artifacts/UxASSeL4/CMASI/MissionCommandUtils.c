/******************************************************************************
* Author: Dan DaCosta                                                         *
*                                                                             *
* Purposes: Implements a number of functions useful for mission               *
* command manipulation.                                                       *
*                                                                             *
* Testing build:                                                              *
*   $CC -g -I./ -I./common/ -D__MISSIONCOMMANDUTILS_TESTS__ *.c common/*.c    *
******************************************************************************/

//#include "lmcp.h"
#include "MissionCommandUtils.h"
#include <stdbool.h>

#define AUTOCORRES
#ifdef AUTOCORRES
#define printf(args...) 
#define assert(x)
void *memset(void *s, int c, size_t n);
void *calloc(size_t nmemb, size_t size);
void free(void * p);
#else
#include <assert.h>
#include <stdlib.h>
#endif

#define DEBUG(fmt,args...) \
  printf("%s,%s,%i:"fmt,__FUNCTION__,"MissionCommandMtils.c",__LINE__,##args)


/*void MCEInit(MissionCommandExt * mce, uint16_t waypoints) {
  memset((uint8_t*)&(mce->missioncommand),0,sizeof(MissionCommand));
  mce->missioncommand.waypointlist = calloc(sizeof(Waypoint*),waypoints);
  mce->waypoints = calloc(sizeof(Waypoint),waypoints);
  for(uint16_t i = 0; i < waypoints; i++) {
    mce->missioncommand.waypointlist[i] = mce->waypoints + i;
  }
  mce->waypointslen = waypoints;
  }*/


/* ASM: ws != null */
/* ASM: length > 0 */
Waypoint * FindWP(Waypoint * ws, uint16_t len, int64_t id) {
  for(uint16_t i = 0 ; i < len; i++) {
    if(ws[i].number == id) {
     return ws + i;
    }
  }
  return NULL;
}

/* NB: Cycles in ws will be unrolled into win. */
/* ASM: id can be found in ws. */
/* ASM: All next ids in the ws waypoint list can be found in ws. */
/* ASM: ws != null */
/* ASM: len_ws > 0. */
/* ASM: ws_win != null */
/* ASM: len_ws_win > 0 */
/* ASM: len_ws_win is less than the number of waypoints that can be
   stored in ws_win. */
/* ASM: Last waypoint starts a cycle. */
void FillWindow(  Waypoint * ws
                           , uint16_t len_ws
                           , int64_t id
                           , uint16_t len_ws_win
                           , Waypoint * ws_win /* out */) {
  uint16_t i;
  int64_t nid = id;
  Waypoint * wp = NULL;
  for(i=0; i < len_ws_win; i++) {
    wp = FindWP(ws, len_ws, nid);
    ws_win[i] = *wp;
    nid = ws_win[i].nextwaypoint;    
  }
  /*ws_win[i].nextwaypoint = ws_win[i].number; */
  return;
}

void GroomWindow(uint16_t len_ws_win
                , Waypoint * ws_win /* out */) {
  ws_win[len_ws_win-1].nextwaypoint = ws_win[len_ws_win-1].number;
  return;
}

/* NB: Cycles in ws will be unrolled into win. */
/* ASM: id can be found in ws. */
/* ASM: All next ids in the ws waypoint list can be found in ws. */
/* ASM: ws != null */
/* ASM: len_ws > 0. */
/* ASM: ws_win != null */
/* ASM: len_ws_win > 0 */
/* ASM: len_ws_win is less than the number of waypoints that can be
   stored in ws_win. */
/* ASM: Last waypoint starts a cycle. */
void FillAndGroomWindow(  Waypoint * ws
                           , uint16_t len_ws
                           , int64_t id
                           , uint16_t len_ws_win
                           , Waypoint * ws_win /* out */) {
  FillWindow(ws, len_ws, id, len_ws_win, ws_win);
  GroomWindow(len_ws_win, ws_win);
  return;
}

#ifdef __MISSIONCOMMANDUTILS_TESTS__

#include <stdio.h>

#define RETURNONFAIL(X)                                 \
  if((X) != true) {                                     \
    printf("(%s:%u) Test failed.\n",__FILE__,__LINE__); \
    return false;                                       \
  }

#define TEST(X)                                             \
  {                                                         \
    bool flag;                                              \
    printf("%s ",#X);                                       \
    flag = X();                                             \
    printf("%s\n", flag == true ? " Passed." : " Failed."); \
  }

/* missioncommandFromFile: Deserialize a mission command reading from a
   file.

     f - The file to read the mission command from.

     mcp - The mission command to be populated from the serialized data.

*/
void missioncommandFromFile(FILE * f, MissionCommand ** mcpp) {

    uint8_t * source = NULL;
    uint8_t * orig_source = NULL;

    size_t size = 0;

    /* assumption: mcpp is not NULL. */
    assert(mcpp != NULL);
    /* assumption: f is not NULL. */
    assert(f != NULL);

    if (fseek(f, 0L, SEEK_END) == 0) {
        /* Get the size of the file. */
        size = ftell(f);
        if (size == -1) {
            /* Error */
        }

        /* Allocate our buffer to that size. */
        source = malloc(sizeof(char) * (size + 1));
        orig_source = source;
        /* Go back to the start of the file. */
        if (fseek(f, 0L, SEEK_SET) != 0) {
            /* Error */
        }

        /* Read the entire file into memory. */
        size_t newlen = fread(source, sizeof(char), size, f);
        if ( ferror( f ) != 0 ) {
            fputs("Error reading file", stderr);
        } else {
            source[newlen++] = '\0'; /* Just to be safe. */
        }

        assert(lmcp_process_msg(&source,size,(lmcp_object**)mcpp) != -1);
        //lmcp_pp(*mcpp);
        free(orig_source);
        source = NULL;
        orig_source = NULL;
    }


    return;
}

bool CheckMCWaypointSubSequence(const MissionCommand * omcp,
                                const MissionCommand * smcp,
                                const int64_t start_waypoint_id,
                                const uint16_t sslen) {
  Waypoint * owp;
  Waypoint * swp;
  Waypoint twp;
  int64_t it_id = start_waypoint_id;

  /* Certain parts of a mission command subsequence should not change. */
  RETURNONFAIL(omcp->super.commandid == smcp->super.commandid);
  RETURNONFAIL(omcp->super.vehicleid == smcp->super.vehicleid);
  RETURNONFAIL(omcp->super.vehicleactionlist_ai.length
               == smcp->super.vehicleactionlist_ai.length);
  for(uint16_t i = 0 ; i < smcp->super.vehicleactionlist_ai.length ; i ++) {
    RETURNONFAIL(omcp->super.vehicleactionlist[i] == smcp->super.vehicleactionlist[i]);
  }
  
  /* A subsequence could be shorter. */
  RETURNONFAIL(smcp->super.vehicleactionlist_ai.length <= sslen );

  /* The starting point we provided to the subsequence should be its
     starting point. */
  RETURNONFAIL(start_waypoint_id == smcp->firstwaypoint);

  /* Check that all non-last waypoints are identical up to the
     subsequence size. */
  uint16_t i = 0;
  for(; i < smcp->waypointlist_ai.length - 1; i++) {
    owp = FindWP(omcp,it_id);
    RETURNONFAIL(owp != NULL);
    swp = FindWP(smcp,it_id);
    RETURNONFAIL(swp != NULL);
    RETURNONFAIL(owp->super.latitude == smcp->waypointlist[i]->super.latitude);
    RETURNONFAIL(owp->super.longitude == smcp->waypointlist[i]->super.longitude);
    RETURNONFAIL(owp->super.altitude == smcp->waypointlist[i]->super.altitude);
    RETURNONFAIL(owp->super.altitudetype == smcp->waypointlist[i]->super.altitudetype);
    RETURNONFAIL(owp->number == smcp->waypointlist[i]->number);
    RETURNONFAIL(owp->nextwaypoint == smcp->waypointlist[i]->nextwaypoint);
    RETURNONFAIL(owp->speed == smcp->waypointlist[i]->speed);
    RETURNONFAIL(owp->speedtype == smcp->waypointlist[i]->speedtype);
    RETURNONFAIL(owp->climbrate == smcp->waypointlist[i]->climbrate);
    RETURNONFAIL(owp->turntype == smcp->waypointlist[i]->turntype);
    RETURNONFAIL(owp->vehicleactionlist == smcp->waypointlist[i]->vehicleactionlist);
    RETURNONFAIL(owp->vehicleactionlist_ai.length == smcp->waypointlist[i]->vehicleactionlist_ai.length);
    RETURNONFAIL(owp->contingencywaypointa == smcp->waypointlist[i]->contingencywaypointa);
    RETURNONFAIL(owp->contingencywaypointb == smcp->waypointlist[i]->contingencywaypointb);
    RETURNONFAIL(owp->associatedtasks == smcp->waypointlist[i]->associatedtasks);
    RETURNONFAIL(owp->associatedtasks_ai.length == smcp->waypointlist[i]->associatedtasks_ai.length);
    it_id = swp->nextwaypoint;
  }
  
  /* Check that the last waypoint is identical and that its nxid
     points to itself. */
  owp = FindWP(omcp,it_id);
  RETURNONFAIL(owp != NULL);
  swp = FindWP(smcp,it_id);
  RETURNONFAIL(swp != NULL);
  RETURNONFAIL(owp->super.latitude == smcp->waypointlist[i]->super.latitude);
  RETURNONFAIL(owp->super.longitude == smcp->waypointlist[i]->super.longitude);
  RETURNONFAIL(owp->super.altitude == smcp->waypointlist[i]->super.altitude);
  RETURNONFAIL(owp->super.altitudetype == smcp->waypointlist[i]->super.altitudetype);
  RETURNONFAIL(owp->number == smcp->waypointlist[i]->number);
  RETURNONFAIL(owp->speed == smcp->waypointlist[i]->speed);
  RETURNONFAIL(owp->speedtype == smcp->waypointlist[i]->speedtype);
  RETURNONFAIL(owp->climbrate == smcp->waypointlist[i]->climbrate);
  RETURNONFAIL(owp->turntype == smcp->waypointlist[i]->turntype);
  RETURNONFAIL(owp->vehicleactionlist == smcp->waypointlist[i]->vehicleactionlist);
  RETURNONFAIL(owp->vehicleactionlist_ai.length == smcp->waypointlist[i]->vehicleactionlist_ai.length);
  RETURNONFAIL(owp->contingencywaypointa == smcp->waypointlist[i]->contingencywaypointa);
  RETURNONFAIL(owp->contingencywaypointb == smcp->waypointlist[i]->contingencywaypointb);
  RETURNONFAIL(owp->associatedtasks == smcp->waypointlist[i]->associatedtasks);
  RETURNONFAIL(owp->associatedtasks_ai.length == smcp->waypointlist[i]->associatedtasks_ai.length);
  return true;
}


bool ExhaustiveTest(MissionCommand * omcp) {
  Waypoint * wp = NULL;
  uint64_t total_tests = (omcp->waypointlist_ai.length*(omcp->waypointlist_ai.length+1))/2;
  uint64_t ten_percent = total_tests/10;
  uint64_t tests_completed = 0;
  MissionCommandExt smce = {};
  MissionCommand * smcp = (MissionCommand*)&smce;
  Waypoint * ws = NULL;
  Waypoint * len_ws = 0;
  
  len_ws = omcp->waypointlist_ai.length;
  ws = calloc(sizeof(Waypoint),len_ws);
  for(uint16_t i = 0; i < len_ws; i++) {
    ws[i] = (omcp->waypointlist[i])*;
  }
  
  /* For each subsequence length less than or equal to the total
     number of waypoints. */
  for(uint16_t i = 2 ; i <= omcp->waypointlist_ai.length ; i++) {
    int64_t last_subseq_id = omcp->firstwaypoint;
    MCEInit(&smce,i);
    Waypoint ** tmp = smce.missioncommand.waypointlist;
    smce.missioncommand = *omcp;
    smce.missioncommand.waypointlist = tmp;
    
    
    for(uint16_t j = 1 ; j < i ; j++) {
      MCWaypointSubSequence(ws,
                            ws_len,
                            omcp->firstwaypoint,
                            i,
                            smce.waypoints);
      smce.missioncommand.waypointlist_ai.length = i;
      smce.missioncommand.firstwaypoint = omcp->firstwaypoint;
      RETURNONFAIL(CheckMCWaypointSubSequence(omcp,
                                              smcp,
                                              omcp->firstwaypoint,
                                              i));

      last_subseq_id = omcp->firstwaypoint;
      wp = FindWP(ws,ws_len,last_subseq_id);
      RETURNONFAIL(wp != NULL);
      GetMCWaypointSubSequence(ws,
                               len_ws,
                               last_subseq_id,
                               wp->number,
                               i,
                               smce.waypoints);
      smce.missioncommand.waypointlist_ai.length = i;
      smce.missioncommand.firstwaypoint = wp->number;
      

      uint16_t c = 0;
      uint16_t n = 0;
      for(uint16_t k = 0 ; k < j ; k++) {
        n++;
        c++;
        wp = FindWP(ws,len_ws,wp->nextwaypoint);
        RETURNONFAIL(wp != NULL);
      }
      while(wp->number != wp->nextwaypoint) {
        bool flag = GetMCWaypointSubSequence(ws,
                                             len_ws
                                             last_subseq_id,
                                             wp->number,
                                             i,
                                             smce.waypoints);
        if(n < i/2) {
          RETURNONFAIL(flag != true);
        } else  {
          n = 0;
          last_subseq_id = wp->number;
          RETURNONFAIL(flag == true);
          RETURNONFAIL(CheckMCWaypointSubSequence(omcp,
                                                  smcp,
                                                  last_subseq_id,
                                                  i));
        }
        for(uint16_t k = 0 ; k < j ; k++) {
          c++;
          n++;
          wp = FindWP(ws, len_ws,wp->nextwaypoint);
          RETURNONFAIL(wp != NULL);
        }
      }
      tests_completed++;
      if(tests_completed % ten_percent == 0) {
        /* Update test progress. */
        fprintf(stdout," %lu0 ",tests_completed / ten_percent);
        fflush(stdout);
      }
    }
    free(smce.waypoints);
    smce.waypoints = NULL;
    free(smce.missioncommand.waypointlist);
    smce.missioncommand.waypointlist = NULL;
  }
  free(ws);
  return true;
}

bool WaterwaysTestFromFile() {

    FILE * f = NULL;
    MissionCommand * omcp = NULL;
    lmcp_init_MissionCommand(&omcp);
    /* Load waterways data. */
    f = fopen("./testdata/waterways.mc","r");
    RETURNONFAIL(f != NULL);
    missioncommandFromFile(f, &omcp);
    fclose(f);

    ExhaustiveTest(omcp);

    lmcp_free((lmcp_object*)omcp);
    return true;
}


int main(void) {
    TEST(WaterwaysTestFromFile);

    return 0;
}

#endif /* __MISSIONCOMMANDUTILS_TESTS__ */
