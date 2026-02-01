#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "CadProcessor.h"
#include <QAction>
#include <QMainWindow>
#include <QTextEdit>
#include <QToolBar>
#include <memory>


class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void onImport();
  void onCheck();
  void onHeal();
  void onExport();

private:
  void setupUI();
  void updateButtonStates();
  void appendLog(const QString &message, const QString &level = "INFO");

  // UI Components
  QTextEdit       *m_logDisplay;
  QAction         *m_importAction;
  QAction         *m_checkAction;
  QAction         *m_healAction;
  QAction         *m_exportAction;
  QToolBar        *m_toolBar;

  // Logic
  std::unique_ptr<CADCore::CadProcessor>    m_processor;
  QString                                   m_currentFilePath;
};

#endif // MAINWINDOW_H
