#include "MainWindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QStatusBar> // Added
#include <QStyle>
#include <QVBoxLayout>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_processor(std::make_unique<CADCore::CadProcessor>()) {
  setupUI();
  updateButtonStates();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
  setWindowTitle("CAD Healing Tool");
  resize(800, 600);

  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  // Toolbar
  m_toolBar = addToolBar("Main Toolbar");
  m_toolBar->setMovable(false);

  m_importAction =
      m_toolBar->addAction(QIcon(), "Import", this, &MainWindow::onImport);
  m_checkAction =
      m_toolBar->addAction(QIcon(), "Check", this, &MainWindow::onCheck);
  m_healAction =
      m_toolBar->addAction(QIcon(), "Healing", this, &MainWindow::onHeal);
  m_exportAction =
      m_toolBar->addAction(QIcon(), "Export", this, &MainWindow::onExport);

  // Icons (Using standard styles as fallback if icons not available)
  m_importAction->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
  m_checkAction->setIcon(
      style()->standardIcon(QStyle::SP_FileDialogContentsView));
  m_healAction->setIcon(style()->standardIcon(
      QStyle::SP_BrowserReload)); // Closest to 'wrench' in standard
  m_exportAction->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));

  // Log Display
  m_logDisplay = new QTextEdit(this);
  m_logDisplay->setReadOnly(true);
  m_logDisplay->setFontFamily("Courier New");

  // Initial Welcome Message
  m_logDisplay->setText("Welcome to CAD Healing Tool\n\n"
                        "Please click 'Import' to load a CAD file.\n\n"
                        "Supported formats:\n"
                        "- STEP (*.step, *.stp)\n"
                        "- IGES (*.iges, *.igs)\n"
                        "- BREP (*.brep)\n");

  mainLayout->addWidget(m_logDisplay);

  // Status Bar
  statusBar()->showMessage("Ready");
}

void MainWindow::updateButtonStates() {
  bool hasFile = m_processor->HasLoadedFile();
  m_checkAction->setEnabled(hasFile);
  m_healAction->setEnabled(hasFile);
  m_exportAction->setEnabled(hasFile);
}

void MainWindow::appendLog(const QString &message, const QString &level) {
  m_logDisplay->append(message);
  // Auto scroll to bottom
  QScrollBar *sb = m_logDisplay->verticalScrollBar();
  sb->setValue(sb->maximum());
  QApplication::processEvents();
}

void MainWindow::onImport() {
  QString fileName = QFileDialog::getOpenFileName(
      this, "Import CAD File", "",
      "CAD Files (*.step *.stp *.iges *.igs *.brep);;STEP (*.step *.stp);;IGES "
      "(*.iges *.igs);;BREP (*.brep)");
  if (fileName.isEmpty())
    return;

  appendLog("\nLoading file: " + fileName);
  statusBar()->showMessage("Loading " + fileName + "...");
  QApplication::setOverrideCursor(Qt::WaitCursor);

  bool success = m_processor->ImportFile(fileName.toStdString());

  QApplication::restoreOverrideCursor();

  if (success) {
    m_currentFilePath = fileName;
    updateButtonStates();
    statusBar()->showMessage("Loaded: " + fileName);
    appendLog("Import Successful.");
  } else {
    appendLog("Import Failed!", "ERROR");
    statusBar()->showMessage("Import Failed");
  }
}

void MainWindow::onCheck() {
  if (!m_processor->HasLoadedFile())
    return;

  appendLog("\n========== CAD Model Check Report ==========");
  appendLog(QString("File: %1").arg(QFileInfo(m_currentFilePath).fileName()));

  statusBar()->showMessage("Checking model...");
  QApplication::setOverrideCursor(Qt::WaitCursor);

  CADCore::CheckResult result = m_processor->CheckModel();

  QApplication::restoreOverrideCursor();
  statusBar()->showMessage("Check completed.");

  appendLog("\n[Overall Status]");
  if (result.isValid) {
    appendLog(" Shape is topologically valid");
  } else {
    appendLog(" Shape has validity issues");
  }

  appendLog("\n[Detailed Analysis]");
  appendLog(QString("Total Solids: %1").arg(result.totalSolids));
  appendLog(QString("Total Faces: %1").arg(result.totalFaces));
  appendLog(QString("Total Edges: %1").arg(result.totalEdges));

  appendLog("\n[Issues Found]");
  if (result.issues.empty() && result.warnings.empty()) {
    appendLog(" No critical issues found");
  }
  for (const auto &issue : result.issues)
    appendLog(QString::fromStdString(issue));
  for (const auto &warn : result.warnings)
    appendLog(QString::fromStdString(warn));

  appendLog("\n[Summary]");
  appendLog(QString("Total Issues: %1")
                .arg(result.issues.size() + result.warnings.size()));
  appendLog("============================================");

  if (!result.isValid || !result.issues.empty()) {
    appendLog("Recommendation: Healing is recommended");
  }
}

void MainWindow::onHeal() {
  if (!m_processor->HasLoadedFile())
    return;

  appendLog("\n========== Starting Healing Process ==========");
  statusBar()->showMessage("Healing model...");
  QApplication::setOverrideCursor(Qt::WaitCursor);

  std::vector<std::string> log;
  bool success = m_processor->HealModel(log);

  QApplication::restoreOverrideCursor();

  // Output log
  for (const auto &msg : log) {
    appendLog(QString::fromStdString(msg));
  }

  if (success) {
    statusBar()->showMessage("Healing Completed");
    appendLog("\n========== Healing Completed Successfully ==========");
    appendLog("Status:  SUCCESS - Model is ready for export");
    appendLog("================================================");
  } else {
    statusBar()->showMessage("Healing Failed");
    appendLog("Error: Healing process failed.");
  }
}

void MainWindow::onExport() {
  QString fileName = QFileDialog::getSaveFileName(
      this, "Export CAD File", "",
      "STEP (*.step *.stp);;IGES (*.iges *.igs);;BREP (*.brep)");
  if (fileName.isEmpty())
    return;

  appendLog("\n========== Export Report ==========");
  appendLog("Output file: " + fileName);
  statusBar()->showMessage("Exporting...");
  QApplication::setOverrideCursor(Qt::WaitCursor);

  bool success = m_processor->ExportFile(fileName.toStdString());

  QApplication::restoreOverrideCursor();

  if (success) {
    statusBar()->showMessage("Export Successful");
    appendLog("Status:  Export successful");
    appendLog("===================================");
  } else {
    statusBar()->showMessage("Export Failed");
    appendLog("Status: ✗ Export failed");
  }
}
