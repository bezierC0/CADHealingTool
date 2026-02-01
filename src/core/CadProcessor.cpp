#include "CadProcessor.h"

// OpenCASCADE Includes
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck_Result.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <IGESCAFControl_Writer.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <ShapeBuild_ReShape.hxx>
#include <ShapeExtend.hxx>
#include <ShapeFix_Shape.hxx>
#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_DataMapOfShapeShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <XCAFDoc_MaterialTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace CADCore {

struct CadProcessor::Implementation {
  TopoDS_Shape m_currentShape;
  Handle(TDocStd_Document) m_doc;

  // XCAF Tools
  Handle(XCAFDoc_ColorTool) m_colorTool;
  Handle(XCAFDoc_ShapeTool) m_shapeTool;
  Handle(XCAFDoc_LayerTool) m_layerTool;
  Handle(XCAFDoc_MaterialTool) m_materialTool;

  Implementation() {
    // Initialize XCAF application
    if (XCAFApp_Application::GetApplication().IsNull()) {
      // Just ensuring it's initialized
    }
  }
};

CadProcessor::CadProcessor()
    : m_impl(std::make_unique<Implementation>()), m_hasLoadedFile(false) {}

CadProcessor::~CadProcessor() = default;

bool CadProcessor::HasLoadedFile() const { return m_hasLoadedFile; }

std::string CadProcessor::GetCurrentFilePath() const {
  return m_currentFilePath;
}

bool CadProcessor::ImportFile(const std::string &filePath) {
  bool success = false;
  std::filesystem::path p(filePath);
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Reset old data
  m_impl->m_currentShape.Nullify();
  if (!m_impl->m_doc.IsNull())
    m_impl->m_doc->Close();
  m_impl->m_doc.Nullify();

  if (ext == ".step" || ext == ".stp") {
    success = ReadSTEP(filePath);
  } else if (ext == ".iges" || ext == ".igs") {
    success = ReadIGES(filePath);
  } else if (ext == ".brep") {
    success = ReadBREP(filePath);
  }

  if (success) {
    m_currentFilePath = filePath;
    m_hasLoadedFile = true;
  }
  return success;
}

bool CadProcessor::ReadSTEP(const std::string &filePath) {
  Handle(TDocStd_Application) app = XCAFApp_Application::GetApplication();
  app->NewDocument("MDTV-XCAF", m_impl->m_doc);

  STEPCAFControl_Reader reader;
  reader.SetColorMode(true);
  reader.SetNameMode(true);
  reader.SetLayerMode(true);
  reader.SetPropsMode(true);

  Interface_Static::SetCVal("xstep.cascade.unit", "M");

  IFSelect_ReturnStatus status = reader.ReadFile(filePath.c_str());
  if (status != IFSelect_RetDone)
    return false;

  if (!reader.Transfer(m_impl->m_doc))
    return false;

  m_impl->m_shapeTool = XCAFDoc_DocumentTool::ShapeTool(m_impl->m_doc->Main());
  m_impl->m_colorTool = XCAFDoc_DocumentTool::ColorTool(m_impl->m_doc->Main());
  m_impl->m_layerTool = XCAFDoc_DocumentTool::LayerTool(m_impl->m_doc->Main());
  m_impl->m_materialTool =
      XCAFDoc_DocumentTool::MaterialTool(m_impl->m_doc->Main());

  TDF_LabelSequence freeShapes;
  m_impl->m_shapeTool->GetFreeShapes(freeShapes);

  BRep_Builder B;
  TopoDS_Compound compound;
  B.MakeCompound(compound);

  for (Standard_Integer i = 1; i <= freeShapes.Length(); i++) {
    TopoDS_Shape shape = m_impl->m_shapeTool->GetShape(freeShapes.Value(i));
    B.Add(compound, shape);
  }

  m_impl->m_currentShape = compound;
  return !m_impl->m_currentShape.IsNull();
}

