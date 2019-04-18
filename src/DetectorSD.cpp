#include "DetectorSD.h"
#include "G4VProcess.hh"
#include "G4PrimaryParticle.hh"
#include "PrimaryParticleInformation.h"

DetectorSD::DetectorSD(G4String name, G4int scinSum)
    :G4VSensitiveDetector(name), totScinNum(scinSum)
{
     collectionName.insert("detectorCollection");

    for (G4int i=0; i<=totScinNum; i++)
    {
        previousHitHistory.push_back(-1);
        previousHitTimeHistory.push_back(0.);
    }

}

DetectorSD::~DetectorSD()
{}

void DetectorSD::Initialize(G4HCofThisEvent* HCE)
{
    static int HCID = -1;
    fDetectorCollection = new DetectorHitsCollection(SensitiveDetectorName,collectionName[0]);

    if(HCID<0)
    { HCID = GetCollectionID(0); }
    HCE->AddHitsCollection(HCID,fDetectorCollection);

    for (G4int i=0; i<=totScinNum; i++)
    {
        previousHitHistory[i] = -1;
        previousHitTimeHistory[i] = 0.;
    }

}



G4bool DetectorSD::ProcessHits(G4Step* aStep, G4TouchableHistory* )
{
    G4double edep = aStep->GetTotalEnergyDeposit();

    if(edep==0.0) return false;

    G4TouchableHistory* theTouchable  = (G4TouchableHistory*)(aStep->GetPreStepPoint()->GetTouchable()); 
    G4VPhysicalVolume* physVol = theTouchable->GetVolume();
    G4int   currentScinCopy = physVol->GetCopyNo();
    G4double currentTime = aStep->GetPreStepPoint()->GetGlobalTime();



    if( (previousHitHistory[currentScinCopy] !=-1 )
          &&( abs(previousHitTimeHistory[currentScinCopy]-currentTime)<timeIntervals) ) 
    {
        // update track
        (*fDetectorCollection)[previousHitHistory[currentScinCopy]]->AddEdep(edep);
        (*fDetectorCollection)[previousHitHistory[currentScinCopy]]->AddInteraction();

    } else {
        // new hit
        DetectorHit* newHit = new DetectorHit();
        newHit->SetEdep( edep );
        newHit->SetTrackID(aStep->GetTrack()->GetTrackID());
        newHit->SetTrackPDG(aStep->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
        newHit->SetProcessName(aStep->GetPostStepPoint()->GetProcessDefinedStep()->GetProcessName());
        newHit->SetInteractionNumber();
        newHit->SetPosition(aStep->GetPostStepPoint()->GetPosition());
        newHit->SetTime(currentTime);

        newHit->SetScinID(physVol->GetCopyNo());

        newHit->SetPolarizationIn(aStep->GetPreStepPoint()->GetPolarization());
        newHit->SetPolarizationOut(aStep->GetPostStepPoint()->GetPolarization());
        newHit->SetMomentumIn(aStep->GetPreStepPoint()->GetMomentum());
        //printf(" momentum track %4.2f %4.2f %4.2f \n",aStep->GetTrack()->GetMomentum().x(),aStep->GetTrack()->GetMomentum().y(),
        //aStep->GetTrack()->GetMomentum().z());
        //printf(" momentum presteppoint %4.2f %4.2f %4.2f \n",aStep->GetPreStepPoint()->GetMomentum().x(),aStep->GetPreStepPoint()->GetMomentum().y(),
        //aStep->GetPreStepPoint()->GetMomentum().z());

        newHit->SetMomentumOut(aStep->GetPostStepPoint()->GetMomentum());


        // only particles generated by user has PrimaryParticleInformation
        if(aStep->GetTrack()->GetParentID() == 0 ){
            PrimaryParticleInformation* info  = static_cast<PrimaryParticleInformation*> (aStep->GetTrack()->GetDynamicParticle()->GetPrimaryParticle()->GetUserInformation());
    
            if (info != 0 ){
                newHit->SetGenGammaMultiplicity(info->GetGammaMultiplicity());
                newHit->SetGenGammaIndex(info->GetIndex());
            }
        }

        G4int id = fDetectorCollection->insert(newHit);
        previousHitHistory[currentScinCopy] = id-1;
        previousHitTimeHistory[currentScinCopy]= currentTime;



    }


    return true;
}


