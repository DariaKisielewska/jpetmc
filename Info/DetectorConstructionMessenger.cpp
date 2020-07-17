/**
 *  @copyright Copyright 2020 The J-PET Monte Carlo Authors. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may find a copy of the License in the LICENCE file.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  @file DetectorConstructionMessenger.cpp
 */

#include "../Info/DetectorConstructionMessenger.h"
#include "../Core/DetectorConstants.h"

DetectorConstructionMessenger::DetectorConstructionMessenger(DetectorConstruction* detector) : fDetector(detector)
{
  fDirectory = new G4UIdirectory("/jpetmc/detector/");
  fDirectory->SetGuidance("Commands for controling the geometry");

  fLoadTargetForRun = new G4UIcmdWithAnInteger("/jpetmc/detector/loadTargetForRun", this);
  fLoadTargetForRun->SetGuidance("Set RUN number to simulate given Target");
  fLoadTargetForRun->SetDefaultValue(kDefaultRunNumber);

  fLoadIdealGeometry = new G4UIcmdWithAnInteger("/jpetmc/detector/loadIdealGeom", this);
  fLoadIdealGeometry->SetGuidance("Generate ideal geometry for 1-4 layers");
  fLoadIdealGeometry->SetDefaultValue(1);

  fLoadJPetBasicGeometry = new G4UIcmdWithoutParameter("/jpetmc/detector/loadJPetBasicGeom", this);
  fLoadJPetBasicGeometry->SetGuidance("Generate standard JPet detector geometry");

  fLoadOnlyScintillators = new G4UIcmdWithoutParameter("/jpetmc/detector/loadOnlyScintillators", this);
  fLoadOnlyScintillators->SetGuidance("Generate only scintillators (for test purposes)");

  //! Bool converted into string
  fLoadModularLayer = new G4UIcmdWithAString("/jpetmc/detector/loadModularLayer", this);
  fLoadModularLayer->SetGuidance("Load additional layer made out of modules");

  fScinHitMergingTime = new G4UIcmdWithADoubleAndUnit("/jpetmc/detector/hitMergingTime", this);
  fScinHitMergingTime->SetGuidance("Define time range (ns) while merging hits in scintillators");
  fScinHitMergingTime->SetDefaultUnit("ns");
  fScinHitMergingTime->SetUnitCandidates("ns");
  
  fCreateGeometryFile = new G4UIcmdWithoutParameter("/jpetmc/detector/createGeometryFile", this);
  fCreateGeometryFile->SetGuidance("Create a Json file for the simulated setup");
  
  fCreateOldGeometryFileStyle = new G4UIcmdWithoutParameter("/jpetmc/detector/createOldGeometryFileStyle", this);
  fCreateOldGeometryFileStyle->SetGuidance("Json file will be created with old style");
}

DetectorConstructionMessenger::~DetectorConstructionMessenger()
{
  delete fLoadTargetForRun;
  delete fLoadIdealGeometry;
  delete fLoadJPetBasicGeometry;
  delete fLoadOnlyScintillators;
  delete fLoadModularLayer;
  delete fScinHitMergingTime;
  delete fCreateGeometryFile;
  delete fCreateOldGeometryFileStyle;
}

void DetectorConstructionMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if (command == fLoadTargetForRun) {
    fDetector->LoadGeometryForRun(fLoadTargetForRun->GetNewIntValue(newValue));
    fDetector->UpdateGeometry();
  } else if (command == fLoadIdealGeometry){
    G4Exception(
      "DetectorConstructionMessenger", "DCM01",
      JustWarning, "Option is not yet implemented"
    );
  } else if (command == fLoadJPetBasicGeometry) {
    fDetector->ConstructBasicGeometry(true);
    fDetector->LoadFrame(true);
    fDetector->UpdateGeometry();
  } else if (command == fLoadOnlyScintillators) {
    fDetector->LoadFrame(false);
    fDetector->UpdateGeometry();
  } else if (command == fLoadModularLayer) {
    fDetector->ConstructModularLayer(newValue);
    fDetector->UpdateGeometry();
  } else if (command == fScinHitMergingTime) {
    DetectorConstants::SetMergingTimeValueForScin(fScinHitMergingTime->GetNewDoubleValue(newValue));
    fDetector->UpdateGeometry();
  } else if (command == fCreateGeometryFile) {
    fDetector->CreateGeometryFileFlag(true);
  } else if (command == fCreateOldGeometryFileStyle) {
    fDetector->SetOldStyleOfGeometryFile(true);
  }
}
