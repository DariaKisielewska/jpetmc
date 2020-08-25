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
 *  @file JPetGeantDecayTree.cpp
 */

#include "JPetGeantDecayTree.h"
#include <iostream>

ClassImp(JPetGeantDecayTree)

JPetGeantDecayTree::JPetGeantDecayTree() {}

JPetGeantDecayTree::~JPetGeantDecayTree() 
{
  fNodeConnections.clear();
  fNodeInteractionType.clear();
}

void JPetGeantDecayTree::Clean()
{
  this->ClearVectors();
}

void JPetGeantDecayTree::ClearVectors() 
{
  fNodeConnections.clear();
  fNodeInteractionType.clear();
}

// cppcheck-suppress unusedFunction
InteractionType JPetGeantDecayTree::GetInteractionType(int nodeID, int trackID)
{
  for (int i=fNodeInteractionType.size()-1; i>=0; i--) {
    if (std::get<0>(fNodeInteractionType[i]) == nodeID && std::get<2>(fNodeInteractionType[i]) == trackID)
      return std::get<1>(fNodeInteractionType[i]);
  }
  return InteractionType::unknown;
}

// cppcheck-suppress unusedFunction
int JPetGeantDecayTree::GetPreviousNodeID(int nodeID, int trackID)
{
  int previousNodeID = nodeID;
  for (int i=fNodeConnections.size()-1; i>=0; i--) {
    if (std::get<0>(fNodeConnections[i]) == nodeID && std::get<1>(fNodeConnections[i]) != -1 
                                                    && std::get<2>(fNodeConnections[i]) == trackID)
      return std::get<1>(fNodeConnections[i]);
    else if (std::get<0>(fNodeConnections[i]) == nodeID && std::get<1>(fNodeConnections[i]) != -1)
      return std::get<1>(fNodeConnections[i]);
  }
  return previousNodeID;
}

// cppcheck-suppress unusedFunction
int JPetGeantDecayTree::GetPrimaryNodeID(int nodeID, int trackID)
{
  int previousNodeID = nodeID, primaryNodeID = nodeID;
  while (previousNodeID >= 10) {
    primaryNodeID = previousNodeID;
    previousNodeID = GetPreviousNodeID(primaryNodeID, trackID);
  }
  return previousNodeID;
}

void JPetGeantDecayTree::AddNode(int nodeID, int previousNodeID, int trackID, InteractionType interactionType)
{
  fNodeConnections.push_back(std::make_tuple(nodeID, previousNodeID, trackID));
  fNodeInteractionType.push_back(std::make_tuple(nodeID, interactionType, trackID));
}