bool CadProcessor::ReadIGES(const std::string &filePath) {
  Handle(TDocStd_Application) app = XCAFApp_Application::GetApplication();
  app->NewDocument("MDTV-XCAF", m_impl->m_doc);

  IGESCAFControl_Reader reader;
  reader.SetColorMode(true);
  reader.SetNameMode(true);
  reader.SetLayerMode(true);

  IFSelect_ReturnStatus status = reader.ReadFile(filePath.c_str());
  if (status != IFSelect_RetDone)
    return false;

  if (!reader.Transfer(m_impl->m_doc))
    return false;

  m_impl->m_shapeTool = XCAFDoc_DocumentTool::ShapeTool(m_impl->m_doc->Main());
  m_impl->m_colorTool = XCAFDoc_DocumentTool::ColorTool(m_impl->m_doc->Main());
  m_impl->m_layerTool = XCAFDoc_DocumentTool::LayerTool(m_impl->m_doc->Main());
  m_impl->m_materialTool =
      XCAFDoc_DocumentTool::MaterialTool(m_impl->m_doc->Main());

  TDF_LabelSequence freeShapes;
  m_impl->m_shapeTool->GetFreeShapes(freeShapes);

  BRep_Builder B;
  TopoDS_Compound compound;
  B.MakeCompound(compound);

  for (Standard_Integer i = 1; i <= freeShapes.Length(); i++) {
    TopoDS_Shape shape = m_impl->m_shapeTool->GetShape(freeShapes.Value(i));
    B.Add(compound, shape);
  }
  m_impl->m_currentShape = compound;
  return !m_impl->m_currentShape.IsNull();
}

bool CadProcessor::ReadBREP(const std::string &filePath) {
  BRep_Builder builder;
  if (!BRepTools::Read(m_impl->m_currentShape, filePath.c_str(), builder)) {
    return false;
  }

  Handle(TDocStd_Application) app = XCAFApp_Application::GetApplication();
  app->NewDocument("MDTV-XCAF", m_impl->m_doc);
  m_impl->m_shapeTool = XCAFDoc_DocumentTool::ShapeTool(m_impl->m_doc->Main());
  m_impl->m_colorTool = XCAFDoc_DocumentTool::ColorTool(m_impl->m_doc->Main());
  m_impl->m_layerTool = XCAFDoc_DocumentTool::LayerTool(m_impl->m_doc->Main());
  m_impl->m_materialTool =
      XCAFDoc_DocumentTool::MaterialTool(m_impl->m_doc->Main());

  m_impl->m_shapeTool->AddShape(m_impl->m_currentShape);

  return true;
}

CheckResult CadProcessor::CheckModel() {
  if (m_impl->m_currentShape.IsNull())
    return CheckResult();

  return CheckShapeInternal(m_impl->m_currentShape);
}

CheckResult CadProcessor::CheckShapeInternal(const TopoDS_Shape &shape) {
  CheckResult res;
  BRepCheck_Analyzer analyzer(shape);
  res.isValid = analyzer.IsValid();

  TopExp_Explorer exSolid(shape, TopAbs_SOLID);
  for (; exSolid.More(); exSolid.Next())
    res.totalSolids++;

  TopExp_Explorer exFace(shape, TopAbs_FACE);
  for (; exFace.More(); exFace.Next())
    res.totalFaces++;

  TopExp_Explorer exEdge(shape, TopAbs_EDGE);
  for (; exEdge.More(); exEdge.Next())
    res.totalEdges++;

  // Deep inspection
  TopExp_Explorer ex(shape, TopAbs_FACE);
  for (; ex.More(); ex.Next()) {
    TopoDS_Face F = TopoDS::Face(ex.Current());
    Handle(BRepCheck_Result) faceRes = analyzer.Result(F);
    if (!faceRes.IsNull()) {
    }
  }

  int smallEdges = 0;
  TopExp_Explorer exE(shape, TopAbs_EDGE);
  for (; exE.More(); exE.Next()) {
    TopoDS_Edge E = TopoDS::Edge(exE.Current());
    GProp_GProps props;
    BRepGProp::LinearProperties(E, props);
    if (props.Mass() < 1e-6) {
      smallEdges++;
    }
  }

  if (smallEdges > 0) {
    res.warnings.push_back("Warning: " + std::to_string(smallEdges) +
                           " small edges detected (< 1e-6)");
  }

  if (!res.isValid) {
    res.issues.push_back("Critical: Shape is not valid according to BRepCheck");
  }

  return res;
}

