/**
 *  Class: GlobalCosmicMuonTrajectoryBuilder
 *
 *  $Date: 2008/10/31 15:26:41 $
 *  $Revision: 1.13 $
 *  \author Chang Liu  -  Purdue University <Chang.Liu@cern.ch>
 *
 **/

#include "RecoMuon/CosmicMuonProducer/interface/GlobalCosmicMuonTrajectoryBuilder.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "TrackingTools/PatternTools/interface/TrajectoryMeasurement.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"

#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackReco/interface/TrackExtraFwd.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateTransform.h"
#include "TrackingTools/Records/interface/TransientRecHitRecord.h"
#include "TrackingTools/TransientTrackingRecHit/interface/TransientTrackingRecHitBuilder.h"

using namespace std;
using namespace edm;

//
// constructor
//
GlobalCosmicMuonTrajectoryBuilder::GlobalCosmicMuonTrajectoryBuilder(const edm::ParameterSet& par,
						                     const MuonServiceProxy* service) : theService(service) {
  ParameterSet smootherPSet = par.getParameter<ParameterSet>("SmootherParameters");
  theSmoother = new CosmicMuonSmoother(smootherPSet,theService);

  ParameterSet trackMatcherPSet = par.getParameter<ParameterSet>("GlobalMuonTrackMatcher");
  theTrackMatcher = new GlobalMuonTrackMatcher(trackMatcherPSet,theService);

  theTkTrackLabel = par.getParameter<string>("TkTrackCollectionLabel");
  theTrackerRecHitBuilderName = par.getParameter<string>("TrackerRecHitBuilder");
  theMuonRecHitBuilderName = par.getParameter<string>("MuonRecHitBuilder");
  thePropagatorName = par.getParameter<string>("Propagator");
  
}

//
// destructor
//

GlobalCosmicMuonTrajectoryBuilder::~GlobalCosmicMuonTrajectoryBuilder() {

  if (theSmoother) delete theSmoother;

}

//
// set Event
//
void GlobalCosmicMuonTrajectoryBuilder::setEvent(const edm::Event& event) {
  event.getByLabel(theTkTrackLabel,theTrackerTracks);

//  edm::Handle<std::vector<Trajectory> > handleTrackerTrajs;
//  if ( event.getByLabel(theTkTrackLabel,handleTrackerTrajs) && handleTrackerTrajs.isValid() ) {
//      tkTrajsAvailable = true;
//      allTrackerTrajs = &*handleTrackerTrajs;   
//      LogInfo("GlobalCosmicMuonTrajectoryBuilder") 
//	<< "Tk Trajectories Found! " << endl;
//  } else {
//      LogInfo("GlobalCosmicMuonTrajectoryBuilder") 
//	<< "No Tk Trajectories Found! " << endl;
//      tkTrajsAvailable = false;
//  }

   theService->eventSetup().get<TransientRecHitRecord>().get(theTrackerRecHitBuilderName,theTrackerRecHitBuilder);
    theService->eventSetup().get<TransientRecHitRecord>().get(theMuonRecHitBuilderName,theMuonRecHitBuilder);

}

