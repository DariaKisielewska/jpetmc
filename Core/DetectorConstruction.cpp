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
 *  @file DetectorConstruction.cpp
 */

#include "../Info/DetectorConstructionMessenger.h"
#include "DetectorConstruction.h"
#include "MaterialParameters.h"
#include "DetectorConstants.h"
#include "MaterialExtension.h"
#include "RunManager.h"

#include <G4PhysicalVolumeStore.hh>
#include <G4LogicalVolumeStore.hh>
#include <G4SubtractionSolid.hh>
#include <G4RegionStore.hh>
#include <G4SolidStore.hh>
#include <G4UnionSolid.hh>
#include <G4Polycone.hh>
#include <TBufferJSON.h>
#include <G4Tubs.hh>
#include <iostream>
#include <vector>

DetectorConstruction* DetectorConstruction::fInstance = 0;

DetectorConstruction* DetectorConstruction::GetInstance()
{
  if (fInstance == 0) fInstance = new DetectorConstruction();
  return fInstance;
}

DetectorConstruction::DetectorConstruction() :
G4VUserDetectorConstruction(), fRunNumber(0), fLoadScintillators(false), 
fLoadCADFrame(false), fLoadWrapping(true), fLoadModularLayer(false), fCreateGeometryFile(false)
{
  InitializeMaterials();
  fMessenger = new DetectorConstructionMessenger(this);
}

DetectorConstruction::~DetectorConstruction() { delete fMessenger; }

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  G4GeometryManager::GetInstance()->OpenGeometry();
  G4PhysicalVolumeStore::GetInstance()->Clean();
  G4LogicalVolumeStore::GetInstance()->Clean();
  G4SolidStore::GetInstance()->Clean();

  //! world
  fWorldSolid = new G4Box(
    "world", DetectorConstants::world_size[0],
    DetectorConstants::world_size[1], DetectorConstants::world_size[2]
  );
  fWorldLogical = new G4LogicalVolume(fWorldSolid, fAir, "worldLogical");
  fWorldPhysical = new G4PVPlacement(
    0, G4ThreeVector(), fWorldLogical, "worldPhysical", 0, false, 0, checkOverlaps
  );

  if (fLoadScintillators) {
    //! scintillators for standard setup
    ConstructScintillators();
  }
  if (fLoadModularLayer) {
    ConstructScintillatorsModularLayer();
  }
  if (fLoadCADFrame) {
    ConstructFrameCAD();
  }
  if (fRunNumber == 3) {
    ConstructTargetRun3();
  } else if (fRunNumber == 5) {
    ConstructTargetRun5();
  } else if (fRunNumber == 6) {
    ConstructTargetRun6();
  } else if (fRunNumber == 7) {
    ConstructTargetRun7();
  }
  
  if (fCreateGeometryFile) {
    CreateGeometryFile();
  }

  return fWorldPhysical;
}

void DetectorConstruction::ConstructSDandField()
{
  if (!fDetectorSD.Get()) {
    DetectorSD* det = new DetectorSD(
      "/mydet/detector", maxScinID, DetectorConstants::GetMergingTimeValueForScin()
    );
    fDetectorSD.Put(det);
  }

  G4SDManager::GetSDMpointer()->AddNewDetector(fDetectorSD.Get());
  SetSensitiveDetector(fScinLog, fDetectorSD.Get());
  if (fLoadModularLayer) {
    SetSensitiveDetector(fScinLogInModule, fDetectorSD.Get());
  }
}

void DetectorConstruction::LoadGeometryForRun(G4int nr)
{
  fRunNumber = nr;
  if (
    fRunNumber == 3 || fRunNumber == 5 || fRunNumber == 6
    || fRunNumber == 7 || fRunNumber == 0
  ){
    LoadFrame(false);
  } else {
    G4Exception(
      "DetectorConstruction", "DC02", FatalException,
      "This run setup is not implemented "
    );
  }
}

G4int DetectorConstruction::ReturnNumberOfScintillators()
{
  if (fLoadModularLayer) return 504;
  else return 192;
}

void DetectorConstruction::UpdateGeometry()
{
  RunManager::GetRunManager()->ReinitializeGeometry();
}

void DetectorConstruction::ReloadMaterials(const G4String& material)
{
  if (material == "xad4") {
    fXADMaterial->ChangeMaterialConstants();
    fXADMaterial->FillIntensities();
  } else if (material == "kapton") {
    fKapton->ChangeMaterialConstants();
    fKapton->FillIntensities();
  } else if (material == "aluminium") {
    fAluminiumMaterial->ChangeMaterialConstants();
    fAluminiumMaterial->FillIntensities();
    fSmallChamberMaterial->ChangeMaterialConstants();
    fSmallChamberMaterial->FillIntensities();
  } else if (material == "plexiglass") {
    fPlexiglass->ChangeMaterialConstants();
    fPlexiglass->FillIntensities();
  } else if (material == "pa6") {
    fSmallChamberRun7Material->ChangeMaterialConstants();
    fSmallChamberRun7Material->FillIntensities();
  } else {
    G4Exception(
      "DetectorConstruction", "DC01", FatalException, "Wrong material ID given to reload"
    );
  }
}

