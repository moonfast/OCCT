// Created on: 2016-10-11
// Created by: Ilya SEVRIKOV
// Copyright (c) 2016 OPEN CASCADE SAS
//
// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.
//
// Alternatively, this file may be used under the terms of Open CASCADE
// commercial license or contractual agreement.

#include <V3d_Trihedron.hxx>

#include <gp_Ax3.hxx>
#include <Graphic3d_ArrayOfPolylines.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_ArrayOfTriangles.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Prs3d.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Prs3d_Text.hxx>
#include <Prs3d_TextAspect.hxx>
#include <StdPrs_ToolCylinder.hxx>
#include <StdPrs_ToolDisk.hxx>
#include <StdPrs_ToolSphere.hxx>
#include <V3d_View.hxx>

IMPLEMENT_STANDARD_RTTIEXT (V3d_Trihedron, Standard_Transient)

namespace
{
  //! Compensates difference between old implementation (without transform persistence) and current implementation.
  static const Standard_Real THE_INTERNAL_SCALE_FACTOR = 500.0;

  static const Standard_ShortReal THE_CYLINDER_LENGTH      = 0.75f;
  static const Standard_Integer   THE_CIRCLE_SERMENTS_NB   = 24;
  static const Standard_Real      THE_CIRCLE_SEGMENT_ANGLE = 2.0 * M_PI / THE_CIRCLE_SERMENTS_NB;
}

//! Dummy implementation of Graphic3d_Structure overriding ::Compute() method for handling Device Lost.
class V3d_Trihedron::TrihedronStructure : public Graphic3d_Structure
{
public:
  //! Main constructor.
  TrihedronStructure (const Handle(Graphic3d_StructureManager)& theManager, V3d_Trihedron* theTrihedron)
  : Graphic3d_Structure (theManager), myTrihedron (theTrihedron) {}

  //! Override method to redirect to V3d_Trihedron.
  virtual void Compute() Standard_OVERRIDE { myTrihedron->compute(); }

private:
  V3d_Trihedron* myTrihedron;
};

// ============================================================================
// function : V3d_Trihedron
// purpose  :
// ============================================================================
V3d_Trihedron::V3d_Trihedron()
: myScale      (1.0),
  myRatio      (0.8),
  myDiameter   (0.05),
  myNbFacettes (12),
  myIsWireframe(Standard_False),
  myToCompute  (Standard_True)
{
  myTransformPers = new Graphic3d_TransformPers (Graphic3d_TMF_TriedronPers, Aspect_TOTP_LEFT_LOWER);
  SetPosition (Aspect_TOTP_LEFT_LOWER);

  // Set material.
  Graphic3d_MaterialAspect aShadingMaterial;
  aShadingMaterial.SetReflectionModeOff (Graphic3d_TOR_SPECULAR);
  aShadingMaterial.SetMaterialType (Graphic3d_MATERIAL_ASPECT);

  for (Standard_Integer anIt = 0; anIt < 3; ++anIt)
  {
    myArrowShadingAspects[anIt] = new Prs3d_ShadingAspect();
    myArrowLineAspects[anIt]    = new Prs3d_LineAspect (Quantity_NOC_WHITE, Aspect_TOL_SOLID, 1.0);

    // mark texture map ON to actually disable environment map
    myArrowShadingAspects[anIt]->Aspect()->SetTextureMapOn();
    myArrowShadingAspects[anIt]->Aspect()->SetInteriorStyle (Aspect_IS_SOLID);
    myArrowShadingAspects[anIt]->SetMaterial (aShadingMaterial);
  }
  myArrowShadingAspects[0]->SetColor (Quantity_NOC_RED);
  myArrowLineAspects   [0]->SetColor (Quantity_NOC_RED);
  myArrowShadingAspects[1]->SetColor (Quantity_NOC_GREEN);
  myArrowLineAspects   [1]->SetColor (Quantity_NOC_GREEN);
  myArrowShadingAspects[2]->SetColor (Quantity_NOC_BLUE1);
  myArrowLineAspects   [2]->SetColor (Quantity_NOC_BLUE1);

  mySphereShadingAspect = new Prs3d_ShadingAspect();
  mySphereLineAspect    = new Prs3d_LineAspect (Quantity_NOC_WHITE, Aspect_TOL_SOLID, 1.0);

  // mark texture map ON to actually disable environment map
  mySphereShadingAspect->Aspect()->SetTextureMapOn();
  mySphereShadingAspect->Aspect()->SetInteriorStyle (Aspect_IS_SOLID);
  mySphereShadingAspect->SetMaterial (aShadingMaterial);
  mySphereShadingAspect->SetColor (Quantity_NOC_WHITE);

  myTextAspect = new Prs3d_TextAspect();
  myTextAspect->SetFont ("Courier");
  myTextAspect->SetHeight (16);
  myTextAspect->SetHorizontalJustification (Graphic3d_HTA_LEFT);
  myTextAspect->SetVerticalJustification (Graphic3d_VTA_BOTTOM);
}