bool CadProcessor::HealModel(std::vector<std::string> &log) {
  if (m_impl->m_currentShape.IsNull())
    return false;

  log.push_back("Starting Healing Process...");

  TopoDS_Shape healedShape = HealShapeInternal(m_impl->m_currentShape, log);

  log.push_back("[Step 4] Preserving attributes...");
  PreserveColors(m_impl->m_currentShape, healedShape, log);

  m_impl->m_currentShape = healedShape;

  // Update XCAF
  m_impl->m_shapeTool->AddShape(m_impl->m_currentShape);

  log.push_back("Healing Completed Successfully");
  return true;
}

TopoDS_Shape CadProcessor::HealShapeInternal(const TopoDS_Shape &shape,
                                             std::vector<std::string> &log) {
  // Setup ShapeFix
  Handle(ShapeFix_Shape) sfs = new ShapeFix_Shape(shape);
  sfs->Init(shape);
  sfs->SetPrecision(1e-7);
  sfs->SetMinTolerance(1e-7);
  sfs->SetMaxTolerance(1.0);

  // Enable all relevant fixes
  sfs->FixSolidMode() = 1;  // Returns (modifiable) the mode for applying fixes
                            // of ShapeFix_Solid, by default True.

  sfs->FixFreeShellMode() = 1; // Returns (modifiable) the mode for applying
                               // fixes of ShapeFix_FreeShell, by default True.
  sfs->FixFreeFaceMode() = 1; // Returns (modifiable) the mode for applying
                              // fixes of ShapeFix_FreeFace, by default True.
  sfs->FixFreeWireMode() = 1; // Returns (modifiable) the mode for applying
                              // fixes of ShapeFix_FreeWire, by default True.
  sfs->FixSameParameterMode() = 1;
  sfs->FixVertexPositionMode() = 1;
  //  sfs->FixSmallAreaWireMode() = 1;

  log.push_back("[Step 1] Configuring ShapeFix (Tolerance 1e-7)...");

  // Execute Healing
  log.push_back("[Step 2] Executing ShapeFix_Shape...");
  if (sfs->Perform()) {
    log.push_back("  Status: Changes applied.");
  } else {
    log.push_back("  Status: No critical changes required or fixes failed.");
  }

  TopoDS_Shape result = sfs->Shape();

  // Report detailed status (Querying the tool's status flags)
  log.push_back("[Step 3] Analysing repair results...");

  /*This enumeration is used in ShapeHealing toolkit for representing flags in
   * the return statuses of class methods. */
  if (sfs->Status(ShapeExtend_DONE1))
    log.push_back("  - Fixed: Solids/Shells structure");
  if (sfs->Status(ShapeExtend_DONE2))
    log.push_back("  - Fixed: Faces orientation/composition");
  if (sfs->Status(ShapeExtend_DONE3))
    log.push_back("  - Fixed: Wires/Internal topology");
  if (sfs->Status(ShapeExtend_DONE4))
    log.push_back("  - Fixed: Edges geometry");
  if (sfs->Status(ShapeExtend_DONE5))
    log.push_back("  - Fixed: Vertices");
  if (sfs->Status(ShapeExtend_DONE6))
    log.push_back("  - Fixed: Geometric coherence");
  if (sfs->Status(ShapeExtend_FAIL)) {
    log.push_back("  - Warning: Some failure bits were set during processing.");
  }

  return result;
}