void DetectorConstruction::InitializeMaterials()
{
  G4NistManager* nistManager = G4NistManager::Instance();

  //! Air
  nistManager->FindOrBuildMaterial("G4_AIR");
  fAir = new MaterialExtension(
    MaterialParameters::MaterialID::mAir, "air", G4Material::GetMaterial("G4_AIR")
  );

  //! Kapton
  nistManager->FindOrBuildMaterial("G4_KAPTON");
  fKapton = new MaterialExtension(
    MaterialParameters::MaterialID::mKapton, "kapton",
    G4Material::GetMaterial("G4_KAPTON")
  );
  nistManager->FindOrBuildMaterial("G4_Galactic");
  fVacuum = new MaterialExtension(
    MaterialParameters::MaterialID::mAir, "vacuum",
    G4Material::GetMaterial("G4_Galactic")
  );

  //! Plexiglass
  nistManager->FindOrBuildMaterial("G4_PLEXIGLASS");
  fPlexiglass = new MaterialExtension(
    MaterialParameters::MaterialID::mPlexiglass, "bigChamberRun6",
    G4Material::GetMaterial("G4_PLEXIGLASS")
  );
  fPlexiglass->AllowsAnnihilations(true);

  //! XAD material, ref: https://www.sigmaaldrich.com/catalog/product/sigma/xad4
  nistManager->FindOrBuildMaterial("G4_POLYSTYRENE");
  fXADMaterial = new MaterialExtension(
    MaterialParameters::MaterialID::mXAD4, "XAD",
    G4Material::GetMaterial("G4_POLYSTYRENE")
  );
  fXADMaterial->AllowsAnnihilations(true);

  //! Scintillator material
  nistManager->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
  fScinMaterial = new MaterialExtension(
    MaterialParameters::MaterialID::mScin, "scinMaterial",
    G4Material::GetMaterial("G4_PLASTIC_SC_VINYLTOLUENE")
  );

  //! Aluminium
  nistManager->FindOrBuildMaterial("G4_Al");
  fAluminiumMaterial = new MaterialExtension(
    MaterialParameters::MaterialID::mAl, "aluminium",
    G4Material::GetMaterial("G4_Al")
  );
  fAluminiumMaterial->AllowsAnnihilations(true);
  //! Small chamber is also made out of aluminium
  fSmallChamberMaterial = new MaterialExtension(
    MaterialParameters::MaterialID::mAl, "smallChamber",
    G4Material::GetMaterial("G4_Al")
  );
  fSmallChamberMaterial->AllowsAnnihilations(true);

  //! Polyamide PA6
  nistManager->FindOrBuildMaterial("G4_NYLON-6-6");
  fSmallChamberRun7Material = new MaterialExtension(
    MaterialParameters::MaterialID::mPA6, "smallChamberRun7",
    G4Material::GetMaterial("G4_NYLON-6-6")
  );
  fSmallChamberRun7Material->AllowsAnnihilations(true);
}

/**
 * This method uses CAD library to read stl file
 * with construction of J-PET metal frame. Note that
 * in the file the scintillator slots were made bigger
 *          length    width
 *  lab     2.1 cm    0.9 cm
 *  file    2.6 cm    1.7 cm
 */
void DetectorConstruction::ConstructFrameCAD()
{
  CADMesh* mesh1 = new CADMesh((char*) "stl_geometry/Frame_JPET.stl");
  mesh1->SetScale(mm);

  G4VSolid* cad_solid1 = mesh1->TessellatedMesh();
  G4LogicalVolume* cad_logical = new G4LogicalVolume(cad_solid1, fAluminiumMaterial, "cad_logical");

  G4VisAttributes* detVisAtt = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9));
  detVisAtt->SetForceWireframe(true);
  detVisAtt->SetForceSolid(true);
  cad_logical->SetVisAttributes(detVisAtt);

  G4RotationMatrix rot = G4RotationMatrix();
  rot.rotateY(90 * deg);
  G4ThreeVector loc = G4ThreeVector(0 * cm, 306.5 * cm, -23 * cm);
  G4Transform3D transform(rot, loc);
  new G4PVPlacement(
    transform, cad_logical, "cadGeom", fWorldLogical, true, 0, checkOverlaps
  );
}

