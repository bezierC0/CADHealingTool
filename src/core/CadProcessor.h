#ifndef CADPROCESSOR_H
#define CADPROCESSOR_H

#include "CADCore_global.h"
#include <memory>
#include <string>
#include <vector>

class Standard_Transient;
template <class T> class Handle;
class TDocStd_Document;
class TopoDS_Shape;
class XCAFDoc_ColorTool;
class XCAFDoc_ShapeTool;
class XCAFDoc_LayerTool;
class XCAFDoc_MaterialTool;

namespace CADCore {

struct CheckResult {
  bool isValid = false;
  int totalSolids = 0;
  int totalFaces = 0;
  int totalEdges = 0;
  std::vector<std::string> issues;
  std::vector<std::string> warnings;
};

class CADCORE_EXPORT CadProcessor {
public:
  CadProcessor();
  ~CadProcessor();

  // File Operations
  bool ImportFile(const std::string &filePath);
  bool ExportFile(const std::string &filePath);

  // Processing
  CheckResult CheckModel();
  bool HealModel(std::vector<std::string> &log);

  // Info
  bool HasLoadedFile() const;
  std::string GetCurrentFilePath() const;

  // TODO : Handle(TDocStd_Document) GetDocument();

private:
  // Read Step
  bool ReadSTEP(const std::string &filePath);
  // Read IGES
  bool ReadIGES(const std::string &filePath);
  // Read BREP
  bool ReadBREP(const std::string &filePath);

  // Write
  bool WriteSTEP(const std::string &filePath);
  bool WriteIGES(const std::string &filePath);
  bool WriteBREP(const std::string &filePath);

  // Check Shape
  CheckResult CheckShapeInternal(const TopoDS_Shape &shape);

  // Heal
  // https://dev.opencascade.org/doc/overview/html/occt_user_guides__shape_healing.html#occt_shg_1
  TopoDS_Shape HealShapeInternal(const TopoDS_Shape &shape,
                                 std::vector<std::string> &log);

  //
  void PreserveColors(const TopoDS_Shape &oldShape,
                      const TopoDS_Shape &newShape,
                      std::vector<std::string> &log);

private:
  struct Implementation;
  std::unique_ptr<Implementation> m_impl;

  std::string m_currentFilePath{};
  bool m_hasLoadedFile{};
};

} // namespace CADCore

#endif // CADPROCESSOR_H