//
// reconstruct trajectories
//
MuonCandidate::CandidateContainer GlobalCosmicMuonTrajectoryBuilder::trajectories(const TrackCand& muCand) {

  const std::string metname = "Muon|RecoMuon|CosmicMuon|GlobalCosmicMuonTrajectoryBuilder";
  MuonCandidate::CandidateContainer result;

  if (!theTrackerTracks.isValid()) {
    LogTrace(metname)<< "Tracker Track collection is invalid!!!";
    return result;
  }

  LogTrace(metname) <<"Found "<<theTrackerTracks->size()<<" tracker Tracks";
  if (theTrackerTracks->empty()) return result;

  LogTrace(metname) <<"It has "<<theTrackerTracks->front().found()<<" tk rhs";

  reco::TrackRef muTrack = muCand.second;

  //build tracker TrackCands and pick the best match if size greater than 2
  vector<TrackCand> tkTrackCands;
  for(reco::TrackCollection::size_type i=0; i<theTrackerTracks->size(); ++i){
    reco::TrackRef tkTrack(theTrackerTracks,i);
    TrackCand tkCand = TrackCand(0,tkTrack);
    tkTrackCands.push_back(tkCand);
    LogTrace(metname) << "chisq is " << theTrackMatcher->match(muCand,tkCand,0,0);
    LogTrace(metname) << "d is " << theTrackMatcher->match(muCand,tkCand,1,0);
    LogTrace(metname) << "r_pos is " << theTrackMatcher->match(muCand,tkCand,2,0);
  }

  // match muCand to tkTrackCands
  vector<TrackCand> matched_trackerTracks = theTrackMatcher->match(muCand,tkTrackCands);

  LogTrace(metname) <<"TrackMatcher found " << matched_trackerTracks.size() << "tracker tracks matched";
  
  if ( matched_trackerTracks.empty()) return result;
  reco::TrackRef tkTrack;
  
  if(  matched_trackerTracks.size() == 1 ) {
    tkTrack = matched_trackerTracks.front().second;
  } else {
    // in case of more than 1 tkTrack,
    // select the best-one based on distance (matchOption==1)
    // at innermost Mu hit surface. (surfaceOption == 0)
    double quality = 1e6;
    double max_quality = 1e6;
    for( vector<TrackCand>::const_iterator iter = matched_trackerTracks.begin(); iter != matched_trackerTracks.end(); iter++) {
      quality = theTrackMatcher->match(muCand,*iter, 1, 0);
      LogTrace(metname) <<" quality of tracker track is " << quality;
      if( quality < max_quality ) {
        max_quality=quality;
        tkTrack = iter->second;
      }
    }
      LogTrace(metname) <<" Picked tracker track with quality " << max_quality;
  }  
  if ( tkTrack.isNull() ) return result;

  ConstRecHitContainer muRecHits;

  if (muCand.first == 0 || !muCand.first->isValid()) { 
     muRecHits = getTransientRecHits(*muTrack);
  } else {
     muRecHits = muCand.first->recHits();
  }

  LogTrace(metname)<<"mu RecHits: "<<muRecHits.size();

  ConstRecHitContainer tkRecHits = getTransientRecHits(*tkTrack);

//  if ( !tkTrajsAvailable ) {
//     tkRecHits = getTransientRecHits(*tkTrack);
//  } else {
//     tkRecHits = allTrackerTrajs->front().recHits();
//  }

  ConstRecHitContainer hits; //= tkRecHits;
  LogTrace(metname)<<"tk RecHits: "<<tkRecHits.size();

//  hits.insert(hits.end(), muRecHits.begin(), muRecHits.end());
//  stable_sort(hits.begin(), hits.end(), DecreasingGlobalY());

  sortHits(hits, muRecHits, tkRecHits);

  LogTrace(metname) << "Used RecHits after sort: "<<hits.size();
  LogTrace(metname)<<utilities()->print(hits);
  LogTrace(metname) << "== End of Used RecHits == ";

  TrajectoryStateTransform tsTrans;

  TrajectoryStateOnSurface muonState1 = tsTrans.innerStateOnSurface(*muTrack, *theService->trackingGeometry(), &*theService->magneticField());
  TrajectoryStateOnSurface tkState1 = tsTrans.innerStateOnSurface(*tkTrack, *theService->trackingGeometry(), &*theService->magneticField());

  TrajectoryStateOnSurface muonState2 = tsTrans.outerStateOnSurface(*muTrack, *theService->trackingGeometry(), &*theService->magneticField());
  TrajectoryStateOnSurface tkState2 = tsTrans.outerStateOnSurface(*tkTrack, *theService->trackingGeometry(), &*theService->magneticField());

  TrajectoryStateOnSurface firstState1 =
   ( muonState1.globalPosition().y() > tkState1.globalPosition().y() )? muonState1 : tkState1;
  TrajectoryStateOnSurface firstState2 =
   ( muonState2.globalPosition().y() > tkState2.globalPosition().y() )? muonState2 : tkState2;

  TrajectoryStateOnSurface firstState =
   ( firstState1.globalPosition().y() > firstState2.globalPosition().y() )? firstState1 : firstState2;

  if (!firstState.isValid()) return result;
  
  LogTrace(metname) <<"firstTSOS pos: "<<firstState.globalPosition()<<"mom: "<<firstState.globalMomentum();

  // begin refitting

  TrajectorySeed seed;
  vector<Trajectory> refitted = theSmoother->trajectories(seed,hits,firstState);

  if ( refitted.empty() ) refitted = theSmoother->fit(seed,hits,firstState); //FIXME

  if (refitted.empty()) {
     LogTrace(metname)<<"refit fail";
     return result;
  }

  Trajectory* myTraj = new Trajectory(refitted.front());

  std::vector<TrajectoryMeasurement> mytms = myTraj->measurements(); 
  LogTrace(metname)<<"measurements in final trajectory "<<mytms.size();
  LogTrace(metname) <<"Orignally there are "<<tkTrack->found()<<" tk rhs and "<<muTrack->found()<<" mu rhs.";

  if ( mytms.size() <= tkTrack->found() ) {
     LogTrace(metname)<<"insufficient measurements. skip... ";
     return result;
  }

  MuonCandidate* myCand = new MuonCandidate(myTraj,muTrack,tkTrack);
  result.push_back(myCand);
  LogTrace(metname)<<"final global cosmic muon: ";
  for (std::vector<TrajectoryMeasurement>::const_iterator itm = mytms.begin();
       itm != mytms.end(); ++itm ) {
       LogTrace(metname)<<"updated pos "<<itm->updatedState().globalPosition()
                       <<"mom "<<itm->updatedState().globalMomentum();
       }
  return result;
}