void DetectorConstruction::ConstructScintillators()
{
  G4Box* scinBox = new G4Box(
    "scinBox", DetectorConstants::scinDim[0] / 2.0,
    DetectorConstants::scinDim[1] / 2.0, DetectorConstants::scinDim[2] / 2.0
  );

  fScinLog = new G4LogicalVolume(scinBox, fScinMaterial, "scinLogical");
  G4VisAttributes* boxVisAtt = new G4VisAttributes(G4Colour(0.447059, 0.623529, 0.811765));
  boxVisAtt->SetForceWireframe(true);
  boxVisAtt->SetForceSolid(true);
  fScinLog->SetVisAttributes(boxVisAtt);

  G4Box* scinBoxFree = new G4Box(
    "scinBoxFree",
    DetectorConstants::scinDim[0] / 2.0 + DetectorConstants::wrappingShift,
    DetectorConstants::scinDim[1] / 2.0 + DetectorConstants::wrappingShift,
    DetectorConstants::scinDim[2] / 2.0
  );

  G4Box* wrappingBox = new G4Box(
    "wrappingBox",
    DetectorConstants::scinDim[0] / 2.0 + DetectorConstants::wrappingThickness,
    DetectorConstants::scinDim[1] / 2.0 + DetectorConstants::wrappingThickness,
    DetectorConstants::scinDim[2] / 2.0 - 1 * cm
  );

  G4LogicalVolume* wrappingLog;
  G4VisAttributes* boxVisAttWrapping = new G4VisAttributes(G4Colour(0.447059, 0.623529, 0.811765));
  boxVisAttWrapping->SetForceWireframe(true);
  boxVisAttWrapping->SetForceSolid(true);

  G4int icopy = 1;
  G4int oldLayerNumber = fLayerNumber;
  G4int moduleNumber = 0;
  for (int j = 0; j < DetectorConstants::layers; j++) {
    fLayerNumber = oldLayerNumber + j + 1;
    Layer layTemp(fLayerNumber, "Layer nr " + std::to_string(fLayerNumber), DetectorConstants::radius[j]/10 /*to cm*/, fSetup.fId);
    fLayerContainer.push_back(layTemp);
    fSetup.fDescription = "Setup with " + std::to_string(fLayerNumber) + " layers";
    for (int i = 0; i < DetectorConstants::nSegments[j]; i++) {
      moduleNumber++;
      G4double phi = i * 2 * M_PI / DetectorConstants::nSegments[j];
      G4double fi = M_PI / DetectorConstants::nSegments[j];

      if (j == 0) fi = 0.0;

      G4RotationMatrix rot = G4RotationMatrix();
      rot.rotateZ(phi + fi);

      G4ThreeVector loc = G4ThreeVector(
        DetectorConstants::radius[j] * (cos(phi + fi)),
        DetectorConstants::radius[j] * (sin(phi + fi)),
        0.0
      );

      G4Transform3D transform(rot, loc);
      G4String name = "scin_" + G4UIcommand::ConvertToString(icopy);
      
      if (fCreateGeometryFile) {
        for (unsigned i=0; i<4; i++) {
          Channel channTemp(fChannelNumber+i+1, 2*(moduleNumber - 1)+1, i+1, (i+1)*80);
          fChannelContainer.push_back(channTemp);
          
          Channel channTemp2(fChannelNumber+i+5, 2*(moduleNumber - 1)+2, i+1, (i+1)*80);
          fChannelContainer.push_back(channTemp2);
        }
        PM pmTemp(2*(moduleNumber - 1)+1, std::to_string(2*(moduleNumber - 1)+1), moduleNumber, (int)moduleNumber, "A");
        fPmContainer.push_back(pmTemp);
        PM pmTemp2(2*(moduleNumber - 1)+2, std::to_string(2*(moduleNumber - 1)+2), moduleNumber, (int)moduleNumber, "B");
        fPmContainer.push_back(pmTemp2);
        Scin scinTemp(moduleNumber, moduleNumber, DetectorConstants::scinDim[0], DetectorConstants::scinDim[1], DetectorConstants::scinDim[2],
                        DetectorConstants::radius[j]*cos((phi+fi)*180/M_PI)/10, DetectorConstants::radius[j]*sin((phi+fi)*180/M_PI)/10, 0);
        fScinContainer.push_back(scinTemp);
        Slot slotTemp(moduleNumber, fLayerNumber, (phi+fi)*180/M_PI, "single");
        fSlotContainer.push_back(slotTemp);
        fChannelNumber+=8;     // Because 4 thresholds for two sides - 4*2
      }
    
      new G4PVPlacement(
        transform, fScinLog, name, fWorldLogical, true, icopy, checkOverlaps
      );

      if (fLoadWrapping) {
        G4VSolid* unionSolid = new G4SubtractionSolid(
          "wrapping", wrappingBox, scinBoxFree
        );
        wrappingLog = new G4LogicalVolume(unionSolid, fKapton, "wrappingLogical");
        wrappingLog->SetVisAttributes(boxVisAttWrapping);
        G4String nameWrapping = "wrapping_" + G4UIcommand::ConvertToString(icopy);
        new G4PVPlacement(
          transform, wrappingLog, nameWrapping, fWorldLogical, true, icopy, checkOverlaps
        );
      }
      icopy++;
    }
  }
}

/**
 * Construction of modular layer (4th) - added by S. Sharma 20.06.2018
 */
void DetectorConstruction::ConstructScintillatorsModularLayer()
{
  G4Box* scinBoxInModule = new G4Box(
    "scinBoxInModule",
    DetectorConstants::scinDim_inModule[0] / 2.0,
    DetectorConstants::scinDim_inModule[1] / 2.0,
    DetectorConstants::scinDim_inModule[2] / 2.0
  );

  fScinLogInModule = new G4LogicalVolume(scinBoxInModule, fScinMaterial, "scinBoxInModule");

  G4VisAttributes* boxVisAttI = new G4VisAttributes(G4Colour(0.105, 0.210, 0.210, 0.9));
  boxVisAttI->SetForceWireframe(true);
  boxVisAttI->SetForceSolid(true);
  fScinLogInModule->SetVisAttributes(boxVisAttI);

  //! 13 straight scintillators in single module
  //! radius of each scintillator in tabular form
  //! taken from equation: radius0/cos(j*displacement) where
  //! j -> number of scintillator
  //! radius0 = 38.186*cm
  //! displacement -> Angular displacement
  //! (1.04 degree / 0.01815 radius - fixed; value provided by Sushil)

  const G4double radius_24[13] = {
    38.416, 38.346, 38.289, 38.244, 38.212, 38.192, 38.186,
    38.192, 38.212, 38.244, 38.289, 38.346, 38.416
  };

  const G4double radius_8[13] = {
    13.4037, 13.2011, 13.0330, 12.9007, 12.8055, 12.7479, 12.7287,
    12.7479, 12.8055, 12.9007, 13.0330, 13.2011, 13.4037
  };

  const G4double radius_16[13] = {
    25.8015, 25.69680, 25.61085, 25.54379, 25.49579, 25.4669, 25.45733,
    25.4669, 25.49579, 25.54379, 25.61085, 25.6968,  25.8015
  };

  std::vector<G4double> radius_dynamic = std::vector<G4double>(13, 0.0);

  //! starting ID for modular layer
  G4int icopyI = 201;

  G4double angDisp_8 = 0.0531204920;  // 3.0435^0
  G4double angDisp_16 = 0.0272515011; // 1.561396^0
  G4double angDisp_24 = 0.01815;      // 1.04^0

  G4double angDisp_dynamic = 0;
  G4int numberofModules = 0;

  // Assume: enum GeometryKind { Geo24ModulesLayer, Geo24ModulesLayerDistributed};
  // Single : for 24 modules layer
  // Double : for 8 and 16 layer configuration

  switch (fGeoKind) {
    case GeometryKind::Geo24ModulesLayer:
      for (int i = 0; i < 13; i++) {
        radius_dynamic[i] = radius_24[i];
      }
      numberofModules = 24;
      angDisp_dynamic = angDisp_24;
      ConstructLayers(radius_dynamic, numberofModules, angDisp_dynamic, icopyI);
      break;
    case GeometryKind::Geo24ModulesLayerDistributed:
      for (int i = 0; i < 13; i++) {
        radius_dynamic[i] = radius_8[i];
      }
      numberofModules = 8;
      angDisp_dynamic = angDisp_8;
      ConstructLayers(radius_dynamic, numberofModules, angDisp_dynamic, icopyI);
      for (int i = 0; i < 13; i++) {
        radius_dynamic[i] = radius_16[i];
      }
      numberofModules = 16;
      angDisp_dynamic = angDisp_16;
      icopyI += 8 * 13;
      ConstructLayers(radius_dynamic, numberofModules, angDisp_dynamic, icopyI);
      break;
    default:
      G4cout << " Wrong option for modular setup: choose either Single or Double" << G4endl;
      break;
  }
}

