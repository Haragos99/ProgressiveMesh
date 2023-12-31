// -*- mode: c++ -*-
#pragma once

#include <QtWidgets/QMainWindow>

#include "MyViewer.h"

class QApplication;
class QProgressBar;

class MyWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MyWindow(QApplication *parent);
  ~MyWindow();

private slots:

  void open();
  void save();
  void setCutoff();
  void setRange();
  void setSlicing();
  void startComputation(QString message);
  void midComputation(int percent);
  void endComputation();
  void displayMessage(const QString& message);

private:
  QApplication *parent;
  MyViewer *viewer;
  QProgressBar *progress;
  QString last_directory;
  QLabel* wlayer = new QLabel(tr("weight: "));
  QLabel* flayer = new QLabel(tr("Frame: "));
  QLabel* layer = new QLabel(tr("Bones: "));
};