void GlobalCosmicMuonTrajectoryBuilder::sortHits(ConstRecHitContainer& hits, ConstRecHitContainer& muonHits, ConstRecHitContainer& tkHits) {

   std::string metname = "Muon|RecoMuon|CosmicMuon|GlobalCosmicMuonTrajectoryBuilder";
   if ( tkHits.empty() ) {
      LogTrace(metname) << "No valid tracker hits";
      return;
   }
   if ( muonHits.empty() ) {
      LogTrace(metname) << "No valid muon hits";
      return;
   }

   ConstRecHitContainer::const_iterator frontTkHit = tkHits.begin();
   ConstRecHitContainer::const_iterator backTkHit  = tkHits.end() - 1;
   while ( !(*frontTkHit)->isValid() && frontTkHit != backTkHit) {frontTkHit++;}
   while ( !(*backTkHit)->isValid() && backTkHit != frontTkHit)  {backTkHit--;}

   ConstRecHitContainer::const_iterator frontMuHit = muonHits.begin();
   ConstRecHitContainer::const_iterator backMuHit  = muonHits.end() - 1;
   while ( !(*frontMuHit)->isValid() && frontMuHit != backMuHit) {frontMuHit++;}
   while ( !(*backMuHit)->isValid() && backMuHit != frontMuHit)  {backMuHit--;}

   if ( frontTkHit == backTkHit ) {
      LogTrace(metname) << "No valid tracker hits";
      return;
   }
   if ( frontMuHit == backMuHit ) {
      LogTrace(metname) << "No valid muon hits";
      return;
   }

  GlobalPoint frontTkPos = (*frontTkHit)->globalPosition();
  GlobalPoint backTkPos = (*backTkHit)->globalPosition();

  //sort hits going from higher to lower positions
  if ( frontTkPos.y() < backTkPos.y() )  {//check if tk hits order same direction
    reverse(tkHits.begin(), tkHits.end());
  }

  if ( (*frontMuHit)->globalPosition().y() < (*backMuHit)->globalPosition().y() )  {//check if tk hits order same direction
    reverse(muonHits.begin(), muonHits.end());
  }

  //separate muon hits into 2 different hemisphere
  ConstRecHitContainer::iterator middlepoint = muonHits.begin();
  bool insertInMiddle = false;

  for (ConstRecHitContainer::iterator ihit = muonHits.begin(); 
       ihit != muonHits.end() - 1; ihit++ ) {
    GlobalPoint ipos = (*ihit)->globalPosition();
    GlobalPoint nextpos = (*(ihit+1))->globalPosition();
    GlobalPoint middle((ipos.x()+nextpos.x())/2, (ipos.y()+nextpos.y())/2, (ipos.z()+nextpos.z())/2);
    LogTrace(metname)<<"ipos "<<ipos<<"nextpos"<<nextpos<<" middle "<<middle;
    if ( (middle.perp() < ipos.perp()) && (middle.perp() < nextpos.perp() ) ) {
      LogTrace(metname)<<"found middlepoint";
      middlepoint = ihit;
      insertInMiddle = true;
      break;
    }
  }

  //insert track hits in correct order
  if ( insertInMiddle ) { //if tk hits should be sandwich
    GlobalPoint jointpointpos = (*middlepoint)->globalPosition();
    LogTrace(metname)<<"jointpoint "<<jointpointpos;
    if ((frontTkPos - jointpointpos).mag() > (backTkPos - jointpointpos).mag() ) {//check if tk hits order same direction
      reverse(tkHits.begin(), tkHits.end());
    }
    muonHits.insert(middlepoint+1, tkHits.begin(), tkHits.end());
    hits = muonHits; 
  } else { // append at one end
    if ( (frontTkPos - backTkPos).y() < 0 ) { //insert at the end
      hits = muonHits; 
      hits.insert(hits.end(), tkHits.begin(), tkHits.end());
    } else { //insert at the beginning
      hits = tkHits;
      hits.insert(hits.end(), muonHits.begin(), muonHits.end());
    }
  }
}


TransientTrackingRecHit::ConstRecHitContainer
GlobalCosmicMuonTrajectoryBuilder::getTransientRecHits(const reco::Track& track) const {

  TransientTrackingRecHit::ConstRecHitContainer result;
  std::string metname = "Muon|RecoMuon|CosmicMuon|GlobalCosmicMuonTrajectoryBuilder";

  TrajectoryStateTransform tsTrans;

  TrajectoryStateOnSurface currTsos = tsTrans.innerStateOnSurface(track, *theService->trackingGeometry(), &*theService->magneticField());
  for (trackingRecHit_iterator hit = track.recHitsBegin(); hit != track.recHitsEnd(); ++hit) {
    if((*hit)->isValid()) {
      DetId recoid = (*hit)->geographicalId();
      if ( recoid.det() == DetId::Tracker ) {
        TransientTrackingRecHit::RecHitPointer ttrhit = theTrackerRecHitBuilder->build(&**hit);
        TrajectoryStateOnSurface predTsos =  theService->propagator(thePropagatorName)->propagate(currTsos, theService->trackingGeometry()->idToDet(recoid)->surface());
        LogTrace(metname)<<"predtsos "<<predTsos.isValid();
        if ( predTsos.isValid() ) currTsos = predTsos;
        TransientTrackingRecHit::RecHitPointer preciseHit = ttrhit->clone(predTsos);
        result.push_back(preciseHit);
      } else if ( recoid.det() == DetId::Muon ) {
	result.push_back(theMuonRecHitBuilder->build(&**hit));
      }
    }
  }
  return result;
}