void DetectorConstruction::ConstructLayers(std::vector<G4double>& radius_dynamic, G4int& numberofModules, 
                                           G4double& angDisp_dynamic, G4int& icopyI)
{
  G4double phi = 0.0;
  G4double phi1 = 0.0;
  fLayerNumber++;
  
  Layer layTemp(fLayerNumber, "Layer nr " + std::to_string(fLayerNumber), radius_dynamic[6], fSetup.fId);
  fLayerContainer.push_back(layTemp);
  fSetup.fDescription = "Setup with " + std::to_string(fLayerNumber) + " layers";
  
  G4int moduleNumber = 0;
  for (int i = 0; i < numberofModules; i++){
    phi = (i * 2 * M_PI / numberofModules);
    for (int j = -6; j < 7; j++) {
      phi1 = phi + j * angDisp_dynamic;
      G4double radius_new = radius_dynamic[j + 6] * cm;
      G4RotationMatrix rot = G4RotationMatrix();
      rot.rotateZ(phi);
      G4ThreeVector loc = G4ThreeVector(radius_new * cos(phi1), radius_new * sin(phi1), 0.0);
      G4Transform3D transform(rot, loc);
      moduleNumber = icopyI + i * 13 + j + 6;
      G4String nameNewI = "scin_" + G4UIcommand::ConvertToString(moduleNumber);
      
      if (fCreateGeometryFile) {
        for (unsigned i=0; i<2; i++) {
          Channel channTemp(fChannelNumber+i+1, 2*(moduleNumber - 1)+1, i+1, 0);
          fChannelContainer.push_back(channTemp);
          
          Channel channTemp2(fChannelNumber+i+3, 2*(moduleNumber - 1)+2, i+1, 0);
          fChannelContainer.push_back(channTemp2);
        }
        PM pmTemp(2*(moduleNumber - 1)+1, std::to_string(2*(moduleNumber - 1)+1), (2*(moduleNumber - 1)+1)%4 + 1, 
                        (int)moduleNumber, "A");
        fPmContainer.push_back(pmTemp);
        PM pmTemp2(2*(moduleNumber - 1)+2, std::to_string(2*(moduleNumber - 1)+2), (2*(moduleNumber - 1)+2)%4 + 1, 
                        (int)moduleNumber, "B");
        fPmContainer.push_back(pmTemp2);
        Scin scinTemp(moduleNumber, moduleNumber, DetectorConstants::scinDim_inModule[0], DetectorConstants::scinDim_inModule[1], 
                      DetectorConstants::scinDim_inModule[2], (radius_new/cm)*cos(phi1*180/M_PI), (radius_new/cm)*sin(phi1*180/M_PI), 0);
        fScinContainer.push_back(scinTemp);
        Slot slotTemp(moduleNumber, fLayerNumber, phi*180/M_PI, "module");
        fSlotContainer.push_back(slotTemp);
        fChannelNumber+=4;     // Beacuse 2 thresholds for 2 sides - 2*2
      }
      
      new G4PVPlacement(
        transform, fScinLogInModule, nameNewI, fWorldLogical,
        true, icopyI + i * 13 + j + 6, checkOverlaps
      );
    }
  }
}

/**
 * Method for construction of setup used in Run 3
 * Please note that chamber is symmetric in z,
 * therefore only half of the table is presented.
 * Chamber has four different outer radii.
 *  z         |  r Inner | r Outer | Comment
 * -----------|----------|---------|-------
 *  -37.00*cm |  0*cm    | 3*cm    | chamber end
 *  -32.61*cm |  0*cm    | 3*cm    | start endcup
 *  -32.60*cm |  0*cm    | 10*cm   | end slope
 *  -31.10*cm |  0*cm    | 10*cm   | start slope
 *  -31.00*cm |  7.1*cm  | 7.5*cm  | end slope and start empty volume
 */