void CadProcessor::PreserveColors(const TopoDS_Shape &oldShape,
                                  const TopoDS_Shape &newShape,
                                  std::vector<std::string> &log) {
  TopTools_IndexedMapOfShape oldFaces;
  TopExp::MapShapes(oldShape, TopAbs_FACE, oldFaces);

  TopTools_IndexedMapOfShape newFaces;
  TopExp::MapShapes(newShape, TopAbs_FACE, newFaces);

  struct FaceInfo {
    TopoDS_Face face;
    gp_Pnt center;
    bool valid;
  };

  std::vector<FaceInfo> oldFaceInfos;
  oldFaceInfos.reserve(oldFaces.Extent());

  for (int j = 1; j <= oldFaces.Extent(); j++) {
    TopoDS_Face oldF = TopoDS::Face(oldFaces(j));
    GProp_GProps oldProps;
    try {
      BRepGProp::SurfaceProperties(oldF, oldProps);
      oldFaceInfos.push_back({oldF, oldProps.CentreOfMass(), true});
    } catch (...) {
    }
  }

  int preservedCount = 0;
  const double tolerance = 1e-3;

  for (int i = 1; i <= newFaces.Extent(); i++) {
    TopoDS_Face newF = TopoDS::Face(newFaces(i));
    GProp_GProps newProps;
    try {
      BRepGProp::SurfaceProperties(newF, newProps);
    } catch (...) {
      continue;
    }

    gp_Pnt newCenter = newProps.CentreOfMass();
    TopoDS_Face bestOldF;
    double minDist = 1e9;
    bool found = false;

    for (const auto &info : oldFaceInfos) {
      if (!info.valid)
        continue;
      double dist = info.center.SquareDistance(newCenter);
      if (dist < minDist) {
        minDist = dist;
        bestOldF = info.face;
        found = true;
      }
    }

    if (found && minDist < (tolerance * tolerance)) {
      Quantity_Color col;
      if (m_impl->m_colorTool->GetColor(bestOldF, XCAFDoc_ColorSurf, col)) {
        m_impl->m_colorTool->SetColor(newF, col, XCAFDoc_ColorSurf);
        preservedCount++;
      }
    }
  }

  log.push_back("Color attributes preserved: " +
                std::to_string(preservedCount) + " faces");
}

bool CadProcessor::ExportFile(const std::string &filePath) {
  bool success = false;
  std::filesystem::path p(filePath);
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".step" || ext == ".stp") {
    success = WriteSTEP(filePath);
  } else if (ext == ".iges" || ext == ".igs") {
    success = WriteIGES(filePath);
  } else if (ext == ".brep") {
    success = WriteBREP(filePath);
  }
  return success;
}

bool CadProcessor::WriteSTEP(const std::string &filePath) {
  STEPCAFControl_Writer writer;
  writer.SetColorMode(true);
  writer.SetNameMode(true);
  writer.SetLayerMode(true);
  writer.SetPropsMode(true);

  // Ensure we are transferring the doc
  if (!writer.Transfer(m_impl->m_doc, STEPControl_AsIs))
    return false;

  IFSelect_ReturnStatus status = writer.Write(filePath.c_str());
  return (status == IFSelect_RetDone);
}

bool CadProcessor::WriteIGES(const std::string &filePath) {
  IGESCAFControl_Writer writer;
  writer.SetColorMode(true);
  writer.SetNameMode(true);
  writer.SetLayerMode(true);

  if (!writer.Transfer(m_impl->m_doc))
    return false;

  return writer.Write(filePath.c_str());
}

bool CadProcessor::WriteBREP(const std::string &filePath) {
  return BRepTools::Write(m_impl->m_currentShape, filePath.c_str());
}

} // namespace CADCore
