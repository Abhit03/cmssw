import FWCore.ParameterSet.Config as cms

#from Configuration.Eras.Era_Run2_cff import Run2
#process = cms.Process('G4PrintGeometry',Run2)
#process.load('Configuration.Geometry.GeometryExtended2015_cff')
#process.load('Configuration.Geometry.GeometryExtended2017_cff')

from Configuration.Eras.Era_Run3_cff import Run3
process = cms.Process('G4PrintGeometry',Run3)
process.load('Configuration.Geometry.GeometryExtended2021_cff')

#from Configuration.Eras.Era_Phase2C11_cff import Phase2C11
#process = cms.Process('G4PrintGeometry',Phase2C11)
#process.load('Configuration.Geometry.GeometryExtended2026D76_cff')

process.load('FWCore.MessageService.MessageLogger_cfi')


from SimG4Core.PrintGeomInfo.g4PrintGeomInfo_cfi import *

process = printGeomInfo(process)

if hasattr(process,'MessageLogger'):
    process.MessageLogger.G4cerr=dict()
    process.MessageLogger.G4cout=dict()



process.g4SimHits.g4GeometryDD4hepSource = cms.bool(False)
process.g4SimHits.Watchers = cms.VPSet(cms.PSet(
    DumpSummary      = cms.untracked.bool(True),
    DumpLVTree       = cms.untracked.bool(True),
    DumpMaterial     = cms.untracked.bool(False),
    DumpLVList       = cms.untracked.bool(True),
    DumpLV           = cms.untracked.bool(False),
    DumpSolid        = cms.untracked.bool(True),
    DumpAttributes   = cms.untracked.bool(False),
    DumpPV           = cms.untracked.bool(False),
    DumpRotation     = cms.untracked.bool(False),
    DumpReplica      = cms.untracked.bool(False),
    DumpTouch        = cms.untracked.bool(True),
    DumpSense        = cms.untracked.bool(True),
    DD4Hep           = cms.untracked.bool(False),
    Name             = cms.untracked.string('ME11*'),
    Names            = cms.untracked.vstring('EcalHitsEB'),
    MaterialFileName = cms.untracked.string('matfileDDD.txt'),
    SolidFileName    = cms.untracked.string('solidfileDDD.txt'),
    LVFileName       = cms.untracked.string('lvfileDDD.txt'),
    PVFileName       = cms.untracked.string('pvfileDDD.txt'),
    type             = cms.string('PrintGeomInfoAction')
))