void DetectorConstruction::ConstructTargetRun3()
{
  const double chamber_radius_inner = 7.1 * cm;
  const double chamber_radius_outer = 7.5 * cm;
  const double chamber_endcup_min = 3.0 * cm;
  const double chamber_endcup_max = 10.0 * cm;

  G4RotationMatrix rot = G4RotationMatrix();

  G4double z[] = {
    -37 * cm, -32.61 * cm, -32.6 * cm, -31.1 * cm, -31 * cm,
    31 * cm, 31.1 * cm, 32.6 * cm, 32.61 * cm, 37 * cm
  };

  G4double rInner[] = {
    0 * cm, 0 * cm, 0 * cm, 0 * cm, chamber_radius_inner,
    chamber_radius_inner, 0 * cm, 0 * cm, 0 * cm, 0 * cm
  };

  G4double rOuter[] = {
    chamber_endcup_min, chamber_endcup_min, chamber_endcup_max,
    chamber_endcup_max, chamber_radius_outer, chamber_radius_outer,
    chamber_endcup_max, chamber_endcup_max, chamber_endcup_min,
    chamber_endcup_min
  };

  G4Polycone* bigChamber = new G4Polycone(
    "bigChamber", 0 * degree, 360 * degree, 10, z, rInner, rOuter
  );

  G4LogicalVolume* bigChamber_logical = new G4LogicalVolume(
    bigChamber, fAluminiumMaterial, "bigChamber_logical"
  );

  G4VisAttributes* detVisAtt = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9));
  detVisAtt->SetForceWireframe(true);
  detVisAtt->SetForceSolid(true);
  bigChamber_logical->SetVisAttributes(detVisAtt);

  G4ThreeVector loc = DetectorConstants::GetChamberCenter();
  G4Transform3D transform(rot, loc);

  new G4PVPlacement(
    transform, bigChamber_logical, "bigChamberGeom",
    fWorldLogical, true, 0, checkOverlaps
  );

  G4Tubs* ringInner = new G4Tubs(
    "ringInner", 15 * mm, 20.8 * mm, 0.8 * mm, 0 * degree, 360 * degree
  );

  G4Box* conn = new G4Box("conn", 25 * mm, 7. * mm, 0.8 * mm);
  G4LogicalVolume* conn_logical = new G4LogicalVolume(
    conn, fAluminiumMaterial, "conn_logical"
  );
  conn_logical->SetVisAttributes(detVisAtt);

  G4ThreeVector loc2;
  G4Transform3D transform2;

  loc2 = G4ThreeVector(39.8 * mm, 0.0, 0.0) + DetectorConstants::GetChamberCenter();
  transform2 = G4Transform3D(rot, loc2);
  G4UnionSolid* unionSolid = new G4UnionSolid("c1", ringInner, conn, transform2);

  loc2 = G4ThreeVector(-39.8 * mm, 0.0, 0.0) + DetectorConstants::GetChamberCenter();
  transform2 = G4Transform3D(rot, loc2);
  unionSolid = new G4UnionSolid("c2", unionSolid, conn, transform2);

  loc2 = G4ThreeVector(0.0, 39.8 * mm, 0.0) + DetectorConstants::GetChamberCenter();
  transform2 = G4Transform3D(rot.rotateZ(90 * degree), loc2);
  unionSolid = new G4UnionSolid("c3", unionSolid, conn, transform2);

  loc2 = G4ThreeVector(0.0, -39.8 * mm, 0.0) + DetectorConstants::GetChamberCenter();
  transform2 = G4Transform3D(rot, loc2);
  unionSolid = new G4UnionSolid("c4", unionSolid, conn, transform2);

  G4Tubs* ringOuter = new G4Tubs(
    "ringOuter", 60 * mm, 70 * mm, 0.8 * mm, 0 * degree, 360 * degree
  );
  unionSolid = new G4UnionSolid("c5", unionSolid, ringOuter);

  G4LogicalVolume* unionSolid_logical = new G4LogicalVolume(
    unionSolid, fAluminiumMaterial, "union_logical"
  );
  unionSolid_logical->SetVisAttributes(detVisAtt);

  new G4PVPlacement(
    transform, unionSolid_logical, "bigChamberInnerStructure",
    fWorldLogical, true, 0, checkOverlaps
  );
}

/**
 * Method for construction of setup used in Run 5
 * Parameters taken from http://koza.if.uj.edu.pl/petwiki/index.php/Small_annihilation_chamber
 * below main values are written. Please note that chamber is symmetric in z,
 * therefore only half of the table is presented.
 * Chamber has four different outer radii.
 *  z       |  r Inner | r Outer | Comment
 * ---------|----------|---------|-------
 *  -7.6*cm | 0*cm     | 2.5*cm  | chamber end
 *  -7.0*cm | 0*cm     | 2.5*cm  | start slope (R1->R2)
 *  -6.9*cm | 1.5*cm   | 2.0*cm  | end slope and start empty space
 *  -4.3*cm | 1.5*cm   | 2.0*cm  | start slope (R2->R3)
 *  -4.2*cm | 1.5*cm   | 1.8*cm  | end slope
 *  -2.7*cm | 1.5*cm   | 1.8*cm  | start slope (R3->R4)
 *  -2.6*cm | 1.5*cm   | 1.57*cm | end slope
 */