// ============================================================================
// function : SetLabelsColor
// purpose  :
// ============================================================================
void V3d_Trihedron::SetLabelsColor (const Quantity_Color& theColor)
{
  myTextAspect->SetColor (theColor);
}

// ============================================================================
// function : SetArrowsColor
// purpose  :
// ============================================================================
void V3d_Trihedron::SetArrowsColor (const Quantity_Color& theXColor,
                                    const Quantity_Color& theYColor,
                                    const Quantity_Color& theZColor)
{
  const Quantity_Color aColors[3] = { theXColor, theYColor, theZColor };
  for (Standard_Integer anIt = 0; anIt < 3; ++anIt)
  {
    myArrowShadingAspects[anIt]->SetColor (aColors[anIt]);
    myArrowLineAspects   [anIt]->SetColor (aColors[anIt]);
  }
}

// ============================================================================
// function : SetScale
// purpose  :
// ============================================================================
void V3d_Trihedron::SetScale (const Standard_Real theScale)
{
  if (Abs (myScale - theScale) > Precision::Confusion())
  {
    invalidate();
  }
  myScale = theScale;
}

// =======================================================================
// function : SetSizeRatio
// purpose  :
// =======================================================================
void V3d_Trihedron::SetSizeRatio (const Standard_Real theRatio)
{
  if (Abs (myRatio - theRatio) > Precision::Confusion())
  {
    invalidate();
  }
  myRatio = theRatio;
}

// =======================================================================
// function : SetArrowDiameter
// purpose  :
// =======================================================================
void V3d_Trihedron::SetArrowDiameter (const Standard_Real theDiam)
{
  if (Abs (myDiameter - theDiam) > Precision::Confusion())
  {
    invalidate();
  }
  myDiameter = theDiam;
}

// =======================================================================
// function : SetNbFacets
// purpose  :
// =======================================================================
void V3d_Trihedron::SetNbFacets (const Standard_Integer theNbFacets)
{
  if (Abs (myNbFacettes - theNbFacets) > 0)
  {
    invalidate();
  }
  myNbFacettes = theNbFacets;
}

// ============================================================================
// function : Display
// purpose  :
// ============================================================================
void V3d_Trihedron::Display (const V3d_View& theView)
{
  if (myStructure.IsNull())
  {
    myStructure = new TrihedronStructure (theView.Viewer()->StructureManager(), this);
    myStructure->SetTransformPersistence (myTransformPers);
    myStructure->SetZLayer (Graphic3d_ZLayerId_Topmost);
    myStructure->SetDisplayPriority (9);
    myStructure->SetInfiniteState (Standard_True);
    myStructure->CStructure()->ViewAffinity = new Graphic3d_ViewAffinity();
    myStructure->CStructure()->ViewAffinity->SetVisible (Standard_False);
    myStructure->CStructure()->ViewAffinity->SetVisible (theView.View()->Identification(), true);
  }
  if (myToCompute)
  {
    compute();
  }

  myStructure->Display();
}

// ============================================================================
// function : Erase
// purpose  :
// ============================================================================
void V3d_Trihedron::Erase()
{
  if (!myStructure.IsNull())
  {
    myStructure->Erase();
    myStructure.Nullify();
  }
}

// ============================================================================
// function : SetPosition
// purpose  :
// ============================================================================
void V3d_Trihedron::SetPosition (const Aspect_TypeOfTriedronPosition thePosition)
{
  Graphic3d_Vec2i anOffset (0, 0);
  if ((thePosition & (Aspect_TOTP_LEFT | Aspect_TOTP_RIGHT)) != 0)
  {
    anOffset.x() = static_cast<Standard_Integer> (myScale * THE_INTERNAL_SCALE_FACTOR);
  }
  if ((thePosition & (Aspect_TOTP_TOP | Aspect_TOTP_BOTTOM)) != 0)
  {
    anOffset.y() = static_cast<Standard_Integer> (myScale * THE_INTERNAL_SCALE_FACTOR);
  }

  myTransformPers->SetCorner2d (thePosition);
  myTransformPers->SetOffset2d (anOffset);
}