void DetectorConstruction::ConstructTargetRun5()
{
  const double chamber_radius_inner = 1.5 * cm;
  const double chamber_radius_outer_1 = 2.5 * cm;
  const double chamber_radius_outer_2 = 2.0 * cm;
  const double chamber_radius_outer_3 = 1.8 * cm;
  const double chamber_radius_outer_4 = 1.57 * cm;
  const double xadFilling_halfthickness = 0.6 * cm;

  G4RotationMatrix rot = G4RotationMatrix();
  G4double z[] = {
    -7.6 * cm, -7.0 * cm, -6.9 * cm, -4.3 * cm, -4.2 * cm, -2.7 * cm, -2.6 * cm,
    2.6 * cm,  2.7 * cm,  4.2 * cm,  4.3 * cm,  6.9 * cm,  7.0 * cm,  7.6 * cm
  };

  G4double rInner[] = {
    0 * cm, 0 * cm, chamber_radius_inner, chamber_radius_inner, chamber_radius_inner,
    chamber_radius_inner, chamber_radius_inner, chamber_radius_inner, chamber_radius_inner,
    chamber_radius_inner, chamber_radius_inner, chamber_radius_inner, 0 * cm, 0 * cm
  };

  G4double rOuter[] = {
    chamber_radius_outer_1, chamber_radius_outer_1, chamber_radius_outer_2,
    chamber_radius_outer_2, chamber_radius_outer_3, chamber_radius_outer_3,
    chamber_radius_outer_4, chamber_radius_outer_4, chamber_radius_outer_3,
    chamber_radius_outer_3, chamber_radius_outer_2, chamber_radius_outer_2,
    chamber_radius_outer_1, chamber_radius_outer_1
  };

  G4Polycone* smallChamber = new G4Polycone(
    "bigChamber", 0 * degree, 360 * degree, 14, z, rInner, rOuter
  );

  G4LogicalVolume* smallChamber_logical = new G4LogicalVolume(
    smallChamber, fSmallChamberMaterial, "smallChamber_logical"
  );

  G4VisAttributes* detVisAtt = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9));
  detVisAtt->SetForceWireframe(true);
  detVisAtt->SetForceSolid(true);
  smallChamber_logical->SetVisAttributes(detVisAtt);

  G4ThreeVector loc = DetectorConstants::GetChamberCenter();
  G4Transform3D transform(rot, loc);
  new G4PVPlacement(
    transform, smallChamber_logical, "smallChamberGeom",
    fWorldLogical, true, 0, checkOverlaps
  );

  G4Tubs* xadFilling = new G4Tubs(
    "xadFilling", 0 * cm, chamber_radius_inner - 0.01 * cm,
    xadFilling_halfthickness, 0 * degree, 360 * degree
  );
  G4LogicalVolume* xadFilling_logical = new G4LogicalVolume(
    xadFilling, fXADMaterial, "xadFilling_logical"
  );
  G4VisAttributes* xadVisAtt = new G4VisAttributes(G4Colour(0.2, 0.3, 0.5));
  xadVisAtt->SetForceWireframe(true);
  xadVisAtt->SetForceSolid(true);
  xadFilling_logical->SetVisAttributes(xadVisAtt);

  new G4PVPlacement(
    transform, xadFilling_logical, "xadFillingGeom",
    fWorldLogical, true, 0, checkOverlaps
  );
}

/**
 * Method for construction of setup used in Run 6
 * Parameters taken from http://koza.if.uj.edu.pl/petwiki/images/7/71/AnnihilationChamberRun6.pdf
 * below main values are written. Please note that chamber is symmetric in z,
 * therefore only half of the table is presented.
 *  z        | r Inner  | r Outer | Comment
 * ----------|----------|---------|-------
 *  -31.0*cm | 0*cm     | 12.7*cm | chamber end
 *  -29.0*cm | 0*cm     | 12.7*cm | start slope before endcup
 *  -28.9*cm | 0*cm     | 15*cm   | stop slope before endcup
 *  -28.0*cm | 0*cm     | 15*cm   | endcup
 *  -27.9*cm | 12.2*cm  | 12.5*cm | end slope after endcup and start empty inside
 */
void DetectorConstruction::ConstructTargetRun6()
{
  //! Annihilation Chamber part
  const double chamber_radius_inner = 12.2 * cm;
  const double chamber_radius_outer = 12.5 * cm;
  const double chamber_endcup_min = 12.7 * cm;
  const double chamber_endcup_max = 15.0 * cm;
  const double xad_z_coverage = 27.5 * cm;
  const double xad_halfthickness = 0.2 * cm;
  const double source_holder_connector_halfthickness = 1.125 * mm;
  const double source_holder_connector_width = 4.5 * mm;
  const double source_holder_connector_height = 44.5 * mm;
  const double source_holder_radius_inner = 24 * mm;
  const double source_holder_radius_outer = 31 * mm;
  const double source_holder_radius_halfthickness = 1.78 * mm;
  const double kapton_foil_halfthickness = 0.1 * cm;

  G4RotationMatrix rot = G4RotationMatrix();

  G4double z[] = {
    -31.0 * cm, -29.0 * cm, -28.9 * cm, -28 * cm, -27.9 * cm,
    27.9 * cm, 28 * cm, 28.9 * cm, 29 * cm, 31 * cm
  };

  G4double rInner[] = {
    0 * cm, 0 * cm, 0 * cm, 0 * cm, chamber_radius_inner,
    chamber_radius_inner, 0 * cm, 0 * cm, 0 * cm, 0 * cm
  };

  G4double rOuter[] = {
    chamber_endcup_min, chamber_endcup_min, chamber_endcup_max, chamber_endcup_max,
    chamber_radius_outer, chamber_radius_outer, chamber_endcup_max,
    chamber_endcup_max, chamber_endcup_min, chamber_endcup_min
  };

  G4Polycone* bigChamber = new G4Polycone(
    "bigChamber", 0 * degree, 360 * degree, 10, z, rInner, rOuter
  );

  G4LogicalVolume* bigChamber_logical = new G4LogicalVolume(
    bigChamber, fPlexiglass, "bigChamber_logical"
  );

  G4VisAttributes* detVisAtt = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9));
  detVisAtt->SetForceWireframe(true);
  detVisAtt->SetForceSolid(true);
  bigChamber_logical->SetVisAttributes(detVisAtt);

  G4ThreeVector loc = DetectorConstants::GetChamberCenter();
  G4Transform3D transform(rot, loc);
  new G4PVPlacement(
    transform, bigChamber_logical, "bigChamberGeom",
    fWorldLogical, true, 0, checkOverlaps
  );

  G4Tubs* ringInner = new G4Tubs(
    "ringInner", source_holder_radius_inner, source_holder_radius_outer,
    source_holder_radius_halfthickness, 0 * degree, 360 * degree
  );

  G4Box* conn = new G4Box(
    "conn", source_holder_connector_height, source_holder_connector_width,
    source_holder_connector_halfthickness
  );

  G4LogicalVolume* conn_logical = new G4LogicalVolume(
    conn, fAluminiumMaterial, "conn_logical"
  );
  conn_logical->SetVisAttributes(detVisAtt);

  G4ThreeVector loc2;
  G4Transform3D transform2;

  loc2 = G4ThreeVector(75. * mm, 0.0, 0.0);
  transform2 = G4Transform3D(rot, loc2);
  G4UnionSolid* unionSolid = new G4UnionSolid("c1", ringInner, conn, transform2);

  loc2 = G4ThreeVector(-53. * mm, 53. * mm, 0.0);
  transform2 = G4Transform3D(rot.rotateZ(-45 * degree), loc2);
  unionSolid = new G4UnionSolid("c2", unionSolid, conn, transform2);

  loc2 = G4ThreeVector(-53. * mm, -53. * mm, 0.0);
  transform2 = G4Transform3D(rot.rotateZ(90 * degree), loc2);
  unionSolid = new G4UnionSolid("c3", unionSolid, conn, transform2);

  G4LogicalVolume* unionSolid_logical = new G4LogicalVolume(
    unionSolid, fAluminiumMaterial, "union_logical"
  );
  unionSolid_logical->SetVisAttributes(detVisAtt);

  new G4PVPlacement(
    transform, unionSolid_logical, "bigChamberInnerStructure",
    fWorldLogical, true, 0, checkOverlaps
  );

  //! XAD filling part
  G4Tubs* xadFilling = new G4Tubs(
    "xadFilling", chamber_radius_inner - 0.1 * cm - xad_halfthickness,
    chamber_radius_inner - 0.1 * cm, xad_z_coverage, 0 * degree, 360 * degree
  );
  G4LogicalVolume* xadFilling_logical = new G4LogicalVolume(
    xadFilling, fXADMaterial, "xadFilling_logical"
  );
  G4VisAttributes* xadVisAtt = new G4VisAttributes(G4Colour(0.2, 0.3, 0.5));
  xadVisAtt->SetForceWireframe(true);
  xadVisAtt->SetForceSolid(true);
  xadFilling_logical->SetVisAttributes(xadVisAtt);

  new G4PVPlacement(
    transform, xadFilling_logical, "xadFillingGeom",
    fWorldLogical, true, 0, checkOverlaps
  );

  //! Kapton foil part
  G4Tubs* kaptonFilling = new G4Tubs(
    "kaptonFilling", 0. * cm, source_holder_radius_inner - 0.01 * mm,
    kapton_foil_halfthickness, 0 * degree, 360 * degree
  );
  G4LogicalVolume* kaptonFilling_logical = new G4LogicalVolume(
    kaptonFilling, fKapton, "kaptonFilling_logical"
  );
  G4VisAttributes* kaptonVisAtt = new G4VisAttributes(G4Colour(0.2, 0.3, 0.5));
  kaptonVisAtt->SetForceWireframe(true);
  kaptonVisAtt->SetForceSolid(true);
  kaptonFilling_logical->SetVisAttributes(kaptonVisAtt);

  new G4PVPlacement(
    transform, kaptonFilling_logical, "kaptonFillingGeom",
    fWorldLogical, true, 0, checkOverlaps
  );
}

void DetectorConstruction::ConstructTargetRun7()
{
  G4RotationMatrix rot = G4RotationMatrix();

  G4double z[] = {
    -8.8 * cm, -7.8 * cm, -7.8 * cm, -6.6 * cm, -6.6 * cm, -5.1 * cm, -4.9 * cm,
    -3.1 * cm, -2.8 * cm, -0.57 * cm, 0.57 * cm, 2.8 * cm,  3.1 * cm,  4.9 * cm,
    5.1 * cm,  6.6 * cm,  6.6 * cm,  7.8 * cm,  7.8 * cm,  8.8 * cm
  };

  G4double rInner[] = {
    0. * cm,  0.6 * cm,  0.6 * cm, 0.6 * cm, 0.6 * cm, 0.6 * cm, 0.6 * cm,
    0.6 * cm, 0.54 * cm, 0.5 * cm, 0.5 * cm, 0.54 * cm, 0.6 * cm, 0.6 * cm,
    0.6 * cm, 0.6 * cm, 0.6 * cm, 0.6 * cm, 0.6 * cm,  0. * cm
  };

  G4double rOuter[] = {
    1.75 * cm, 1.75 * cm, 1.35 * cm, 1.35 * cm, 1.0 * cm, 1.0 * cm, 1.0 * cm,
    1.0 * cm, 0.6 * cm, 0.6 * cm, 0.6 * cm, 0.6 * cm, 1.0 * cm, 1.0 * cm,
    1.0 * cm, 1.0 * cm, 1.35 * cm, 1.35 * cm, 1.75 * cm, 1.75 * cm
  };

  G4Polycone* smallChamberRun7 = new G4Polycone(
    "smallchamberRun7", 0 * degree, 360 * degree, 19, z, rInner, rOuter
  );

  G4LogicalVolume* smallChamber_logical = new G4LogicalVolume(
    smallChamberRun7, fSmallChamberRun7Material, "smallChamberRun7_logical"
  );

  G4VisAttributes* detVisAtt = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9));
  detVisAtt->SetForceWireframe(true);
  detVisAtt->SetForceSolid(true);
  smallChamber_logical->SetVisAttributes(detVisAtt);

  G4ThreeVector loc = DetectorConstants::GetChamberCenter();
  G4Transform3D transform(rot, loc);
  new G4PVPlacement(
    transform, smallChamber_logical, "smallChamberRun7_logical",
    fWorldLogical, true, 0, checkOverlaps
  );

  G4Tubs* xadFilling = new G4Tubs(
    "xadFilling", 0 * cm, 0.4 * cm, 0.6 * cm, 0 * degree, 360 * degree
  );

  G4LogicalVolume* xadFilling_logical = new G4LogicalVolume(
    xadFilling, fXADMaterial, "xadFilling_logical"
  );

  G4VisAttributes* xadVisAtt = new G4VisAttributes(G4Colour(0.2, 0.3, 0.5));
  xadVisAtt->SetForceWireframe(true);
  xadVisAtt->SetForceSolid(true);
  xadFilling_logical->SetVisAttributes(xadVisAtt);

  new G4PVPlacement(
    transform, xadFilling_logical, "xadFillingGeom",
    fWorldLogical, true, 0, checkOverlaps
  );
}