// ============================================================================
// function : compute
// purpose  :
// ============================================================================
void V3d_Trihedron::compute()
{
  myStructure->GraphicClear (Standard_False);

  // Create trihedron.
  const Standard_Real aScale           = myScale * myRatio * THE_INTERNAL_SCALE_FACTOR;
  const Standard_Real aCylinderLength  = aScale * THE_CYLINDER_LENGTH;
  const Standard_Real aCylinderDiametr = aScale * myDiameter;
  const Standard_Real aConeDiametr     = myIsWireframe ? aCylinderDiametr : (aCylinderDiametr * 2.0);
  const Standard_Real aConeLength      = aScale * (1.0 - THE_CYLINDER_LENGTH);
  const Standard_Real aSphereRadius    = aCylinderDiametr * 2.0;
  const Standard_Real aRayon           = aScale / 30.0;
  {
    Handle(Graphic3d_Group) aSphereGroup = myStructure->NewGroup();

    // Display origin.
    if (myIsWireframe)
    {
      Handle(Graphic3d_ArrayOfPolylines) anCircleArray = new Graphic3d_ArrayOfPolylines (THE_CIRCLE_SERMENTS_NB + 2);
      for (Standard_Integer anIt = THE_CIRCLE_SERMENTS_NB; anIt >= 0; --anIt)
      {
        anCircleArray->AddVertex (aRayon * Sin (anIt * THE_CIRCLE_SEGMENT_ANGLE),
                                  aRayon * Cos (anIt * THE_CIRCLE_SEGMENT_ANGLE), 0.0);
      }
      anCircleArray->AddVertex (aRayon * Sin (THE_CIRCLE_SERMENTS_NB * THE_CIRCLE_SEGMENT_ANGLE),
                                aRayon * Cos (THE_CIRCLE_SERMENTS_NB * THE_CIRCLE_SEGMENT_ANGLE), 0.0);

      aSphereGroup->SetGroupPrimitivesAspect (mySphereLineAspect->Aspect());
      aSphereGroup->AddPrimitiveArray (anCircleArray);
    }
    else
    {
      gp_Trsf aSphereTransform;
      aSphereGroup->SetGroupPrimitivesAspect (mySphereShadingAspect->Aspect());
      aSphereGroup->AddPrimitiveArray (StdPrs_ToolSphere::Create (aSphereRadius, myNbFacettes, myNbFacettes, aSphereTransform));
    }
  }

  // Display axes.
  {
    const gp_Ax1 anAxes[3] = { gp::OX(), gp::OY(), gp::OZ() };
    for (Standard_Integer anIter = 0; anIter < 3; ++anIter)
    {
      Handle(Graphic3d_Group) anAxisGroup = myStructure->NewGroup();
      anAxisGroup->SetGroupPrimitivesAspect (myArrowShadingAspects[anIter]->Aspect());

      gp_Ax1 aPosition (anAxes[anIter]);

      // Create a tube.
      if (myIsWireframe)
      {
        Handle(Graphic3d_ArrayOfPrimitives) anArray = new Graphic3d_ArrayOfSegments (2);
        anArray->AddVertex (0.0f, 0.0f, 0.0f);
        anArray->AddVertex (anAxes[anIter].Direction().XYZ() * aCylinderLength);

        anAxisGroup->SetGroupPrimitivesAspect (myArrowLineAspects[anIter]->Aspect());
        anAxisGroup->AddPrimitiveArray (anArray);
      }
      else
      {
        gp_Ax3  aSystem (aPosition.Location(), aPosition.Direction());
        gp_Trsf aTrsf;
        aTrsf.SetTransformation (aSystem, gp_Ax3());

        anAxisGroup->AddPrimitiveArray (StdPrs_ToolCylinder::Create (aCylinderDiametr, aCylinderDiametr, aCylinderLength, myNbFacettes, 1, aTrsf));
      }

      aPosition.Translate (gp_Vec (aPosition.Direction().X() * aCylinderLength,
                                   aPosition.Direction().Y() * aCylinderLength,
                                   aPosition.Direction().Z() * aCylinderLength));
      // Create a disk.
      {
        gp_Ax3  aSystem (aPosition.Location(), aPosition.Direction());
        gp_Trsf aTrsf;
        aTrsf.SetTransformation (aSystem, gp_Ax3());

        anAxisGroup->AddPrimitiveArray (StdPrs_ToolDisk::Create (0.0, aConeDiametr, myNbFacettes, 1, aTrsf));
      }

      // Create a cone.
      {
        gp_Ax3  aSystem (aPosition.Location(), aPosition.Direction());
        gp_Trsf aTrsf;
        aTrsf.SetTransformation (aSystem, gp_Ax3());

        anAxisGroup->AddPrimitiveArray (StdPrs_ToolCylinder::Create (aConeDiametr, 0.0, aConeLength, myNbFacettes, 1, aTrsf));
      }
    }
  }

  // Display labels.
  {
    Handle(Graphic3d_Group) aLabelGroup = myStructure->NewGroup();
    const TCollection_ExtendedString aLabels[3] = { "X", "Y", "Z" };
    const gp_Pnt aPoints[3] = { gp_Pnt (aScale + 2.0 * aRayon,                   0.0,               -aRayon),
                                gp_Pnt (               aRayon, aScale + 3.0 * aRayon,          2.0 * aRayon),
                                gp_Pnt (        -2.0 * aRayon,          0.5 * aRayon, aScale + 3.0 * aRayon) };
    for (Standard_Integer anAxisIter = 0; anAxisIter < 3; ++anAxisIter)
    {
      Prs3d_Text::Draw (aLabelGroup, myTextAspect, aLabels[anAxisIter], aPoints[anAxisIter]);
    }
  }
}