void DetectorConstruction::CreateGeometryFile()
{
  std::ofstream fileWithGeometry;
  std::string fileName = "newGeometry.json";
  fileWithGeometry.open(fileName, std::ios::out);
  G4cout << "--- Saving simulated geometry to " << fileName << " ---" << G4endl;
  if (fileWithGeometry.is_open()) {
    if (fCreateOldGeometryFileStyle) {
      int channelShift = 260;
      fileWithGeometry << "{\n    \"" << fRunNumber+90 << "\": {\n      \"FEBs\": [\n";
      fileWithGeometry << "         {\n            \"active\": \"true\",\n";
      fileWithGeometry << "            \"no_time_outputs_per_input\": 1,\n";
      fileWithGeometry << "            \"id\": 1,\n            \"status\": \"OK\",\n";
      fileWithGeometry << "            \"version\": 1,\n            \"TRBs_id\": 1,\n";
      fileWithGeometry << "            \"time_outputs_per_input\": 4,\n            \"creator_id\": 1,\n";
      fileWithGeometry << "            \"description\": \"\"\n         }\n      ],\n";
      fileWithGeometry << "      \"TRBs\": [\n         {\n";
      fileWithGeometry << "            \"channel\": " << channelShift << ",\n            \"id\": 1,\n";
      fileWithGeometry << "            \"type\": 0\n         }\n      ],\n";
      fileWithGeometry << "      \"scintillators\": [\n";
      for (unsigned i=0; i<fScinContainer.size(); i++) {
        fileWithGeometry <<= fScinContainer[i];
        if (i<fScinContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n";
      fileWithGeometry << "      \"barrelSlots\": [\n";
      for (unsigned i=0; i<fSlotContainer.size(); i++) {
        fileWithGeometry <<= fSlotContainer[i];
        if (i<fSlotContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n      \"PMCalibs\": \"\",\n      \"PMs\": [\n";
      for (unsigned i=0; i<fPmContainer.size(); i++) {
        fileWithGeometry <<= fPmContainer[i];
        if (i<fPmContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n      \"TOMBChannels\": [\n";
      for (unsigned i=0; i<fChannelContainer.size(); i++) {
        fileWithGeometry <<= fChannelContainer[i];
        if (i<fChannelContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n      \"frames\": [\n";
      fileWithGeometry << "         {\n            \"active\": \"true\",\n";
      fileWithGeometry << "            \"id\": 1,\n            \"status\": \"OK\",\n";
      fileWithGeometry << "            \"version\": 1,\n            \"creator_id\": 1,\n";
      fileWithGeometry << "            \"description\": \"Big barrel stub setup\"\n";
      fileWithGeometry << "         }\n      ],\n      \"layers\": [\n";
      for (unsigned i=0; i<fLayerContainer.size(); i++) {
        fileWithGeometry <<= fLayerContainer[i];
        if (i<fLayerContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ]\n  }\n}" << std::endl;
      fileWithGeometry.close();
    } else {
      fileWithGeometry << "{\n  \"" << fRunNumber+90 << "\": {\n      \"channel\" : [\n";
      for (unsigned i=0; i<fChannelContainer.size(); i++) {
        fileWithGeometry << fChannelContainer[i];
        if (i<fChannelContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n";
      fileWithGeometry << "      \"layer\" : [\n";
      for (unsigned i=0; i<fLayerContainer.size(); i++) {
        fileWithGeometry << fLayerContainer[i];
        if (i<fLayerContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n";
      fileWithGeometry << "      \"pm\" : [\n";
      for (unsigned i=0; i<fPmContainer.size(); i++) {
        fileWithGeometry << fPmContainer[i];
        if (i<fPmContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n";
      fileWithGeometry << "      \"scin\" : [\n";
      for (unsigned i=0; i<fScinContainer.size(); i++) {
        fileWithGeometry << fScinContainer[i];
        if (i<fScinContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ],\n";
      fileWithGeometry << "      \"setup\" : [\n";
      fileWithGeometry << fSetup;
      fileWithGeometry << "\n      ],\n";
      fileWithGeometry << "      \"slot\" : [\n";
      for (unsigned i=0; i<fSlotContainer.size(); i++) {
        fileWithGeometry << fSlotContainer[i];
        if (i<fSlotContainer.size()-1)
          fileWithGeometry << ",";
        fileWithGeometry << "\n";
      }
      fileWithGeometry << "      ]\n    }\n}\n";
      fileWithGeometry.close();
    }
  }
  else {
    G4Exception("DetectorConstruction", "DC03",
      JustWarning, "Failed to open a file to save geometry");
  }
}
